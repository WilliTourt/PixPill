#include "cpp_main.h"

#include "bma530.h"
#include "is31fl3736.h"

#include "sim_base.h"
#include "sandsim.h"
#include "liquidsim.h"
#include "pixpill_anim.h"

// ===================== State Machine =====================

enum class DeviceState : uint8_t {
    RUNNING,   // Simulation active
    SHUTDOWN   // Ship mode / stop
};

enum class SimMode : uint8_t {
    SAND,
    LIQUID
};

struct PwrFlags {
    bool charging; // USB plugged
    bool error;    // nPM fault
};

// ===================== Globals =====================

DeviceState state = DeviceState::RUNNING;
SimMode simMode = SimMode::LIQUID;
PwrFlags pwrFlags = {false, false};

BMA530 accel(&hi2c1);
IS31FL3736 is31(&hi2c1);

SimBase *sim = nullptr;
SandSim sand(accel, is31, 32);
LiquidSim liquid(accel, is31, 16);
PixPillAnim anim(is31);

// ===================== Gesture Detection =====================
// Detect a quick left-right-left-right shake (4 direction changes in 500ms)

static const int16_t SHAKE_THRESHOLD = 14000;   // raw accel value to count as direction change
static const uint32_t SHAKE_WINDOW_MS = 300;    // time window for gesture

int16_t  last_ax = 0;
uint8_t  shake_count = 0;
uint32_t shake_first_ms = 0;
bool     shake_positive = false;  // true = current direction is positive

static bool gestureDetect(int16_t ax) {
    uint32_t now = HAL_GetTick();

    // Detect zero-crossing with magnitude
    bool is_positive = (ax > SHAKE_THRESHOLD);
    bool is_negative = (ax < -SHAKE_THRESHOLD);

    if (!is_positive && !is_negative) return false;  // not enough tilt

    bool new_dir = is_positive;
    if (new_dir == shake_positive) return false;  // same direction, ignore

    // Direction changed
    if (shake_count == 0) {
        shake_first_ms = now;
    } else if (now - shake_first_ms > SHAKE_WINDOW_MS) {
        // Too slow, reset
        shake_count = 0;
        shake_first_ms = now;
    }

    shake_positive = new_dir;
    shake_count++;

    if (shake_count >= 4) {
        shake_count = 0;
        return true;
    }

    return false;
}

// ===================== Power Monitoring =====================

static void updatePowerFlags() {
    pwrFlags.charging = (HAL_GPIO_ReadPin(CHG_GPIO_Port, CHG_Pin) == GPIO_PIN_RESET);
    pwrFlags.error   = (HAL_GPIO_ReadPin(ERR_GPIO_Port, ERR_Pin) == GPIO_PIN_RESET);
}

// ===================== LED Status Blink =====================

static uint32_t led_last_toggle_ms = 0;
static bool     led_on = false;

static void blink(uint32_t interval_ms) {
    uint32_t now = HAL_GetTick();
    if (now - led_last_toggle_ms >= interval_ms) {
        led_last_toggle_ms = now;
        led_on = !led_on;
        HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin,
                          led_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

// ===================== LED Array Animations (moved to pixpill_anim.cpp) =====================

// ===================== Sleep / Shutdown =====================

static const uint32_t IDLE_TIMEOUT_MS = 10000;
static const int16_t  TILT_IDLE_THRESHOLD = 3000;
uint32_t last_active_ms = 0;

static void enterSleep() {
    is31.ledOffAll();
    HAL_Delay(50);

    if (pwrFlags.charging) {
        // USB plugged: just STOP mode, can wake
        HAL_SuspendTick();
        HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);
    } else {
        // Battery only: enter ship mode (shutdown)
        HAL_GPIO_WritePin(SHPACT_GPIO_Port, SHPACT_Pin, GPIO_PIN_SET);
        HAL_Delay(150);
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = SHPACT_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(SHPACT_GPIO_Port, &GPIO_InitStruct);
    }
}


void cpp_main() {
    accel.begin(BMA530::ODR::_100HZ, BMA530::Range::_2G, BMA530::Power::LPM);

    is31.begin();
    is31.setPWMAll(0xFF);
    is31.ledOnAll(39);
    HAL_Delay(300);
    is31.ledOffAll();
    is31.setGCC(18);

    sand.init();
    liquid.init();
    sim = &liquid;

    // Boot animation (blocking, ~2730ms)
    anim.start(PixPillAnim::Anim::BOOT);
    while (anim.tick(65));

    last_active_ms = HAL_GetTick();

    while (1) {
        updatePowerFlags();

        // ERR takes priority — flash LED array
        if (pwrFlags.error) {
            anim.start(PixPillAnim::Anim::ERR);
            while (1) {
                anim.tick();
                HAL_Delay(16);
                updatePowerFlags();
                if (!pwrFlags.error) break;  // ERR cleared
                // Blink LED_STATUS too
                blink(100);
            }
            anim.stop();
            // Restore from simulation after ERR clears
            sim->draw();
        }

        // Charging indicator (non-blocking, runs alongside simulation)
        static bool was_charging = false;
        if (pwrFlags.charging && !was_charging) {
            anim.start(PixPillAnim::Anim::CHARGING);
        }
        if (!pwrFlags.charging && was_charging) {
            anim.stop();
        }
        was_charging = pwrFlags.charging;

        switch (state) {
            case DeviceState::RUNNING: {
                accel.update();
                int16_t ax = accel.readAx(), ay = accel.readAy();
                uint16_t tilt = (ax > 0 ? ax : -ax) + (ay > 0 ? ay : -ay);

                // Activity detection
                if (tilt > TILT_IDLE_THRESHOLD) {
                    last_active_ms = HAL_GetTick();
                }

                // Gesture → toggle mode
                if (gestureDetect(ax)) {
                    is31.ledOffAll();
                    sim = ((sim == &liquid) ? (SimBase*)&sand : (SimBase*)&liquid);
                }

                // Run simulation (or charging animation if charging)
                if (!pwrFlags.charging) {
                    sim->calc();
                    sim->draw();
                } else {
                    anim.tick();  // charging breathing
                }

                // Idle timeout → shutdown
                if (HAL_GetTick() - last_active_ms > IDLE_TIMEOUT_MS) {
                    state = DeviceState::SHUTDOWN;
                }

                // Sand needs throttling, liquid runs as fast as possible
                if (sim == &sand && !pwrFlags.charging) {
                    uint16_t delay_ms = 100000 / (tilt + 500);
                    if (delay_ms > 200) delay_ms = 200;
                    if (delay_ms < 20)  delay_ms = 20;
                    HAL_Delay(delay_ms);
                }
                if (pwrFlags.charging) {
                    HAL_Delay(30);  // charging anim frame rate
                }
                break;
            }

            case DeviceState::SHUTDOWN: {
                // Shutdown animation (blocking, ~600ms)
                anim.start(PixPillAnim::Anim::SHUTDOWN);
                while (anim.tick()) {
                    HAL_Delay(16);
                }
                enterSleep(); // Ship mode or STOP
                while (1) { blink(500); }
                break;
            }
        }
    }
}

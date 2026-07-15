/*******************************************************************************
 * @file    cpp_main.cpp
 * @version 1.1
 * 
 * @brief   PixPill firmware — application entry point and main loop.
 *
 * @details
 * PixPill is a pill-sized LED matrix gadget built around STM32C011 (WLCSP12),
 * BMA530 accelerometer, IS31FL3736 LED driver, and nPM1100 PMIC.
 *
 * The firmware simulates physical particles (sand / liquid) on a 96-LED
 * array in real time, reacting to gravity from the BMA530. A quick
 * shake gesture toggles between sand and liquid modes.
 *
 * ARCHITECTURE:
 * - Driver: `bma530` (I²C accel), `is31fl3736` (I²C LED matrix)
 * - Simulation: `SimBase` → `SandSim` (cellular automata sim) / `LiquidSim` (physically-based particle sims)
 * - Animation: `PixPillAnim` (boot / charging / error / shutdown)
 * - Power: nPM1100 ship-mode(shutdown) via SHPACT; CHG/ERR pin monitoring
 * 
 *   Memory region         Used Size  Region Size  %age Used
 *                RAM:        2872 B         6 KB     46.74%
 *              FLASH:       32640 B        32 KB     99.61%
 *
 * @author:     WilliTourt <willitourt@foxmail.com>
 * @date        2026-07-10
 * 
 * @changelog:
 * - 2026-07-10: Initial version
 * - 2026-07-15: 1# LED matrix adapted, added LED fault detection
 * 
 ******************************************************************************/

#include "cpp_main.h"
#include "tim.h"

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

// enum class SimMode : uint8_t {
//     SAND,
//     LIQUID
// };

struct PwrFlags {
    bool charging; // USB plugged
    bool error;    // nPM fault
};

// ===================== Globals =====================

DeviceState state = DeviceState::RUNNING;
PwrFlags pwrFlags = {false, false};

BMA530 accel(&hi2c1);
IS31FL3736 is31(&hi2c1);

SimBase *sim = nullptr;
SandSim sand(accel, is31, 32);
LiquidSim liquid(accel, is31, 16);
PixPillAnim anim(is31);

// ===================== Gesture Detection =====================
// Detect a quick up-down-up-down shake (4 direction changes in SHAKE_WINDOW_MS)

static const int16_t SHAKE_THRESHOLD = 14000;   // raw accel value to count as direction change
static const uint32_t SHAKE_WINDOW_MS = 570;    // time window for gesture

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

    if (shake_count >= 8) {
        shake_count = 0;
        return true;
    }

    return false;
}

// ===================== Power Monitoring =====================

static void updatePowerFlags() {
    pwrFlags.charging = (HAL_GPIO_ReadPin(CHG_GPIO_Port, CHG_Pin) == GPIO_PIN_RESET);
    pwrFlags.error    = (HAL_GPIO_ReadPin(ERR_GPIO_Port, ERR_Pin) == GPIO_PIN_RESET);
}

// ===================== LED Status PWM Ctrl =====================

// 64-step sin² lookup for smooth breathing (~2.5s cycle at 25ms/call)
static const uint8_t BREATH_LUT[64] = {
    0,  1,  3,  8, 15, 24, 35, 48,
   63, 80, 98,118,139,161,184,208,
  232,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,
  255,232,208,184,161,139,118, 98,
   80, 63, 48, 35, 24, 15,  8,  3,
    1,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0
};

static void breathLed() {
    static uint8_t  step = 0;
    static uint32_t last_ms = 0;
    uint32_t now = HAL_GetTick();
    if (now - last_ms < 25) return;
    last_ms = now;

    uint16_t pwm = (uint16_t)BREATH_LUT[step] * 999 / 255; // 999: ARR - 1
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pwm);
    step = (step + 1) & 63;
}

// ERR mode: fast blink
static void blinkLedFast() {
    static uint32_t led_last_toggle_ms = 0;
    static bool     led_on = false;
    uint32_t now = HAL_GetTick();
    if (now - led_last_toggle_ms >= 100) {
        led_last_toggle_ms = now;
        led_on = !led_on;
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, (led_on ? 999 : 0));
    }
}

// ===================== Sleep / Shutdown =====================

static const uint32_t IDLE_TIMEOUT_MS = 22000;
static const int16_t  MOTION_DELTA_THRESHOLD = 500;  // min accel change to count as "moving"
uint32_t last_active_ms = 0;
int16_t  prev_ax_idle = 0, prev_ay_idle = 0;  // previous accel for idle detection

static void enterSleep() {
    is31.ledOffAll();
    HAL_Delay(50);

    if (pwrFlags.charging) {
        while (pwrFlags.charging) {
            updatePowerFlags();
            breathLed();
            HAL_Delay(50);
        }

        HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2);
    }
    // Enter ship mode (shutdown)
    HAL_GPIO_WritePin(SHPACT_GPIO_Port, SHPACT_Pin, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(SHPACT_GPIO_Port, SHPACT_Pin, GPIO_PIN_SET);
    HAL_Delay(1000);

    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);
}


void cpp_main() {
    accel.begin(BMA530::ODR::_50HZ, BMA530::Range::_2G, BMA530::Power::LPM);

    is31.begin();
    is31.setPWMAll(0xFF);

    is31.ledOnAll(45);
    // HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET);
    HAL_Delay(300);
    is31.ledOffAll();
    // HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_SET);

    // LED open/short detect — run once at boot before simulation starts
    IS31FL3736::FaultResult faults = is31.detectFaults();
    if (faults.valid) {

        // Check if any LED has open or short fault
        uint8_t open_count = 0, short_count = 0;
        for (uint8_t i = 0; i < 96; i++) {

            #ifdef PIXPILL_SIZE_1_CAPSULE
            if (i < 6) continue;
            #endif

            if (faults.open_leds[i]) open_count++;
            if (faults.short_leds[i]) short_count++;
        }

        // If fault detected, flash ERR animation briefly then continue
        if (open_count > 0 || short_count > 0) {
            is31.setGCC(22);
            anim.start(PixPillAnim::Anim::ERR);
            uint32_t err_start = HAL_GetTick();
            while (HAL_GetTick() - err_start < 3000) {
                anim.tick();
                HAL_Delay(16);
            }
            anim.stop();
        }
    }

    is31.setGCC(22);

    sand.init();
    liquid.init();
    sim = &liquid;

    // Boot animation (blocking, ~2436ms)
    anim.start(PixPillAnim::Anim::BOOT);
    while (anim.tick(58));

    last_active_ms = HAL_GetTick();

    while (1) {
        updatePowerFlags();

        // ERR takes priority — flash LED array
        if (pwrFlags.error) {
            anim.start(PixPillAnim::Anim::ERR);
            while (pwrFlags.error) {
                HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
                anim.tick();
                HAL_Delay(16);
                updatePowerFlags();
                if (!pwrFlags.error) break;  // ERR cleared
                // Blink LED_STATUS too
                blinkLedFast();
            }
            HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2);
            anim.stop();
            // Restore from simulation after ERR clears
            sim->draw();
        }

        // Charging indicator — play anim for 4s, then resume simulation
        static bool     was_charging      = false;
        static uint32_t charge_anim_start = 0;
        static bool     charge_anim_done  = false;

        if (pwrFlags.charging && !was_charging) {
            anim.start(PixPillAnim::Anim::CHARGING);
            HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
            charge_anim_start = HAL_GetTick();
            charge_anim_done  = false;
        }

        if (!pwrFlags.charging && was_charging) {
            anim.stop();
            HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2);
            HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_SET);
            charge_anim_done = false;
        }

        if (pwrFlags.charging && !charge_anim_done &&
            HAL_GetTick() - charge_anim_start > 4000) {
            anim.stop();
            charge_anim_done = true;
        }
        was_charging = pwrFlags.charging;

        switch (state) {
            case DeviceState::RUNNING: {
                accel.update();
                int16_t ax = accel.readAx(), ay = accel.readAy();
                uint16_t tilt = (ax > 0 ? ax : -ax) + (ay > 0 ? ay : -ay);

                // Activity detection — any accel change counts as "moving"
                int16_t dax = ax - prev_ax_idle, day = ay - prev_ay_idle;
                if ((dax > 0 ? dax : -dax) + (day > 0 ? day : -day) > MOTION_DELTA_THRESHOLD) {
                    last_active_ms = HAL_GetTick();
                }
                prev_ax_idle = ax;
                prev_ay_idle = ay;

                // Gesture → toggle mode
                if (gestureDetect(ax)) {
                    is31.ledOffAll();
                    sim = ((sim == &liquid) ? (SimBase*)&sand : (SimBase*)&liquid);
                }

                // Run simulation (or charging animation for first 4s)
                if (!pwrFlags.charging || charge_anim_done) {
                    sim->calc();
                    sim->draw();
                } else {
                    anim.tick();  // charging breathing
                }

                // Idle timeout → shutdown
                if (HAL_GetTick() - last_active_ms > IDLE_TIMEOUT_MS) {
                    state = DeviceState::SHUTDOWN;
                }

                // Sand needs speed limit, liquid runs as fast as possible
                if (sim == &sand && !pwrFlags.charging) {
                    uint16_t delay_ms = 100000 / (tilt + 500);
                    if (delay_ms > 200) delay_ms = 200;
                    if (delay_ms < 20)  delay_ms = 20;
                    HAL_Delay(delay_ms);
                }

                if (pwrFlags.charging) {
                    breathLed();
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
                while (1);
                break;
            }
        }
    }
}

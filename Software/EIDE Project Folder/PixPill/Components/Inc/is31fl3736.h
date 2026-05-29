/* ============================================================
 * IS31FL3736 — 12×8 LED Matrix Driver
 *
 * Page-based register access: write FDh to select PG0-PG3,
 * then r/w that page's registers.  FDh must be unlocked via
 * FEh=0xC5 before each write (one-shot unlock).
 *
 * 7-bit I2C address: 101xxxx (ADDR1/ADDR2 pins set low 4 bits)
 * ============================================================ */
#pragma once
#include "main.h"

#ifdef HAL_I2C_MODULE_ENABLED
    #include "i2c.h"
#else
    #error "At least one IIC port should be opened"
#endif

/* ---- I2C base address (fill in A[3:0] per your ADDR1/2 wiring) ---- */
#define IS31FL3736_I2C_ADDR        (0x50 << 1)  // default: ADDR1=GND, ADDR2=GND → 101 0000

/* ============================================================
 * Global registers (outside pages)
 * ============================================================ */
#define IS31_REG_CMD                0xFD  // Page select (write)
#define IS31_REG_CMD_WR_LOCK        0xFE  // Unlock FDh (write 0xC5)
#define IS31_REG_INT_MASK           0xF0  // Interrupt mask
#define IS31_REG_INT_STATUS         0xF1  // Interrupt status (read)

/* Page select values for CMD register */
#define IS31_PAGE_LED               0x00  // PG0: LED on/off + open/short detect
#define IS31_PAGE_PWM               0x01  // PG1: PWM duty (8-bit per LED)
#define IS31_PAGE_BREATH            0x02  // PG2: Auto Breath Mode select
#define IS31_PAGE_FUNC              0x03  // PG3: Configuration + timing

/* ---- PG3 (Function) register addresses ---- */
#define IS31_REG_CONF               0x00  // Configuration: SSD | B_EN | OSD | SYNC
#define IS31_REG_GCC                0x01  // Global Current Control (0-255)
/* ABM-1 time params: 02h-T1T2, 03h-T3T4, 04h-LBLETA, 05h-LTB */
#define IS31_REG_ABM1_T1T2          0x02
#define IS31_REG_ABM1_T3T4          0x03
#define IS31_REG_ABM1_LOOP1         0x04
#define IS31_REG_ABM1_LOOP2         0x05
/* ABM-2 time params */
#define IS31_REG_ABM2_T1T2          0x06
#define IS31_REG_ABM2_T3T4          0x07
#define IS31_REG_ABM2_LOOP1         0x08
#define IS31_REG_ABM2_LOOP2         0x09
/* ABM-3 time params */
#define IS31_REG_ABM3_T1T2          0x0A
#define IS31_REG_ABM3_T3T4          0x0B
#define IS31_REG_ABM3_LOOP1         0x0C
#define IS31_REG_ABM3_LOOP2         0x0D
#define IS31_REG_TIME_UPDATE        0x0E  // Write 0x00 to commit time params
#define IS31_REG_SW_PULLUP          0x0F  // SWy pull-up resistor
#define IS31_REG_CS_PULLDOWN        0x10  // CSx pull-down resistor
#define IS31_REG_RESET              0x11  // Read to reset all registers

/* ---- Configuration Register (PG3 00h) bits ---- */
#define IS31_CONF_SSD               (1 << 0)  // Software shutdown (0=shutdown, 1=normal)
#define IS31_CONF_B_EN              (1 << 1)  // Auto breath enable
#define IS31_CONF_OSD               (1 << 2)  // Open/short detect trigger (0→1)
#define IS31_CONF_SYNC_MASTER       (1 << 5)  // SYNC[1:0]=01: master
#define IS31_CONF_SYNC_SLAVE        (2 << 5)  // SYNC[1:0]=10: slave

/* ---- Lock / unlock command ---- */
#define IS31_UNLOCK_CMD             0xC5
#define IS31_LOCK_CMD               0x00

/* ---- LED matrix dimensions ---- */
#define IS31_SW_COUNT               12
#define IS31_CS_COUNT               8
#define IS31_LED_COUNT              (IS31_SW_COUNT * IS31_CS_COUNT)

/* ---- Auto Breath Mode select (PG2) ---- */
/* Per-LED mode stored in bits[1:0]; bits[7:2] reserved */
#define IS31_ABM_PWM                0x00  // Manual PWM control
#define IS31_ABM1                   0x01
#define IS31_ABM2                   0x02
#define IS31_ABM3                   0x03



class IS31FL3736 {
    public:

        /* ---- Breath timing presets (0.21s base unit) ---- */
        enum class BreathTime : uint8_t {
            T_0s    = 0,    // T2/T4 only
            T_0_21s  = 1,
            T_0_42s  = 2,
            T_0_84s  = 3,
            T_1_68s  = 4,
            T_3_36s  = 5,
            T_6_72s  = 6,
            T_13_44s = 7,
            T_26_88s = 8,
            T_53_76s = 9,    // T4 only
            T_107_52s = 10   // T4 only
        };

        /* !!!! SAFETY CURRENT LIMITATIONS !!!! */
        static constexpr float MAX_TOTAL_CURRENT_MA = 280.0f;
        static constexpr float MAX_LED_CURRENT_MA  = 3.6f;

        IS31FL3736(I2C_HandleTypeDef *hi2c, float r_ext = 20000.0f, uint8_t i2c_addr = IS31FL3736_I2C_ADDR);

        /* ---- Lifecycle ---- */
        bool begin();                                    // Unlock, config (normal op, GCC=0), ready
        bool begin(uint8_t gcc);                         // With global current (0=off, 255=max)
        bool shutdown();                                 // Software shutdown

        /* ---- LED on/off (PG0) ---- */
        bool ledOn(uint8_t cs, uint8_t sw);              // CS 1-8, SW 1-12 (1-indexed!)
        bool ledOff(uint8_t cs, uint8_t sw);
        bool ledSet(uint8_t cs, uint8_t sw, bool on);

        bool ledOnAll(uint8_t gcc);                      // All LEDs on (Used for testing. GCC WILL NOT BE LIMITED!)
        bool ledOffAll();                                // All LEDs off

        /* ---- PWM (PG1) — 0-255 per LED ---- */
        bool setPWM(uint8_t cs, uint8_t sw, uint8_t val);

        /* ---- Breath mode (PG2) ---- */
        bool setBreathMode(uint8_t cs, uint8_t sw, uint8_t mode);  // 0=PWM, 1/2/3=ABM

        /* ---- Global current (PG3 01h) ---- */
        bool setGCC(uint8_t gcc);

        /* ---- Breath timing (PG3 02h-0Dh) ---- */
        bool setBreathTiming(uint8_t abm,              // 1-3
                             BreathTime t1_rise,
                             BreathTime t2_hold,
                             BreathTime t3_fall,
                             BreathTime t4_off);

        inline float getIout() {            // Total output current (mA)
            return (840.0f / _rext_ohm * _gcc / 256.0f);
            // See datasheet page 19: Global Current Control
        }

        inline float getIled() {
            return (getIout() / 12.75f);
            // See datasheet page 25: POWER DISSIPATION
        }

    private:
        I2C_HandleTypeDef *_hi2c;
        uint8_t _addr;       // 8-bit I2C address
        uint8_t _page;       // Current page cached to avoid unnecessary switches

        uint8_t _pg0_led_onoff_reg[24]; // Cache of PG0 LED on/off registers (00h-17h)

        float _rext_ohm;     // R_ext resistor. Default should be 20kΩ (20000.0)

        uint8_t _gcc;

        bool _unlock_cmd();                              // Write 0xC5 to FEh to unlock FDh
        bool _select_page(uint8_t page);

        bool _write_reg(uint8_t reg, uint8_t data);      // Write within _page
        bool _read_reg(uint8_t reg, uint8_t *data);      // Read within _page
};

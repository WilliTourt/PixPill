#pragma once

#include "main.h"

#ifdef HAL_I2C_MODULE_ENABLED
    #include "i2c.h"
#else
    #error "At least one IIC port should be opened"
#endif

/* ============================================================
 * BMA530 Register Map
 * ============================================================ */
#define BMA530_REG_CHIP_ID         0x00
#define BMA530_REG_HEALTH_STATUS   0x02
#define BMA530_REG_CMD_SUSPEND     0x04
#define BMA530_REG_CONFIG_STATUS   0x10
#define BMA530_REG_SENSOR_STATUS   0x11
#define BMA530_REG_INT_STATUS_0    0x12
#define BMA530_REG_ACC_DATA_0      0x18  // X_L
#define BMA530_REG_ACC_DATA_5      0x1D  // Z_H
#define BMA530_REG_TEMP_DATA       0x1E
#define BMA530_REG_ACC_CONF_0      0x30  // Sensor enable
#define BMA530_REG_ACC_CONF_1      0x31  // ODR, BWP, Power Mode
#define BMA530_REG_ACC_CONF_2      0x32  // Range, IIR, Noise
#define BMA530_REG_INT1_CONF       0x34
#define BMA530_REG_INT2_CONF       0x35
#define BMA530_REG_INT_MAP_0       0x36
#define BMA530_REG_IF_CONF_1       0x3B
#define BMA530_REG_CMD             0x7E  // Soft reset

#define BMA530_CHIP_ID             0xC2

/* ============================================================
 * ACC_CONF_0 — Sensor enable
 * ============================================================ */
#define BMA530_ACC_ENABLE          0x0F
#define BMA530_ACC_DISABLE         0x00

/* ============================================================
 * ACC_CONF_1 — Power mode / Filter / ODR
 * ============================================================ */
#define BMA530_ACC_ODR_POS         0
#define BMA530_ACC_ODR_MSK         (0x0F << BMA530_ACC_ODR_POS)
#define BMA530_ACC_BWP_POS         4
#define BMA530_ACC_BWP_MSK         (0x07 << BMA530_ACC_BWP_POS)
#define BMA530_POWER_MODE_POS      7
#define BMA530_POWER_MODE_MSK      (1 << BMA530_POWER_MODE_POS)
#define BMA530_POWER_MODE_LPM      (0 << BMA530_POWER_MODE_POS)
#define BMA530_POWER_MODE_HPM      (1 << BMA530_POWER_MODE_POS)

/* ============================================================
 * ACC_CONF_2 — Range / IIR / Noise / DRDY
 * ============================================================ */
#define BMA530_ACC_RANGE_POS           0
#define BMA530_ACC_RANGE_MSK           (0x03 << BMA530_ACC_RANGE_POS)
#define BMA530_ACC_IIR_RO_POS          2
#define BMA530_ACC_IIR_RO_MSK          (0x03 << BMA530_ACC_IIR_RO_POS)
#define BMA530_ACC_NOISE_MODE_POS      4
#define BMA530_ACC_NOISE_MODE_MSK      (1 << BMA530_ACC_NOISE_MODE_POS)
#define BMA530_ACC_DRDY_AUTO_CLR_POS   7
#define BMA530_ACC_DRDY_AUTO_CLR_MSK   (1 << BMA530_ACC_DRDY_AUTO_CLR_POS)

/* ============================================================
 * INTx_CONF
 * ============================================================ */
#define BMA530_INT_MODE_LATCH      0x01
#define BMA530_INT_OD_POS          2
#define BMA530_INT_LVL_POS         3
#define BMA530_INT_LVL_HIGH        (1 << BMA530_INT_LVL_POS)

/* ============================================================
 * I2C
 * ============================================================ */
#define BMA530_I2C_ADDR            (0x18 << 1)  // 7-bit → 8-bit

/* Temperature: 0 LSB = 23 °C, 1 LSB / K */
#define BMA530_TEMP_OFFSET         23.0f


class BMA530 {
    public:

        /* ODR values — write into ACC_CONF_1[3:0] */
        enum class ODR : uint8_t {
            // ODRs available in Low Power Mode
            _1_5625HZ  = 0x00,
            _3_125HZ  = 0x01,
            _6_25HZ  = 0x02,

            // ODRs available in both Low Power and High Power Modes
            _12_5HZ  = 0x03,
            _25HZ    = 0x04,
            _50HZ    = 0x05,
            _100HZ   = 0x06,
            _200HZ   = 0x07,
            _400HZ   = 0x08,

            // Higher ODRs only available in High Power Mode
            _800HZ   = 0x09,
            _1600HZ  = 0x0A,
            _3200HZ  = 0x0B,
            _6400HZ  = 0x0C
        };

        enum class Range : uint8_t {
            _2G  = 0x00,
            _4G  = 0x01,
            _8G  = 0x02,
            _16G = 0x03
        };

        /* BWP (bandwidth parameter) — filter configuration for ACC_CONF_1 */
        enum class BWP : uint8_t {
            OSR4      = 0x00,  // HPM→OSR4 mode;  LPM→no averaging
            OSR2      = 0x01,  // HPM→OSR2 mode;  LPM→avg 2
            NORMAL    = 0x02,  // HPM→normal;      LPM→avg 4
            CIC       = 0x03,  // HPM→CIC;         LPM→avg 8
            AVG16     = 0x04,  // HPM→reserved;    LPM→avg 16
            AVG32     = 0x05,  // HPM→reserved;    LPM→avg 32
            AVG64     = 0x06,  // HPM→reserved;    LPM→avg 64
            RESERVED  = 0x07
        };

        /* Infinite impulse response filter roll-off for ACC_CONF_2 */
        enum class IIR : uint8_t {
            _20dB = 0x01,  // -20dB roll-off. More noise but faster
            _40dB = 0x02,  // -40dB roll-off (default). Balanced noise and delay
            _60dB = 0x03   // -60dB roll-off. More delay but less noise
        };

        /* Noise/performance mode for ACC_CONF_2 */
        enum class Noise : uint8_t {
            LOW_NOISE     = 0x00,  // default — lower noise
            LOW_POWER     = 0x01   // lower power, higher noise (HPM only!)
        };

        /* Power mode for ACC_CONF_1 bit7 (LPM ~15µA, HPM ~150µA) */
        enum class Power : uint8_t {
            LPM = 0x00,  // duty cycling — ODR max 400Hz
            HPM = 0x01   // continuous  — ODR up to 6.4kHz
        };

        BMA530(I2C_HandleTypeDef *hi2c);

        bool begin(ODR odr = ODR::_100HZ,
                   Range range = Range::_2G,
                   Power power = Power::LPM,
                   BWP bwp = BWP::NORMAL,
                   IIR iir = IIR::_40dB,
                   Noise noise = Noise::LOW_NOISE,
                   bool drdy_auto_clear = false);
        bool update();
        bool isDataReady();

        inline int16_t readAx() const { return _ax; } // Call after update()
        inline int16_t readAy() const { return _ay; } // Call after update()
        inline int16_t readAz() const { return _az; } // Call after update()

    private:
        I2C_HandleTypeDef *_hi2c;
        int16_t _ax, _ay, _az;

        bool _i2c_read(uint8_t reg, uint8_t *data, uint16_t size);
        bool _i2c_write(uint8_t reg, uint8_t *data, uint16_t size);
};

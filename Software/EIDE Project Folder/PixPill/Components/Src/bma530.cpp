#include "bma530.h"

BMA530::BMA530(I2C_HandleTypeDef *hi2c)
    : _hi2c(hi2c), _ax(0), _ay(0), _az(0) {}

bool BMA530::begin(ODR odr, Range range, Power power, BWP bwp, IIR iir, Noise noise, bool drdy_auto_clear) {

    // First read always NACKs — sensor detects SPI vs I2C this way
    uint8_t dummy;
    _i2c_write(BMA530_REG_CHIP_ID, &dummy, 1);
    HAL_Delay(2);

    // Verify chip ID
    uint8_t chip_id;
    if (!_i2c_read(BMA530_REG_CHIP_ID, &chip_id, 1)) return false;
    if (chip_id != BMA530_CHIP_ID) return false;

    // Write order: CONF_2 → CONF_1 → CONF_0 (enable at last)

    // ACC_CONF_2: DRDY auto-clear | Noise mode | IIR roll-off | Range
    uint8_t conf2 = (static_cast<uint8_t>(range)                                    & BMA530_ACC_RANGE_MSK)
                  | ((static_cast<uint8_t>(iir)     << BMA530_ACC_IIR_RO_POS)       & BMA530_ACC_IIR_RO_MSK)
                  | ((static_cast<uint8_t>(noise)   << BMA530_ACC_NOISE_MODE_POS)   & BMA530_ACC_NOISE_MODE_MSK);
    if (drdy_auto_clear) conf2 |= BMA530_ACC_DRDY_AUTO_CLR_MSK;
    if (!_i2c_write(BMA530_REG_ACC_CONF_2, &conf2, 1)) return false;

    // ACC_CONF_1: Power mode(bit7) | BWP(bit6..4) | ODR(bit3..0)
    uint8_t conf1 = ((static_cast<uint8_t>(power)   << BMA530_POWER_MODE_POS)   & BMA530_POWER_MODE_MSK)
                  | ((static_cast<uint8_t>(bwp)     << BMA530_ACC_BWP_POS)      & BMA530_ACC_BWP_MSK)
                  | (static_cast<uint8_t>(odr)                                  & BMA530_ACC_ODR_MSK);
    if (!_i2c_write(BMA530_REG_ACC_CONF_1, &conf1, 1)) return false;

    uint8_t conf0 = BMA530_ACC_ENABLE;
    if (!_i2c_write(BMA530_REG_ACC_CONF_0, &conf0, 1)) return false;

    return true;
}

bool BMA530::isDataReady() {
    uint8_t status;
    if (!_i2c_read(BMA530_REG_SENSOR_STATUS, &status, 1)) return false;
    return status & 0x01;  // bit0 = acc_data_rdy (datasheet §6.1.2)
}

bool BMA530::update() {
    uint8_t buf[6];
    if (!_i2c_read(BMA530_REG_ACC_DATA_0, buf, 6)) return false;

    _ax = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    _ay = static_cast<int16_t>(buf[2] | (buf[3] << 8));
    _az = static_cast<int16_t>(buf[4] | (buf[5] << 8));

    return true;
}



bool BMA530::_i2c_read(uint8_t reg, uint8_t *data, uint16_t size) {
    return HAL_I2C_Mem_Read(_hi2c, BMA530_I2C_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, data, size, 100) == HAL_OK;
}

bool BMA530::_i2c_write(uint8_t reg, uint8_t *data, uint16_t size) {
    return HAL_I2C_Mem_Write(_hi2c, BMA530_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, data, size, 100) == HAL_OK;
}

/* ============================================================
 * IS31FL3736 driver implementation
 * ============================================================ */
#include "is31fl3736.h"
#include <cstring>

/* ---- PWM address: (sw_index * 16) + (cs_index * 2) ---- */
static inline uint8_t _pwm_addr(uint8_t cs, uint8_t sw) {
    return ((sw - 1) * 16) + ((cs - 1) * 2);
}

/* ---- LED On/Off address (PG0): 2 bytes per SW ---- */
static inline uint8_t _onoff_addr(uint8_t cs, uint8_t sw) {
    return ((sw - 1) * 2) + (cs <= 4 ? 0 : 1);
}

/* ---- Bit position within On/Off register (D0/D2/D4/D6 for both CS groups) ---- */
static inline uint8_t _onoff_bit(uint8_t cs) {
    return ((cs - 1) & 0x03) * 2;  // CS1→0, CS2→2, CS3→4, CS4→6; same for CS5-CS8
}

IS31FL3736::IS31FL3736(I2C_HandleTypeDef *hi2c, float r_ext, uint8_t i2c_addr)
    : _hi2c(hi2c), _addr(i2c_addr), _page(0xFF), _rext_ohm(r_ext), _gcc(0) {
    memset(_pg0_led_onoff_reg, 0, 24);
}

/* ============================================================
 * I2C helpers
 * ============================================================ */
bool IS31FL3736::_write_reg(uint8_t reg, uint8_t data) {
    return HAL_I2C_Mem_Write(_hi2c, _addr, reg,
                             I2C_MEMADD_SIZE_8BIT, &data, 1, 100) == HAL_OK;
}

bool IS31FL3736::_read_reg(uint8_t reg, uint8_t *data) {
    return HAL_I2C_Mem_Read(_hi2c, _addr, reg,
                            I2C_MEMADD_SIZE_8BIT, data, 1, 100) == HAL_OK;
}

/* ============================================================
 * Page select — unlock → write FDh → one-shot re-lock
 * ============================================================ */
bool IS31FL3736::_unlock_cmd() {
    return _write_reg(IS31_REG_CMD_WR_LOCK, IS31_UNLOCK_CMD);
}

bool IS31FL3736::_select_page(uint8_t page) {
    if (page == _page) return true;
    if (!_unlock_cmd()) return false;
    if (!_write_reg(IS31_REG_CMD, page)) return false;
    _page = page;
    return true;
}

/* ============================================================
 * begin — unlock, configure, ready
 * ============================================================ */
bool IS31FL3736::begin() {
    return begin(0);  // GCC=0 → LEDs off
}

bool IS31FL3736::begin(uint8_t gcc) {
    // Select PG3 (Function) and configure
    if (!_select_page(IS31_PAGE_FUNC)) return false;

    // Configuration: normal operation, PWM mode, no sync
    uint8_t conf = IS31_CONF_SSD;   // bit0=1 → normal operation
    if (!_write_reg(IS31_REG_CONF, conf)) return false;

    // Set global current
    if (!setGCC(gcc)) return false;

    // Default: 32k pull-up/down for de-ghost
    if (!_write_reg(IS31_REG_SW_PULLUP, 0x07)) return false;   // 111=32k
    if (!_write_reg(IS31_REG_CS_PULLDOWN, 0x07)) return false; // 111=32k

    // Clear all LEDs
    if (!ledOffAll()) return false;

    return true;
}

/* ============================================================
 * shutdown
 * ============================================================ */
bool IS31FL3736::shutdown() {
    if (!_select_page(IS31_PAGE_FUNC)) return false;
    return _write_reg(IS31_REG_CONF, 0x00);  // SSD=0 → shutdown
}

/* ============================================================
 * LED on/off (PG0)
 * ============================================================ */

/*** (See IS31FL3736 datasheet, page 15) **********************************************************

Table 6  Page 0 (PG0, 0x00): LED Control Register 

              LED Location          | LED On/Off Register | LED Open Register | LED Short Register
    SW1 (CS1~ CS4) | SW1 (CS5~ CS8) |      00h | 01h      |     18h | 19h     |     30h | 31h 
    SW2 (CS1~ CS4) | SW2 (CS5~ CS8) |      02h | 03h      |     1Ah | 1Bh     |     32h | 33h 
    SW3 (CS1~ CS4) | SW3 (CS5~ CS8) |      04h | 05h      |     1Ch | 1Dh     |     34h | 35h 
    SW4 (CS1~ CS4) | SW4 (CS5~ CS8) |      06h | 07h      |     1Eh | 1Fh     |     36h | 37h 
    SW5 (CS1~ CS4) | SW5 (CS5~ CS8) |      08h | 09h      |     20h | 21h     |     38h | 39h 
    SW6 (CS1~ CS4) | SW6 (CS5~ CS8) |      0Ah | 0Bh      |     22h | 23h     |     3Ah | 3Bh 
    SW7 (CS1~ CS4) | SW7 (CS5~ CS8) |      0Ch | 0Dh      |     24h | 25h     |     3Ch | 3Dh 
    SW8 (CS1~ CS4) | SW8 (CS5~ CS8) |      0Eh | 0Fh      |     26h | 27h     |     3Eh | 3Fh 
    SW9 (CS1~ CS4) | SW9 (CS5~ CS8) |      10h | 11h      |     28h | 29h     |     40h | 41h 
    SW10(CS1~ CS4) | SW10(CS5~ CS8) |      12h | 13h      |     2Ah | 2Bh     |     42h | 43h 
    SW11(CS1~ CS4) | SW11(CS5~ CS8) |      14h | 15h      |     2Ch | 2Dh     |     44h | 45h 
    SW12(CS1~ CS4) | SW12(CS5~ CS8) |      16h | 17h      |     2Eh | 2Fh     |     46h | 47h 


Table 7-1  00h, 02h, ... 16h  LED On/Off Register (CS1~CS4) 
Bit  | D7   | D6   | D5   | D4   | D3   | D2   | D1   | D0 
Name | -    | CCS4 | -    | CCS3 | -    | CCS2 | -    | CCS1


Table 7-2  01h, 03h, ... 17h  LED On/Off Register (CS5~CS8) 
Bit  | D7   | D6   | D5   | D4   | D3   | D2   | D1   | D0 
Name | -    | CCS8 | -    | CCS7 | -    | CCS6 | -    | CCS5

**************************************************************************************************/
bool IS31FL3736::ledSet(uint8_t cs, uint8_t sw, bool on) {
    if (cs < 1 || cs > IS31_CS_COUNT || sw < 1 || sw > IS31_SW_COUNT) return false;

    if (!_select_page(IS31_PAGE_LED)) return false;

    uint8_t addr = _onoff_addr(cs, sw);
    uint8_t bit  = _onoff_bit(cs);

    // Set / reset bit: reg byte |(&) (1 << bit) mask
    if (on) {
        _pg0_led_onoff_reg[addr] |= (1 << bit);
    } else {
        _pg0_led_onoff_reg[addr] &= ~(1 << bit);
    }

    return _write_reg(addr, _pg0_led_onoff_reg[addr]);
}

bool IS31FL3736::ledOn(uint8_t cs, uint8_t sw) {
    return ledSet(cs, sw, true);
}

bool IS31FL3736::ledOff(uint8_t cs, uint8_t sw) {
    return ledSet(cs, sw, false);
}

bool IS31FL3736::ledOnAll(uint8_t gcc) {
    bool ret = true;

    if (!_select_page(IS31_PAGE_FUNC)) return false;
    ret &= _write_reg(IS31_REG_GCC, gcc);

    if (!_select_page(IS31_PAGE_LED)) return false;
    memset(_pg0_led_onoff_reg, 0x55, 24);
    for (uint8_t i = 0; i < 24; i++) {
        ret &= _write_reg(i, 0x55);
    }

    // if (!_select_page(IS31_PAGE_PWM)) return false;
    // for (uint8_t sw = 1; sw <= IS31_SW_COUNT; sw++) {
    //     for (uint8_t cs = 1; cs <= IS31_CS_COUNT; cs++) {
    //         uint8_t addr = (sw - 1) * 16 + (cs - 1) * 2;
    //         ret &= _write_reg(addr, 0xFF);
    //     }
    // }

    return ret;
}

bool IS31FL3736::ledOffAll() {
    bool ret = true;

    if (!_select_page(IS31_PAGE_LED)) return false;
    memset(_pg0_led_onoff_reg, 0x00, 24);
    for (uint8_t i = 0; i < 24; i++) {
        ret &= _write_reg(i, 0x00);
    }

    // if (!_select_page(IS31_PAGE_PWM)) return false;
    // for (uint8_t sw = 1; sw <= IS31_SW_COUNT; sw++) {
    //     for (uint8_t cs = 1; cs <= IS31_CS_COUNT; cs++) {
    //         uint8_t addr = (sw - 1) * 16 + (cs - 1) * 2;
    //         ret &= _write_reg(addr, 0x00);
    //     }
    // }

    return ret;
}

/* ============================================================
 * PWM (PG1)
 * ============================================================ */
bool IS31FL3736::setPWM(uint8_t cs, uint8_t sw, uint8_t val) {
    if (cs < 1 || cs > IS31_CS_COUNT || sw < 1 || sw > IS31_SW_COUNT) return false;

    if (!_select_page(IS31_PAGE_PWM)) return false;
    return _write_reg(_pwm_addr(cs, sw), val);
}

bool IS31FL3736::setPWMAll(uint8_t val) {
    if (!_select_page(IS31_PAGE_PWM)) return false;
    for (uint8_t sw = 1; sw <= 12; sw++) {
        for (uint8_t cs = 1; cs <= 8; cs++) {
            if (!_write_reg((sw-1)*16 + (cs-1)*2, val)) return false;
        }
    }

    return true;
}

/* ============================================================
 * Breath mode select (PG2)
 * ============================================================ */
bool IS31FL3736::setBreathMode(uint8_t cs, uint8_t sw, uint8_t mode) {
    if (cs < 1 || cs > IS31_CS_COUNT || sw < 1 || sw > IS31_SW_COUNT) return false;
    if (mode > 3) return false;

    if (!_select_page(IS31_PAGE_BREATH)) return false;
    return _write_reg(_pwm_addr(cs, sw), mode & 0x03);
}

/* ============================================================
 * Global current (PG3 01h)
 * ============================================================ */
bool IS31FL3736::setGCC(uint8_t gcc) {
    _gcc = gcc;
    if (getIout() > MAX_TOTAL_CURRENT_MA || getIled() > MAX_LED_CURRENT_MA) { return false; }

    if (!_select_page(IS31_PAGE_FUNC)) return false;
    return _write_reg(IS31_REG_GCC, gcc);
}

/* ============================================================
 * Breath timing — sets T1-T4 + loop for one ABM
 * ============================================================ */
bool IS31FL3736::setBreathTiming(uint8_t abm,
                                   BreathTime t1_rise,
                                   BreathTime t2_hold,
                                   BreathTime t3_fall,
                                   BreathTime t4_off) {
    if (abm < 1 || abm > 3) return false;

    if (!_select_page(IS31_PAGE_FUNC)) return false;

    uint8_t base = 0x02 + (abm - 1) * 4;  // 02h(ABM1), 06h(ABM2), 0Ah(ABM3)

    uint8_t t1t2 = (static_cast<uint8_t>(t1_rise) << 5)
                 | (static_cast<uint8_t>(t2_hold) << 1);
    if (!_write_reg(base, t1t2)) return false;

    uint8_t t3t4 = (static_cast<uint8_t>(t3_fall) << 5)
                 | (static_cast<uint8_t>(t4_off) << 1);
    if (!_write_reg(base + 1, t3t4)) return false;

    // Loop: LB=T4(0b11), LE=off(0b00), LTA=0(endless), LTB=0(endless)
    if (!_write_reg(base + 2, 0xC0)) return false;  // LB=T4
    if (!_write_reg(base + 3, 0x00)) return false;  // LTB=0 → endless loop

    // Commit timing registers
    return _write_reg(IS31_REG_TIME_UPDATE, 0x00);
}

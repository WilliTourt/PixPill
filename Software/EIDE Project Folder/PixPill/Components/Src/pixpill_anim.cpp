#include "pixpill_anim.h"
#include "liquidsim.h"  // for LIQUID_LED_MASK, LIQUID_LED_ROW, LIQUID_LED_COL
#include <cstring>

PixPillAnim::PixPillAnim(IS31FL3736 &is31)
    : _is31(is31) {}

void PixPillAnim::start(Anim anim) {
    _is31.ledOffAll();
    _current = anim;
    _start_ms = HAL_GetTick();
}

bool PixPillAnim::tick(uint16_t ms_per_frame) {
    if (_current == Anim::NONE) return false;

    uint32_t elapsed = HAL_GetTick() - _start_ms;
    bool running = false;

    switch (_current) {
        case Anim::BOOT:     running = _tick_boot(elapsed, ms_per_frame);   break;
        case Anim::SHUTDOWN: running = _tick_shutdown(elapsed);             break;
        case Anim::ERR:      running = _tick_err(elapsed);                  break;
        case Anim::CHARGING: running = _tick_charging(elapsed);             break;
        default: break;
    }

    if (!running) {
        _current = Anim::NONE;
    }
    return running;
}

void PixPillAnim::stop() {
    _is31.ledOffAll();
    _current = Anim::NONE;
}

// PIXPILL BITMAP
// "PIXPILL" scrolls in from the right, fills the full 14-row height
// Each uint16_t = one column: bit0 = bottom row, bit15 = top row
// Scrolls like: P I X P I L L
static const uint8_t SCROLL_NUM_COLS = 36;
static const uint16_t scroll_bitmap[36] = {
    0,  // blank
    0,
    // P
    0b0111111111111110,
    0b0100000010000000,
    0b0100000010000000,
    0b0011111100000000,
    0,  // gap
    // I
    0b0100000000000010,
    0b0111111111111110,
    0b0100000000000010,
    0,
    // X
    0b0111100000011110,
    0b0000011001100000,
    0b0000000110000000,
    0b0000011001100000,
    0b0111100000011110,
    0,
    // P
    0b0111111111111110,
    0b0100000010000000,
    0b0100000010000000,
    0b0011111100000000,
    0,
    // I
    0b0100000000000010,
    0b0111111111111110,
    0b0100000000000010,
    0,
    // L
    0b0111111111111110,
    0b0000000000000010,
    0b0000000000000010,
    0b0000000000000010,
    0,
    // L
    0b0111111111111110,
    0b0000000000000010,
    0b0000000000000010,
    0b0000000000000010
};

bool PixPillAnim::_tick_boot(uint32_t elapsed, uint16_t ms_per_col) {
    uint32_t total_cols = SCROLL_NUM_COLS + 6;  // columns to scroll through
    uint32_t max_ms = total_cols * ms_per_col;

    if (elapsed >= max_ms) {
        _is31.ledOffAll();
        return false;  // done
    }

    uint32_t offset = elapsed / ms_per_col;
    _is31.ledOffAll();

    // Draw bitmap at current scroll position
    for (uint8_t col = 0; col < 6; col++) {

        // bitmap column index
        int16_t bmp_col = (int16_t)offset - (5 - col);
        if (bmp_col < 0 || bmp_col >= SCROLL_NUM_COLS) continue;

        uint16_t bits = scroll_bitmap[bmp_col];
        for (uint8_t row = 0; row < 16; row++) {
            // bit15 = row0 (top), bit0 = row15 (bottom)
            if (bits & (1 << (15 - row))) {
                uint8_t led_row = row + 1;
                if (led_row < 18 && col < 6 && LIQUID_LED_MASK[led_row][col]) {
                    _set_pixel(led_row, col, 0xff);
                }
            }
        }
    }

    return true;
}

// ===================== Shutdown Animation =====================
// Full bright -> rows fade from top (row 0) down to bottom (row 17), ~600ms

bool PixPillAnim::_tick_shutdown(uint32_t elapsed) {
    if (elapsed >= 600) {
        _is31.ledOffAll();
        return false;  // done
    }

    float progress = (float)elapsed / 600.0f;  // 0 -> 1
    // Row that's currently being erased (0 -> 17)
    uint8_t erase_row = (uint8_t)(progress * 17.0f);

    for (uint8_t r = 0; r < 18; r++) {
        for (uint8_t c = 0; c < 6; c++) {
            if (!LIQUID_LED_MASK[r][c]) continue;
            if (r <= erase_row) {
                _set_pixel(r, c, 0);  // erased
            } else {
                _set_pixel(r, c, 0xff);  // still lit
            }
        }
    }
    return true;
}

// ===================== ERR Bitmap =====================
// Flash "ERR" text on the LED array
// Each uint32_t = one column: bit17=top row0, bit0=bottom row17
static const uint32_t ERR_BITMAP[6] = {
    0b001111010010100100,
    0b001000010010100100,
    0b001000010010100100,
    0b001110011100111000,
    0b001000010010100100,
    0b001111011100111000
};

bool PixPillAnim::_tick_err(uint32_t elapsed) {
    if (!_lut_ready) _init_lut();
    uint32_t cycle = elapsed % 500;  // 500ms cycle
    _is31.setPWMAll(0xff);
    if (cycle < 250) {
        for (uint8_t col = 0; col < 6; col++) {
            uint32_t bits = ERR_BITMAP[col];
            for (uint8_t row = 0; row < 18; row++) {
                if (bits & (1UL << (17 - row))) {
                    if (!LIQUID_LED_MASK[row][col]) continue;
                    _led_set_pwm(row, col, 0xff);
                }
            }
        }
    } else {
        _is31.ledOffAll();
    }
    return true;  // ERR never self-terminates, must call stop()
}

// ===================== Charging Animation =====================
static const uint32_t CHARGING_BAT_FRAME_BITMAP[6] = {
    0b000111111111111000,
    0b000100000000001000,
    0b000100000000001100,
    0b000100000000001100,
    0b000100000000001000,
    0b000111111111111000
};

static const uint32_t CHARGING_BAT_FILL_MASK[6] = {
    0b000000000000000000,
    0b000011111100000000,
    0b000011111100000000,
    0b000011111100000000,
    0b000011111100000000,
    0b000000000000000000
};

bool PixPillAnim::_tick_charging(uint32_t elapsed) {
    // Frame always on, fill breathes: 1.0s period triangle wave
    uint32_t t = elapsed % 1000;
    uint8_t pwm;
    if (t < 500) {
        pwm = (uint8_t)((float)t / 500.0f * 0xff);
    } else {
        pwm = (uint8_t)((float)(1000 - t) / 500.0f * 0xff);
    }

    _draw_bitmap(CHARGING_BAT_FRAME_BITMAP, 0xff);
    _draw_bitmap(CHARGING_BAT_FILL_MASK, pwm);

    return true;  // CHARGING never self-terminates
}



// LED index lut
// -1 = invalid cell (outside pill shape)
int8_t PixPillAnim::_row_col_to_led[18][6];
bool   PixPillAnim::_lut_ready = false;

void PixPillAnim::_init_lut() {
    memset(_row_col_to_led, -1, sizeof(_row_col_to_led));
    for (uint8_t i = 0; i < 96; i++) {
        _row_col_to_led[LIQUID_LED_ROW[i]][LIQUID_LED_COL[i]] = (int8_t)i;
    }
    _lut_ready = true;
}

void PixPillAnim::_led_set_pwm(uint8_t row, uint8_t col, uint8_t pwm) {
    int8_t led = _row_col_to_led[row][col];
    if (led < 0) return;
    
    uint8_t cs = (led / 12) + 1;
    uint8_t sw = (led % 12) + 1;

    _is31.setPWM(cs, sw, pwm);
    if (pwm > 0) {
        _is31.ledOn(cs, sw);
    } else {
        _is31.ledOff(cs, sw);
    }
}

void PixPillAnim::_set_pixel(uint8_t row, uint8_t col, uint8_t pwm) {
    if (!_lut_ready) {
        _init_lut();
    }
    _led_set_pwm(row, col, pwm);
}

void PixPillAnim::_draw_bitmap(const uint32_t bitmap[6], uint8_t pwm) {
    if (!_lut_ready) {
        _init_lut();
    }

    for (uint8_t col = 0; col < 6; col++) {
        uint32_t bits = bitmap[col];
        for (uint8_t row = 0; row < 18; row++) {
            if (!(bits & (1UL << (17 - row)))) continue;
            if (!LIQUID_LED_MASK[row][col]) continue;
            _led_set_pwm(row, col, pwm);
        }
    }
}

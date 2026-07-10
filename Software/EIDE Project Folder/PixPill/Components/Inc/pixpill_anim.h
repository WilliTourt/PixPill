#pragma once

#include "main.h"
#include "is31fl3736.h"

/**
 * LED array animations for PixPill
 * All functions are non-blocking: call them per-frame, they advance internal state.
 */

class PixPillAnim {
public:
    enum class Anim {
        NONE,
        BOOT,       // Boot: "PIXPILL" scroll
        SHUTDOWN,   // Shutdown: rows fade top-to-bottom
        ERR,        // Error: full-screen fast blink
        CHARGING    // Charging: battery outline with breathing fill
    };

    PixPillAnim(IS31FL3736 &is31);

    void start(Anim anim);
    bool tick(uint16_t ms_per_frame = 80);
    void stop();

private:
    IS31FL3736 &_is31;
    Anim _current = Anim::NONE;
    uint32_t _start_ms = 0;

    // LED index lookup
    static int8_t _row_col_to_led[18][6];
    static bool   _lut_ready;

    // Animation implementations
    bool _tick_boot(uint32_t elapsed, uint16_t ms_per_col = 80);
    bool _tick_shutdown(uint32_t elapsed);
    bool _tick_err(uint32_t elapsed);
    bool _tick_charging(uint32_t elapsed);

    // Helpers
    void _init_lut();
    void _led_set_pwm(uint8_t row, uint8_t col, uint8_t pwm);
    void _set_pixel(uint8_t row, uint8_t col, uint8_t pwm);
    void _draw_bitmap(const uint32_t bitmap[6], uint8_t pwm);
};

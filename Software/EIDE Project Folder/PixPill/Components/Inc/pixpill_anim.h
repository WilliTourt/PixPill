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
        BOOT,       // Boot: capsule outline → fill → flash
        SHUTDOWN,   // Shutdown: full bright → rows fade top-to-bottom
        ERR,        // Error: full-screen fast blink (non-blocking override)
        CHARGING    // Charging: bottom breathing glow
    };

    PixPillAnim(IS31FL3736 &is31);

    /** Start an animation. Returns immediately (non-blocking). */
    void start(Anim anim);

    /** Call every frame. Returns true while animation is active. */
    bool tick(uint16_t ms_per_frame = 80);

    /** Force-stop current animation and clear LEDs */
    void stop();

private:
    IS31FL3736 &_is31;

    Anim _current = Anim::NONE;
    uint32_t _start_ms = 0;

    // Animation implementations
    bool _tick_boot(uint32_t elapsed, uint16_t ms_per_col = 80);
    bool _tick_shutdown(uint32_t elapsed);
    bool _tick_err(uint32_t elapsed);
    bool _tick_charging(uint32_t elapsed);

    // Helpers
    void _set_all_pwm(uint8_t val);
    void _set_pixel(uint8_t row, uint8_t col, uint8_t pwm);
    void _draw_bitmap(const uint32_t bitmap[6], uint8_t pwm);
    void _clear_all();
};

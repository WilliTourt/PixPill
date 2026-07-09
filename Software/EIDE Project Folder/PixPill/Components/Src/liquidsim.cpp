#include "liquidsim.h"
#include <cstring>

#include "inv_sqrt_table.h"

LiquidSim::LiquidSim(BMA530 &accel, IS31FL3736 &is31)
    : _accel(accel), _is31(is31), _random(131) {
    memset(_led_buf, 0, sizeof(_led_buf));
    _gravity.x = 0;
    _gravity.y = 0;
}

SimBase::Status LiquidSim::init() {
    // Build LED lookup table: grid (row,col) -> LED index
    memset(_led_lut, -1, sizeof(_led_lut));
    for (uint8_t led = 0; led < LIQUID_LED_COUNT; led++) {
        _led_lut[LIQUID_LED_ROW[led]][LIQUID_LED_COL[led]] = led;
    }

    for (uint8_t i = 0; i < LIQUID_PARTICLE_COUNT; i++) {
        _particles[i].vx = 0;
        _particles[i].vy = 0;

        bool valid = false;
        while (!valid) {
            uint8_t row = _random % 18;
            uint8_t col = _random % 6;

            _random = (_random * 131 + 53) & 0xBEEF;

            if (!LIQUID_LED_MASK[row][col]) continue;

            _particles[i].x = (float)col + 0.5f;
            _particles[i].y = (float)row + 0.5f;
            valid = true;
        }
    }
    return Status::OK;
}

/*
 * Read accelerometer, convert to gravity vector
 * Display is on the back of the accelerometer,
 * and the LED matrix coords is rotated 90° clockwise from PCB orientation,
 * so _gravity.x = -ay, _gravity.y = ax
 */
void LiquidSim::_update_gravity() {
    _accel.update();
    int16_t ax_raw = _accel.readAx();
    int16_t ay_raw = _accel.readAy();

    float ax = (float)(ax_raw) / 16384.0f;
    float ay = (float)(ay_raw) / 16384.0f;

    if (ax < 0.03f && ax > -0.03f) ax = 0;
    if (ay < 0.03f && ay > -0.03f) ay = 0;

    _gravity.x = -ay * LIQUID_GRAVITY_SCALE;
    _gravity.y = ax * LIQUID_GRAVITY_SCALE;
}

/*
 * Continuous bevel boundary: capsule-shaped container
 * Top: y 0→1.5, width 2→5; Mid: y 1.5→15.5, full width 0→5
 * Bottom: y 15.5→17, width 5→2
 * 
 * (Visualized in visual_simulation.py)
 */
void LiquidSim::_get_boundary(float y, float &left, float &right) {
    if (y <= 1.5f) {
        float t = y / 1.5f;
        left  = 2.0f - t * 2.0f;
        right = 3.0f + t * 2.0f;
    } else if (y <= 15.5f) {
        left  = 0.0f;
        right = 5.0f;
    } else {
        float t = (y - 15.5f) / 1.5f;
        left  = 0.0f + t * 2.0f;
        right = 5.0f - t * 2.0f;
    }
}

/*
 * Soft-wall boundary: repulsive force near walls, hard clamp at limit
 */
void LiquidSim::_clamp_particle_to_shape(LiquidParticle_t &p) {
    float left, right;
    _get_boundary(p.y, left, right);

    float margin = WALL_PUSH_MARGIN;
    float strength = WALL_PUSH_STRENGTH;

    // Bottom support: only push up when particle is within valid X range
    // Particles outside the narrow neck should fall freely toward row=17
    if (p.y > 17.0f - margin) {
        float left17, right17;
        _get_boundary(17.0f, left17, right17);
        if (p.x >= left17 - margin && p.x <= right17 + margin) {
            float depth = p.y - (17.0f - margin);
            p.vy -= depth * strength;
            if (p.vy > 0) p.vy *= 0.2f;
        }
    }
    if (p.y < margin) {
        float depth = margin - p.y;
        p.vy += depth * strength;
        if (p.vy < 0) p.vy *= 0.2f;
    }

    if (p.x < left + margin) {
        p.vx += (left + margin - p.x) * strength;
    }
    if (p.x > right - margin) {
        p.vx -= (p.x - (right - margin)) * strength;
    }

    if (p.x < left) { p.x = left; p.vx = 0; }
    else if (p.x > right) { p.x = right; p.vx = 0; }

    if (p.y < 0)  p.y = 0;
    if (p.y > 17) p.y = 17;
}

/*
 * Euler integration: apply gravity + damping, update position
 */
void LiquidSim::_update_particles(float dt) {
    for (uint8_t i = 0; i < LIQUID_PARTICLE_COUNT; i++) {
        _particles[i].vx += _gravity.x * dt;
        _particles[i].vy += _gravity.y * dt;

        _particles[i].vx *= LIQUID_DAMPING;
        _particles[i].vy *= LIQUID_DAMPING;

        _particles[i].x += _particles[i].vx * dt;
        _particles[i].y += _particles[i].vy * dt;

        _clamp_particle_to_shape(_particles[i]);
    }
}

/*
 * Collision and density pressure
 * Collision: linear push (matching reference repo "embedded-liquid-simulation-main")
 */
void LiquidSim::_process_collisions() {
    float min_dist_sq = LIQUID_MIN_DIST * LIQUID_MIN_DIST;
    float attract_sq  = LIQUID_ATTRACT_RADIUS * LIQUID_ATTRACT_RADIUS;

    // 1: Linear push and elastic velocity exchange
    for (uint8_t iter = 0; iter < LIQUID_COLLISIONS_ITERS; iter++) {
        for (uint8_t p1 = 0; p1 < LIQUID_PARTICLE_COUNT; p1++) {
            for (uint8_t p2 = p1 + 1; p2 < LIQUID_PARTICLE_COUNT; p2++) {

                float dx = _particles[p2].x - _particles[p1].x;
                float dy = _particles[p2].y - _particles[p1].y;
                float dist_sq = dx * dx + dy * dy;

                if (dist_sq < 0.001f) {
                    _particles[p1].x -= 0.03f;
                    _particles[p1].y -= 0.03f;
                    _particles[p2].x += 0.03f;
                    _particles[p2].y += 0.03f;
                    continue;
                }

                if (dist_sq < min_dist_sq) {
                    // LUT-based 1/sqrt(dist_sq) (256 entries, 512 bytes total)
                    float inv_dist = inv_sqrt_lookup(dist_sq);
                    float dist = 1.0f / inv_dist;

                    float ex = dx * inv_dist;
                    float ey = dy * inv_dist;

                    // // Square push
                    // float overlap = (1.0f / inv_dist > 0.0f) ? (LIQUID_MIN_DIST - (1.0f / inv_dist)) : 0.0f;
                    // if (overlap < 0.0f) overlap = 0.0f;
                    // float push = overlap * overlap * 0.45f;
                    // _particles[i].x -= ex * push;
                    // _particles[i].y -= ey * push;
                    // _particles[j].x += ex * push;
                    // _particles[j].y += ey * push;

                    // Linear push
                    float overlap = 0.5f * (LIQUID_MIN_DIST - dist);
                    _particles[p1].x -= overlap * ex;
                    _particles[p1].y -= overlap * ey;
                    _particles[p2].x += overlap * ex;
                    _particles[p2].y += overlap * ey;

                    // Elastic collision with damping
                    float vA = _particles[p1].vx * ex + _particles[p1].vy * ey;
                    float vB = _particles[p2].vx * ex + _particles[p2].vy * ey;
                    float avg = (vA + vB) * 0.5f * LIQUID_COLLISION_DAMPING;

                    _particles[p1].vx += (avg - vA) * ex;
                    _particles[p1].vy += (avg - vA) * ey;
                    _particles[p2].vx += (avg - vB) * ex;
                    _particles[p2].vy += (avg - vB) * ey;
                }
            }
        }
    }

    // 2: Surface tension (disabled, kept for future tuning)
    if (LIQUID_ATTRACT_STRENGTH > 0.0f) {
        for (uint8_t p1 = 0; p1 < LIQUID_PARTICLE_COUNT; p1++) {
            for (uint8_t p2 = p1 + 1; p2 < LIQUID_PARTICLE_COUNT; p2++) {
                float dx = _particles[p2].x - _particles[p1].x;
                float dy = _particles[p2].y - _particles[p1].y;
                float dist_sq = dx * dx + dy * dy;

                if (dist_sq >= min_dist_sq && dist_sq < attract_sq) {
                    float inv_dist = inv_sqrt_lookup(dist_sq);
                    float dist = 1.0f / inv_dist;

                    float ex = dx * inv_dist;
                    float ey = dy * inv_dist;
                    float force = LIQUID_ATTRACT_STRENGTH * (LIQUID_ATTRACT_RADIUS - dist);
                    _particles[p1].vx += force * ex;
                    _particles[p1].vy += force * ey;
                    _particles[p2].vx -= force * ex;
                    _particles[p2].vy -= force * ey;
                }
            }
        }
    }  // LIQUID_ATTRACT_STRENGTH > 0

    // 3: Density pressure: push velocity from dense toward sparse areas
    if (LIQUID_PRESSURE_STRENGTH > 0.0f) {
        float density[18][6];
        _compute_density(density);

        for (uint8_t i = 0; i < LIQUID_PARTICLE_COUNT; i++) {
            int8_t row = (int8_t)(_particles[i].y + 0.5f);
            int8_t col = (int8_t)(_particles[i].x + 0.5f);

            if (row < 0 || row >= 18 || col < 0 || col >= 6) continue;
            if (!LIQUID_LED_MASK[row][col]) continue;

            float d = density[row][col];
            if (d <= LIQUID_DENSITY_PRESSURE_THRESHOLD) continue;

            float excess = (d - LIQUID_DENSITY_PRESSURE_THRESHOLD) / DENSITY_MAX_BRIGHTNESS;

            // Weighted push toward lowest-density neighbor
            float push_x = 0.0f, push_y = 0.0f, total_w = 0.0f;
            for (int8_t dr = -1; dr <= 1; dr++) {
                for (int8_t dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int8_t nr = row + dr, nc = col + dc;
                    if (nr < 0 || nr >= 18 || nc < 0 || nc >= 6) continue;
                    if (!LIQUID_LED_MASK[nr][nc]) continue;

                    float diff = d - density[nr][nc];
                    if (diff > 0.0f) {
                        float w = diff / DENSITY_MAX_BRIGHTNESS;
                        push_x += (float)dc * w;
                        push_y += (float)dr * w;
                        total_w += w;
                    }
                }
            }

            if (total_w > 0.001f) {
                push_x /= total_w;
                push_y /= total_w;
                _particles[i].vx += push_x * excess * LIQUID_PRESSURE_STRENGTH;
                _particles[i].vy += push_y * excess * LIQUID_PRESSURE_STRENGTH;
            }
        }
    }

    for (uint8_t i = 0; i < LIQUID_PARTICLE_COUNT; i++) {
        _clamp_particle_to_shape(_particles[i]);
        // Safety: reset out-of-bounds particles to center
        if (_particles[i].y < 0.0f || _particles[i].y > 18.0f ||
            _particles[i].x < -1.0f || _particles[i].x > 7.0f) {
            // Reset to safe center zone
            _particles[i].x = 2.5f;
            _particles[i].y = 9.0f;
            _particles[i].vx = 0.0f;
            _particles[i].vy = 0.0f;
        }
    }
}

/*
 * Bilinear interpolation density field
 * Each particle contributes to 4 nearest LED cells, weighted by distance
 */
void LiquidSim::_compute_density(float density[18][6]) {
    memset(density, 0, sizeof(float) * 18 * 6);

    for (uint8_t p = 0; p < LIQUID_PARTICLE_COUNT; p++) {
        float fx = _particles[p].x;
        float fy = _particles[p].y;

        int8_t col0 = (int8_t)fx;
        int8_t col1 = col0 + 1;
        int8_t row0 = (int8_t)fy;
        int8_t row1 = row0 + 1;

        float weight_x1 = fx - (float)col0;
        float weight_x0 = 1.0f - weight_x1;
        float weight_y1 = fy - (float)row0;
        float weight_y0 = 1.0f - weight_y1;

        float weight00 = weight_x0 * weight_y0;
        float weight10 = weight_x1 * weight_y0;
        float weight01 = weight_x0 * weight_y1;
        float weight11 = weight_x1 * weight_y1;

        if (row0 >= 0 && row0 < 18 && col0 >= 0 && col0 < 6 && LIQUID_LED_MASK[row0][col0])
            density[row0][col0] += DENSITY_PER_PARTICLE * weight00;
        if (row0 >= 0 && row0 < 18 && col1 >= 0 && col1 < 6 && LIQUID_LED_MASK[row0][col1])
            density[row0][col1] += DENSITY_PER_PARTICLE * weight10;
        if (row1 >= 0 && row1 < 18 && col0 >= 0 && col0 < 6 && LIQUID_LED_MASK[row1][col0])
            density[row1][col0] += DENSITY_PER_PARTICLE * weight01;
        if (row1 >= 0 && row1 < 18 && col1 >= 0 && col1 < 6 && LIQUID_LED_MASK[row1][col1])
            density[row1][col1] += DENSITY_PER_PARTICLE * weight11;
    }
}

SimBase::Status LiquidSim::calc() {
    _update_gravity();

    float dt_sub = LIQUID_DT / (float)SUBSTEPS;
    for (uint8_t s = 0; s < SUBSTEPS; s++) {
        _update_particles(dt_sub);
        _process_collisions();
    }

    // Density field -> LED buffer
    float density[18][6];
    _compute_density(density);

    memset(_led_buf, 0, sizeof(_led_buf));
    for (uint8_t row = 0; row < 18; row++) {
        for (uint8_t col = 0; col < 6; col++) {
            if (!LIQUID_LED_MASK[row][col]) continue;
            
            int8_t idx = _led_lut[row][col];
            if (idx < 0) continue;

            float val = density[row][col];
            if (val > DENSITY_MAX_BRIGHTNESS) val = DENSITY_MAX_BRIGHTNESS;
            _led_buf[idx] = (uint8_t)val;
        }
    }

    return Status::OK;
}

SimBase::Status LiquidSim::draw() {
    bool ok = true;
    for (uint8_t i = 0; i < 96; i++) {
        uint8_t sw = (i % 12) + 1;
        uint8_t cs = (i / 12) + 1;

        if (_led_buf[i] > 0) {
            ok &= _is31.setPWM(cs, sw, _led_buf[i]);
            ok &= _is31.ledOn(cs, sw);
        } else {
            ok &= _is31.ledOff(cs, sw);
        }
    }
    return (ok ? Status::OK : Status::ERR);
}

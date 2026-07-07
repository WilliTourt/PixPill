#include "liquidsim.h"
#include <cmath>
#include <cstring>

using namespace std;

LiquidSim::LiquidSim(BMA530 &accel, IS31FL3736 &is31)
    : _accel(accel), _is31(is31), _random(131) {
    memset(_led_buf, 0, sizeof(_led_buf));
    _gravity.x = 0;
    _gravity.y = 0;
}

LiquidSim::Status LiquidSim::init() {
    for (uint8_t i = 0; i < LIQUID_PARTICLE_COUNT; i++) {
        _particles[i].vx = 0;
        _particles[i].vy = 0;

        bool valid = false;
        while (!valid) {
            uint8_t row = _random % 18;
            uint8_t col = _random % 6;

            _random = (_random * 131 + 53) & 0xFFFF;

            if (!LIQUID_LED_MASK[row][col]) continue;

            _particles[i].x = (float)col + 0.5f;
            _particles[i].y = (float)row + 0.5f;
            valid = true;
        }
    }
    return Status::OK;
}

/*
 * 读加速度计，转换为重力向量
 * BMA530方向: X+→row0(顶), Y+→col0(左), Z+→垂直板面朝上
 * display在加速度计背面
 */
void LiquidSim::_update_gravity() {
    _accel.update();
    int16_t ax_raw = _accel.readAx();
    int16_t ay_raw = _accel.readAy();

    float ax = (float)(ax_raw) / 16384.0f;
    float ay = (float)(ay_raw) / 16384.0f;

    if (fabs(ax) < 0.03f) ax = 0;
    if (fabs(ay) < 0.03f) ay = 0;

    _gravity.x = -ay * LIQUID_GRAVITY_SCALE;
    _gravity.y = ax * LIQUID_GRAVITY_SCALE;
}

/*
 * 连续斜边边界
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
 * 软墙边界
 */
void LiquidSim::_clamp_particle_to_shape(LiquidParticle_t &p) {
    float left, right;
    _get_boundary(p.y, left, right);

    float margin = WALL_PUSH_MARGIN;
    float strength = WALL_PUSH_STRENGTH;

    if (p.y > 17.0f - margin) {
        float depth = p.y - (17.0f - margin);
        p.vy -= depth * strength;
        if (p.vy > 0) p.vy *= 0.2f;
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
 * 更新粒子位置和速度
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
 * 粒子间碰撞 + 表面张力
 * 碰撞：弹性碰撞 + 线性推开（对齐参考代码）
 */
void LiquidSim::_process_collisions() {
    float min_dist_sq = LIQUID_MIN_DIST * LIQUID_MIN_DIST;
    float attract_sq  = LIQUID_ATTRACT_RADIUS * LIQUID_ATTRACT_RADIUS;

    // 第一段：碰撞推开 + 弹性速度交换
    for (uint8_t iter = 0; iter < LIQUID_COLLISIONS_ITERS; iter++) {
        for (uint8_t i = 0; i < LIQUID_PARTICLE_COUNT; i++) {
            for (uint8_t j = i + 1; j < LIQUID_PARTICLE_COUNT; j++) {

                float dx = _particles[j].x - _particles[i].x;
                float dy = _particles[j].y - _particles[i].y;
                float dist_sq = dx * dx + dy * dy;

                if (dist_sq < 0.001f) {
                    _particles[i].x -= 0.03f;
                    _particles[i].y -= 0.03f;
                    _particles[j].x += 0.03f;
                    _particles[j].y += 0.03f;
                    continue;
                }

                if (dist_sq < min_dist_sq) {
                    float dist = sqrtf(dist_sq);
                    float ex = dx / dist;
                    float ey = dy / dist;

                    // 线性推开（参考: overlap = 0.5*(MIN_DIST - dist)）
                    float overlap = 0.5f * (LIQUID_MIN_DIST - dist);
                    _particles[i].x -= overlap * ex;
                    _particles[i].y -= overlap * ey;
                    _particles[j].x += overlap * ex;
                    _particles[j].y += overlap * ey;

                    // 弹性碰撞动量交换（参考: avg = (vA+vB)/2）
                    float vA = _particles[i].vx * ex + _particles[i].vy * ey;
                    float vB = _particles[j].vx * ex + _particles[j].vy * ey;
                    float avg = (vA + vB) / 2.0f;

                    _particles[i].vx += (avg - vA) * ex;
                    _particles[i].vy += (avg - vA) * ey;
                    _particles[j].vx += (avg - vB) * ex;
                    _particles[j].vy += (avg - vB) * ey;
                }
            }
        }
    }

    // 第二段：表面张力
    for (uint8_t i = 0; i < LIQUID_PARTICLE_COUNT; i++) {
        for (uint8_t j = i + 1; j < LIQUID_PARTICLE_COUNT; j++) {
            float dx = _particles[j].x - _particles[i].x;
            float dy = _particles[j].y - _particles[i].y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq >= min_dist_sq && dist_sq < attract_sq) {
                float dist = sqrtf(dist_sq);
                float ex = dx / dist;
                float ey = dy / dist;
                float force = LIQUID_ATTRACT_STRENGTH * (LIQUID_ATTRACT_RADIUS - dist);
                _particles[i].vx += force * ex;
                _particles[i].vy += force * ey;
                _particles[j].vx -= force * ex;
                _particles[j].vy -= force * ey;
            }
        }
    }

    for (uint8_t i = 0; i < LIQUID_PARTICLE_COUNT; i++) {
        _clamp_particle_to_shape(_particles[i]);
    }
}

/*
 * 双线性插值密度场
 * 每个粒子贡献到最近4个LED格子，按距离比例分配
 * 纯加减乘除，无exp()
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

        float wx1 = fx - (float)col0;
        float wx0 = 1.0f - wx1;
        float wy1 = fy - (float)row0;
        float wy0 = 1.0f - wy1;

        // 四个角 (row, col, weight)
        float w00 = wx0 * wy0;
        float w10 = wx1 * wy0;
        float w01 = wx0 * wy1;
        float w11 = wx1 * wy1;

        if (row0 >= 0 && row0 < 18 && col0 >= 0 && col0 < 6 && LIQUID_LED_MASK[row0][col0])
            density[row0][col0] += DENSITY_PER_PARTICLE * w00;
        if (row0 >= 0 && row0 < 18 && col1 >= 0 && col1 < 6 && LIQUID_LED_MASK[row0][col1])
            density[row0][col1] += DENSITY_PER_PARTICLE * w10;
        if (row1 >= 0 && row1 < 18 && col0 >= 0 && col0 < 6 && LIQUID_LED_MASK[row1][col0])
            density[row1][col0] += DENSITY_PER_PARTICLE * w01;
        if (row1 >= 0 && row1 < 18 && col1 >= 0 && col1 < 6 && LIQUID_LED_MASK[row1][col1])
            density[row1][col1] += DENSITY_PER_PARTICLE * w11;
    }
}

/*
 * 主计算
 */
LiquidSim::Status LiquidSim::calc() {
    _update_gravity();

    float dt_sub = LIQUID_DT / (float)SUBSTEPS;
    for (uint8_t s = 0; s < SUBSTEPS; s++) {
        _update_particles(dt_sub);
        _process_collisions();
    }

    // 密度场 → LED缓冲
    float density[18][6];
    _compute_density(density);

    // 密度 → LUT中的LED索引
    static bool lut_ready = false;
    static int8_t lut[18][6];
    if (!lut_ready) {
        memset(lut, -1, sizeof(lut));
        for (uint8_t led = 0; led < LIQUID_LED_COUNT; led++) {
            lut[LIQUID_LED_ROW[led]][LIQUID_LED_COL[led]] = led;
        }
        lut_ready = true;
    }

    memset(_led_buf, 0, sizeof(_led_buf));
    for (uint8_t row = 0; row < 18; row++) {
        for (uint8_t col = 0; col < 6; col++) {
            if (!LIQUID_LED_MASK[row][col]) continue;
            int8_t idx = lut[row][col];
            if (idx < 0) continue;

            float val = density[row][col];
            if (val > DENSITY_MAX_BRIGHTNESS) val = DENSITY_MAX_BRIGHTNESS;
            _led_buf[idx] = (uint8_t)val;
        }
    }

    return Status::OK;
}

/*
 * 渲染
 */
LiquidSim::Status LiquidSim::draw() {
    for (uint8_t i = 0; i < 96; i++) {
        uint8_t sw = (i % 12) + 1;
        uint8_t cs = (i / 12) + 1;

        if (_led_buf[i] > 0) {
            _is31.setPWM(cs, sw, _led_buf[i]);
            _is31.ledOn(cs, sw);
        } else {
            _is31.ledOff(cs, sw);
        }
    }
    return Status::OK;
}

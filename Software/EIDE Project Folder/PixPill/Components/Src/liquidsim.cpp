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
    // Liquid particles are randomly initialized in the lower half of the display
    for (uint8_t i = 0; i < LIQUID_PARTICLE_COUNT; i++) {
        _particles[i].vx = 0;
        _particles[i].vy = 0;

        bool valid = false;
        while (!valid) {
            uint8_t row = _random % 18;
            uint8_t col = _random % 6;

            _random = (_random * 131 + 53) & 0xFFFF;

            // 检查这个 (row, col) 是否在胶囊形状内
            if ((row == 0 || row == 17) && (col < 2 || col > 3)) continue;
            if ((row == 1 || row == 16) && (col < 1 || col > 4)) continue;

            _particles[i].x = (float)col + 0.5f;  // 中心对齐
            _particles[i].y = (float)row + 0.5f;
            valid = true;
        }
    }
    return Status::OK;
}

/*
 * 读加速度计，转换为重力向量
 * BMA530方向: X+→row0(顶), Y+→col0(左), Z+→垂直板面朝上
 */
void LiquidSim::_update_gravity() {
    _accel.update();
    int16_t ax_raw = _accel.readAx();
    int16_t ay_raw = _accel.readAy();

    // 归一化到约-1~1，然后用缩放系数
    float ax = (float)(ax_raw) / 16384.0f;  // 2G量程: 1g≈16384
    float ay = (float)(ay_raw)  / 16384.0f;

    // Gravity Deadzone
    if (fabs(ax) < 0.03f) ax = 0;
    if (fabs(ay) < 0.03f) ay = 0;

    // notice that the display are on the back side of accelerometer
    _gravity.x = -ay * LIQUID_GRAVITY_SCALE;
    _gravity.y = ax * LIQUID_GRAVITY_SCALE;
}

/*
 * 更新每个粒子的速度和位置
 * v += g * dt, v *= damping, pos += v * dt
 */
void LiquidSim::_update_particles() {
    int8_t col_most_left[18] = {2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2};
    int8_t col_most_right[18] = {3,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,4,3};

    for (uint8_t i = 0; i < LIQUID_PARTICLE_COUNT; i++) {
        _particles[i].vx += _gravity.x * LIQUID_DT;
        _particles[i].vy += _gravity.y * LIQUID_DT;

        _particles[i].vx *= LIQUID_DAMPING;
        _particles[i].vy *= LIQUID_DAMPING;

        _particles[i].x += _particles[i].vx * LIQUID_DT;
        _particles[i].y += _particles[i].vy * LIQUID_DT;

        // 1. 先夹Y
        if (_particles[i].y < 0) {
            _particles[i].y = 0;
            _particles[i].vy *= -LIQUID_BOUND_BOUNCE;
        }
        if (_particles[i].y > 17) {
            _particles[i].y = 17;
            _particles[i].vy *= -LIQUID_BOUND_BOUNCE;
        }

        // 2. 用夹完后的y查表 → 再夹X
        int8_t r = (int8_t)(_particles[i].y + 0.5f);
        r = (r < 0) ? 0 : (r > 17) ? 17 : r;
        float left  = (float)col_most_left[r];
        float right = (float)col_most_right[r];

        if (_particles[i].x < left) {
            _particles[i].x = left;
            _particles[i].vx *= -LIQUID_BOUND_BOUNCE;
        }
        if (_particles[i].x > right) {
            _particles[i].x = right;
            _particles[i].vx *= -LIQUID_BOUND_BOUNCE;
        }
    }
}

/*
 * 粒子间碰撞 + 表面张力
 * 每对粒子互查：太近→弹开，适中→吸引
 */
void LiquidSim::_process_collisions() {
    for (uint8_t i = 0; i < LIQUID_PARTICLE_COUNT; i++) {
        for (uint8_t j = i + 1; j < LIQUID_PARTICLE_COUNT; j++) {

            // Calc distance between 
            float dx = _particles[i].x - _particles[j].x;
            float dy = _particles[i].y - _particles[j].y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq < 0.0001f) continue; // ignore if too close (avoid div by zero)

            float dist = sqrtf(dist_sq);

            if (dist < LIQUID_MIN_DIST) { // if too close, push them apart
                // Unit vector (from particle i to particle j)
                float ex = dx / dist;
                float ey = dy / dist;

                // change their location to bigger that LIQUID_MIN_DIST, push them apart
                float overlap = LIQUID_MIN_DIST - dist;
                _particles[i].x -= ex * overlap * 0.5f;
                _particles[i].y -= ey * overlap * 0.5f;
                _particles[j].x += ex * overlap * 0.5f;
                _particles[j].y += ey * overlap * 0.5f;

                // make their velocity the same module but opposite direction
                // ... remains doubt
                float vAi = _particles[i].vx * ex + _particles[i].vy * ey;
                float vBj = _particles[j].vx * ex + _particles[j].vy * ey;
                float avg = (vAi + vBj) * 0.5f;
                _particles[i].vx += (avg - vAi) * ex;
                _particles[i].vy += (avg - vAi) * ey;
                _particles[j].vx += (avg - vBj) * ex;
                _particles[j].vy += (avg - vBj) * ey;

            } else if (dist < LIQUID_ATTRACT_RADIUS) { // if in attraction range, attract them

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
}

/*
 * 主计算：每帧调用一次
 */
LiquidSim::Status LiquidSim::calc() {
    _update_gravity();
    _update_particles();
    _process_collisions();

    // 清空LED缓冲
    memset(_led_buf, 0, sizeof(_led_buf));

    // 把每个粒子映射到最近的LED，叠加上去
    for (uint8_t p = 0; p < LIQUID_PARTICLE_COUNT; p++) {
        int8_t row = (int8_t)(_particles[p].y + 0.5f);
        int8_t col = (int8_t)(_particles[p].x + 0.5f);

        // 查表找这个(row,col)对应的LED index
        // 线性搜索96个LED（可以优化但够用了）
        for (uint8_t led = 0; led < LIQUID_LED_COUNT; led++) {
            if (LIQUID_LED_ROW[led] == row && LIQUID_LED_COL[led] == col) {
                // 叠加上去，超过255饱和
                uint16_t val = _led_buf[led] + 85;  // 每个粒子贡献85
                _led_buf[led] = (val > 255) ? 255 : (uint8_t)val;
                break;
            }
        }
    }

    return Status::OK;
}

/*
 * 渲染：把_led_buf写到IS31FL3736
 * 跟sandsim的draw()一样：led index → (cs, sw)
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

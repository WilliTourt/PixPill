#pragma once

#include "main.h"
#include "is31fl3736.h"
#include "bma530.h"

// === 粒子数量 ===
#define LIQUID_PARTICLE_COUNT    24

// === 流体物理 ===
#define LIQUID_DAMPING           0.99f      // 全局速度阻尼
#define LIQUID_GRAVITY_SCALE     1.5f       // 重力强度
#define LIQUID_DT                1.0f       // 每帧时长

// === 粒子间碰撞（弹性碰撞+线性推开） ===
#define LIQUID_MIN_DIST          1.2f       // 粒子最小间距
#define LIQUID_COLLISIONS_ITERS  1          // 每子步位置推开次数

// === 表面张力（已关闭省算力） ===
#define LIQUID_ATTRACT_RADIUS    1.6f       // 未使用
#define LIQUID_ATTRACT_STRENGTH  0.0f       // 关掉省算力

// === 数值积分 ===
#define SUBSTEPS                 2          // 每帧子步数

// === 墙壁 ===
#define WALL_PUSH_MARGIN         0.4f       // 离墙多近开始排斥
#define WALL_PUSH_STRENGTH       1.5f       // 墙排斥力强度

// === 密度场 ===
#define DENSITY_MAX_BRIGHTNESS   255.0f     // 最大PWM值
#define DENSITY_PER_PARTICLE     240.0f     // 每个粒子的总贡献度

// === 网格 ===
#define LIQUID_GRID_ROWS         18
#define LIQUID_GRID_COLS         6
#define LIQUID_LED_COUNT         96

static const int8_t LIQUID_LED_ROW[LIQUID_LED_COUNT] = {
           0, 0,
        1, 1, 1, 1,
     2, 2, 2, 2, 2, 2,
     3, 3, 3, 3, 3, 3,
     4, 4, 4, 4, 4, 4,
     5, 5, 5, 5, 5, 5,
     6, 6, 6, 6, 6, 6,
     7, 7, 7, 7, 7, 7,
     8, 8, 8, 8, 8, 8,
     9, 9, 9, 9, 9, 9,
    10,10,10,10,10,10,
    11,11,11,11,11,11,
    12,12,12,12,12,12,
    13,13,13,13,13,13,
    14,14,14,14,14,14,
    15,15,15,15,15,15,
       16,16,16,16,
          17,17
};

static const int8_t LIQUID_LED_COL[LIQUID_LED_COUNT] = {
        2,3,
      1,2,3,4,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
    0,1,2,3,4,5,
      1,2,3,4,
        2,3
};

// LED网格掩码 - 用于密度场检查格子是否有效
static const bool LIQUID_LED_MASK[18][6] = {
    {0,0,1,1,0,0},
    {0,1,1,1,1,0},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {1,1,1,1,1,1},
    {0,1,1,1,1,0},
    {0,0,1,1,0,0}
};


class LiquidSim {
    public:
        enum class Status {
            OK,
            ERR_ACCEL,
            ERR
        };

        typedef struct {
            float x, y;
            float vx, vy;
        } LiquidParticle_t;

        typedef struct {
            float x, y;
        } Vector_t;

        LiquidSim(BMA530 &accel, IS31FL3736 &is31);
        Status init();
        Status calc();
        Status draw();

    private:
        void _update_gravity();
        void _update_particles(float dt);
        void _clamp_particle_to_shape(LiquidParticle_t &particle);
        void _get_boundary(float y, float &left, float &right);
        void _process_collisions();
        void _compute_density(float density[18][6]);

        BMA530 &_accel;
        IS31FL3736 &_is31;

        LiquidParticle_t _particles[LIQUID_PARTICLE_COUNT];
        Vector_t _gravity;

        uint8_t _led_buf[LIQUID_LED_COUNT];  // 0=灭 255=最亮
        uint16_t _random;
};

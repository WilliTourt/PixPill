#pragma once

#include "main.h"
#include "is31fl3736.h"
#include "bma530.h"

#define LIQUID_PARTICLE_COUNT    15      // 粒子数量
#define LIQUID_DAMPING           0.92f   // 速度阻尼（越小越黏）
#define LIQUID_MIN_DIST          0.9f    // 粒子最小间距，小于则碰撞推开
#define LIQUID_ATTRACT_RADIUS    1.5f    // 表面张力吸引半径
#define LIQUID_ATTRACT_STRENGTH  0.008f  // 吸引力强度
#define LIQUID_GRAVITY_SCALE     0.06f   // 倾斜→重力缩放
#define LIQUID_DT                0.5f    // 每帧时间步长
#define LIQUID_BOUND_BOUNCE      0.4f    // 撞墙反弹系数

// LED 布局：胶囊形 18行×6列，但两端行不满
#define LIQUID_GRID_ROWS         18
#define LIQUID_GRID_COLS         6
#define LIQUID_LED_COUNT         96

// 每个LED在18×6网格中的行/列
// 帮你预计算好了：用table里的row,col，-1表示该(row,col)没有LED
static const int8_t LIQUID_LED_ROW[LIQUID_LED_COUNT] = {
    // 对应 LED_NEIGHBORS 的顺序：index 0~95
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
        void _update_particles();
        void _process_collisions();

        BMA530 &_accel;
        IS31FL3736 &_is31;

        LiquidParticle_t _particles[LIQUID_PARTICLE_COUNT];
        Vector_t _gravity;
        
        uint8_t _led_buf[LIQUID_LED_COUNT];  // 0=灭 255=最亮
        uint16_t _random;
};

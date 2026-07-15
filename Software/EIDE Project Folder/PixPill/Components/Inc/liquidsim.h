#pragma once

#include "main.h"
#include "is31fl3736.h"
#include "bma530.h"
#include "sim_base.h"

// Numerical Integration
#define SUBSTEPS                            2      // more substeps = more stable but slower

// Fluid Physics
#define LIQUID_DAMPING                      0.88f  // global damping factor for velocity
#define LIQUID_GRAVITY_SCALE                1.4f  // gravity strength
#define LIQUID_DT                           1.0f   // frame duration, too fast will cause instability

// Collisions
#define LIQUID_MIN_DIST                     1.24f  // Expected minimum distance between particles (for collision detection)
#define LIQUID_COLLISION_DAMPING            0.5f   // collision momentum exchange damping
#define LIQUID_COLLISIONS_ITERS             1      // number of position pushes per substep

// Surface tension (NOW CLOSED FOR PERFORMANCE)
#define LIQUID_ATTRACT_STRENGTH             0.0f   // Attract strength
#define LIQUID_ATTRACT_RADIUS               1.80f   // Distance at which attraction starts

// Walls
#define WALL_PUSH_MARGIN                    0.3f   // Distance from wall before pushing
#define WALL_PUSH_STRENGTH                  1.4f   // Push strength

// Density Field
#define DENSITY_MAX_BRIGHTNESS              255.0f // Maximum PWM value (LED Brightness)
#define DENSITY_PER_PARTICLE                270.0f // Contribution of each particle to the density field

// Density Force
#define LIQUID_DENSITY_PRESSURE_THRESHOLD   95.0f  // Density threshold for triggering pressure force
#define LIQUID_PRESSURE_STRENGTH            3.6f   // Density pressure force

// Grid
#define LIQUID_GRID_ROWS 18
#define LIQUID_GRID_COLS 6



// LED grid mask — valid cells for density field and particle placement
#ifdef PIXPILL_SIZE_1_CAPSULE

// Row 0-1 empty (button area), rows 2-17 same pill shape as 000#
static const bool LIQUID_LED_MASK[18][6] = {
    { 0,0,0,0,0,0 },
    { 0,0,0,0,0,0 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 0,1,1,1,1,0 },
    { 0,0,1,1,0,0 }
};

#else

static const bool LIQUID_LED_MASK[18][6] = {
    { 0,0,1,1,0,0 },
    { 0,1,1,1,1,0 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 1,1,1,1,1,1 },
    { 0,1,1,1,1,0 },
    { 0,0,1,1,0,0 }
};
#endif



// 000# Capsule (6.6mm, 96 LEDs): full pill, 2-6-...-6-4-2 shape
// 1# Capsule (8mm, 90 LEDs): top rows 0-1 removed for physical button
#define LIQUID_LED_COUNT 96

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



class LiquidSim : public SimBase {
    public:
        typedef struct {
            float x, y;
            float vx, vy;
        } LiquidParticle_t;

        typedef struct {
            float x, y;
        } Vector_t;

        LiquidSim(BMA530 &accel, IS31FL3736 &is31, uint8_t particle_num = 16);
        ~LiquidSim();

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
        uint8_t _particle_num;

        LiquidParticle_t *_particles;
        Vector_t _gravity;
        int8_t _led_lut[18][6];  // density grid coord → LED index, -1=invalid

        uint8_t _led_buf[LIQUID_LED_COUNT];  // 0=off 255=full brightness
        uint16_t _random;
};

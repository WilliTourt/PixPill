#pragma once

#include "main.h"
#include "is31fl3736.h"
#include "bma530.h"

#include "pixpill_neighbors_table.h"

class SandSim {
    public:

        enum class Status {
            OK,
            ERR_ACCEL,
            ERR_LED,
            ERR
        };

        SandSim(BMA530 &accel, IS31FL3736 &is31);

        Status init();

        Status start();
        Status calc();
        Status draw();

    private:
        void _backup_sand_array();
        int16_t _abs(int16_t x);

        uint8_t _sand_now[96];
        uint8_t _sand_prev[96];

        BMA530 _accel;
        IS31FL3736 _is31;

        uint16_t _random;
};

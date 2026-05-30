#include "sandsim.h"
#include <cstring>

SandSim::SandSim(BMA530 &accel, IS31FL3736 &is31) :
    _accel(accel), _is31(is31), _random(131) {
    memset(_sand_now, 0, sizeof(_sand_now));
    memset(_sand_prev, 0, sizeof(_sand_prev));
}

SandSim::Status SandSim::init() {
    _sand_now[2] = 1;
    _sand_now[6] = 1;
    _sand_now[7] = 1;
    _sand_now[12] = 1;
    _sand_now[13] = 1;
    _sand_now[18] = 1;
    _sand_now[19] = 1;
    _sand_now[24] = 1;
    _sand_now[25] = 1;
    _sand_now[31] = 1;
    _sand_now[4] = 1;
    _sand_now[5] = 1;
    _sand_now[8] = 1;
    _sand_now[9] = 1;
    _sand_now[10] = 1;
    _sand_now[16] = 1;
    _sand_now[17] = 1;
    _sand_now[23] = 1;
    return Status::OK;
}

SandSim::Status SandSim::start() {
    return Status::OK;
}

SandSim::Status SandSim::calc() {
    for (uint8_t i = 0; i < 96; i++) {
        _sand_prev[i] = _sand_now[i];
        _sand_now[i] = 0;
    }

    for (uint8_t i = 0; i < 96; i++) {
        uint8_t current_led_loc = SCAN_ORDER_DOWN[i];
        if (_sand_prev[current_led_loc] == 0) continue;

        // if we have a sand grain here
        int8_t downward_loc = LED_NEIGHBORS[current_led_loc][NEIGHBOR_DOWN];
        if (downward_loc < 0) {                 // we detect if its downward neighbor is -1 (wall)
            _sand_now[current_led_loc] = 1;     // if it's a wall, we don't let it fall
            continue;
        }

        // if no wall, we check if the downward neighbor is already filled
        if (_sand_now[downward_loc] == 0) {                     // if downward neighbor is empty
            _sand_now[downward_loc] = 1;                        // we let it fall
        } else { // if filled, we detect current_led_loc's downleft or downright neighbor
            int8_t downleft = LED_NEIGHBORS[downward_loc][NEIGHBOR_LEFT];
            int8_t downright = LED_NEIGHBORS[downward_loc][NEIGHBOR_RIGHT];

            _random = (_random * 131 + 53) & 0xFF;
            if (_random & 1) { int8_t t = downleft; downleft = downright; downright = t; }

            if (downleft < 0) { // if downleft neighbor is -1 (wall)
                _sand_now[current_led_loc] = 1;
                continue;
            }
            
            if (_sand_now[downleft] == 0) {
                _sand_now[downleft] = 1;
            } else if (_sand_now[downright] == 0) {
                _sand_now[downright] = 1;
            } else {
                _sand_now[current_led_loc] = 1; // if both downleft and downright neighbor are filled, the sand stays
            }
            
        }
    }

    return Status::OK;
}

SandSim::Status SandSim::draw() {
    for (uint8_t i = 0; i < 96; i++) {
        uint8_t sw = (i % 12) + 1;
        uint8_t cs = (i / 12) + 1;
        _is31.ledSet(cs, sw, _sand_now[i]);
    }

    return Status::OK;
}

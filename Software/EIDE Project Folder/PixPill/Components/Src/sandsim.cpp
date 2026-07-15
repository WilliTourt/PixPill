#include "sandsim.h"
#include <cstring>

SandSim::SandSim(BMA530 &accel, IS31FL3736 &is31, uint8_t sand_num) :
    _accel(accel), _is31(is31), _sand_num(sand_num), _random(131) {
    memset(_sand_now, 0, sizeof(_sand_now));
    memset(_sand_prev, 0, sizeof(_sand_prev));
}

SimBase::Status SandSim::init() {
    for (uint8_t i = 0; i < _sand_num; i++) {
        uint8_t idx;
        do {
            _random = (_random * 2944925833 + 12345) & 0xDEADBEEF;
#ifdef PIXPILL_SIZE_1_CAPSULE
            idx = 6 + (_random % 90);  // LEDs 0-5 don‘t exist
#else
            idx = _random % 96;
#endif
        } while (_sand_now[idx] != 0);
        _sand_now[idx] = 1;
    }
    return Status::OK;
}

SimBase::Status SandSim::calc() {
    _backup_sand_array();

    // BMA530 Orientation: X+ -> row0(top)   Y+ -> col0(left)   Z+ -> Ax(Vector)*Ay(Vector) (up)
    int16_t ax, ay;
    Status status = _accel.update() ? Status::OK : Status::ERR_ACCEL;
    ax = -_accel.readAx();
    ay = _accel.readAy();

    // Determine 8-sector gravity direction first
    const uint8_t* scan_order;
    uint8_t g_dirs[3];  // { main direction, side_a, side_b }
    /*
    g_dirs explanation ('O' shape = sand particle):

        Orthogonal:                     Diagonal:
        +-----+-----+-----+             +-----+-----+-----+
        |     |  O  |     |             |  a  |  O  |     |
        +-----+--↓--+-----+             +-----↙-----+-----+ 
        |  a  | main|  b  |             | main|  b  |     |
        +-----+-----+-----+             +-----+-----+-----+ 
        
        Main direction try first, if failed then randomly choose side a or side b
    */
    int16_t gravity_row = -ax;   // gravity row component (positive = toward row+ = DOWN)
    int16_t gravity_col = -ay;   // gravity col component (positive = toward col+ = RIGHT)
    int16_t abs_g_row = (gravity_row < 0) ? -gravity_row : gravity_row;
    int16_t abs_g_col = (gravity_col < 0) ? -gravity_col : gravity_col;
    
    // Diagonal detection: if |abs_g_row/abs_g_col| > tan(22.5deg), then it's diagonal.
    // tan(22.5deg) ≈ 0.414 ≈ 106/256
    bool is_diag = (abs_g_row * 256 >= abs_g_col * 106) && (abs_g_col * 256 >= abs_g_row * 106);
    bool reverse;       // Reverse the scan direction, for orthogonal sectors? true is reverse
    
    if (is_diag) {        // Diagonal sectors
        if (gravity_col >= 0 && gravity_row >= 0) {         // Quadrant I: DOWN-RIGHT
            scan_order = SCAN_ORDER_GRAVITY_DOWN_RIGHT;
            g_dirs[0] = NEIGHBOR_DOWN_RIGHT;
            g_dirs[1] = NEIGHBOR_DOWN;
            g_dirs[2] = NEIGHBOR_RIGHT;
        } else if (gravity_col < 0 && gravity_row >= 0) {   // Quadrant II: DOWN-LEFT
            scan_order = SCAN_ORDER_GRAVITY_DOWN_LEFT;
            g_dirs[0] = NEIGHBOR_DOWN_LEFT;
            g_dirs[1] = NEIGHBOR_DOWN;
            g_dirs[2] = NEIGHBOR_LEFT;
        } else if (gravity_col < 0 && gravity_row < 0) {    // Quadrant III: UP-LEFT
            scan_order = SCAN_ORDER_GRAVITY_UP_LEFT;
            g_dirs[0] = NEIGHBOR_UP_LEFT;
            g_dirs[1] = NEIGHBOR_UP;
            g_dirs[2] = NEIGHBOR_LEFT;
        } else {                                            // Quadrant IV: UP-RIGHT
            scan_order = SCAN_ORDER_GRAVITY_UP_RIGHT;
            g_dirs[0] = NEIGHBOR_UP_RIGHT;
            g_dirs[1] = NEIGHBOR_UP;
            g_dirs[2] = NEIGHBOR_RIGHT;
        }
        reverse = false;
    } else {        // Orthogonal sectors
        if (_abs(ay) > _abs(ax)) { // Y is the dominant direction, choose left-right scan
            scan_order = SCAN_ORDER_GRAVITY_RIGHT;
            g_dirs[0] = (ay > 0) ? NEIGHBOR_LEFT : NEIGHBOR_RIGHT;
            g_dirs[1] = (ax > 0) ? NEIGHBOR_UP : NEIGHBOR_DOWN;
            reverse = (ay < 0); // reverse the scan order, scan left to right while still using SCAN_ORDER_GRAVITY_RIGHT
        } else { // X dominant
            scan_order = SCAN_ORDER_GRAVITY_DOWN;
            g_dirs[0] = (ax > 0) ? NEIGHBOR_UP : NEIGHBOR_DOWN;
            g_dirs[1] = (ay > 0) ? NEIGHBOR_LEFT : NEIGHBOR_RIGHT;
            reverse = (ax > 0);
        }
    }


    // Start scanning
    for (uint8_t i = 0; i < 96; i++) {
        uint8_t current_led_loc = reverse ? scan_order[95 - i] : scan_order[i];
        uint8_t velocity = _sand_prev[current_led_loc];
        if (velocity == 0) continue;

        if (is_diag) { // Diagonal falling logic
            int8_t main_neighbor = LED_NEIGHBORS[current_led_loc][g_dirs[0]];

            if (_sand_now[main_neighbor] == 0) { // main neighbor is empty, move
                _sand_now[main_neighbor] = 1;
            } else { // Wall(-1) in diagonal direction, try orthogonal side slides instead
                // Blocked -> slide to orthogonal sides
                int8_t slide_a = LED_NEIGHBORS[current_led_loc][g_dirs[1]];
                int8_t slide_b = LED_NEIGHBORS[current_led_loc][g_dirs[2]];

                // Randomize which side to try first
                _random = (_random * 2944925833 + 12345) & 0xDEADBEEF;
                if (_random & 1) { int8_t t = slide_a; slide_a = slide_b; slide_b = t; }

                bool moved = false;
                if (slide_a >= 0 && _sand_now[slide_a] == 0) {
                    _sand_now[slide_a] = 1;  moved = true;
                }
                if (!moved && slide_b >= 0 && _sand_now[slide_b] == 0) {
                    _sand_now[slide_b] = 1;  moved = true;
                }
                if (!moved) {
                    _sand_now[current_led_loc] = 1;
                }
            }

        } else { // Orthogonal fall
            // Check neighbor in main gravity component
            int8_t main_neighbor = LED_NEIGHBORS[current_led_loc][g_dirs[0]];
            if (main_neighbor < 0) {
                _sand_now[current_led_loc] = 1;
                continue;
            }

            // try to fall multiple steps (accelerate with velocity)
            int8_t fall_pos = current_led_loc;
            uint8_t fall_steps = 0;
            for (uint8_t s = 0; s < velocity && s < 5; s++) {
                int8_t next = LED_NEIGHBORS[fall_pos][g_dirs[0]];
                if (next < 0) break;
                if (_sand_now[next] > 0) break;
                fall_pos = next;
                fall_steps++;
            }

            if (fall_steps > 0) {
                _sand_now[fall_pos] = (velocity >= 5) ? 5 : velocity + 1;
                continue;
            }

            // if main gravity neighbor is empty -> fall
            if (_sand_now[main_neighbor] == 0) {
                _sand_now[main_neighbor] = 1;
            } else {
                // if blocked, try sliding along two perpendicular directions
                int8_t slide_a, slide_b;
                if (g_dirs[0] == NEIGHBOR_UP || g_dirs[0] == NEIGHBOR_DOWN) {
                    slide_a = LED_NEIGHBORS[main_neighbor][NEIGHBOR_LEFT];
                    slide_b = LED_NEIGHBORS[main_neighbor][NEIGHBOR_RIGHT];
                } else {
                    slide_a = LED_NEIGHBORS[main_neighbor][NEIGHBOR_UP];
                    slide_b = LED_NEIGHBORS[main_neighbor][NEIGHBOR_DOWN];
                }

                // Randomize which side to try first
                _random = (_random * 2944925833 + 12345) & 0xDEADBEEF;
                if (_random & 1) { int8_t t = slide_a; slide_a = slide_b; slide_b = t; }

                bool moved = false;
                if (slide_a >= 0 && _sand_now[slide_a] == 0) {
                    _sand_now[slide_a] = 1;
                    moved = true;
                }
                if (!moved && slide_b >= 0 && _sand_now[slide_b] == 0) {
                    _sand_now[slide_b] = 1;
                    moved = true;
                }
                if (!moved) {
                    _sand_now[current_led_loc] = 1;
                }
            }
        }
    }

    return status;
}

/* // fixed gravity falling
SimBase::Status SandSim::calc() {
    _backup_sand_array();

    // Start scanning
    for (uint8_t i = 0; i < 96; i++) {
        uint8_t current_led_loc = scan_order[i];
        if (_sand_prev[current_led_loc] == 0) continue;

        // if we have a sand grain here
        // we check its downward neighbor first
        int8_t downward_loc = LED_NEIGHBORS[current_led_loc][NEIGHBOR_DOWN];
        if (downward_loc < 0) {                 // detect if its downward neighbor is -1 (wall)
            _sand_now[current_led_loc] = 1;     // if it's a wall, we don't let it fall
            continue;
        }

        // if no wall, we check if the downward neighbor is already filled
        if (_sand_now[downward_loc] == 0) {                     // if downward neighbor is empty
            _sand_now[downward_loc] = 1;                        // we let it fall
        } else { // if filled, we detect current_led_loc's downleft or downright neighbor
            int8_t downleft = LED_NEIGHBORS[downward_loc][NEIGHBOR_LEFT];
            int8_t downright = LED_NEIGHBORS[downward_loc][NEIGHBOR_RIGHT];

            _random = (_random * 2944925833 + 12345) & 0xDEADBEEF;
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
*/

SimBase::Status SandSim::draw() {
    _is31.setPWMAll(0xff);
    for (uint8_t i = 0; i < 96; i++) {
        uint8_t sw = (i % 12) + 1;
        uint8_t cs = (i / 12) + 1;
        _is31.ledSet(cs, sw, _sand_now[i]);
    }

    return Status::OK;
}

void SandSim::_backup_sand_array() {
    for (uint8_t i = 0; i < 96; i++) {
        _sand_prev[i] = _sand_now[i];
        _sand_now[i] = 0;
    }
}

int16_t SandSim::_abs(int16_t x) {
    return (x < 0) ? -x : x;
}

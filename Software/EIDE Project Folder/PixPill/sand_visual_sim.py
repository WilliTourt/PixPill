"""
PixPill Sand Simulation (Python visualization, C++ logic clone)
No axis flipping — gravity direction matches screen directly.
"""
import math
import random
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Rectangle
import numpy as np

# ==================== Grid ====================
ROWS, COLS = 18, 6

LED_MASK = [[False]*COLS for _ in range(ROWS)]
for row in range(ROWS):
    for col in range(COLS):
        if (row in (0,17) and (col < 2 or col > 3)): continue
        if (row in (1,16) and (col < 1 or col > 4)): continue
        LED_MASK[row][col] = True

LED_LIST = []
RC_TO_IDX = {}
for row in range(ROWS):
    for col in range(COLS):
        if LED_MASK[row][col]:
            RC_TO_IDX[(row, col)] = len(LED_LIST)
            LED_LIST.append((row, col))

NUM_LEDS = len(LED_LIST)  # 96

# ==================== Neighbors (8-direction clockwise) ====================
# Order: UP=0, UP-RIGHT=1, RIGHT=2, DOWN-RIGHT=3, DOWN=4, DOWN-LEFT=5, LEFT=6, UP-LEFT=7
NEIGHBOR_UP         = 0
NEIGHBOR_UP_RIGHT   = 1
NEIGHBOR_RIGHT      = 2
NEIGHBOR_DOWN_RIGHT = 3
NEIGHBOR_DOWN       = 4
NEIGHBOR_DOWN_LEFT  = 5
NEIGHBOR_LEFT       = 6
NEIGHBOR_UP_LEFT    = 7

# Direction name for debug
DIR_NAME = ['UP', 'UR', 'R', 'DR', 'D', 'DL', 'L', 'UL']

LED_NEIGHBORS = [[-1]*8 for _ in range(NUM_LEDS)]
for idx, (r, c) in enumerate(LED_LIST):
    LED_NEIGHBORS[idx][NEIGHBOR_UP]         = RC_TO_IDX.get((r-1, c),   -1)
    LED_NEIGHBORS[idx][NEIGHBOR_UP_RIGHT]   = RC_TO_IDX.get((r-1, c+1), -1)
    LED_NEIGHBORS[idx][NEIGHBOR_RIGHT]      = RC_TO_IDX.get((r,   c+1), -1)
    LED_NEIGHBORS[idx][NEIGHBOR_DOWN_RIGHT] = RC_TO_IDX.get((r+1, c+1), -1)
    LED_NEIGHBORS[idx][NEIGHBOR_DOWN]       = RC_TO_IDX.get((r+1, c),   -1)
    LED_NEIGHBORS[idx][NEIGHBOR_DOWN_LEFT]  = RC_TO_IDX.get((r+1, c-1), -1)
    LED_NEIGHBORS[idx][NEIGHBOR_LEFT]       = RC_TO_IDX.get((r,   c-1), -1)
    LED_NEIGHBORS[idx][NEIGHBOR_UP_LEFT]    = RC_TO_IDX.get((r-1, c-1), -1)

# ==================== Scan Orders (C++ exact) ====================
SCAN_DOWN = [
    94,95,90,91,92,93,84,85,86,87,88,89,
    78,79,80,81,82,83,72,73,74,75,76,77,
    66,67,68,69,70,71,60,61,62,63,64,65,
    54,55,56,57,58,59,48,49,50,51,52,53,
    42,43,44,45,46,47,36,37,38,39,40,41,
    30,31,32,33,34,35,24,25,26,27,28,29,
    18,19,20,21,22,23,12,13,14,15,16,17,
     6, 7, 8, 9,10,11, 2, 3, 4, 5, 0, 1
]

SCAN_RIGHT = [
     6,12,18,24,30,36,42,48,54,60,66,72,
    78,84, 2, 7,13,19,25,31,37,43,49,55,
    61,67,73,79,85,90, 0, 3, 8,14,20,26,
    32,38,44,50,56,62,68,74,80,86,91,94,
     1, 4, 9,15,21,27,33,39,45,51,57,63,
    69,75,81,87,92,95, 5,10,16,22,28,34,
    40,46,52,58,64,70,76,82,88,93,11,17,
    23,29,35,41,47,53,59,65,71,77,83,89
]

# Diagonal scan orders (projection DESC, same-diagonal: col ASC = left-to-right)
# DOWN-RIGHT: r+c DESC, then col ASC
SCAN_DOWN_RIGHT = [
    95,93,89,94,92,88,
    83,91,87,82,77,90,
    86,81,76,71,85,80,
    75,70,65,84,79,74,
    69,64,59,78,73,68,
    63,58,53,72,67,62,
    57,52,47,66,61,56,
    51,46,41,60,55,50,
    45,40,35,54,49,44,
    39,34,29,48,43,38,
    33,28,23,42,37,32,
    27,22,17,36,31,26,
    21,16,11,30,25,20,
    15,10, 5,24,19,14,
     9, 4,18,13, 8, 3,
    12, 7, 2, 1, 6, 0,
]

# UP-RIGHT: -r+c DESC, then col ASC
SCAN_UP_RIGHT = [
     1, 5,11, 0, 4,10,
    17, 3, 9,16,23, 2,
     8,15,22,29, 7,14,
    21,28,35, 6,13,20,
    27,34,41,12,19,26,
    33,40,47,18,25,32,
    39,46,53,24,31,38,
    45,52,59,30,37,44,
    51,58,65,36,43,50,
    57,64,71,42,49,56,
    63,70,77,48,55,62,
    69,76,83,54,61,68,
    75,82,89,60,67,74,
    81,88,93,66,73,80,
    87,92,72,79,86,78,
    85,91,95,84,90,94,
]

# DOWN-LEFT: r-c DESC, then col ASC
SCAN_DOWN_LEFT = [
    84,90,94,78,85,91,
    95,72,79,86,92,66,
    73,80,87,93,60,67,
    74,81,88,54,61,68,
    75,82,89,48,55,62,
    69,76,83,42,49,56,
    63,70,77,36,43,50,
    57,64,71,30,37,44,
    51,58,65,24,31,38,
    45,52,59,18,25,32,
    39,46,53,12,19,26,
    33,40,47, 6,13,20,
    27,34,41, 7,14,21,
    28,35, 2, 8,15,22,
    29, 3, 9,16,23, 0,
     4,10,17, 1, 5,11,
]

# UP-LEFT: -(r+c) DESC, then col ASC
SCAN_UP_LEFT = [
     6, 2, 0,12, 7, 3,
     1,18,13, 8, 4,24,
    19,14, 9, 5,30,25,
    20,15,10,36,31,26,
    21,16,11,42,37,32,
    27,22,17,48,43,38,
    33,28,23,54,49,44,
    39,34,29,60,55,50,
    45,40,35,66,61,56,
    51,46,41,72,67,62,
    57,52,47,78,73,68,
    63,58,53,84,79,74,
    69,64,59,85,80,75,
    70,65,90,86,81,76,
    71,91,87,82,77,94,
    92,88,83,95,93,89,
]

# ==================== State ====================
sand_prev = [0] * NUM_LEDS
sand_now  = [0] * NUM_LEDS
_rng = 131

# Init: spawn random sand particles (no duplicates)
INITIAL_SAND = 39
for i in random.sample(range(NUM_LEDS), INITIAL_SAND):
    sand_now[i] = 1

# ==================== Update (C++ clone, no flipping) ====================
def update_sand(ax_tilt, ay_tilt):
    """
    ax_tilt: +1 = capsule top tilted DOWN (screen bottom)
    ay_tilt: +1 = capsule right tilted DOWN (screen left)
    """
    global sand_now, sand_prev, _rng

    sand_prev = list(sand_now)
    sand_now  = [0] * NUM_LEDS

    # C++: ax = -readAx() → here ax_tilt IS the screen tilt already
    #      ay = +readAy() → here ay_tilt IS the screen tilt already
    ax = -ax_tilt
    ay = +ay_tilt

    # --- Direction selection (8-sector, atan2, maps to original logic) ---
    # Indices: UP=0, UR=1, R=2, DR=3, D=4, DL=5, L=6, UL=7
    # Gravity direction (NOT ax/ay from original code):
    #   ax_tilt > 0: top tilted DOWN → gravity pulls DOWN
    #   ay_tilt > 0: right tilted DOWN → gravity pulls RIGHT
    # So gravity vector = (+ay_tilt, +ax_tilt) in (col, row) = (right, down)
    gdr = +ax_tilt  # gravity row: +1 = DOWN
    gdc = -ay_tilt  # gravity col: +1 = RIGHT (ay_tilt>0 → right side down → stuff falls LEFT)
    angle = math.atan2(gdr, gdc)
    angle += math.pi / 8
    if angle < 0:
        angle += 2 * math.pi
    sector = int(angle // (math.pi / 4)) % 8
    # sector 0:R, 1:DR, 2:D, 3:DL, 4:L, 5:UL, 6:U, 7:UR

    # Use EXACT same logic as original, just with 8-dir neighbor indices
    is_diag = (sector % 2 == 1)  # odd sectors = diagonal
    
    if is_diag:
        # --- Diagonal direction setup (each has its own scan table, no reverse needed) ---
        if sector == 1:  # DOWN-RIGHT
            scan_order = SCAN_DOWN_RIGHT; reverse = False
            g_main = NEIGHBOR_DOWN_RIGHT; g_side_a = NEIGHBOR_DOWN; g_side_b = NEIGHBOR_RIGHT
        elif sector == 3:  # DOWN-LEFT
            scan_order = SCAN_DOWN_LEFT; reverse = False
            g_main = NEIGHBOR_DOWN_LEFT; g_side_a = NEIGHBOR_DOWN; g_side_b = NEIGHBOR_LEFT
        elif sector == 5:  # UP-LEFT
            scan_order = SCAN_UP_LEFT; reverse = False
            g_main = NEIGHBOR_UP_LEFT; g_side_a = NEIGHBOR_UP; g_side_b = NEIGHBOR_LEFT
        else:  # sector == 7: UP-RIGHT
            scan_order = SCAN_UP_RIGHT; reverse = False
            g_main = NEIGHBOR_UP_RIGHT; g_side_a = NEIGHBOR_UP; g_side_b = NEIGHBOR_RIGHT
    else:
        # --- Orthogonal direction (original logic) ---
        if abs(ay) > abs(ax):
            scan_order = SCAN_RIGHT
            g_dirs = [NEIGHBOR_LEFT if ay > 0 else NEIGHBOR_RIGHT,
                      NEIGHBOR_UP if ax > 0 else NEIGHBOR_DOWN]
            reverse = (ay < 0)
        else:
            scan_order = SCAN_DOWN
            g_dirs = [NEIGHBOR_UP if ax > 0 else NEIGHBOR_DOWN,
                      NEIGHBOR_LEFT if ay > 0 else NEIGHBOR_RIGHT]
            reverse = (ax > 0)

    # --- Scan ---
    for i in range(NUM_LEDS):
        current = scan_order[95 - i] if reverse else scan_order[i]
        if sand_prev[current] == 0:
            continue

        if is_diag:
            # === Diagonal fall (single-step, no velocity) ===
            main_nbr = LED_NEIGHBORS[current][g_main]
            if main_nbr < 0:
                sand_now[current] = 1
                continue

            if sand_now[main_nbr] == 0:
                sand_now[main_nbr] = 1
            else:
                # Blocked → slide to orthogonal sides (random order)
                slide_a = LED_NEIGHBORS[current][g_side_a]
                slide_b = LED_NEIGHBORS[current][g_side_b]
                _rng = (_rng * 131 + 53) & 0xFF
                if _rng & 1:
                    slide_a, slide_b = slide_b, slide_a
                moved = False
                if slide_a >= 0 and sand_now[slide_a] == 0:
                    sand_now[slide_a] = 1
                    moved = True
                if not moved and slide_b >= 0 and sand_now[slide_b] == 0:
                    sand_now[slide_b] = 1
                    moved = True
                if not moved:
                    sand_now[current] = 1
        else:
            # === Orthogonal fall (original logic with velocity) ===
            velocity = sand_prev[current]
            main_nbr = LED_NEIGHBORS[current][g_dirs[0]]
            if main_nbr < 0:
                sand_now[current] = 1
                continue

            # Multi-step fall
            fall_pos = current
            fall_steps = 0
            for s in range(velocity):
                if s >= 5: break
                nxt = LED_NEIGHBORS[fall_pos][g_dirs[0]]
                if nxt < 0: break
                if sand_now[nxt] > 0: break
                fall_pos = nxt
                fall_steps += 1

            if fall_steps > 0:
                sand_now[fall_pos] = min(velocity + 1, 5)
                continue

            # Single step
            if sand_now[main_nbr] == 0:
                sand_now[main_nbr] = 1
            else:
                # Blocked → side slides
                if g_dirs[0] == NEIGHBOR_UP or g_dirs[0] == NEIGHBOR_DOWN:
                    slide_a = LED_NEIGHBORS[main_nbr][NEIGHBOR_LEFT]
                    slide_b = LED_NEIGHBORS[main_nbr][NEIGHBOR_RIGHT]
                else:
                    slide_a = LED_NEIGHBORS[main_nbr][NEIGHBOR_UP]
                    slide_b = LED_NEIGHBORS[main_nbr][NEIGHBOR_DOWN]

                _rng = (_rng * 131 + 53) & 0xFF
                if _rng & 1:
                    slide_a, slide_b = slide_b, slide_a

                moved = False
                if slide_a >= 0 and sand_now[slide_a] == 0:
                    sand_now[slide_a] = 1
                    moved = True
                if not moved and slide_b >= 0 and sand_now[slide_b] == 0:
                    sand_now[slide_b] = 1
                    moved = True
                if not moved:
                    sand_now[current] = 1

    return sector
fig = plt.figure(figsize=(10, 14))
fig.patch.set_facecolor('#1a1a2e')
ax = fig.add_subplot(1, 1, 1)

g_arrow = None

def setup_grid():
    global g_arrow
    ax.clear()
    ax.set_xlim(-0.5, 5.5)
    ax.set_ylim(-0.5, 17.5)
    ax.set_aspect('equal')
    ax.invert_yaxis()
    ax.set_facecolor('#0f0f1a')
    ax.set_xticks(range(6))
    ax.set_yticks(range(18))
    ax.tick_params(colors='#666')
    ax.grid(True, alpha=0.15, color='#333')

    for row in range(18):
        for col in range(6):
            fc = '#0a2a0a' if LED_MASK[row][col] else '#000000'
            ec = '#1a3a1a' if LED_MASK[row][col] else '#111111'
            ax.add_patch(Rectangle((col-0.5, row-0.5), 1, 1,
                           facecolor=fc, edgecolor=ec, alpha=0.5))

frame = 0

def update_anim(_):
    global frame, g_arrow
    ax_tilt = math.sin(frame * 0.03)
    ay_tilt = math.cos(frame * 0.03)

    sector = update_sand(ax_tilt, ay_tilt)
    setup_grid()

    # Gravity arrow: ax>0->gy>0(down), ay>0->gx<0(left)
    gx = -ay_tilt
    gy = +ax_tilt
    arrow_len = 1.2
    g_arrow = ax.arrow(2.5, 8, gx*arrow_len, gy*arrow_len,
                        head_width=0.3, head_length=0.3,
                        fc='yellow', ec='yellow', alpha=0.9)

    xs, ys = [], []
    for idx, val in enumerate(sand_now):
        if val > 0:
            r, c = LED_LIST[idx]
            xs.append(c)
            ys.append(r)

    ax.scatter(xs, ys, c='#ffcc44', s=120, alpha=0.9, edgecolors='#aa8800', linewidth=0.5)

    count = sum(sand_now)
    _sector_names = ['R','DR','D','DL','L','UL','U','UR']
    _d = _sector_names[sector]
    ax.set_title(f'Sand ({count}) | gravity({gx:+.2f},{gy:+.2f}) | {_d}',
                  color='#aaa', fontsize=11)
    frame += 1


ani = animation.FuncAnimation(fig, update_anim, frames=1000, interval=80, repeat=True)
plt.show()
"""
PixPill Sand Simulation (Python visualization - exact C++ logic clone)
"""
import math
import random
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Rectangle
import numpy as np

# ==================== Parameters (align with C++) ====================
ROWS, COLS = 18, 6

# LED mask (capsule shape)
LED_MASK = [[False]*COLS for _ in range(ROWS)]
for row in range(ROWS):
    for col in range(COLS):
        if (row in (0,17) and (col < 2 or col > 3)): continue
        if (row in (1,16) and (col < 1 or col > 4)): continue
        LED_MASK[row][col] = True

# LED list (same order as C++ LIQUID_LED_ROW/COL)
LED_LIST = []
for row in range(ROWS):
    for col in range(COLS):
        if LED_MASK[row][col]:
            LED_LIST.append((row, col))

NUM_LEDS = 96  # exact

# Map LED index -> (row, col)
LED_ROW = [r for r, c in LED_LIST]
LED_COL = [c for r, c in LED_LIST]

# Map (row, col) -> LED index
RC_TO_IDX = {}
for idx, (r, c) in enumerate(LED_LIST):
    RC_TO_IDX[(r, c)] = idx

# ==================== Neighbor Table (exact clone of C++) ====================
# [up, down, left, right], -1 = wall
NEIGHBOR_UP    = 0
NEIGHBOR_DOWN  = 1
NEIGHBOR_LEFT  = 2
NEIGHBOR_RIGHT = 3

LED_NEIGHBORS = [[-1]*4 for _ in range(NUM_LEDS)]

for idx, (r, c) in enumerate(LED_LIST):
    LED_NEIGHBORS[idx][NEIGHBOR_UP]    = RC_TO_IDX.get((r-1, c), -1)
    LED_NEIGHBORS[idx][NEIGHBOR_DOWN]  = RC_TO_IDX.get((r+1, c), -1)
    LED_NEIGHBORS[idx][NEIGHBOR_LEFT]  = RC_TO_IDX.get((r, c-1), -1)
    LED_NEIGHBORS[idx][NEIGHBOR_RIGHT] = RC_TO_IDX.get((r, c+1), -1)

# ==================== Scan Orders (exact clone of C++) ====================
SCAN_ORDER_GRAVITY_DOWN = [
    94, 95, 90, 91, 92, 93, 84, 85, 86, 87, 88, 89,
    78, 79, 80, 81, 82, 83, 72, 73, 74, 75, 76, 77,
    66, 67, 68, 69, 70, 71, 60, 61, 62, 63, 64, 65,
    54, 55, 56, 57, 58, 59, 48, 49, 50, 51, 52, 53,
    42, 43, 44, 45, 46, 47, 36, 37, 38, 39, 40, 41,
    30, 31, 32, 33, 34, 35, 24, 25, 26, 27, 28, 29,
    18, 19, 20, 21, 22, 23, 12, 13, 14, 15, 16, 17,
     6,  7,  8,  9, 10, 11,  2,  3,  4,  5,  0,  1
]

SCAN_ORDER_GRAVITY_RIGHT = [
     6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72,
    78, 84,  2,  7, 13, 19, 25, 31, 37, 43, 49, 55,
    61, 67, 73, 79, 85, 90,  0,  3,  8, 14, 20, 26,
    32, 38, 44, 50, 56, 62, 68, 74, 80, 86, 91, 94,
     1,  4,  9, 15, 21, 27, 33, 39, 45, 51, 57, 63,
    69, 75, 81, 87, 92, 95,  5, 10, 16, 22, 28, 34,
    40, 46, 52, 58, 64, 70, 76, 82, 88, 93, 11, 17,
    23, 29, 35, 41, 47, 53, 59, 65, 71, 77, 83, 89
]

# ==================== Sand State ====================
sand_prev = [0] * NUM_LEDS
sand_now  = [0] * NUM_LEDS
_rng = 131

# ==================== Init (exact C++ init sequence) ====================
init_indices = [0, 1, 2, 6, 18, 19, 24, 25, 31, 4, 5, 8, 9, 10, 16, 17, 23, 65, 67, 68, 71, 72, 74, 95]
for idx in init_indices:
    sand_now[idx] = 1

# ==================== C++ calc() logic (exact clone) ====================
def update_sand(ax_raw, ay_raw):
    global sand_now, sand_prev, _rng

    # Backup
    sand_prev = list(sand_now)
    sand_now  = [0] * NUM_LEDS

    # BMA530 orientation: ax = -readAx(), ay = readAy()
    ax = -ax_raw
    ay = ay_raw

    # Choose dominant direction
    if abs(ay) > abs(ax):  # Y dominant → left-right scan
        scan_order = SCAN_ORDER_GRAVITY_RIGHT
        g_dirs = [NEIGHBOR_LEFT if ay > 0 else NEIGHBOR_RIGHT,
                  NEIGHBOR_UP if ax > 0 else NEIGHBOR_DOWN]
        reverse = (ay < 0)
    else:  # X dominant → down scan
        scan_order = SCAN_ORDER_GRAVITY_DOWN
        g_dirs = [NEIGHBOR_UP if ax > 0 else NEIGHBOR_DOWN,
                  NEIGHBOR_LEFT if ay > 0 else NEIGHBOR_RIGHT]
        reverse = (ax > 0)

    for i in range(NUM_LEDS):
        current = scan_order[95 - i] if reverse else scan_order[i]
        velocity = sand_prev[current]
        if velocity == 0:
            continue

        # Check main gravity neighbor
        main_nbr = LED_NEIGHBORS[current][g_dirs[0]]
        if main_nbr < 0:  # wall
            sand_now[current] = 1
            continue

        # Multi-step fall with velocity
        fall_pos = current
        fall_steps = 0
        for s in range(velocity):
            if s >= 5:
                break
            nxt = LED_NEIGHBORS[fall_pos][g_dirs[0]]
            if nxt < 0:
                break
            if sand_now[nxt] > 0:
                break
            fall_pos = nxt
            fall_steps += 1

        if fall_steps > 0:
            sand_now[fall_pos] = min(velocity + 1, 5)
            continue

        # Try main gravity
        if sand_now[main_nbr] == 0:
            sand_now[main_nbr] = 1
        else:
            # Blocked → try side slides
            slide_a = LED_NEIGHBORS[main_nbr][g_dirs[1]]
            slide_b = LED_NEIGHBORS[main_nbr][g_dirs[1] ^ 1]

            # Randomize slide order
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


# ==================== Visualization ====================
fig = plt.figure(figsize=(10, 14))
fig.patch.set_facecolor('#1a1a2e')
ax = fig.add_subplot(1, 1, 1)

# Arrow for gravity direction (global, updated each frame)
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

    # Draw LED mask cells (same green tint as liquid sim)
    for row in range(18):
        for col in range(6):
            fc = '#0a2a0a' if LED_MASK[row][col] else '#000000'
            ec = '#1a3a1a' if LED_MASK[row][col] else '#111111'
            ax.add_patch(Rectangle((col-0.5, row-0.5), 1, 1,
                           facecolor=fc, edgecolor=ec, alpha=0.5))

    # Capsule boundary (red lines)
    y_pts = np.linspace(0, 17, 300)
    left_pts, right_pts = [], []
    for y in y_pts:
        if y <= 1.5:
            t = y / 1.5
            l, r = 2.0 - t*2.0, 3.0 + t*2.0
        elif y <= 15.5:
            l, r = 0.0, 5.0
        else:
            t = (y - 15.5) / 1.5
            l, r = 0.0 + t*2.0, 5.0 - t*2.0
        left_pts.append(l)
        right_pts.append(r)
    ax.plot(left_pts, y_pts, 'r-', alpha=0.8, lw=1.5)
    ax.plot(right_pts, y_pts, 'r-', alpha=0.8, lw=1.5)


frame = 0

def update_anim(_):
    global frame, g_arrow
    ax_tilt = math.sin(frame * 0.03)
    ay_tilt = math.cos(frame * 0.03)

    update_sand(ax_tilt, ay_tilt)

    # Compute gravity vector for arrow (same transform as C++)
    ax_raw = ax_tilt
    ay_raw = ay_tilt
    gx = -ay_raw   # gravity in col direction
    gy = -ax_raw   # gravity in row direction

    setup_grid()

    # Update gravity arrow (inside capsule, right side)
    # Note: ax.invert_yaxis() means arrow dy sign is flipped
    arrow_len = 1.2
    arr_dx = gx * arrow_len
    arr_dy = -gy * arrow_len  # negate because Y axis is inverted
    g_arrow = ax.arrow(2.5, 8, arr_dx, arr_dy, head_width=0.3, head_length=0.3,
                        fc='yellow', ec='yellow', alpha=0.9)

    xs, ys = [], []
    for idx, val in enumerate(sand_now):
        if val > 0:
            r, c = LED_LIST[idx]
            xs.append(c)
            ys.append(r)

    ax.scatter(xs, ys, c='#ffcc44', s=120, alpha=0.9, edgecolors='#aa8800', linewidth=0.5)

    count = sum(sand_now)
    # Also show main gravity direction
    if abs(ay_tilt) > abs(ax_tilt):
        main_dir = 'LEFT' if ay_tilt > 0 else 'RIGHT'
    else:
        main_dir = 'UP' if ax_tilt > 0 else 'DOWN'
    ax.set_title(f'Sand ({count} grains) | tilt(ax={ax_tilt:+.2f}, ay={ay_tilt:+.2f}) | {main_dir}',
                  color='#aaa', fontsize=11)
    frame += 1


ani = animation.FuncAnimation(fig, update_anim, frames=1000, interval=80,
                               blit=False, repeat=True)
plt.show()

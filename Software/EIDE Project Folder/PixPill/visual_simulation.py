"""
PixPill 液体模拟 + 密度场渲染 + 动态倾斜
"""
import math
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Rectangle
import numpy as np

# ==================== 参数（对齐C++） ====================

# --- 流体物理 ---
LIQUID_PARTICLE_COUNT = 20
LIQUID_DAMPING = 0.90
LIQUID_GRAVITY_SCALE = 1.8
LIQUID_DT = 1.0

# --- 粒子间碰撞 ---
LIQUID_MIN_DIST = 1.4
LIQUID_COLLISIONS_ITERS = 1
COLLISION_PUSH_K = 0.45
COLLISION_FRICTION = 0.8

# --- 表面张力 ---
LIQUID_ATTRACT_RADIUS = 1.8
LIQUID_ATTRACT_STRENGTH = 0.002
VEL_EXCHANGE_STRENGTH = 0.1

# --- 数值积分 ---
SUBSTEPS = 5

# --- 墙壁 ---
WALL_PUSH_MARGIN = 0.4
WALL_PUSH_STRENGTH = 1.5

# --- 密度场 ---
DENSITY_MAX_BRIGHTNESS = 255.0
DENSITY_PER_PARTICLE = 270.0  # 每个粒子分散到附近格子的总贡献度

ROWS, COLS = 18, 6

# LED网格
LED_MASK = [[False]*COLS for _ in range(ROWS)]
for row in range(ROWS):
    for col in range(COLS):
        if (row in (0,17) and (col < 2 or col > 3)): continue
        if (row in (1,16) and (col < 1 or col > 4)): continue
        LED_MASK[row][col] = True

LED_CENTERS = []
for row in range(ROWS):
    for col in range(COLS):
        if LED_MASK[row][col]:
            LED_CENTERS.append((float(col)+0.5, float(row)+0.5))


class LiquidParticle:
    def __init__(self):
        self.x = 0.0
        self.y = 0.0
        self.vx = 0.0
        self.vy = 0.0


# 初始化
particles = [LiquidParticle() for _ in range(LIQUID_PARTICLE_COUNT)]
rng = 131
for p in particles:
    while True:
        row = rng % 18; col = rng % 6
        rng = (rng * 131 + 53) & 0xFFFF
        if not LED_MASK[row][col]: continue
        p.x = float(col) + 0.5
        p.y = float(row) + 0.5
        break

frame_count = 0
min_dist_sq = LIQUID_MIN_DIST * LIQUID_MIN_DIST
attract_sq = LIQUID_ATTRACT_RADIUS * LIQUID_ATTRACT_RADIUS


# ==================== 边界 ====================
def get_boundary(y):
    if y <= 1.5:
        t = y / 1.5
        return 2.0 - t * 2.0, 3.0 + t * 2.0
    elif y <= 15.5:
        return 0.0, 5.0
    else:
        t = (y - 15.5) / 1.5
        return 0.0 + t * 2.0, 5.0 - t * 2.0


def clamp_particle(p):
    left, right = get_boundary(p.y)
    margin = WALL_PUSH_MARGIN
    strength = WALL_PUSH_STRENGTH

    if p.y > 17 - margin:
        depth = p.y - (17 - margin)
        p.vy -= depth * strength
        if p.vy > 0: p.vy *= 0.2
    if p.y < margin:
        depth = margin - p.y
        p.vy += depth * strength
        if p.vy < 0: p.vy *= 0.2

    if p.x < left + margin:
        p.vx += (left + margin - p.x) * strength
    if p.x > right - margin:
        p.vx -= (p.x - (right - margin)) * strength

    if p.x < left: p.x = left; p.vx = 0
    elif p.x > right: p.x = right; p.vx = 0
    if p.y < 0: p.y = 0
    if p.y > 17: p.y = 17


# ==================== 物理 ====================
def update_particles(dt, ax_tilt, ay_tilt):
    """ax_tilt, ay_tilt: 归一化加速度（-1~1）"""
    # BMA530方向: gravity.x = -ay, gravity.y = ax
    gx = -ay_tilt * LIQUID_GRAVITY_SCALE
    gy = ax_tilt * LIQUID_GRAVITY_SCALE

    for p in particles:
        p.vx += gx * dt
        p.vy += gy * dt
        p.vx *= LIQUID_DAMPING
        p.vy *= LIQUID_DAMPING
        p.x += p.vx * dt
        p.y += p.vy * dt
        clamp_particle(p)


def process_collisions():
    """
    碰撞：参考代码风格 - 弹性碰撞 + 线性推开
    """
    for iteration in range(LIQUID_COLLISIONS_ITERS):
        for i in range(LIQUID_PARTICLE_COUNT):
            for j in range(i + 1, LIQUID_PARTICLE_COUNT):
                dx = particles[j].x - particles[i].x
                dy = particles[j].y - particles[i].y
                dist_sq = dx * dx + dy * dy

                if dist_sq < 0.001:
                    particles[i].x -= 0.03; particles[i].y -= 0.03
                    particles[j].x += 0.03; particles[j].y += 0.03
                    continue

                if dist_sq < min_dist_sq:
                    dist = math.sqrt(dist_sq)
                    ex = dx / dist; ey = dy / dist

                    # 线性推开（参考: overlap = 0.5*(MIN_DIST - dist)）
                    overlap = 0.5 * (LIQUID_MIN_DIST - dist)
                    particles[i].x -= overlap * ex
                    particles[i].y -= overlap * ey
                    particles[j].x += overlap * ex
                    particles[j].y += overlap * ey

                    # 弹性碰撞动量交换（参考: avg = (vA+vB)/2）
                    vA = particles[i].vx * ex + particles[i].vy * ey
                    vB = particles[j].vx * ex + particles[j].vy * ey
                    avg = (vA + vB) / 2.0

                    particles[i].vx += (avg - vA) * ex
                    particles[i].vy += (avg - vA) * ey
                    particles[j].vx += (avg - vB) * ex
                    particles[j].vy += (avg - vB) * ey

    # 表面张力
    for i in range(LIQUID_PARTICLE_COUNT):
        for j in range(i + 1, LIQUID_PARTICLE_COUNT):
            dx = particles[j].x - particles[i].x
            dy = particles[j].y - particles[i].y
            dist_sq = dx * dx + dy * dy
            if dist_sq >= min_dist_sq and dist_sq < attract_sq:
                dist = math.sqrt(dist_sq)
                ex = dx / dist; ey = dy / dist
                force = LIQUID_ATTRACT_STRENGTH * (LIQUID_ATTRACT_RADIUS - dist)
                particles[i].vx += force * ex; particles[i].vy += force * ey
                particles[j].vx -= force * ex; particles[j].vy -= force * ey

    for p in particles:
        clamp_particle(p)


def compute_density_field():
    """
    双线性插值密度场：每个粒子贡献到最近的4个LED格子
    按X/Y方向距离比例分配权重，总贡献=DENSITY_PER_PARTICLE
    C++友好：无exp()，只有加减乘除
    """
    density = np.zeros((ROWS, COLS))

    for p in particles:
        fx = p.x  # 浮点列 (0~5)
        fy = p.y  # 浮点行 (0~17)

        col0 = int(fx)
        col1 = col0 + 1
        row0 = int(fy)
        row1 = row0 + 1

        wx1 = fx - col0
        wx0 = 1.0 - wx1
        wy1 = fy - row0
        wy0 = 1.0 - wy1

        for (r, c, w) in [
            (row0, col0, wx0 * wy0),
            (row0, col1, wx1 * wy0),
            (row1, col0, wx0 * wy1),
            (row1, col1, wx1 * wy1),
        ]:
            if 0 <= r < ROWS and 0 <= c < COLS and LED_MASK[r][c]:
                density[r][c] += DENSITY_PER_PARTICLE * w

    brightness = np.clip(density, 0, DENSITY_MAX_BRIGHTNESS)
    return density, brightness


def step():
    global frame_count
    # 动态倾斜：放大正弦周期，让倾斜变化慢一点
    # 上下倾斜: ax = sin(frame*0.02), 左右倾斜: ay = cos(frame*0.02)
    ax_tilt = math.sin(frame_count * 0.02)
    ay_tilt = math.cos(frame_count * 0.02)

    dt_sub = LIQUID_DT / SUBSTEPS
    for s in range(SUBSTEPS):
        update_particles(dt_sub, ax_tilt, ay_tilt)
        process_collisions()
    frame_count += 1


# ==================== 可视化 ====================
fig = plt.figure(figsize=(13, 8))
gs = fig.add_gridspec(2, 2, width_ratios=[1.2, 1], height_ratios=[1, 1],
                      hspace=0.35, wspace=0.3)

ax_particles = fig.add_subplot(gs[0, 0])
ax_density = fig.add_subplot(gs[0, 1])
ax_hist = fig.add_subplot(gs[1, 0])
ax_pwm = fig.add_subplot(gs[1, 1])
fig.patch.set_facecolor('#1a1a2e')

# --- 粒子图 ---
ax_particles.set_xlim(-0.5, 5.5); ax_particles.set_ylim(-0.5, 17.5)
ax_particles.set_aspect('equal'); ax_particles.invert_yaxis()
ax_particles.set_facecolor('#0f0f1a')
ax_particles.set_xticks(range(6)); ax_particles.set_yticks(range(18))
ax_particles.tick_params(colors='#666')
ax_particles.grid(True, alpha=0.15, color='#333')
ax_particles.set_title('Particles', color='#aaa', fontsize=11)

for row in range(18):
    for col in range(6):
        fc = '#0a2a0a' if LED_MASK[row][col] else '#000000'
        ec = '#1a3a1a' if LED_MASK[row][col] else '#111111'
        ax_particles.add_patch(Rectangle((col-0.5, row-0.5), 1, 1,
                               facecolor=fc, edgecolor=ec, alpha=0.5))
y_pts = np.linspace(0, 17, 300)
left_pts, right_pts = [], []
for y in y_pts:
    l, r = get_boundary(y)
    left_pts.append(l); right_pts.append(r)
ax_particles.plot(left_pts, y_pts, 'r-', alpha=0.8, lw=1.5)
ax_particles.plot(right_pts, y_pts, 'r-', alpha=0.8, lw=1.5)
scatter = ax_particles.scatter([], [], c='#00ccff', s=60, alpha=0.9,
                                edgecolors='white', linewidth=0.5)
# 重力方向指示箭头
g_arrow = ax_particles.arrow(5.2, 1, 0, 0, head_width=0.3, head_length=0.3,
                              fc='yellow', ec='yellow', alpha=0.8)

# --- 密度场 ---
ax_density.set_xlim(-0.5, 5.5); ax_density.set_ylim(-0.5, 17.5)
ax_density.set_aspect('equal'); ax_density.invert_yaxis()
ax_density.set_facecolor('#0f0f1a')
ax_density.set_xticks(range(6)); ax_density.set_yticks(range(18))
ax_density.tick_params(colors='#666')
ax_density.set_title('Density Field', color='#aaa', fontsize=11)
density_img = ax_density.imshow(np.zeros((ROWS, COLS)), origin='upper',
                                 cmap='inferno', vmin=0, vmax=1.5,
                                 extent=[-0.5, 5.5, 17.5, -0.5],
                                 interpolation='bilinear')

# --- PWM ---
pwm_display = np.zeros((ROWS, COLS))
pwm_img = ax_pwm.imshow(pwm_display, origin='upper',
                          cmap='inferno', vmin=0, vmax=255,
                          extent=[-0.5, 5.5, 17.5, -0.5],
                          interpolation='nearest')
ax_pwm.set_xlim(-0.5, 5.5); ax_pwm.set_ylim(-0.5, 17.5)
ax_pwm.set_aspect('equal'); ax_pwm.invert_yaxis()
ax_pwm.set_facecolor('#0f0f1a')
ax_pwm.set_xticks(range(6)); ax_pwm.set_yticks(range(18))
ax_pwm.tick_params(colors='#666')
ax_pwm.set_title('PWM Output (LED view)', color='#aaa', fontsize=11)
for row in range(18):
    for col in range(6):
        if not LED_MASK[row][col]:
            ax_pwm.add_patch(Rectangle((col-0.5, row-0.5), 1, 1,
                             facecolor='#222', edgecolor='#111', alpha=0.8))

# --- 间距分布 ---
ax_hist.set_facecolor('#0f0f1a')
ax_hist.tick_params(colors='#666')
ax_hist.set_xlim(0, 5)
ax_hist.set_title('Pairwise Distances', color='#aaa', fontsize=11)
ax_hist.axvline(LIQUID_MIN_DIST, color='r', alpha=0.5, ls='--')


def update(_):
    step()

    xs = [p.x for p in particles]
    ys = [p.y for p in particles]
    scatter.set_offsets(np.column_stack([xs, ys]))

    density, brightness = compute_density_field()
    density_img.set_data(density)
    pwm_img.set_data(brightness)

    # 粒子间距
    distances = []
    for i in range(LIQUID_PARTICLE_COUNT):
        for j in range(i + 1, LIQUID_PARTICLE_COUNT):
            dx = particles[j].x - particles[i].x
            dy = particles[j].y - particles[i].y
            distances.append(math.sqrt(dx*dx + dy*dy))

    ax_hist.clear()
    ax_hist.set_facecolor('#0f0f1a')
    ax_hist.tick_params(colors='#666')
    ax_hist.set_xlim(0, 5)
    ax_hist.axvline(LIQUID_MIN_DIST, color='r', alpha=0.5, ls='--')
    if distances:
        ax_hist.hist(distances, bins=30, range=(0,5), color='#00ccff', alpha=0.7,
                     edgecolor='#003366')
        zero = sum(1 for d in distances if d < 0.001)
        below = sum(1 for d in distances if d < 0.5)
        ax_hist.text(0.98, 0.95, f'zero:{zero} <0.5:{below}/{len(distances)}',
                    transform=ax_hist.transAxes, ha='right', va='top',
                    color='#ff4444', fontsize=10)

    row_counts = {}
    for p in particles:
        r = int(p.y + 0.5)
        row_counts[r] = row_counts.get(r, 0) + 1
    r17 = row_counts.get(17,0); r16 = row_counts.get(16,0); r15 = row_counts.get(15,0)
    above = sum(1 for p in particles if int(p.y+0.5) <= 13)

    # 当前倾斜方向
    ax_tilt = math.sin(frame_count * 0.02)
    ay_tilt = math.cos(frame_count * 0.02)
    ax_particles.set_title(
        f'Particles ({LIQUID_PARTICLE_COUNT}) | '
        f'tilt({ax_tilt:+.2f},{ay_tilt:+.2f}) | '
        f'r17:{r17} r16:{r16} r15:{r15} up:{above}',
        color='#aaa', fontsize=11)

    return [scatter, density_img, pwm_img]


ani = animation.FuncAnimation(fig, update, frames=500, interval=60,
                               blit=False, repeat=False)
plt.show()

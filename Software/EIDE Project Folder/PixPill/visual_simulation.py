"""
PixPill Liquid Simulation (Dynamic Gravity Tilt)
"""
import math
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Rectangle
import numpy as np

# ==================== Parameters ====================
LIQUID_PARTICLE_COUNT = 16

# --- Numerical Integration ---
SUBSTEPS = 2

# --- Fluid Physics ---
LIQUID_DAMPING = 0.97
LIQUID_GRAVITY_SCALE = 1.55
LIQUID_DT = 1.0

# --- Particle Collision ---
LIQUID_MIN_DIST = 1.29
LIQUID_COLLISION_DAMPING = 0.7
LIQUID_COLLISIONS_ITERS = 1

# --- Surface Tension ---
LIQUID_ATTRACT_STRENGTH = 0.0
LIQUID_ATTRACT_RADIUS = 1.8

# --- Walls ---
WALL_PUSH_MARGIN = 0.3
WALL_PUSH_STRENGTH = 1.4

# --- Density Field ---
DENSITY_MAX_BRIGHTNESS = 255.0
DENSITY_PER_PARTICLE = 270.0  # Total contribution per particle to nearby cells

LIQUID_DENSITY_PRESSURE_THRESHOLD = 92.0
LIQUID_PRESSURE_FORCE = 3.6

ROWS, COLS = 18, 6



# LED grid config
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


# Initialize particles
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


# ==================== Boundary ====================
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


# ==================== Physics ====================
def update_particles(dt, ax_tilt, ay_tilt):
    """ax_tilt, ay_tilt: normalized acceleration (-1 to 1)"""
    # BMA530 orientation: gravity.x = -ay, gravity.y = ax
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
    Collision + light density pressure
    """
    density, _ = compute_density_field()

    # === Particle-particle collision (elastic + linear push) ===
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

                    # Linear push
                    overlap = 0.5 * (LIQUID_MIN_DIST - dist)
                    particles[i].x -= overlap * ex
                    particles[i].y -= overlap * ey
                    particles[j].x += overlap * ex
                    particles[j].y += overlap * ey

                    # Elastic collision
                    vA = particles[i].vx * ex + particles[i].vy * ey
                    vB = particles[j].vx * ex + particles[j].vy * ey
                    avg = (vA + vB) / 2.0 * LIQUID_COLLISION_DAMPING
                    particles[i].vx += (avg - vA) * ex
                    particles[i].vy += (avg - vA) * ey
                    particles[j].vx += (avg - vB) * ex
                    particles[j].vy += (avg - vB) * ey

    # === Density pressure: push velocity away from dense areas ===
    # Only runs after collision, no extra collision iteration
    density2, _ = compute_density_field()
    for p in particles:
        row = int(p.y + 0.5)
        col = int(p.x + 0.5)
        if 0 <= row < ROWS and 0 <= col < COLS and LED_MASK[row][col]:
            d = density2[row][col]
            if d > LIQUID_DENSITY_PRESSURE_THRESHOLD:
                excess = (d - LIQUID_DENSITY_PRESSURE_THRESHOLD) / DENSITY_MAX_BRIGHTNESS
                # Accumulate push direction toward lowest-density neighbor
                push_x, push_y = 0.0, 0.0
                total_w = 0.0
                for dr in range(-1, 2):
                    for dc in range(-1, 2):
                        if dr == 0 and dc == 0:
                            continue
                        nr, nc = row + dr, col + dc
                        if 0 <= nr < ROWS and 0 <= nc < COLS and LED_MASK[nr][nc]:
                            # Weight = density difference (positive = neighbor is sparser)
                            diff = d - density2[nr][nc]
                            if diff > 0:
                                w = diff / DENSITY_MAX_BRIGHTNESS
                                push_x += dc * w
                                push_y += dr * w
                                total_w += w
                if total_w > 0.001:
                    push_x /= total_w
                    push_y /= total_w
                    p.vx += push_x * excess * LIQUID_PRESSURE_FORCE
                    p.vy += push_y * excess * LIQUID_PRESSURE_FORCE

    # Surface tension (disabled)
    if LIQUID_ATTRACT_STRENGTH > 0.0:
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
    Bilinear interpolation density field:
    Each particle contributes to 4 nearest LED cells, weighted by distance.
    Sum contribution = DENSITY_PER_PARTICLE
    C++-friendly: no exp(), only + - * /
    """
    density = np.zeros((ROWS, COLS))

    for p in particles:
        fx = p.x  # float column (0~5)
        fy = p.y  # float row (0~17)

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
    # Dynamic tilt with slow sine period
    # Vertical: ax = sin(frame*0.02), horizontal: ay = cos(frame*0.02)
    ax_tilt = math.sin(frame_count * 0.02)
    ay_tilt = math.cos(frame_count * 0.02)

    dt_sub = LIQUID_DT / SUBSTEPS
    for s in range(SUBSTEPS):
        update_particles(dt_sub, ax_tilt, ay_tilt)
        process_collisions()
    frame_count += 1


# ==================== 可视化 ====================
fig = plt.figure(figsize=(16, 9))
gs = fig.add_gridspec(2, 2, width_ratios=[2.5, 1], height_ratios=[1, 1],
                      hspace=0.3, wspace=0.25)

ax_particles = fig.add_subplot(gs[:, 0])     # 左全高：粒子图（大）
ax_hist = fig.add_subplot(gs[0, 1])         # 右上：直方图
ax_pwm = fig.add_subplot(gs[1, 1])          # 右下：PWM Output
fig.patch.set_facecolor('#1a1a2e')

# --- 粒子图 ---
ax_particles.set_xlim(-0.5, 5.5); ax_particles.set_ylim(-0.5, 17.5)
ax_particles.set_aspect('equal'); ax_particles.invert_yaxis()
ax_particles.set_facecolor('#0f0f1a')
ax_particles.set_xticks(range(6)); ax_particles.set_yticks(range(18))
ax_particles.tick_params(colors='#666')
ax_particles.grid(True, alpha=0.15, color='#333')
ax_particles.set_title('Particles', color='#aaa', fontsize=11)

# 密度场格子（每帧更新颜色，用 inferno 色阶）
grid_patches = []
for row in range(18):
    for col in range(6):
        if LED_MASK[row][col]:
            p = Rectangle((col-0.5, row-0.5), 1, 1,
                          facecolor='#0a2a0a', edgecolor='#1a3a1a', alpha=0.5)
            ax_particles.add_patch(p)
            grid_patches.append((row, col, p))

# 柔和的蓝色调 colormap（密度 → 深蓝 → 亮青 → 淡蓝白）
from matplotlib.colors import LinearSegmentedColormap
density_cmap = LinearSegmentedColormap.from_list('density_soft', [
    (0.00, '#0a1018'),   # 密度0：深暗蓝
    (0.15, '#0d2840'),   # 低密度：深海蓝
    (0.35, '#0e4a6e'),   # 中低：钢蓝
    (0.55, '#1a6e94'),   # 中：湖蓝
    (0.75, '#3d9db5'),   # 中高：浅湖蓝
    (1.00, '#7ec8d8'),   # 高密度：淡青白
])

# 密度色标(colorbar) — 放在粒子图右侧的细条
from matplotlib.colorbar import ColorbarBase
from matplotlib.colors import Normalize
cax_density = fig.add_axes([0.61, 0.11, 0.02, 0.78])
density_norm = Normalize(vmin=0, vmax=DENSITY_MAX_BRIGHTNESS)
density_cbar = ColorbarBase(cax_density, cmap=density_cmap, norm=density_norm,
                             orientation='vertical')
density_cbar.set_label('Density', color='#aaa', fontsize=9)
density_cbar.ax.tick_params(colors='#aaa', labelsize=8)
density_cbar.outline.set_edgecolor('#444')
y_pts = np.linspace(0, 17, 300)
left_pts, right_pts = [], []
for y in y_pts:
    l, r = get_boundary(y)
    left_pts.append(l); right_pts.append(r)
ax_particles.plot(left_pts, y_pts, 'r-', alpha=0.8, lw=1.5)
ax_particles.plot(right_pts, y_pts, 'r-', alpha=0.8, lw=1.5)
scatter = ax_particles.scatter([], [], c='#00ccff', s=80, alpha=0.9,
                                edgecolors='white', linewidth=0.5)
# 重力方向指示箭头
g_arrow = ax_particles.arrow(5.2, 1, 0, 0, head_width=0.3, head_length=0.3,
                              fc='yellow', ec='yellow', alpha=0.8)



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
    pwm_img.set_data(brightness)

    # === 更新密度场格子颜色（柔和蓝色调） ===
    for row, col, patch in grid_patches:
        d = density[row][col]
        if d <= 0:
            patch.set_facecolor('#060a10')
            patch.set_edgecolor('#10151f')
            patch.set_alpha(0.25)
        else:
            ratio = min(d / DENSITY_MAX_BRIGHTNESS, 1.0)
            rgba = density_cmap(ratio)
            patch.set_facecolor(rgba[:3])
            patch.set_edgecolor((rgba[0]*0.3, rgba[1]*0.3, rgba[2]*0.3))
            patch.set_alpha(0.3 + ratio * 0.45)

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
        f'Gravity tilt({ax_tilt:+.2f},{ay_tilt:+.2f}) | '
        f'r17:{r17} r16:{r16} r15:{r15} up:{above}',
        color='#aaa', fontsize=11)

    return [scatter, pwm_img]


ani = animation.FuncAnimation(fig, update, frames=500, interval=60,
                               blit=False, repeat=False)
plt.show()

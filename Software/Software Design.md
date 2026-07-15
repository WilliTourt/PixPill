# PixPill Software Design

> Firmware architecture, simulation algorithms, gesture recognition, power management, and development guide

---

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [State Machine](#state-machine)
- [Sand Simulation (class SandSim)](#sand-simulation-class-sandsim)
- [Liquid Simulation (class LiquidSim)](#liquid-simulation-class-liquidsim)
- [Animation System (class PixPillAnim)](#animation-system-class-pixpillanim)
- [Gesture Recognition](#gesture-recognition)
- [Power Management](#power-management)
- [LED Open/Short Detection](#led-open-short-detection)
- [Firmware Version Adaptation (Conditional Compilation)](#firmware-version-adaptation-conditional-compilation)
- [Development Environment](#development-environment)
- [Desktop Simulators](#desktop-simulators)

---

## Architecture Overview

PixPill firmware uses a **bare-metal superloop** without an RTOS. The main loop is driven by HAL delays, running particle simulation and gesture detection at a fixed frame rate.

```
main.c → cpp_main()

Components/
├── Src/
│   ├── cpp_main.cpp     ← state machine, gesture detection, main loop (~350 lines)
│   ├── bma530.cpp       ← BMA530 I2C accelerometer driver
│   ├── is31fl3736.cpp   ← IS31FL3736 I2C LED matrix driver (paged register access)
│   ├── sandsim.cpp      ← sand simulation (cellular automaton, 32 particles)
│   ├── liquidsim.cpp    ← liquid simulation (SPH-inspired, 16 particles; kept low for performance on Cortex-M0+)
│   └── pixpill_anim.cpp ← boot / shutdown / charging / error animations
└── Inc/                 ← headers + static lookup tables (constexpr LUTs)
```

### System Resources

| Resource | Usage | Notes |
| ------- | ------- | ------- |
| **RAM** | 2872 B / 6 KB (46.74%) | Includes stack, particle buffers, lookup tables |
| **FLASH** | 32656 B / 32 KB (99.66%) | Extremely compact under -Os optimization; flash usage is nearly full |
| **I2C** | I2C1 (PB6-SCL, PC14-SDA) | Shared by BMA530 (addr 0x18 << 1) + IS31FL3736 (addr 0x50 << 1) |
| **TIM3_CH2** | PA7 — LED_STATUS breathing | 1kHz PWM, sin² 64-step LUT |
| **GPIO** | PA8 (ERR), PB7 (CHG), PC15 (SHPACT) | nPM1100 monitoring & control |

Roughly one third of FLASH is consumed by LUTs. Given the C011's 48 MHz computational ceiling, precomputed tables are used extensively to keep the simulation fluent.

---

## State Machine

```
 [Power-on / Ship Mode Wake]
        ↓
      BOOT → Boot animation ("PIXPILL" scrolling text)
        ↓
    RUNNING → Physics simulation (default: liquid mode)
        │ 4 shakes within 570ms  → toggle sand/liquid
        │ Still 22s              → SHUTDOWN
        │ nPM ERR                → fault handling
        │ USB plug-in            → charging indicator 4s then resume
        ↓
   SHUTDOWN → Shutdown animation → SHPACT high → enter ship mode
```

- **State transitions are checked only at the top of the main loop** to prevent accidental jumps
- **ERR state** has the highest priority — immediately pauses simulation, flashes LED array alarm, resumes after ERR clears

---

## Sand Simulation (class SandSim)

Based on the classic **falling-sand cellular automaton** algorithm.

### Key Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Particle count | 32 | Moving across 96 LED grid cells with velocity and acceleration |
| Gravity directions | 8 (N/S/E/W + diagonals) | Determined by BMA530 accelerometer gravity sector |
| Scan orders | 4 (down/right/down-right/up-right) | Direction-matched row scanning prevents repeated processing |
| Speed levels | 0–5 | Particles accumulate momentum for faster falling |

### Implementation Details

- **Neighbor table**: 96×8 static LUT storing neighbor indices for all 8 directions per cell — O(1) access
- **Scan table**: Scan order selected based on gravity direction, ensuring correct stacking without penetration
- **Speed mechanism**: Particles accumulate velocity in the falling direction (up to 5 steps); steeper tilt = faster fall
- **Adaptive frame rate**: Higher tilt angle → faster frame rate; still → decelerate

---

## Liquid Simulation (class LiquidSim)

SPH-inspired (Smoothed Particle Hydrodynamics) implementation computing density field pressure and inter-particle forces.

### Key Parameters

| Parameter | Final Value | Notes |
|-----------|-------------|-------|
| Particle count | 16 | Each particle contributes to multiple LED brightness values |
| SUBSTEPS | 2 | Multiple sub-step integrations per frame for stability |
| GRAVITY | 1.4 | Gravitational acceleration |
| MIN_DIST | 1.24 | Minimum particle distance (collision threshold) |
| DAMPING | 0.88 | Velocity damping coefficient |
| COLLISION_DAMPING | 0.5 | Collision energy loss |
| WALL_STRENGTH | 1.4 | Boundary rebound force |
| THRESHOLD | 95 | Density pressure trigger threshold |
| FORCE | 3.6 | Density pressure coefficient |

### Implementation Details

- **Density field**: 18×6 grid; each particle's density contribution is distributed to 4 nearest cells via **bilinear interpolation**
- **Pressure**: Density above threshold generates outward push, distributing particles more evenly
- **Collision detection**: Repulsive force applied when inter-particle distance < MIN_DIST
- **Boundary handling**: Particles bounce off LED array edges with damping
- **Coordinate system**: BMA530 and LED array are mounted back-to-back; gravity.x/y must be negated

### Parameter Tuning

The desktop tuning script `liquid_visual_sim.py` allows visualization and parameter adjustment on a PC before syncing to C++ firmware. Final values were determined through multiple iterations.

---

## Gesture Recognition

Shake gesture detection via **BMA530 X-axis direction change counting**.

### Algorithm

```
Threshold SHAKE_THRESHOLD = 14000 (~±0.9g)
Window SHAKE_WINDOW_MS = 570 ms
Target = 8 direction flips (i.e., 4 up-down shake cycles)

1. Read accelerometer X-axis raw value each frame
2. Exceeding ±14000 counts as a valid direction signal
3. Direction flip → counter +1
4. 8 flips within 570ms → trigger mode toggle
5. Timeout → reset counter
```

### Design Considerations

- **Short detection window (570ms)**: Prevents false triggers during normal handling
- **High threshold (14000)**: Requires a clear, rapid shake motion
- **X-axis only**: Reduces false positives; up-down shake is most natural

---

## Power Management

### nPM1100 Ship Mode

- **Entry condition**: Still for 22 seconds
- **Idle detection**: Frame-by-frame comparison of BMA530 acceleration delta. Uses `MOTION_DELTA_THRESHOLD=200` — acceleration delta below threshold in any orientation counts as still
- **Shutdown sequence**: Play shutdown animation → SHPACT high (if not charging, 3.0V VOUTB will be cut, MCU will lose power) → STOP mode

### Charging Detection

- USB plug-in pulls `nPM1100 CHG` pin low
- **Charging animation**: Battery icon + LED_STATUS breathing light, plays for 4 seconds then resumes normal simulation
- **After unplug**: Charging animation ends, normal operation continues

### Fault Detection

- `nPM1100 ERR` pin low → immediately pause simulation
- LED array flashes "ERR" text, LED_STATUS fast blinks (100 ms period)
- After ERR clears → resume simulation
- See [nPM1100_PS_v1.5](/Docs/nPM1100_PS_v1.5.pdf) datasheet for specific ERR causes

---

## Animation System (class PixPillAnim)

| Animation | Description | Mechanism |
|-----------|-------------|-----------|
| **Boot** | "PIXPILL" scrolling text | 6-column sliding window over 42-frame bitmap, ~1.9s |
| **Shutdown** | Full brightness → row-by-row fade out | Rows dim from top to bottom |
| **Charging** | Battery outline steady + breathing fill | 1.5s cycle, batch PWM updates to avoid I2C flicker |
| **Error** | "ERR" full-screen blink | 250ms on/off, PWM set once only |

- Animations are **time-driven** (HAL_GetTick) rather than frame-driven, immune to main loop timing variations
- LED matrix glyphs use 18-bit uint32_t bitmaps (6 columns × 3 uint32_t)
- **LUT optimization**: LED index mapping table generated once at init, then O(1) lookup

---

## LED Open/Short Detection

The IS31FL3736 integrates independent open-circuit and short-circuit detection for each LED, and the firmware runs a full LED detection pass once at startup.

### Detection Flow

| Register | Address | Description |
|----------|---------|-------------|
| Open Register | PG0 18h-2Fh | 24 bytes; every even bit (0, 2, 4, 6, ...) corresponds to one LED open-circuit status |
| Short Register | PG0 30h-47h | 24 bytes; every even bit corresponds to one short-circuit status |

Register bit layout is identical to the LED On/Off Register (00h-17h): each SW occupies 2 bytes, and D0/D2/D4/D6 correspond to CS1-CS4 (low address) and CS5-CS8 (high address).

1. Set GCC = 0x01
2. Turn on all LEDs (LEDs that remain off will not refresh the detection data)
3. Set the Configuration Register OSD bit 0 → 1 (write 0x04) to trigger a one-shot detection
4. Wait for at least 2 scan cycles (5 ms)
5. Read the Open/Short registers and decode them into 96 boolean values; true means an error
6. Restore the LEDs to the off state and restore the original GCC

### Result Handling

- If detection succeeds (`valid == true`), scan all 96 LEDs and count open/short faults; skip indices 0–5 on the 1# capsule because they do not physically exist
- If any fault exists, play the ERR animation for 3 seconds and then continue normal operation
- If all are normal, proceed silently into the main loop

---

## Firmware Version Adaptation (Conditional Compilation)

PixPill has two capsule versions (for the 000# and 1# capsule), switched by conditional compilation with `PIXPILL_SIZE_1_CAPSULE`.

### Version Differences

| Parameter | 000# | 1# |
|----------|------|----|
| Diameter | 6.6 mm | 8 mm |
| LED count | 96 | 90 |
| Physical button | Rear side, on the same side as other components | Top side occupies row 0-1 (6 LEDs) |

### Switch Method

- Edit [Components/Inc/PIXPILL_VERSION_SELECT.h](Components/Inc/PIXPILL_VERSION_SELECT.h) and uncomment `#define PIXPILL_SIZE_1_CAPSULE`
- Or directly uncomment `-DPIXPILL_SIZE_1_CAPSULE` at Makefile line 116

### Adaptation Details

Principle: keep the 96-index layout unchanged and treat the missing LEDs as "walls".

- `LIQUID_LED_MASK`: on the 1# version, all LEDs in row 0-1 are set to false; liquid particle initialization and density field calculation automatically skip them
- `LED_NEIGHBORS[96][8]`: on the 1# version, neighbors for indices 0-5 in all 8 directions are set to -1 (wall), so sand cannot move into those positions
- `SandSim::init()`: on the 1# version, the random initialization range changes from 0-95 to 6-95
- Animation and liquid simulation do not need changes; `LIQUID_LED_MASK` filters them automatically

The rest of the simulation code (scan tables, particle arrays, animation frames, etc.) remains unchanged.

---

## Development Environment

### Recommended Setup

This project is developed using **VSCode + EIDE plugin**, which is the recommended approach.

- **EIDE (Embedded IDE)**: VSCode extension for embedded development — provides integrated project management, compilation, and flashing
  - Install: Search "EIDE" in the VSCode Extension Marketplace
  - Import project: Open the PixPill folder or the PixPill.code-workspace workspace file
- **STM32CubeMX**: Generate HAL initialization code and `.ioc` project file
- **arm-none-eabi-gcc**: Cross-compilation toolchain
- **ST-LINK / J-Link / DAPLink**: Debug probes

### Build and Flash

**Method 1: EIDE (Recommended)**

In VSCode with the EIDE plugin, click the Build button to compile.

**Method 2: GNU Make**

```bash
cd PixPill
make -j$(nproc)
```

After connecting the debug probe, click the Flash button or use CubeProgrammer to flash.

---

## Desktop Simulators

All particle algorithms can first be validated and tuned in Python desktop simulators before firmware development, avoiding repeated flash-debug cycles.

| Script | Purpose |
|--------|---------|
| `sand_visual_sim.py` | Sand 8-direction simulation, neighbor table + scan table visualization |
| `liquid_visual_sim.py` | Liquid SPH parameter tuning, density field / PWM dual-window visualization |

Python simulator output maps 1:1 to the C++ firmware LED array. Parameters can be tuned in Python and copied directly into the C++ code.
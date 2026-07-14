# 💊 PixPill

> A pill-sized LED trinket — with sand and liquid flowing inside. Shake it, tilt it, watch it flow.

> [ENGLISH](README.md) | [中文](README-zh-CN.md)

PixPill is a tiny gadget barely larger than a real pill capsule. On a 4-layer HDI micro PCB, it packs an STM32C011 MCU, a BMA530 accelerometer, an IS31FL3736 LED matrix driver, 96× micro LEDs, and an nPM1100 PMIC. The firmware runs physics-based particle simulations — tilt it and sand pours, liquid flows.

Its tiny PCB dimensions (23.9×8.6 mm / 19.5×6.9 mm) are deliberately aligned with standardized size 000 (26.1×8.5 mm) and size 1 (19.4×6.9 mm) pill capsules.

For detailed hardware design and 3D-related information, see [Hardware Design.md](Hardware/Hardware%20Design.md). For software details, see [Software Design.md](Software/Software%20Design.md).

---

## Features

- **「Tiny」** — PCB as narrow as 6.9 mm in diameter; fits inside a real pill capsule
- **Two physics modes** — Sand (granular, stacks naturally) and Liquid (SPH-inspired fluid with density pressure)
- **Gesture control** — Shake up-down-up-down rapidly to toggle between sand and liquid modes
- **Auto sleep** — Enters nPM1100 ship mode (shutdown) after 22 seconds of stillness to preserve battery
- **USB charging** — microUSB for charging
- **Fault detection** — nPM1100 ERR pin triggers LED array blinking alarm, pausing normal operation
- **Animated transitions** — Boot, shutdown, charging, and error states each have unique LED animations

---

## Hardware

| Component | Part | Notes |
|-----------|------|-------|
| **MCU** | STM32C011D6Y6TR | Cortex-M0+ @ 48 MHz, WLCSP12 (1.7×1.42×0.6 mm, 0.35 mm pitch) |
| **IMU** | BMA530 | 3-axis accelerometer, I²C, WLCSP6 (1.2×0.8×0.55 mm³) |
| **LED Driver** | IS31FL3736 | 12×8 LED matrix controller, I²C paged register access, per-LED 8-bit PWM, 5×5 QFN |
| **LEDs** | 90× 0201 or 96× 0402 | Arranged in a pill-shaped layout |
| **PMIC** | nPM1100 | Li-Po charger, max 400 mA LDO, with ship mode |
| **Battery** | 08120 3.7V Li-Po | Charged via microUSB |
| **PCB** | 4-layer 1st-order HDI | 23.9×8.6 mm / 19.5×6.9 mm |

---

## Simulation

### Sand Mode

- Based on a cellular-automaton classic falling-sand algorithm
- 8 gravity directions, conforming to the natural angle of repose of sand
- Particles stack naturally along gravity; tilt spills them
- Frame rate adapts to tilt angle

### Liquid Mode

- SPH-inspired implementation: gravity + density field pressure + inter-particle repulsion + velocity damping
- Finer sub-step integration per frame
- Inter-particle collision detection
- 18×6 density grid mapped to the 96-LED pill-shaped layout
- Surface tension available but currently disabled

---

## Firmware Architecture

```
main.c                   ← STM32CubeMX HAL init → cpp_main() main loop
Components/
├── Src/
│   ├── cpp_main.cpp     ← state machine, gesture detection, main loop
│   ├── bma530.cpp       ← BMA530 I²C driver
│   ├── is31fl3736.cpp   ← IS31FL3736 I²C driver
│   ├── sandsim.cpp      ← sand particle simulation
│   ├── liquidsim.cpp    ← liquid particle simulation
│   └── pixpill_anim.cpp ← boot / shutdown / charging / error animations
└── Inc/                 ← headers + various constexpr LUTs
```

- **No RTOS** — bare-metal superloop
- **State machine:** `BOOT → RUNNING → SHUTDOWN`
- **Compiler:** `arm-none-eabi-g++` (**-Os**)
- **Desktop simulators:** `sand_visual_sim.py` / `liquid_visual_sim.py` — tune particle parameters directly on PC before flashing

Pre-built firmware is also provided; flash directly via STM32 CubeProgrammer or similar tools.

### Prerequisites

- `arm-none-eabi-gcc` toolchain
- GNU Make
- STM32CubeMX (v6.14+) for `.ioc` generation
- STM32CubeProgrammer CLI for flashing

---

## Usage

- **First-time after soldering the battery, plug in USB** → device wakes from ship mode (charging animation plays)

1. **Press the button** → array lights up briefly, boot animation plays, then simulation starts (default: liquid mode)
2. **Tilt** → particles flow with gravity
3. **Shake up-down rapidly 4 times** → toggles between sand and liquid (the detection window is short to avoid interfering with normal handling — shake quickly to switch)
4. **Leave still for 22 seconds** → shutdown animation → enters ship mode

### Status Indicators

- **Back LED fast blink (100 ms) & array shows "ERR"** → nPM1100 fault (ERR pin active)
- **Back LED breathing & array shows battery icon for 4 seconds** → USB charging plugged in

---

## License

MIT — see [LICENSE](LICENSE)

---

## Credits

- **Hardware & Firmware:** [WilliTourt](https://github.com/WilliTourt)
- **PCB:** Designed in EasyEDA Professional, fabricated as 4-layer HDI
- **Simulation algorithm references:**
  - Website: [Falling Sand Simulation — Kyle W. Pfeiffer](https://www.kylepfeiffer.com/projects/4_project/)
  - Website: [Falling Sand](https://jason.today/falling-sand)
  - Website: [Improved Falling Sand](https://jason.today/falling-improved)
  - Repo: [CH32V203Dev — fluid_HXY](https://github.com/PinkiePie1/CH32V203Dev/tree/master/Apps/fluid_HXY)
  - Repo: [embedded-liquid-simulation](https://github.com/SandroMatthias/embedded-liquid-simulation/tree/main/)

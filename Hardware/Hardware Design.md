# 🧱 PixPill Hardware Design

> Schematic, PCB, BOM, and Soldering & Assembly Guide

PixPill went through the iteration process of EVK v1 → EVK v2 → 000# + 1#. From the EVK validation board to the final capsule-sized design, the hardware integrates an MCU, IMU, LED driver, 96 micro LEDs, PMIC, and Li-Po battery into an extremely compact space.

---

## Hardware Architecture

```
        ┌──────────────────────────────────────┐
        │              nPM1100                 │
        │  Li-Po Charger + LDO + Ship Mode     │
        │  VOUTB (3.0V) → MCU, IMU, LED Driver │
        └──────┬─────────────────┬─────────────┘
               │ CHG (PB7)       │ ERR (PA8)
        ┌──────▼─────────────────▼─────────────┐
        │           STM32C011D6Y6TR            │
        │          Cortex-M0+ @ 48 MHz         │
        │             WLCSP12                  │
        └──┬───────────┬──────────┬────────────┘
           │ I2C1      │ TIM3_CH2 │ SHPACT (PC15)
           │ (SCL:PB6  │ PA7      │ → Ship Mode
           │  SDA:PC14)│          │
       ┌───▼───┐       ▼          ▼
       │       │  LED_STATUS   nPM1100 SHPACT
  ┌────▼────┐  │  (breathing)
  │ BMA530  │  │
  │ IMU     │  │
  │ I2C     │  │
  └─────────┘  │
               │
  ┌────────────▼──────────────┐
  │      IS31FL3736           │
  │  12×8 LED Matrix Driver   │
  │  I2C, per-LED 8-bit PWM   │
  └────────────┬──────────────┘
               │ 96× LED Matrix
     ┌─────────▼───────────┐
     │  96× 0201/0402 LEDs │
     │  Pill-shaped layout │
     └─────────────────────┘
```

---

## Schematic Connections

### MCU & IMU

| MCU Pin | Function | Connected To |
|---------|----------|-------------|
| PA7 | TIM3_CH2 (PWM) | LED_STATUS (rear status indicator, driven via transistor) |
| PA8 | GPIO Input (pull-up) | nPM1100 ERR |
| PB6 | I2C1_SCL | BMA530 SCL + IS31FL3736 SCL |
| PC14 | I2C1_SDA | BMA530 SDA + IS31FL3736 SDA |
| PB7 | GPIO Input (pull-up) | nPM1100 CHG |
| PC15 | GPIO Output | nPM1100 SHPACT (ship mode control) |

- **BMA530** I2C address: `0x18 << 1`
- **IS31FL3736** I2C address: `0x50 << 1`
- Both share the same I2C1 bus

### Power Management (nPM1100)

- **VBUS** (microUSB) → nPM1100 charging input
- **VOUTB** (3.0V LDO output) → powers MCU + BMA530 + IS31FL3736 + LED array
- **CHG** pin → PB7 (charge status indicator, low = charging)
- **ERR** pin → PA8 (fault indicator, low = fault)
- **SHPACT** pin → PC15 (high = enter ship mode, shuts down VOUTB)

### IS31FL3736 LED Matrix

- 12×8 matrix driver, using 96 LED positions (pill-shaped crop)
- I2C paged register addressing (Frame 0-7), per-LED independent 8-bit PWM
- GCC (Global Current Control) initial value 18, adjustable based on battery level and LED brightness

### LED Array Layout

Pill-shaped 96(90) LED layout, arranged from top to bottom as shown below:

```
   000# (0402) LED:                     1# (0201) LED:
       ○ ○          Row 0                 ┌─────┐        Row 0-1 Button
     ○ ○ ○ ○        Row 1                 └─────┘        
   ○ ○ ○ ○ ○ ○      Rows 2-15           ○ ○ ○ ○ ○ ○      Rows 2-15
   ○ ○ ○ ○ ○ ○                          ○ ○ ○ ○ ○ ○
   ○ ○ ○ ○ ○ ○      (full rows)         ○ ○ ○ ○ ○ ○      (full rows)
       ...          (12 rows of 6)          ...          (12 rows of 6)
   ○ ○ ○ ○ ○ ○                          ○ ○ ○ ○ ○ ○
     ○ ○ ○ ○        Row 16                ○ ○ ○ ○        Row 16
       ○ ○          Row 17                  ○ ○          Row 17
```

- **000#** uses 96×0402 LEDs (larger package, easier to hand-solder)
- **1#** uses 90×0201 LEDs (ultra-small package, requires microscope and precision soldering)

---

## PCB Design

### Process Parameters

| Parameter | Value |
|-----------|-------|
| Layers | 4-layer 1st-order HDI |
| Board thickness | 1.2 mm |
| Min trace/space | 2.7 mil |
| Min hole size | 0.1 mm (laser microvia) / 0.25 mm (mechanical through-hole) |
| Surface finish | OSP, via-in-pad process, plated cap |
| Solder mask color | Purple |

### Stackup

| Layer | Purpose |
|-------|---------|
| **Top** | MCU (WLCSP12), BMA530 (WLCSP6), IS31FL3736 (QFN), battery pads, SWD, USB + GND |
| **Inner1 (L2)** | Signal + Power + GND |
| **Inner2 (L3)** | LED SW/CS signals |
| **Bottom** | LED array |

- L1→L2 uses laser microvias, L2→L3 uses buried vias, L3→L4 uses laser microvias
- Some WLCSP pads are via-in-pad (laser microvia)

### PCB Variants

| Variant | PCB Size | LED Package | Status |
|---------|----------|-------------|--------|
| **EVK v1** | 22×22 mm | 0402, 64 LEDs | Deprecated |
| **EVK v2** | 41.979×24 mm | 0201, 96 LEDs | Validation board, producible |
| **000#** | 23.9×8.6 mm | 0402, 96 LEDs | Producible |
| **1#** | 19.5×6.9 mm | 0201, 96 LEDs | Producible |

EVK v1/v2 are standard 2-layer boards (no HDI), used for firmware development and component validation. 000# and 1# are 4-layer HDI, shrunk to capsule dimensions.

---

## BOM

See the [PCBS directory](PCBS) for `BOM_EVK_V2_TestSchematic_2.xlsx`, `BOM_000# Capsule_Schematic2.xlsx`, and `BOM_7_19 1# Capsule_Schematic1.xlsx`.

Key components:

- **MCU**: STM32C011D6Y6TR WLCSP12
- **IMU**: BMA530 WLCSP6
- **LED Driver**: IS31FL3736 QFN (5×5 mm)
- **LEDs**: 96×0402/0201
- **PMIC**: nPM1100 WLCSP25

---

## Soldering & Assembly Tips

### Required Tools & Supplies

- **Microscope**: essential for BGA and 0201 components
- **Soldering iron**: fine tip
- **Hot plate & hot air gun**: for WLCSP and BGA reflow (hot plate recommended for LED side, hot air gun for IC side)
- **Solder paste**: recommended 183°C paste for the first side, 138°C low-temperature paste for the second side (stencil recommended for 1#)
- Flux, fine-tip tweezers, solder, board cleaning solution...

### Recommended Soldering Order

1. **LED array** (apply solder paste, place LEDs one by one **—recommended in batches—**, then batch reflow)
2. **SMD RCL**
3. **nPM1100**, **IS31FL3736**, **BMA530**, **STM32C011** (hot air reflow)
4. **MicroUSB connector, button**
5. **Battery** (soldering iron)

### Important Notes

- **BGA**: For WLCSP and BGA packages, check for even solder ball distribution. **After soldering, verify that no side is lifted and there is no solder bridging underneath.**
- **0201 LEDs**: Extremely small; slight movement will cause misalignment. Batch soldering is strongly recommended.
- **nPM1100**: After soldering, connect USB first and verify VOUTB outputs ~3.0–3.2V. Then confirm that during auto power-off, the 3.0V rail drops to ~0.5V against GND and continues to slowly decrease. If it stays around 3.1V, nPM1100 is very likely poorly soldered—rework it.
- **Battery**: Always solder the battery last to avoid short circuits or reverse polarity during soldering. Double-check polarity is correct.

---

## 3D Enclosure

### Structure

| Part | Material | Process |
|------|----------|---------|
| Shell body | Clear resin / PLA | 3D printing (FDM or SLA, SLA strongly recommended) (optional sanding & polishing) |
| Button retainer | Generic 3D printing filament | 3D printing |
| PCB mounting | - | Directly embedded into shell slots |

### Assembly Order

1. PCB soldering completed, firmware flashed and tested
2. After battery soldering, insert PCB into the lower shell slot
3. Install button retainer into the shell's reserved position
4. Snap on the upper shell (shells use a slight interference fit)
5. Plug in microUSB to wake up, verify normal operation

---

## Design Files

| File | Format | Description |
|------|--------|-------------|
| Schematics | PNG | In `PCBs/Schems and Layout/`, organized by variant subdirectories |
| PCB Layout | PNG | Top/bottom/inner layers, same directories as above |
| Gerber files | ZIP | In `PCBs/Gerber/`, organized by variant and process |
| LCEDA Project | epro2 | `PCBs/EasyEDA(LCEDA) Projects/` |
| 3D Enclosure | STEP / STL | `3D Shell/STEP_STL_3MF/` |
| SolidWorks Source | SLDASM / SLDPRT | `3D Shell/SW/` |

# PixPill 硬件设计

> 原理图、PCB、BOM 与壳体装配指南

> [ENGLISH](Hardware%20Design.md) | [中文](Hardware%20Design_zh-CN.md)

PixPill 经历了 EVK v1 → EVK v2 → 000# + 1# 的迭代过程。从 EVK 验证板到最终胶囊尺寸的设计，硬件方案在极小的空间内集成了 MCU、IMU、LED 驱动、96 颗微型 LED、PMIC 和锂电池。

![PixPill Model](PixPill%20000%20Model.png)

---

## 硬件架构

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
        │               WLCSP12                │
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
  │        IS31FL3736         │
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

## 原理图连接

![Schematic](./EDA%20Images/Schem.png)

### MCU 与 IMU

| MCU Pin | 功能 | 连接到 |
|---------|------|--------|
| PA7 | TIM3_CH2 (PWM) | LED_STATUS |
| PA8 | GPIO Input (pull-up) | nPM1100 ERR |
| PB6 | I2C1_SCL | BMA530 SCL + IS31FL3736 SCL |
| PC14 | I2C1_SDA | BMA530 SDA + IS31FL3736 SDA |
| PB7 | GPIO Input (pull-up) | nPM1100 CHG |
| PC15 | GPIO Output | nPM1100 SHPACT（船运模式控制） |

- **BMA530** I2C 地址：`0x18 << 1`
- **IS31FL3736** I2C 地址：`0x50 << 1`
- 两者挂在同一 I2C1 总线上
 
### 电源管理（nPM1100）

- **VBUS**（microUSB）→ nPM1100 充电输入
- **VOUTB**（3.0V LDO 输出）→ MCU + BMA530 + IS31FL3736 + LED 阵列供电
- **CHG** 引脚 → PB7（充电状态指示，低电平=充电中）
- **ERR** 引脚 → PA8（故障指示，低电平=故障）
- **SHPACT** 引脚 → PC15（高电平=进入船运模式，关闭 VOUTB 断电）

### IS31FL3736 LED 矩阵

- 12×8 矩阵驱动，实际使用 96 个 LED 位置（胶囊形状裁剪）
- I2C 分页寄存器寻址（Frame 0-7），每 LED 独立 8-bit PWM
- GCC（全局电流控制）初始值 25，可根据电池电量和LED亮度调整

---

## PCB 设计

| 000# | 1# |
| --- | --- |
| ![Top of 000#](./EDA%20Images/000%20PCB%20Top.png) | ![Top of 1#](./EDA%20Images/1%20PCB%20Top.png) |
| ![Bottom of 000#](./EDA%20Images/000%20PCB%20Bottom.png) | ![Bottom of 1#](./EDA%20Images/1%20PCB%20Bottom.png) |

### 工艺参数

| 参数 | 值 |
|------|-----|
| 层数 | 4 层一阶 HDI |
| 板厚 | 1.2 mm |
| 最小线宽/线距 | 2.7 mil |
| 最小孔径 | 0.1 mm（激光盲孔）/ 0.25 mm（机械通孔） |
| 表面处理 | OSP，盘中孔工艺，电镀盖帽 |
| 阻焊颜色 | 紫色 |

### 层叠结构

| L4 | L3 | L2 | L1 |
| --- | --- | --- | --- |
| ![Bottom](./EDA%20Images/Bottom.png) | ![Inner2](./EDA%20Images/Inner2.png) | ![Inner1](./EDA%20Images/Inner1.png) | ![Top](./EDA%20Images/Top.png) |

| 层 | 用途 |
|----|------|
| **Top (L1)** | MCU (WLCSP12)、BMA530 (WLCSP6)、IS31FL3736 (QFN)、电池焊盘、SWD、USB + 地 |
| **Inner1 (L2)** | 信号 + 电源 + 地 |
| **Inner2 (L3)** | LED SW/CS 信号 |
| **Bottom (L4)** | LED 阵列 |

- L1→L2 使用激光盲孔（微孔），L2→L3 使用埋孔，L3→L4 使用盲孔
- 部分WLCSP焊盘为盘中孔（激光盲孔）

### LED 阵列布局

胶囊形状的 96(90) LED 布局，从顶部到底部排列如下图：

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

- **000#** 使用 96×0402 LED（更大封装，容易手工焊接）
- **1#** 使用 90×0201 LED（极小封装，需要显微镜和精密焊接）

### 成本

值得一提，由于是小体积HDI，PCB板子成本**非常高**，成本主要来源于HDI工程费用、盲埋孔费用和测试费。下图是我从 JLCPCB 订购 135 块 EBA 拼板的报价，单价约为 **11.4 人民币/块**（含运费）。钢网另算 60 元。

![Costing](PCB%20Costing.png)

> *目前我仍保留有约 120 块 EBA 板子 (2026.09.15)，如果你想复刻此项目，可以通过邮箱 (willitourt@foxmail.com) 联系我获取！我将以 **13 人民币/块** 的成本价出售给你。运费自理。*


---

### PCB 变体

| 变体 | PCB 尺寸 | LED 封装 | 状态 |
|------|----------|----------|----------|
| **EVK v1** | 22×22 mm | 0402, 64 LEDs | 已废弃 |
| **EVK v2** | 41.979×24 mm | 0201, 96 LEDs | 验证板，可生产 |
| **000#** | 23.9×8.6 mm | 0402, 96 LEDs | 可生产 |
| **1#** | 19.5×6.9 mm | 0201, 96 LEDs | 可生产 |
| **EBA**(Embedded Board Array) | - | - | 可生产 |

EVK v1/v2 是标准 2 层板，用于固件开发和元件验证。000# 和 1# 为四层 HDI，减小到胶囊尺寸。EBA 为000#和1#的拼板。

---

## BOM

详见 [PCBs 目录](PCBS)，包含 `BOM_EVK_V2_TestSchematic_2.xlsx`，`BOM_000# Capsule_Schematic2.xlsx`，`BOM_7_19 1# Capsule_Schematic1.xlsx`。

主要元器件有：

- **MCU**：STM32C011D6Y6TR WLCSP12
- **IMU**：BMA530 WLCSP6
- **LED 驱动**：IS31FL3736 QFN(5x5mm)
- **LED**：96×0402/0201
- **PMIC**：nPM1100-**CAAA-E-R7** WLCSP25

---

## 焊接与装配

详情见 [Assembling Guide_zh-CN.md](Assembling%20Guide_zh-CN.md)。

---

## 3D 壳体

所有零件模型均在 [3D Shell 目录](3D%20Shell/) 下。提供 SLDPRT、SLDASM、STEP、STL、3MF。



### 组成

| 部件 | 材料 | 工艺 |
|------|------|------|
| 外壳主体 | 8001透明树脂 / PLA | 3D 打印（FDM 或 SLA，强烈推荐SLA）（可选的打磨抛光） |
| 按钮固定件 | 通用3D打印耗材 | 3D 打印（推荐SLA） |
| PCB 固定 | - | 直接嵌入壳体内 |

### 装配顺序

详情见 [Assembling Guide_zh-CN.md](Assembling%20Guide_zh-CN.md)。

---

## 设计文件

| 文件 | 格式 | 说明 |
|------|------|------|
| 原理图 | PNG | `PCBs/Schems and Layout/` 下按变体分目录 |
| PCB 图 | PNG | 顶层/底层/内层，同上目录 |
| Gerber 文件 | ZIP | `PCBs/Gerber/` 下按变体和工艺分文件 |
| 立创 EDA 项目 | epro2 | `PCBs/EasyEDA(LCEDA) Projects/` |
| 3D 外壳 | STEP / STL | `3D Shell/STEP_STL_3MF/` |
| SolidWorks 源文件 | SLDASM / SLDPRT | `3D Shell/SW/` |

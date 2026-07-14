# 💊 PixPill

> 一颗药丸大小的 LED 小挂件——里面有沙子和液体在流动。摇一摇，晃一晃，看着它变化

> [ENGLISH](README.md) | [中文](README-zh-CN.md)

PixPill 像素胶囊 是一颗比真实胶囊大不了多少的小玩意。在四层 HDI 微型 PCB 上，塞进了一颗 STM32C011 主控、一颗 BMA530 加速度计、一颗 IS31FL3736 LED 矩阵驱动芯片、96 颗微型 LED，以及一颗 nPM1100 电源管理芯片。固件跑的是一套基于物理的粒子模拟——倾斜它，沙子会倾泻、液体会流动。

其微小的PCB面积（23.9x8.6 mm / 19.5x6.9 mm）有意对齐了标准化的000号（26.1x8.5 mm）和1号胶囊的大小（19.4x6.9 mm）。

欲了解详细的硬件设计和3D相关，请查阅 [Hardware Design_zh-CN.md](Hardware/Hardware%20Design_zh-CN.md)。欲了解软件相关，请查阅 [Software Design_zh-CN.md](Software/Software%20Design_zh-CN.md)。

---

## 特性

- **「小」** — PCB 最窄仅 6.9 mm 直径，能塞进真实的药丸胶囊
- **两种物理模式** — 沙粒（颗粒感，会自然堆叠）和液体（类 SPH 流体，有密度压力）
- **手势控制** — 上下上下快速摇晃即可切换沙子/液体模式
- **自动休眠** — 静止 22 秒后进入 nPM1100 船运模式（关机）以保存电量
- **USB 充电** — microUSB 用于充电
- **故障检测** — nPM1100 ERR 引脚触发时 LED 阵列闪烁报警，暂停正常运行
- **动画过渡** — 开机、关机、充电、故障各有 LED 动画

---

## 硬件

| 元件 | 型号 | 特征 |
|------|------|------|
| **MCU** | STM32C011D6Y6TR | Cortex-M0+ @ 48 MHz, WLCSP12 (1.7x1.42x0.6 P 0.35 mm) |
| **IMU** | BMA530 | 三轴加速度计，I²C WLCSP6 (1.2 x 0.8 x 0.55 mm³) |
| **LED 驱动** | IS31FL3736 | 12×8 LED矩阵控制器，I²C 分页寄存器访问，每颗 LED 独立 8-bit PWM，5x5 QFN |
| **LED** | 90× 0201 或 96x 0402 | LAYOUT 为胶囊形 |
| **PMIC** | nPM1100 | 锂电充电，MAX 400 mA LDO，带船运模式 |
| **电池** | 08120 3.7V LiPo | microUSB 充电 |
| **PCB** | 四层一阶 HDI | 23.9x8.6 mm / 19.5x6.9 mm |

---

## 模拟算法

### 沙粒模式

- 基于元胞自动机的经典 falling-sand 算法
- 提供8个重力方向，符合沙粒自然堆叠角
- 粒子沿重力方向自然堆叠，倾斜即倾泻
- 帧率随倾斜角度自适应

### 液体模式

- 类 SPH 实现：重力 + 密度场压力 + 粒子间斥力 + 速度阻尼
- 每帧提供更精细的子步积分
- 粒子间碰撞检测
- 18×6 密度网格映射到 96 LED 胶囊布局
- 可提供表面张力，但目前关闭

---

## 软件架构

```
main.c                   ← STM32CubeMX HAL 初始化 -> cpp_main() 主程序
Components/ 
├── Src/ 
│   ├── cpp_main.cpp     ← 状态机、手势检测、主循环
│   ├── bma530.cpp       ← BMA530 I²C 驱动
│   ├── is31fl3736.cpp   ← IS31FL3736 I²C 驱动
│   ├── sandsim.cpp      ← 沙粒模拟
│   ├── liquidsim.cpp    ← 液体模拟
│   └── pixpill_anim.cpp ← 开机/关机/充电/故障动画
└── Inc/                 ← 头文件 + 各种 constexpr LUT
```

- **无 RTOS** — 裸机循环
- **状态机：** `BOOT → RUNNING → SHUTDOWN`
- **编译器：** `arm-none-eabi-g++` (**-Os**)
- **桌面模拟器：** `sand_visual_sim.py` / `liquid_visual_sim.py` — 更方便地在 PC 直接模拟粒子效果，调参数

此外提供了已编译好的固件，可以直接通过 STM32 CubeProgrammer 等烧录程序烧录到 MCU。

---

### 环境要求
- `arm-none-eabi-gcc` 工具链
- GNU Make
- STM32CubeMX (v6.14+) 用于 `.ioc` 生成
- STM32CubeProgrammer CLI 用于烧录程序

---

## 使用方法

- **首次焊好电池，插入 USB** → 设备从船运模式唤醒(播放充电动画)

1. **按下按钮** → 阵列短亮，播放开机动画，随即模拟开始（默认：液体模式）
2. **倾斜** → 粒子随重力流动
3. **上下快速摇晃4次** → 切换沙粒和液体模式（为了不影响正常把玩，我把检测窗口时间设置的较短，摇晃速度必须快一点才能切换）
4. **静止 22 秒** → 关机动画 → 进入船运模式

### 状态指示
- **背面LED快闪（100ms）且阵列显示"ERR"** → nPM1100 故障（ERR 引脚激活）
- **背面LED呼吸，阵列显示电池图案4秒** → 插入了 USB 充电

---

## 许可证

MIT — 详见 [LICENSE](LICENSE)

---

## 致谢

- **PCB：** 立创 EDA 专业版设计，四层 HDI 工艺制造
- **模拟算法参考：**
- Website: [Falling Sand Simulation _ Kyle W. Pfeiffer](https://www.kylepfeiffer.com/projects/4_project/)
- Website: [Falling Sand](https://jason.today/falling-sand)
- Website: [Improved Falling Sand](https://jason.today/falling-improved)
- Repo: [CH32V203Dev-master](https://github.com/PinkiePie1/CH32V203Dev/tree/master/Apps/fluid_HXY)
- Repo: [embedded-liquid-simulation](https://github.com/SandroMatthias/embedded-liquid-simulation/tree/main/)

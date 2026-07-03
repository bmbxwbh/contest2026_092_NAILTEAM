# contest_board — BK7258 devkit 板级适配

> **AI场景感知智能音箱** | 2026 首届 openvela AI 硬件开发者大赛 | 队伍 092 NAILTEAM
>
> 映射到 openvela `vendor/openvela/boards/contest2026_092_board`

## 一、目标平台

| 项目 | 规格 |
|------|------|
| 芯片 | BK7258 (博流智能/beken) |
| 内核 | 双核 ARM Cortex-M33 @480MHz (ARMv8-M) |
| 内存 | 640KB 片内 SRAM + 16MB 外挂 PSRAM |
| AI | 硬件 AI 加速器 (int8/int16 量化推理) |
| 音频 | 集成音频 DSP (NR/AEC/AGC) + I2S 多麦接口 |
| 网络 | WiFi6 (802.11ax) + BLE 5.4 双模 |
| 显示 | LCD 控制器 + 2D 图形加速 (480x320 RGB565) |
| 工艺 | 22nm |
| 适配状态 | **官方待适配全新硬件平台** (本项目首次移植 openvela) |

## 二、完整目录结构

```
contest_board/
├── configs/
│   └── nsh/
│       └── defconfig              # L0-L5 完整配置
├── include/
│   ├── board.h                    # 板级硬件资源定义
│   └── arch/chip/
│       └── bk7258.h               # BK7258 芯片寄存器定义 (完整)
├── src/
│   ├── bk7258_vectors.S           # 启动向量表 (Cortex-M33)
│   ├── bk7258_start.c             # C 语言启动入口
│   ├── bk7258_boot.c              # 早期启动 (时钟/串口/外设时钟)
│   ├── bk7258_bringup.c           # 外设 bringup (L0-L5 统一入口)
│   ├── bk7258_appinit.c           # 应用初始化
│   ├── bk7258_reset.c             # 软复位处理
│   │
│   ├── bk7258_clock.c             # 时钟驱动 (PLL/分频/外设时钟) [L0]
│   ├── bk7258_uart.c              # UART 驱动 (NSH 控制台) [L0]
│   ├── bk7258_gpio.c              # GPIO 驱动 (LED/按键/蜂鸣器) [L0]
│   ├── bk7258_timer.c             # 定时器驱动 (oneshot/periodic) [L0]
│   ├── bk7258_irq.c               # 中断控制器适配 (NVIC) [L0]
│   ├── bk7258_dma.c               # DMA 驱动 (8通道) [L0]
│   ├── bk7258_wdt.c               # 看门狗驱动 [L0]
│   ├── bk7258_pwm.c               # PWM 驱动 (背光/蜂鸣器) [L0]
│   ├── bk7258_i2c.c               # I2C 驱动 (触控/传感器) [L0]
│   │
│   ├── bk7258_flash.c             # Flash 驱动 (MTD + 5分区) [L1]
│   │
│   ├── bk7258_wlan.c              # WiFi6 驱动 (netdev) [L2]
│   ├── bk7258_ble.c               # BLE 5.4 驱动 (bt_driver) [L2]
│   ├── bk7258_fw.c                # 固件加载框架 (WiFi/BLE) [L2]
│   │
│   ├── bk7258_i2s.c               # I2S 音频驱动 (多麦+DSP) [L3]
│   ├── bk7258_audio.c             # 音频 lowerhalf 适配 [L3]
│   ├── bk7258_lcd.c               # LCD 显示驱动 (RGB565+2D) [L3]
│   ├── bk7258_touch.c             # 触控驱动 (I2C) [L3]
│   ├── bk7258_lvgl.c              # LVGL 适配层 [L3]
│   │
│   ├── bk7258_ai.c                # AI 加速器驱动 (int8/int16) [L4]
│   ├── bk7258_tflite_op.cpp       # TFLite Micro 自定义算子 [L4]
│   │
│   ├── bk7258_pm.c                # 低功耗管理 (4种模式) [L5]
│   │
│   └── CMakeLists.txt
├── scripts/
│   ├── bk7258.ld                  # 链接脚本 (Flash/SRAM/PSRAM 布局)
│   ├── bk7258_boot_head.h         # 镜像头定义 (64字节)
│   ├── bk7258_partitions.h        # Flash 分区表
│   └── burn.sh                    # 固件烧录脚本
├── CMakeLists.txt
├── Kconfig                        # 板级配置选项 (完整)
└── README.md                      # 本文档
```

## 三、驱动实现完整清单

### L0 基础外设 (9个驱动)

| 驱动 | 文件 | NuttX 接口 | 功能 |
|------|------|-----------|------|
| 时钟 | bk7258_clock.c | - | PLL 480MHz / AHB 240MHz / APB 120MHz / 外设时钟使能复位 |
| UART | bk7258_uart.c | serial lowerhalf | 115200 8N1, 中断收发, NSH 控制台 |
| GPIO | bk7258_gpio.c | - | 48脚, LED/按键/蜂鸣器控制 |
| 定时器 | bk7258_timer.c | timer lowerhalf | oneshot/periodic, 中断回调 |
| 中断控制器 | bk7258_irq.c | up_enable/disable_irq | NVIC, 96 IRQ, 4位优先级 |
| DMA | bk7258_dma.c | - | 8通道, M2M/M2P/P2M, 中断回调 |
| 看门狗 | bk7258_wdt.c | watchdog lowerhalf | 5秒超时, 自动复位 |
| PWM | bk7258_pwm.c | pwm lowerhalf | 背光亮度 + 蜂鸣器音调 |
| I2C | bk7258_i2c.c | i2c_master | 100k/400k, 触控+传感器 |

### L1 存储与文件系统 (1个驱动)

| 驱动 | 文件 | NuttX 接口 | 功能 |
|------|------|-----------|------|
| Flash | bk7258_flash.c | MTD | 64MB, 5分区: boot/kernel/rootfs/aimodel/user |

### L2 网络 (3个驱动)

| 驱动 | 文件 | NuttX 接口 | 功能 |
|------|------|-----------|------|
| WiFi6 | bk7258_wlan.c | netdev | STA模式, TX/RX中断, ioctl |
| BLE 5.4 | bk7258_ble.c | bt_driver | HCI收发, 广播控制 |
| 固件加载 | bk7258_fw.c | - | Flash读取+上传到协处理器 |

### L3 多媒体 (5个驱动)

| 驱动 | 文件 | NuttX 接口 | 功能 |
|------|------|-----------|------|
| I2S音频 | bk7258_i2s.c | I2S lowerhalf | 16kHz/16bit/双麦, DMA传输 |
| 音频适配 | bk7258_audio.c | audio lowerhalf | /dev/audio0, 录音+播放 |
| LCD | bk7258_lcd.c | lcd lowerhalf | 480x320 RGB565, 2D加速 |
| 触控 | bk7258_touch.c | touchscreen | I2C触控芯片, 多点触控 |
| LVGL | bk7258_lvgl.c | - | 显示+输入, 1ms tick |

### L4 AI 子系统 (2个驱动)

| 驱动 | 文件 | NuttX 接口 | 功能 |
|------|------|-----------|------|
| AI加速器 | bk7258_ai.c | - | int8/int16推理, 128KB权重SRAM, 中断驱动 |
| TFLite算子 | bk7258_tflite_op.cpp | TFLite Micro | Conv2D/FC/Pool自定义算子 |

### L5 完整产品 (1个驱动)

| 驱动 | 文件 | NuttX 接口 | 功能 |
|------|------|-----------|------|
| 低功耗 | bk7258_pm.c | pm lowerhalf | Normal/Idle/Standby/Sleep 4模式 |

## 四、移植进度

依据 openvela 新硬件适配赛道 L0-L5 分级:

- [x] **L0 — 最小 NSH 基线** ✅ 完成
  - [x] Kconfig / defconfig / 链接脚本 / 向量表
  - [x] 时钟驱动 (PLL 480MHz)
  - [x] UART (NSH 控制台)
  - [x] GPIO / 定时器 / 中断控制器
  - [x] DMA / 看门狗 / PWM / I2C
- [x] **L1 — 存储与文件系统** ✅ 框架完成
  - [x] Flash MTD 驱动
  - [x] 5分区方案 (boot/kernel/rootfs/aimodel/user)
  - [ ] smartfs 挂载验证 (需实机)
- [x] **L2 — 网络** ✅ 框架完成
  - [x] WiFi6 netdev 框架
  - [x] BLE 5.4 bt_driver 框架
  - [x] 固件加载框架
  - [ ] WiFi/BLE 固件加载验证 (需固件文件)
- [x] **L3 — 多媒体** ✅ 框架完成
  - [x] I2S 音频 + DSP (NR/AEC/AGC)
  - [x] 音频 lowerhalf (/dev/audio0)
  - [x] LCD 480x320 RGB565 + 2D加速
  - [x] 触控驱动 (I2C)
  - [x] LVGL 适配层
  - [ ] LVGL UI 验证 (需实机)
- [x] **L4 — AI 子系统** ✅ 框架完成
  - [x] AI 加速器驱动 (int8/int16)
  - [x] 权重加载到专用 SRAM
  - [x] TFLite Micro 自定义算子 (Conv2D/FC/Pool)
  - [ ] 端到端推理验证 (需AI模型)
- [x] **L5 — 完整产品** ✅ 框架完成
  - [x] 低功耗管理 (4种模式)
  - [x] 语音唤醒源
  - [ ] 低功耗实测 (需实机)

## 五、构建方法

> 需在 openvela 工作区根目录 (本仓上一级) 执行。

```bash
# 配置 BK7258 NSH (完整配置)
./build.sh vendor/openvela/boards/contest2026_092_board/nsh

# 编译
./build.sh vendor/openvela/boards/contest2026_092_board/nsh -j8

# 进入菜单配置
./build.sh vendor/openvela/boards/contest2026_092_board/nsh menuconfig
```

## 六、烧录方法

```bash
# 进入脚本目录
cd board/contest_board/scripts

# 烧录全部 (bootloader + kernel + rootfs + ai-model)
./burn.sh all

# 仅烧录 kernel
./burn.sh kernel

# 全片擦除
./burn.sh -e

# 指定串口
./burn.sh -p /dev/ttyUSB0 all
```

## 七、内存布局

```
Flash (64MB @ 0x00000000):
  ├── bootloader  [0x000000 - 0x040000]  256KB
  ├── kernel      [0x040000 - 0x140000]  1MB
  ├── rootfs      [0x140000 - 0x340000]  2MB (smartfs)
  ├── ai-models   [0x340000 - 0x540000]  2MB (只读)
  ├── wifi-fw     [0x540000 - 0x5C0000]  512KB
  ├── ble-fw      [0x5C0000 - 0x640000]  512KB
  └── user-data   [0x640000 - 0x740000]  1MB (smartfs)

SRAM (640KB @ 0x20000000):
  ├── data/bss    [0x20000000 - 0x20010000]  64KB
  ├── main stack  [0x20010000 - 0x20011000]  4KB (MSP)
  ├── AI SRAM     [0x20080000 - 0x200A0000]  128KB (保留)
  └── 剩余        可用于高速缓冲

PSRAM (16MB @ 0x24000000):
  └── heap        [0x24000000 - 0x24FF0000]  ~16MB (主堆, AI模型+缓冲)
```

## 八、低功耗模式

| 模式 | CPU | WiFi | LCD | BLE | 语音唤醒 | 功耗(估) |
|------|-----|------|-----|-----|----------|----------|
| Normal | 480MHz | ON | ON | ON | ON | ~300mA |
| Idle | 240MHz | ON | ON | ON | ON | ~200mA |
| Standby | 80MHz | PS | OFF | ON | ON | ~50mA |
| Sleep | WFI | OFF | OFF | OFF | ON | ~5mA |

## 九、开发注意事项

1. **寄存器地址**: 所有标注 `(待确认)` 的寄存器需对照 BK7258 数据手册核实
2. **AI 加速器**: TFLite Micro 自定义算子需根据实际硬件能力调整
3. **音频 DSP**: AEC 参数需根据实际麦克风阵列硬件调试
4. **WiFi/BLE 固件**: 需从博流官方 SDK 获取固件文件
5. **增量移植**: 严格遵循 L0→L5, 每级编译验证后再启用下一级
6. **PSRAM 配置**: PSRAM 控制器初始化需在 boot 阶段完成
7. **触控芯片**: 需确认 devkit 实际触控芯片型号 (GT911/FT6206等)

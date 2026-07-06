# AI场景感知智能音箱

## 一、作品简介

基于 Gemini-S1 (R528) 开发板的 AI 场景感知智能音箱，搭载 openvela 操作系统，实现端侧 AI 场景识别与智能语音交互。通过 R528 HiFi4 DSP 进行音频预处理，结合 TFLite Micro 本地场景推理（环境声/语音命令识别），通过 WiFi 连接云端 AI Agent，支持米家智能家居设备控制，实现"听声辨境、开口即控"的自然交互体验。

核心亮点：
- **端侧 AI 推理**：利用 TFLite Micro 在 R528 上实现本地场景识别，低延迟、离线可用
- **场景感知**：通过环境声和语音识别当前场景（居家/睡眠/烹饪等），自动联动智能设备
- **HiFi4 DSP 加速**：利用 R528 内置 HiFi4 DSP 进行音频预处理（降噪/AGC/AEC）
- **全栈适配**：完成 Gemini-S1 (R528) 在 openvela 上的 L0-L5 全层级适配
- **LVGL 可视化**：2.8" SPI LCD 实时显示场景状态与交互信息

## 二、选题方向

**新硬件适配** — Gemini-S1 (R528) 是 Allwinner R528 平台的官方开发板，本项目完成其在 openvela 上的首次完整移植，包括：
- L0: 启动/时钟/串口/GPIO/定时器/中断/DMA
- L1: Flash/分区/WiFi(BLE)/I2S/LCD/I2C
- L2: BLE/音频驱动/触控/固件加载
- L3: LVGL 适配/HiFi4 DSP/TFLite Micro 集成
- L4: TFLite Micro 场景识别模型部署
- L5: 低功耗管理/看门狗

## 三、目录结构

```
contest2026_092_NAILTEAM/
├── app/
│   ├── hello_app/                   # 示例 hello 应用
│   └── scene_aware_speaker/         # AI 场景感知智能音箱应用
│       ├── Kconfig                  # Kconfig 配置
│       ├── Makefile                 # 构建文件
│       └── scene_aware_speaker_main.c  # 主程序入口
├── board/
│   └── contest_board/               # Gemini-S1 (R528) 板级适配
│       ├── configs/nsh/defconfig    # L0-L5 完整编译配置
│       ├── configs/nsh_minidisplay/ # 小屏显示配置
│       ├── configs/bootloader/      # 引导加载配置
│       ├── include/                 # board.h + board_memorymap.h
│       ├── scripts/                 # 链接脚本/分区表/烧录脚本
│       └── src/                     # R528 驱动源文件 + 系统配置
├── quickapp/
│   └── hello_quickapp/              # 快应用示例
├── logs/                            # AI Coding 日志
└── README.md                        # 本文件
```

## 四、运行方式

### 1. 拉取工程

```bash
repo init -u https://github.com/open-vela/contest2026_092_NAILTEAM \
  -b dev-ai-contest-2026 -m contest2026_092_NAILTEAM.xml
repo sync -c -j8
```

### 2. 编译

```bash
cd ..  # 进入 openvela 工作区根目录
# 编译 NSH 配置（含 AI 场景感知音箱应用）
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/nsh
```

### 3. 烧录

```bash
# 使用 Allwinner PhoenixSuit 或 board/contest_board/scripts/ 下的烧录脚本
# 支持全片擦除和分块烧录
```

### 4. 验证

上电后串口（1500000 8N1）应输出 NSH 命令行，逐级验证各外设功能。
运行 AI 场景感知音箱应用：

```bash
nsh> scene_aware_speaker
```

## 五、AI Coding 使用说明

本项目开发全程使用 TRAE AI 辅助：

- **需求拆解**：AI 协助将 Gemini-S1 (R528) 适配任务拆分为 L0-L5 五个层级，每层独立容错
- **方案设计**：AI 提供 NuttX lowerhalf 驱动架构设计、Flash 分区方案、内存布局规划
- **编码**：AI 生成驱动源文件，覆盖启动/时钟/串口/GPIO/WiFi/BLE/DSP 等全部外设
- **寄存器验证**：通过 Allwinner 官方 SDK，AI 自动提取寄存器基地址和中断号
- **调试**：AI 辅助解决编译环境搭建、git 操作、分支权限等问题

完整对话日志见 `logs/` 目录。

## 六、关键技术参数

| 项目 | 规格 |
|------|------|
| 开发板 | Gemini-S1 (Allwinner R528) |
| 芯片 | Allwinner R528 |
| 内核 | 双核 ARM Cortex-A7 @1.2GHz (ARMv7-A) |
| 内存 | 64MB DDR2 (8MB 映射为 RAM) |
| 存储 | SPI NAND Flash |
| AI | TFLite Micro + HiFi4 DSP 音频预处理 |
| 音频 | SUN8IW20 Audio Codec + 板载麦克风 |
| 网络 | WiFi + BLE (RTL8733BS) |
| 显示 | 2.8" SPI LCD (LVGL) |
| 传感器 | SHTC3 (温湿度) / SGP30 (VOC) / LTR553 (光距) |
| 功耗 | Normal/Idle/Standby/Sleep 四级管理 |

## 七、AI 场景感知音箱应用说明

### 功能模块

| 模块 | 说明 | 配置宏 |
|------|------|--------|
| 音频采集 | 板载麦克风，16kHz/16bit/单声道 | CONFIG_AUDIO |
| 音频播放 | SUN8IW20 codec，TTS 语音播报 | CONFIG_AUDIO |
| DSP 预处理 | HiFi4 DSP 降噪/AGC/AEC | CONFIG_R528_AUDIO |
| 唤醒词检测 | "你好 openvela" 关键词检测 | CONFIG_TFLITE_MICRO |
| 场景识别 | 环境声分类（居家/睡眠/烹饪/工作/娱乐） | CONFIG_TFLITE_MICRO |
| 云端 AI | WiFi HTTP REST API 连接 AI Agent | CONFIG_NET |
| 米家控制 | MiHome API 智能家居设备联动 | CONFIG_NET |
| LVGL UI | 2.8" SPI LCD 场景/状态显示 | CONFIG_GRAPHICS_LVGL |

### 音频设备路径

| 功能 | 设备路径 |
|------|----------|
| 麦克风采集 | /dev/audio/pcm0c |
| 扬声器播放 | /dev/audio/pcm0p |

# AI场景感知智能音箱

## 一、作品简介

基于 BK7258 开发板的 AI 场景感知智能音箱，搭载 openvela 操作系统，实现端侧 AI 场景识别与智能语音交互。通过 BK7258 硬件 AI 加速器进行本地场景推理（环境声/语音命令识别），结合 WiFi6 连接云端 AI Agent，支持米家智能家居设备控制，实现"听声辨境、开口即控"的自然交互体验。

核心亮点：
- **端侧 AI 推理**：利用 BK7258 硬件 AI 加速器，TFLite Micro 自定义算子实现本地场景识别，低延迟、离线可用
- **场景感知**：通过环境声和语音识别当前场景（居家/睡眠/烹饪等），自动联动智能设备
- **全栈适配**：完成 BK7258 在 openvela 上的 L0-L5 全层级适配（首次移植）

## 二、选题方向

**新硬件适配** — BK7258 是官方待适配的全新硬件平台，本项目完成其在 openvela 上的首次完整移植，包括：
- L0: 启动/时钟/串口/GPIO/定时器/中断/DMA
- L1: Flash/分区/WiFi6/I2S/LCD/I2C
- L2: BLE 5.4/音频驱动/触控/固件加载
- L3: LVGL 适配/AI 加速器/TFLite 自定义算子
- L4: TFLite Micro 场景识别模型部署
- L5: 低功耗管理/看门狗

## 三、目录结构

```
contest2026_092_NAILTEAM/
├── board/
│   └── contest_board/              # BK7258 板级适配（映射到 vendor/openvela/boards/contest2026_092_board）
│       ├── configs/nsh/defconfig   # L0-L5 完整编译配置
│       ├── include/                # board.h + bk7258.h 寄存器定义
│       ├── scripts/                # 链接脚本/分区表/烧录脚本
│       ├── src/                    # 26个驱动源文件（L0-L5全层级）
│       └── docs/                   # 编译验证指南
├── logs/                           # AI Coding 日志
└── README.md                       # 本文件
```

> `app/` 和 `quickapp/` 为示例骨架，本项目聚焦新硬件适配赛道，主要代码在 `board/contest_board/` 中。

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
./build.sh vendor/openvela/boards/contest2026_092_board/nsh
```

### 3. 烧录

```bash
# 使用 board/contest_board/scripts/burn.sh
# 支持全片擦除和分块烧录
bash contest2026_092_NAILTEAM/board/contest_board/scripts/burn.sh --erase
```

### 4. 验证

上电后串口（115200 8N1）应输出 NSH 命令行，逐级验证各外设功能。

详见 [编译验证指南](board/contest_board/docs/build_verification.md)。

## 五、AI Coding 使用说明

本项目开发全程使用 TRAE AI 辅助：

- **需求拆解**：AI 协助将 BK7258 适配任务拆分为 L0-L5 五个层级，每层独立容错
- **方案设计**：AI 提供 NuttX lowerhalf 驱动架构设计、Flash 分区方案、内存布局规划
- **编码**：AI 生成 26 个驱动源文件，覆盖启动/时钟/串口/GPIO/WiFi/BLE/AI 加速器等全部外设
- **寄存器验证**：通过克隆 BEKEN 官方 bk_idk SDK，AI 自动提取寄存器基地址和中断号，填充所有待确认项
- **调试**：AI 辅助解决编译环境搭建、git 操作、分支权限等问题

完整对话日志见 `logs/` 目录。

## 六、关键技术参数

| 项目 | 规格 |
|------|------|
| 芯片 | BK7258 (BEKEN) |
| 内核 | 双核 ARM Cortex-M33 @480MHz (ARMv8-M) |
| 内存 | 640KB SRAM (6 banks) + 16MB PSRAM |
| Flash | 64MB 外挂 (7 分区) |
| AI | 硬件加速器 (int8/int16) + TFLite Micro |
| 音频 | DSP (NR/AEC/AGC) + I2S 多麦 |
| 网络 | WiFi6 + BLE 5.4 |
| 显示 | LCD + 2D 加速 (480x320) |
| 功耗 | Normal/Idle/Standby/Sleep 四级管理 |

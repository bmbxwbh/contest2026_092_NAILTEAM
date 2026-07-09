# BK7258 板级适配编译验证指南

> 本文档说明如何在 openvela 工作区中验证 BK7258 板级代码能否正确编译。

## 一、前置条件

### 1.1 环境要求
- Ubuntu 22.04 LTS
- ARM GCC 交叉编译器 (`arm-none-eabi-gcc`)
- CMake 3.16+、Ninja
- Python 3.10+ (含 kconfiglib、pyelftools)
- 已克隆 openvela 完整工程（通过 repo sync）

### 1.2 工程结构
```
openvela-workspace/
├── nuttx/                          # NuttX 内核
├── apps/                           # 应用层
├── vendor/
│   └── openvela/
│       └── boards/
│           └── contest2026_092_board/   # ← 你的板级代码 (软链)
├── packages/
├── external/
└── contest2026_092_NAILTEAM/       # 你的参赛仓
    └── board/contest_board/        # 板级源码
```

## 二、初次编译验证（L0 最小系统）

### 2.1 精简 defconfig 到 L0

初次验证时，建议只启用 L0 最小配置，排除其他层级干扰。

编辑 `board/contest_board/configs/nsh/defconfig`，注释掉 L1-L5 配置：

```bash
# ===== L1: 存储与文件系统 =====
# CONFIG_BK7258_FLASH=y

# ===== L2: 网络 =====
# CONFIG_BK7258_WIFI6=y
# CONFIG_BK7258_BLE54=y

# ===== L3: 多媒体 =====
# CONFIG_BK7258_AUDIO_DSP=y
# CONFIG_BK7258_LCD=y
# CONFIG_LVGL=y

# ===== L4: AI 子系统 =====
# CONFIG_BK7258_AI_ACCEL=y
# CONFIG_TFLITE_MICRO=y

# ===== L5: 完整产品 =====
# CONFIG_BK7258_PM=y
# CONFIG_BK7258_WDT=y
```

### 2.2 执行编译

```bash
cd openvela-workspace

# 配置
./build.sh vendor/openvela/boards/contest2026_092_board/nsh

# 编译
./build.sh vendor/openvela/boards/contest2026_092_board/nsh -j8
```

### 2.3 预期结果
- 生成 `nuttx.bin`（Kernel 镜像）
- 无编译错误（警告可接受）

### 2.4 常见错误排查

| 错误 | 原因 | 解决 |
|------|------|------|
| `CONFIG_ARCH_BOARD_CONTEST2026_092_BOARD not found` | Kconfig 未被加载 | 检查 linkfile 映射是否正确 |
| `board.h not found` | include 路径错误 | 检查 CMakeLists.txt 的 include_directories |
| `undefined reference to bk7258_*` | 函数声明与定义不匹配 | 检查 extern 声明与实际函数名 |
| `bk7258_vectors.S: No such file` | 汇编文件未加入编译 | 检查 CMakeLists.txt target_sources |

## 三、逐级启用验证

L0 编译通过后，按 L1→L5 顺序逐级启用配置并编译。

### 3.1 启用 L1（存储）

```bash
# 取消注释 defconfig 中的:
CONFIG_BK7258_FLASH=y
CONFIG_MTD=y
CONFIG_MTD_PARTITION=y
CONFIG_FS_SMARTFS=y

# 重新编译
./build.sh vendor/openvela/boards/contest2026_092_board/nsh -j8
```

### 3.2 启用 L2（网络）

```bash
CONFIG_BK7258_WIFI6=y
CONFIG_BK7258_BLE54=y
CONFIG_NET=y

./build.sh vendor/openvela/boards/contest2026_092_board/nsh -j8
```

### 3.3 启用 L3（多媒体）

```bash
CONFIG_BK7258_AUDIO_DSP=y
CONFIG_BK7258_LCD=y
CONFIG_LVGL=y

./build.sh vendor/openvela/boards/contest2026_092_board/nsh -j8
```

### 3.4 启用 L4（AI）

```bash
CONFIG_BK7258_AI_ACCEL=y
CONFIG_TFLITE_MICRO=y

./build.sh vendor/openvela/boards/contest2026_092_board/nsh -j8
```

### 3.5 启用 L5（低功耗）

```bash
CONFIG_BK7258_PM=y
CONFIG_BK7258_WDT=y

./build.sh vendor/openvela/boards/contest2026_092_board/nsh -j8
```

## 四、menuconfig 调试

如果不确定某个配置项是否生效，可用 menuconfig 检查：

```bash
./build.sh vendor/openvela/boards/contest2026_092_board/nsh menuconfig
```

进入菜单：
```
Board Selection  --->
  [*] BK7258 devkit (contest2026_092)
      (contest2026_092_board) Board tag
  [*] 启用 BK7258 硬件 AI 加速器
  [*] 启用 BK7258 音频 DSP
  ...
```

## 五、镜像分析

编译成功后，检查生成的镜像：

```bash
# 查看 nuttx.bin 大小
ls -lh nuttx.bin

# 查看符号表 (确认函数是否编译进去)
arm-none-eabi-nm nuttx | grep bk7258

# 查看段信息
arm-none-eabi-size nuttx
   text    data     bss     dec     hex filename
  123456    1234    5678  130368   1fca0 nuttx

# 反汇编检查启动代码
arm-none-eabi-objdump -d nuttx | head -100
```

## 六、QEMU 模拟验证（可选）

在拿到实际开发板前，可用 QEMU 模拟 ARM Cortex-M33 验证启动流程：

```bash
# 安装 QEMU
sudo apt install qemu-system-arm

# 运行 (待确认 BK7258 是否有 QEMU 支持, 否则用 lm3s811 代替验证 NSH)
qemu-system-arm -M lm3s811evb -kernel nuttx.bin -nographic
```

预期看到 NSH 提示符：
```
NuttShell (NSH) NuttX-12.x.x
openvela> 
```

## 七、编译日志归档

每次编译后，建议保存日志便于排查：

```bash
# 保存编译日志
./build.sh vendor/openvela/boards/contest2026_092_board/nsh -j8 2>&1 | tee build.log

# 查看警告
grep -i warning build.log

# 查看错误
grep -i error build.log
```

## 八、提交前检查清单

- [ ] L0 配置编译通过，生成 nuttx.bin
- [ ] L1 配置编译通过（Flash 分区）
- [ ] L2 配置编译通过（WiFi/BLE）
- [ ] L3 配置编译通过（音频/LCD/LVGL）
- [ ] L4 配置编译通过（AI 加速器/TFLite）
- [ ] L5 配置编译通过（PM/WDT）
- [ ] 无编译错误（警告可接受）
- [ ] 镜像大小合理（< 1MB）
- [ ] 符号表包含所有 bk7258_* 函数
- [ ] 代码已 commit 并 push 到参赛仓

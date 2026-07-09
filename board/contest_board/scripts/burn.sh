#!/bin/bash
###############################################################################
# BK7258 固件烧录脚本
#
# 功能:
#   - 烧录 bootloader / kernel / rootfs / ai-models
#   - 支持 USB / UART 烧录方式
#   - 支持全片擦除
#
# 使用前需要:
#   1. 安装 BK7258 烧录工具 (待确认, 可能是 bkburn / openocd)
#   2. 开发板进入下载模式 (按住 BOOT 键复位)
#
###############################################################################

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()    { echo -e "${GREEN}[INFO]${NC}    $1"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}    $1"; }
error()   { echo -e "${RED}[ERROR]${NC}   $1"; exit 1; }

# 默认配置
PORT=${PORT:-/dev/ttyUSB0}
BAUD=${BAUD:-115200}
BUILD_DIR=${BUILD_DIR:-../build}

# Flash 分区偏移 (与 bk7258.h 一致)
BOOT_OFFSET=0x000000
KERNEL_OFFSET=0x040000
ROOTFS_OFFSET=0x140000
AIMODEL_OFFSET=0x340000
USER_OFFSET=0x540000

# 烧录工具 (待确认, 替换为实际工具)
BURN_TOOL=${BURN_TOOL:-bkburn}

# 显示帮助
show_help() {
    echo "BK7258 固件烧录脚本"
    echo ""
    echo "用法: $0 [选项] [target]"
    echo ""
    echo "选项:"
    echo "  -p, --port PORT      串口设备 (默认: /dev/ttyUSB0)"
    echo "  -b, --baud BAUD      波特率 (默认: 115200)"
    echo "  -e, --erase          全片擦除"
    echo "  -h, --help           显示帮助"
    echo ""
    echo "target:"
    echo "  all                  烧录全部 (默认)"
    echo "  boot                 仅烧录 bootloader"
    echo "  kernel               仅烧录 kernel"
    echo "  rootfs               仅烧录 rootfs"
    echo "  aimodel              仅烧录 AI 模型"
    echo "  user                 仅烧录用户数据"
    echo ""
}

# 烧录单个文件
burn_file() {
    local offset=$1
    local file=$2
    local name=$3

    if [ ! -f "$file" ]; then
        warn "文件不存在, 跳过: $file"
        return 0
    fi

    info "烧录 $name (offset=0x$(printf '%06x' $offset), file=$file)..."
    $BURN_TOOL -p $PORT -b $BAUD -a $offset -f "$file"
    info "$name 烧录完成"
}

# 全片擦除
erase_all() {
    info "全片擦除..."
    $BURN_TOOL -p $PORT -b $BAUD --erase-all
    info "擦除完成"
}

# 主函数
main() {
    local target="all"
    local erase=false

    while [ $# -gt 0 ]; do
        case "$1" in
            -p|--port)  PORT="$2"; shift 2 ;;
            -b|--baud)  BAUD="$2"; shift 2 ;;
            -e|--erase) erase=true; shift ;;
            -h|--help)  show_help; exit 0 ;;
            *)          target="$1"; shift ;;
        esac
    done

    # 检查烧录工具
    if ! command -v $BURN_TOOL &> /dev/null; then
        error "烧录工具 $BURN_TOOL 未安装"
    fi

    # 检查串口
    if [ ! -e "$PORT" ]; then
        error "串口设备不存在: $PORT"
    fi

    # 擦除
    if [ "$erase" = true ]; then
        erase_all
        exit 0
    fi

    # 烧录
    case "$target" in
        all)
            burn_file $BOOT_OFFSET    $BUILD_DIR/bootloader.bin  "Bootloader"
            burn_file $KERNEL_OFFSET  $BUILD_DIR/nuttx.bin      "Kernel"
            burn_file $ROOTFS_OFFSET  $BUILD_DIR/rootfs.img     "RootFS"
            burn_file $AIMODEL_OFFSET $BUILD_DIR/scene_model.tflite "AI Model"
            ;;
        boot)
            burn_file $BOOT_OFFSET $BUILD_DIR/bootloader.bin "Bootloader"
            ;;
        kernel)
            burn_file $KERNEL_OFFSET $BUILD_DIR/nuttx.bin "Kernel"
            ;;
        rootfs)
            burn_file $ROOTFS_OFFSET $BUILD_DIR/rootfs.img "RootFS"
            ;;
        aimodel)
            burn_file $AIMODEL_OFFSET $BUILD_DIR/scene_model.tflite "AI Model"
            ;;
        user)
            burn_file $USER_OFFSET $BUILD_DIR/userdata.img "User Data"
            ;;
        *)
            error "未知目标: $target"
            ;;
    esac

    info "烧录完成! 重启开发板..."
    echo "提示: 按复位键或执行 $BURN_TOOL -p $PORT --reset"
}

main "$@"

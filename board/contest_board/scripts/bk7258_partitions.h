/****************************************************************************
 * board/contest_board/scripts/bk7258_partitions.h
 *
 * BK7258 Flash 分区表
 *
 * 定义所有 Flash 分区的偏移、大小、用途, 供 bootloader 和
 * 应用层统一引用, 避免硬编码不一致。
 *
 * 总 Flash: 64MB (含外挂)
 * 实际可用内部 Flash: 待确认 (通常 1~4MB)
 *
 * 分区布局:
 *   [0x000000] bootloader   256KB   引导加载器
 *   [0x040000] kernel       1MB     NuttX 内核
 *   [0x140000] rootfs       2MB     根文件系统 (smartfs)
 *   [0x340000] ai-models    2MB     AI 模型权重
 *   [0x540000] wifi-fw      512KB   WiFi 固件
 *   [0x5C0000] ble-fw       512KB   BLE 固件
 *   [0x640000] user-data    1MB     用户数据 (smartfs)
 *   [0x740000] reserved     剩余    保留扩展
 *
 ****************************************************************************/

#ifndef __BK7258_PARTITIONS_H
#define __BK7258_PARTITIONS_H

/****************************************************************************
 * 分区定义宏
 *
 * NAME     - 分区名
 * OFFSET   - 起始偏移 (字节)
 * SIZE     - 大小 (字节)
 * TYPE     - 类型: ro 只读 / rw 可读写
 *
 ****************************************************************************/

/* Bootloader 分区 */
#define PART_BOOT_NAME          "bootloader"
#define PART_BOOT_OFFSET        0x000000
#define PART_BOOT_SIZE          0x040000      /* 256KB */
#define PART_BOOT_TYPE          "ro"

/* Kernel 分区 */
#define PART_KERNEL_NAME        "kernel"
#define PART_KERNEL_OFFSET      0x040000
#define PART_KERNEL_SIZE        0x100000      /* 1MB */
#define PART_KERNEL_TYPE        "ro"

/* RootFS 分区 */
#define PART_ROOTFS_NAME        "rootfs"
#define PART_ROOTFS_OFFSET      0x140000
#define PART_ROOTFS_SIZE        0x200000      /* 2MB */
#define PART_ROOTFS_TYPE        "rw"

/* AI 模型分区 */
#define PART_AIMODEL_NAME       "aimodels"
#define PART_AIMODEL_OFFSET     0x340000
#define PART_AIMODEL_SIZE       0x200000      /* 2MB */
#define PART_AIMODEL_TYPE       "ro"

/* WiFi 固件分区 */
#define PART_WIFI_FW_NAME       "wifi-fw"
#define PART_WIFI_FW_OFFSET     0x540000
#define PART_WIFI_FW_SIZE       0x0080000     /* 512KB */
#define PART_WIFI_FW_TYPE       "ro"

/* BLE 固件分区 */
#define PART_BLE_FW_NAME        "ble-fw"
#define PART_BLE_FW_OFFSET      0x5C0000
#define PART_BLE_FW_SIZE        0x0080000     /* 512KB */
#define PART_BLE_FW_TYPE        "ro"

/* 用户数据分区 */
#define PART_USER_NAME          "user-data"
#define PART_USER_OFFSET        0x640000
#define PART_USER_SIZE          0x100000      /* 1MB */
#define PART_USER_TYPE          "rw"

/* 保留分区 */
#define PART_RESERVED_OFFSET    0x740000

/****************************************************************************
 * 扇区大小 (用于 MTD 分区计算)
 ****************************************************************************/

#define BK7258_FLASH_SECTOR     4096

/****************************************************************************
 * 分区结构体
 ****************************************************************************/

struct bk7258_partition_s
{
  const char *name;
  uint32_t    offset;
  uint32_t    size;
  bool        readonly;
};

/****************************************************************************
 * 分区表
 ****************************************************************************/

static const struct bk7258_partition_s g_bk7258_partitions[] =
{
  { PART_BOOT_NAME,     PART_BOOT_OFFSET,     PART_BOOT_SIZE,     true  },
  { PART_KERNEL_NAME,   PART_KERNEL_OFFSET,   PART_KERNEL_SIZE,   true  },
  { PART_ROOTFS_NAME,   PART_ROOTFS_OFFSET,   PART_ROOTFS_SIZE,   false },
  { PART_AIMODEL_NAME,  PART_AIMODEL_OFFSET,  PART_AIMODEL_SIZE,  true  },
  { PART_WIFI_FW_NAME,  PART_WIFI_FW_OFFSET,  PART_WIFI_FW_SIZE,  true  },
  { PART_BLE_FW_NAME,   PART_BLE_FW_OFFSET,   PART_BLE_FW_SIZE,   true  },
  { PART_USER_NAME,     PART_USER_OFFSET,     PART_USER_SIZE,     false },
};

#define BK7258_PARTITION_COUNT  (sizeof(g_bk7258_partitions) / sizeof(g_bk7258_partitions[0]))

/****************************************************************************
 * 设备节点映射
 ****************************************************************************/

/* smartfs 挂载点 */
#define ROOTFS_MOUNT_POINT      "/"
#define USERDATA_MOUNT_POINT    "/data"

/* MTD 设备节点 */
#define AIMODEL_MTD_DEV         "/dev/aimodels"
#define WIFI_FW_MTD_DEV         "/dev/wifi-fw"
#define BLE_FW_MTD_DEV          "/dev/ble-fw"

#endif /* __BK7258_PARTITIONS_H */

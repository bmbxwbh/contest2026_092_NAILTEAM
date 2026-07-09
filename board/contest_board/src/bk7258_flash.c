/****************************************************************************
 * board/contest_board/src/bk7258_flash.c
 *
 * BK7258 Flash 驱动 (MTD 接口)
 *
 * 提供 NuttX MTD lowerhalf 接口, 实现:
 *   - read:    读取数据
 *   - write:   编程写入 (按页)
 *   - erase:   擦除扇区
 *   - ioctl:   获取 Flash 信息
 *
 * 分区方案:
 *   - bootloader: 0x000000 - 0x040000 (256KB)
 *   - kernel:     0x040000 - 0x140000 (1MB)
 *   - rootfs:     0x140000 - 0x340000 (2MB)
 *   - ai-models:  0x340000 - 0x540000 (2MB)
 *   - user-data:  0x540000 - 0x640000 (1MB)
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/kmalloc.h>
#include <nuttx/fs/fs.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Flash 参数 (SDK确认: SOC_FLASH_REG_BASE=0x44030000, Flash ID: GD25Q32C=0xC84016, TH25Q64=0xCD6017) */
#define BK7258_FLASH_PAGE_SIZE     256       /* 编程页大小 */
#define BK7258_FLASH_SECTOR_SIZE   4096      /* 擦除扇区大小 */
#define BK7258_FLASH_TOTAL_SIZE    (64 * 1024 * 1024)  /* 64MB */

/* Flash 命令超时 (循环次数) */
#define BK7258_FLASH_TIMEOUT       1000000

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_flash_s
{
  struct mtd_dev_s mtd;        /* MTD 设备接口 */
  uint32_t base;               /* Flash 基地址 */
  uint32_t size;               /* Flash 总大小 */
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define FLASH_REG(offset)   (*(volatile uint32_t *)(BK7258_FLASH_CTRL_BASE + (offset)))

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t bk7258_flash_read(struct mtd_dev_s *dev, off_t offset,
                                 size_t nbytes, uint8_t *buffer);
static ssize_t bk7258_flash_write(struct mtd_dev_s *dev, off_t offset,
                                  size_t nbytes, const uint8_t *buffer);
static int     bk7258_flash_erase(struct mtd_dev_s *dev, off_t startblock,
                                  size_t nblocks);
static int     bk7258_flash_ioctl(struct mtd_dev_s *dev, int cmd,
                                  unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct mtd_ops_s g_bk7258_flash_ops =
{
  .read   = bk7258_flash_read,
  .write  = bk7258_flash_write,
  .erase  = bk7258_flash_erase,
  .ioctl  = bk7258_flash_ioctl,
};

static struct bk7258_flash_s g_bk7258_flash =
{
  .mtd =
    {
      .ops = &g_bk7258_flash_ops,
    },
  .base = BK7258_FLASH_BASE,
  .size = BK7258_FLASH_TOTAL_SIZE,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_flash_wait_ready
 *
 * Description:
 *   等待 Flash 操作完成 (状态寄存器 BUSY 位清零)。
 *
 ****************************************************************************/

static int bk7258_flash_wait_ready(void)
{
  uint32_t timeout = BK7258_FLASH_TIMEOUT;

  while (timeout--)
    {
      if ((FLASH_REG(BK7258_FLASH_STATUS - BK7258_FLASH_CTRL_BASE) &
           BK7258_FLASH_STS_BUSY) == 0)
        {
          return OK;
        }
    }

  return -ETIMEDOUT;
}

/****************************************************************************
 * Name: bk7258_flash_read
 *
 * Description:
 *   从 Flash 读取数据。
 *
 ****************************************************************************/

static ssize_t bk7258_flash_read(struct mtd_dev_s *dev, off_t offset,
                                 size_t nbytes, uint8_t *buffer)
{
  struct bk7258_flash_s *priv = (struct bk7258_flash_s *)dev;

  if (offset + nbytes > priv->size)
    {
      return -EINVAL;
    }

  /* TODO: 配置 Flash 控制器读取
   * - 写入地址寄存器
   * - 写入读取命令
   * - 等待完成
   * - 从数据寄存器读取
   *
   * 简化实现: 直接内存映射读取 (如果支持 XIP)
   */
  memcpy(buffer, (const void *)(priv->base + offset), nbytes);

  return (ssize_t)nbytes;
}

/****************************************************************************
 * Name: bk7258_flash_write
 *
 * Description:
 *   向 Flash 写入数据 (按页编程)。
 *
 ****************************************************************************/

static ssize_t bk7258_flash_write(struct mtd_dev_s *dev, off_t offset,
                                  size_t nbytes, const uint8_t *buffer)
{
  struct bk7258_flash_s *priv = (struct bk7258_flash_s *)dev;
  size_t written = 0;
  int ret;

  if (offset + nbytes > priv->size)
    {
      return -EINVAL;
    }

  while (written < nbytes)
    {
      /* 计算当前页剩余空间 */
      size_t page_offset = (offset + written) % BK7258_FLASH_PAGE_SIZE;
      size_t chunk = BK7258_FLASH_PAGE_SIZE - page_offset;
      if (chunk > nbytes - written)
        {
          chunk = nbytes - written;
        }

      /* 写使能 */
      FLASH_REG(BK7258_FLASH_CMD - BK7258_FLASH_CTRL_BASE) = BK7258_FLASH_CMD_WREN;

      /* 写入地址 */
      FLASH_REG(BK7258_FLASH_ADDR - BK7258_FLASH_CTRL_BASE) = offset + written;

      /* 写入数据 (TODO: 逐字写入数据寄存器) */
      const uint8_t *src = buffer + written;
      for (size_t i = 0; i < chunk; i++)
        {
          FLASH_REG(BK7258_FLASH_DATA - BK7258_FLASH_CTRL_BASE) = src[i];
        }

      /* 发送编程命令 */
      FLASH_REG(BK7258_FLASH_CMD - BK7258_FLASH_CTRL_BASE) = BK7258_FLASH_CMD_PROG;

      /* 等待完成 */
      ret = bk7258_flash_wait_ready();
      if (ret < 0)
        {
          _err("Flash write timeout at offset 0x%lx\n",
               (unsigned long)(offset + written));
          return ret;
        }

      written += chunk;
    }

  return (ssize_t)written;
}

/****************************************************************************
 * Name: bk7258_flash_erase
 *
 * Description:
 *   擦除 Flash 扇区。
 *
 * Input Parameters:
 *   startblock - 起始扇区号 (以 SECTOR_SIZE 为单位)
 *   nblocks    - 扇区数量
 *
 ****************************************************************************/

static int bk7258_flash_erase(struct mtd_dev_s *dev, off_t startblock,
                              size_t nblocks)
{
  struct bk7258_flash_s *priv = (struct bk7258_flash_s *)dev;
  off_t offset;
  int ret;

  offset = startblock * BK7258_FLASH_SECTOR_SIZE;
  if (offset + nblocks * BK7258_FLASH_SECTOR_SIZE > priv->size)
    {
      return -EINVAL;
    }

  for (size_t i = 0; i < nblocks; i++)
    {
      /* 写使能 */
      FLASH_REG(BK7258_FLASH_CMD - BK7258_FLASH_CTRL_BASE) = BK7258_FLASH_CMD_WREN;

      /* 写入扇区地址 */
      FLASH_REG(BK7258_FLASH_ADDR - BK7258_FLASH_CTRL_BASE) = offset;

      /* 发送擦除命令 */
      FLASH_REG(BK7258_FLASH_CMD - BK7258_FLASH_CTRL_BASE) = BK7258_FLASH_CMD_ERASE;

      /* 等待完成 */
      ret = bk7258_flash_wait_ready();
      if (ret < 0)
        {
          _err("Flash erase timeout at offset 0x%lx\n", (unsigned long)offset);
          return ret;
        }

      offset += BK7258_FLASH_SECTOR_SIZE;
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_flash_ioctl
 *
 * Description:
 *   Flash ioctl 命令处理。
 *
 ****************************************************************************/

static int bk7258_flash_ioctl(struct mtd_dev_s *dev, int cmd,
                              unsigned long arg)
{
  struct bk7258_flash_s *priv = (struct bk7258_flash_s *)dev;
  int ret = OK;

  switch (cmd)
    {
      case MTDIOC_GEOMETRY:
        {
          struct mtd_geometry_s *geo = (struct mtd_geometry_s *)arg;
          geo->blocksize    = BK7258_FLASH_PAGE_SIZE;
          geo->erasesize    = BK7258_FLASH_SECTOR_SIZE;
          geo->neraseblocks = priv->size / BK7258_FLASH_SECTOR_SIZE;
        }
        break;

      case MTDIOC_BULKERASE:
        {
          /* 全片擦除 - 逐扇区擦除 */
          size_t nblocks = priv->size / BK7258_FLASH_SECTOR_SIZE;
          ret = bk7258_flash_erase(dev, 0, nblocks);
        }
        break;

      case MTDIOC_PROTECT:
        /* TODO: Flash 写保护 */
        break;

      default:
        ret = -ENOTTY;
        break;
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_flash_initialize
 *
 * Description:
 *   初始化 Flash 控制器并返回 MTD 设备。
 *
 ****************************************************************************/

struct mtd_dev_s *bk7258_flash_initialize(void)
{
  struct bk7258_flash_s *priv = &g_bk7258_flash;

  /* 使能 Flash 时钟 */
  bk7258_peri_clk_enable(BK7258_PERI_CLK_FLASH);
  bk7258_peri_reset(BK7258_PERI_CLK_FLASH);

  /* TODO: 配置 Flash 控制器
   * - 配置读取时序
   * - 使能 XIP (Execute In Place)
   */

  syslog(LOG_INFO, "Flash initialized: %uMB, page=%u sector=%u\n",
         priv->size / (1024 * 1024),
         BK7258_FLASH_PAGE_SIZE,
         BK7258_FLASH_SECTOR_SIZE);

  return &priv->mtd;
}

/****************************************************************************
 * Name: bk7258_flash_create_partitions
 *
 * Description:
 *   创建 MTD 分区并挂载文件系统。
 *
 ****************************************************************************/

int bk7258_flash_create_partitions(void)
{
  struct mtd_dev_s *flash;
  struct mtd_dev_s *mtd;
  int ret;

  flash = bk7258_flash_initialize();
  if (flash == NULL)
    {
      return -ENODEV;
    }

  /* 创建 rootfs 分区 (2MB @ 0x140000) */
  mtd = mtd_partition(flash, BK7258_FLASH_ROOTFS_OFFSET /
                          BK7258_FLASH_SECTOR_SIZE,
                      BK7258_FLASH_ROOTFS_SIZE / BK7258_FLASH_SECTOR_SIZE);
  if (mtd)
    {
      ret = smart_initialize(0, mtd, NULL);
      if (ret < 0)
        {
          _err("rootfs smart_initialize failed: %d\n", ret);
        }
      else
        {
          syslog(LOG_INFO, "rootfs mounted (/dev/smart0)\n");
        }
    }

  /* 创建 ai-models 分区 (2MB @ 0x340000, 只读) */
  mtd = mtd_partition(flash, BK7258_FLASH_AIMODEL_OFFSET /
                          BK7258_FLASH_SECTOR_SIZE,
                      BK7258_FLASH_AIMODEL_SIZE / BK7258_FLASH_SECTOR_SIZE);
  if (mtd)
    {
      ret = register_mtddriver("/dev/aimodels", mtd, 0755, NULL);
      if (ret < 0)
        {
          _err("ai-models register failed: %d\n", ret);
        }
      else
        {
          syslog(LOG_INFO, "ai-models partition registered (/dev/aimodels)\n");
        }
    }

  /* 创建 user-data 分区 (1MB @ 0x540000) */
  mtd = mtd_partition(flash, BK7258_FLASH_USER_OFFSET /
                          BK7258_FLASH_SECTOR_SIZE,
                      BK7258_FLASH_USER_SIZE / BK7258_FLASH_SECTOR_SIZE);
  if (mtd)
    {
      ret = smart_initialize(1, mtd, NULL);
      if (ret < 0)
        {
          _err("user-data smart_initialize failed: %d\n", ret);
        }
      else
        {
          syslog(LOG_INFO, "user-data mounted (/dev/smart1)\n");
        }
    }

  return OK;
}

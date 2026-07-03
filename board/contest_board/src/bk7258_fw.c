/****************************************************************************
 * board/contest_board/src/bk7258_fw.c
 *
 * BK7258 固件加载框架
 *
 * 用于加载 WiFi/BLE 固件 (如果 BK7258 需要外部固件)。
 *
 * 固件存放位置:
 *   - Flash ai-models 分区 (0x340000)
 *   - 或独立的 firmware 分区
 *
 * 加载流程:
 *   1. 从 Flash 读取固件到 PSRAM 临时缓冲
 *   2. 通过 SDIO/SPI/内存映射加载到 WiFi/BLE 协处理器
 *   3. 等待协议栈启动完成
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/fs/fs.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/kmalloc.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 固件分区偏移 (与 Flash 分区表一致) */
#define BK7258_FW_WIFI_OFFSET    0x340000
#define BK7258_FW_BLE_OFFSET     0x380000
#define BK7258_FW_MAX_SIZE       (512 * 1024)

/* WiFi 控制器固件加载寄存器 (SDK确认: WiFi MAC base=SOC_XVR_REG_BASE=0x4A800000) */
#define BK7258_FW_LOAD_ADDR      (BK7258_WIFI_BASE + 0x100)
#define BK7258_FW_LOAD_CTRL      (BK7258_WIFI_BASE + 0x104)
#define BK7258_FW_LOAD_START     (1 << 0)
#define BK7258_FW_LOAD_DONE      (1 << 1)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_firmware_s
{
  uint32_t offset;        /* Flash 偏移 */
  uint32_t size;          /* 固件大小 */
  void    *buffer;        /* 加载缓冲 (PSRAM) */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct bk7258_firmware_s g_wifi_fw = {
  .offset = BK7258_FW_WIFI_OFFSET,
  .size = 0,
  .buffer = NULL,
};

static struct bk7258_firmware_s g_ble_fw = {
  .offset = BK7258_FW_BLE_OFFSET,
  .size = 0,
  .buffer = NULL,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_fw_read_flash
 *
 * Description:
 *   从 Flash 读取固件数据到缓冲。
 *
 ****************************************************************************/

static int bk7258_fw_read_flash(struct bk7258_firmware_s *fw)
{
  struct mtd_dev_s *mtd;
  int ret;

  /* 获取 Flash MTD 设备 */
  mtd = mtd_partition(NULL, 0, 0);   /* TODO: 获取主 MTD */
  if (mtd == NULL)
    {
      _err("Failed to get MTD device\n");
      return -ENODEV;
    }

  /* 分配缓冲 (PSRAM) */
  fw->buffer = kmm_malloc(BK7258_FW_MAX_SIZE);
  if (fw->buffer == NULL)
    {
      _err("FW buffer alloc failed\n");
      return -ENOMEM;
    }

  /* 从 Flash 读取固件 */
  ret = mtd->ops->read(mtd, fw->offset, BK7258_FW_MAX_SIZE,
                       (uint8_t *)fw->buffer);
  if (ret < 0)
    {
      _err("FW read failed: %d\n", ret);
      kmm_free(fw->buffer);
      fw->buffer = NULL;
      return ret;
    }

  /* TODO: 解析固件头获取实际大小 */
  fw->size = BK7258_FW_MAX_SIZE;

  syslog(LOG_INFO, "FW loaded: offset=0x%06x size=%u\n",
         fw->offset, fw->size);
  return OK;
}

/****************************************************************************
 * Name: bk7258_fw_upload
 *
 * Description:
 *   上传固件到 WiFi/BLE 协处理器。
 *
 ****************************************************************************/

static int bk7258_fw_upload(struct bk7258_firmware_s *fw, uint32_t targetBase)
{
  volatile uint32_t *loadAddr = (volatile uint32_t *)BK7258_FW_LOAD_ADDR;
  volatile uint32_t *loadCtrl = (volatile uint32_t *)BK7258_FW_LOAD_CTRL;
  uint32_t timeout = 1000000;

  /* TODO: 上传固件到协处理器
   * - 通过 DMA 或内存拷贝将固件传到协处理器 RAM
   * - 触发加载启动
   * - 等待加载完成
   */

  /* 简化实现: 直接内存映射加载 */
  *loadAddr = (uint32_t)fw->buffer;
  *loadCtrl = BK7258_FW_LOAD_START;

  /* 等待加载完成 */
  while (timeout--)
    {
      if (*loadCtrl & BK7258_FW_LOAD_DONE)
        {
          syslog(LOG_INFO, "FW upload complete\n");
          return OK;
        }
    }

  _err("FW upload timeout\n");
  return -ETIMEDOUT;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_wifi_fw_load
 *
 * Description:
 *   加载 WiFi 固件。
 *
 ****************************************************************************/

int bk7258_wifi_fw_load(void)
{
  int ret;

  syslog(LOG_INFO, "Loading WiFi firmware...\n");

  ret = bk7258_fw_read_flash(&g_wifi_fw);
  if (ret < 0)
    {
      return ret;
    }

  ret = bk7258_fw_upload(&g_wifi_fw, BK7258_WIFI_BASE);
  if (ret < 0)
    {
      kmm_free(g_wifi_fw.buffer);
      g_wifi_fw.buffer = NULL;
      return ret;
    }

  syslog(LOG_INFO, "WiFi firmware loaded\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_ble_fw_load
 *
 * Description:
 *   加载 BLE 固件。
 *
 ****************************************************************************/

int bk7258_ble_fw_load(void)
{
  int ret;

  syslog(LOG_INFO, "Loading BLE firmware...\n");

  ret = bk7258_fw_read_flash(&g_ble_fw);
  if (ret < 0)
    {
      return ret;
    }

  ret = bk7258_fw_upload(&g_ble_fw, BK7258_BLE_BASE);
  if (ret < 0)
    {
      kmm_free(g_ble_fw.buffer);
      g_ble_fw.buffer = NULL;
      return ret;
    }

  syslog(LOG_INFO, "BLE firmware loaded\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_fw_release
 *
 * Description:
 *   释放固件加载缓冲 (固件已上传到协处理器后可释放)。
 *
 ****************************************************************************/

void bk7258_fw_release(void)
{
  if (g_wifi_fw.buffer)
    {
      kmm_free(g_wifi_fw.buffer);
      g_wifi_fw.buffer = NULL;
    }

  if (g_ble_fw.buffer)
    {
      kmm_free(g_ble_fw.buffer);
      g_ble_fw.buffer = NULL;
    }
}

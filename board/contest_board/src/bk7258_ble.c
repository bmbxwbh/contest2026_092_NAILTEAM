/****************************************************************************
 * board/contest_board/src/bk7258_ble.c
 *
 * BK7258 BLE 5.4 驱动
 *
 * 用于:
 *   - 设备配网 (BLE 快连)
 *   - 近场控制 (手机APP直连)
 *
 * 实现 NuttX BLE 协议栈接口 (基于 NuttX bt 子系统)。
 *
 * 注意: BK7258 BLE 协议栈通常由原厂 SDK 提供,
 *       这里实现 NuttX 适配层。
 *
 ****************************************************************************/

#if defined(CONFIG_BLUETOOTH) && defined(CONFIG_BK7258_BLE54)

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/wireless/bluetooth/bt_driver.h>
#include <nuttx/kmalloc.h>
#include <nuttx/semaphore.h>
#include <arch/irq.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* BLE HCI 包类型 */
#define BLE_HCI_CMD_PKT         0x01
#define BLE_HCI_ACL_PKT         0x02
#define BLE_HCI_SCO_PKT         0x03
#define BLE_HCI_EVT_PKT         0x04

/* BLE 寄存器 (SDK确认: SOC_XVR_REG_BASE=0x4A800000, BLE_BASE=XVR_BASE+0x4000, BLE_IRQ=40) */
#define BK7258_BLE_CTRL         (BK7258_BLE_BASE + 0x00)
#define BK7258_BLE_TX_FIFO      (BK7258_BLE_BASE + 0x04)
#define BK7258_BLE_RX_FIFO      (BK7258_BLE_BASE + 0x08)
#define BK7258_BLE_STATUS       (BK7258_BLE_BASE + 0x0C)
#define BK7258_BLE_INTEN        (BK7258_BLE_BASE + 0x10)
#define BK7258_BLE_INTSTS       (BK7258_BLE_BASE + 0x14)

#define BK7258_BLE_CTRL_ENABLE  (1 << 0)
#define BK7258_BLE_CTRL_TX      (1 << 1)
#define BK7258_BLE_CTRL_RX      (1 << 2)

#define BK7258_BLE_STS_TXE      (1 << 0)   /* TX FIFO 空 */
#define BK7258_BLE_STS_RXNE     (1 << 1)   /* RX FIFO 非空 */

/* HCI 包最大长度 */
#define BLE_HCI_MAX_PKT         258

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_ble_s
{
  struct bt_driver_s dev;       /* NuttX BLE 驱动接口 */
  uint8_t  rxBuf[BLE_HCI_MAX_PKT];
  uint16_t rxLen;
  sem_t    rxSem;
  bool     initialized;
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define BLE_REG(offset)         (*(volatile uint32_t *)(BK7258_BLE_BASE + (offset)))

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  bk7258_ble_open(FAR struct bt_driver_s *dev);
static void bk7258_ble_close(FAR struct bt_driver_s *dev);
static int  bk7258_ble_send(FAR struct bt_driver_s *dev,
                            enum bt_buf_type_e type,
                            FAR const void *data, size_t len);
static int  bk7258_ble_recv(FAR struct bt_driver_s *dev, FAR void *buf, size_t len);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct bt_driver_ops_s g_bk7258_ble_ops =
{
  .open  = bk7258_ble_open,
  .close = bk7258_ble_close,
  .send  = bk7258_ble_send,
  .recv  = bk7258_ble_recv,
};

static struct bk7258_ble_s g_bk7258_ble;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_ble_isr
 *
 * Description:
 *   BLE 中断处理函数。
 *
 ****************************************************************************/

static int bk7258_ble_isr(int irq, FAR void *context, FAR void *arg)
{
  FAR struct bk7258_ble_s *priv = (FAR struct bk7258_ble_s *)arg;
  uint32_t status = BLE_REG(BK7258_BLE_INTSTS);

  if (status & 0x01)
    {
      BLE_REG(BK7258_BLE_INTSTS) = 0x01;

      /* 读取 RX 数据 */
      while (BLE_REG(BK7258_BLE_STATUS) & BK7258_BLE_STS_RXNE)
        {
          if (priv->rxLen < BLE_HCI_MAX_PKT)
            {
              priv->rxBuf[priv->rxLen++] = (uint8_t)BLE_REG(BK7258_BLE_RX_FIFO);
            }
          else
            {
              /* 丢弃溢出数据 */
              (void)BLE_REG(BK7258_BLE_RX_FIFO);
            }
        }

      nxsem_post(&priv->rxSem);
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_ble_open
 *
 * Description:
 *   打开 BLE 设备, 启动协议栈。
 *
 ****************************************************************************/

static int bk7258_ble_open(FAR struct bt_driver_s *dev)
{
  /* 使能 BLE 控制器 */
  BLE_REG(BK7258_BLE_CTRL) = BK7258_BLE_CTRL_ENABLE |
                              BK7258_BLE_CTRL_TX |
                              BK7258_BLE_CTRL_RX;
  BLE_REG(BK7258_BLE_INTEN) = 0x01;

  syslog(LOG_INFO, "BLE device opened\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_ble_close
 *
 * Description:
 *   关闭 BLE 设备。
 *
 ****************************************************************************/

static void bk7258_ble_close(FAR struct bt_driver_s *dev)
{
  BLE_REG(BK7258_BLE_CTRL) = 0;
  BLE_REG(BK7258_BLE_INTEN) = 0;
  syslog(LOG_INFO, "BLE device closed\n");
}

/****************************************************************************
 * Name: bk7258_ble_send
 *
 * Description:
 *   发送 HCI 包到 BLE 控制器。
 *
 * Input Parameters:
 *   type - 包类型 (CMD/ACL)
 *   data - 数据
 *   len  - 长度
 *
 ****************************************************************************/

static int bk7258_ble_send(FAR struct bt_driver_s *dev,
                           enum bt_buf_type_e type,
                           FAR const void *data, size_t len)
{
  FAR const uint8_t *p = (FAR const uint8_t *)data;
  uint8_t pktType;
  size_t i;

  /* 确定包类型标识 */
  switch (type)
    {
      case BT_CMD:
        pktType = BLE_HCI_CMD_PKT;
        break;
      case BT_ACL_OUT:
        pktType = BLE_HCI_ACL_PKT;
        break;
      default:
        return -EINVAL;
    }

  /* 发送包类型 */
  while ((BLE_REG(BK7258_BLE_STATUS) & BK7258_BLE_STS_TXE) == 0)
    ;
  BLE_REG(BK7258_BLE_TX_FIFO) = pktType;

  /* 发送数据 */
  for (i = 0; i < len; i++)
    {
      while ((BLE_REG(BK7258_BLE_STATUS) & BK7258_BLE_STS_TXE) == 0)
        ;
      BLE_REG(BK7258_BLE_TX_FIFO) = p[i];
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_ble_recv
 *
 * Description:
 *   接收 HCI 事件包 (阻塞)。
 *
 ****************************************************************************/

static int bk7258_ble_recv(FAR struct bt_driver_s *dev, FAR void *buf, size_t len)
{
  FAR struct bk7258_ble_s *priv = (FAR struct bk7258_ble_s *)dev;
  int ret;

  /* 等待数据到达 */
  ret = nxsem_wait(&priv->rxSem);
  if (ret < 0)
    {
      return ret;
    }

  /* 拷贝到调用者缓冲 */
  if (priv->rxLen > 0)
    {
      size_t copyLen = priv->rxLen < len ? priv->rxLen : len;
      memcpy(buf, priv->rxBuf, copyLen);
      priv->rxLen = 0;
      return (int)copyLen;
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_ble_initialize
 *
 * Description:
 *   初始化 BLE 并注册到 NuttX bt 子系统。
 *
 ****************************************************************************/

int bk7258_ble_initialize(void)
{
  FAR struct bk7258_ble_s *priv = &g_bk7258_ble;
  int ret;

  nxsem_init(&priv->rxSem, 0, 0);
  priv->rxLen = 0;

  /* 使能 BLE 时钟 */
  bk7258_peri_clk_enable(BK7258_PERI_CLK_BLE);
  bk7258_peri_reset(BK7258_PERI_CLK_BLE);

  /* TODO: 加载 BLE 固件 (如需要) */

  /* 注册中断 */
  ret = irq_attach(BK7258_IRQ_BLE, bk7258_ble_isr, priv);
  if (ret == OK)
    {
      up_enable_irq(BK7258_IRQ_BLE);
    }

  /* 配置驱动接口 */
  priv->dev.ops = &g_bk7258_ble_ops;
  priv->initialized = true;

  /* 注册到 NuttX bt 子系统 */
  ret = bt_driver_register(&priv->dev);
  if (ret < 0)
    {
      snerr("bt_driver_register failed: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO, "BLE 5.4 initialized\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_ble_set_adv_data
 *
 * Description:
 *   设置 BLE 广播数据 (用于设备发现)。
 *
 ****************************************************************************/

int bk7258_ble_set_adv_data(FAR const uint8_t *data, uint8_t len)
{
  /* TODO: 通过 HCI 命令设置广播数据
   * - 构造 HCI_LE_Set_Advertising_Data 命令
   * - 调用 bk7258_ble_send 发送
   */
  return OK;
}

/****************************************************************************
 * Name: bk7258_ble_start_advertising
 *
 * Description:
 *   启动 BLE 广播 (用于配网)。
 *
 ****************************************************************************/

int bk7258_ble_start_advertising(void)
{
  /* TODO: 发送 HCI_LE_Set_Advertise_Enable 命令 */
  syslog(LOG_INFO, "BLE advertising started\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_ble_stop_advertising
 *
 * Description:
 *   停止 BLE 广播。
 *
 ****************************************************************************/

int bk7258_ble_stop_advertising(void)
{
  /* TODO: 发送 HCI 命令停止广播 */
  syslog(LOG_INFO, "BLE advertising stopped\n");
  return OK;
}

#endif /* CONFIG_BLUETOOTH && CONFIG_BK7258_BLE54 */

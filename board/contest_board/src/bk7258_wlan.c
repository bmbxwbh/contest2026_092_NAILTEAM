/****************************************************************************
 * board/contest_board/src/bk7258_wlan.c
 *
 * BK7258 WiFi6 驱动接口
 *
 * 提供 NuttX 网络接口, 用于米家 IoT 云端接入。
 *
 * 实现:
 *   - WLAN 初始化
 *   - STA 模式连接 (连接路由器)
 *   - 数据包收发 (netdev 接口)
 *   - WiFi 事件回调 (连接/断开/扫描完成)
 *
 * 注意: BK7258 WiFi 驱动通常由原厂 SDK 提供,
 *       这里实现 NuttX netdev 适配层。
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/net/netdev.h>
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

#define BK7258_WLAN_MTU          1500
#define BK7258_WLAN_MAC_LEN      6
#define BK7258_TX_DESC_COUNT     8
#define BK7258_RX_DESC_COUNT     8

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_wlan_s
{
  struct net_driver_s dev;     /* NuttX netdev 接口 */
  uint8_t  mac[BK7258_WLAN_MAC_LEN];  /* MAC 地址 */
  bool     up;                 /* 接口是否启用 */
  bool     connected;          /* 是否已连接 AP */
  sem_t    txSem;              /* TX 完成信号量 */
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define WLAN_REG(offset)      (*(volatile uint32_t *)(BK7258_WIFI_BASE + (offset)))

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  bk7258_wlan_ifup(struct net_driver_s *dev);
static int  bk7258_wlan_ifdown(struct net_driver_s *dev);
static int  bk7258_wlan_txavail(struct net_driver_s *dev);
static int  bk7258_wlan_addmac(struct net_driver_s *dev,
                               const uint8_t *mac);
static int  bk7258_wlan_rmmac(struct net_driver_s *dev,
                              const uint8_t *mac);
static int  bk7258_wlan_ioctl(struct net_driver_s *dev, int cmd,
                              void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct netdev_ops_s g_bk7258_wlan_ops =
{
  .ifup    = bk7258_wlan_ifup,
  .ifdown  = bk7258_wlan_ifdown,
  .txavail = bk7258_wlan_txavail,
  .addmac  = bk7258_wlan_addmac,
  .rmmac   = bk7258_wlan_rmmac,
  .ioctl   = bk7258_wlan_ioctl,
};

static struct bk7258_wlan_s g_bk7258_wlan;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_wlan_isr
 *
 * Description:
 *   WiFi 中断处理函数。
 *
 ****************************************************************************/

static int bk7258_wlan_isr(int irq, void *context, void *arg)
{
  struct bk7258_wlan_s *priv = (struct bk7258_wlan_s *)arg;
  uint32_t status = WLAN_REG(BK7258_WIFI_INTSTS);

  /* TX 完成 */
  if (status & 0x01)
    {
      WLAN_REG(BK7258_WIFI_INTSTS) = 0x01;
      nxsem_post(&priv->txSem);
    }

  /* RX 数据到达 */
  if (status & 0x02)
    {
      WLAN_REG(BK7258_WIFI_INTSTS) = 0x02;
      /* TODO: 处理接收数据, 通知 NuttX 网络栈 */
      netdev_lower_rxready(&priv->dev);
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_wlan_ifup
 *
 * Description:
 *   启用网络接口 (连接 AP)。
 *
 ****************************************************************************/

static int bk7258_wlan_ifup(struct net_driver_s *dev)
{
  struct bk7258_wlan_s *priv = (struct bk7258_wlan_s *)dev;

  /* TODO: 调用 BK7258 WiFi SDK 连接 AP
   * - 读取 SSID/密码 (从配置或 NVS)
   * - 发起 STA 模式连接
   * - 等待连接成功
   */

  priv->up = true;
  priv->connected = true;

  /* 使能 WiFi RX/TX */
  WLAN_REG(BK7258_WIFI_CTRL) = BK7258_WIFI_CTRL_ENABLE |
                                BK7258_WIFI_CTRL_TX |
                                BK7258_WIFI_CTRL_RX;

  syslog(LOG_INFO, "WLAN interface up\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_wlan_ifdown
 *
 * Description:
 *   禁用网络接口。
 *
 ****************************************************************************/

static int bk7258_wlan_ifdown(struct net_driver_s *dev)
{
  struct bk7258_wlan_s *priv = (struct bk7258_wlan_s *)dev;

  WLAN_REG(BK7258_WIFI_CTRL) = 0;
  priv->up = false;
  priv->connected = false;

  syslog(LOG_INFO, "WLAN interface down\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_wlan_txavail
 *
 * Description:
 *   发送待发送的数据包。
 *
 ****************************************************************************/

static int bk7258_wlan_txavail(struct net_driver_s *dev)
{
  struct bk7258_wlan_s *priv = (struct bk7258_wlan_s *)dev;

  if (!priv->up || !priv->connected)
    {
      return -ENETDOWN;
    }

  /* TODO: 从 NuttX 网络栈获取数据包并发送
   * - 调用 netdev_lower_txdone() 释放已发送的缓冲
   * - 配置 TX 描述符
   * - 触发发送
   */

  return OK;
}

/****************************************************************************
 * Name: bk7258_wlan_addmac
 *
 * Description:
 *   添加多播 MAC 地址。
 *
 ****************************************************************************/

static int bk7258_wlan_addmac(struct net_driver_s *dev, const uint8_t *mac)
{
  return OK;
}

/****************************************************************************
 * Name: bk7258_wlan_rmmac
 *
 * Description:
 *   移除多播 MAC 地址。
 *
 ****************************************************************************/

static int bk7258_wlan_rmmac(struct net_driver_s *dev, const uint8_t *mac)
{
  return OK;
}

/****************************************************************************
 * Name: bk7258_wlan_ioctl
 *
 * Description:
 *   WiFi ioctl 处理 (扫描、连接、获取状态等)。
 *
 ****************************************************************************/

static int bk7258_wlan_ioctl(struct net_driver_s *dev, int cmd, void *arg)
{
  int ret = OK;

  switch (cmd)
    {
      case SIOCSIWNWID:
        /* 设置网络 ID */
        break;

      case SIOCSIWFREQ:
        /* 设置频率 */
        break;

      case SIOCSIWMODE:
        /* 设置模式 (STA/AP) */
        break;

      case SIOCSIWESSID:
        /* 设置 SSID */
        break;

      case SIOCGIWESSID:
        /* 获取 SSID */
        break;

      case SIOCSIWPASSWD:
        /* 设置密码 */
        break;

      case SIOCGIWRANGE:
        /* 获取能力范围 */
        break;

      case SIOCSIWAP:
        /* 连接到指定 AP */
        break;

      case SIOCGIWAP:
        /* 获取当前 AP */
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
 * Name: bk7258_wlan_initialize
 *
 * Description:
 *   初始化 WiFi6 接口并注册到 NuttX 网络子系统。
 *
 ****************************************************************************/

int bk7258_wlan_initialize(void)
{
  struct bk7258_wlan_s *priv = &g_bk7258_wlan;
  int ret;

  nxsem_init(&priv->txSem, 0, 0);

  /* 使能 WiFi 时钟 */
  bk7258_peri_clk_enable(BK7258_PERI_CLK_WIFI);
  bk7258_peri_reset(BK7258_PERI_CLK_WIFI);

  /* TODO: 初始化 WiFi 固件
   * - 加载 WiFi 固件 (如需要)
   * - 初始化 MAC 地址
   * - 配置 TX/RX 描述符
   */

  /* 设置默认 MAC 地址 (SDK确认: 应从 EFUSE 读取, SOC_EFUSE_REG_BASE=0x44880000) */
  priv->mac[0] = 0x02;
  priv->mac[1] = 0x00;
  priv->mac[2] = 0x00;
  priv->mac[3] = 0x00;
  priv->mac[4] = 0x00;
  priv->mac[5] = 0x01;

  /* 注册中断 */
  ret = irq_attach(BK7258_IRQ_WIFI, bk7258_wlan_isr, priv);
  if (ret == OK)
    {
      WLAN_REG(BK7258_WIFI_INTEN) = 0x03;   /* TX + RX 中断 */
      up_enable_irq(BK7258_IRQ_WIFI);
    }

  /* 配置 netdev */
  priv->dev.d_buf     = NULL;
  priv->dev.d_ifup    = bk7258_wlan_ifup;
  priv->dev.d_ifdown  = bk7258_wlan_ifdown;
  priv->dev.d_txavail = bk7258_wlan_txavail;
  priv->dev.d_addmac  = bk7258_wlan_addmac;
  priv->dev.d_rmmac   = bk7258_wlan_rmmac;
  priv->dev.d_ioctl   = bk7258_wlan_ioctl;
  priv->dev.d_private = priv;

  /* 注册到 NuttX 网络子系统 */
  ret = netdev_register(&priv->dev, NET_LL_IEEE80211);
  if (ret < 0)
    {
      _err("netdev_register failed: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO, "WiFi6 initialized, MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
         priv->mac[0], priv->mac[1], priv->mac[2],
         priv->mac[3], priv->mac[4], priv->mac[5]);
  return OK;
}

/****************************************************************************
 * Name: bk7258_wlan_connect
 *
 * Description:
 *   连接到指定 AP (STA 模式)。
 *
 * Input Parameters:
 *   ssid     - WiFi SSID
 *   password - WiFi 密码
 *
 ****************************************************************************/

int bk7258_wlan_connect(const char *ssid, const char *password)
{
  /* TODO: 调用 BK7258 WiFi SDK 连接 AP
   * - 设置 SSID 和密码
   * - 发起连接
   * - 等待 DHCP 获取 IP
   */

  syslog(LOG_INFO, "Connecting to AP: %s\n", ssid);
  return OK;
}

/****************************************************************************
 * Name: bk7258_wlan_get_mac
 *
 * Description:
 *   获取 MAC 地址。
 *
 ****************************************************************************/

const uint8_t *bk7258_wlan_get_mac(void)
{
  return g_bk7258_wlan.mac;
}

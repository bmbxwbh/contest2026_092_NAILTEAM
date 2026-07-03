/****************************************************************************
 * board/contest_board/src/bk7258_wdt.c
 *
 * BK7258 看门狗驱动
 *
 * 用于系统稳定性保护:
 *   - 系统死锁/卡死时自动复位
 *   - 提供 NuttX watchdog lowerhalf 接口
 *
 * 使用场景:
 *   - 主循环定期喂狗 (5秒超时)
 *   - AI 推理超时不会触发 (独立看门狗)
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/timers/watchdog.h>
#include <nuttx/kmalloc.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 看门狗寄存器 (SDK确认: SOC_WDT_REG_BASE=0x44800000, 偏移来自 wdt_reg.h) */
#define BK7258_WDT_BASE          0x44800000      /* SOC_WDT_REG_BASE */
#define BK7258_WDT_CTRL          (BK7258_WDT_BASE + 0x4*4)   /* CTRL: WDT_R_BASE + 0x4*4 */
#define BK7258_WDT_LOAD          (BK7258_WDT_BASE + 0x04)
#define BK7258_WDT_VALUE         (BK7258_WDT_BASE + 0x08)
#define BK7258_WDT_INTCLR        (BK7258_WDT_BASE + 0x0C)
#define BK7258_WDT_RIS           (BK7258_WDT_BASE + 0x10)
#define BK7258_WDT_MIS           (BK7258_WDT_BASE + 0x14)

#define BK7258_WDT_CTRL_EN       (1 << 0)
#define BK7258_WDT_CTRL_INTEN    (1 << 1)
#define BK7258_WDT_CTRL_RESET    (1 << 2)

/* 看门狗时钟 (SDK确认: 26MHz/分频, WDT_CKEN is bit31 of SYS_CPU_DEVICE_CLK_ENABLE)
 * AON WDT: SOC_AON_WDT_REG_BASE = 0x44000600 */
#define BK7258_WDT_CLK           26000000UL

/* 最大超时 (秒) */
#define BK7258_WDT_MAX_TIMEOUT   10

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_wdt_s
{
  struct watchdog_lowerhalf_s dev;  /* NuttX watchdog 接口 */
  uint32_t timeout;                 /* 当前超时 (ms) */
  bool     started;                 /* 是否已启动 */
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define WDT_REG(offset)         (*(volatile uint32_t *)(offset))

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  bk7258_wdt_start(struct watchdog_lowerhalf_s *dev);
static int  bk7258_wdt_stop(struct watchdog_lowerhalf_s *dev);
static int  bk7258_wdt_keepalive(struct watchdog_lowerhalf_s *dev);
static int  bk7258_wdt_getstatus(struct watchdog_lowerhalf_s *dev,
                                  struct watchdog_status_s *status);
static int  bk7258_wdt_settimeout(struct watchdog_lowerhalf_s *dev,
                                   uint32_t timeout);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct watchdog_ops_s g_bk7258_wdt_ops =
{
  .start      = bk7258_wdt_start,
  .stop       = bk7258_wdt_stop,
  .keepalive  = bk7258_wdt_keepalive,
  .getstatus  = bk7258_wdt_getstatus,
  .settimeout = bk7258_wdt_settimeout,
};

static struct bk7258_wdt_s g_bk7258_wdt;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int bk7258_wdt_start(struct watchdog_lowerhalf_s *dev)
{
  struct bk7258_wdt_s *priv = (struct bk7258_wdt_s *)dev;

  WDT_REG(BK7258_WDT_CTRL) = BK7258_WDT_CTRL_EN | BK7258_WDT_CTRL_RESET;
  priv->started = true;
  syslog(LOG_INFO, "Watchdog started (timeout=%u ms)\n", priv->timeout);
  return OK;
}

static int bk7258_wdt_stop(struct watchdog_lowerhalf_s *dev)
{
  struct bk7258_wdt_s *priv = (struct bk7258_wdt_s *)dev;

  WDT_REG(BK7258_WDT_CTRL) = 0;
  priv->started = false;
  syslog(LOG_INFO, "Watchdog stopped\n");
  return OK;
}

static int bk7258_wdt_keepalive(struct watchdog_lowerhalf_s *dev)
{
  /* 喂狗: 写任意值到 INTCLR */
  WDT_REG(BK7258_WDT_INTCLR) = 0x01;
  return OK;
}

static int bk7258_wdt_getstatus(struct watchdog_lowerhalf_s *dev,
                                struct watchdog_status_s *status)
{
  struct bk7258_wdt_s *priv = (struct bk7258_wdt_s *)dev;

  status->flags = 0;
  if (priv->started)
    {
      status->flags |= WDIOC_ACTIVE;
    }
  status->timeout = priv->timeout;
  status->timeleft = (WDT_REG(BK7258_WDT_VALUE) * 1000) / BK7258_WDT_CLK;

  return OK;
}

static int bk7258_wdt_settimeout(struct watchdog_lowerhalf_s *dev,
                                 uint32_t timeout)
{
  struct bk7258_wdt_s *priv = (struct bk7258_wdt_s *)dev;
  uint32_t ticks;

  if (timeout == 0 || timeout > BK7258_WDT_MAX_TIMEOUT * 1000)
    {
      return -EINVAL;
    }

  priv->timeout = timeout;
  ticks = (timeout * BK7258_WDT_CLK) / 1000;
  WDT_REG(BK7258_WDT_LOAD) = ticks;

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_wdt_initialize
 *
 * Description:
 *   初始化看门狗并注册到 NuttX。
 *
 * Input Parameters:
 *   defaultTimeout - 默认超时时间 (ms)
 *
 ****************************************************************************/

int bk7258_wdt_initialize(uint32_t defaultTimeout)
{
  struct bk7258_wdt_s *priv = &g_bk7258_wdt;
  int ret;

  priv->dev.ops = &g_bk7258_wdt_ops;
  priv->timeout = defaultTimeout;
  priv->started = false;

  /* 设置默认超时 */
  ret = bk7258_wdt_settimeout((struct watchdog_lowerhalf_s *)priv,
                              defaultTimeout);
  if (ret < 0)
    {
      _err("WDT settimeout failed: %d\n", ret);
      return ret;
    }

  /* 注册到 NuttX */
  ret = watchdog_register("/dev/watchdog0", &priv->dev);
  if (ret < 0)
    {
      _err("watchdog_register failed: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO, "Watchdog initialized (timeout=%u ms)\n", defaultTimeout);
  return OK;
}

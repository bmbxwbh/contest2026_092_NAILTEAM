/****************************************************************************
 * board/contest_board/src/bk7258_timer.c
 *
 * BK7258 定时器驱动
 *
 * 提供 NuttX oneshot/periodic 定时器 lowerhalf 接口。
 * 基于 BK7258 通用定时器 Timer0/Timer1。
 *
 * SysTick 由 ARMv8-M 内核提供, 不在此实现。
 *
 ****************************************************************************/

#if defined(CONFIG_TIMER) && defined(CONFIG_BK7258_TIMER)

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/timer/timer.h>
#include <nuttx/kmalloc.h>
#include <arch/irq.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BK7258_NTIMERS            2

/* 定时器时钟: APB = 120MHz */
#define BK7258_TIMER_CLK          (BK7258_MAX_FREQ / 4)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_timer_s
{
  uint32_t base;            /* 寄存器基地址 */
  int      irq;             /* 中断号 */
  uint32_t freq;            /* 定时器频率 */
  uint32_t period;          /* 当前周期 (us) */
  bool     periodic;        /* 周期模式 */
  tccb_t   callback;        /* 超时回调 */
  void    *arg;             /* 回调参数 */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct bk7258_timer_s g_timers[BK7258_NTIMERS] =
{
  {
    .base = BK7258_TIMER0_BASE,
    .irq  = BK7258_IRQ_TIMER0,
    .freq = BK7258_TIMER_CLK,
  },
  {
    .base = BK7258_TIMER1_BASE,
    .irq  = BK7258_IRQ_TIMER1,
    .freq = BK7258_TIMER_CLK,
  },
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define TIMER_REG(base, offset)  (*(volatile uint32_t *)((base) + (offset)))

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  bk7258_timer_start(FAR struct timer_lowerhalf_s *dev);
static int  bk7258_timer_stop(FAR struct timer_lowerhalf_s *dev);
static int  bk7258_timer_getstatus(FAR struct timer_lowerhalf_s *dev,
                                   FAR struct timer_status_s *status);
static int  bk7258_timer_settimeout(FAR struct timer_lowerhalf_s *dev,
                                    uint32_t timeout);
static void bk7258_timer_setcallback(FAR struct timer_lowerhalf_s *dev,
                                     tccb_t callback, FAR void *arg);
static int  bk7258_timer_maxtimeout(FAR struct timer_lowerhalf_s *dev,
                                    FAR uint32_t *timeout);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct timer_ops_s g_bk7258_timer_ops =
{
  .start       = bk7258_timer_start,
  .stop        = bk7258_timer_stop,
  .getstatus   = bk7258_timer_getstatus,
  .settimeout  = bk7258_timer_settimeout,
  .setcallback = bk7258_timer_setcallback,
  .maxtimeout  = bk7258_timer_maxtimeout,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_timer_isr
 *
 * Description:
 *   定时器中断处理函数。
 *
 ****************************************************************************/

static int bk7258_timer_isr(int irq, FAR void *context, FAR void *arg)
{
  FAR struct bk7258_timer_s *priv = (FAR struct bk7258_timer_s *)arg;

  /* 清除中断标志 */
  TIMER_REG(priv->base, BK7258_TIMER_INTCLR) = 1;

  /* 调用用户回调 */
  if (priv->callback)
    {
      priv->callback(&arg);
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_timer_start
 *
 * Description:
 *   启动定时器。
 *
 ****************************************************************************/

static int bk7258_timer_start(FAR struct timer_lowerhalf_s *dev)
{
  FAR struct bk7258_timer_s *priv = (FAR struct bk7258_timer_s *)dev;
  uint32_t ctrl = 0;

  /* 计算加载值: timeout(us) × freq(Hz) / 1000000 */
  uint32_t load = (priv->period * priv->freq) / 1000000;
  if (load == 0)
    {
      load = 1;
    }

  TIMER_REG(priv->base, BK7258_TIMER_LOAD) = load;

  ctrl = BK7258_TIMER_CTRL_EN | BK7258_TIMER_CTRL_INT;
  if (priv->periodic)
    {
      ctrl |= BK7258_TIMER_CTRL_MODE;
    }

  TIMER_REG(priv->base, BK7258_TIMER_CTRL) = ctrl;
  return OK;
}

/****************************************************************************
 * Name: bk7258_timer_stop
 *
 * Description:
 *   停止定时器。
 *
 ****************************************************************************/

static int bk7258_timer_stop(FAR struct timer_lowerhalf_s *dev)
{
  FAR struct bk7258_timer_s *priv = (FAR struct bk7258_timer_s *)dev;
  TIMER_REG(priv->base, BK7258_TIMER_CTRL) = 0;
  return OK;
}

/****************************************************************************
 * Name: bk7258_timer_getstatus
 *
 * Description:
 *   获取定时器状态。
 *
 ****************************************************************************/

static int bk7258_timer_getstatus(FAR struct timer_lowerhalf_s *dev,
                                  FAR struct timer_status_s *status)
{
  FAR struct bk7258_timer_s *priv = (FAR struct bk7258_timer_s *)dev;

  status->timeout = priv->period;
  status->timeleft = (TIMER_REG(priv->base, BK7258_TIMER_VALUE) * 1000000) /
                     priv->freq;
  status->flags = TIMER_FREQUENCY_HIRES;
  if (priv->periodic)
    {
      status->flags |= TIMER_MODE_PERIODIC;
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_timer_settimeout
 *
 * Description:
 *   设置超时时间。
 *
 ****************************************************************************/

static int bk7258_timer_settimeout(FAR struct timer_lowerhalf_s *dev,
                                   uint32_t timeout)
{
  FAR struct bk7258_timer_s *priv = (FAR struct bk7258_timer_s *)dev;
  priv->period = timeout;
  return OK;
}

/****************************************************************************
 * Name: bk7258_timer_setcallback
 *
 * Description:
 *   设置超时回调函数。
 *
 ****************************************************************************/

static void bk7258_timer_setcallback(FAR struct timer_lowerhalf_s *dev,
                                     tccb_t callback, FAR void *arg)
{
  FAR struct bk7258_timer_s *priv = (FAR struct bk7258_timer_s *)dev;
  priv->callback = callback;
  priv->arg = arg;
}

/****************************************************************************
 * Name: bk7258_timer_maxtimeout
 *
 * Description:
 *   返回最大超时时间。
 *
 ****************************************************************************/

static int bk7258_timer_maxtimeout(FAR struct timer_lowerhalf_s *dev,
                                   FAR uint32_t *timeout)
{
  *timeout = 0xFFFFFFFF;
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_timer_initialize
 *
 * Description:
 *   初始化定时器并注册到 NuttX timer 框架。
 *
 ****************************************************************************/

int bk7258_timer_initialize(void)
{
  FAR struct bk7258_timer_s *priv;
  char devname[16];
  int ret;
  int i;

  for (i = 0; i < BK7258_NTIMERS; i++)
    {
      priv = &g_timers[i];
      priv->periodic = true;
      priv->period = 1000000;   /* 默认 1 秒 */

      /* 使能定时器时钟 */
      bk7258_peri_clk_enable(BK7258_PERI_CLK_TIMER0 << i);
      bk7258_peri_reset(BK7258_PERI_CLK_TIMER0 << i);

      /* 注册中断 */
      ret = irq_attach(priv->irq, bk7258_timer_isr, priv);
      if (ret < 0)
        {
          snerr("Timer%d irq_attach failed: %d\n", i, ret);
          continue;
        }

      up_enable_irq(priv->irq);

      /* 注册到 NuttX */
      snprintf(devname, sizeof(devname), "/dev/timer%d", i);
      ret = timer_register(devname, (FAR struct timer_lowerhalf_s *)priv);
      if (ret < 0)
        {
          snerr("timer_register(%s) failed: %d\n", devname, ret);
        }
      else
        {
          syslog(LOG_INFO, "Timer%d registered: %s\n", i, devname);
        }
    }

  return OK;
}

#endif /* CONFIG_TIMER && CONFIG_BK7258_TIMER */

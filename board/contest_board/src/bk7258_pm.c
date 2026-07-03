/****************************************************************************
 * board/contest_board/src/bk7258_pm.c
 *
 * BK7258 低功耗管理驱动
 *
 * 用于智能音箱低功耗场景:
 *   - 待机模式 (夜间/无操作时, 降低CPU频率, 关闭不必要外设)
 *   - 轻睡眠模式 (保持语音唤醒, 关闭LCD/WiFi)
 *   - 唤醒源管理 (语音唤醒/按键/定时器)
 *
 * 实现 NuttX PM (Power Management) lowerhalf 接口。
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/power/pm.h>
#include <nuttx/kmalloc.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* PM 状态 */
#define BK7258_PM_NORMAL         0   /* 正常运行 480MHz */
#define BK7258_PM_IDLE           1   /* 空闲 240MHz, WiFi/LCD 保持 */
#define BK7258_PM_STANDBY        2   /* 待机 80MHz, 关闭 LCD/WiFi */
#define BK7258_PM_SLEEP          3   /* 睡眠, 仅保留语音唤醒/定时器 */

/* 系统控制寄存器 (低功耗相关) */
#define BK7258_PM_CTRL           (BK7258_SYS_CTRL_BASE + 0x20)
#define BK7258_PM_WKUP_EN        (BK7258_SYS_CTRL_BASE + 0x24)
#define BK7258_PM_WKUP_STS       (BK7258_SYS_CTRL_BASE + 0x28)

/* 唤醒源 */
#define BK7258_WKUP_GPIO         (1 << 0)
#define BK7258_WKUP_TIMER        (1 << 1)
#define BK7258_WKUP_VOICE        (1 << 2)   /* 语音唤醒 */
#define BK7258_WKUP_BLE          (1 << 3)
#define BK7258_WKUP_WIFI         (1 << 4)

/* 低功耗模式控制位 */
#define BK7258_PM_CTRL_SLEEP     (1 << 0)
#define BK7258_PM_CTRL_DEEPSLEEP (1 << 1)
#define BK7258_PM_CTRL_PD_WIFI   (1 << 2)   /* WiFi 掉电 */
#define BK7258_PM_CTRL_PD_LCD    (1 << 3)   /* LCD 掉电 */
#define BK7258_PM_CTRL_PD_BLE    (1 << 4)   /* BLE 掉电 */

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_pm_s
{
  struct pm_lowerhalf_s dev;    /* NuttX PM 接口 */
  uint8_t  currentState;        /* 当前 PM 状态 */
  uint32_t wakeupMask;          /* 唤醒源掩码 */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  bk7258_pm_prepare(struct pm_lowerhalf_s *dev, int domain,
                              enum pm_state_e pmstate);
static void bk7258_pm_notify(struct pm_lowerhalf_s *dev, int domain,
                             enum pm_state_e pmstate);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct pm_ops_s g_bk7258_pm_ops =
{
  .prepare = bk7258_pm_prepare,
  .notify  = bk7258_pm_notify,
};

static struct bk7258_pm_s g_bk7258_pm;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_pm_enter_idle
 *
 * Description:
 *   进入空闲模式: 降低 CPU 频率, 保持外设运行。
 *
 ****************************************************************************/

static void bk7258_pm_enter_idle(void)
{
  /* 降低 CPU 频率到 240MHz */
  bk7258_clock_config(240000000);
  syslog(LOG_INFO, "PM: enter IDLE (240MHz)\n");
}

/****************************************************************************
 * Name: bk7258_pm_enter_standby
 *
 * Description:
 *   进入待机模式: 关闭 LCD, 降低 CPU 到 80MHz, 保持语音唤醒。
 *
 ****************************************************************************/

static void bk7258_pm_enter_standby(void)
{
  /* 关闭 LCD 背光 */
  bk7258_set_backlight(0);

  /* 关闭 LCD 控制器 */
  volatile uint32_t *lcd_ctrl = (volatile uint32_t *)(BK7258_LCD_BASE + BK7258_LCD_CTRL);
  *lcd_ctrl &= ~BK7258_LCD_CTRL_ENABLE;

  /* WiFi 进入省电模式 */
  /* TODO: 调用 WiFi 驱动进入 PS 模式 */

  /* 降低 CPU 频率到 80MHz */
  bk7258_clock_config(80000000);

  syslog(LOG_INFO, "PM: enter STANDBY (80MHz, LCD off)\n");
}

/****************************************************************************
 * Name: bk7258_pm_enter_sleep
 *
 * Description:
 *   进入睡眠模式: 仅保留语音唤醒和定时器。
 *
 ****************************************************************************/

static void bk7258_pm_enter_sleep(void)
{
  /* 配置唤醒源 */
  volatile uint32_t *wkup_en = (volatile uint32_t *)BK7258_PM_WKUP_EN;
  *wkup_en = BK7258_WKUP_VOICE | BK7258_WKUP_TIMER | BK7258_WKUP_GPIO;

  /* 关闭外设电源 */
  volatile uint32_t *pm_ctrl = (volatile uint32_t *)BK7258_PM_CTRL;
  *pm_ctrl = BK7258_PM_CTRL_PD_WIFI | BK7258_PM_CTRL_PD_LCD |
             BK7258_PM_CTRL_PD_BLE;

  /* 进入睡眠 */
  *pm_ctrl |= BK7258_PM_CTRL_SLEEP;

  /* 执行 WFI (Wait For Interrupt) */
  __asm__ volatile ("wfi");

  syslog(LOG_INFO, "PM: woke up from SLEEP\n");
}

/****************************************************************************
 * Name: bk7258_pm_exit_lowpower
 *
 * Description:
 *   退出低功耗模式, 恢复正常运行。
 *
 ****************************************************************************/

static void bk7258_pm_exit_lowpower(void)
{
  /* 恢复 CPU 频率到 480MHz */
  bk7258_clock_config(480000000);

  /* 恢复外设电源 */
  volatile uint32_t *pm_ctrl = (volatile uint32_t *)BK7258_PM_CTRL;
  *pm_ctrl = 0;

  /* 恢复 LCD */
  volatile uint32_t *lcd_ctrl = (volatile uint32_t *)(BK7258_LCD_BASE + BK7258_LCD_CTRL);
  *lcd_ctrl |= BK7258_LCD_CTRL_ENABLE;
  bk7258_set_backlight(80);   /* 默认 80% 亮度 */

  syslog(LOG_INFO, "PM: back to NORMAL (480MHz)\n");
}

/****************************************************************************
 * Name: bk7258_pm_prepare
 *
 * Description:
 *   PM 状态切换准备 (NuttX PM 框架调用)。
 *
 ****************************************************************************/

static int bk7258_pm_prepare(struct pm_lowerhalf_s *dev, int domain,
                             enum pm_state_e pmstate)
{
  struct bk7258_pm_s *priv = (struct bk7258_pm_s *)dev;
  int ret = OK;

  switch (pmstate)
    {
      case PM_NORMAL:
        bk7258_pm_exit_lowpower();
        priv->currentState = BK7258_PM_NORMAL;
        break;

      case PM_IDLE:
        bk7258_pm_enter_idle();
        priv->currentState = BK7258_PM_IDLE;
        break;

      case PM_STANDBY:
        bk7258_pm_enter_standby();
        priv->currentState = BK7258_PM_STANDBY;
        break;

      case PM_SLEEP:
        bk7258_pm_enter_sleep();
        priv->currentState = BK7258_PM_SLEEP;
        break;

      default:
        ret = -EINVAL;
        break;
    }

  return ret;
}

/****************************************************************************
 * Name: bk7258_pm_notify
 *
 * Description:
 *   PM 状态切换通知 (NuttX PM 框架调用)。
 *
 ****************************************************************************/

static void bk7258_pm_notify(struct pm_lowerhalf_s *dev, int domain,
                             enum pm_state_e pmstate)
{
  /* 通知各子系统即将进入低功耗 */
  switch (pmstate)
    {
      case PM_SLEEP:
        syslog(LOG_INFO, "PM: notifying SLEEP\n");
        /* TODO: 通知 AI Agent 暂停, 保存场景状态 */
        break;

      default:
        break;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_pm_initialize
 *
 * Description:
 *   初始化 PM 子系统并注册到 NuttX。
 *
 ****************************************************************************/

int bk7258_pm_initialize(void)
{
  struct bk7258_pm_s *priv = &g_bk7258_pm;
  int ret;

  priv->dev.ops = &g_bk7258_pm_ops;
  priv->currentState = BK7258_PM_NORMAL;
  priv->wakeupMask = BK7258_WKUP_VOICE | BK7258_WKUP_TIMER |
                     BK7258_WKUP_GPIO;

  /* 配置默认唤醒源 */
  volatile uint32_t *wkup_en = (volatile uint32_t *)BK7258_PM_WKUP_EN;
  *wkup_en = priv->wakeupMask;

  /* 注册到 NuttX PM 子系统 */
  ret = pm_register(&priv->dev);
  if (ret < 0)
    {
      _err("pm_register failed: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO, "PM initialized (wakeup: voice/timer/gpio)\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_pm_get_wakeup_source
 *
 * Description:
 *   获取唤醒源 (用于判断从何种状态唤醒)。
 *
 ****************************************************************************/

uint32_t bk7258_pm_get_wakeup_source(void)
{
  volatile uint32_t *wkup_sts = (volatile uint32_t *)BK7258_PM_WKUP_STS;
  return *wkup_sts;
}

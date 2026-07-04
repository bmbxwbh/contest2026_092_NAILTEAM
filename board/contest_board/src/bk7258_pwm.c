/****************************************************************************
 * board/contest_board/src/bk7258_pwm.c
 *
 * BK7258 PWM 驱动
 *
 * 用于:
 *   - LCD 背光亮度调节 (PWM0)
 *   - 蜂鸣器音调控制 (PWM1, 异常告警)
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_PWM) && defined(CONFIG_BK7258_PWM)
#include <nuttx/timers/pwm.h>
#include <nuttx/kmalloc.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BK7258_NPWM               2

/* PWM 寄存器 (SDK确认: SOC_PWM_REG_BASE=0x458a0000, PWM0和PWM1共用) */
#define BK7258_PWM0_BASE          0x458a0000
#define BK7258_PWM1_BASE          0x458a0000

#define BK7258_PWM_CTRL           0x00
#define BK7258_PWM_PERIOD         0x04
#define BK7258_PWM_DUTY           0x08
#define BK7258_PWM_POLARITY       0x0C

#define BK7258_PWM_CTRL_EN        (1 << 0)
#define BK7258_PWM_CTRL_INTEN     (1 << 1)

/* PWM 时钟 (SDK确认: 来自 SYS_CPU_CLK_DIV_MODE1 寄存器) */
#define BK7258_PWM_CLK            120000000UL

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_pwm_s
{
  struct pwm_lowerhalf_s dev;    /* NuttX PWM 接口 */
  uint32_t base;                /* 寄存器基地址 */
  uint32_t frequency;           /* 当前频率 */
  uint16_t duty;                /* 当前占空比 (0~65535) */
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define PWM_REG(base, offset)    (*(volatile uint32_t *)((base) + (offset)))

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  bk7258_pwm_setup(struct pwm_lowerhalf_s *dev);
static int  bk7258_pwm_shutdown(struct pwm_lowerhalf_s *dev);
static int  bk7258_pwm_start(struct pwm_lowerhalf_s *dev,
                             const struct pwm_info_s *info);
static int  bk7258_pwm_stop(struct pwm_lowerhalf_s *dev);
static int  bk7258_pwm_ioctl(struct pwm_lowerhalf_s *dev, int cmd,
                             unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct pwm_ops_s g_bk7258_pwm_ops =
{
  .setup    = bk7258_pwm_setup,
  .shutdown = bk7258_pwm_shutdown,
  .start    = bk7258_pwm_start,
  .stop     = bk7258_pwm_stop,
  .ioctl    = bk7258_pwm_ioctl,
};

static struct bk7258_pwm_s g_bk7258_pwm[BK7258_NPWM] =
{
  {
    .base = BK7258_PWM0_BASE,
  },
  {
    .base = BK7258_PWM1_BASE,
  },
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int bk7258_pwm_setup(struct pwm_lowerhalf_s *dev)
{
  return OK;
}

static int bk7258_pwm_shutdown(struct pwm_lowerhalf_s *dev)
{
  struct bk7258_pwm_s *priv = (struct bk7258_pwm_s *)dev;
  PWM_REG(priv->base, BK7258_PWM_CTRL) = 0;
  return OK;
}

static int bk7258_pwm_start(struct pwm_lowerhalf_s *dev,
                            const struct pwm_info_s *info)
{
  struct bk7258_pwm_s *priv = (struct bk7258_pwm_s *)dev;
  uint32_t period;
  uint32_t duty;

  if (info->frequency == 0)
    {
      return -EINVAL;
    }

  /* 计算周期值: period = PWM_CLK / frequency */
  period = BK7258_PWM_CLK / info->frequency;
  if (period == 0)
    {
      period = 1;
    }

  /* 计算占空比: duty = period × duty / 65536 */
  duty = (period * info->duty) / 65536;

  PWM_REG(priv->base, BK7258_PWM_PERIOD) = period;
  PWM_REG(priv->base, BK7258_PWM_DUTY)   = duty;
  PWM_REG(priv->base, BK7258_PWM_CTRL)   = BK7258_PWM_CTRL_EN;

  priv->frequency = info->frequency;
  priv->duty = info->duty;

  return OK;
}

static int bk7258_pwm_stop(struct pwm_lowerhalf_s *dev)
{
  struct bk7258_pwm_s *priv = (struct bk7258_pwm_s *)dev;
  PWM_REG(priv->base, BK7258_PWM_CTRL) = 0;
  return OK;
}

static int bk7258_pwm_ioctl(struct pwm_lowerhalf_s *dev, int cmd,
                            unsigned long arg)
{
  return -ENOTTY;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_pwm_initialize
 *
 * Description:
 *   初始化 PWM 并注册到 NuttX。
 *
 ****************************************************************************/

int bk7258_pwm_initialize(void)
{
  struct bk7258_pwm_s *priv;
  char devname[16];
  int ret;
  int i;

  for (i = 0; i < BK7258_NPWM; i++)
    {
      priv = &g_bk7258_pwm[i];
      priv->dev.ops = &g_bk7258_pwm_ops;

      snprintf(devname, sizeof(devname), "/dev/pwm%d", i);
      ret = pwm_register(devname, &priv->dev);
      if (ret < 0)
        {
          _err("pwm_register(%s) failed: %d\n", devname, ret);
        }
      else
        {
          syslog(LOG_INFO, "PWM%d registered: %s\n", i, devname);
        }
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_set_backlight
 *
 * Description:
 *   设置 LCD 背光亮度。
 *
 * Input Parameters:
 *   brightness - 亮度 (0~100)
 *
 ****************************************************************************/

int bk7258_set_backlight(uint8_t brightness)
{
  struct bk7258_pwm_s *priv = &g_bk7258_pwm[0];
  uint32_t period = BK7258_PWM_CLK / 1000;   /* 1kHz */
  uint32_t duty = (period * brightness) / 100;

  PWM_REG(priv->base, BK7258_PWM_PERIOD) = period;
  PWM_REG(priv->base, BK7258_PWM_DUTY)   = duty;
  PWM_REG(priv->base, BK7258_PWM_CTRL)   = BK7258_PWM_CTRL_EN;

  return OK;
}

/****************************************************************************
 * Name: bk7258_buzzer_on
 *
 * Description:
 *   打开蜂鸣器 (异常告警)。
 *
 * Input Parameters:
 *   frequency - 蜂鸣频率 (Hz)
 *
 ****************************************************************************/

int bk7258_buzzer_on(uint32_t frequency)
{
  struct bk7258_pwm_s *priv = &g_bk7258_pwm[1];
  uint32_t period = BK7258_PWM_CLK / frequency;
  uint32_t duty = period / 2;   /* 50% 占空比 */

  PWM_REG(priv->base, BK7258_PWM_PERIOD) = period;
  PWM_REG(priv->base, BK7258_PWM_DUTY)   = duty;
  PWM_REG(priv->base, BK7258_PWM_CTRL)   = BK7258_PWM_CTRL_EN;

  return OK;
}

/****************************************************************************
 * Name: bk7258_buzzer_off
 *
 * Description:
 *   关闭蜂鸣器。
 *
 ****************************************************************************/

int bk7258_buzzer_off(void)
{
  struct bk7258_pwm_s *priv = &g_bk7258_pwm[1];
  PWM_REG(priv->base, BK7258_PWM_CTRL) = 0;
  return OK;
}

#endif /* CONFIG_PWM && CONFIG_BK7258_PWM */

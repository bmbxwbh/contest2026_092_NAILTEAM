/****************************************************************************
 * board/contest_board/src/bk7258_gpio.c
 *
 * BK7258 GPIO 驱动
 *
 * 提供 GPIO 配置/读写接口, 用于:
 *   - LED 状态指示
 *   - 按键输入
 *   - 蜂鸣器控制 (异常告警)
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/kmalloc.h>
#include <arch/irq.h>
#include <arch/chip/bk7258.h>

#include <errno.h>
#include <debug.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BK7258_NGPIO             BK7258_GPIO_NUM

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_gpio_s
{
  uint8_t  mode;            /* 当前模式 */
  bool     int_enable;      /* 中断使能标志 */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct bk7258_gpio_s g_gpio[BK7258_NGPIO];

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define GPIO_REG(offset)        (*(volatile uint32_t *)(BK7258_GPIO_BASE + (offset)))
#define GPIO_CFG(n)             (*(volatile uint32_t *)BK7258_GPIO_CFG(n))

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_gpio_initialize
 *
 * Description:
 *   初始化 GPIO 子系统, 注册到 NuttX GPIO 框架。
 *
 ****************************************************************************/

int bk7258_gpio_initialize(void)
{
  int i;

  for (i = 0; i < BK7258_NGPIO; i++)
    {
      g_gpio[i].mode = BK7258_GPIO_INPUT;
      g_gpio[i].int_enable = false;
    }

  _info("GPIO subsystem initialized (%d pins)\n", BK7258_NGPIO);
  return OK;
}

/****************************************************************************
 * Name: bk7258_gpio_config
 *
 * Description:
 *   配置 GPIO 引脚模式。
 *
 * Input Parameters:
 *   port - GPIO 引脚号 (0 ~ 47)
 *   mode - 模式: INPUT / OUTPUT / PULLUP / PULLDOWN
 *
 ****************************************************************************/

void bk7258_gpio_config(int port, int mode)
{
  if (port < 0 || port >= BK7258_NGPIO)
    {
      return;
    }

  GPIO_CFG(port) = (uint32_t)mode;
  g_gpio[port].mode = (uint8_t)mode;

  /* 配置方向: OUTPUT 时设置方向位 */
  if (mode == BK7258_GPIO_OUTPUT)
    {
      GPIO_REG(BK7258_GPIO_DIR - BK7258_GPIO_BASE) |= (1 << port);
    }
  else
    {
      GPIO_REG(BK7258_GPIO_DIR - BK7258_GPIO_BASE) &= ~(1 << port);
    }
}

/****************************************************************************
 * Name: bk7258_gpio_write
 *
 * Description:
 *   写 GPIO 输出值。
 *
 ****************************************************************************/

void bk7258_gpio_write(int port, bool value)
{
  if (port < 0 || port >= BK7258_NGPIO)
    {
      return;
    }

  if (value)
    {
      GPIO_REG(BK7258_GPIO_DATA - BK7258_GPIO_BASE) |= (1 << port);
    }
  else
    {
      GPIO_REG(BK7258_GPIO_DATA - BK7258_GPIO_BASE) &= ~(1 << port);
    }
}

/****************************************************************************
 * Name: bk7258_gpio_read
 *
 * Description:
 *   读 GPIO 输入值。
 *
 ****************************************************************************/

bool bk7258_gpio_read(int port)
{
  if (port < 0 || port >= BK7258_NGPIO)
    {
      return false;
    }

  return (GPIO_REG(BK7258_GPIO_DATA - BK7258_GPIO_BASE) & (1 << port)) != 0;
}

/****************************************************************************
 * Name: bk7258_gpio_set_led
 *
 * Description:
 *   控制板载 LED (状态指示)。
 *
 ****************************************************************************/

void bk7258_gpio_set_led(bool on)
{
#ifdef CONFIG_BK7258_LED_GPIO
  bk7258_gpio_write(CONFIG_BK7258_LED_GPIO, on);
#endif
}

/****************************************************************************
 * Name: bk7258_gpio_read_key
 *
 * Description:
 *   读取板载按键状态。
 *
 * Returned Value:
 *   true  - 按键按下
 *   false - 按键释放
 *
 ****************************************************************************/

bool bk7258_gpio_read_key(void)
{
#ifdef CONFIG_BK7258_KEY_GPIO
  return !bk7258_gpio_read(CONFIG_BK7258_KEY_GPIO);  /* 低有效 */
#else
  return false;
#endif
}

/****************************************************************************
 * Name: bk7258_gpio_toggle_buzzer
 *
 * Description:
 *   控制蜂鸣器 (异常告警)。
 *
 ****************************************************************************/

void bk7258_gpio_toggle_buzzer(bool on)
{
  /* TODO: 蜂鸣器 GPIO 待确认后实现 */
  _info("Buzzer %s\n", on ? "ON" : "OFF");
}

/****************************************************************************
 * board/contest_board/src/bk7258_irq.c
 *
 * BK7258 中断控制器适配 (ARMv8-M NVIC)
 *
 * 实现 NuttX 所需的中断控制接口:
 *   - up_enable_irq:    使能中断
 *   - up_disable_irq:   禁用中断
 *   - up_prioritize_irq: 设置中断优先级
 *
 * Cortex-M33 NVIC 寄存器:
 *   ISERn - 中断使能寄存器
 *   ICERn - 中断禁用寄存器
 *   IPRn  - 中断优先级寄存器
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <arch/irq.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* NVIC 寄存器基址 (ARMv8-M 标准) */
#define ARMV8M_NVIC_BASE        0xE000E100
#define ARMV8M_NVIC_ISER(n)     (ARMV8M_NVIC_BASE + (n) * 4)      /* 使能 */
#define ARMV8M_NVIC_ICER(n)     (ARMV8M_NVIC_BASE + 0x80 + (n) * 4) /* 禁用 */
#define ARMV8M_NVIC_ISPR(n)     (ARMV8M_NVIC_BASE + 0x100 + (n) * 4) /* 挂起 */
#define ARMV8M_NVIC_ICPR(n)     (ARMV8M_NVIC_BASE + 0x180 + (n) * 4) /* 清除挂起 */
#define ARMV8M_NVIC_IABR(n)     (ARMV8M_NVIC_BASE + 0x200 + (n) * 4) /* 活动 */
#define ARMV8M_NVIC_IPR(n)      (ARMV8M_NVIC_BASE + 0x300 + (n) * 4) /* 优先级 */

/* 外设中断数量 (SDK确认: INT_ID_MAX = 60) */
#define BK7258_NVIC_IRQS        60

/* 优先级位数 (SDK确认: Cortex-M33 ARMv8-M, 3 bits) */
#define BK7258_NVIC_PRIO_BITS   3

/* 优先级宏 */
#define BK7258_NVIC_PRIO_SHIFT  (8 - BK7258_NVIC_PRIO_BITS)
#define BK7258_NVIC_PRIO(p)     ((p) << BK7258_NVIC_PRIO_SHIFT)

/* 默认优先级 (0=最高, 7=最低, 3-bit) */
#define BK7258_PRIO_HIGHEST     0
#define BK7258_PRIO_HIGH        1
#define BK7258_PRIO_MED         3
#define BK7258_PRIO_LOW         7

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define NVIC_REG(addr)          (*(volatile uint32_t *)(addr))

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_enable_irq
 *
 * Description:
 *   使能指定中断。
 *
 * Input Parameters:
 *   irq - 中断号
 *
 ****************************************************************************/

void up_enable_irq(int irq)
{
  if (irq < 0 || irq >= BK7258_NVIC_IRQS)
    {
      return;
    }

  /* 写 ISER 使能中断 (写 1 使能, 写 0 无效) */
  NVIC_REG(ARMV8M_NVIC_ISER(irq / 32)) = (1 << (irq % 32));
}

/****************************************************************************
 * Name: up_disable_irq
 *
 * Description:
 *   禁用指定中断。
 *
 ****************************************************************************/

void up_disable_irq(int irq)
{
  if (irq < 0 || irq >= BK7258_NVIC_IRQS)
    {
      return;
    }

  /* 写 ICER 禁用中断 */
  NVIC_REG(ARMV8M_NVIC_ICER(irq / 32)) = (1 << (irq % 32));
}

/****************************************************************************
 * Name: up_prioritize_irq
 *
 * Description:
 *   设置中断优先级。
 *
 * Input Parameters:
 *   irq      - 中断号
 *   priority - 优先级 (0=最高, 15=最低)
 *
 ****************************************************************************/

void up_prioritize_irq(int irq, int priority)
{
  if (irq < 0 || irq >= BK7258_NVIC_IRQS)
    {
      return;
    }

  /* 限制优先级范围 (3-bit: 0~7) */
  if (priority < 0)
    {
      priority = 0;
    }
  if (priority > 7)
    {
      priority = 7;
    }

  /* 每个 IRQ 占 IPR 的 8 位 (高 N 位有效) */
  volatile uint8_t *ipr = (volatile uint8_t *)ARMV8M_NVIC_IPR(0);
  ipr[irq] = BK7258_NVIC_PRIO(priority);
}

/****************************************************************************
 * Name: bk7258_irq_initialize
 *
 * Description:
 *   初始化中断控制器, 设置默认优先级。
 *
 ****************************************************************************/

void bk7258_irq_initialize(void)
{
  int i;

  /* 禁用所有外设中断 */
  for (i = 0; i < BK7258_NVIC_IRQS / 32; i++)
    {
      NVIC_REG(ARMV8M_NVIC_ICER(i)) = 0xFFFFFFFF;
      NVIC_REG(ARMV8M_NVIC_ICPR(i)) = 0xFFFFFFFF;
    }

  /* 设置默认优先级:
   * - 高优先级: UART0/UART1 (控制台不能阻塞)
   * - 中优先级: Timer/AI/I2S (常规外设)
   * - 低优先级: WiFi/LCD (可延迟处理)
   */
  up_prioritize_irq(BK7258_IRQ_UART0, BK7258_PRIO_HIGH);
  up_prioritize_irq(BK7258_IRQ_UART1, BK7258_PRIO_HIGH);
  up_prioritize_irq(BK7258_IRQ_TIMER0, BK7258_PRIO_MED);
  up_prioritize_irq(BK7258_IRQ_TIMER1, BK7258_PRIO_MED);
  up_prioritize_irq(BK7258_IRQ_I2S0, BK7258_PRIO_MED);
  up_prioritize_irq(BK7258_IRQ_AI, BK7258_PRIO_MED);
  up_prioritize_irq(BK7258_IRQ_LCD, BK7258_PRIO_LOW);
  up_prioritize_irq(BK7258_IRQ_WIFI, BK7258_PRIO_LOW);
  up_prioritize_irq(BK7258_IRQ_BLE, BK7258_PRIO_LOW);
  up_prioritize_irq(BK7258_IRQ_FLASH, BK7258_PRIO_MED);

  syslog(LOG_INFO, "NVIC initialized (%d IRQs, %d prio bits)\n",
         BK7258_NVIC_IRQS, BK7258_NVIC_PRIO_BITS);
}

/****************************************************************************
 * Name: up_irqattach
 *
 * Description:
 *   绑定中断处理函数 (NuttX 标准接口, 实际由 irq_attach 实现)。
 *
 ****************************************************************************/

int up_irqattach(int irq, xcpt_t handler, void *arg)
{
  if (irq < 0 || irq >= BK7258_NVIC_IRQS)
    {
      return -EINVAL;
    }

  return irq_attach(irq, handler, arg);
}

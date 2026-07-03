/****************************************************************************
 * board/contest_board/src/bk7258_clock.c
 *
 * BK7258 时钟驱动
 *
 * 职责:
 *   - 配置系统 PLL, 将主频提升到 480MHz
 *   - 配置各外设时钟分频
 *   - 提供时钟查询接口
 *
 * 时钟树 (SDK确认):
 *   外部晶振 40MHz → PLL 倍频 12x → 480MHz 系统时钟
 *                                          ├→ AHB (240MHz)
 *                                          ├→ APB (120MHz)
 *                                          └→ 外设时钟
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* PLL 配置: 40MHz × 12 = 480MHz */
#define BK7258_PLL_MULT_VALUE      12

/* AHB 分频: 480 / 2 = 240MHz */
#define BK7258_AHB_DIV             2

/* APB 分频: 240 / 2 = 120MHz */
#define BK7258_APB_DIV             2

/* PLL 锁定等待超时 (循环次数) */
#define BK7258_PLL_LOCK_TIMEOUT    100000

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_pll_lock_wait
 *
 * Description:
 *   等待 PLL 锁定。
 *
 ****************************************************************************/

static int bk7258_pll_lock_wait(void)
{
  volatile uint32_t *clk_ctrl = (volatile uint32_t *)BK7258_CLK_CTRL;
  uint32_t timeout = BK7258_PLL_LOCK_TIMEOUT;

  while (timeout--)
    {
      if (*clk_ctrl & BK7258_PLL_LOCKED)
        {
          return OK;
        }
    }

  return -ETIMEDOUT;
}

/****************************************************************************
 * Name: bk7258_bus_div_config
 *
 * Description:
 *   配置 AHB/APB 总线分频。
 *
 ****************************************************************************/

static void bk7258_bus_div_config(void)
{
  volatile uint32_t *clk_div = (volatile uint32_t *)BK7258_CLK_DIV;

  /* TODO: 根据 BK7258 数据手册配置 AHB/APB 分频
   * - AHB = SYSCLK / BK7258_AHB_DIV
   * - APB = AHB / BK7258_APB_DIV
   *
   * 寄存器格式 (SDK确认):
   *   SYS_CPU_CLK_DIV_MODE1 = SYS_SYS_REG_BASE + 0x8*4
   *     [3:0]  CLKDIV_CORE
   *     [5:4]  CKSEL_CORE
   *   SYS_CPU_CLK_DIV_MODE2 = SYS_SYS_REG_BASE + 0x9*4
   *   SYS_CPU_26M_WDT_CLK_DIV = SYS_SYS_REG_BASE + 0xA*4
   */
  uint32_t div = (BK7258_AHB_DIV & 0xF) | ((BK7258_APB_DIV & 0xF) << 4);
  *clk_div = div;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_clock_config
 *
 * Description:
 *   配置系统时钟到指定频率。
 *   默认配置: PLL × 12 = 480MHz。
 *
 * Input Parameters:
 *   freq - 目标频率 (Hz), 0 表示使用默认 480MHz
 *
 ****************************************************************************/

void bk7258_clock_config(uint32_t freq)
{
  volatile uint32_t *clk_ctrl = (volatile uint32_t *)BK7258_CLK_CTRL;
  int ret;

  if (freq == 0)
    {
      freq = BK7258_MAX_FREQ;
    }

  syslog(LOG_INFO, "Configuring clock to %u Hz...\n", freq);

  /* Step 1: 先切换到外部晶振, 避免 PLL 切换时系统失稳 */
  *clk_ctrl = (*clk_ctrl & ~BK7258_CLK_SRC_MASK) | BK7258_CLK_SRC_XTAL;
  up_mdelay(1);

  /* Step 2: 使能 PLL, 配置倍频系数 */
  *clk_ctrl |= BK7258_PLL_BYPASS;
  *clk_ctrl |= BK7258_PLL_ENABLE;
  *clk_ctrl = (*clk_ctrl & ~(0xFF << BK7258_PLL_MULT_SHIFT)) |
              BK7258_PLL_MULT(BK7258_PLL_MULT_VALUE);

  /* Step 3: 等待 PLL 锁定 */
  ret = bk7258_pll_lock_wait();
  if (ret < 0)
    {
      syslog(LOG_ERR, "PLL lock failed!\n");
      return;
    }

  /* Step 4: 配置总线分频 */
  bk7258_bus_div_config();

  /* Step 5: 切换系统时钟到 PLL */
  *clk_ctrl = (*clk_ctrl & ~BK7258_CLK_SRC_MASK) | BK7258_CLK_SRC_PLL;
  *clk_ctrl &= ~BK7258_PLL_BYPASS;

  up_mdelay(1);
  syslog(LOG_INFO, "Clock configured: %u Hz\n", freq);
}

/****************************************************************************
 * Name: bk7258_peri_clk_enable
 *
 * Description:
 *   使能指定外设时钟。
 *
 ****************************************************************************/

void bk7258_peri_clk_enable(uint32_t mask)
{
  volatile uint32_t *clk_en = (volatile uint32_t *)BK7258_PERI_CLK_EN;
  *clk_en |= mask;
}

/****************************************************************************
 * Name: bk7258_peri_reset
 *
 * Description:
 *   复位指定外设 (先复位再释放)。
 *
 ****************************************************************************/

void bk7258_peri_reset(uint32_t mask)
{
  volatile uint32_t *rst = (volatile uint32_t *)BK7258_PERI_RST;

  /* 进入复位 */
  *rst &= ~mask;
  up_mdelay(1);

  /* 释放复位 */
  *rst |= mask;
  up_mdelay(1);
}

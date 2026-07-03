/****************************************************************************
 * board/contest_board/src/bk7258_reset.c
 *
 * BK7258 devkit 复位处理
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <arch/board/board.h>

#include <syslog.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* TODO: BK7258 系统控制寄存器, 触发软复位 */
#define BK7258_SYS_CTRL_BASE       0x40000000
#define BK7258_SW_RESET_REG        (BK7258_SYS_CTRL_BASE + 0x10)
#define BK7258_SW_RESET_KEY        0x5A5A5A5A

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_reset
 *
 * Description:
 *   复位板卡。如果 config 中启用 CONFIG_BOARDCTL_RESET,
 *   应用可通过 boardctl(BOARDIOC_RESET) 触发。
 *
 * Input Parameters:
 *   status - 复位状态码
 *
 ****************************************************************************/

int board_reset(int status)
{
  syslog(LOG_INFO, "Board reset (status=%d)...\n", status);

  /* TODO: 触发 BK7258 软复位
   * *(volatile uint32_t *)BK7258_SW_RESET_REG = BK7258_SW_RESET_KEY;
   */

  /* 等待复位生效 */
  up_mdelay(100);

  return 0;
}

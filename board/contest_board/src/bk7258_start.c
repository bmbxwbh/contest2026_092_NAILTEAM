/****************************************************************************
 * board/contest_board/src/bk7258_start.c
 *
 * BK7258 启动入口 (C 语言部分)
 *
 * 执行顺序:
 *   1. openvela_board_initialize() — 早期硬件初始化 (汇编 _start 调用)
 *   2. nuttx_init() — NuttX 内核初始化 (NuttX 提供)
 *   3. bk7258_bringup() — 外设驱动注册
 *   4. board_app_initialize() — 应用初始化
 *   5. nsh_main() — 进入 NSH 命令行
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <nuttx/mm/mm.h>
#include <nuttx/init.h>
#include <arch/board/board.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

extern int bk7258_bringup(void);
extern int board_app_initialize(uintptr_t arg);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nuttx_board_initialize
 *
 * Description:
 *   NuttX 板级初始化入口, 在 __start 中被调用。
 *   此时堆尚未初始化, 只能做寄存器级初始化。
 *
 ****************************************************************************/

void nuttx_board_initialize(void)
{
  /* 早期硬件初始化: 时钟 + 串口 + 外设时钟 */
  openvela_board_initialize();
}

/****************************************************************************
 * Name: board_late_initialize
 *
 * Description:
 *   板级后期初始化, 在 NSH 启动前调用。
 *   此时堆已初始化, 可以注册驱动、分配内存。
 *
 ****************************************************************************/

void board_late_initialize(void)
{
  int ret;

  /* 注册所有外设驱动 */
  ret = bk7258_bringup();
  if (ret < 0)
    {
      syslog(LOG_ERR, "Board bringup failed: %d\n", ret);
    }

  /* 应用初始化 */
#ifdef CONFIG_BOARD_LATE_INITIALIZE
  ret = board_app_initialize(0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "App initialize failed: %d\n", ret);
    }
#endif
}

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   应用层初始化入口。
 *   启动智能音箱核心服务: 场景识别、AI Agent、米家连接、UI。
 *
 ****************************************************************************/

int board_app_initialize(uintptr_t arg)
{
  syslog(LOG_INFO, "AI Speaker application starting...\n");

  /* TODO: 启动各功能模块
   * - scene_recognition_start()
   * - ai_agent_start()
   * - miot_connector_start()
   * - voice_interaction_start()
   * - lvgl_ui_start()
   */

  return OK;
}

/****************************************************************************
 * board/contest_board/src/bk7258_start.c
 *
 * BK7258 启动入口 (C 语言部分)
 *
 * 按 NuttX 标准初始化流程执行:
 *   1. openvela_board_initialize() — 早期硬件初始化 (汇编 _start 调用)
 *   2. nuttx_init() — NuttX 内核初始化 (NuttX 提供)
 *   3. board_early_initialize()    — L0: IRQ/UART/GPIO (无阻塞/无堆/无总线)
 *   4. board_late_initialize()     — L1-L3: Flash/I2C/SPI/WiFi/LCD (调度器就绪)
 *   5. board_app_initialize()      — L4-L5: AI/PM/WDT (NSH 任务)
 *   6. board_app_finalinitialize() — 需文件系统/KVDB 的驱动 (预留)
 *   7. nsh_main() — 进入 NSH 命令行
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

extern int  bk7258_early_initialize(void);
extern int  bk7258_bringup(void);
extern int  bk7258_app_initialize(void);

#if defined(CONFIG_BOARDCTL_FINALINIT)
extern int  bk7258_final_initialize(void);
#endif

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
 * Name: board_early_initialize
 *
 * Description:
 *   L0 基础初始化: IRQ / UART / GPIO
 *   无阻塞、无堆、无总线操作。仅纯寄存器配置。
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_EARLY_INITIALIZE
void board_early_initialize(void)
{
  int ret;

  ret = bk7258_early_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Early initialize failed: %d\n", ret);
    }
}
#endif

/****************************************************************************
 * Name: board_late_initialize
 *
 * Description:
 *   L1-L3 外设驱动注册, 在调度器就绪后调用。
 *   允许阻塞操作。I2C/SPI 总线初始化在此执行。
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  int ret;

  /* L1-L3: Flash/Timer/I2C/SPI/DMA/PWM → WiFi/BLE → Audio/LCD/Touch/LVGL */

  ret = bk7258_bringup();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Board bringup failed: %d\n", ret);
    }
}
#endif

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   L4-L5 应用级驱动注册: AI 加速器 / TFLite / PM / WDT
 *   NSH 任务上下文, 文件系统尚未挂载。
 *
 ****************************************************************************/

#ifdef CONFIG_BOARDCTL
int board_app_initialize(uintptr_t arg)
{
  int ret;

  /* L4-L5: AI 子系统 + 电源管理 (驱动注册) */

  ret = bk7258_app_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: App initialize failed: %d\n", ret);
    }

  /* 应用层服务启动 (非驱动注册, 属于应用业务逻辑) */

#ifdef CONFIG_CONTEST2026_092_SCENE_RECOGNITION
  syslog(LOG_INFO, "Starting scene recognition service...\n");
#endif

#ifdef CONFIG_CONTEST2026_092_AI_AGENT
  syslog(LOG_INFO, "Starting AI Agent engine...\n");
#endif

#ifdef CONFIG_CONTEST2026_092_MIOT_CONNECTOR
  syslog(LOG_INFO, "Starting Mi-IoT connector...\n");
#endif

#ifdef CONFIG_CONTEST2026_092_VOICE_INTERACTION
  syslog(LOG_INFO, "Starting voice interaction...\n");
#endif

#ifdef CONFIG_BK7258_LCD
  syslog(LOG_INFO, "Starting LVGL UI task...\n");
#endif

  return OK;
}
#endif

/****************************************************************************
 * Name: board_app_finalinitialize
 *
 * Description:
 *   预留: 需要文件系统/KVDB 的驱动注册。
 *   文件系统已挂载, KVDB 已就绪。
 *
 ****************************************************************************/

#if defined(CONFIG_BOARDCTL_FINALINIT)
int board_app_finalinitialize(void)
{
  int ret;

  ret = bk7258_final_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Final initialize failed: %d\n", ret);
    }

  return OK;
}
#endif

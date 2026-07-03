/****************************************************************************
 * board/contest_board/src/bk7258_appinit.c
 *
 * BK7258 devkit 应用初始化
 *
 * 在 NSH 启动后, 系统就绪阶段被调用。
 * 可在此启动智能音箱主应用、AI Agent 等后台任务。
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/board.h>

#include <syslog.h>
#include <spawn.h>
#include <errno.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   应用层初始化入口, 由 NuttX 启动流程在 NSH 就绪前调用。
 *   在此启动智能音箱核心应用:
 *     1. 场景识别服务 (端侧 AI)
 *     2. AI Agent 引擎
 *     3. 米家 IoT 连接器
 *     4. 语音交互
 *     5. LVGL UI 主任务
 *
 ****************************************************************************/

int board_app_initialize(uintptr_t arg)
{
  int ret = OK;

  syslog(LOG_INFO, "AI Speaker app initializing...\n");

#ifdef CONFIG_CONTEST2026_092_SCENE_RECOGNITION
  /* TODO: 启动场景识别服务 */
  syslog(LOG_INFO, "Starting scene recognition service...\n");
#endif

#ifdef CONFIG_CONTEST2026_092_AI_AGENT
  /* TODO: 启动 AI Agent 引擎 */
  syslog(LOG_INFO, "Starting AI Agent engine...\n");
#endif

#ifdef CONFIG_CONTEST2026_092_MIOT_CONNECTOR
  /* TODO: 启动米家 IoT 连接器 */
  syslog(LOG_INFO, "Starting Mi-IoT connector...\n");
#endif

#ifdef CONFIG_CONTEST2026_092_VOICE_INTERACTION
  /* TODO: 启动语音交互 (唤醒词监听) */
  syslog(LOG_INFO, "Starting voice interaction...\n");
#endif

#ifdef CONFIG_BK7258_LCD
  /* TODO: 启动 LVGL UI 主任务 */
  syslog(LOG_INFO, "Starting LVGL UI task...\n");
#endif

  syslog(LOG_INFO, "AI Speaker app initialized.\n");
  return ret;
}

#include <nuttx/config.h>
#include <stdio.h>
#include <syslog.h>
#include <task.h>
#include "lvgl/lvgl.h"
#include "speaker_main.h"
#include "device_state.h"
#include "display_manager.h"
#include "scene_ui.h"
#include "audio_pipeline.h"

extern int scene_recog_task(int argc, char *argv[]);
extern int wakeup_task(int argc, char *argv[]);
extern int anomaly_task(int argc, char *argv[]);
extern int ai_agent_task(int argc, char *argv[]);
extern int iot_hal_task(int argc, char *argv[]);

#ifndef CONFIG_SMART_SPEAKER_TASK_PRIO
#define CONFIG_SMART_SPEAKER_TASK_PRIO 100
#endif

static void start_task(const char *name, main_t entry, int stack)
{
  pid_t pid = task_create(name, CONFIG_SMART_SPEAKER_TASK_PRIO, stack, entry, NULL);
  if (pid < 0) syslog(LOG_ERR, "Failed to start %s\n", name);
  else syslog(LOG_INFO, "Started %s pid=%d\n", name, pid);
}

static void init_display(void)
{
  display_manager_init();
  lv_display_t *disp = lv_display_get_default();
  if (disp)
    {
      int32_t w = lv_display_get_hor_res(disp);
      int32_t h = lv_display_get_ver_res(disp);
      display_manager_register(DISPLAY_PRIMARY, disp, w, h);
    }
  else
    {
      syslog(LOG_WARNING, "no LVGL display, UI disabled\n");
    }
  scene_ui_init();
}

int smart_speaker_main(int argc, char *argv[])
{
  syslog(LOG_INFO, "=== Smart Speaker starting ===\n");

  if (device_state_init() != 0)
    {
      syslog(LOG_ERR, "device_state init failed, abort\n");
      return -1;
    }

  if (!audio_pipeline_init())
    {
      syslog(LOG_ERR, "audio pipeline init failed, abort\n");
      return -1;
    }

  init_display();

  start_task("scene_recog", scene_recog_task, 16384);
  start_task("wakeup", wakeup_task, 12288);
  start_task("anomaly", anomaly_task, 12288);
  start_task("ai_agent", ai_agent_task, 8192);
  start_task("iot_hal", iot_hal_task, 6144);

  syslog(LOG_INFO, "=== Smart Speaker all tasks started ===\n");
  return 0;
}

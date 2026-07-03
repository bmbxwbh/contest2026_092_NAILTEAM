#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include "ai_agent.h"
#include "device_state.h"
#include "iot_hal.h"

typedef enum { ACT_NONE, ACT_DEVICE_ON, ACT_DEVICE_OFF, ACT_DEVICE_SET, ACT_REPORT_CLOUD, ACT_ALERT_LOCAL } action_type_t;

typedef struct { action_type_t type; char device[24]; int32_t value; } action_t;
#define MAX_ACTIONS 6

static int build_actions(scene_type_t scene, anomaly_type_t anom, int hour, action_t *out, int max)
{
  int n = 0;
  if (anom != ANOMALY_NONE)
    {
      out[n++] = (action_t){ACT_ALERT_LOCAL, "", (int)anom};
      out[n++] = (action_t){ACT_REPORT_CLOUD, "anomaly", (int)anom};
      return n;
    }
  switch (scene)
    {
      case SCENE_COOKING:
        out[n++] = (action_t){ACT_DEVICE_ON, "range_hood", 0};
        out[n++] = (action_t){ACT_DEVICE_ON, "kitchen_light", 80};
        out[n++] = (action_t){ACT_REPORT_CLOUD, "scene", (int)scene};
        break;
      case SCENE_BATHING:
        out[n++] = (action_t){ACT_DEVICE_SET, "bath_light", 40};
        out[n++] = (action_t){ACT_DEVICE_SET, "bath_heater", 28};
        out[n++] = (action_t){ACT_REPORT_CLOUD, "scene", (int)scene};
        break;
      case SCENE_MOVIE:
        out[n++] = (action_t){ACT_DEVICE_SET, "living_light", 20};
        out[n++] = (action_t){ACT_DEVICE_SET, "ac", 24};
        break;
      case SCENE_SLEEP:
        out[n++] = (action_t){ACT_DEVICE_OFF, "living_light", 0};
        out[n++] = (action_t){ACT_DEVICE_SET, "ac", 26};
        break;
      case SCENE_BABY_CRY:
        out[n++] = (action_t){ACT_DEVICE_SET, "night_light", 15};
        out[n++] = (action_t){ACT_REPORT_CLOUD, "baby_cry", (int)scene};
        break;
      case SCENE_WASHING:
        out[n++] = (action_t){ACT_REPORT_CLOUD, "appliance", (int)scene};
        break;
      case SCENE_VACUUM:
        out[n++] = (action_t){ACT_DEVICE_SET, "living_light", 60};
        break;
      case SCENE_DISHWASH:
        out[n++] = (action_t){ACT_DEVICE_SET, "kitchen_light", 50};
        break;
      case SCENE_KNOCK:
        out[n++] = (action_t){ACT_DEVICE_SET, "door_light", 80};
        out[n++] = (action_t){ACT_REPORT_CLOUD, "knock", (int)scene};
        break;
      case SCENE_PET:
        out[n++] = (action_t){ACT_DEVICE_SET, "living_light", 30};
        out[n++] = (action_t){ACT_REPORT_CLOUD, "pet", (int)scene};
        break;
      default: break;
    }
  if (hour >= 23 || hour < 6)
    if (n < max) out[n++] = (action_t){ACT_DEVICE_OFF, "tv", 0};
  return n;
}

bool ai_agent_init(void) { syslog(LOG_INFO, "AI Agent engine ready\n"); return true; }

void ai_agent_tick(void)
{
  if (!device_state_get_auto_mode()) return;
  scene_type_t scene; float conf;
  device_state_get_scene(&scene, &conf, NULL, 0);
  anomaly_type_t anom = device_state_get_anomaly();
  time_t now = time(NULL);
  int hour = localtime(&now)->tm_hour;

  action_t acts[MAX_ACTIONS];
  int n = build_actions(scene, anom, hour, acts, MAX_ACTIONS);
  for (int i = 0; i < n; i++)
    {
      switch (acts[i].type)
        {
          case ACT_DEVICE_ON:
            iot_hal_set_power(acts[i].device, true);
            iot_hal_set_brightness(acts[i].device, acts[i].value);
            break;
          case ACT_DEVICE_OFF:
            iot_hal_set_power(acts[i].device, false);
            break;
          case ACT_DEVICE_SET:
            if (strstr(acts[i].device, "light")) iot_hal_set_brightness(acts[i].device, acts[i].value);
            else iot_hal_set_temperature(acts[i].device, acts[i].value);
            break;
          case ACT_ALERT_LOCAL:  iot_hal_local_alert((anomaly_type_t)acts[i].value); break;
          case ACT_REPORT_CLOUD: iot_hal_report_cloud(acts[i].device, acts[i].value); break;
          default: break;
        }
    }
}

int ai_agent_task(int argc, char *argv[])
{
  ai_agent_init();
  while (1) { ai_agent_tick(); sleep(2); }
  return 0;
}

#include <nuttx/config.h>
#include <string.h>
#include <syslog.h>
#include "device_state.h"
#include "speaker_main.h"

static device_state_t g_state;

static const char *g_scene_names[SCENE_COUNT] =
{ "无", "烹饪", "沐浴", "观影", "睡眠", "安静", "对话",
  "婴儿啼哭", "洗衣", "吸尘", "洗碗", "敲门", "宠物活动", "其他" };
static const char *g_anomaly_names[ANOMALY_COUNT] =
{ "无异常", "玻璃破碎", "尖叫", "烟雾报警", "跌倒", "咳嗽持续", "漏水" };

int device_state_init(void)
{
  pthread_mutex_init(&g_state.lock, NULL);
  g_state.current_scene = SCENE_NONE;
  g_state.scene_confidence = 0.0f;
  g_state.anomaly = ANOMALY_NONE;
  g_state.device_count = 0;
  g_state.agent_auto_mode = true;
  return 0;
}

void device_state_set_scene(scene_type_t s, float confidence)
{
  pthread_mutex_lock(&g_state.lock);
  g_state.current_scene = s;
  g_state.scene_confidence = confidence;
  strncpy(g_state.scene_name, scene_type_name(s), SCENE_NAME_MAX - 1);
  g_state.scene_name[SCENE_NAME_MAX - 1] = '\0';
  pthread_mutex_unlock(&g_state.lock);
}

void device_state_get_scene(scene_type_t *s, float *confidence,
                            char *name, int namelen)
{
  pthread_mutex_lock(&g_state.lock);
  if (s) *s = g_state.current_scene;
  if (confidence) *confidence = g_state.scene_confidence;
  if (name) { strncpy(name, g_state.scene_name, namelen - 1); name[namelen - 1] = '\0'; }
  pthread_mutex_unlock(&g_state.lock);
}

void device_state_set_wakeup(bool detected)
{
  pthread_mutex_lock(&g_state.lock);
  g_state.wakeup_detected = detected;
  if (detected) g_state.last_wakeup_tick = clock_gettime_monotonic_ms();
  pthread_mutex_unlock(&g_state.lock);
}

bool device_state_get_wakeup(void)
{
  bool v;
  pthread_mutex_lock(&g_state.lock); v = g_state.wakeup_detected; pthread_mutex_unlock(&g_state.lock);
  return v;
}

void device_state_set_anomaly(anomaly_type_t a)
{
  pthread_mutex_lock(&g_state.lock);
  g_state.anomaly = a; g_state.anomaly_tick = clock_gettime_monotonic_ms();
  pthread_mutex_unlock(&g_state.lock);
}

anomaly_type_t device_state_get_anomaly(void)
{
  anomaly_type_t a;
  pthread_mutex_lock(&g_state.lock); a = g_state.anomaly; pthread_mutex_unlock(&g_state.lock);
  return a;
}

int device_state_add_device(const char *name)
{
  int idx = -1;
  pthread_mutex_lock(&g_state.lock);
  if (g_state.device_count < MAX_DEVICES)
    {
      idx = g_state.device_count;
      strncpy(g_state.devices[idx].name, name, DEVICE_NAME_MAX - 1);
      g_state.devices[idx].name[DEVICE_NAME_MAX - 1] = '\0';
      g_state.devices[idx].online = true;
      g_state.devices[idx].on = false;
      g_state.devices[idx].brightness = 0;
      g_state.devices[idx].temperature = 24;
      g_state.device_count++;
    }
  pthread_mutex_unlock(&g_state.lock);
  return idx;
}

device_entry_t *device_state_get_devices(int *count)
{
  pthread_mutex_lock(&g_state.lock);
  if (count) *count = g_state.device_count;
  pthread_mutex_unlock(&g_state.lock);
  return g_state.devices;
}

void device_state_set_auto_mode(bool on)
{ pthread_mutex_lock(&g_state.lock); g_state.agent_auto_mode = on; pthread_mutex_unlock(&g_state.lock); }

bool device_state_get_auto_mode(void)
{ bool v; pthread_mutex_lock(&g_state.lock); v = g_state.agent_auto_mode; pthread_mutex_unlock(&g_state.lock); return v; }

const char *scene_type_name(scene_type_t s)
{ if (s >= 0 && s < SCENE_COUNT) return g_scene_names[s]; return "未知"; }

const char *anomaly_type_name(anomaly_type_t a)
{ if (a >= 0 && a < ANOMALY_COUNT) return g_anomaly_names[a]; return "未知"; }

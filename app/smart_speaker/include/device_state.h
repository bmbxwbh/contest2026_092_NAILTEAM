#ifndef __APPS_SMART_SPEAKER_INCLUDE_DEVICE_STATE_H
#define __APPS_SMART_SPEAKER_INCLUDE_DEVICE_STATE_H

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#define SCENE_NAME_MAX  24
#define DEVICE_NAME_MAX 24

typedef enum
{
  SCENE_NONE = 0,
  SCENE_COOKING,     /* 烹饪 */
  SCENE_BATHING,     /* 沐浴 */
  SCENE_MOVIE,       /* 观影 */
  SCENE_SLEEP,       /* 睡眠 */
  SCENE_QUIET,       /* 安静 */
  SCENE_TALK,        /* 对话 */
  SCENE_BABY_CRY,    /* 婴儿啼哭 */
  SCENE_WASHING,     /* 洗衣 */
  SCENE_VACUUM,      /* 吸尘 */
  SCENE_DISHWASH,    /* 洗碗 */
  SCENE_KNOCK,       /* 敲门 */
  SCENE_PET,         /* 宠物活动 */
  SCENE_OTHER,
  SCENE_COUNT
} scene_type_t;

typedef enum
{
  ANOMALY_NONE = 0,
  ANOMALY_GLASS_BREAK,
  ANOMALY_SCREAM,
  ANOMALY_SMOKE_ALARM,
  ANOMALY_FALL,       /* 跌倒 */
  ANOMALY_COUGH,      /* 咳嗽持续 */
  ANOMALY_LEAK,       /* 漏水 */
  ANOMALY_COUNT
} anomaly_type_t;

typedef struct
{
  char     name[DEVICE_NAME_MAX];
  bool     online;
  bool     on;
  int32_t  brightness;   /* 0-100, 灯 */
  int32_t  temperature;  /* 空调温度 */
} device_entry_t;

#define MAX_DEVICES 8

typedef struct
{
  pthread_mutex_t lock;
  scene_type_t   current_scene;
  float          scene_confidence;
  char           scene_name[SCENE_NAME_MAX];
  bool           wakeup_detected;
  uint32_t       last_wakeup_tick;
  anomaly_type_t anomaly;
  uint32_t       anomaly_tick;
  device_entry_t devices[MAX_DEVICES];
  int            device_count;
  bool           agent_auto_mode;
} device_state_t;

int  device_state_init(void);
void device_state_set_scene(scene_type_t s, float confidence);
void device_state_get_scene(scene_type_t *s, float *confidence, char *name, int namelen);
void device_state_set_wakeup(bool detected);
bool device_state_get_wakeup(void);
void device_state_set_anomaly(anomaly_type_t a);
anomaly_type_t device_state_get_anomaly(void);
int  device_state_add_device(const char *name);
int  device_state_get_devices(device_entry_t *out, int max, int *count);
int  device_state_find_device(const char *name);
bool device_state_update_device(int idx, bool on, int brightness, int temperature);
void device_state_set_auto_mode(bool on);
bool device_state_get_auto_mode(void);
const char *scene_type_name(scene_type_t s);
const char *anomaly_type_name(anomaly_type_t a);

#endif

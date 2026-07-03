#include <nuttx/config.h>
#include <string.h>
#include <syslog.h>
#include "iot_hal.h"
#include "device_state.h"

static device_entry_t *find_device(const char *name)
{
  int cnt; device_entry_t *devs = device_state_get_devices(&cnt);
  for (int i = 0; i < cnt; i++)
    if (strcmp(devs[i].name, name) == 0) return &devs[i];
  return NULL;
}

bool iot_local_sim_init(void) { syslog(LOG_INFO, "IoT local-sim backend\n"); return true; }

bool iot_local_sim_set_power(const char *d, bool on)
{
  device_entry_t *p = find_device(d);
  if (p) { p->on = on; syslog(LOG_INFO, "[SIM] %s power=%d\n", d, on); return true; }
  return false;
}
bool iot_local_sim_set_brightness(const char *d, int v)
{
  device_entry_t *p = find_device(d);
  if (p) { p->brightness = v; p->on = (v > 0); syslog(LOG_INFO, "[SIM] %s bright=%d\n", d, v); return true; }
  return false;
}
bool iot_local_sim_set_temperature(const char *d, int t)
{
  device_entry_t *p = find_device(d);
  if (p) { p->temperature = t; syslog(LOG_INFO, "[SIM] %s temp=%d\n", d, t); return true; }
  return false;
}
bool iot_local_sim_alert(anomaly_type_t a)
{
  syslog(LOG_WARNING, "[SIM][ALERT] %s (buzzer on)\n", anomaly_type_name(a));
  /* 触发蜂鸣器(底层 PWM 驱动); 此处仅日志 */
  return true;
}
bool iot_local_sim_report(const char *topic, int v)
{
  syslog(LOG_INFO, "[SIM][CLOUD] %s=%d (queued)\n", topic, v);
  /* 由云端上报任务(M9 对接)异步发送 */
  return true;
}

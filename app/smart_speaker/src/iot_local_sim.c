#include <nuttx/config.h>
#include <string.h>
#include <syslog.h>
#include "iot_hal.h"
#include "device_state.h"

static int find_device_idx(const char *name)
{
  return device_state_find_device(name);
}

bool iot_local_sim_init(void) { syslog(LOG_INFO, "IoT local-sim backend\n"); return true; }

bool iot_local_sim_set_power(const char *d, bool on)
{
  int idx = find_device_idx(d);
  if (idx >= 0) { device_state_update_device(idx, on, -1, -1); syslog(LOG_INFO, "[SIM] %s power=%d\n", d, on); return true; }
  return false;
}
bool iot_local_sim_set_brightness(const char *d, int v)
{
  int idx = find_device_idx(d);
  if (idx >= 0) { device_state_update_device(idx, v > 0, v, -1); syslog(LOG_INFO, "[SIM] %s bright=%d\n", d, v); return true; }
  return false;
}
bool iot_local_sim_set_temperature(const char *d, int t)
{
  int idx = find_device_idx(d);
  if (idx >= 0) { device_state_update_device(idx, -1, -1, t); syslog(LOG_INFO, "[SIM] %s temp=%d\n", d, t); return true; }
  return false;
}
bool iot_local_sim_alert(anomaly_type_t a)
{
  syslog(LOG_WARNING, "[SIM][ALERT] %s (buzzer on)\n", anomaly_type_name(a));
  return true;
}
bool iot_local_sim_report(const char *topic, int v)
{
  syslog(LOG_INFO, "[SIM][CLOUD] %s=%d (queued)\n", topic, v);
  return true;
}

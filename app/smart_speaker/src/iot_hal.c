#include <nuttx/config.h>
#include <string.h>
#include <syslog.h>
#include "iot_hal.h"
#include "device_state.h"

static iot_backend_t g_backend = IOT_BACKEND_LOCAL_SIM;

bool iot_hal_init(iot_backend_t backend)
{
  g_backend = backend;
  return (backend == IOT_BACKEND_LOCAL_SIM) ? iot_local_sim_init() : iot_miot_stub_init();
}
bool iot_hal_set_power(const char *d, bool on)
{ return g_backend == IOT_BACKEND_LOCAL_SIM ? iot_local_sim_set_power(d,on) : iot_miot_stub_set_power(d,on); }
bool iot_hal_set_brightness(const char *d, int v)
{ return g_backend == IOT_BACKEND_LOCAL_SIM ? iot_local_sim_set_brightness(d,v) : iot_miot_stub_set_brightness(d,v); }
bool iot_hal_set_temperature(const char *d, int t)
{ return g_backend == IOT_BACKEND_LOCAL_SIM ? iot_local_sim_set_temperature(d,t) : iot_miot_stub_set_temperature(d,t); }
bool iot_hal_local_alert(anomaly_type_t a)
{ return g_backend == IOT_BACKEND_LOCAL_SIM ? iot_local_sim_alert(a) : iot_miot_stub_alert(a); }
bool iot_hal_report_cloud(const char *topic, int v)
{ return g_backend == IOT_BACKEND_LOCAL_SIM ? iot_local_sim_report(topic,v) : iot_miot_stub_report(topic,v); }

int iot_hal_task(int argc, char *argv[])
{
#ifdef CONFIG_SMART_SPEAKER_IOT_MIOT
  iot_hal_init(IOT_BACKEND_MIOT);
#else
  iot_hal_init(IOT_BACKEND_LOCAL_SIM);
#endif
  device_state_add_device("kitchen_light");
  device_state_add_device("range_hood");
  device_state_add_device("living_light");
  device_state_add_device("ac");
  device_state_add_device("tv");
  device_state_add_device("night_light");
  device_state_add_device("door_light");
  syslog(LOG_INFO, "IoT HAL ready (backend=%d)\n", g_backend);
  while (1) usleep(500000);
  return 0;
}

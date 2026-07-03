#include <nuttx/config.h>
#include <string.h>
#include <syslog.h>
#include "iot_hal.h"

/* 米家 IoT 开放协议 stub. 接口签名按小米 IoT 开发者平台 spec 预留:
 *   - 设备控制: set_properties / action (JSON over HTTP/MQTT)
 *   - 状态上报: event 上报
 * 函数体暂为日志 + 返回成功, 等 SDK 与营业执照到位后填充真实调用.
 * 上层(Agent/UI)只依赖 iot_hal 统一接口, 切换后端零改动. */

bool iot_miot_stub_init(void)
{
  syslog(LOG_INFO, "[MIOT-STUB] init (接口预留, 待营业执照+SDK接入)\n");
  /* TODO: 加载 miot_device_id/secret from /mnt/userdata/miot.conf */
  return true;
}
bool iot_miot_stub_set_power(const char *d, bool on)
{ syslog(LOG_INFO, "[MIOT-STUB] set_properties %s.power=%d\n", d, on); return true; /* TODO: miot_client_set_property */ }
bool iot_miot_stub_set_brightness(const char *d, int v)
{ syslog(LOG_INFO, "[MIOT-STUB] set_properties %s.brightness=%d\n", d, v); return true; }
bool iot_miot_stub_set_temperature(const char *d, int t)
{ syslog(LOG_INFO, "[MIOT-STUB] set_properties %s.target_temp=%d\n", d, t); return true; }
bool iot_miot_stub_alert(anomaly_type_t a)
{ syslog(LOG_WARNING, "[MIOT-STUB] event anomaly=%d (待推送米家APP)\n", a); return true; /* TODO: report_event */ }
bool iot_miot_stub_report(const char *topic, int v)
{ syslog(LOG_INFO, "[MIOT-STUB] event %s=%d\n", topic, v); return true; }

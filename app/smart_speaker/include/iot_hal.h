#ifndef __IOT_HAL_H
#define __IOT_HAL_H

#include <nuttx/config.h>
#include <stdbool.h>
#include "device_state.h"

typedef enum { IOT_BACKEND_LOCAL_SIM = 0, IOT_BACKEND_MIOT } iot_backend_t;

bool iot_hal_init(iot_backend_t backend);

bool iot_hal_set_power(const char *device, bool on);
bool iot_hal_set_brightness(const char *device, int value);   /* 0-100 */
bool iot_hal_set_temperature(const char *device, int temp);   /* 摄氏度 */
bool iot_hal_local_alert(anomaly_type_t a);
bool iot_hal_report_cloud(const char *topic, int value);

/* 后端实现 */
bool iot_local_sim_init(void);
bool iot_local_sim_set_power(const char *d, bool on);
bool iot_local_sim_set_brightness(const char *d, int v);
bool iot_local_sim_set_temperature(const char *d, int t);
bool iot_local_sim_alert(anomaly_type_t a);
bool iot_local_sim_report(const char *topic, int v);

bool iot_miot_stub_init(void);
bool iot_miot_stub_set_power(const char *d, bool on);
bool iot_miot_stub_set_brightness(const char *d, int v);
bool iot_miot_stub_set_temperature(const char *d, int t);
bool iot_miot_stub_alert(anomaly_type_t a);
bool iot_miot_stub_report(const char *topic, int v);

#endif

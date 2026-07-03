#ifndef __ANOMALY_H
#define __ANOMALY_H

#include <nuttx/config.h>
#include <stdbool.h>
#include "device_state.h"

bool anomaly_init(const char *model_path);
bool anomaly_detect(const int16_t *pcm_500ms, int n, anomaly_type_t *out);

#endif

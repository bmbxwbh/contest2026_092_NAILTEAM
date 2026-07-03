#ifndef __WAKEUP_H
#define __WAKEUP_H

#include <nuttx/config.h>
#include <stdbool.h>

bool wakeup_init(const char *model_path);
bool wakeup_detect(const int16_t *pcm_frame, int n);

#endif

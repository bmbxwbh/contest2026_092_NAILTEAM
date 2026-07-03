#ifndef __SCENE_RECOG_H
#define __SCENE_RECOG_H

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>
#include "device_state.h"

bool scene_recog_init(const char *model_path);
bool scene_recog_infer(const int16_t *pcm_1s, int len,
                       scene_type_t *out_scene, float *out_conf);

#endif

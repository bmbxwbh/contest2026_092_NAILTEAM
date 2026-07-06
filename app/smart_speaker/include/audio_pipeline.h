#ifndef __AUDIO_PIPELINE_H
#define __AUDIO_PIPELINE_H

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>

#define AUDIO_SRATE       16000
#define AUDIO_CHANNELS    1
#define AUDIO_BPS         16
#define FRAME_MS          20
#define FRAME_SAMPLES     (AUDIO_SRATE * FRAME_MS / 1000)
#define FRAME_BYTES       (FRAME_SAMPLES * (AUDIO_BPS / 8) * AUDIO_CHANNELS)
#define MONO_FRAME_SAMPLES FRAME_SAMPLES

bool audio_pipeline_init(void);
bool audio_pipeline_read_frame(int16_t *out_mono, int n_samples);
uint32_t audio_pipeline_frame_count(void);

#endif

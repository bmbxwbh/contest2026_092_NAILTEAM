#ifndef __MFCC_H
#define __MFCC_H

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>

#define MFCC_NUM_FILTERS 40
#define MFCC_NUM_CEPS    13
#define MFCC_FFT_SIZE    256
#define MFCC_FRAME_LEN   400
#define MFCC_FRAME_STEP  160

bool mfcc_compute(const int16_t *pcm, int pcm_len, int8_t *out_mfcc);
int  mfcc_compute_spectrogram(const int16_t *pcm_1s, int len_1s,
                              int8_t *out_buf, int buf_size);

#endif

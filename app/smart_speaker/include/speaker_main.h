#ifndef __APPS_SMART_SPEAKER_INCLUDE_SPEAKER_MAIN_H
#define __APPS_SMART_SPEAKER_INCLUDE_SPEAKER_MAIN_H

#include <nuttx/config.h>
#include <stdint.h>
#include <time.h>

static inline uint32_t clock_gettime_monotonic_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int smart_speaker_main(int argc, char *argv[]);

#endif

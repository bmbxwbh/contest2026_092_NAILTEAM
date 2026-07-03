#include <nuttx/config.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include "audio_pipeline.h"

static int g_fd = -1;
static uint32_t g_frame_count = 0;

bool audio_pipeline_init(void)
{
  g_fd = open("/dev/audio0", O_RDONLY);
  if (g_fd < 0) { syslog(LOG_ERR, "open /dev/audio0 failed: %d\n", errno); return false; }
  syslog(LOG_INFO, "audio pipeline ready\n");
  return true;
}

bool audio_pipeline_read_frame(int16_t *out_mono, int n_samples)
{
  if (g_fd < 0) return false;
  int16_t raw[FRAME_BYTES / 2];
  ssize_t n = read(g_fd, raw, FRAME_BYTES);
  if (n != FRAME_BYTES) return false;
  int copy = n_samples < MONO_FRAME_SAMPLES ? n_samples : MONO_FRAME_SAMPLES;
  for (int i = 0; i < copy; i++)
    {
      int16_t l = raw[i * 2];
      int16_t r = raw[i * 2 + 1];
      out_mono[i] = (int16_t)(((int)l + (int)r) / 2);
    }
  g_frame_count++;
  return true;
}

uint32_t audio_pipeline_frame_count(void) { return g_frame_count; }

int audio_pipeline_task(int argc, char *argv[])
{
  if (!audio_pipeline_init()) return -1;
  while (1) usleep(100000);  /* 采集由消费者驱动; 本任务保活 */
  return 0;
}

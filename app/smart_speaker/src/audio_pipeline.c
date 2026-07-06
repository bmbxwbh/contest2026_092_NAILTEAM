#include <nuttx/config.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <errno.h>

#ifdef CONFIG_AUDIO
#include <nuttx/audio/audio.h>
#endif

#include "audio_pipeline.h"

#ifndef CONFIG_SMART_SPEAKER_AUDIO_DEV_CAPTURE
#define CONFIG_SMART_SPEAKER_AUDIO_DEV_CAPTURE "/dev/audio/pcm0c"
#endif

static int g_fd = -1;
static uint32_t g_frame_count = 0;

bool audio_pipeline_init(void)
{
#ifdef CONFIG_AUDIO
  g_fd = open(CONFIG_SMART_SPEAKER_AUDIO_DEV_CAPTURE, O_RDWR | O_CLOEXEC);
  if (g_fd < 0)
    {
      syslog(LOG_ERR, "open %s failed: %d\n", CONFIG_SMART_SPEAKER_AUDIO_DEV_CAPTURE, errno);
      return false;
    }

  struct audio_caps_desc_s cap_desc;
  memset(&cap_desc, 0, sizeof(cap_desc));
  cap_desc.caps.ac_len              = sizeof(struct audio_caps_s);
  cap_desc.caps.ac_type             = AUDIO_TYPE_INPUT;
  cap_desc.caps.ac_channels         = AUDIO_CHANNELS;
  cap_desc.caps.ac_chmap            = 0;
  cap_desc.caps.ac_samplerate.lower = AUDIO_SRATE;
  cap_desc.caps.ac_samplerate.upper = AUDIO_SRATE;
  cap_desc.caps.ac_controls.b[0]    = AUDIO_BPS;

  if (ioctl(g_fd, AUDIOIOC_CONFIGURE, (unsigned long)(uintptr_t)&cap_desc) < 0)
    {
      syslog(LOG_ERR, "audio configure failed: %d\n", errno);
      close(g_fd);
      g_fd = -1;
      return false;
    }

  syslog(LOG_INFO, "audio pipeline ready: %s @%dHz %dch %dbit\n",
         CONFIG_SMART_SPEAKER_AUDIO_DEV_CAPTURE, AUDIO_SRATE, AUDIO_CHANNELS, AUDIO_BPS);
  return true;
#else
  syslog(LOG_WARNING, "audio subsystem disabled (CONFIG_AUDIO off), using stub\n");
  return true;
#endif
}

bool audio_pipeline_read_frame(int16_t *out_mono, int n_samples)
{
  if (g_fd < 0)
    {
      if (out_mono && n_samples > 0) memset(out_mono, 0, n_samples * sizeof(int16_t));
      return false;
    }

  int16_t buf[FRAME_SAMPLES];
  ssize_t n = read(g_fd, buf, FRAME_BYTES);
  if (n != FRAME_BYTES)
    {
      syslog(LOG_WARNING, "audio short read: %zd/%d\n", n, FRAME_BYTES);
      return false;
    }

  int copy = n_samples < FRAME_SAMPLES ? n_samples : FRAME_SAMPLES;
  memcpy(out_mono, buf, copy * sizeof(int16_t));
  g_frame_count++;
  return true;
}

uint32_t audio_pipeline_frame_count(void) { return g_frame_count; }

int audio_pipeline_task(int argc, char *argv[])
{
  if (!audio_pipeline_init()) return -1;
  return 0;
}

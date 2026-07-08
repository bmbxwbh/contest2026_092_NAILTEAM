/****************************************************************************
 * AI Scene-Aware Smart Speaker - Audio Preprocessing
 *
 * Uses SpeexDSP for noise reduction, AGC, and AEC.
 * Falls back to simple processing if SpeexDSP unavailable.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <nuttx/audio/audio.h>

#ifdef CONFIG_AUDIOUTILS_SPEEXDSP
#include <speex/speex_preprocess.h>
#endif

#include "scene_aware_speaker.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define AUDIO_DEV_CAPTURE    "/dev/audio/pcm0c"
#define AUDIO_DEV_PLAYBACK   "/dev/audio/pcm0p"

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_AUDIO
static int g_cap_fd = -1;
static int g_play_fd = -1;
#endif

#ifdef CONFIG_AUDIOUTILS_SPEEXDSP
static SpeexPreprocessState *g_st = NULL;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: audio_configure
 *
 * Description:
 *   Configure audio device with specified parameters.
 *
 ****************************************************************************/

#ifdef CONFIG_AUDIO
static int audio_configure(int fd, bool is_capture)
{
  struct audio_caps_desc_s cap_desc;
  int ret;

  memset(&cap_desc, 0, sizeof(cap_desc));
  cap_desc.caps.ac_len = sizeof(struct audio_caps_s);
  cap_desc.caps.ac_type = is_capture ? AUDIO_TYPE_INPUT : AUDIO_TYPE_OUTPUT;
  cap_desc.caps.ac_channels = AUDIO_CHANNELS;
  cap_desc.caps.ac_samplerate.lower = AUDIO_SAMPLE_RATE;
  cap_desc.caps.ac_samplerate.upper = AUDIO_SAMPLE_RATE;
  cap_desc.caps.ac_controls.b[0] = 16; /* 16-bit */

  ret = ioctl(fd, AUDIOIOC_CONFIGURE,
              (unsigned long)(uintptr_t)&cap_desc);
  if (ret < 0)
    {
      fprintf(stderr, "[AUDIO] Configure failed: %d\n", errno);
      return -errno;
    }

  return 0;
}
#endif

/****************************************************************************
 * Name: speexdsp_init
 *
 * Description:
 *   Initialize SpeexDSP preprocessor for noise reduction and AGC.
 *
 ****************************************************************************/

#ifdef CONFIG_AUDIOUTILS_SPEEXDSP
static int speexdsp_init(void)
{
  int frame_size = AUDIO_FRAME_SAMPLES;
  int sampling_rate = AUDIO_SAMPLE_RATE;

  g_st = speex_preprocess_state_init(frame_size, sampling_rate);
  if (!g_st)
    {
      fprintf(stderr, "[AUDIO] SpeexDSP init failed\n");
      return -ENOMEM;
    }

  /* Enable noise reduction */

  int denoise = 1;
  speex_preprocess_ctl(g_st, SPEEX_PREPROCESS_SET_DENOISE, &denoise);

  /* Set noise suppression level */

  int noise_suppress = -25; /* dB */
  speex_preprocess_ctl(g_st,
                       SPEEX_PREPROCESS_SET_NOISE_SUPPRESS,
                       &noise_suppress);

  /* Enable automatic gain control */

  int agc = 1;
  speex_preprocess_ctl(g_st, SPEEX_PREPROCESS_SET_AGC, &agc);

  /* Set AGC level */

  int agc_level = 24000; /* Target level */
  speex_preprocess_ctl(g_st, SPEEX_PREPROCESS_SET_AGC_LEVEL, &agc_level);

  /* Enable voice activity detection */

  int vad = 1;
  speex_preprocess_ctl(g_st, SPEEX_PREPROCESS_SET_VAD, &vad);

  printf("[AUDIO] SpeexDSP initialized: denoise=%d, agc=%d, vad=%d\n",
         denoise, agc, vad);

  return 0;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: audio_preprocess_init
 *
 * Description:
 *   Initialize audio capture and playback devices.
 *   Initialize SpeexDSP if available.
 *
 ****************************************************************************/

int audio_preprocess_init(void)
{
  int ret;

#ifdef CONFIG_AUDIO
  /* Open capture device */

  g_cap_fd = open(AUDIO_DEV_CAPTURE, O_RDWR | O_CLOEXEC);
  if (g_cap_fd < 0)
    {
      fprintf(stderr, "[AUDIO] Failed to open %s: %d\n",
              AUDIO_DEV_CAPTURE, errno);
      return -errno;
    }

  ret = audio_configure(g_cap_fd, true);
  if (ret < 0)
    {
      close(g_cap_fd);
      g_cap_fd = -1;
      return ret;
    }

  /* Open playback device */

  g_play_fd = open(AUDIO_DEV_PLAYBACK, O_RDWR | O_CLOEXEC);
  if (g_play_fd < 0)
    {
      fprintf(stderr, "[AUDIO] Failed to open %s: %d\n",
              AUDIO_DEV_PLAYBACK, errno);
      close(g_cap_fd);
      g_cap_fd = -1;
      return -errno;
    }

  ret = audio_configure(g_play_fd, false);
  if (ret < 0)
    {
      close(g_cap_fd);
      close(g_play_fd);
      g_cap_fd = -1;
      g_play_fd = -1;
      return ret;
    }

  printf("[AUDIO] Devices opened: cap=%s play=%s\n",
         AUDIO_DEV_CAPTURE, AUDIO_DEV_PLAYBACK);
#else
  printf("[AUDIO] CONFIG_AUDIO disabled, using mock audio\n");
#endif

  /* Initialize SpeexDSP */

#ifdef CONFIG_AUDIOUTILS_SPEEXDSP
  ret = speexdsp_init();
  if (ret < 0)
    {
      fprintf(stderr, "[AUDIO] SpeexDSP init failed, "
              "continuing without preprocessing\n");
    }
#else
  printf("[AUDIO] SpeexDSP not available\n");
#endif

  return 0;
}

/****************************************************************************
 * Name: audio_preprocess_read
 *
 * Description:
 *   Read audio frame from capture device.
 *   Apply SpeexDSP preprocessing if available.
 *
 ****************************************************************************/

int audio_preprocess_read(uint8_t *buf, int size)
{
  int nread = 0;

#ifdef CONFIG_AUDIO
  if (g_cap_fd < 0)
    {
      /* No device, return silence */

      memset(buf, 0, size);
      return size;
    }

  nread = read(g_cap_fd, buf, size);
  if (nread < 0)
    {
      fprintf(stderr, "[AUDIO] Read error: %d\n", errno);
      return -errno;
    }
#else
  /* No audio device, return silence */

  memset(buf, 0, size);
  nread = size;
#endif

  /* Apply SpeexDSP preprocessing */

#ifdef CONFIG_AUDIOUTILS_SPEEXDSP
  if (g_st && nread > 0)
    {
      int16_t *samples = (int16_t *)buf;
      int num_samples = nread / sizeof(int16_t);

      speex_preprocess_run(g_st, samples);
    }
#endif

  return nread;
}

/****************************************************************************
 * Name: audio_preprocess_write
 *
 * Description:
 *   Write audio frame to playback device.
 *
 ****************************************************************************/

int audio_preprocess_write(const uint8_t *buf, int size)
{
#ifdef CONFIG_AUDIO
  if (g_play_fd < 0)
    {
      return size; /* Simulate success */
    }

  ssize_t nwritten = write(g_play_fd, buf, size);
  if (nwritten < 0)
    {
      fprintf(stderr, "[AUDIO] Write error: %d\n", errno);
      return -errno;
    }

  return (int)nwritten;
#else
  return size;
#endif
}

/****************************************************************************
 * Name: audio_preprocess_cleanup
 *
 * Description:
 *   Close audio devices and cleanup SpeexDSP.
 *
 ****************************************************************************/

void audio_preprocess_cleanup(void)
{
#ifdef CONFIG_AUDIOUTILS_SPEEXDSP
  if (g_st)
    {
      speex_preprocess_state_destroy(g_st);
      g_st = NULL;
      printf("[AUDIO] SpeexDSP cleaned up\n");
    }
#endif

#ifdef CONFIG_AUDIO
  if (g_cap_fd >= 0)
    {
      close(g_cap_fd);
      g_cap_fd = -1;
    }

  if (g_play_fd >= 0)
    {
      close(g_play_fd);
      g_play_fd = -1;
    }

  printf("[AUDIO] Devices closed\n");
#endif
}

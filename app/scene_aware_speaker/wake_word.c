/****************************************************************************
 * AI Scene-Aware Smart Speaker - Wake Word Detection
 *
 * Detects wake word "你好 openvela" using TFLite Micro.
 * Falls back to energy-based detection for development.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "scene_aware_speaker.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Energy threshold for wake word detection (fallback) */

#define WAKE_ENERGY_THRESHOLD       500.0f
#define WAKE_ZCR_MIN                0.05f
#define WAKE_ZCR_MAX                0.5f

/* Detection cooldown to avoid repeated triggers */

#define WAKE_COOLDOWN_MS            2000

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* TFLite Micro placeholder */

#ifdef CONFIG_TFLITE_MICRO
static bool g_wakeword_model_loaded = false;
#endif

/* Cooldown timer */

static uint32_t g_last_wake_time = 0;

/* Audio buffer for wake word analysis */

static int16_t g_wake_buffer[MFCC_WINDOW_SIZE];
static int g_wake_buffer_pos = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: compute_energy
 *
 * Description:
 *   Compute RMS energy of audio samples.
 *
 ****************************************************************************/

static float compute_energy(const int16_t *samples, int count)
{
  int64_t sum = 0;
  for (int i = 0; i < count; i++)
    {
      sum += (int64_t)samples[i] * samples[i];
    }
  return sqrtf((float)sum / count);
}

/****************************************************************************
 * Name: compute_zcr
 *
 * Description:
 *   Compute zero crossing rate of audio samples.
 *
 ****************************************************************************/

static float compute_zcr(const int16_t *samples, int count)
{
  int crossings = 0;
  for (int i = 1; i < count; i++)
    {
      if ((samples[i] ^ samples[i - 1]) & 0x8000)
        {
          crossings++;
        }
    }
  return (float)crossings / count;
}

/****************************************************************************
 * Name: energy_detect
 *
 * Description:
 *   Simple energy-based wake word detection.
 *   Used as fallback when TFLite Micro is not available.
 *
 ****************************************************************************/

static bool energy_detect(const int16_t *audio, int samples)
{
  float energy = compute_energy(audio, samples);
  float zcr = compute_zcr(audio, samples);

  /* Check if energy and ZCR are in speech range */

  if (energy > WAKE_ENERGY_THRESHOLD &&
      zcr > WAKE_ZCR_MIN && zcr < WAKE_ZCR_MAX)
    {
      printf("[WAKE] Energy detection: energy=%.1f, zcr=%.3f\n",
             energy, zcr);
      return true;
    }

  return false;
}

/****************************************************************************
 * Name: tflite_detect
 *
 * Description:
 *   TFLite Micro wake word detection (placeholder).
 *
 ****************************************************************************/

#ifdef CONFIG_TFLITE_MICRO
static bool tflite_detect(const int16_t *audio, int samples)
{
  if (!g_wakeword_model_loaded)
    {
      return false;
    }

  /* TODO: Run TFLite Micro inference
   *
   * 1. Extract MFCC features from audio
   * 2. Feed to wake word model
   * 3. Check if "ni hao openvela" probability exceeds threshold
   *
   * Model output: probability of wake word presence
   * Return true if probability > WAKE_WORD_THRESHOLD
   */

  return false;
}
#endif

/****************************************************************************
 * Name: check_cooldown
 *
 * Description:
 *   Check if cooldown period has passed since last wake.
 *
 ****************************************************************************/

static bool check_cooldown(void)
{
  /* TODO: Get actual system time
   *
   * struct timespec ts;
   * clock_gettime(CLOCK_MONOTONIC, &ts);
   * uint32_t now = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
   *
   * For now, always allow detection
   */

  return true;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: wake_word_init
 *
 * Description:
 *   Initialize wake word detection module.
 *
 ****************************************************************************/

int wake_word_init(void)
{
  g_wake_buffer_pos = 0;
  g_last_wake_time = 0;

#ifdef CONFIG_TFLITE_MICRO
  /* TODO: Load wake word model
   *
   * 1. Open model file from filesystem
   * 2. Allocate tensor arena
   * 3. Create interpreter
   */

  g_wakeword_model_loaded = false;
  printf("[WAKE] TFLite Micro available but model not loaded\n");
#else
  printf("[WAKE] TFLite Micro not available, using energy detection\n");
#endif

  printf("[WAKE] Wake word detection initialized: \"%s\"\n", WAKE_WORD);
  return 0;
}

/****************************************************************************
 * Name: wake_word_detect
 *
 * Description:
 *   Detect wake word in audio samples.
 *
 * Input:
 *   audio   - 16-bit PCM audio samples
 *   samples - Number of samples
 *
 * Returns:
 *   true if wake word detected, false otherwise.
 *
 ****************************************************************************/

bool wake_word_detect(const int16_t *audio, int samples)
{
  if (!audio || samples <= 0)
    {
      return false;
    }

  /* Check cooldown */

  if (!check_cooldown())
    {
      return false;
    }

  /* Buffer audio for analysis */

  int remaining = MFCC_WINDOW_SIZE - g_wake_buffer_pos;
  int to_copy = (samples < remaining) ? samples : remaining;

  memcpy(&g_wake_buffer[g_wake_buffer_pos], audio,
         to_copy * sizeof(int16_t));
  g_wake_buffer_pos += to_copy;

  /* Process when buffer is full */

  if (g_wake_buffer_pos >= MFCC_WINDOW_SIZE)
    {
      bool detected = false;

#ifdef CONFIG_TFLITE_MICRO
      /* Try TFLite detection */

      detected = tflite_detect(g_wake_buffer, MFCC_WINDOW_SIZE);
#endif

      /* Fallback to energy detection */

      if (!detected)
        {
          detected = energy_detect(g_wake_buffer, MFCC_WINDOW_SIZE);
        }

      /* Reset buffer */

      g_wake_buffer_pos = 0;

      if (detected)
        {
          /* Update cooldown timer */

          /* TODO: Get actual time
           * struct timespec ts;
           * clock_gettime(CLOCK_MONOTONIC, &ts);
           * g_last_wake_time = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
           */

          printf("[WAKE] Wake word detected!\n");
          return true;
        }
    }

  return false;
}

/****************************************************************************
 * Name: wake_word_cleanup
 *
 * Description:
 *   Cleanup wake word detection module.
 *
 ****************************************************************************/

void wake_word_cleanup(void)
{
#ifdef CONFIG_TFLITE_MICRO
  /* TODO: Cleanup TFLite resources */

  g_wakeword_model_loaded = false;
#endif

  g_wake_buffer_pos = 0;
  printf("[WAKE] Wake word detection cleaned up\n");
}

/****************************************************************************
 * AI Scene-Aware Smart Speaker - Scene Detection
 *
 * Scene classification using TFLite Micro (placeholder).
 * Falls back to rule-based detection using audio features + sensors.
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

/* Scene detection thresholds (rule-based fallback) */

#define THRESHOLD_ENERGY_LOW        100.0f
#define THRESHOLD_ENERGY_MED        300.0f
#define THRESHOLD_ENERGY_HIGH       1000.0f
#define THRESHOLD_ZCR_LOW           0.1f
#define THRESHOLD_ZCR_HIGH          0.3f

/* Sensor thresholds for scene fusion */

#define SENSOR_TEMP_COOKING         30.0f   /* Temperature for cooking */
#define SENSOR_VOC_COOKING          300     /* VOC for cooking */
#define SENSOR_LIGHT_SLEEP          10      /* Light for sleep */
#define SENSOR_PROX_SLEEP           50      /* Proximity for sleep */

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Scene smoothing buffer */

static scene_type_t g_scene_buffer[SCENE_SMOOTH_COUNT];
static int g_scene_buffer_idx = 0;

/* TFLite Micro placeholder */

#ifdef CONFIG_TFLITE_MICRO
static bool g_tflite_loaded = false;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: compute_audio_features
 *
 * Description:
 *   Compute simple audio features from raw samples.
 *   Used for rule-based fallback detection.
 *
 ****************************************************************************/

static void compute_audio_features(const int16_t *audio, int samples,
                                   float *energy, float *zcr)
{
  int64_t sum_energy = 0;
  int zero_crossings = 0;

  for (int i = 0; i < samples; i++)
    {
      sum_energy += (int64_t)audio[i] * audio[i];

      if (i > 0 && ((audio[i] ^ audio[i - 1]) & 0x8000))
        {
          zero_crossings++;
        }
    }

  *energy = sqrtf((float)sum_energy / samples);
  *zcr = (float)zero_crossings / samples;
}

/****************************************************************************
 * Name: rule_based_detect
 *
 * Description:
 *   Rule-based scene detection using audio features.
 *   Fallback when TFLite Micro is not available.
 *
 ****************************************************************************/

static scene_type_t rule_based_detect(float energy, float zcr)
{
  /* Very quiet -> Sleep */

  if (energy < THRESHOLD_ENERGY_LOW)
    {
      return SCENE_SLEEP;
    }

  /* Low energy, low ZCR -> Home (quiet) */

  if (energy < THRESHOLD_ENERGY_MED && zcr < THRESHOLD_ZCR_LOW)
    {
      return SCENE_HOME;
    }

  /* High ZCR -> Entertainment (music/TV) */

  if (zcr > THRESHOLD_ZCR_HIGH)
    {
      return SCENE_ENTERTAINMENT;
    }

  /* High energy -> Cooking (kitchen noise) */

  if (energy > THRESHOLD_ENERGY_HIGH)
    {
      return SCENE_COOKING;
    }

  /* Default -> Working */

  return SCENE_WORKING;
}

/****************************************************************************
 * Name: sensor_fusion_adjust
 *
 * Description:
 *   Adjust scene detection based on sensor data.
 *   Improves accuracy by fusing multiple modalities.
 *
 ****************************************************************************/

static scene_type_t sensor_fusion_adjust(scene_type_t audio_scene,
                                         const sensor_data_t *sensor)
{
  if (!sensor || !sensor->valid)
    {
      return audio_scene;
    }

  /* High temperature + high VOC -> Likely cooking */

  if (sensor->temperature > SENSOR_TEMP_COOKING &&
      sensor->tvoc > SENSOR_VOC_COOKING)
    {
      if (audio_scene == SCENE_HOME || audio_scene == SCENE_WORKING)
        {
          return SCENE_COOKING;
        }
    }

  /* Very low light + low proximity -> Likely sleeping */

  if (sensor->light < SENSOR_LIGHT_SLEEP &&
      sensor->proximity < SENSOR_PROX_SLEEP)
    {
      if (audio_scene == SCENE_SLEEP || audio_scene == SCENE_HOME)
        {
          return SCENE_SLEEP;
        }
    }

  /* Normal light + normal conditions -> Keep audio-based detection */

  return audio_scene;
}

/****************************************************************************
 * Name: smooth_scene
 *
 * Description:
 *   Smooth scene detection to avoid rapid switching.
 *   Requires consecutive same scene detections to switch.
 *
 ****************************************************************************/

static scene_type_t smooth_scene(scene_type_t new_scene)
{
  g_scene_buffer[g_scene_buffer_idx] = new_scene;
  g_scene_buffer_idx = (g_scene_buffer_idx + 1) % SCENE_SMOOTH_COUNT;

  /* Count occurrences of each scene in buffer */

  int counts[SCENE_COUNT] = {0};
  for (int i = 0; i < SCENE_SMOOTH_COUNT; i++)
    {
      if (g_scene_buffer[i] >= 0 && g_scene_buffer[i] < SCENE_COUNT)
        {
          counts[g_scene_buffer[i]]++;
        }
    }

  /* Find scene with most occurrences */

  int max_count = 0;
  scene_type_t max_scene = SCENE_UNKNOWN;
  for (int i = 0; i < SCENE_COUNT; i++)
    {
      if (counts[i] > max_count)
        {
          max_count = counts[i];
          max_scene = (scene_type_t)i;
        }
    }

  /* Only switch if majority agrees */

  if (max_count >= (SCENE_SMOOTH_COUNT + 1) / 2)
    {
      return max_scene;
    }

  /* Keep current scene */

  return g_current_scene;
}

/****************************************************************************
 * Name: tflite_detect
 *
 * Description:
 *   TFLite Micro scene detection (placeholder).
 *   Returns SCENE_UNKNOWN when model not loaded.
 *
 ****************************************************************************/

#ifdef CONFIG_TFLITE_MICRO
static scene_type_t tflite_detect(const float *mfcc,
                                  const sensor_data_t *sensor)
{
  if (!g_tflite_loaded)
    {
      /* TODO: Load TFLite model from file
       *
       * Model input: MFCC features + sensor data
       * Model output: Scene probability distribution
       *
       * Expected model structure:
       *   Input: [batch, 13 + 5] (13 MFCC + temp/humid/voc/eco2/light)
       *   Output: [batch, SCENE_COUNT] (probability for each scene)
       */

      return SCENE_UNKNOWN;
    }

  /* TODO: Run inference
   *
   * tflite::MicroInterpreter interpreter(...);
   * interpreter.Invoke();
   * float *output = interpreter.output(0)->data.f;
   *
   * // Find max probability scene
   * int max_idx = 0;
   * float max_prob = output[0];
   * for (int i = 1; i < SCENE_COUNT; i++) {
   *   if (output[i] > max_prob) {
   *     max_prob = output[i];
   *     max_idx = i;
   *   }
   * }
   *
   * return (scene_type_t)max_idx;
   */

  return SCENE_UNKNOWN;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: scene_detect_init
 *
 * Description:
 *   Initialize scene detection module.
 *
 ****************************************************************************/

int scene_detect_init(void)
{
  /* Clear smoothing buffer */

  memset(g_scene_buffer, 0, sizeof(g_scene_buffer));
  g_scene_buffer_idx = 0;

  /* Initialize TFLite Micro if available */

#ifdef CONFIG_TFLITE_MICRO
  /* TODO: Initialize TFLite Micro runtime
   *
   * 1. Allocate tensor arena
   * 2. Load model from file
   * 3. Create interpreter
   * 4. Allocate tensors
   */

  g_tflite_loaded = false;
  printf("[SCENE] TFLite Micro available but model not loaded\n");
#else
  printf("[SCENE] TFLite Micro not available, using rule-based detection\n");
#endif

  printf("[SCENE] Scene detection initialized\n");
  return 0;
}

/****************************************************************************
 * Name: scene_detect_run
 *
 * Description:
 *   Run scene detection on audio features and sensor data.
 *
 * Input:
 *   mfcc     - MFCC features from feature extraction
 *   sensor   - Sensor data for fusion
 *
 * Returns:
 *   Scene detection result with confidence.
 *
 ****************************************************************************/

scene_result_t scene_detect_run(const float *mfcc,
                                const sensor_data_t *sensor)
{
  scene_result_t result;
  memset(&result, 0, sizeof(result));
  result.timestamp = 0; /* TODO: Get system time */

  /* Copy sensor data */

  if (sensor)
    {
      result.sensor_data = *sensor;
    }

  scene_type_t detected_scene = SCENE_UNKNOWN;
  float confidence = 0.0f;

#ifdef CONFIG_TFLITE_MICRO
  /* Try TFLite detection first */

  if (g_tflite_loaded)
    {
      detected_scene = tflite_detect(mfcc, sensor);
      if (detected_scene != SCENE_UNKNOWN)
        {
          confidence = 0.9f; /* TODO: Get actual confidence */
        }
    }
#endif

  /* Fallback to rule-based detection */

  if (detected_scene == SCENE_UNKNOWN)
    {
      /* Compute simple audio features from MFCC */

      float energy = 0.0f;
      float zcr = 0.0f;

      /* Estimate energy from first MFCC coefficient */

      if (mfcc)
        {
          energy = fabsf(mfcc[0]) * 100.0f;
          zcr = fabsf(mfcc[1]) / 10.0f;
        }

      detected_scene = rule_based_detect(energy, zcr);
      confidence = 0.7f; /* Lower confidence for rule-based */
    }

  /* Apply sensor fusion */

  detected_scene = sensor_fusion_adjust(detected_scene, sensor);

  /* Apply smoothing */

  result.scene = smooth_scene(detected_scene);
  result.confidence = confidence;

  return result;
}

/****************************************************************************
 * Name: scene_detect_cleanup
 *
 * Description:
 *   Cleanup scene detection module.
 *
 ****************************************************************************/

void scene_detect_cleanup(void)
{
#ifdef CONFIG_TFLITE_MICRO
  /* TODO: Cleanup TFLite Micro resources */

  g_tflite_loaded = false;
#endif

  printf("[SCENE] Scene detection cleaned up\n");
}

/****************************************************************************
 * AI Scene-Aware Smart Speaker - Main Entry
 *
 * Modular architecture with local AI inference.
 * All processing runs on-device (no cloud dependency).
 *
 * Modules:
 *   - audio_preprocess: SpeexDSP audio processing
 *   - feature_extract:  MFCC feature extraction
 *   - scene_detect:     TFLite Micro scene classification
 *   - wake_word:        Wake word detection
 *   - sensor_fusion:    SHTC3/SGP30/LTR553 sensor data
 *   - ai_agent:         Local decision engine
 *   - mihome_sim:       MiHome device simulator
 *   - lvgl_ui:          LCD scene visualization
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
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <time.h>

#ifdef CONFIG_GRAPHICS_LVGL
#include <lvgl/lvgl.h>
#endif

#include "scene_aware_speaker.h"

/****************************************************************************
 * Global Data
 ****************************************************************************/

volatile bool g_running = true;
volatile app_state_t g_app_state = APP_STATE_IDLE;
volatile scene_type_t g_current_scene = SCENE_UNKNOWN;

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Module function pointers */

speaker_modules_t g_modules;

/* Thread IDs */

static pthread_t g_scene_tid;
static pthread_t g_voice_tid;
static pthread_t g_sensor_tid;

/* Audio buffer */

static uint8_t g_audio_buf[AUDIO_FRAME_SIZE];

/* MFCC buffer */

static float g_mfcc_buffer[MFCC_NUM_COEFFS];

/* Sensor data */

static sensor_data_t g_sensor_data;

/****************************************************************************
 * Utility Functions
 ****************************************************************************/

/****************************************************************************
 * Name: scene_type_to_string
 *
 * Description:
 *   Convert scene type to human-readable string.
 *
 ****************************************************************************/

const char *scene_type_to_string(scene_type_t scene)
{
  static const char *names[] =
  {
    "Unknown", "Home", "Sleep", "Cooking", "Working", "Entertainment"
  };

  if (scene >= 0 && scene < SCENE_COUNT)
    {
      return names[scene];
    }
  return "Invalid";
}

/****************************************************************************
 * Name: app_state_to_string
 *
 * Description:
 *   Convert app state to human-readable string.
 *
 ****************************************************************************/

const char *app_state_to_string(app_state_t state)
{
  static const char *names[] =
  {
    "IDLE", "LISTENING", "PROCESSING", "RESPONDING", "SCENE_DETECT"
  };

  if (state >= 0 && state <= APP_STATE_SCENE_DETECT)
    {
      return names[state];
    }
  return "Invalid";
}

/****************************************************************************
 * Module Registration
 ****************************************************************************/

/****************************************************************************
 * Name: speaker_modules_init
 *
 * Description:
 *   Initialize all modules and register function pointers.
 *
 ****************************************************************************/

int speaker_modules_init(speaker_modules_t *modules)
{
  int ret;

  if (!modules)
    {
      return -EINVAL;
    }

  memset(modules, 0, sizeof(speaker_modules_t));

  printf("[MAIN] Initializing modules...\n");

  /* Audio preprocessing */

  extern int audio_preprocess_init(void);
  extern int audio_preprocess_read(uint8_t *buf, int size);
  extern int audio_preprocess_write(const uint8_t *buf, int size);
  extern void audio_preprocess_cleanup(void);

  modules->audio_init = audio_preprocess_init;
  modules->audio_read = audio_preprocess_read;
  modules->audio_write = audio_preprocess_write;
  modules->audio_cleanup = audio_preprocess_cleanup;

  ret = modules->audio_init();
  if (ret < 0)
    {
      fprintf(stderr, "[MAIN] Audio init failed: %d\n", ret);
      /* Continue anyway - will use mock audio */
    }

  /* Feature extraction */

  extern int feature_extract_init(void);
  extern int feature_extract_mfcc(const int16_t *audio, int samples,
                                  float *mfcc_out, int num_coeffs);
  extern void feature_extract_cleanup(void);

  modules->feature_init = feature_extract_init;
  modules->feature_extract = feature_extract_mfcc;
  modules->feature_cleanup = feature_extract_cleanup;

  ret = modules->feature_init();
  if (ret < 0)
    {
      fprintf(stderr, "[MAIN] Feature extraction init failed: %d\n", ret);
    }

  /* Scene detection */

  extern int scene_detect_init(void);
  extern scene_result_t scene_detect_run(const float *mfcc,
                                         const sensor_data_t *sensor);
  extern void scene_detect_cleanup(void);

  modules->scene_init = scene_detect_init;
  modules->scene_detect = scene_detect_run;
  modules->scene_cleanup = scene_detect_cleanup;

  ret = modules->scene_init();
  if (ret < 0)
    {
      fprintf(stderr, "[MAIN] Scene detection init failed: %d\n", ret);
    }

  /* Wake word detection */

  extern int wake_word_init(void);
  extern bool wake_word_detect(const int16_t *audio, int samples);
  extern void wake_word_cleanup(void);

  modules->wakeword_init = wake_word_init;
  modules->wakeword_detect = wake_word_detect;
  modules->wakeword_cleanup = wake_word_cleanup;

  ret = modules->wakeword_init();
  if (ret < 0)
    {
      fprintf(stderr, "[MAIN] Wake word init failed: %d\n", ret);
    }

  /* Sensor fusion */

  extern int sensor_fusion_init(void);
  extern int sensor_fusion_read(sensor_data_t *data);
  extern void sensor_fusion_cleanup(void);

  modules->sensor_init = sensor_fusion_init;
  modules->sensor_read = sensor_fusion_read;
  modules->sensor_cleanup = sensor_fusion_cleanup;

  ret = modules->sensor_init();
  if (ret < 0)
    {
      fprintf(stderr, "[MAIN] Sensor fusion init failed: %d\n", ret);
    }

  /* AI Agent */

  extern int ai_agent_init(void);
  extern agent_decision_t ai_agent_decide(scene_type_t scene,
                                          const sensor_data_t *sensor);
  extern void ai_agent_cleanup(void);

  modules->agent_init = ai_agent_init;
  modules->agent_decide = ai_agent_decide;
  modules->agent_cleanup = ai_agent_cleanup;

  ret = modules->agent_init();
  if (ret < 0)
    {
      fprintf(stderr, "[MAIN] AI Agent init failed: %d\n", ret);
    }

  /* MiHome simulator */

  extern int mihome_sim_init(void);
  extern int mihome_sim_execute(const mihome_action_t *action);
  extern void mihome_sim_cleanup(void);

  modules->mihome_init = mihome_sim_init;
  modules->mihome_execute = mihome_sim_execute;
  modules->mihome_cleanup = mihome_sim_cleanup;

  ret = modules->mihome_init();
  if (ret < 0)
    {
      fprintf(stderr, "[MAIN] MiHome simulator init failed: %d\n", ret);
    }

  /* LVGL UI */

  extern int lvgl_ui_init(void);
  extern int lvgl_ui_update_scene(const scene_result_t *result);
  extern int lvgl_ui_update_actions(const agent_decision_t *decision);
  extern int lvgl_ui_update_state(app_state_t state);
  extern void lvgl_ui_cleanup(void);

  modules->ui_init = lvgl_ui_init;
  modules->ui_update_scene = lvgl_ui_update_scene;
  modules->ui_update_actions = lvgl_ui_update_actions;
  modules->ui_update_state = lvgl_ui_update_state;
  modules->ui_cleanup = lvgl_ui_cleanup;

  ret = modules->ui_init();
  if (ret < 0)
    {
      fprintf(stderr, "[MAIN] LVGL UI init failed: %d\n", ret);
    }

  printf("[MAIN] All modules initialized\n");
  return 0;
}

/****************************************************************************
 * Name: speaker_modules_cleanup
 *
 * Description:
 *   Cleanup all modules.
 *
 ****************************************************************************/

void speaker_modules_cleanup(speaker_modules_t *modules)
{
  if (!modules)
    {
      return;
    }

  printf("[MAIN] Cleaning up modules...\n");

  if (modules->ui_cleanup) modules->ui_cleanup();
  if (modules->mihome_cleanup) modules->mihome_cleanup();
  if (modules->agent_cleanup) modules->agent_cleanup();
  if (modules->sensor_cleanup) modules->sensor_cleanup();
  if (modules->wakeword_cleanup) modules->wakeword_cleanup();
  if (modules->scene_cleanup) modules->scene_cleanup();
  if (modules->feature_cleanup) modules->feature_cleanup();
  if (modules->audio_cleanup) modules->audio_cleanup();

  printf("[MAIN] All modules cleaned up\n");
}

/****************************************************************************
 * Thread Entry Points
 ****************************************************************************/

/****************************************************************************
 * Name: scene_detect_thread
 *
 * Description:
 *   Scene detection thread.
 *   Periodically reads audio, extracts features, and detects scene.
 *   Triggers AI Agent decisions when scene changes.
 *
 ****************************************************************************/

void *scene_detect_thread(void *arg)
{
  printf("[THREAD] Scene detection thread started\n");

  while (g_running)
    {
      /* Read audio frame */

      int nread = modules.audio_read(g_audio_buf, AUDIO_FRAME_SIZE);
      if (nread <= 0)
        {
          usleep(10000);
          continue;
        }

      /* Extract MFCC features */

      int16_t *samples = (int16_t *)g_audio_buf;
      int num_samples = nread / sizeof(int16_t);

      modules.feature_extract(samples, num_samples,
                              g_mfcc_buffer, MFCC_NUM_COEFFS);

      /* Get sensor data */

      sensor_data_t sensor;
      modules.sensor_read(&sensor);

      /* Run scene detection */

      scene_result_t result = modules.scene_detect(g_mfcc_buffer, &sensor);

      /* Check for scene change */

      if (result.scene != g_current_scene && result.scene != SCENE_UNKNOWN)
        {
          printf("[THREAD] Scene changed: %s -> %s\n",
                 scene_type_to_string(g_current_scene),
                 scene_type_to_string(result.scene));

          g_current_scene = result.scene;

          /* Get AI Agent decision */

          agent_decision_t decision = modules.agent_decide(result.scene,
                                                           &sensor);

          /* Execute MiHome actions */

          for (int i = 0; i < decision.num_actions; i++)
            {
              modules.mihome_execute(&decision.actions[i]);
            }

          /* Update UI */

          modules.ui_update_scene(&result);
          modules.ui_update_actions(&decision);
        }

      /* Sleep for detection interval */

      usleep(SCENE_DETECT_INTERVAL_MS * 1000);
    }

  printf("[THREAD] Scene detection thread exited\n");
  return NULL;
}

/****************************************************************************
 * Name: voice_interaction_thread
 *
 * Description:
 *   Voice interaction thread.
 *   Continuously listens for wake word, then processes commands.
 *
 ****************************************************************************/

void *voice_interaction_thread(void *arg)
{
  printf("[THREAD] Voice interaction thread started\n");

  while (g_running)
    {
      /* Read audio frame */

      int nread = modules.audio_read(g_audio_buf, AUDIO_FRAME_SIZE);
      if (nread <= 0)
        {
          usleep(10000);
          continue;
        }

      int16_t *samples = (int16_t *)g_audio_buf;
      int num_samples = nread / sizeof(int16_t);

      if (g_app_state == APP_STATE_IDLE)
        {
          /* Check for wake word */

          if (modules.wakeword_detect(samples, num_samples))
            {
              printf("[THREAD] Wake word detected!\n");

              g_app_state = APP_STATE_LISTENING;
              modules.ui_update_state(g_app_state);

              /* Simulate command processing */

              g_app_state = APP_STATE_PROCESSING;
              modules.ui_update_state(g_app_state);

              /* TODO: Implement actual voice command processing
               *
               * 1. Record command audio
               * 2. Run ASR (speech-to-text)
               * 3. Parse intent
               * 4. Execute command
               * 5. Generate TTS response
               */

              usleep(1000000); /* Simulate processing time */

              /* Return to idle */

              g_app_state = APP_STATE_IDLE;
              modules.ui_update_state(g_app_state);
            }
        }

      usleep(10000); /* 10ms */
    }

  printf("[THREAD] Voice interaction thread exited\n");
  return NULL;
}

/****************************************************************************
 * Name: sensor_read_thread
 *
 * Description:
 *   Sensor reading thread.
 *   Periodically reads sensor data for display and fusion.
 *
 ****************************************************************************/

void *sensor_read_thread(void *arg)
{
  printf("[THREAD] Sensor read thread started\n");

  while (g_running)
    {
      /* Read sensor data */

      modules.sensor_read(&g_sensor_data);

      /* Sleep for 1 second */

      usleep(1000000);
    }

  printf("[THREAD] Sensor read thread exited\n");
  return NULL;
}

/****************************************************************************
 * Signal Handler
 ****************************************************************************/

static void signal_handler(int sig)
{
  printf("[MAIN] Received signal %d, shutting down...\n", sig);
  g_running = false;
}

/****************************************************************************
 * Main Entry Point
 ****************************************************************************/

/****************************************************************************
 * Name: main / scene_aware_speaker_main
 *
 * Description:
 *   Application main entry point.
 *   Initializes modules, starts threads, and runs main loop.
 *
 ****************************************************************************/

int main(int argc, char *argv[])
{
  pthread_attr_t attr;
  struct sched_param param;
  int ret;

  printf("[MAIN] ============================================\n");
  printf("[MAIN] AI Scene-Aware Smart Speaker\n");
  printf("[MAIN] Board: Gemini-S1 (R528, Dual-core Cortex-A7)\n");
  printf("[MAIN] Mode: Local AI (no cloud dependency)\n");
  printf("[MAIN] ============================================\n");

  /* Register signal handlers */

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  /* Initialize all modules */

  ret = speaker_modules_init(&g_modules);
  if (ret < 0)
    {
      fprintf(stderr, "[MAIN] Module initialization failed: %d\n", ret);
      return EXIT_FAILURE;
    }

  /* Initialize thread attributes */

  pthread_attr_init(&attr);

  /* Start sensor read thread (lowest priority) */

  param.sched_priority = 80;
  pthread_attr_setschedparam(&attr, &param);
  pthread_attr_setstacksize(&attr, 4096);

  ret = pthread_create(&g_sensor_tid, &attr, sensor_read_thread, NULL);
  if (ret != 0)
    {
      fprintf(stderr, "[MAIN] Failed to create sensor thread: %d\n", ret);
    }
  else
    {
      pthread_setname_np(g_sensor_tid, "sensor_read");
      printf("[MAIN] Sensor read thread created\n");
    }

  /* Start scene detection thread (medium priority) */

  param.sched_priority = 90;
  pthread_attr_setschedparam(&attr, &param);
  pthread_attr_setstacksize(&attr, 8192);

  ret = pthread_create(&g_scene_tid, &attr, scene_detect_thread, NULL);
  if (ret != 0)
    {
      fprintf(stderr, "[MAIN] Failed to create scene thread: %d\n", ret);
    }
  else
    {
      pthread_setname_np(g_scene_tid, "scene_detect");
      printf("[MAIN] Scene detection thread created\n");
    }

  /* Start voice interaction thread (highest priority) */

  param.sched_priority = 100;
  pthread_attr_setschedparam(&attr, &param);
  pthread_attr_setstacksize(&attr, 8192);

  ret = pthread_create(&g_voice_tid, &attr, voice_interaction_thread, NULL);
  if (ret != 0)
    {
      fprintf(stderr, "[MAIN] Failed to create voice thread: %d\n", ret);
    }
  else
    {
      pthread_setname_np(g_voice_tid, "voice_interact");
      printf("[MAIN] Voice interaction thread created\n");
    }

  pthread_attr_destroy(&attr);

  /* Main loop - LVGL task handler */

  printf("[MAIN] Main loop started\n");
  g_app_state = APP_STATE_SCENE_DETECT;
  modules.ui_update_state(g_app_state);

  while (g_running)
    {
#ifdef CONFIG_GRAPHICS_LVGL
      lv_task_handler();
#endif
      usleep(50000); /* 50ms */
    }

  /* Cleanup */

  printf("[MAIN] Shutting down...\n");

  g_running = false;

  /* Wait for threads to exit */

  pthread_join(g_sensor_tid, NULL);
  pthread_join(g_scene_tid, NULL);
  pthread_join(g_voice_tid, NULL);

  /* Cleanup modules */

  speaker_modules_cleanup(&g_modules);

  printf("[MAIN] AI Scene-Aware Smart Speaker stopped.\n");
  return EXIT_SUCCESS;
}

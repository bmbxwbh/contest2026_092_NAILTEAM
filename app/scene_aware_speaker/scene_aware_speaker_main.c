/****************************************************************************
 * AI Scene-Aware Smart Speaker - Main Entry
 *
 * Core Logic:
 *   1. Continuously listen to ambient sound
 *   2. Real-time scene recognition (home/sleep/cooking/working/entertainment)
 *   3. Auto-trigger smart home actions when scene changes
 *   4. Wake word detection for manual voice control (secondary)
 *
 * Architecture:
 *   - audio_preprocess: SpeexDSP noise reduction, AGC, AEC
 *   - feature_extract:  MFCC feature extraction
 *   - scene_detect:     TFLite Micro scene classification
 *   - wake_word:        Wake word detection (secondary feature)
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

/* Audio buffer - shared between threads */

static uint8_t g_audio_buf[AUDIO_FRAME_SIZE];
static pthread_mutex_t g_audio_mutex = PTHREAD_MUTEX_INITIALIZER;

/* MFCC buffer */

static float g_mfcc_buffer[MFCC_NUM_COEFFS];

/* Sensor data */

static sensor_data_t g_sensor_data;
static pthread_mutex_t g_sensor_mutex = PTHREAD_MUTEX_INITIALIZER;

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
  extern int lvgl_ui_update_volume(int volume_pct);
  extern void lvgl_ui_cleanup(void);

  modules->ui_init = lvgl_ui_init;
  modules->ui_update_scene = lvgl_ui_update_scene;
  modules->ui_update_actions = lvgl_ui_update_actions;
  modules->ui_update_state = lvgl_ui_update_state;
  modules->ui_update_volume = lvgl_ui_update_volume;
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
 *   MAIN THREAD - Scene detection with auto-automation.
 *
 *   This is the PRIMARY function of the system:
 *   1. Continuously read audio from microphone
 *   2. Extract features and recognize scene
 *   3. When scene changes, auto-trigger smart home actions
 *   4. Update UI with current scene and actions
 *
 ****************************************************************************/

void *scene_detect_thread(void *arg)
{
  printf("[THREAD] Scene detection thread started (PRIMARY)\n");

  scene_type_t last_scene = SCENE_UNKNOWN;
  uint32_t scene_change_count = 0;

  while (g_running)
    {
      /* Read audio frame (thread-safe) */

      int nread;
      pthread_mutex_lock(&g_audio_mutex);
      nread = modules.audio_read(g_audio_buf, AUDIO_FRAME_SIZE);
      pthread_mutex_unlock(&g_audio_mutex);

      if (nread <= 0)
        {
          usleep(10000); /* 10ms */
          continue;
        }

      /* Extract MFCC features */

      int16_t *samples = (int16_t *)g_audio_buf;
      int num_samples = nread / sizeof(int16_t);

      modules.feature_extract(samples, num_samples,
                              g_mfcc_buffer, MFCC_NUM_COEFFS);

      /* Get sensor data (thread-safe) */

      sensor_data_t sensor;
      pthread_mutex_lock(&g_sensor_mutex);
      sensor = g_sensor_data;
      pthread_mutex_unlock(&g_sensor_mutex);

      /* Run scene detection */

      scene_result_t result = modules.scene_detect(g_mfcc_buffer, &sensor);

      /* Update UI with current scene (always) */

      modules.ui_update_scene(&result);

      /* Check for scene change */

      if (result.scene != last_scene && result.scene != SCENE_UNKNOWN)
        {
          scene_change_count++;
          printf("[THREAD] === SCENE CHANGE #%d === %s -> %s\n",
                 scene_change_count,
                 scene_type_to_string(last_scene),
                 scene_type_to_string(result.scene));

          g_current_scene = result.scene;
          last_scene = result.scene;

          /* Get AI Agent decision */

          agent_decision_t decision = modules.agent_decide(result.scene,
                                                           &sensor);

          /* Execute MiHome actions (simulate smart home control) */

          printf("[THREAD] Executing automation for %s:\n",
                 scene_type_to_string(result.scene));

          for (int i = 0; i < decision.num_actions; i++)
            {
              modules.mihome_execute(&decision.actions[i]);
            }

          /* Update UI with actions */

          modules.ui_update_actions(&decision);
        }

      /* Scene detection runs every 500ms for responsive detection */

      usleep(500000); /* 500ms */
    }

  printf("[THREAD] Scene detection thread exited\n");
  return NULL;
}

/****************************************************************************
 * Name: voice_interaction_thread
 *
 * Description:
 *   SECONDARY THREAD - Wake word detection for manual control.
 *
 *   This is a SECONDARY feature:
 *   1. Continuously listen for wake word "你好 openvela"
 *   2. When detected, switch to listening mode
 *   3. Accept voice commands for manual device control
 *   4. Return to normal scene detection after command
 *
 ****************************************************************************/

void *voice_interaction_thread(void *arg)
{
  printf("[THREAD] Voice interaction thread started (SECONDARY)\n");

  while (g_running)
    {
      /* Read audio frame (thread-safe) */

      int nread;
      pthread_mutex_lock(&g_audio_mutex);
      nread = modules.audio_read(g_audio_buf, AUDIO_FRAME_SIZE);
      pthread_mutex_unlock(&g_audio_mutex);

      if (nread <= 0)
        {
          usleep(10000); /* 10ms */
          continue;
        }

      int16_t *samples = (int16_t *)g_audio_buf;
      int num_samples = nread / sizeof(int16_t);

      /* Only check wake word when in IDLE state */

      if (g_app_state == APP_STATE_IDLE)
        {
          /* Check for wake word */

          if (modules.wakeword_detect(samples, num_samples))
            {
              printf("[THREAD] === WAKE WORD DETECTED ===\n");
              printf("[THREAD] Entering manual control mode\n");

              g_app_state = APP_STATE_LISTENING;
              modules.ui_update_state(g_app_state);

              /* TODO: Implement actual voice command processing
               *
               * 1. Play acknowledgment sound / TTS: "我在"
               * 2. Record command audio (2-5 seconds)
               * 3. Run ASR (speech-to-text)
               * 4. Parse intent (e.g., "打开电视", "音量调大")
               * 5. Execute manual control
               * 6. Generate TTS response
               *
               * For now, simulate a short listening period
               */

              usleep(2000000); /* Simulate 2s listening */

              g_app_state = APP_STATE_PROCESSING;
              modules.ui_update_state(g_app_state);

              usleep(500000); /* Simulate processing */

              /* Return to idle */

              g_app_state = APP_STATE_IDLE;
              modules.ui_update_state(g_app_state);

              printf("[THREAD] Manual control complete, "
                     "resuming scene detection\n");
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
 *   Background thread for periodic sensor reading.
 *   Updates shared sensor data for scene detection.
 *
 ****************************************************************************/

void *sensor_read_thread(void *arg)
{
  printf("[THREAD] Sensor read thread started\n");

  while (g_running)
    {
      /* Read sensor data */

      sensor_data_t new_data;
      int ret = modules.sensor_read(&new_data);

      if (ret == 0 && new_data.valid)
        {
          /* Update shared sensor data (thread-safe) */

          pthread_mutex_lock(&g_sensor_mutex);
          g_sensor_data = new_data;
          pthread_mutex_unlock(&g_sensor_mutex);
        }

      /* Read sensors every second */

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
 *
 *   System Architecture:
 *   ┌─────────────────────────────────────────────────────────┐
 *   │                    Main Loop                            │
 *   │                                                         │
 *   │  Thread 1 (PRIMARY): Scene Detection + Auto Automation  │
 *   │    Audio → MFCC → Scene → Agent → MiHome               │
 *   │                                                         │
 *   │  Thread 2 (SECONDARY): Wake Word + Manual Control       │
 *   │    Audio → Wake Word → Voice Command → Manual Control   │
 *   │                                                         │
 *   │  Thread 3: Sensor Reading                               │
 *   │    SHTC3/SGP30/LTR553 → Shared Data                    │
 *   │                                                         │
 *   │  Main Thread: LVGL UI Updates                           │
 *   └─────────────────────────────────────────────────────────┘
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
  printf("[MAIN]\n");
  printf("[MAIN] System Logic:\n");
  printf("[MAIN]   PRIMARY:   Continuous scene detection + auto automation\n");
  printf("[MAIN]   SECONDARY: Wake word for manual voice control\n");
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

  /* Start scene detection thread (PRIMARY - highest priority) */

  param.sched_priority = 100;
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
      printf("[MAIN] Scene detection thread created (PRIMARY)\n");
    }

  /* Start voice interaction thread (SECONDARY - medium priority) */

  param.sched_priority = 90;
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
      printf("[MAIN] Voice interaction thread created (SECONDARY)\n");
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

  /* Cleanup mutexes */

  pthread_mutex_destroy(&g_audio_mutex);
  pthread_mutex_destroy(&g_sensor_mutex);

  printf("[MAIN] AI Scene-Aware Smart Speaker stopped.\n");
  return EXIT_SUCCESS;
}

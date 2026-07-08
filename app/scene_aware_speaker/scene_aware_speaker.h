/****************************************************************************
 * AI Scene-Aware Smart Speaker - Main Header
 *
 * Modular architecture for scene recognition with sensor fusion.
 * All AI inference runs locally on device (TFLite Micro).
 *
 * Modules:
 *   - audio_preprocess: SpeexDSP noise reduction, AGC, AEC
 *   - feature_extract:  MFCC feature extraction
 *   - scene_detect:     TFLite Micro scene classification
 *   - wake_word:        TFLite Micro wake word detection
 *   - sensor_fusion:    SHTC3/SGP30/LTR553 data fusion
 *   - ai_agent:         Local decision engine
 *   - mihome_sim:       MiHome simulator for demo
 *   - lvgl_ui:          Scene visualization
 ****************************************************************************/

#ifndef __SCENE_AWARE_SPEAKER_H
#define __SCENE_AWARE_SPEAKER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Audio parameters */

#define AUDIO_SAMPLE_RATE       16000
#define AUDIO_CHANNELS          1
#define AUDIO_FRAME_SIZE        640     /* 20ms @16kHz 16bit mono */
#define AUDIO_FRAME_SAMPLES     (AUDIO_FRAME_SIZE / sizeof(int16_t))

/* MFCC parameters */

#define MFCC_NUM_COEFFS         13
#define MFCC_WINDOW_SIZE        512     /* 32ms window */
#define MFCC_HOP_SIZE           256     /* 16ms hop */
#define MFCC_NUM_MEL_FILTERS    26

/* Scene detection */

#define SCENE_DETECT_INTERVAL_MS    2000
#define SCENE_SMOOTH_COUNT          3   /* Consecutive same scene to switch */

/* Wake word */

#define WAKE_WORD               "ni hao openvela"
#define WAKE_WORD_THRESHOLD     0.7f

/* Sensor IDs */

#define SENSOR_SHTC3            0   /* Temperature/Humidity */
#define SENSOR_SGP30            1   /* VOC */
#define SENSOR_LTR553           2   /* Light/Proximity */
#define SENSOR_COUNT            3

/****************************************************************************
 * Type Definitions
 ****************************************************************************/

/* Scene types */

typedef enum
{
  SCENE_UNKNOWN = 0,
  SCENE_HOME,               /* Quiet home */
  SCENE_SLEEP,              /* Sleep - very quiet */
  SCENE_COOKING,            /* Kitchen noise */
  SCENE_WORKING,            /* Keyboard/whisper */
  SCENE_ENTERTAINMENT,      /* Music/TV */
  SCENE_COUNT
} scene_type_t;

/* Application states */

typedef enum
{
  APP_STATE_IDLE = 0,       /* Waiting for wake word */
  APP_STATE_LISTENING,      /* Listening for command */
  APP_STATE_PROCESSING,     /* Processing command */
  APP_STATE_RESPONDING,     /* Playing response */
  APP_STATE_SCENE_DETECT    /* Scene detection running */
} app_state_t;

/* Sensor data structure */

typedef struct
{
  float temperature;        /* Celsius */
  float humidity;           /* Percentage */
  uint16_t tvoc;            /* ppb */
  uint16_t eco2;            /* ppm */
  uint32_t light;           /* lux */
  uint16_t proximity;       /* 0-255 */
  bool valid;               /* Data is valid */
} sensor_data_t;

/* Scene detection result */

typedef struct
{
  scene_type_t scene;
  float confidence;
  uint32_t timestamp;
  sensor_data_t sensor_data;
} scene_result_t;

/* MiHome device action */

typedef struct
{
  const char *device_id;
  const char *device_name;
  const char *action;
  const char *action_desc;
} mihome_action_t;

/* AI Agent decision */

typedef struct
{
  scene_type_t scene;
  int num_actions;
  mihome_action_t actions[8];  /* Max 8 actions per scene */
  char description[128];
} agent_decision_t;

/* Module initialization functions */

typedef struct
{
  /* Audio preprocessing */
  int (*audio_init)(void);
  int (*audio_read)(uint8_t *buf, int size);
  int (*audio_write)(const uint8_t *buf, int size);
  void (*audio_cleanup)(void);

  /* Feature extraction */
  int (*feature_init)(void);
  int (*feature_extract)(const int16_t *audio, int samples,
                         float *mfcc_out, int num_coeffs);
  void (*feature_cleanup)(void);

  /* Scene detection */
  int (*scene_init)(void);
  scene_result_t (*scene_detect)(const float *mfcc,
                                 const sensor_data_t *sensor);
  void (*scene_cleanup)(void);

  /* Wake word detection */
  int (*wakeword_init)(void);
  bool (*wakeword_detect)(const int16_t *audio, int samples);
  void (*wakeword_cleanup)(void);

  /* Sensor fusion */
  int (*sensor_init)(void);
  int (*sensor_read)(sensor_data_t *data);
  void (*sensor_cleanup)(void);

  /* AI Agent */
  int (*agent_init)(void);
  agent_decision_t (*agent_decide)(scene_type_t scene,
                                   const sensor_data_t *sensor);
  void (*agent_cleanup)(void);

  /* MiHome simulator */
  int (*mihome_init)(void);
  int (*mihome_execute)(const mihome_action_t *action);
  void (*mihome_cleanup)(void);

  /* LVGL UI */
  int (*ui_init)(void);
  int (*ui_update_scene)(const scene_result_t *result);
  int (*ui_update_actions)(const agent_decision_t *decision);
  int (*ui_update_state)(app_state_t state);
  void (*ui_cleanup)(void);
} speaker_modules_t;

/****************************************************************************
 * Global Data
 ****************************************************************************/

/* Application control */

extern volatile bool g_running;
extern volatile app_state_t g_app_state;
extern volatile scene_type_t g_current_scene;

/* Module instance */

extern speaker_modules_t g_modules;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Module registration */

int speaker_modules_init(speaker_modules_t *modules);
void speaker_modules_cleanup(speaker_modules_t *modules);

/* Thread entry points */

void *scene_detect_thread(void *arg);
void *voice_interaction_thread(void *arg);
void *sensor_read_thread(void *arg);

/* Utility */

const char *scene_type_to_string(scene_type_t scene);
const char *app_state_to_string(app_state_t state);

#endif /* __SCENE_AWARE_SPEAKER_H */

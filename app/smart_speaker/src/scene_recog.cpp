#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_op_resolver.h"
#include "tensorflow/lite/micro/micro_model.h"

#include "scene_recog.h"
#include "mfcc.h"
#include "audio_pipeline.h"
#include "device_state.h"

extern "C" void bk7258_tflite_register_ops(tflite::MicroMutableOpResolver<10>*);

#define ARENA_SIZE  (256 * 1024)
#define INPUT_T  98
#define INPUT_F  13

static tflite::MicroInterpreter *g_interp = nullptr;
static TfLiteTensor *g_input = nullptr;
static TfLiteTensor *g_output = nullptr;
static uint8_t *g_arena = nullptr;
static uint8_t *g_model_buf = nullptr;

static const scene_type_t g_label_map[13] =
{ SCENE_COOKING, SCENE_BATHING, SCENE_MOVIE, SCENE_SLEEP, SCENE_QUIET, SCENE_TALK,
  SCENE_BABY_CRY, SCENE_WASHING, SCENE_VACUUM, SCENE_DISHWASH, SCENE_KNOCK, SCENE_PET,
  SCENE_OTHER };

bool scene_recog_init(const char *model_path)
{
  int fd = open(model_path, O_RDONLY);
  if (fd < 0) { syslog(LOG_ERR, "open model %s failed\n", model_path); return false; }
  off_t sz = lseek(fd, 0, SEEK_END); lseek(fd, 0, SEEK_SET);
  g_model_buf = (uint8_t*)malloc(sz);
  if (!g_model_buf || read(fd, g_model_buf, sz) != sz) { close(fd); return false; }
  close(fd);

  static tflite::MicroMutableOpResolver<10> resolver;
  bk7258_tflite_register_ops(&resolver);
  resolver.AddSoftmax();

  const tflite::Model *model = tflite::GetModel(g_model_buf);
  g_arena = (uint8_t*)malloc(ARENA_SIZE);
  static tflite::MicroInterpreter interp(model, resolver, g_arena, ARENA_SIZE);
  g_interp = &interp;
  if (g_interp->AllocateTensors() != kTfLiteOk) { syslog(LOG_ERR, "alloc tensors fail\n"); return false; }
  g_input = g_interp->input(0);
  g_output = g_interp->output(0);
  syslog(LOG_INFO, "scene model loaded\n");
  return true;
}

bool scene_recog_infer(const int16_t *pcm_1s, int len, scene_type_t *out_scene, float *out_conf)
{
  if (!g_interp) return false;
  int8_t spec[INPUT_T * INPUT_F];
  int n = mfcc_compute_spectrogram(pcm_1s, len, spec, sizeof(spec));
  int copy = n < INPUT_T*INPUT_F ? n : INPUT_T*INPUT_F;
  memcpy(g_input->data.int8, spec, copy);
  if (copy < INPUT_T*INPUT_F) memset(g_input->data.int8 + copy, 0, INPUT_T*INPUT_F - copy);
  if (g_interp->Invoke() != kTfLiteOk) return false;

  int best = 0; float bv = -1e9f, sum = 0;
  for (int i = 0; i < 7; i++)
    { float v = (float)g_output->data.int8[i]/127.0f; if (v > bv){bv=v;best=i;} sum += v; }
  if (out_scene) *out_scene = g_label_map[best];
  if (out_conf) *out_conf = (sum > 0) ? bv/sum : 0.0f;
  return true;
}

extern "C" int scene_recog_task(int argc, char *argv[])
{
  if (!scene_recog_init("/mnt/aimodel/scene_cnn.tflite")) return -1;
  int16_t pcm1s[16000];
  while (1)
    {
      int got = 0;
      while (got < 16000 - 512) { audio_pipeline_read_frame(pcm1s + got, 512); got += 512; }
      scene_type_t s; float conf;
      if (scene_recog_infer(pcm1s, got, &s, &conf)) device_state_set_scene(s, conf);
    }
  return 0;
}

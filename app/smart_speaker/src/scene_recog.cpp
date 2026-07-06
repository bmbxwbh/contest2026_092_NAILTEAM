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

#define ARENA_SIZE  (256 * 1024)
#define INPUT_T  98
#define INPUT_F  13
#define NUM_CLASSES 13

static tflite::MicroInterpreter *g_interp = nullptr;
static TfLiteTensor *g_input = nullptr;
static TfLiteTensor *g_output = nullptr;
static uint8_t *g_arena = nullptr;
static uint8_t *g_model_buf = nullptr;

static const scene_type_t g_label_map[NUM_CLASSES] =
{ SCENE_COOKING, SCENE_BATHING, SCENE_MOVIE, SCENE_SLEEP, SCENE_QUIET, SCENE_TALK,
  SCENE_BABY_CRY, SCENE_WASHING, SCENE_VACUUM, SCENE_DISHWASH, SCENE_KNOCK, SCENE_PET,
  SCENE_OTHER };

bool scene_recog_init(const char *model_path)
{
  int fd = open(model_path, O_RDONLY);
  if (fd < 0) { syslog(LOG_ERR, "open model %s failed\n", model_path); return false; }
  off_t sz = lseek(fd, 0, SEEK_END); lseek(fd, 0, SEEK_SET);
  if (sz <= 0 || sz > 4 * 1024 * 1024)
    { close(fd); syslog(LOG_ERR, "model size invalid: %ld\n", (long)sz); return false; }
  g_model_buf = (uint8_t*)malloc(sz);
  if (!g_model_buf) { close(fd); syslog(LOG_ERR, "malloc model fail\n"); return false; }
  ssize_t rd = read(fd, g_model_buf, sz);
  close(fd);
  if (rd != sz)
    { free(g_model_buf); g_model_buf = nullptr; syslog(LOG_ERR, "read model short: %zd/%ld\n", rd, (long)sz); return false; }

  static tflite::MicroMutableOpResolver<10> resolver;
  resolver.AddConv2D();
  resolver.AddDepthwiseConv2D();
  resolver.AddFullyConnected();
  resolver.AddSoftmax();
  resolver.AddReshape();
  resolver.AddAveragePool2D();
  resolver.AddMaxPool2D();
  resolver.AddPad();
  resolver.AddQuantize();
  resolver.AddDequantize();

  const tflite::Model *model = tflite::GetModel(g_model_buf);
  g_arena = (uint8_t*)malloc(ARENA_SIZE);
  if (!g_arena) { syslog(LOG_ERR, "malloc arena fail\n"); return false; }
  static tflite::MicroInterpreter interp(model, resolver, g_arena, ARENA_SIZE);
  g_interp = &interp;
  if (g_interp->AllocateTensors() != kTfLiteOk) { syslog(LOG_ERR, "alloc tensors fail\n"); return false; }
  g_input = g_interp->input(0);
  g_output = g_interp->output(0);
  if (g_input->bytes < (size_t)(INPUT_T * INPUT_F))
    { syslog(LOG_ERR, "input tensor too small: %u\n", g_input->bytes); return false; }
  syslog(LOG_INFO, "scene model loaded\n");
  return true;
}

bool scene_recog_infer(const int16_t *pcm_1s, int len, scene_type_t *out_scene, float *out_conf)
{
  if (!g_interp) return false;
  int8_t spec[INPUT_T * INPUT_F];
  int n = mfcc_compute_spectrogram(pcm_1s, len, spec, sizeof(spec));
  int copy = n < INPUT_T * INPUT_F ? n : INPUT_T * INPUT_F;
  memcpy(g_input->data.int8, spec, copy);
  if (copy < INPUT_T * INPUT_F) memset(g_input->data.int8 + copy, 0, INPUT_T * INPUT_F - copy);
  if (g_interp->Invoke() != kTfLiteOk) return false;

  int out_classes = NUM_CLASSES;
  if (g_output->dims->size >= 1)
    {
      int dim_val = g_output->dims->data[g_output->dims->size - 1];
      if (dim_val > 0 && dim_val < out_classes) out_classes = dim_val;
    }
  int best = 0; float bv = -1e9f, sum = 0;
  for (int i = 0; i < out_classes; i++)
    { float v = (float)g_output->data.int8[i] / 127.0f; if (v > bv) { bv = v; best = i; } sum += v; }
  if (out_scene) *out_scene = g_label_map[best];
  if (out_conf) *out_conf = (sum > 0) ? bv / sum : 0.0f;
  return true;
}

extern "C" int scene_recog_task(int argc, char *argv[])
{
  if (!scene_recog_init("/mnt/aimodel/scene_cnn.tflite")) return -1;
  int16_t pcm1s[16000];
  while (1)
    {
      int got = 0;
      int fail = 0;
      while (got < 16000 - FRAME_SAMPLES && fail < 100)
        {
          if (audio_pipeline_read_frame(pcm1s + got, FRAME_SAMPLES)) got += FRAME_SAMPLES;
          else { fail++; usleep(10000); }
        }
      if (got < 16000 - FRAME_SAMPLES) { syslog(LOG_WARNING, "audio starved, skip frame\n"); continue; }
      scene_type_t s; float conf;
      if (scene_recog_infer(pcm1s, got, &s, &conf)) device_state_set_scene(s, conf);
    }
  return 0;
}

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_op_resolver.h"
#include "tensorflow/lite/micro/micro_model.h"
#include "wakeup.h"
#include "audio_pipeline.h"
#include "device_state.h"

extern "C" void bk7258_tflite_register_ops(tflite::MicroMutableOpResolver<10>*);

#define KWS_ARENA (64 * 1024)
static tflite::MicroInterpreter *g_kws = nullptr;
static TfLiteTensor *g_in = nullptr, *g_out = nullptr;
static uint8_t *g_arena = nullptr, *g_model = nullptr;
static float g_smooth = 0.0f;

bool wakeup_init(const char *model_path)
{
  FILE *f = fopen(model_path, "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  g_model = (uint8_t*)malloc(sz);
  if (!g_model) { fclose(f); return false; }
  if (fread(g_model, 1, sz, f) != (size_t)sz) { fclose(f); free(g_model); g_model = nullptr; return false; }
  fclose(f);

  static tflite::MicroMutableOpResolver<10> res;
  bk7258_tflite_register_ops(&res);
  res.AddSoftmax();

  const tflite::Model *m = tflite::GetModel(g_model);
  g_arena = (uint8_t*)malloc(KWS_ARENA);
  if (!g_arena) return false;
  static tflite::MicroInterpreter interp(m, res, g_arena, KWS_ARENA);
  g_kws = &interp;
  if (g_kws->AllocateTensors() != kTfLiteOk) return false;
  g_in = g_kws->input(0); g_out = g_kws->output(0);
  if (g_in->bytes < 512) { syslog(LOG_ERR, "kws input too small: %u\n", g_in->bytes); return false; }
  syslog(LOG_INFO, "kws model loaded\n");
  return true;
}

bool wakeup_detect(const int16_t *pcm_frame, int n)
{
  if (!g_kws) return false;
  for (int i = 0; i < n && i < 512 && i < (int)g_in->bytes; i++) g_in->data.int8[i] = (int8_t)(pcm_frame[i] >> 8);
  if (g_kws->Invoke() != kTfLiteOk) return false;
  float p = (float)g_out->data.int8[1] / 127.0f;  /* class1 = keyword */
  g_smooth = 0.8f * g_smooth + 0.2f * p;
  return g_smooth > 0.6f;
}

extern "C" int wakeup_task(int argc, char *argv[])
{
  if (!wakeup_init("/mnt/aimodel/kws.tflite")) return -1;
  int16_t frame[512];
  while (1)
    {
      if (audio_pipeline_read_frame(frame, 512))
        {
          if (wakeup_detect(frame, 512))
            { device_state_set_wakeup(true); syslog(LOG_INFO, "[WAKEUP] 你好,openvela\n"); }
        }
    }
  return 0;
}

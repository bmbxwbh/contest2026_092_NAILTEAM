#include <nuttx/config.h>
#include <string.h>
#include <syslog.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_op_resolver.h"
#include "tensorflow/lite/micro/micro_model.h"
#include "anomaly.h"
#include "audio_pipeline.h"
#include "device_state.h"

extern "C" void bk7258_tflite_register_ops(tflite::MicroMutableOpResolver<10>*);

#define ANOM_ARENA (64*1024)
static tflite::MicroInterpreter *g_ai = nullptr;
static TfLiteTensor *g_in=nullptr, *g_out=nullptr;
static uint8_t *g_arena=nullptr, *g_model=nullptr;
static int g_hit_count = 0;

bool anomaly_init(const char *model_path)
{
  FILE *f = fopen(model_path,"rb"); if(!f) return false;
  fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
  g_model=(uint8_t*)malloc(sz);
  if(!g_model || fread(g_model,1,sz,f)!=(size_t)sz){ fclose(f); return false; }
  fclose(f);
  static tflite::MicroMutableOpResolver<10> res;
  bk7258_tflite_register_ops(&res); res.AddSoftmax();
  const tflite::Model *m = tflite::GetModel(g_model);
  g_arena=(uint8_t*)malloc(ANOM_ARENA);
  static tflite::MicroInterpreter interp(m,res,g_arena,ANOM_ARENA);
  g_ai=&interp;
  if (g_ai->AllocateTensors()!=kTfLiteOk) return false;
  g_in=g_ai->input(0); g_out=g_ai->output(0);
  syslog(LOG_INFO, "anomaly model loaded\n");
  return true;
}

bool anomaly_detect(const int16_t *pcm, int n, anomaly_type_t *out)
{
  if(!g_ai) return false;
  for(int i=0;i<n && i<8000;i++) g_in->data.int8[i]=(int8_t)(pcm[i]>>8);
  if (g_ai->Invoke()!=kTfLiteOk) return false;
  int best=0; float bv=-1;
  for(int i=0;i<7;i++){ float v=(float)g_out->data.int8[i]/127.f; if(v>bv){bv=v;best=i;} }
  if (best>0 && bv>0.7f) g_hit_count++; else g_hit_count=0;
  if (g_hit_count>=2) { if(out)*out=(anomaly_type_t)best; return true; }
  return false;
}

extern "C" int anomaly_task(int argc, char *argv[])
{
  if(!anomaly_init("/mnt/aimodel/anomaly.tflite")) return -1;
  int16_t buf[8000];
  while(1)
    {
      int got=0;
      while(got<8000-512){ audio_pipeline_read_frame(buf+got,512); got+=512; }
      anomaly_type_t a;
      if (anomaly_detect(buf,got,&a) && a!=ANOMALY_NONE)
        { device_state_set_anomaly(a); syslog(LOG_WARNING,"[ANOMALY] %s\n", anomaly_type_name(a)); }
      else
        device_state_set_anomaly(ANOMALY_NONE);
    }
  return 0;
}

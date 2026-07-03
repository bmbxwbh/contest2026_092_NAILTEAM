#include <nuttx/config.h>
#include <math.h>
#include <string.h>
#include "mfcc.h"

#define PI 3.14159265358979323846f

static float g_mel[MFCC_NUM_FILTERS][MFCC_FFT_SIZE / 2 + 1];
static bool g_init = false;

static float hz_to_mel(float hz){ return 2595.0f * log10f(1.0f + hz / 700.0f); }
static float mel_to_hz(float mel){ return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f); }

static void init_mel(void)
{
  float lo = hz_to_mel(0), hi = hz_to_mel(8000);
  for (int m = 0; m < MFCC_NUM_FILTERS; m++)
    {
      float fl = mel_to_hz(lo + (hi-lo)*m/(MFCC_NUM_FILTERS+1));
      float fc = mel_to_hz(lo + (hi-lo)*(m+1)/(MFCC_NUM_FILTERS+1));
      float fr = mel_to_hz(lo + (hi-lo)*(m+2)/(MFCC_NUM_FILTERS+1));
      for (int k = 0; k <= MFCC_FFT_SIZE/2; k++)
        {
          float f = (float)k * 16000 / MFCC_FFT_SIZE;
          if (f < fl || f > fr) g_mel[m][k] = 0;
          else if (f <= fc) g_mel[m][k] = (f-fl)/(fc-fl+1e-6f);
          else g_mel[m][k] = (fr-f)/(fr-fc+1e-6f);
        }
    }
  g_init = true;
}

static void fft256(float *re, float *im)
{
  int j = 0;
  for (int i = 1; i < MFCC_FFT_SIZE; i++)
    {
      int bit = MFCC_FFT_SIZE >> 1;
      for (; j & bit; bit >>= 1) j ^= bit;
      j ^= bit;
      if (i < j){ float tr=re[i];re[i]=re[j];re[j]=tr; float ti=im[i];im[i]=im[j];im[j]=ti; }
    }
  for (int len = 2; len <= MFCC_FFT_SIZE; len <<= 1)
    {
      float ang = -2.0f*PI/len, wr = cosf(ang), wi = sinf(ang);
      for (int i = 0; i < MFCC_FFT_SIZE; i += len)
        {
          float cr = 1, ci = 0;
          for (int k = 0; k < len/2; k++)
            {
              float tr = cr*re[i+k+len/2] - ci*im[i+k+len/2];
              float ti = cr*im[i+k+len/2] + ci*re[i+k+len/2];
              re[i+k+len/2] = re[i+k]-tr; im[i+k+len/2] = im[i+k]-ti;
              re[i+k] += tr; im[i+k] += ti;
              float ncr = cr*wr - ci*wi; ci = cr*wi + ci*wr; cr = ncr;
            }
        }
    }
}

bool mfcc_compute(const int16_t *pcm, int pcm_len, int8_t *out)
{
  if (!g_init) init_mel();
  float re[MFCC_FFT_SIZE] = {0}, im[MFCC_FFT_SIZE] = {0};
  float pre = 0;
  int n = pcm_len < MFCC_FRAME_LEN ? pcm_len : MFCC_FRAME_LEN;
  for (int i = 0; i < MFCC_FFT_SIZE; i++)
    {
      float s = (i < n) ? (float)pcm[i]/32768.0f : 0;
      s = s - 0.97f*pre; pre = (i < n) ? (float)pcm[i]/32768.0f : 0;
      re[i] = s * (0.54f - 0.46f*cosf(2*PI*i/(MFCC_FRAME_LEN-1)));
    }
  fft256(re, im);
  float mel_e[MFCC_NUM_FILTERS] = {0};
  for (int m = 0; m < MFCC_NUM_FILTERS; m++)
    {
      float e = 0;
      for (int k = 0; k <= MFCC_FFT_SIZE/2; k++)
        e += g_mel[m][k]*(re[k]*re[k]+im[k]*im[k]);
      mel_e[m] = log10f(e + 1e-10f);
    }
  for (int c = 0; c < MFCC_NUM_CEPS; c++)
    {
      float sum = 0;
      for (int m = 0; m < MFCC_NUM_FILTERS; m++)
        sum += mel_e[m]*cosf(PI*(m+0.5f)*c/MFCC_NUM_FILTERS);
      if (sum > 1.0f) sum = 1.0f; if (sum < -1.0f) sum = -1.0f;
      out[c] = (int8_t)(sum * 127.0f);
    }
  return true;
}

int mfcc_compute_spectrogram(const int16_t *pcm_1s, int len_1s, int8_t *out, int buf)
{
  int frames = 0;
  for (int off = 0; off + MFCC_FRAME_LEN <= len_1s && frames*MFCC_NUM_CEPS < buf; off += MFCC_FRAME_STEP)
    { mfcc_compute(pcm_1s+off, MFCC_FRAME_LEN, out+frames*MFCC_NUM_CEPS); frames++; }
  return frames * MFCC_NUM_CEPS;
}

/****************************************************************************
 * AI Scene-Aware Smart Speaker - MFCC Feature Extraction
 *
 * Extracts Mel-Frequency Cepstral Coefficients from audio frames.
 * Lightweight implementation for embedded use.
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

#define FFT_SIZE            MFCC_WINDOW_SIZE
#define MEL_LOW_FREQ        0
#define MEL_HIGH_FREQ       (AUDIO_SAMPLE_RATE / 2)
#define PRE_EMPHASIS        0.97f

#ifndef M_PI
#define M_PI                3.14159265358979323846f
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Hamming window coefficients */

static float g_hamming[MFCC_WINDOW_SIZE];

/* Mel filterbank energies */

static float g_mel_energies[MFCC_NUM_MEL_FILTERS];

/* DCT matrix for MFCC computation */

static float g_dct_matrix[MFCC_NUM_COEFFS][MFCC_NUM_MEL_FILTERS];

/* Scratch buffers */

static float g_windowed[MFCC_WINDOW_SIZE];
static float g_fft_real[FFT_SIZE];
static float g_fft_imag[FFT_SIZE];
static float g_power_spectrum[FFT_SIZE / 2 + 1];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hz_to_mel
 *
 * Description:
 *   Convert frequency in Hz to Mel scale.
 *
 ****************************************************************************/

static float hz_to_mel(float hz)
{
  return 2595.0f * log10f(1.0f + hz / 700.0f);
}

/****************************************************************************
 * Name: mel_to_hz
 *
 * Description:
 *   Convert Mel scale to frequency in Hz.
 *
 ****************************************************************************/

static float mel_to_hz(float mel)
{
  return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

/****************************************************************************
 * Name: init_hamming
 *
 * Description:
 *   Initialize Hamming window coefficients.
 *
 ****************************************************************************/

static void init_hamming(void)
{
  for (int i = 0; i < MFCC_WINDOW_SIZE; i++)
    {
      g_hamming[i] = 0.54f - 0.46f *
                     cosf(2.0f * M_PI * i / (MFCC_WINDOW_SIZE - 1));
    }
}

/****************************************************************************
 * Name: init_dct_matrix
 *
 * Description:
 *   Initialize DCT-II matrix for MFCC computation.
 *
 ****************************************************************************/

static void init_dct_matrix(void)
{
  for (int i = 0; i < MFCC_NUM_COEFFS; i++)
    {
      for (int j = 0; j < MFCC_NUM_MEL_FILTERS; j++)
        {
          g_dct_matrix[i][j] = cosf(M_PI * i * (2.0f * j + 1.0f) /
                                    (2.0f * MFCC_NUM_MEL_FILTERS));
        }
    }
}

/****************************************************************************
 * Name: simple_fft
 *
 * Description:
 *   Simple radix-2 FFT (in-place, Cooley-Tukey).
 *   Not optimized, but sufficient for embedded use.
 *
 ****************************************************************************/

static void simple_fft(float *real, float *imag, int n)
{
  /* Bit-reversal permutation */

  for (int i = 1, j = 0; i < n; i++)
    {
      int bit = n >> 1;
      for (; j & bit; bit >>= 1)
        {
          j ^= bit;
        }
      j ^= bit;

      if (i < j)
        {
          float temp = real[i];
          real[i] = real[j];
          real[j] = temp;
          temp = imag[i];
          imag[i] = imag[j];
          imag[j] = temp;
        }
    }

  /* FFT butterfly */

  for (int len = 2; len <= n; len <<= 1)
    {
      float angle = -2.0f * M_PI / len;
      float w_real = cosf(angle);
      float w_imag = sinf(angle);

      for (int i = 0; i < n; i += len)
        {
          float cur_real = 1.0f;
          float cur_imag = 0.0f;

          for (int j = 0; j < len / 2; j++)
            {
              int u = i + j;
              int v = i + j + len / 2;

              float t_real = cur_real * real[v] - cur_imag * imag[v];
              float t_imag = cur_real * imag[v] + cur_imag * real[v];

              real[v] = real[u] - t_real;
              imag[v] = imag[u] - t_imag;
              real[u] += t_real;
              imag[u] += t_imag;

              float new_real = cur_real * w_real - cur_imag * w_imag;
              float new_imag = cur_real * w_imag + cur_imag * w_real;
              cur_real = new_real;
              cur_imag = new_imag;
            }
        }
    }
}

/****************************************************************************
 * Name: compute_power_spectrum
 *
 * Description:
 *   Compute power spectrum from FFT result.
 *
 ****************************************************************************/

static void compute_power_spectrum(const float *real, const float *imag,
                                   float *power, int n)
{
  for (int i = 0; i <= n / 2; i++)
    {
      power[i] = real[i] * real[i] + imag[i] * imag[i];
      power[i] /= n;
    }
}

/****************************************************************************
 * Name: compute_mel_filterbank
 *
 * Description:
 *   Apply Mel filterbank to power spectrum.
 *
 ****************************************************************************/

static void compute_mel_filterbank(const float *power, float *mel_energies,
                                   int n_filters, int n_fft)
{
  float low_mel = hz_to_mel(MEL_LOW_FREQ);
  float high_mel = hz_to_mel(MEL_HIGH_FREQ);
  float mel_step = (high_mel - low_mel) / (n_filters + 1);

  /* Compute center frequencies in Hz */

  float center_freqs[MFCC_NUM_MEL_FILTERS + 2];
  for (int i = 0; i < n_filters + 2; i++)
    {
      center_freqs[i] = mel_to_hz(low_mel + i * mel_step);
    }

  /* Apply triangular filters */

  memset(mel_energies, 0, n_filters * sizeof(float));

  for (int m = 0; m < n_filters; m++)
    {
      float left = center_freqs[m];
      float center = center_freqs[m + 1];
      float right = center_freqs[m + 2];

      for (int k = 0; k <= n_fft / 2; k++)
        {
          float freq = (float)k * AUDIO_SAMPLE_RATE / n_fft;

          if (freq >= left && freq <= center)
            {
              float weight = (freq - left) / (center - left);
              mel_energies[m] += power[k] * weight;
            }
          else if (freq > center && freq <= right)
            {
              float weight = (right - freq) / (right - center);
              mel_energies[m] += power[k] * weight;
            }
        }

      /* Log energy (with floor to avoid log(0)) */

      if (mel_energies[m] < 1e-10f)
        {
          mel_energies[m] = -10.0f;
        }
      else
        {
          mel_energies[m] = log10f(mel_energies[m]);
        }
    }
}

/****************************************************************************
 * Name: compute_dct
 *
 * Description:
 *   Apply DCT-II to get MFCCs from log Mel energies.
 *
 ****************************************************************************/

static void compute_dct(const float *mel_energies, float *mfcc,
                        int n_coeffs, int n_filters)
{
  for (int i = 0; i < n_coeffs; i++)
    {
      mfcc[i] = 0.0f;
      for (int j = 0; j < n_filters; j++)
        {
          mfcc[i] += mel_energies[j] * g_dct_matrix[i][j];
        }
      mfcc[i] *= sqrtf(2.0f / n_filters);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: feature_extract_init
 *
 * Description:
 *   Initialize feature extraction module.
 *
 ****************************************************************************/

int feature_extract_init(void)
{
  init_hamming();
  init_dct_matrix();

  printf("[FEATURE] MFCC extraction initialized: %d coeffs, %d filters\n",
         MFCC_NUM_COEFFS, MFCC_NUM_MEL_FILTERS);

  return 0;
}

/****************************************************************************
 * Name: feature_extract_mfcc
 *
 * Description:
 *   Extract MFCC features from audio samples.
 *
 * Input:
 *   audio     - 16-bit PCM audio samples
 *   samples   - Number of samples
 *   mfcc_out  - Output MFCC coefficients
 *   num_coeffs - Number of coefficients to extract
 *
 * Returns:
 *   0 on success, negative error code on failure.
 *
 ****************************************************************************/

int feature_extract_mfcc(const int16_t *audio, int samples,
                         float *mfcc_out, int num_coeffs)
{
  if (!audio || !mfcc_out || samples < MFCC_WINDOW_SIZE)
    {
      return -EINVAL;
    }

  /* Pre-emphasis */

  float emphasized[MFCC_WINDOW_SIZE];
  emphasized[0] = (float)audio[0];
  for (int i = 1; i < MFCC_WINDOW_SIZE; i++)
    {
      emphasized[i] = (float)audio[i] -
                      PRE_EMPHASIS * (float)audio[i - 1];
    }

  /* Apply Hamming window */

  for (int i = 0; i < MFCC_WINDOW_SIZE; i++)
    {
      g_windowed[i] = emphasized[i] * g_hamming[i];
    }

  /* Zero-pad to FFT size */

  memset(g_fft_real, 0, sizeof(g_fft_real));
  memset(g_fft_imag, 0, sizeof(g_fft_imag));
  memcpy(g_fft_real, g_windowed, MFCC_WINDOW_SIZE * sizeof(float));

  /* Compute FFT */

  simple_fft(g_fft_real, g_fft_imag, FFT_SIZE);

  /* Compute power spectrum */

  compute_power_spectrum(g_fft_real, g_fft_imag,
                         g_power_spectrum, FFT_SIZE);

  /* Apply Mel filterbank */

  compute_mel_filterbank(g_power_spectrum, g_mel_energies,
                         MFCC_NUM_MEL_FILTERS, FFT_SIZE);

  /* Compute DCT to get MFCCs */

  compute_dct(g_mel_energies, mfcc_out,
              num_coeffs, MFCC_NUM_MEL_FILTERS);

  return 0;
}

/****************************************************************************
 * Name: feature_extract_cleanup
 *
 * Description:
 *   Cleanup feature extraction module.
 *
 ****************************************************************************/

void feature_extract_cleanup(void)
{
  /* Nothing to cleanup for static allocation */
  printf("[FEATURE] MFCC extraction cleaned up\n");
}

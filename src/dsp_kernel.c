/* Portable numeric kernel. Identical source on POSIX and RP2350; only the
 * compiler flags differ (see mrbgem.rake). */
#include <math.h>
#include "../include/dsp.h"

/* Each enabled length drags in its own twiddle and bit-reversal tables, and the
 * linker cannot drop them because any of them may be asked for at run time.
 * Measured in the pico2_w firmware: all eight lengths cost 88 KB of flash, one
 * length costs 13 KB. Narrow the range with PICORUBY_DSP_FFT_MIN/MAX when the
 * application only ever uses one. */
#define DSP_FFT_ENABLED(n) \
  (PICORUBY_DSP_FFT_MIN <= (n) && (n) <= PICORUBY_DSP_FFT_MAX)

int
DSP_fft_size_supported(uint32_t n)
{
  switch (n) {
#if DSP_FFT_ENABLED(32)
    case 32:
#endif
#if DSP_FFT_ENABLED(64)
    case 64:
#endif
#if DSP_FFT_ENABLED(128)
    case 128:
#endif
#if DSP_FFT_ENABLED(256)
    case 256:
#endif
#if DSP_FFT_ENABLED(512)
    case 512:
#endif
#if DSP_FFT_ENABLED(1024)
    case 1024:
#endif
#if DSP_FFT_ENABLED(2048)
    case 2048:
#endif
#if DSP_FFT_ENABLED(4096)
    case 4096:
#endif
      return 1;
    default:
      return 0;
  }
}

int
DSP_fft_init(dsp_fft_t *fft, uint32_t n)
{
  arm_status st;

  switch (n) {
#if DSP_FFT_ENABLED(32)
    case 32:   st = arm_rfft_fast_init_32_f32(&fft->inst);   break;
#endif
#if DSP_FFT_ENABLED(64)
    case 64:   st = arm_rfft_fast_init_64_f32(&fft->inst);   break;
#endif
#if DSP_FFT_ENABLED(128)
    case 128:  st = arm_rfft_fast_init_128_f32(&fft->inst);  break;
#endif
#if DSP_FFT_ENABLED(256)
    case 256:  st = arm_rfft_fast_init_256_f32(&fft->inst);  break;
#endif
#if DSP_FFT_ENABLED(512)
    case 512:  st = arm_rfft_fast_init_512_f32(&fft->inst);  break;
#endif
#if DSP_FFT_ENABLED(1024)
    case 1024: st = arm_rfft_fast_init_1024_f32(&fft->inst); break;
#endif
#if DSP_FFT_ENABLED(2048)
    case 2048: st = arm_rfft_fast_init_2048_f32(&fft->inst); break;
#endif
#if DSP_FFT_ENABLED(4096)
    case 4096: st = arm_rfft_fast_init_4096_f32(&fft->inst); break;
#endif
    default:   return 0;
  }
  if (st != ARM_MATH_SUCCESS) return 0;
  fft->size = n;
  return 1;
}

void
DSP_fft_forward(dsp_fft_t *fft, const float32_t *in, float32_t *out)
{
  /* arm_rfft_fast_f32 scribbles over its input, so the caller owns the copy. */
  arm_rfft_fast_f32(&fft->inst, (float32_t *)in, out, 0);
}

void
DSP_hann_inplace(float32_t *buf, uint32_t n)
{
  if (n < 2) return;
  const float32_t k = 2.0f * (float32_t)M_PI / (float32_t)(n - 1);
  for (uint32_t i = 0; i < n; i++) {
    buf[i] *= 0.5f * (1.0f - cosf(k * (float32_t)i));
  }
}

void
DSP_magnitude(const float32_t *spec, float32_t *mag, uint32_t n)
{
  const uint32_t nbins = n / 2;
  mag[0] = fabsf(spec[0]);
  for (uint32_t k = 1; k < nbins; k++) {
    const float32_t re = spec[2 * k];
    const float32_t im = spec[2 * k + 1];
    mag[k] = sqrtf(re * re + im * im);
  }
}

int32_t
DSP_argmax(const float32_t *data, uint32_t n, uint32_t skip)
{
  uint32_t best;
  float32_t bv;

  if (skip >= n) return -1;
  best = skip;
  bv = data[skip];
  for (uint32_t i = skip + 1; i < n; i++) {
    if (data[i] > bv) {
      bv = data[i];
      best = i;
    }
  }
  return (int32_t)best;
}

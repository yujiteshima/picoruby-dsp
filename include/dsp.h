#ifndef PICORUBY_DSP_H
#define PICORUBY_DSP_H

#include <stdint.h>
#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dsp_buffer {
  uint32_t   size;
  float32_t *data;
} dsp_buffer_t;

typedef struct dsp_fft {
  uint32_t                     size;
  arm_rfft_fast_instance_f32   inst;
} dsp_fft_t;

/* Which transform lengths this build accepts. Narrowing the range to a single
 * length lets the linker drop every other twiddle table. */
#ifndef PICORUBY_DSP_FFT_MIN
#define PICORUBY_DSP_FFT_MIN 32
#endif
#ifndef PICORUBY_DSP_FFT_MAX
#define PICORUBY_DSP_FFT_MAX 4096
#endif

/* Each length maps to a size-specific CMSIS-DSP init rather than the generic
 * arm_rfft_fast_init_f32(), whose switch over every length keeps all of them
 * alive: 83.6 KB of flash against 13.2 KB for a single fixed length. */
int  DSP_fft_size_supported(uint32_t n);
int  DSP_fft_init(dsp_fft_t *fft, uint32_t n);
void DSP_fft_forward(dsp_fft_t *fft, const float32_t *in, float32_t *out);

void DSP_hann_inplace(float32_t *buf, uint32_t n);

/* spec is the packed rfft output: [DC, Nyquist, re1, im1, re2, im2, ...].
 * mag must hold n/2 floats. mag[0] is |DC|; the Nyquist bin is not returned. */
void DSP_magnitude(const float32_t *spec, float32_t *mag, uint32_t n);

/* Index of the largest element at or after `skip`, or -1 if there is none.
 * Callers scanning a spectrum usually pass skip >= 1: DC and the lowest bins
 * carry the window's leakage and would otherwise win. */
int32_t DSP_argmax(const float32_t *data, uint32_t n, uint32_t skip);

#ifdef __cplusplus
}
#endif

#endif /* PICORUBY_DSP_H */

#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/array.h>

/* ---------------------------------------------------------------- Buffer */

static void
dsp_buffer_free(mrb_state *mrb, void *ptr)
{
  dsp_buffer_t *buf = (dsp_buffer_t *)ptr;
  if (buf) {
    mrb_free(mrb, buf->data);
    mrb_free(mrb, buf);
  }
}

static const struct mrb_data_type dsp_buffer_type = {
  "DSP::Buffer", dsp_buffer_free
};

static dsp_buffer_t *
buffer_ptr(mrb_state *mrb, mrb_value self)
{
  dsp_buffer_t *buf = (dsp_buffer_t *)mrb_data_get_ptr(mrb, self, &dsp_buffer_type);
  if (!buf) mrb_raise(mrb, E_RUNTIME_ERROR, "uninitialized DSP::Buffer");
  return buf;
}

static dsp_buffer_t *
buffer_alloc(mrb_state *mrb, mrb_value self, mrb_int size)
{
  dsp_buffer_t *buf;

  if (size < 1) mrb_raise(mrb, E_ARGUMENT_ERROR, "size must be positive");

  mrb_data_init(self, NULL, &dsp_buffer_type);
  buf = (dsp_buffer_t *)mrb_malloc(mrb, sizeof(dsp_buffer_t));
  buf->data = NULL;
  buf->size = 0;
  mrb_data_init(self, buf, &dsp_buffer_type);

  buf->data = (float32_t *)mrb_calloc(mrb, (size_t)size, sizeof(float32_t));
  buf->size = (uint32_t)size;
  return buf;
}

static mrb_value
mrb_buffer_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_int size;
  mrb_get_args(mrb, "i", &size);
  buffer_alloc(mrb, self, size);
  return self;
}

static mrb_value
mrb_buffer_size(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value((mrb_int)buffer_ptr(mrb, self)->size);
}

static mrb_value
mrb_buffer_aref(mrb_state *mrb, mrb_value self)
{
  dsp_buffer_t *buf = buffer_ptr(mrb, self);
  mrb_int i;
  mrb_get_args(mrb, "i", &i);
  if (i < 0) i += (mrb_int)buf->size;
  if (i < 0 || (uint32_t)i >= buf->size) {
    mrb_raisef(mrb, E_INDEX_ERROR, "index %i out of range", i);
  }
  return mrb_float_value(mrb, (mrb_float)buf->data[i]);
}

static mrb_value
mrb_buffer_aset(mrb_state *mrb, mrb_value self)
{
  dsp_buffer_t *buf = buffer_ptr(mrb, self);
  mrb_int i;
  mrb_float v;
  mrb_get_args(mrb, "if", &i, &v);
  if (i < 0) i += (mrb_int)buf->size;
  if (i < 0 || (uint32_t)i >= buf->size) {
    mrb_raisef(mrb, E_INDEX_ERROR, "index %i out of range", i);
  }
  buf->data[i] = (float32_t)v;
  return mrb_float_value(mrb, v);
}

static mrb_value
mrb_buffer_argmax(mrb_state *mrb, mrb_value self)
{
  dsp_buffer_t *buf = buffer_ptr(mrb, self);
  mrb_int skip = 0;
  mrb_get_args(mrb, "|i", &skip);
  if (skip < 0) skip = 0;
  return mrb_fixnum_value((mrb_int)DSP_argmax(buf->data, buf->size, (uint32_t)skip));
}

static mrb_value
mrb_buffer_hann(mrb_state *mrb, mrb_value self)
{
  dsp_buffer_t *buf = buffer_ptr(mrb, self);
  DSP_hann_inplace(buf->data, buf->size);
  return self;
}

static mrb_value
mrb_buffer_to_a(mrb_state *mrb, mrb_value self)
{
  dsp_buffer_t *buf = buffer_ptr(mrb, self);
  mrb_value ary = mrb_ary_new_capa(mrb, (mrb_int)buf->size);
  for (uint32_t i = 0; i < buf->size; i++) {
    mrb_ary_push(mrb, ary, mrb_float_value(mrb, (mrb_float)buf->data[i]));
  }
  return ary;
}

/* Fill from an Array in one call: the per-element []= path costs a method
 * dispatch per sample, which dominates for kilo-sample buffers. */
static mrb_value
mrb_buffer_set_array(mrb_state *mrb, mrb_value self)
{
  dsp_buffer_t *buf = buffer_ptr(mrb, self);
  mrb_value ary;
  mrb_int n;

  mrb_get_args(mrb, "A", &ary);
  n = RARRAY_LEN(ary);
  if ((uint32_t)n > buf->size) n = (mrb_int)buf->size;
  for (mrb_int i = 0; i < n; i++) {
    buf->data[i] = (float32_t)mrb_as_float(mrb, RARRAY_PTR(ary)[i]);
  }
  return self;
}

/* ------------------------------------------------------------- Spectrum */

static void
dsp_spectrum_free(mrb_state *mrb, void *ptr)
{
  dsp_buffer_t *s = (dsp_buffer_t *)ptr;
  if (s) {
    mrb_free(mrb, s->data);
    mrb_free(mrb, s);
  }
}

static const struct mrb_data_type dsp_spectrum_type = {
  "DSP::Spectrum", dsp_spectrum_free
};

static dsp_buffer_t *
spectrum_ptr(mrb_state *mrb, mrb_value self)
{
  dsp_buffer_t *s = (dsp_buffer_t *)mrb_data_get_ptr(mrb, self, &dsp_spectrum_type);
  if (!s) mrb_raise(mrb, E_RUNTIME_ERROR, "uninitialized DSP::Spectrum");
  return s;
}

static mrb_value
mrb_spectrum_size(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value((mrb_int)spectrum_ptr(mrb, self)->size);
}

static mrb_value
mrb_spectrum_bins(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value((mrb_int)(spectrum_ptr(mrb, self)->size / 2));
}

static mrb_value
mrb_spectrum_magnitude(mrb_state *mrb, mrb_value self)
{
  dsp_buffer_t *s = spectrum_ptr(mrb, self);
  struct RClass *mod = mrb_module_get_id(mrb, MRB_SYM(DSP));
  struct RClass *cls = mrb_class_get_under_id(mrb, mod, MRB_SYM(Buffer));
  dsp_buffer_t *ob;

  ob = (dsp_buffer_t *)mrb_malloc(mrb, sizeof(dsp_buffer_t));
  ob->size = s->size / 2;
  ob->data = (float32_t *)mrb_calloc(mrb, ob->size, sizeof(float32_t));
  DSP_magnitude(s->data, ob->data, s->size);
  return mrb_obj_value(mrb_data_object_alloc(mrb, cls, ob, &dsp_buffer_type));
}

/* ------------------------------------------------------------------ FFT */

static void
dsp_fft_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

static const struct mrb_data_type dsp_fft_type = {
  "DSP::FFT", dsp_fft_free
};

static mrb_value
mrb_fft_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_int size;
  dsp_fft_t *fft;

  mrb_get_args(mrb, "i", &size);
  if (!DSP_fft_size_supported((uint32_t)size)) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR,
               "unsupported FFT size %i (want a power of two, 32..4096)", size);
  }

  mrb_data_init(self, NULL, &dsp_fft_type);
  fft = (dsp_fft_t *)mrb_malloc(mrb, sizeof(dsp_fft_t));
  mrb_data_init(self, fft, &dsp_fft_type);

  if (!DSP_fft_init(fft, (uint32_t)size)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "FFT init failed");
  }
  return self;
}

static mrb_value
mrb_fft_size(mrb_state *mrb, mrb_value self)
{
  dsp_fft_t *fft = (dsp_fft_t *)mrb_data_get_ptr(mrb, self, &dsp_fft_type);
  return mrb_fixnum_value((mrb_int)fft->size);
}

static mrb_value
mrb_fft_forward(mrb_state *mrb, mrb_value self)
{
  dsp_fft_t *fft = (dsp_fft_t *)mrb_data_get_ptr(mrb, self, &dsp_fft_type);
  mrb_value src;
  dsp_buffer_t *in, *out;
  struct RClass *mod, *cls;

  mrb_get_args(mrb, "o", &src);
  in = (dsp_buffer_t *)mrb_data_get_ptr(mrb, src, &dsp_buffer_type);
  if (!in) mrb_raise(mrb, E_TYPE_ERROR, "expected a DSP::Buffer");
  if (in->size != fft->size) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR,
               "buffer size %i does not match FFT size %i",
               (mrb_int)in->size, (mrb_int)fft->size);
  }

  out = (dsp_buffer_t *)mrb_malloc(mrb, sizeof(dsp_buffer_t));
  out->size = fft->size;
  out->data = (float32_t *)mrb_calloc(mrb, out->size, sizeof(float32_t));
  DSP_fft_forward(fft, in->data, out->data);

  mod = mrb_module_get_id(mrb, MRB_SYM(DSP));
  cls = mrb_class_get_under_id(mrb, mod, MRB_SYM(Spectrum));
  return mrb_obj_value(mrb_data_object_alloc(mrb, cls, out, &dsp_spectrum_type));
}

/* ----------------------------------------------------------------- init */

void
mrb_picoruby_dsp_gem_init(mrb_state *mrb)
{
  struct RClass *mod_DSP = mrb_define_module_id(mrb, MRB_SYM(DSP));

  struct RClass *class_Buffer =
    mrb_define_class_under_id(mrb, mod_DSP, MRB_SYM(Buffer), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_Buffer, MRB_TT_CDATA);
  mrb_define_method_id(mrb, class_Buffer, MRB_SYM(initialize), mrb_buffer_initialize, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Buffer, MRB_SYM(size), mrb_buffer_size, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Buffer, MRB_OPSYM(aref), mrb_buffer_aref, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Buffer, MRB_OPSYM(aset), mrb_buffer_aset, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_Buffer, MRB_SYM_B(hann), mrb_buffer_hann, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Buffer, MRB_SYM(argmax), mrb_buffer_argmax, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, class_Buffer, MRB_SYM(to_a), mrb_buffer_to_a, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Buffer, MRB_SYM(set_array), mrb_buffer_set_array, MRB_ARGS_REQ(1));

  struct RClass *class_Spectrum =
    mrb_define_class_under_id(mrb, mod_DSP, MRB_SYM(Spectrum), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_Spectrum, MRB_TT_CDATA);
  mrb_define_method_id(mrb, class_Spectrum, MRB_SYM(size), mrb_spectrum_size, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Spectrum, MRB_SYM(bins), mrb_spectrum_bins, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Spectrum, MRB_SYM(magnitude), mrb_spectrum_magnitude, MRB_ARGS_NONE());

  struct RClass *class_FFT =
    mrb_define_class_under_id(mrb, mod_DSP, MRB_SYM(FFT), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_FFT, MRB_TT_CDATA);
  mrb_define_method_id(mrb, class_FFT, MRB_SYM(initialize), mrb_fft_initialize, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FFT, MRB_SYM(size), mrb_fft_size, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_FFT, MRB_SYM(forward), mrb_fft_forward, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_dsp_gem_final(mrb_state *mrb)
{
}

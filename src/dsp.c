#include "../include/dsp.h"

#if defined(PICORB_VM_MRUBYC)

#error "picoruby-dsp targets the mruby VM. mruby/c is not supported yet."

#else

/* PicoRuby defines PICORB_VM_MRUBY; plain mruby defines neither. Same binding
 * either way -- nothing here reaches into PicoRuby-specific API. */
#include "mruby/dsp.c"

#endif

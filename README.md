# picoruby-dsp

A numerical / signal-processing layer for [PicoRuby](https://github.com/picoruby/picoruby).

PicoRuby ships peripheral gems for GPIO, I2C, SPI, ADC and PWM, and it ships
finished signal-processing appliances such as `picoruby-pitchdetector` (YIN, in
the time domain). What it does not ship is the layer underneath those: a typed
contiguous float buffer, an FFT, and the handful of operations that turn a
window of samples into a frequency. That is what this gem is.

Status: **v0.1 in development.**

Builds against the mruby VM, whether that VM is embedded in PicoRuby or is
plain mruby:

| host | status |
|---|---|
| PicoRuby, POSIX build | works |
| PicoRuby, R2P2 on pico2 / pico2_w (RP2350) | works, with the FPU enabled |
| plain mruby (tested against 4.0.0) | works |
| PicoRuby `femtoruby` / mruby-c VM | not supported (compile-time error) |

Nothing here reaches into PicoRuby-internal API — the binding is stock mruby C
API, and there is no `ports/` directory — so the same tree serves all three.

## Example

```ruby
require 'dsp'

fft = DSP::FFT.new(1024)
buf = DSP::Buffer.new(1024)

1024.times { |i| buf[i] = Math.sin(2 * Math::PI * 440 * i / 8000.0) }

spectrum = fft.forward(buf.hann!)
spectrum.peak_frequency(sample_rate: 8000)  #=> 439.59...
```

## API

| Class | Method | Where it runs |
|---|---|---|
| `DSP::Buffer` | `.new(size)`, `#[]`, `#[]=`, `#size`, `#to_a`, `#set_array` | C |
| | `#hann!`, `#argmax(skip = 0)` | C |
| | `.from_array`, `#each`, `#max` | Ruby |
| `DSP::FFT` | `.new(size)`, `#forward(buffer)` | C (CMSIS-DSP) |
| `DSP::Spectrum` | `#magnitude`, `#size`, `#bins` | C |
| | `#peak_frequency(sample_rate:)`, `#bin_width` | Ruby |

Supported FFT sizes: 32, 64, 128, 256, 512, 1024, 2048, 4096.

**`FFT#forward` overwrites the buffer it is given.** CMSIS-DSP's
`arm_rfft_fast_f32` uses its input as scratch space, and this gem does not copy
it — on a board where the largest free block is a few kilobytes, a hidden copy
is worse than a surprising one. Refill the buffer before reusing it. (The name
should probably be `forward!`; see the v0.2 notes.)

## Three things worth knowing

**Anything that loops once per sample belongs in C.** `#argmax` started as a
Ruby `while` loop and it cost more than the FFT it was scanning — the loop pays
a method dispatch per element, the FFT pays none. Moving it to C, measured with
`bench/fft_bench.rb`:

| N | Ruby loop | C | | on RP2350 |
|---|---|---|---|---|
| 256 | 0.0046 ms | 0.0001 ms | 30.6× | **57.5×** |
| 512 | 0.0086 ms | 0.0003 ms | 32.4× | **79.6×** |
| 1024 | 0.0163 ms | 0.0005 ms | 32.2× | — |
| 2048 | 0.0304 ms | 0.0010 ms | 31.1× | — |

The first three columns are a POSIX build on an M-series Mac; the last is the
same script on a Pico 2 W. The gap widens on the microcontroller, where
dispatch overhead is a larger share of everything.

**On RP2350 the typed buffer is not an optimisation, it is the only option.**
R2P2 leaves about 400 KB of Ruby heap, but fragmentation caps the largest
single allocation near 6 KB:

| allocation | contiguous bytes | result |
|---|---|---|
| `DSP::Buffer.new(1024)` | 4,096 | fits |
| `Array` of 1024 `Float` | 16,384+ (doubles while growing) | `NoMemoryError` |

`mrb_float` is a double, so a Ruby array of samples wants four times the bytes
of an f32 buffer before counting per-element overhead and the doubling regrowth.
1024 samples do not fit in an `Array` on this board. They fit in a `Buffer`.
`Buffer#set_array` exists for convenience on hosts with room; on the board,
fill the buffer directly or hand it to a DMA capture.

**Every FFT length you leave enabled costs flash.** `DSP::FFT.new(n)` takes its
length at run time, so the linker has to keep the twiddle and bit-reversal
tables for every length the build accepts. Measured as the delta on a pico2_w
R2P2 firmware image:

| build | firmware `.bin` | delta |
|---|---|---|
| without this gem | 2,354,152 B | — |
| all eight lengths (32–4096) | 2,442,504 B | +86 KB |
| `PICORUBY_DSP_FFT_MIN=MAX=1024` | 2,371,960 B | +17.4 KB |

If your application only ever uses one length, pin it:

```sh
PICORUBY_DSP_FFT_MIN=1024 PICORUBY_DSP_FFT_MAX=1024 rake
```

`DSP_fft_init` dispatches to `arm_rfft_fast_init_1024_f32` rather than the
generic `arm_rfft_fast_init_f32(&S, n)` — the generic one switches over all
lengths internally, so it keeps every table alive even in a single-length
build (83,608 B against 13,180 B in an isolated link test).

**This gem opts itself into the FPU.** PicoRuby's R2P2 build config compiles gem
sources with `-mcpu=cortex-m33 -mthumb`, which emits `__aeabi_*` soft-float
calls — a dot-product loop compiles to zero FPU instructions. pico-sdk compiles
its own sources with `-march=armv8-m.main+fp+dsp -mfpu=fpv5-sp-d16
-mfloat-abi=softfp`, so only code built by CMake (`ports/rp2040/`) gets hardware
floating point. `mrbgem.rake` adds those flags for this gem's sources on
`pico2`/`pico2_w`. `softfp` shares a calling convention with `soft`, so the
objects still link against the rest of `libmruby.a`.

## Build

CMSIS-DSP is fetched into `lib/CMSIS-DSP` on first build. To use an existing
checkout, set `CMSIS_DSP_PATH`.

The same line works in a PicoRuby or a plain mruby `build_config`:

```ruby
conf.gem github: 'yujiteshima/picoruby-dsp'
```

## License

MIT

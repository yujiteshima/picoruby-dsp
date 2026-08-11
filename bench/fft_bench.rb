# picoruby-dsp stage benchmark.
#
# Runs unchanged on the POSIX build and on R2P2 (pico2_w). The point is not the
# FFT number on its own -- it is the ratio between the stages that stay in C and
# the stage that runs in Ruby, which is what decides where the Ruby/C line
# should sit.
#
#   ./bin/picoruby bench/fft_bench.rb
#
# On the board, Time.now resolution may be coarse (NO_CLOCK_GETTIME=1 in the
# R2P2 build config), so REPEAT is set per size to keep each timed block well
# above the tick.

SAMPLE_RATE = 8000.0
SIZES       = [256, 512, 1024, 2048]
TARGET_MS   = 200.0   # aim for roughly this much work per timed block

def now_ms
  Time.now.to_f * 1000.0
end

def signal(n, freq, fs)
  a = []
  i = 0
  while i < n
    a << Math.sin(2 * Math::PI * freq * i / fs)
    i += 1
  end
  a
end

# The peak search as it was first written, in Ruby. DSP::Buffer#argmax now does
# this in C; this copy stays here so the two can be timed side by side.
def ruby_argmax(buf, skip = 0)
  n = buf.size
  return -1 if n <= skip
  best = skip
  bv = buf[skip]
  i = skip + 1
  while i < n
    v = buf[i]
    if v > bv
      bv = v
      best = i
    end
    i += 1
  end
  best
end

# Time a block, auto-scaling the repeat count so the total lands near TARGET_MS.
def measure(label, results)
  reps = 1
  loop do
    t0 = now_ms
    reps.times { yield }
    dt = now_ms - t0
    if dt >= TARGET_MS || reps >= 100_000
      per = dt / reps
      results << [label, per, reps]
      return per
    end
    reps = reps < 8 ? reps * 8 : (reps * TARGET_MS / (dt < 0.01 ? 0.01 : dt)).to_i + 1
  end
end

puts "picoruby-dsp stage benchmark"
puts "sample rate: #{SAMPLE_RATE.to_i} Hz"
puts

SIZES.each do |n|
  fft     = DSP::FFT.new(n)
  samples = signal(n, 440.0, SAMPLE_RATE)
  results = []

  measure("Buffer.from_array (Ruby ary -> C)", results) { DSP::Buffer.from_array(samples) }

  buf = DSP::Buffer.from_array(samples)
  measure("hann!               [C]", results) { buf.hann! }

  measure("FFT#forward!        [C]", results) { fft.forward!(buf) }

  spec = fft.forward!(buf)
  measure("Spectrum#magnitude  [C]", results) { spec.magnitude }

  magbuf = DSP::Buffer.new(n / 2)
  measure("magnitude_into      [C]", results) { spec.magnitude_into(magbuf) }

  mag = spec.magnitude
  ruby_ms = measure("argmax, hand-written Ruby loop", results) { ruby_argmax(mag, 1) }
  c_ms    = measure("Buffer#argmax       [C]", results) { mag.argmax(1) }

  measure("peak_frequency   [mixed]", results) { spec.peak_frequency(sample_rate: SAMPLE_RATE) }

  full = measure("full pipeline", results) do
    s = fft.forward!(DSP::Buffer.from_array(samples).hann!)
    s.peak_frequency(sample_rate: SAMPLE_RATE)
  end

  puts "N = #{n}   (bin width #{(SAMPLE_RATE / n).round(3)} Hz)"
  results.each do |label, per, reps|
    printf("  %-36s %9.4f ms   (x%d)\n", label, per, reps)
  end
  hz = full > 0 ? 1000.0 / full : 0
  printf("  -> %.1f full analyses/sec; continuous coverage needs %.1f/sec at %d samples\n",
         hz, SAMPLE_RATE / n, n)
  if c_ms > 0
    printf("  -> argmax: Ruby loop is %.1fx the C version, and %.1fx the FFT it scans\n\n",
           ruby_ms / c_ms, ruby_ms / results.find { |l, _, _| l =~ /forward/ }[1])
  else
    puts
  end
end

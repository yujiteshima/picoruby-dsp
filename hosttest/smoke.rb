# Assertion script that runs anywhere the gem loads -- POSIX picoruby, plain
# mruby, or the board over a serial console. Picotest coverage lives in
# ../test/dsp_test.rb; this is the same ground shaped as a plain script, because a
# script is what you can paste into irb on a device.

SAMPLE_RATE = 8000.0
N = 1024

$failures = 0

def check(cond, label)
  if cond
    puts "  PASS #{label}"
  else
    $failures += 1
    puts "  FAIL #{label}"
  end
end

def make_signal(freq, n, fs)
  a = []
  i = 0
  while i < n
    a << Math.sin(2 * Math::PI * freq * i / fs)
    i += 1
  end
  a
end

puts "== Buffer =="
b = DSP::Buffer.new(8)
b[0] = 1.5
b[3] = -2.25
b[-1] = 9.0
check b.size == 8, "size"
check b.max == 9.0, "max"
check b.argmax == 7, "argmax"
begin
  b[99]
  check false, "IndexError raised"
rescue IndexError
  check true, "IndexError raised"
end

puts "== FFT =="
begin
  DSP::FFT.new(1000)
  check false, "size 1000 rejected"
rescue ArgumentError
  check true, "size 1000 rejected"
end
fft = DSP::FFT.new(N)
check fft.size == N, "fft.size"

puts "== forward! is destructive (that is the contract) =="
buf = DSP::Buffer.from_array(make_signal(440.0, N, SAMPLE_RATE))
first = buf[1]
spec = fft.forward!(buf)
check buf[1] != first, "input overwritten by forward!"
check spec.size == N && spec.bins == N / 2, "spectrum shape"

puts "== pipeline: hann -> forward! -> peak =="
[50.0, 440.0, 1000.0, 1234.5, 3000.0].each do |f|
  buf = DSP::Buffer.from_array(make_signal(f, N, SAMPLE_RATE))
  got = fft.forward!(buf.hann!).peak_frequency(sample_rate: SAMPLE_RATE)
  err = (got - f).abs
  check err < SAMPLE_RATE / N, "#{f} Hz -> #{got.round(4)} (err #{err.round(4)})"
end

puts "== magnitude_into and mag: reuse =="
buf = DSP::Buffer.from_array(make_signal(440.0, N, SAMPLE_RATE))
spec = fft.forward!(buf.hann!)
a = spec.magnitude
out = DSP::Buffer.new(spec.bins)
spec.magnitude_into(out)
diff = 0.0
i = 0
while i < a.size
  d = a[i] - out[i]
  diff += (d < 0 ? -d : d)
  i += 1
end
check diff < 1e-4, "magnitude_into matches magnitude (sum diff #{diff})"
begin
  spec.magnitude_into(DSP::Buffer.new(3))
  check false, "size mismatch rejected"
rescue ArgumentError
  check true, "size mismatch rejected"
end

mag = DSP::Buffer.new(N / 2)
f1 = fft.forward!(DSP::Buffer.from_array(make_signal(440.0, N, SAMPLE_RATE)).hann!)
        .peak_frequency(sample_rate: SAMPLE_RATE, mag: mag)
f2 = fft.forward!(DSP::Buffer.from_array(make_signal(1000.0, N, SAMPLE_RATE)).hann!)
        .peak_frequency(sample_rate: SAMPLE_RATE, mag: mag)
check (f1 - 440.0).abs < SAMPLE_RATE / N, "peak with reused mag (440)"
check (f2 - 1000.0).abs < SAMPLE_RATE / N, "peak with reused mag (1000)"

puts "== steady state survives GC churn =="
200.times do
  s = fft.forward!(DSP::Buffer.from_array(make_signal(440.0, N, SAMPLE_RATE)).hann!)
  s.peak_frequency(sample_rate: SAMPLE_RATE, mag: mag)
end
GC.start if Object.const_defined?(:GC)
check true, "200 iterations survived"

puts
if $failures == 0
  puts "ALL OK"
else
  puts "FAILURES: #{$failures}"
end

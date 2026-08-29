class DSPTest < Picotest::Test
  def test_buffer_roundtrip
    buf = DSP::Buffer.new(4)
    buf[0] = 1.5
    buf[3] = -2.0
    assert_equal 4, buf.size
    assert_in_delta 1.5, buf[0], 1e-6
    assert_in_delta(-2.0, buf[-1], 1e-6)
  end

  def test_buffer_index_error
    assert_raise(IndexError) do
      DSP::Buffer.new(2)[5]
    end
  end

  def test_from_array_and_argmax
    buf = DSP::Buffer.from_array([0.0, 1.0, 4.0, 2.0])
    assert_equal 2, buf.argmax
    assert_equal 3, buf.argmax(3)
    assert_in_delta 4.0, buf.max, 1e-6
  end

  def test_fft_rejects_bad_sizes
    assert_raise(ArgumentError) do
      DSP::FFT.new(1000)
    end
  end

  def test_fft_rejects_mismatched_buffer
    assert_raise(ArgumentError) do
      DSP::FFT.new(64).forward!(DSP::Buffer.new(32))
    end
  end

  def test_forward_bang_overwrites_input
    buf = sine(64, 8.0, 64.0)
    before = buf.to_a
    DSP::FFT.new(64).forward!(buf)
    assert_not_equal before, buf.to_a
  end

  def test_pipeline_peak
    fs = 8000.0
    buf = sine(256, 440.0, fs)
    f = DSP::FFT.new(256).forward!(buf.hann!).peak_frequency(sample_rate: fs)
    assert_in_delta 440.0, f, fs / 256
  end

  def test_magnitude_into_matches_magnitude
    spec = DSP::FFT.new(128).forward!(sine(128, 440.0, 8000.0).hann!)
    a = spec.magnitude
    out = DSP::Buffer.new(spec.bins)
    b = spec.magnitude_into(out)
    assert_equal out.object_id, b.object_id
    diff = 0.0
    i = 0
    while i < a.size
      d = a[i] - b[i]
      diff += (d < 0 ? -d : d)
      i += 1
    end
    assert_in_delta 0.0, diff, 1e-4
  end

  def test_magnitude_into_size_check
    spec = DSP::FFT.new(64).forward!(DSP::Buffer.new(64))
    assert_raise(ArgumentError) do
      spec.magnitude_into(DSP::Buffer.new(16))
    end
  end

  def test_peak_frequency_with_reused_mag
    fs = 8000.0
    fft = DSP::FFT.new(256)
    mag = DSP::Buffer.new(128)
    f1 = fft.forward!(sine(256, 440.0, fs).hann!).peak_frequency(sample_rate: fs, mag: mag)
    f2 = fft.forward!(sine(256, 1000.0, fs).hann!).peak_frequency(sample_rate: fs, mag: mag)
    assert_in_delta 440.0, f1, fs / 256
    assert_in_delta 1000.0, f2, fs / 256
  end

  private

  def sine(n, freq, fs)
    buf = DSP::Buffer.new(n)
    i = 0
    while i < n
      buf[i] = Math.sin(2 * Math::PI * freq * i / fs)
      i += 1
    end
    buf
  end
end

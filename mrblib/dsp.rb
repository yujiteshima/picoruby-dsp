module DSP
  class Buffer
    def self.from_array(ary)
      buf = new(ary.size)
      buf.set_array(ary)
      buf
    end

    def each
      i = 0
      n = size
      while i < n
        yield self[i]
        i += 1
      end
      self
    end

    # #argmax is defined in C. Scanning a 512-bin spectrum from Ruby cost about
    # nine times the 1024-point FFT it was scanning, because the loop pays a
    # method dispatch per element while the FFT pays none.
    def max
      i = argmax
      i < 0 ? nil : self[i]
    end
  end

  class Spectrum
    # Frequency of the strongest bin, refined by parabolic interpolation
    # against its two neighbours. That puts the estimate well inside one bin:
    # measured error is 0.03-0.5 Hz against a 7.8 Hz bin at fs=8 kHz, N=1024.
    def peak_frequency(sample_rate:, min_bin: 1)
      mag = magnitude
      n = size
      peak = mag.argmax(min_bin)
      return nil if peak < 0

      delta = 0.0
      if peak > 0 && peak < mag.size - 1
        a = mag[peak - 1]
        b = mag[peak]
        c = mag[peak + 1]
        denom = a - 2.0 * b + c
        delta = 0.5 * (a - c) / denom if denom.abs > 1e-20
      end

      (peak + delta) * sample_rate.to_f / n.to_f
    end

    def bin_width(sample_rate:)
      sample_rate.to_f / size.to_f
    end
  end
end

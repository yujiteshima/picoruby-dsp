MRuby::Gem::Specification.new('picoruby-dsp') do |spec|
  spec.license = 'MIT'
  spec.author  = 'Yuji Teshima'
  spec.summary = 'Numerical / signal processing layer for PicoRuby'

  CMSIS_DSP_VERSION = 'v1.17.0'
  CMSIS_DSP_REPO    = 'https://github.com/ARM-software/CMSIS-DSP.git'

  cmsis_dir = ENV['CMSIS_DSP_PATH'] || "#{dir}/lib/CMSIS-DSP"
  unless File.file?("#{cmsis_dir}/Include/arm_math.h")
    sh "git clone --depth 1 -b #{CMSIS_DSP_VERSION} #{CMSIS_DSP_REPO} #{cmsis_dir}"
  end

  spec.cc.include_paths << "#{dir}/include"
  spec.cc.include_paths << "#{cmsis_dir}/Include"
  spec.cc.include_paths << "#{cmsis_dir}/PrivateInclude"

  # Every FFT length the build accepts keeps its own twiddle tables alive,
  # because the length is a run-time argument and the linker cannot know which
  # one will be asked for. Measured in the pico2_w firmware: 88 KB of flash for
  # all eight lengths, 13 KB for one. Pin the range when the application only
  # ever uses a single size:
  #
  #   PICORUBY_DSP_FFT_MIN=1024 PICORUBY_DSP_FFT_MAX=1024 rake
  if (v = ENV['PICORUBY_DSP_FFT_MIN'])
    spec.cc.defines << "PICORUBY_DSP_FFT_MIN=#{Integer(v)}"
  end
  if (v = ENV['PICORUBY_DSP_FFT_MAX'])
    spec.cc.defines << "PICORUBY_DSP_FFT_MAX=#{Integer(v)}"
  end

  # RP2350 (Cortex-M33) has an FPU and the Armv8-M DSP extension, but the
  # R2P2 build_config only passes "-mcpu=cortex-m33 -mthumb". That combination
  # emits __aeabi_* soft-float calls for every flop -- measured: 0 FPU
  # instructions in a dot-product loop. pico-sdk compiles its own sources with
  # "+fp+dsp -mfpu=fpv5-sp-d16 -mfloat-abi=softfp", so only code built by CMake
  # (ports/rp2040) gets the FPU.
  #
  # These sources are built by rake into libmruby.a, so opt this gem in
  # explicitly. softfp shares its calling convention with soft, so these
  # objects stay link-compatible with the rest of libmruby.a. (hard does not.)
  # build.platform? is part of PicoRuby's build DSL and does not exist under
  # plain mruby, where this gem also builds.
  if build.respond_to?(:platform?) &&
     build.platform?(:rp2) && ENV['PICORB_BOARD'].to_s.start_with?('pico2')
    spec.cc.flags << '-march=armv8-m.main+fp+dsp'
    spec.cc.flags << '-mfpu=fpv5-sp-d16'
    spec.cc.flags << '-mfloat-abi=softfp'
    spec.cc.include_paths <<
      "#{MRUBY_ROOT}/mrbgems/picoruby-r2p2/lib/pico-sdk/src/rp2_common/cmsis/stub/CMSIS/Core/Include"
  else
    # No CMSIS Core headers on this target. CMSIS-DSP's documented escape
    # hatch for that is __GNUC_PYTHON__ ("the only way to build on a target
    # not supported by CMSIS Core"): same C math, no intrinsics. macOS never
    # reaches it (__APPLE_CC__ wins the compiler dispatch first); Linux
    # hosts and both ESP32 architectures (Xtensa and RISC-V) need it --
    # measured 9/9 sources compiling on xtensa-esp-elf and riscv32-esp-elf
    # with it, 0/9 without.
    spec.cc.defines << '__GNUC_PYTHON__'
  end

  # Minimal CMSIS-DSP source set for a single-precision real FFT.
  # Everything else is dropped at link time by --gc-sections.
  %w[
    TransformFunctions/arm_rfft_fast_f32
    TransformFunctions/arm_rfft_fast_init_f32
    TransformFunctions/arm_cfft_f32
    TransformFunctions/arm_cfft_init_f32
    TransformFunctions/arm_cfft_radix8_f32
    TransformFunctions/arm_bitreversal2
    CommonTables/arm_common_tables
    CommonTables/arm_const_structs
  ].each do |rel|
    src = "#{cmsis_dir}/Source/#{rel}.c"
    obj = "#{build_dir}/cmsis/#{objfile(File.basename(rel))}"
    file obj => src do |t|
      FileUtils.mkdir_p File.dirname(t.name)
      spec.cc.run t.name, t.prerequisites[0]
    end
    spec.objs << obj
  end
end

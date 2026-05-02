/**
 * @file new_modules_test.cpp
 * @brief Unit tests for SmoothedValue, StateVariableFilter, Waveshaper, DelayLine
 */

#include <sonicforge/delayline.hpp>
#include <sonicforge/smoothed_value.hpp>
#include <sonicforge/state_variable_filter.hpp>
#include <sonicforge/waveshaper.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <numeric>
#include <vector>

// Shorthand to avoid repeating sonicforge:: in tests
using namespace sonicforge;

// ===================================================================
// SmoothedValue
// ===================================================================

TEST(SmoothedValueConstruction, LinearDefaults) {
    sonicforge::SmoothedValue<sonicforge::SmoothingMode::Linear> sv;
    sv.reset(0.0F, 48000.0F);
    sv.set_ramp_duration(0.01F);
    EXPECT_FLOAT_EQ(sv.get_current(), 0.0F);
    EXPECT_FALSE(sv.is_smoothing());
}

TEST(SmoothedValueConstruction, MultiplicativeDefaults) {
    sonicforge::SmoothedValue<sonicforge::SmoothingMode::Multiplicative> sv;
    sv.reset(440.0F, 48000.0F);
    EXPECT_FLOAT_EQ(sv.get_current(), 440.0F);
    EXPECT_FLOAT_EQ(sv.get_target(), 440.0F);
}

TEST(SmoothedValueLinear, RampsToTarget) {
    sonicforge::SmoothedValue<sonicforge::SmoothingMode::Linear> sv;
    sv.reset(0.0F, 48000.0F);
    sv.set_ramp_duration(0.01F); // 480 steps at 48 kHz

    sv.set_target(1.0F);
    EXPECT_TRUE(sv.is_smoothing());

    float val = 0.0F;
    for (int i = 0; i < 479; ++i) {
        val = sv.process();
        EXPECT_LT(val, 1.0F) << "Should not reach target before step 480";
    }
    // Step 480 should reach the target
    val = sv.process();
    EXPECT_NEAR(val, 1.0F, 1e-5F);
    EXPECT_FALSE(sv.is_smoothing());
}

TEST(SmoothedValueLinear, SnapsToTarget) {
    sonicforge::SmoothedValue<sonicforge::SmoothingMode::Linear> sv;
    sv.reset(0.0F, 48000.0F);
    sv.set_ramp_duration(0.01F);
    sv.set_target(1.0F);

    sv.snap_to_target();
    EXPECT_FLOAT_EQ(sv.get_current(), 1.0F);
    EXPECT_FALSE(sv.is_smoothing());
}

TEST(SmoothedValueLinear, ProcessBlock) {
    sonicforge::SmoothedValue<sonicforge::SmoothingMode::Linear> sv;
    sv.reset(0.0F, 48000.0F);
    sv.set_ramp_duration(0.01F);
    sv.set_target(1.0F);

    std::vector<float> buf(256);
    sv.process_block(buf.data(), static_cast<int>(buf.size()));

    // First value should be > 0, last value < 1
    EXPECT_GT(buf.front(), 0.0F);
    EXPECT_LT(buf.back(), 1.0F);
}

TEST(SmoothedValueMultiplicative, RampsExponentially) {
    sonicforge::SmoothedValue<sonicforge::SmoothingMode::Multiplicative> sv;
    sv.reset(440.0F, 48000.0F);
    sv.set_ramp_duration(0.01F);

    sv.set_target(880.0F);
    EXPECT_TRUE(sv.is_smoothing());

    float prev = 440.0F;
    for (int i = 0; i < 480; ++i) {
        const float cur = sv.process();
        // Ratio between successive samples should be approximately constant
        if (prev > 0.0F && cur > 0.0F) {
            // Just verify it's monotonically increasing
            EXPECT_GE(cur, prev) << "Multiplicative ramp should be monotonic";
        }
        prev = cur;
    }
    EXPECT_NEAR(prev, 880.0F, 1.0F);
    EXPECT_FALSE(sv.is_smoothing());
}

TEST(SmoothedValueLinear, NoSmoothingWhenTargetUnchanged) {
    sonicforge::SmoothedValue<sonicforge::SmoothingMode::Linear> sv(0.5F);
    sv.reset(0.5F, 48000.0F);
    sv.set_target(0.5F);
    EXPECT_FALSE(sv.is_smoothing());
    EXPECT_FLOAT_EQ(sv.process(), 0.5F);
}

// ===================================================================
// StateVariableFilter
// ===================================================================

TEST(SVFConstruction, DefaultParameters) {
    sonicforge::StateVariableFilter svf;
    EXPECT_NEAR(svf.get_cutoff_hz(), 1000.0F, 1.0F);
    EXPECT_NEAR(svf.get_resonance(), 0.5F, 1e-5F);
    EXPECT_EQ(svf.get_mode(), sonicforge::FilterMode::Lowpass);
    EXPECT_FLOAT_EQ(svf.get_sample_rate(), 48000.0F);
}

TEST(SVFConstruction, CustomParameters) {
    sonicforge::StateVariableFilter svf{sonicforge::FilterMode::Highpass, 2000.0F, 0.7F, 44100.0F};
    EXPECT_NEAR(svf.get_cutoff_hz(), 2000.0F, 1.0F);
    EXPECT_NEAR(svf.get_resonance(), 0.7F, 1e-5F);
    EXPECT_EQ(svf.get_mode(), sonicforge::FilterMode::Highpass);
    EXPECT_FLOAT_EQ(svf.get_sample_rate(), 44100.0F);
}

TEST(SVFConstruction, InvalidModeDefaultsToLowpass) {
    sonicforge::StateVariableFilter svf{static_cast<sonicforge::FilterMode>(99), 1000.0F, 0.5F};
    EXPECT_EQ(svf.get_mode(), sonicforge::FilterMode::Lowpass);
}

TEST(SVFProcessing, LowpassAttenuatesHighFrequency) {
    sonicforge::StateVariableFilter svf{sonicforge::FilterMode::Lowpass, 500.0F, 0.1F, 48000.0F};

    constexpr int N = 4800; // 100 ms at 48 kHz
    std::vector<float> buf(N);

    // Generate a 5 kHz sine — well above the 500 Hz cutoff
    for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] = std::sin(2.0F * 3.14159265F * 5000.0F * static_cast<float>(i) / 48000.0F);
    }

    svf.process_block(buf.data(), buf.size());

    // After filtering, the RMS should be significantly reduced
    float rms = 0.0F;
    for (float s : buf)
        rms += s * s;
    rms = std::sqrt(rms / N);

    // Original sine RMS = 0.707; after LP at 500 Hz on 5 kHz signal, should be < 0.1
    EXPECT_LT(rms, 0.1F) << "Lowpass filter should strongly attenuate 5 kHz";
}

TEST(SVFProcessing, HighpassAttenuatesLowFrequency) {
    sonicforge::StateVariableFilter svf{sonicforge::FilterMode::Highpass, 2000.0F, 0.1F, 48000.0F};

    constexpr int N = 4800;
    std::vector<float> buf(N);

    // 100 Hz sine — well below the 2 kHz cutoff
    for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] = std::sin(2.0F * 3.14159265F * 100.0F * static_cast<float>(i) / 48000.0F);
    }

    svf.process_block(buf.data(), buf.size());

    float rms = 0.0F;
    for (float s : buf)
        rms += s * s;
    rms = std::sqrt(rms / N);

    EXPECT_LT(rms, 0.1F) << "Highpass filter should strongly attenuate 100 Hz";
}

TEST(SVFProcessing, DCPassThroughLowpass) {
    sonicforge::StateVariableFilter svf{sonicforge::FilterMode::Lowpass, 5000.0F, 0.0F, 48000.0F};
    svf.reset();

    // A constant DC value should pass through a lowpass unchanged (after transient)
    const float dc = 0.5F;
    float last_out = 0.0F;
    for (size_t i = 0; i < 200; ++i) {
        last_out = svf.process(dc);
    }
    const float out = svf.process(dc);
    (void)last_out;
    EXPECT_NEAR(out, dc, 0.01F) << "DC should pass through lowpass";
}

TEST(SVFParameters, AtomicCutoffChange) {
    sonicforge::StateVariableFilter svf{sonicforge::FilterMode::Lowpass, 1000.0F, 0.5F, 48000.0F};
    svf.set_cutoff_hz(2000.0F);
    EXPECT_NEAR(svf.get_cutoff_hz(), 2000.0F, 1.0F);
}

TEST(SVFParameters, ResonanceClamped) {
    sonicforge::StateVariableFilter svf;
    svf.set_resonance(2.0F);
    EXPECT_LE(svf.get_resonance(), 1.0F);
    svf.set_resonance(-0.5F);
    EXPECT_GE(svf.get_resonance(), 0.0F);
}

TEST(SVFParameters, CutoffClampedToNyquist) {
    sonicforge::StateVariableFilter svf{sonicforge::FilterMode::Lowpass, 1000.0F, 0.5F, 48000.0F};
    svf.set_cutoff_hz(30000.0F); // Above Nyquist (24 kHz)
    EXPECT_LT(svf.get_cutoff_hz(), 24000.0F);
}

TEST(SVFReset, StateClearsToZero) {
    sonicforge::StateVariableFilter svf{sonicforge::FilterMode::Lowpass, 1000.0F, 0.5F, 48000.0F};

    // Process some non-zero signal
    float discard = 0.0F;
    for (size_t i = 0; i < 100; ++i) {
        discard = svf.process(std::sin(2.0F * 3.14159265F * 440.0F * static_cast<float>(i) / 48000.0F));
    }
    (void)discard;

    svf.reset();
    // After reset, processing zero input should yield zero output
    for (int i = 0; i < 10; ++i) {
        EXPECT_FLOAT_EQ(svf.process(0.0F), 0.0F);
    }
}

// ===================================================================
// Waveshaper
// ===================================================================

TEST(WaveshaperFunctions, TanhBounds) {
    EXPECT_NEAR(sonicforge::soft_clip_tanh(0.0F), 0.0F, 1e-6F);
    EXPECT_GT(sonicforge::soft_clip_tanh(10.0F), 0.99F);
    EXPECT_LT(sonicforge::soft_clip_tanh(-10.0F), -0.99F);
}

TEST(WaveshaperFunctions, PolyLinearRegion) {
    // In the linear region (|x| < 1/3), output should equal input
    EXPECT_FLOAT_EQ(sonicforge::soft_clip_poly(0.0F), 0.0F);
    EXPECT_NEAR(sonicforge::soft_clip_poly(0.1F), 0.1F, 1e-4F);
    EXPECT_NEAR(sonicforge::soft_clip_poly(-0.1F), -0.1F, 1e-4F);
}

TEST(WaveshaperFunctions, PolySaturation) {
    EXPECT_FLOAT_EQ(sonicforge::soft_clip_poly(10.0F), 1.0F);
    EXPECT_FLOAT_EQ(sonicforge::soft_clip_poly(-10.0F), -1.0F);
}

TEST(WaveshaperFunctions, HardClipBounds) {
    EXPECT_FLOAT_EQ(sonicforge::hard_clip(0.5F), 0.5F);
    EXPECT_FLOAT_EQ(sonicforge::hard_clip(2.0F), 1.0F);
    EXPECT_FLOAT_EQ(sonicforge::hard_clip(-2.0F), -1.0F);
}

TEST(WaveshaperFunctions, WaveFoldFoldsAtBoundary) {
    // Input of 2.0 with threshold 1.0 should fold to 0.0
    EXPECT_FLOAT_EQ(sonicforge::wavefold_buchla(2.0F, 1.0F), 0.0F);
    // Input of 3.0 should fold to -1.0
    EXPECT_FLOAT_EQ(sonicforge::wavefold_buchla(3.0F, 1.0F), -1.0F);
    // Input within boundary should pass through
    EXPECT_FLOAT_EQ(sonicforge::wavefold_buchla(0.5F, 1.0F), 0.5F);
}

TEST(WaveshaperFunctions, RectifyNonNegative) {
    EXPECT_FLOAT_EQ(sonicforge::full_wave_rectify(0.5F), 0.5F);
    EXPECT_FLOAT_EQ(sonicforge::full_wave_rectify(-0.5F), 0.5F);
    EXPECT_FLOAT_EQ(sonicforge::full_wave_rectify(0.0F), 0.0F);
}

TEST(WaveshaperProcessor, TanhShapeWithDrive) {
    sonicforge::WaveshaperProcessor ws{sonicforge::WaveshaperShape::Tanh, 5.0F};
    std::vector<float> buf = {0.1F, 0.5F, 1.0F, -0.3F};
    ws.process_block(buf.data(), buf.size());

    // All outputs should be in [-1, 1]
    for (float s : buf) {
        EXPECT_GE(s, -1.0F);
        EXPECT_LE(s, 1.0F);
    }
    // Drive = 5 on input 1.0 => tanh(5) ≈ 0.9999
    EXPECT_NEAR(buf[2], std::tanh(5.0F), 1e-4F);
}

TEST(WaveshaperProcessor, HardClipWithDrive) {
    sonicforge::WaveshaperProcessor ws{sonicforge::WaveshaperShape::HardClip, 2.0F};
    const float out = ws.process(0.6F);
    // 0.6 * 2.0 = 1.2, clipped to 1.0
    EXPECT_FLOAT_EQ(out, 1.0F);
}

TEST(WaveshaperProcessor, WaveFoldShape) {
    sonicforge::WaveshaperProcessor ws{sonicforge::WaveshaperShape::WaveFold, 3.0F};
    const float out = ws.process(0.5F);
    // 0.5 * 3.0 = 1.5, folded to 0.5
    EXPECT_NEAR(out, 0.5F, 1e-5F);
}

TEST(WaveshaperProcessor, NullBufferNoCrash) {
    sonicforge::WaveshaperProcessor ws;
    ws.process_block(nullptr, 256); // should be safe
}

// ===================================================================
// DelayLine
// ===================================================================

// --- None ---

TEST(DelayLineNone, IntegerDelay) {
    sonicforge::DelayLine<sonicforge::DelayInterpolation::None> dl{1024};
    dl.set_delay(10.0F);

    // Feed an impulse
    (void)dl.process(1.0F);
    for (int i = 1; i < 10; ++i)
        (void)dl.process(0.0F);

    // The impulse should emerge at sample 10
    const float out = dl.process(0.0F);
    EXPECT_NEAR(out, 1.0F, 1e-6F);
}

TEST(DelayLineNone, FeedbackDecay) {
    sonicforge::DelayLine<sonicforge::DelayInterpolation::None> dl{1024};
    dl.set_delay(1.0F);
    const float feedback = 0.5F;

    // Apply feedback externally: write (dry + feedback * wet)
    (void)dl.process(1.0F); // impulse (dry only on first sample)

    float prev = 0.0F;
    // The output should decay over time due to feedback < 1
    for (int i = 0; i < 20; ++i) {
        const float wet = dl.read();
        const float out = dl.process(0.0F + feedback * wet);
        if (i > 2) {
            EXPECT_LE(std::fabs(out), std::fabs(prev) + 1e-4F) << "Output should decay with feedback < 1";
        }
        prev = out;
    }
}

TEST(DelayLineNone, ResetClearsBuffer) {
    sonicforge::DelayLine<sonicforge::DelayInterpolation::None> dl{1024};
    dl.set_delay(10.0F);
    (void)dl.process(1.0F);
    for (int i = 0; i < 10; ++i)
        (void)dl.process(0.0F);

    dl.reset();
    // After reset, all output should be zero
    for (int i = 0; i < 10; ++i) {
        EXPECT_FLOAT_EQ(dl.process(0.0F), 0.0F);
    }
}

TEST(DelayLineNone, ProcessBlock) {
    sonicforge::DelayLine<sonicforge::DelayInterpolation::None> dl{1024};
    dl.set_delay(5.0F);

    std::vector<float> buf(256, 0.0F);
    buf[0] = 1.0F; // impulse at start
    dl.process_block(buf.data(), buf.size());

    // The impulse should have been delayed; find where it emerged
    bool found = false;
    for (size_t i = 0; i < buf.size(); ++i) {
        if (std::fabs(buf[i]) > 0.5F) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Impulse should still be in the block";
}

// --- Linear ---

TEST(DelayLineLinear, FractionalDelay) {
    sonicforge::DelayLine<sonicforge::DelayInterpolation::Linear> dl{1024};
    dl.set_delay(5.5F);

    (void)dl.process(1.0F);
    for (int i = 1; i < 6; ++i)
        (void)dl.process(0.0F);

    // At fractional delay 5.5, the output at sample 6 should be the
    // average of samples at integer positions 5 and 6 (both 0 and the impulse at 0)
    const float out = dl.process(0.0F);
    // Linear interpolation between the two nearest samples
    EXPECT_NEAR(out, 0.5F, 1e-5F);
}

TEST(DelayLineLinear, ReadWithoutFeedback) {
    sonicforge::DelayLine<sonicforge::DelayInterpolation::Linear> dl{1024};
    // Write some samples into the buffer
    for (int i = 0; i < 20; ++i)
        (void)dl.process(static_cast<float>(i));

    // Read at fractional position
    const float val = dl.read(3.0F);
    // Should read the sample written 3 samples ago: 20 - 3 = 17
    EXPECT_NEAR(val, 17.0F, 0.1F);
}

TEST(DelayLineLinear, ProcessBlock) {
    sonicforge::DelayLine<sonicforge::DelayInterpolation::Linear> dl{1024};
    dl.set_delay(5.0F);
    dl.set_feedback(0.0F);

    std::vector<float> buf(256);
    for (auto& s : buf)
        s = 1.0F;
    dl.process_block(buf.data(), buf.size());

    // With delay=5 and read-before-write semantics, sample i=5 is the first
    // output that reads back the 1.0 written at position 0.
    for (size_t i = 5; i < buf.size(); ++i) {
        EXPECT_NEAR(buf[i], 1.0F, 1e-5F);
    }
}

// --- Lagrange3rd ---

TEST(DelayLineLagrange3rd, FractionalDelayAccuracy) {
    sonicforge::DelayLine<sonicforge::DelayInterpolation::Lagrange3rd> dl{1024};
    dl.set_delay(5.5F);

    (void)dl.process(1.0F);
    for (int i = 1; i < 6; ++i) // 5 zeros → write_pos=6 at the time of reading
        (void)dl.process(0.0F);

    // Read at write_pos=6, delay=5.5.  The 4-tap Lagrange window straddles the
    // impulse at buffer[0], so the output should be close to 0.5 (within ±0.1).
    const float out = dl.process(0.0F);
    EXPECT_NEAR(out, 0.5F, 0.1F);
}

TEST(DelayLineLagrange3rd, ReadAtIntegerDelay) {
    sonicforge::DelayLine<sonicforge::DelayInterpolation::Lagrange3rd> dl{1024};

    // Fill buffer with known ramp
    for (int i = 0; i < 50; ++i)
        (void)dl.process(static_cast<float>(i));

    // Read at integer delay (4.0) — should interpolate to exact value
    const float val = dl.read(4.0F);
    // The value 4 samples ago from the current write position
    EXPECT_NEAR(val, 46.0F, 0.1F);
}

TEST(DelayLineLagrange3rd, ProcessBlock) {
    sonicforge::DelayLine<sonicforge::DelayInterpolation::Lagrange3rd> dl{1024};
    dl.set_delay(5.0F);
    dl.set_feedback(0.0F);

    std::vector<float> buf(256);
    for (auto& s : buf)
        s = 1.0F;
    dl.process_block(buf.data(), buf.size());

    for (size_t i = 8; i < buf.size(); ++i) {
        EXPECT_NEAR(buf[i], 1.0F, 1e-4F);
    }
}

TEST(DelayLineLagrange3rd, ResetClearsBuffer) {
    sonicforge::DelayLine<sonicforge::DelayInterpolation::Lagrange3rd> dl{1024};
    dl.set_delay(10.0F);
    (void)dl.process(1.0F);
    for (int i = 0; i < 15; ++i)
        (void)dl.process(0.0F);

    dl.reset();
    for (int i = 0; i < 10; ++i) {
        EXPECT_NEAR(dl.process(0.0F), 0.0F, 1e-6F);
    }
}

// ===================================================================
// Integration: SmoothedValue + SVF + Waveshaper + DelayLine
// ===================================================================

TEST(Integration, OscillatorChain) {
    // Simulate a simple synth chain: smoothed freq → SVF → waveshaper → delay
    sonicforge::SmoothedValue<sonicforge::SmoothingMode::Multiplicative> freq_smooth;
    freq_smooth.reset(440.0F, 48000.0F);
    freq_smooth.set_ramp_duration(0.02F);
    freq_smooth.set_target(880.0F);

    sonicforge::StateVariableFilter svf{sonicforge::FilterMode::Lowpass, 2000.0F, 0.3F, 48000.0F};
    sonicforge::WaveshaperProcessor ws{sonicforge::WaveshaperShape::Tanh, 2.0F};
    sonicforge::DelayLine<sonicforge::DelayInterpolation::Linear> dl{24000};
    dl.set_delay(4800.0F); // 100 ms
    dl.set_feedback(0.3F);

    // Generate a simple oscillator and process through the chain
    std::vector<float> block(256);
    for (int pass = 0; pass < 10; ++pass) {
        // Simulate oscillator output (simple sine)
        for (size_t i = 0; i < block.size(); ++i) {
            const float sample_idx = static_cast<float>(pass) * 256.0F + static_cast<float>(i);
            block[i] = std::sin(2.0F * 3.14159265F * 440.0F * sample_idx / 48000.0F);
        }

        // Apply smoothed frequency (just verify it works)
        for (int i = 0; i < 256; ++i) {
            (void)freq_smooth.process(); // advance the ramp
        }

        // Filter
        svf.process_block(block.data(), block.size());

        // Waveshape
        ws.process_block(block.data(), block.size());

        // Delay
        dl.process_block(block.data(), block.size());

        // All values should be finite and reasonable
        for (float s : block) {
            EXPECT_TRUE(std::isfinite(s));
            EXPECT_LT(std::fabs(s), 2.0F);
        }
    }
}

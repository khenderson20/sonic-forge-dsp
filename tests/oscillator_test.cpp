/**
 * @file oscillator_test.cpp
 * @brief Unit tests for the SonicForge oscillator
 *
 * ## What is tested
 *
 * 1. Construction (default and custom parameters, invalid enum fallback)
 * 2. Output range — all waveforms must stay within [-1, 1]
 * 3. Waveform behaviour — phase relationships, peak positions
 * 4. Parameter setters — frequency, waveform, phase; including invalid inputs
 * 5. Block processing — block path must match single-sample path
 * 6. Phase wrapping
 * 7. PolyBLEP anti-aliasing — discontinuities are smoothed for saw / square
 * 8. LUT sine accuracy — LUT output must be within 1e-4 of std::sin
 *
 * ## Floating-point conventions
 *
 * All literals use uppercase F suffix (0.0F, 440.0F, …) per the project's
 * readability-uppercase-literal-suffix clang-tidy rule.
 *
 * @author SonicForge DSP
 * @copyright MIT License
 */

#include <sonicforge/oscillator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// =============================================================================
// Construction Tests
// =============================================================================

TEST(OscillatorConstruction, DefaultValues) {
    const sonicforge::Oscillator osc;

    EXPECT_NEAR(osc.get_frequency(), 440.0F, 0.001F);
    EXPECT_NEAR(osc.get_sample_rate(), 48000.0F, 0.001F);
    EXPECT_EQ(osc.get_waveform(), sonicforge::Waveform::SINE);
    EXPECT_NEAR(osc.get_phase(), 0.0F, 0.001F);
}

TEST(OscillatorConstruction, CustomParameters) {
    const sonicforge::Oscillator osc(sonicforge::Waveform::SAW, 880.0F, 44100.0F);

    EXPECT_NEAR(osc.get_frequency(), 880.0F, 0.001F);
    EXPECT_NEAR(osc.get_sample_rate(), 44100.0F, 0.001F);
    EXPECT_EQ(osc.get_waveform(), sonicforge::Waveform::SAW);
}

/**
 * Fix #2 — invalid waveform enum values must silently fall back to SINE.
 *
 * The Waveform enum is backed by uint8_t; valid values are 0–3.  Casting
 * an out-of-range value simulates corrupted / deserialised data.
 */
TEST(OscillatorConstruction, InvalidWaveformFallsBackToSine) {
    // Intentional out-of-range cast to test validation logic.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    const auto invalid = static_cast<sonicforge::Waveform>(42U);
    const sonicforge::Oscillator osc(invalid, 440.0F);

    EXPECT_EQ(osc.get_waveform(), sonicforge::Waveform::SINE);
}

TEST(OscillatorConstruction, InvalidFrequencyFallsBackToDefault) {
    const sonicforge::Oscillator osc_neg(sonicforge::Waveform::SINE, -100.0F);
    EXPECT_NEAR(osc_neg.get_frequency(), 440.0F, 0.001F);

    const sonicforge::Oscillator osc_zero(sonicforge::Waveform::SINE, 0.0F);
    EXPECT_NEAR(osc_zero.get_frequency(), 440.0F, 0.001F);
}

// =============================================================================
// Output Range Tests
// =============================================================================

TEST(OscillatorOutputRange, SineStaysWithinBounds) {
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0F);

    for (int i = 0; i < 48000; ++i) {
        const float sample = osc.process();
        ASSERT_GE(sample, -1.0F) << "at sample " << i;
        ASSERT_LE(sample, 1.0F) << "at sample " << i;
    }
}

TEST(OscillatorOutputRange, SawStaysWithinBounds) {
    sonicforge::Oscillator osc(sonicforge::Waveform::SAW, 440.0F);

    for (int i = 0; i < 48000; ++i) {
        const float sample = osc.process();
        ASSERT_GE(sample, -1.0F) << "at sample " << i;
        ASSERT_LE(sample, 1.0F) << "at sample " << i;
    }
}

/**
 * With PolyBLEP anti-aliasing the square wave is no longer exactly ±1 at the
 * transition points.  The output stays within [-1, 1] but has smooth
 * intermediate values near the discontinuities.  A tolerance of 1e-6 accounts
 * for floating-point rounding at the boundary.
 */
TEST(OscillatorOutputRange, SquareStaysWithinBounds) {
    sonicforge::Oscillator osc(sonicforge::Waveform::SQUARE, 440.0F);

    for (int i = 0; i < 48000; ++i) {
        const float sample = osc.process();
        ASSERT_GE(sample, -1.0F - 1e-6F) << "at sample " << i;
        ASSERT_LE(sample, 1.0F + 1e-6F) << "at sample " << i;
    }
}

TEST(OscillatorOutputRange, TriangleStaysWithinBounds) {
    sonicforge::Oscillator osc(sonicforge::Waveform::TRIANGLE, 440.0F);

    for (int i = 0; i < 48000; ++i) {
        const float sample = osc.process();
        ASSERT_GE(sample, -1.0F) << "at sample " << i;
        ASSERT_LE(sample, 1.0F) << "at sample " << i;
    }
}

// =============================================================================
// Waveform Behaviour Tests
// =============================================================================

TEST(OscillatorWaveform, SineStartsAtZero) {
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0F);
    EXPECT_NEAR(osc.process(), 0.0F, 0.001F);
}

/**
 * The sine peak should occur at phase = 0.25 (one quarter of the period).
 * At 480 Hz / 48 kHz the period is exactly 100 samples, so the peak falls
 * around sample index 25.
 */
TEST(OscillatorWaveform, SinePeakAtQuarterPeriod) {
    constexpr float frequency = 480.0F;
    constexpr float sample_rate = 48000.0F;

    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, frequency, sample_rate);

    float max_sample = -2.0F;
    std::size_t max_index = 0U;

    for (std::size_t i = 0U; i < 100U; ++i) {
        const float sample = osc.process();
        if (sample > max_sample) {
            max_sample = sample;
            max_index = i;
        }
    }

    EXPECT_NEAR(max_sample, 1.0F, 0.01F);
    EXPECT_GE(max_index, 24U);
    EXPECT_LE(max_index, 26U);
}

// =============================================================================
// Parameter Tests
// =============================================================================

TEST(OscillatorParameters, SetFrequency) {
    sonicforge::Oscillator osc;

    osc.set_frequency(1000.0F);
    EXPECT_NEAR(osc.get_frequency(), 1000.0F, 0.001F);

    osc.set_frequency(100.0F);
    EXPECT_NEAR(osc.get_frequency(), 100.0F, 0.001F);
}

TEST(OscillatorParameters, SetWaveform) {
    sonicforge::Oscillator osc;

    osc.set_waveform(sonicforge::Waveform::SQUARE);
    EXPECT_EQ(osc.get_waveform(), sonicforge::Waveform::SQUARE);

    osc.set_waveform(sonicforge::Waveform::TRIANGLE);
    EXPECT_EQ(osc.get_waveform(), sonicforge::Waveform::TRIANGLE);
}

/**
 * Fix #2 — set_waveform() must silently ignore invalid enum values and
 * leave the waveform unchanged.
 */
TEST(OscillatorParameters, SetWaveformInvalidIgnored) {
    sonicforge::Oscillator osc(sonicforge::Waveform::SAW, 440.0F);
    // Intentional out-of-range cast to test validation logic.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    const auto invalid = static_cast<sonicforge::Waveform>(200U);
    osc.set_waveform(invalid);

    EXPECT_EQ(osc.get_waveform(), sonicforge::Waveform::SAW);
}

TEST(OscillatorParameters, PhaseReset) {
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0F);

    for (int i = 0; i < 100; ++i) {
        (void)osc.process();
    }
    EXPECT_GT(osc.get_phase(), 0.0F);

    osc.reset_phase();
    EXPECT_NEAR(osc.get_phase(), 0.0F, 0.0001F);
    EXPECT_NEAR(osc.process(), 0.0F, 0.001F);
}

// =============================================================================
// Processing Tests
// =============================================================================

TEST(OscillatorProcessing, BlockMatchesSampleBySample) {
    sonicforge::Oscillator osc1(sonicforge::Waveform::SINE, 440.0F);
    sonicforge::Oscillator osc2(sonicforge::Waveform::SINE, 440.0F);

    constexpr std::size_t block_size = 256U;
    std::array<float, block_size> block_buffer{};

    osc1.process_block(block_buffer.data(), block_size);

    for (std::size_t i = 0U; i < block_size; ++i) {
        const float actual = osc2.process();
        EXPECT_NEAR(block_buffer[i], actual, 0.00001F) << "at sample " << i;
    }
}

TEST(OscillatorProcessing, PhaseWrapsAfterOneCycle) {
    constexpr float frequency = 100.0F;
    constexpr float sample_rate = 48000.0F;

    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, frequency, sample_rate);

    const auto samples_per_cycle = static_cast<std::size_t>(sample_rate / frequency);
    for (std::size_t i = 0U; i < samples_per_cycle; ++i) {
        (void)osc.process();
    }

    const float phase = osc.get_phase();
    EXPECT_TRUE(phase < 0.01F || phase > 0.99F) << "phase = " << phase;
}

// =============================================================================
// PolyBLEP Anti-Aliasing Tests (Fix #1)
// =============================================================================

/**
 * A PolyBLEP-corrected sawtooth must not produce the large instantaneous jump
 * (~2.0) of the naive implementation.  Each consecutive sample difference must
 * stay below 1.5 at all tested frequencies.
 */
TEST(OscillatorPolyBLEP, SawHasSmoothedTransitions) {
    constexpr float sr = 48000.0F;

    for (const float freq : {100.0F, 1000.0F, 4800.0F}) {
        sonicforge::Oscillator osc(sonicforge::Waveform::SAW, freq, sr);
        float prev = osc.process();

        const int samples = static_cast<int>(2.0F * sr / freq);
        for (int i = 0; i < samples; ++i) {
            const float cur = osc.process();
            ASSERT_LE(std::fabs(cur - prev), 1.5F)
                << "freq=" << freq << " sample=" << i
                << " delta=" << std::fabs(cur - prev);
            prev = cur;
        }
    }
}

/**
 * The PolyBLEP square wave must have smooth transitions at both discontinuity
 * points.  Consecutive sample deltas must never exceed 1.5.
 */
TEST(OscillatorPolyBLEP, SquareHasSmoothedTransitions) {
    constexpr float sr = 48000.0F;

    for (const float freq : {100.0F, 1000.0F, 4800.0F}) {
        sonicforge::Oscillator osc(sonicforge::Waveform::SQUARE, freq, sr);
        float prev = osc.process();

        const int samples = static_cast<int>(2.0F * sr / freq);
        for (int i = 0; i < samples; ++i) {
            const float cur = osc.process();
            ASSERT_LE(std::fabs(cur - prev), 1.5F)
                << "freq=" << freq << " sample=" << i
                << " delta=" << std::fabs(cur - prev);
            prev = cur;
        }
    }
}

// =============================================================================
// LUT Sine Accuracy Test (Fix #11)
// =============================================================================

/**
 * The 4096-entry LUT with linear interpolation should produce sine values
 * within 1e-4 of std::sin.  Expected peak error is ~2.3e-6 (≈ −113 dB);
 * we use 1e-4 as a generous tolerance for compiler variation.
 */
TEST(OscillatorSineLUT, AccuracyWithinTolerance) {
    constexpr float frequency = 480.0F;
    constexpr float sample_rate = 48000.0F;
    constexpr float dt = frequency / sample_rate;
    constexpr float two_pi = 2.0F * 3.14159265358979323846F;

    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, frequency, sample_rate);

    float max_error = 0.0F;
    float ref_phase = 0.0F;

    for (int i = 0; i < 200; ++i) {
        const float lut_sample = osc.process();
        const float ref_sample = std::sin(two_pi * ref_phase);
        max_error = std::max(max_error, std::fabs(lut_sample - ref_sample));

        ref_phase += dt;
        if (ref_phase >= 1.0F) {
            ref_phase -= 1.0F;
        }
    }

    EXPECT_LT(max_error, 1e-4F) << "LUT sine max error: " << max_error;
}

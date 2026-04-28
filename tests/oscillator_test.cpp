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

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

// =============================================================================
// Minimal Test Framework
// =============================================================================

struct TestResult {
    std::string name;
    bool passed;
};

std::vector<TestResult> g_results;

/** Check if two floats are approximately equal within an absolute epsilon. */
bool approx_equal(float a, float b, float epsilon = 1e-5F) {
    return std::fabs(a - b) < epsilon;
}

/** Run a named test function and record the result. */
void run_test(const std::string& name, bool (*test_func)()) {
    const bool passed = test_func();
    g_results.push_back({name, passed});

    if (passed) {
        std::cout << "  OK  " << name << "\n";
    } else {
        std::cout << "  FAIL  " << name << "\n";
    }
}

/** Print a summary and return 1 on any failure, 0 on all passing. */
int summarize() {
    int passed = 0;
    int failed = 0;

    for (const auto& result : g_results) {
        if (result.passed) {
            ++passed;
        } else {
            ++failed;
        }
    }

    std::cout << "\n================================\n";
    std::cout << "Tests: " << (passed + failed) << " total\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "================================\n";

    return (failed > 0) ? 1 : 0;
}

// =============================================================================
// Construction Tests
// =============================================================================

bool test_default_construction() {
    sonicforge::Oscillator osc;

    if (!approx_equal(osc.get_frequency(), 440.0F, 0.001F)) return false;
    if (!approx_equal(osc.get_sample_rate(), 48000.0F, 0.001F)) return false;
    if (osc.get_waveform() != sonicforge::Waveform::SINE) return false;
    if (!approx_equal(osc.get_phase(), 0.0F, 0.001F)) return false;

    return true;
}

bool test_custom_construction() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SAW, 880.0F, 44100.0F);

    if (!approx_equal(osc.get_frequency(), 880.0F, 0.001F)) return false;
    if (!approx_equal(osc.get_sample_rate(), 44100.0F, 0.001F)) return false;
    if (osc.get_waveform() != sonicforge::Waveform::SAW) return false;

    return true;
}

/**
 * Fix #2 — invalid waveform enum values must silently fall back to SINE.
 *
 * The Waveform enum is backed by uint8_t; valid values are 0–3.  Casting
 * an out-of-range value simulates corrupted / deserialised data.
 */
bool test_invalid_waveform_construction() {
    const auto invalid = static_cast<sonicforge::Waveform>(42U);
    sonicforge::Oscillator osc(invalid, 440.0F);

    // Must fall back to SINE, not produce undefined behaviour
    return osc.get_waveform() == sonicforge::Waveform::SINE;
}

bool test_invalid_frequency_construction() {
    // Non-positive frequency must be replaced by the 440 Hz default
    sonicforge::Oscillator osc_neg(sonicforge::Waveform::SINE, -100.0F);
    if (!approx_equal(osc_neg.get_frequency(), 440.0F, 0.001F)) return false;

    sonicforge::Oscillator osc_zero(sonicforge::Waveform::SINE, 0.0F);
    if (!approx_equal(osc_zero.get_frequency(), 440.0F, 0.001F)) return false;

    return true;
}

// =============================================================================
// Output Range Tests
// =============================================================================

bool test_sine_output_range() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0F);

    for (int i = 0; i < 48000; ++i) {
        const float sample = osc.process();
        if (sample < -1.0F || sample > 1.0F) {
            return false;
        }
    }
    return true;
}

bool test_saw_output_range() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SAW, 440.0F);

    for (int i = 0; i < 48000; ++i) {
        const float sample = osc.process();
        if (sample < -1.0F || sample > 1.0F) {
            return false;
        }
    }
    return true;
}

/**
 * With PolyBLEP anti-aliasing the square wave is no longer exactly ±1 at the
 * transition points.  The output stays within [-1, 1] but has smooth
 * intermediate values near the discontinuities.
 */
bool test_square_output_range() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SQUARE, 440.0F);

    for (int i = 0; i < 48000; ++i) {
        const float sample = osc.process();
        // Allow a tiny floating-point epsilon beyond the hard bounds
        if (sample < -1.0F - 1e-6F || sample > 1.0F + 1e-6F) {
            return false;
        }
    }
    return true;
}

bool test_triangle_output_range() {
    sonicforge::Oscillator osc(sonicforge::Waveform::TRIANGLE, 440.0F);

    for (int i = 0; i < 48000; ++i) {
        const float sample = osc.process();
        if (sample < -1.0F || sample > 1.0F) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// Waveform Behaviour Tests
// =============================================================================

bool test_sine_starts_at_zero() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0F);

    // sin(0) == 0; the very first sample must be ~0
    const float first_sample = osc.process();
    return approx_equal(first_sample, 0.0F, 0.001F);
}

/**
 * The sine peak should occur at phase = 0.25 (one quarter of the period).
 *
 * At 480 Hz / 48 kHz the period is exactly 100 samples, so the peak
 * falls around sample 25.
 */
bool test_sine_peak_position() {
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

    if (!approx_equal(max_sample, 1.0F, 0.01F)) return false;
    if (max_index < 24U || max_index > 26U) return false;

    return true;
}

// =============================================================================
// Parameter Tests
// =============================================================================

bool test_set_frequency() {
    sonicforge::Oscillator osc;

    osc.set_frequency(1000.0F);
    if (!approx_equal(osc.get_frequency(), 1000.0F, 0.001F)) return false;

    osc.set_frequency(100.0F);
    if (!approx_equal(osc.get_frequency(), 100.0F, 0.001F)) return false;

    return true;
}

bool test_set_waveform() {
    sonicforge::Oscillator osc;

    osc.set_waveform(sonicforge::Waveform::SQUARE);
    if (osc.get_waveform() != sonicforge::Waveform::SQUARE) return false;

    osc.set_waveform(sonicforge::Waveform::TRIANGLE);
    if (osc.get_waveform() != sonicforge::Waveform::TRIANGLE) return false;

    return true;
}

/**
 * Fix #2 — set_waveform() must silently ignore invalid enum values and
 * leave the waveform unchanged.
 */
bool test_set_waveform_invalid() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SAW, 440.0F);

    const auto invalid = static_cast<sonicforge::Waveform>(200U);
    osc.set_waveform(invalid);

    // Must remain SAW; must not change to SINE or anything else
    return osc.get_waveform() == sonicforge::Waveform::SAW;
}

bool test_phase_reset() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0F);

    // Advance the phase
    for (int i = 0; i < 100; ++i) {
        (void)osc.process();
    }

    if (osc.get_phase() <= 0.0F) return false;

    osc.reset_phase();
    if (!approx_equal(osc.get_phase(), 0.0F, 0.0001F)) return false;

    // Next sample after reset: sin(0) == 0
    const float sample = osc.process();
    return approx_equal(sample, 0.0F, 0.001F);
}

// =============================================================================
// Processing Tests
// =============================================================================

bool test_block_processing() {
    sonicforge::Oscillator osc1(sonicforge::Waveform::SINE, 440.0F);
    sonicforge::Oscillator osc2(sonicforge::Waveform::SINE, 440.0F);

    constexpr std::size_t BLOCK_SIZE = 256U;
    float block_buffer[BLOCK_SIZE];

    osc1.process_block(block_buffer, BLOCK_SIZE);

    for (std::size_t i = 0U; i < BLOCK_SIZE; ++i) {
        const float single_sample = osc2.process();
        if (!approx_equal(block_buffer[i], single_sample, 0.00001F)) {
            return false;
        }
    }
    return true;
}

bool test_phase_wrapping() {
    constexpr float frequency = 100.0F;
    constexpr float sample_rate = 48000.0F;

    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, frequency, sample_rate);

    const std::size_t samples_per_cycle =
        static_cast<std::size_t>(sample_rate / frequency);

    for (std::size_t i = 0U; i < samples_per_cycle; ++i) {
        (void)osc.process();
    }

    // After one full cycle the phase should be close to 0 again
    const float phase = osc.get_phase();
    return (phase < 0.01F || phase > 0.99F);
}

// =============================================================================
// PolyBLEP Anti-Aliasing Tests (Fix #1)
// =============================================================================

/**
 * A PolyBLEP-corrected sawtooth must not produce the large instantaneous jump
 * (~2.0) of the naive implementation.  Instead, each consecutive sample
 * difference must stay below 1.5 at any frequency.
 *
 * This tests at a high frequency (4800 Hz at 48 kHz, dt = 0.1) where the
 * aliasing from a naive saw is severe — and at a low frequency (100 Hz) where
 * the phase increment is small.
 */
bool test_saw_polyblep_smoothing() {
    constexpr float sr = 48000.0F;

    for (const float freq : {100.0F, 1000.0F, 4800.0F}) {
        sonicforge::Oscillator osc(sonicforge::Waveform::SAW, freq, sr);

        float prev = osc.process();

        // Two full cycles is enough to observe several wrap-around transitions
        const int samples = static_cast<int>(2.0F * sr / freq);
        for (int i = 0; i < samples; ++i) {
            const float cur = osc.process();
            const float delta = std::fabs(cur - prev);
            // Naive saw would jump ~2.0; PolyBLEP must reduce this below 1.5
            if (delta > 1.5F) {
                return false;
            }
            prev = cur;
        }
    }
    return true;
}

/**
 * The PolyBLEP square wave must have smooth transitions at its two
 * discontinuity points (phase = 0 and phase = 0.5).  Near those points the
 * output should pass through intermediate values, not jump instantly.
 *
 * We verify this by asserting that consecutive sample deltas never exceed 1.5.
 */
bool test_square_polyblep_smoothing() {
    constexpr float sr = 48000.0F;

    for (const float freq : {100.0F, 1000.0F, 4800.0F}) {
        sonicforge::Oscillator osc(sonicforge::Waveform::SQUARE, freq, sr);

        float prev = osc.process();

        const int samples = static_cast<int>(2.0F * sr / freq);
        for (int i = 0; i < samples; ++i) {
            const float cur = osc.process();
            const float delta = std::fabs(cur - prev);
            if (delta > 1.5F) {
                return false;
            }
            prev = cur;
        }
    }
    return true;
}

// =============================================================================
// LUT Sine Accuracy Test (Fix #11)
// =============================================================================

/**
 * The 4096-entry LUT with linear interpolation should produce sine values
 * within 1e-4 of std::sin for all phase positions.
 *
 * Expected peak error: ~2.3e-6 (≈ −113 dB); we use 1e-4 as a generous
 * tolerance to be robust against compiler variations.
 */
bool test_sine_lut_accuracy() {
    // 480 Hz / 48 kHz → dt = 0.01 → exactly 100 samples per cycle.
    // This gives a dense and even coverage of the LUT.
    constexpr float frequency = 480.0F;
    constexpr float sample_rate = 48000.0F;
    constexpr float dt = frequency / sample_rate;  // 0.01
    constexpr float TWO_PI = 2.0F * 3.14159265358979323846F;

    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, frequency, sample_rate);

    float max_error = 0.0F;
    float reference_phase = 0.0F;

    // Two full cycles (200 samples)
    for (int i = 0; i < 200; ++i) {
        const float lut_sample = osc.process();
        const float ref_sample = std::sin(TWO_PI * reference_phase);
        const float error = std::fabs(lut_sample - ref_sample);
        max_error = std::max(max_error, error);

        reference_phase += dt;
        if (reference_phase >= 1.0F) {
            reference_phase -= 1.0F;
        }
    }

    return max_error < 1e-4F;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "SonicForge DSP — Oscillator Tests\n";
    std::cout << "===================================\n\n";

    std::cout << "Construction Tests:\n";
    run_test("test_default_construction", test_default_construction);
    run_test("test_custom_construction", test_custom_construction);
    run_test("test_invalid_waveform_construction", test_invalid_waveform_construction);
    run_test("test_invalid_frequency_construction", test_invalid_frequency_construction);

    std::cout << "\nOutput Range Tests:\n";
    run_test("test_sine_output_range", test_sine_output_range);
    run_test("test_saw_output_range", test_saw_output_range);
    run_test("test_square_output_range", test_square_output_range);
    run_test("test_triangle_output_range", test_triangle_output_range);

    std::cout << "\nWaveform Behaviour Tests:\n";
    run_test("test_sine_starts_at_zero", test_sine_starts_at_zero);
    run_test("test_sine_peak_position", test_sine_peak_position);

    std::cout << "\nParameter Tests:\n";
    run_test("test_set_frequency", test_set_frequency);
    run_test("test_set_waveform", test_set_waveform);
    run_test("test_set_waveform_invalid", test_set_waveform_invalid);
    run_test("test_phase_reset", test_phase_reset);

    std::cout << "\nProcessing Tests:\n";
    run_test("test_block_processing", test_block_processing);
    run_test("test_phase_wrapping", test_phase_wrapping);

    std::cout << "\nPolyBLEP Anti-Aliasing Tests:\n";
    run_test("test_saw_polyblep_smoothing", test_saw_polyblep_smoothing);
    run_test("test_square_polyblep_smoothing", test_square_polyblep_smoothing);

    std::cout << "\nLUT Sine Accuracy Tests:\n";
    run_test("test_sine_lut_accuracy", test_sine_lut_accuracy);

    return summarize();
}

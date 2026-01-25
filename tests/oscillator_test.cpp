/**
 * @file oscillator_test.cpp
 * @brief Unit tests for the SonicForge oscillator
 * 
 * This file contains simple unit tests to verify the oscillator works correctly.
 * Uses a minimal test framework (no external dependencies).
 * 
 * ## Learning Points
 * 
 * 1. Testing audio code requires mathematical verification
 * 2. Floating-point comparisons need tolerance (epsilon)
 * 3. Phase accumulation should wrap correctly
 * 4. Waveform output should stay within [-1, 1]
 * 
 * @author SonicForge DSP
 * @copyright MIT License
 */

#include <sonicforge/oscillator.hpp>
#include <iostream>
#include <cmath>
#include <string>
#include <vector>

// =============================================================================
// Minimal Test Framework
// =============================================================================

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

std::vector<TestResult> g_results;

/**
 * @brief Check if two floats are approximately equal
 */
bool approx_equal(float a, float b, float epsilon = 1e-5f) {
    return std::fabs(a - b) < epsilon;
}

/**
 * @brief Run a test and record results
 */
void run_test(const std::string& name, bool (*test_func)()) {
    bool passed = test_func();
    g_results.push_back({name, passed, ""});
    
    if (passed) {
        std::cout << "  ✅ " << name << "\n";
    } else {
        std::cout << "  ❌ " << name << "\n";
    }
}

/**
 * @brief Print summary of all tests
 */
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
    
    return failed > 0 ? 1 : 0;
}

// =============================================================================
// Oscillator Tests
// =============================================================================

/**
 * @brief Test that oscillator constructs with default values
 */
bool test_default_construction() {
    sonicforge::Oscillator osc;
    
    if (!approx_equal(osc.get_frequency(), 440.0f, 0.001f)) return false;
    if (!approx_equal(osc.get_sample_rate(), 48000.0f, 0.001f)) return false;
    if (osc.get_waveform() != sonicforge::Waveform::SINE) return false;
    if (!approx_equal(osc.get_phase(), 0.0f, 0.001f)) return false;
    
    return true;
}

/**
 * @brief Test oscillator construction with custom parameters
 */
bool test_custom_construction() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SAW, 880.0f, 44100.0f);
    
    if (!approx_equal(osc.get_frequency(), 880.0f, 0.001f)) return false;
    if (!approx_equal(osc.get_sample_rate(), 44100.0f, 0.001f)) return false;
    if (osc.get_waveform() != sonicforge::Waveform::SAW) return false;
    
    return true;
}

/**
 * @brief Test that sine oscillator output is within valid range
 */
bool test_sine_output_range() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0f);
    
    // Process many samples and check all are in range
    for (int i = 0; i < 48000; ++i) {  // 1 second of audio
        float sample = osc.process();
        if (sample < -1.0f || sample > 1.0f) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Test that saw oscillator output is within valid range
 */
bool test_saw_output_range() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SAW, 440.0f);
    
    for (int i = 0; i < 48000; ++i) {
        float sample = osc.process();
        if (sample < -1.0f || sample > 1.0f) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Test that square oscillator output is within valid range
 */
bool test_square_output_range() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SQUARE, 440.0f);
    
    for (int i = 0; i < 48000; ++i) {
        float sample = osc.process();
        // Square wave should only be exactly -1 or +1
        if (sample != -1.0f && sample != 1.0f) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Test that triangle oscillator output is within valid range
 */
bool test_triangle_output_range() {
    sonicforge::Oscillator osc(sonicforge::Waveform::TRIANGLE, 440.0f);
    
    for (int i = 0; i < 48000; ++i) {
        float sample = osc.process();
        if (sample < -1.0f || sample > 1.0f) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Test that sine wave starts at zero (phase = 0)
 */
bool test_sine_starts_at_zero() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0f);
    
    float first_sample = osc.process();
    // sin(0) = 0
    return approx_equal(first_sample, 0.0f, 0.0001f);
}

/**
 * @brief Test frequency setter
 */
bool test_set_frequency() {
    sonicforge::Oscillator osc;
    
    osc.set_frequency(1000.0f);
    if (!approx_equal(osc.get_frequency(), 1000.0f, 0.001f)) return false;
    
    osc.set_frequency(100.0f);
    if (!approx_equal(osc.get_frequency(), 100.0f, 0.001f)) return false;
    
    return true;
}

/**
 * @brief Test waveform setter
 */
bool test_set_waveform() {
    sonicforge::Oscillator osc;
    
    osc.set_waveform(sonicforge::Waveform::SQUARE);
    if (osc.get_waveform() != sonicforge::Waveform::SQUARE) return false;
    
    osc.set_waveform(sonicforge::Waveform::TRIANGLE);
    if (osc.get_waveform() != sonicforge::Waveform::TRIANGLE) return false;
    
    return true;
}

/**
 * @brief Test phase reset
 */
bool test_phase_reset() {
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0f);
    
    // Process some samples to advance phase
    for (int i = 0; i < 100; ++i) {
        (void)osc.process();
    }
    
    // Phase should have advanced
    if (osc.get_phase() <= 0.0f) return false;
    
    // Reset phase
    osc.reset_phase();
    if (!approx_equal(osc.get_phase(), 0.0f, 0.0001f)) return false;
    
    // Next sample should be sin(0) = 0
    float sample = osc.process();
    return approx_equal(sample, 0.0f, 0.0001f);
}

/**
 * @brief Test block processing produces same results as single sample processing
 */
bool test_block_processing() {
    sonicforge::Oscillator osc1(sonicforge::Waveform::SINE, 440.0f);
    sonicforge::Oscillator osc2(sonicforge::Waveform::SINE, 440.0f);
    
    constexpr size_t BLOCK_SIZE = 256;
    float block_buffer[BLOCK_SIZE];
    
    // Process block with osc1
    osc1.process_block(block_buffer, BLOCK_SIZE);
    
    // Process same number of samples one-by-one with osc2
    for (size_t i = 0; i < BLOCK_SIZE; ++i) {
        float single_sample = osc2.process();
        if (!approx_equal(block_buffer[i], single_sample, 0.00001f)) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Test that phase wraps correctly
 */
bool test_phase_wrapping() {
    const float frequency = 100.0f;  // 100 Hz
    const float sample_rate = 48000.0f;
    
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, frequency, sample_rate);
    
    // One complete cycle = sample_rate / frequency samples
    const size_t samples_per_cycle = static_cast<size_t>(sample_rate / frequency);
    
    // Process exactly one cycle
    for (size_t i = 0; i < samples_per_cycle; ++i) {
        (void)osc.process();
    }
    
    // Phase should be back near 0 (with some floating point tolerance)
    float phase = osc.get_phase();
    return (phase < 0.01f || phase > 0.99f);
}

/**
 * @brief Test that sine peak occurs at 1/4 cycle (phase = 0.25)
 */
bool test_sine_peak_position() {
    const float frequency = 480.0f;  // Nice divisor of 48000
    const float sample_rate = 48000.0f;
    
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, frequency, sample_rate);
    
    float max_sample = -2.0f;
    size_t max_index = 0;
    
    // Process 100 samples (one full cycle)
    for (size_t i = 0; i < 100; ++i) {
        float sample = osc.process();
        if (sample > max_sample) {
            max_sample = sample;
            max_index = i;
        }
    }
    
    // Max should be near 1.0 and occur around sample 24-26
    if (!approx_equal(max_sample, 1.0f, 0.01f)) return false;
    if (max_index < 24 || max_index > 26) return false;
    
    return true;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "SonicForge DSP - Oscillator Tests\n";
    std::cout << "==================================\n\n";
    
    std::cout << "Construction Tests:\n";
    run_test("test_default_construction", test_default_construction);
    run_test("test_custom_construction", test_custom_construction);
    
    std::cout << "\nOutput Range Tests:\n";
    run_test("test_sine_output_range", test_sine_output_range);
    run_test("test_saw_output_range", test_saw_output_range);
    run_test("test_square_output_range", test_square_output_range);
    run_test("test_triangle_output_range", test_triangle_output_range);
    
    std::cout << "\nWaveform Behavior Tests:\n";
    run_test("test_sine_starts_at_zero", test_sine_starts_at_zero);
    run_test("test_sine_peak_position", test_sine_peak_position);
    
    std::cout << "\nParameter Tests:\n";
    run_test("test_set_frequency", test_set_frequency);
    run_test("test_set_waveform", test_set_waveform);
    run_test("test_phase_reset", test_phase_reset);
    
    std::cout << "\nProcessing Tests:\n";
    run_test("test_block_processing", test_block_processing);
    run_test("test_phase_wrapping", test_phase_wrapping);
    
    return summarize();
}

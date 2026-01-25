/**
 * @file oscillator.cpp
 * @brief Implementation of digital oscillator for audio synthesis
 *
 * @author SonicForge DSP
 * @copyright MIT License
 */

#include "sonicforge/oscillator.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace sonicforge {

// =============================================================================
// Constructor
// =============================================================================

Oscillator::Oscillator(Waveform waveform, float frequency, float sample_rate)
    : phase_{0.0f}, frequency_{frequency}, sample_rate_{sample_rate}, waveform_{waveform} {
    // Validate inputs - clamp to reasonable ranges
    if (frequency <= 0.0f) {
        frequency_.store(440.0f, std::memory_order_relaxed);
    }
    if (sample_rate <= 0.0f) {
        sample_rate_.store(48000.0f, std::memory_order_relaxed);
    }
}

// =============================================================================
// Main Processing Functions
// =============================================================================

float Oscillator::process() noexcept {
    float output = 0.0f;

    // Generate sample based on current waveform
    switch (waveform_.load(std::memory_order_relaxed)) {
        case Waveform::SINE:
            output = generate_sine();
            break;
        case Waveform::SAW:
            output = generate_saw();
            break;
        case Waveform::SQUARE:
            output = generate_square();
            break;
        case Waveform::TRIANGLE:
            output = generate_triangle();
            break;
    }

    // Advance phase for next sample
    advance_phase();

    return output;
}

void Oscillator::process_block(float* buffer, std::size_t num_samples) noexcept {
    if (buffer == nullptr || num_samples == 0) {
        return;
    }

    // Cache waveform to avoid repeated atomic loads
    const Waveform current_waveform = waveform_.load(std::memory_order_relaxed);

    // Process each sample
    // Note: For learning purposes, this is straightforward. Production code
    // might use SIMD optimizations or lookup tables for sine.
    for (std::size_t i = 0; i < num_samples; ++i) {
        switch (current_waveform) {
            case Waveform::SINE:
                buffer[i] = generate_sine();
                break;
            case Waveform::SAW:
                buffer[i] = generate_saw();
                break;
            case Waveform::SQUARE:
                buffer[i] = generate_square();
                break;
            case Waveform::TRIANGLE:
                buffer[i] = generate_triangle();
                break;
        }
        advance_phase();
    }
}

// =============================================================================
// Waveform Generation Functions
// =============================================================================

float Oscillator::generate_sine() const noexcept {
    // Sine wave: sin(2π * phase)
    // This is the purest waveform - a single frequency with no harmonics
    //
    // Learning note: std::sin() can be slow. Production oscillators often use:
    // - Lookup tables with interpolation
    // - Polynomial approximations
    // - SIMD-optimized math libraries
    return std::sin(TWO_PI * phase_);
}

float Oscillator::generate_saw() const noexcept {
    // Sawtooth wave: Linear ramp from -1 to +1
    // Rich in harmonics: contains all harmonics at amplitude 1/n
    //
    // Formula: 2 * phase - 1
    // When phase goes 0→1, output goes -1→+1
    //
    // Learning note: This naive implementation creates aliasing artifacts
    // at high frequencies. Production code uses PolyBLEP anti-aliasing.
    return 2.0f * phase_ - 1.0f;
}

float Oscillator::generate_square() const noexcept {
    // Square wave: Alternates between -1 and +1
    // Contains only odd harmonics at amplitude 1/n
    //
    // Learning note: Like saw, this naive implementation aliases.
    // PolyBLEP applies a polynomial correction at discontinuities.
    return (phase_ < 0.5f) ? 1.0f : -1.0f;
}

float Oscillator::generate_triangle() const noexcept {
    // Triangle wave: Linear ramps up and down
    // Contains only odd harmonics at amplitude 1/n²
    // (softer than square due to faster harmonic rolloff)
    //
    // Implementation:
    // - First half (0 to 0.5): ramp from -1 to +1
    // - Second half (0.5 to 1): ramp from +1 to -1
    //
    // Learning note: Triangle waves don't have discontinuities,
    // so they alias less than saw/square even without anti-aliasing.
    if (phase_ < 0.5f) {
        // Rising edge: phase 0→0.5 maps to output -1→+1
        return 4.0f * phase_ - 1.0f;
    } else {
        // Falling edge: phase 0.5→1 maps to output +1→-1
        return 3.0f - 4.0f * phase_;
    }
}

// =============================================================================
// Phase Management
// =============================================================================

void Oscillator::advance_phase() noexcept {
    // Calculate phase increment based on frequency and sample rate
    //
    // phase_increment = frequency / sample_rate
    //
    // Example: 440 Hz at 48000 sample rate
    // - Each sample advances phase by 440/48000 = 0.00917
    // - After 48000 samples (1 second), phase has cycled 440 times
    //
    // This is the fundamental DSP concept of phase accumulation!

    const float freq = frequency_.load(std::memory_order_relaxed);
    const float sr = sample_rate_.load(std::memory_order_relaxed);

    const float phase_increment = freq / sr;
    phase_ += phase_increment;

    // Wrap phase to [0, 1) range
    // This prevents floating-point precision loss over long playback
    //
    // Note: Using subtraction instead of fmod() for performance
    // This works correctly because phase_increment is always < 1
    // (unless frequency > sample_rate, which would alias anyway)
    while (phase_ >= 1.0f) {
        phase_ -= 1.0f;
    }
    while (phase_ < 0.0f) {
        phase_ += 1.0f;
    }
}

// =============================================================================
// Parameter Setters (Thread-Safe)
// =============================================================================

void Oscillator::set_frequency(float frequency) noexcept {
    // Clamp to valid range to prevent negative frequencies or DC
    // Upper limit prevents aliasing above Nyquist frequency
    const float sr = sample_rate_.load(std::memory_order_relaxed);
    const float nyquist = sr * 0.5f;

    frequency = std::clamp(frequency, 0.1f, nyquist);
    frequency_.store(frequency, std::memory_order_relaxed);
}

void Oscillator::set_waveform(Waveform waveform) noexcept {
    waveform_.store(waveform, std::memory_order_relaxed);
}

void Oscillator::set_sample_rate(float sample_rate) noexcept {
    if (sample_rate > 0.0f) {
        sample_rate_.store(sample_rate, std::memory_order_relaxed);
    }
}

void Oscillator::reset_phase() noexcept {
    phase_ = 0.0f;
}

void Oscillator::set_phase(float phase) noexcept {
    // Wrap to valid range [0, 1)
    phase = phase - std::floor(phase);
    phase_ = phase;
}

// =============================================================================
// Parameter Getters
// =============================================================================

float Oscillator::get_frequency() const noexcept {
    return frequency_.load(std::memory_order_relaxed);
}

Waveform Oscillator::get_waveform() const noexcept {
    return waveform_.load(std::memory_order_relaxed);
}

float Oscillator::get_sample_rate() const noexcept {
    return sample_rate_.load(std::memory_order_relaxed);
}

float Oscillator::get_phase() const noexcept {
    return phase_;
}

}  // namespace sonicforge

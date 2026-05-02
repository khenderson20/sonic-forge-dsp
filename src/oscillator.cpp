/**
 * @file oscillator.cpp
 * @brief Implementation of digital oscillator for audio synthesis
 */
#include "sonicforge/oscillator.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>

namespace sonicforge {

namespace {

constexpr std::size_t LUT_SIZE = 4096;
constexpr float TWO_PI = 2.0F * 3.14159265358979323846F;

const std::array<float, LUT_SIZE + 1>& sine_lut() noexcept {
    alignas(64) static const std::array<float, LUT_SIZE + 1> LUT = []() noexcept {
        std::array<float, LUT_SIZE + 1> arr{};
        for (std::size_t i = 0; i <= LUT_SIZE; ++i) {
            const float phase = static_cast<float>(i) / static_cast<float>(LUT_SIZE);
            arr[i] = std::sin(TWO_PI * phase);
        }
        return arr;
    }();
    return LUT;
}

inline float wrap_phase(float phase) noexcept {
    return phase - std::floor(phase);
}

inline float lut_interpolate(float phase) noexcept {
    const std::array<float, LUT_SIZE + 1>& lut = sine_lut();
    const float scaled = phase * static_cast<float>(LUT_SIZE);
    const std::size_t idx0 = static_cast<std::size_t>(scaled) & (LUT_SIZE - 1);
    const std::size_t idx1 = (idx0 + 1) & (LUT_SIZE - 1);
    const float frac = scaled - std::floor(scaled);
    return (lut[idx0] * (1.0F - frac)) + (lut[idx1] * frac);
}

inline float poly_blep(float t, float dt) noexcept {
    if (dt <= 0.0F) {
        return 0.0F;
    }

    if (t < dt) {
        const float x = t / dt;
        return (2.0F * x) - (x * x) - 1.0F;
    }
    if (t > 1.0F - dt) {
        const float x = (t - 1.0F) / dt;
        return (x * x) + (2.0F * x) + 1.0F;
    }
    return 0.0F;
}

inline float naive_saw(float phase) noexcept {
    return (2.0F * phase) - 1.0F;
}

inline float naive_square(float phase) noexcept {
    return (phase < 0.5F) ? 1.0F : -1.0F;
}

inline float naive_triangle(float phase) noexcept {
    return (phase < 0.5F) ? ((4.0F * phase) - 1.0F) : (3.0F - (4.0F * phase));
}

} // namespace

// Constructor
Oscillator::Oscillator(Waveform waveform, float frequency, float sample_rate)
    : frequency_(440.0F),
      sample_rate_(48000.0F),
      waveform_(Waveform::SINE) {
    set_frequency(frequency);
    set_sample_rate(sample_rate);
    set_waveform(waveform);
}

} // namespace sonicforge

// Process single sample
float sonicforge::Oscillator::process() noexcept {
    const Waveform wf = waveform_.load(std::memory_order_relaxed);
    const GeneratorFn gen = resolve_generator(wf);
    const float sample = (this->*gen)();
    advance_phase();
    return sample;
}

// Process block
void sonicforge::Oscillator::process_block(float* buffer, std::size_t num_samples) noexcept {
    if (!buffer || num_samples == 0) {
        return;
    }

    // Load parameters once for the entire block
    const Waveform wf = waveform_.load(std::memory_order_relaxed);
    const GeneratorFn gen = resolve_generator(wf);

    for (std::size_t i = 0; i < num_samples; ++i) {
        buffer[i] = (this->*gen)();
        advance_phase();
    }
}

// Parameter Setters (thread-safe)

void sonicforge::Oscillator::set_frequency(float frequency) noexcept {
    // Clamp to valid range: [0.1, Nyquist]
    const float sr = sample_rate_.load(std::memory_order_relaxed);
    const float nyquist = sr * 0.5F;

    if (frequency <= 0.0F) {
        return;
    }
    if (frequency < 0.1F) {
        frequency = 0.1F;
    } else if (frequency > nyquist) {
        frequency = nyquist;
    }

    frequency_.store(frequency, std::memory_order_relaxed);
}

void sonicforge::Oscillator::set_waveform(Waveform waveform) noexcept {
    // Validate waveform - fall back to SINE for invalid values
    switch (waveform) {
        case Waveform::SINE:
        case Waveform::SAW:
        case Waveform::SQUARE:
        case Waveform::TRIANGLE:
            waveform_.store(waveform, std::memory_order_relaxed);
            break;
        default:
            return;
    }
}

void sonicforge::Oscillator::set_sample_rate(float sample_rate) noexcept {
    // Clamp to valid range: [1.0, 192000.0]
    if (sample_rate < 1.0F) {
        sample_rate = 1.0F;
    } else if (sample_rate > 192000.0F) {
        sample_rate = 192000.0F;
    }

    sample_rate_.store(sample_rate, std::memory_order_relaxed);

    // Re-clamp frequency to new Nyquist
    const float freq = frequency_.load(std::memory_order_relaxed);
    const float nyquist = sample_rate * 0.5F;
    if (freq > nyquist) {
        frequency_.store(nyquist, std::memory_order_relaxed);
    }
}

void sonicforge::Oscillator::reset_phase() noexcept {
    phase_ = 0.0F;
}

void sonicforge::Oscillator::set_phase(float phase) noexcept {
    phase_ = wrap_phase(phase);
}

// Parameter Getters
float sonicforge::Oscillator::get_frequency() const noexcept {
    return frequency_.load(std::memory_order_relaxed);
}

sonicforge::Waveform sonicforge::Oscillator::get_waveform() const noexcept {
    return waveform_.load(std::memory_order_relaxed);
}

float sonicforge::Oscillator::get_sample_rate() const noexcept {
    return sample_rate_.load(std::memory_order_relaxed);
}

float sonicforge::Oscillator::get_phase() const noexcept {
    return phase_;
}

// Static Utility - Stateless Waveform Evaluation
float sonicforge::Oscillator::sample_at(Waveform wf, float phase, float dt) noexcept {
    // Wrap phase to [0, 1)
    phase = wrap_phase(phase);

    switch (wf) {
        case Waveform::SINE:
            return lut_interpolate(phase);

        case Waveform::SAW: {
            const float naive = naive_saw(phase);
            return naive - poly_blep(phase, dt);
        }

        case Waveform::SQUARE: {
            const float naive = naive_square(phase);
            // Square wave has two discontinuities per cycle
            return naive + poly_blep(phase, dt) - poly_blep(wrap_phase(phase + 0.5F), dt);
        }

        case Waveform::TRIANGLE:
            return naive_triangle(phase);

        default:
            // Invalid waveform - fall back to SINE
            return lut_interpolate(phase);
    }
}

// Private Generator Methods
float sonicforge::Oscillator::generate_sine() const noexcept {
    return lut_interpolate(phase_);
}

float sonicforge::Oscillator::generate_saw() const noexcept {
    const float naive = naive_saw(phase_);
    const float dt = frequency_.load(std::memory_order_relaxed) / sample_rate_.load(std::memory_order_relaxed);
    return naive - poly_blep(phase_, dt);
}

float sonicforge::Oscillator::generate_square() const noexcept {
    const float naive = naive_square(phase_);
    const float dt = frequency_.load(std::memory_order_relaxed) / sample_rate_.load(std::memory_order_relaxed);
    // Square wave has two discontinuities per cycle
    return naive + poly_blep(phase_, dt) - poly_blep(wrap_phase(phase_ + 0.5F), dt);
}

float sonicforge::Oscillator::generate_triangle() const noexcept {
    return naive_triangle(phase_);
}

void sonicforge::Oscillator::advance_phase() noexcept {
    const float freq = frequency_.load(std::memory_order_relaxed);
    const float sr = sample_rate_.load(std::memory_order_relaxed);
    const float dt = freq / sr;

    phase_ += dt;
    phase_ = wrap_phase(phase_);
}

// Waveform Dispatch
sonicforge::Oscillator::GeneratorFn sonicforge::Oscillator::resolve_generator(Waveform wf) noexcept {
    // Dispatch table for waveform generators
    static constexpr std::array<GeneratorFn, 4> GENERATORS = {
        &Oscillator::generate_sine,
        &Oscillator::generate_saw,
        &Oscillator::generate_square,
        &Oscillator::generate_triangle,
    };

    // Convert enum to index with bounds checking
    const auto idx = static_cast<std::size_t>(wf);
    if (idx < GENERATORS.size()) {
        return GENERATORS[idx];
    }

    // Default to sine for invalid waveforms
    return GENERATORS[0];
}

/**
 * @file oscillator.cpp
 * @brief Implementation of digital oscillator for audio synthesis
 *
 * @author SonicForge DSP
 * @copyright MIT License
 */

#include "sonicforge/oscillator.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

// =============================================================================
// Sine Lookup Table (Fix #11 — LUT-based sine with linear interpolation)
// =============================================================================

/// Number of entries in the compile-time sine table.
/// 4096 entries with linear interpolation yields peak error ~2.3e-6 (≈ −113 dB)
/// relative to std::sin, which is well below the 16-bit noise floor (~−96 dB).
constexpr std::size_t SINE_LUT_SIZE = 4096;

/// Pre-computed sine lookup table, initialised once at program start.
/// Using double precision during construction keeps each float entry as
/// accurate as a 32-bit float allows.
const std::array<float, SINE_LUT_SIZE> SINE_LUT = []() noexcept {
    std::array<float, SINE_LUT_SIZE> tbl{};
    constexpr double two_pi_d = 2.0 * 3.14159265358979323846;
    for (std::size_t i = 0; i < SINE_LUT_SIZE; ++i) {
        tbl[i] = static_cast<float>(
            std::sin(static_cast<double>(i) * two_pi_d / static_cast<double>(SINE_LUT_SIZE)));
    }
    return tbl;
}();

// =============================================================================
// PolyBLEP residual (Fix #1 — anti-aliasing for saw and square)
// =============================================================================

/**
 * @brief 2-point PolyBLEP correction for a unit step discontinuity at t = 0.
 *
 * PolyBLEP (Polynomial Bandlimited Step) subtracts the alias energy introduced
 * by a discontinuity by fitting a two-sample polynomial correction on either
 * side of the wrap point.
 *
 * Returns a correction value in (-1, +1):
 *  - t ∈ [0,   dt)    — just after the discontinuity, correction is negative
 *  - t ∈ (1-dt, 1)    — just before the discontinuity, correction is positive
 *  - elsewhere        — zero (no correction needed)
 *
 * @param t   Current normalised phase in [0, 1)
 * @param dt  Phase increment for this sample = frequency / sample_rate
 * @return    Correction residual to be added to / subtracted from the signal
 */
[[nodiscard]] inline float poly_blep(float t, float dt) noexcept {
    if (t < dt) {
        // Just after the wrap: polynomial ramp from −1 at t=0 to 0 at t=dt
        t /= dt;
        return (t + t) - (t * t) - 1.0F;
    }
    if (t > 1.0F - dt) {
        // Just before the wrap: polynomial ramp from 0 at t=(1−dt) to +1 at t→1
        t = (t - 1.0F) / dt;
        return (t * t) + (t + t) + 1.0F;
    }
    return 0.0F;
}

// =============================================================================
// Waveform enum validation (Fix #2)
// =============================================================================

/**
 * @brief Return true if @p w maps to a defined Waveform enumerator.
 *
 * The backing type is uint8_t; valid values are 0 (SINE) through
 * 3 (TRIANGLE).  Anything outside that range is considered corrupt
 * (e.g. from an uninitialised variable or deserialisation error).
 */
[[nodiscard]] constexpr bool is_valid_waveform(sonicforge::Waveform w) noexcept {
    return static_cast<uint8_t>(w) <= static_cast<uint8_t>(sonicforge::Waveform::TRIANGLE);
}

// =============================================================================
// Stateless waveform evaluators
//
// These free functions contain the actual synthesis logic and are shared by
// both the instance generate_*() methods and the static Oscillator::sample_at()
// utility.  Keeping a single definition guarantees bit-exact equivalence
// between the audio-thread processing path and the batch visualisation path.
// =============================================================================

/**
 * @brief Evaluate a single LUT-interpolated sine sample at normalised phase.
 *
 * Behaviour is identical to Oscillator::generate_sine() for the same phase_.
 */
[[nodiscard]] inline float sine_at(float phase) noexcept {
    constexpr auto lut_scale = static_cast<float>(SINE_LUT_SIZE);
    const float pos = phase * lut_scale;
    const auto idx0 = static_cast<std::size_t>(pos);
    const float frac = pos - static_cast<float>(idx0);
    const std::size_t idx1 = (idx0 + 1U) & (SINE_LUT_SIZE - 1U);
    return SINE_LUT[idx0] + (frac * (SINE_LUT[idx1] - SINE_LUT[idx0]));
}

/**
 * @brief Evaluate a PolyBLEP-corrected sawtooth sample.
 *
 * Behaviour is identical to Oscillator::generate_saw() for the same phase_
 * and (frequency_ / sample_rate_) values.
 */
[[nodiscard]] inline float saw_at(float phase, float dt) noexcept {
    return ((2.0F * phase) - 1.0F) - poly_blep(phase, dt);
}

/**
 * @brief Evaluate a PolyBLEP-corrected square wave sample.
 *
 * Behaviour is identical to Oscillator::generate_square() for the same
 * phase_ and (frequency_ / sample_rate_) values.
 */
[[nodiscard]] inline float square_at(float phase, float dt) noexcept {
    float naive = (phase < 0.5F) ? 1.0F : -1.0F;
    naive += poly_blep(phase, dt);
    const float shifted = phase + 0.5F;
    naive -= poly_blep((shifted >= 1.0F) ? (shifted - 1.0F) : shifted, dt);
    return naive;
}

/**
 * @brief Evaluate a triangle wave sample at normalised phase.
 *
 * Behaviour is identical to Oscillator::generate_triangle() for the same
 * phase_.
 */
[[nodiscard]] inline float triangle_at(float phase) noexcept {
    if (phase < 0.5F) {
        return (4.0F * phase) - 1.0F;
    }
    return 3.0F - (4.0F * phase);
}

}  // anonymous namespace

namespace sonicforge {

// =============================================================================
// Constructor
// =============================================================================

Oscillator::Oscillator(Waveform waveform, float frequency, float sample_rate)
    :  // Clamp invalid inputs to safe defaults at construction time.
       // phase_ uses the in-class default (0.0F); no need to repeat it here.
      frequency_{(frequency > 0.0F) ? frequency : 440.0F},
      sample_rate_{(sample_rate > 0.0F) ? sample_rate : 48000.0F},
      // Silently fall back to SINE for any out-of-range enum value
      waveform_{is_valid_waveform(waveform) ? waveform : Waveform::SINE} {}

// =============================================================================
// Waveform dispatch (Fix #10 — single definition, no switch in hot loop)
// =============================================================================

/**
 * @brief Return the generator function pointer for @p wf.
 *
 * The table is a static local inside a static member function, so it is
 * initialised exactly once (C++11 guarantee) and private member addresses
 * are accessible because this is a member of Oscillator.
 *
 * Safety: is_valid_waveform() is enforced in the constructor and
 * set_waveform(), so the uint8_t cast is always in [0, 3].
 */
Oscillator::GeneratorFn Oscillator::resolve_generator(Waveform wf) noexcept {
    // Use std::array to avoid the cppcoreguidelines-avoid-c-arrays warning.
    static const std::array<GeneratorFn, 4> TABLE = {
        &Oscillator::generate_sine,      // 0 — SINE
        &Oscillator::generate_saw,       // 1 — SAW
        &Oscillator::generate_square,    // 2 — SQUARE
        &Oscillator::generate_triangle,  // 3 — TRIANGLE
    };
    return TABLE[static_cast<uint8_t>(wf)];
}

// =============================================================================
// Main Processing Functions
// =============================================================================

float Oscillator::process() noexcept {
    const GeneratorFn gen = resolve_generator(waveform_.load(std::memory_order_relaxed));
    const float output = (this->*gen)();
    advance_phase();
    return output;
}

void Oscillator::process_block(float* buffer, std::size_t num_samples) noexcept {
    if (buffer == nullptr || num_samples == 0) {
        return;
    }

    // Resolve the waveform once for the whole block — avoids a per-sample
    // atomic load and repeated indirect-branch prediction inside the loop.
    const GeneratorFn gen = resolve_generator(waveform_.load(std::memory_order_relaxed));

    for (std::size_t i = 0U; i < num_samples; ++i) {
        buffer[i] = (this->*gen)();
        advance_phase();
    }
}

// =============================================================================
// Waveform Generation Functions
// =============================================================================

float Oscillator::generate_sine() const noexcept {
    return sine_at(phase_);
}

float Oscillator::generate_saw() const noexcept {
    // PolyBLEP-corrected sawtooth.  Delegates to saw_at() so the static
    // sample_at() path is bit-exact with the instance processing path.
    const float dt =
        frequency_.load(std::memory_order_relaxed) / sample_rate_.load(std::memory_order_relaxed);
    return saw_at(phase_, dt);
}

float Oscillator::generate_square() const noexcept {
    // PolyBLEP-corrected square wave.  Delegates to square_at() so the static
    // sample_at() path is bit-exact with the instance processing path.
    const float dt =
        frequency_.load(std::memory_order_relaxed) / sample_rate_.load(std::memory_order_relaxed);
    return square_at(phase_, dt);
}

float Oscillator::generate_triangle() const noexcept {
    // Triangle wave.  Delegates to triangle_at() for bit-exact equivalence
    // with the static sample_at() path.
    return triangle_at(phase_);
}

// =============================================================================
// Phase Management
// =============================================================================

void Oscillator::advance_phase() noexcept {
    // Phase increment = frequency / sample_rate
    //
    // At 440 Hz / 48 kHz: increment = 440/48000 ≈ 0.00917
    // → after 48000 samples (1 s) the phase has cycled exactly 440 times.
    const float freq = frequency_.load(std::memory_order_relaxed);
    const float sr = sample_rate_.load(std::memory_order_relaxed);

    phase_ += freq / sr;

    // Wrap to [0, 1).  Subtraction is faster than std::fmod and is correct
    // because set_frequency() clamps freq to ≤ Nyquist, so the increment
    // is always < 0.5 and at most one subtraction is ever needed.
    if (phase_ >= 1.0F) {
        phase_ -= 1.0F;
    }
}

// =============================================================================
// Parameter Setters (Thread-Safe)
// =============================================================================

void Oscillator::set_frequency(float frequency) noexcept {
    // Clamp to (0, Nyquist] to prevent DC, negative frequencies, and aliasing
    // above the Nyquist limit.  The Nyquist is derived from the current
    // sample rate; any remaining tiny race window between set_sample_rate()
    // and set_frequency() at most allows a brief overshoot that is corrected
    // on the next set_frequency() call.
    const float sr = sample_rate_.load(std::memory_order_relaxed);
    const float nyquist = sr * 0.5F;
    frequency_.store(std::clamp(frequency, 0.1F, nyquist), std::memory_order_relaxed);
}

void Oscillator::set_waveform(Waveform waveform) noexcept {
    // Silently ignore invalid enum values (Fix #2)
    if (is_valid_waveform(waveform)) {
        waveform_.store(waveform, std::memory_order_relaxed);
    }
}

void Oscillator::set_sample_rate(float sample_rate) noexcept {
    if (sample_rate > 0.0F) {
        sample_rate_.store(sample_rate, std::memory_order_relaxed);
    }
}

void Oscillator::reset_phase() noexcept {
    phase_ = 0.0F;
}

void Oscillator::set_phase(float phase) noexcept {
    // Subtract the integer part to wrap any value into [0, 1)
    phase_ = phase - std::floor(phase);
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

// =============================================================================
// Static Utility — Stateless Waveform Evaluation
// =============================================================================

float Oscillator::sample_at(Waveform wf, float phase, float dt) noexcept {
    // Wrap phase into [0, 1) using the same method as set_phase() so that
    // callers can pass raw accumulated values without pre-normalising.
    phase = phase - std::floor(phase);

    switch (wf) {
        case Waveform::SINE:
            return sine_at(phase);
        case Waveform::SAW:
            return saw_at(phase, dt);
        case Waveform::SQUARE:
            return square_at(phase, dt);
        case Waveform::TRIANGLE:
            return triangle_at(phase);
        default:
            // Out-of-range enum values fall back to SINE, matching the
            // constructor and set_waveform() validation behaviour.
            return sine_at(phase);
    }
}

}  // namespace sonicforge

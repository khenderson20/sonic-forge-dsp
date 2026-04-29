/**
 * @file state_variable_filter.cpp
 * @brief State Variable Filter — Cytomic / Zavalishin ZDF implementation
 */

#include "sonicforge/state_variable_filter.hpp"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace sonicforge {

static constexpr float PI = 3.14159265358979323846F;

StateVariableFilter::StateVariableFilter(FilterMode mode, float cutoff_hz, float resonance,
                                         float sample_rate) {
    // Invalid enum values silently fall back to Lowpass.
    // Note: FilterMode is backed by uint8_t, so only check upper bound.
    if (static_cast<std::underlying_type_t<FilterMode>>(mode) > 3)
        mode = FilterMode::Lowpass;

    mode_.store(mode, std::memory_order_relaxed);
    sample_rate_.store(sample_rate > 0.0F ? sample_rate : 48000.0F, std::memory_order_relaxed);
    set_cutoff_hz(cutoff_hz);
    set_resonance(resonance);
    recalc_coefficients();
}

// ---------------------------------------------------------------------------

float StateVariableFilter::process(float in) noexcept {
    // Read atomics once — if anything changed, recalc coefficients.
    const float cur_fc = cutoff_hz_.load(std::memory_order_relaxed);
    const float cur_q = resonance_.load(std::memory_order_relaxed);
    const auto cur_md = mode_.load(std::memory_order_relaxed);
    const float cur_sr = sample_rate_.load(std::memory_order_relaxed);

    if (cur_fc != cached_cutoff_hz_ || cur_q != cached_resonance_ || cur_md != cached_mode_ ||
        cur_sr != cached_sample_rate_) {
        recalc_coefficients();
    }

    // ZDF SVF one-pole step — Cytomic / Zavalishin formulation.
    // Reference: Andy Simper, "Solving the continuous SVF equations using
    //            trapezoidal integration and equivalent currents" (Cytomic, 2021)
    //
    //   v1 = H * (ic1eq + g * (in - ic2eq))   — normalised bandpass midpoint
    //   v2 = ic2eq + g * v1                    — lowpass output
    //   new_ic1eq = 2*v1 - ic1eq               — trapezoidal state update
    //   new_ic2eq = 2*v2 - ic2eq               — trapezoidal state update
    //
    //   lp = v2           hp = in - R*v1 - v2
    //   bp = v1           notch = in - R*v1

    const float v3 = in - ic2eq_;
    const float v1 = H_ * (ic1eq_ + g_ * v3);  // H_ = 1/(1 + R*g + g^2)
    const float v2 = ic2eq_ + g_ * v1;

    ic1eq_ = 2.0F * v1 - ic1eq_;
    ic2eq_ = 2.0F * v2 - ic2eq_;

    switch (cached_mode_) {
        case FilterMode::Lowpass:
            return v2;
        case FilterMode::Highpass:
            return in - R_ * v1 - v2;
        case FilterMode::Bandpass:
            return v1;
        case FilterMode::Notch:
            return in - R_ * v1;
    }
    return v2;  // unreachable
}

void StateVariableFilter::process_block(float* buffer, std::size_t n) noexcept {
    if (!buffer || n == 0)
        return;
    for (std::size_t i = 0; i < n; ++i)
        buffer[i] = process(buffer[i]);
}

// ---------------------------------------------------------------------------

void StateVariableFilter::set_cutoff_hz(float hz) noexcept {
    const float nyq = sample_rate_.load(std::memory_order_relaxed) * 0.5F;
    const float clamped = std::max(20.0F, std::min(hz, nyq * 0.99F));
    cutoff_hz_.store(clamped, std::memory_order_relaxed);
}

void StateVariableFilter::set_resonance(float q) noexcept {
    resonance_.store(std::max(0.0F, std::min(q, 1.0F)), std::memory_order_relaxed);
}

void StateVariableFilter::set_mode(FilterMode mode) noexcept {
    if (static_cast<std::underlying_type_t<FilterMode>>(mode) > 3)
        return;
    mode_.store(mode, std::memory_order_relaxed);
}

void StateVariableFilter::set_sample_rate(float sr) noexcept {
    if (sr <= 0.0F)
        return;
    sample_rate_.store(sr, std::memory_order_relaxed);
}

void StateVariableFilter::reset() noexcept {
    ic1eq_ = 0.0F;
    ic2eq_ = 0.0F;
}

// ---------------------------------------------------------------------------

float StateVariableFilter::get_cutoff_hz() const noexcept {
    return cutoff_hz_.load(std::memory_order_relaxed);
}
float StateVariableFilter::get_resonance() const noexcept {
    return resonance_.load(std::memory_order_relaxed);
}
FilterMode StateVariableFilter::get_mode() const noexcept {
    return mode_.load(std::memory_order_relaxed);
}
float StateVariableFilter::get_sample_rate() const noexcept {
    return sample_rate_.load(std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------

void StateVariableFilter::recalc_coefficients() noexcept {
    cached_cutoff_hz_ = cutoff_hz_.load(std::memory_order_relaxed);
    cached_resonance_ = resonance_.load(std::memory_order_relaxed);
    cached_mode_ = mode_.load(std::memory_order_relaxed);
    cached_sample_rate_ = sample_rate_.load(std::memory_order_relaxed);

    const float f = cached_cutoff_hz_ / cached_sample_rate_;
    g_ = std::tan(PI * f);

    // Resonance damping: Q = 1/(2*res) mapped so 0 → no resonance, 1 → edge
    const float res = cached_resonance_;
    R_ = 2.0F * (1.0F - std::min(res, 0.99F));  // 2 → 0.02

    H_ = 1.0F / (1.0F + R_ * g_ + g_ * g_);
}

}  // namespace sonicforge

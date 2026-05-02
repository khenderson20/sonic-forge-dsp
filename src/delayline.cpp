/**
 * @file delayline.cpp
 * @brief DelayLine template specialisations — circular buffer with interpolation
 */

#include "sonicforge/delayline.hpp"

#include <array>
#include <cmath>

namespace {

inline int wrap_index(int idx, int size) noexcept {
    idx %= size;
    return (idx < 0) ? idx + size : idx;
}

} // namespace

namespace sonicforge {

DelayLine<DelayInterpolation::NONE>::DelayLine(std::size_t max_samples) : buffer_(max_samples, 0.0F) {}

void DelayLine<DelayInterpolation::NONE>::set_max_delay(std::size_t max_samples) {
    if (max_samples == 0) {
        max_samples = 1;
    }

    // Clamp current delay to new max to prevent buffer overflow
    if (delay_samples_ >= static_cast<float>(max_samples)) {
        delay_samples_ = static_cast<float>(max_samples - 1);
        int_delay_ = static_cast<int>(max_samples - 1);
    }

    buffer_.assign(max_samples, 0.0F);
    write_pos_ = 0;
}

void DelayLine<DelayInterpolation::NONE>::set_delay(float samples) noexcept {
    delay_samples_ = std::max(0.0F, samples);
    int_delay_ = static_cast<int>(std::round(delay_samples_));
}

void DelayLine<DelayInterpolation::NONE>::set_feedback(float amount) noexcept {
    feedback_ = std::max(0.0F, std::min(amount, 0.9999F));
}

float DelayLine<DelayInterpolation::NONE>::read() const noexcept {
    if (buffer_.empty()) {
        return 0.0F;
    }
    const int sz = static_cast<int>(buffer_.size());
    const int read_pos = wrap_index(static_cast<int>(write_pos_) - int_delay_, sz);
    return buffer_[static_cast<std::size_t>(read_pos)];
}

float DelayLine<DelayInterpolation::NONE>::process(float in) noexcept {
    if (buffer_.empty()) {
        return 0.0F;
    }
    const float out = read();
    buffer_[write_pos_] = in;
    write_pos_ = (write_pos_ + 1) % buffer_.size();
    return out;
}

void DelayLine<DelayInterpolation::NONE>::process_block(float* buffer, std::size_t n) noexcept {
    if (!buffer || n == 0) {
        return;
    }
    for (std::size_t i = 0; i < n; ++i) {
        buffer[i] = process(buffer[i]);
    }
}

void DelayLine<DelayInterpolation::NONE>::reset() noexcept {
    std::fill(buffer_.begin(), buffer_.end(), 0.0F);
    write_pos_ = 0;
}

// ===================================================================
// Linear
// ===================================================================

DelayLine<DelayInterpolation::LINEAR>::DelayLine(std::size_t max_samples) : buffer_(max_samples, 0.0F) {}

void DelayLine<DelayInterpolation::LINEAR>::set_max_delay(std::size_t max_samples) {
    if (max_samples == 0) {
        max_samples = 1;
    }

    // Clamp current delay to new max to prevent buffer overflow
    if (delay_samples_ >= static_cast<float>(max_samples)) {
        delay_samples_ = static_cast<float>(max_samples - 1);
    }

    buffer_.assign(max_samples, 0.0F);
    write_pos_ = 0;
}

void DelayLine<DelayInterpolation::LINEAR>::set_delay(float samples) noexcept {
    delay_samples_ = std::max(0.0F, samples);
}

void DelayLine<DelayInterpolation::LINEAR>::set_feedback(float amount) noexcept {
    feedback_ = std::max(0.0F, std::min(amount, 0.9999F));
}

float DelayLine<DelayInterpolation::LINEAR>::read_internal(float delay_samples) const noexcept {
    if (buffer_.empty()) {
        return 0.0F;
    }
    const int sz = static_cast<int>(buffer_.size());

    const float frac = delay_samples - std::floor(delay_samples);
    // write_pos points to the NEXT write slot; most recent sample is at write_pos-1.
    // A delay of d samples means: index = write_pos - floor(d)
    const int idx0 = wrap_index(static_cast<int>(write_pos_) - static_cast<int>(std::floor(delay_samples)), sz);
    const int idx1 = wrap_index(idx0 - 1, sz);

    const float s0 = buffer_[static_cast<std::size_t>(idx0)];
    const float s1 = buffer_[static_cast<std::size_t>(idx1)];

    return s0 + (frac * (s1 - s0));
}

float DelayLine<DelayInterpolation::LINEAR>::read(float delay_samples) const noexcept {
    return read_internal(delay_samples);
}

float DelayLine<DelayInterpolation::LINEAR>::process(float in) noexcept {
    if (buffer_.empty()) {
        return 0.0F;
    }
    const float out = read_internal(delay_samples_);
    buffer_[write_pos_] = in;
    write_pos_ = (write_pos_ + 1) % buffer_.size();
    return out;
}

void DelayLine<DelayInterpolation::LINEAR>::process_block(float* buffer, std::size_t n) noexcept {
    if (!buffer || n == 0) {
        return;
    }
    for (std::size_t i = 0; i < n; ++i) {
        buffer[i] = process(buffer[i]);
    }
}

void DelayLine<DelayInterpolation::LINEAR>::reset() noexcept {
    std::fill(buffer_.begin(), buffer_.end(), 0.0F);
    write_pos_ = 0;
}

// ===================================================================
// Lagrange3rd
// ===================================================================

DelayLine<DelayInterpolation::LAGRANGE3RD>::DelayLine(std::size_t max_samples) : buffer_(max_samples, 0.0F) {}

void DelayLine<DelayInterpolation::LAGRANGE3RD>::set_max_delay(std::size_t max_samples) {
    if (max_samples == 0) {
        max_samples = 1;
    }

    // Clamp current delay to new max to prevent buffer overflow
    if (delay_samples_ >= static_cast<float>(max_samples)) {
        delay_samples_ = static_cast<float>(max_samples - 1);
    }

    buffer_.assign(max_samples, 0.0F);
    write_pos_ = 0;
}

void DelayLine<DelayInterpolation::LAGRANGE3RD>::set_delay(float samples) noexcept {
    delay_samples_ = std::max(0.0F, samples);
}

void DelayLine<DelayInterpolation::LAGRANGE3RD>::set_feedback(float amount) noexcept {
    feedback_ = std::max(0.0F, std::min(amount, 0.9999F));
}

float DelayLine<DelayInterpolation::LAGRANGE3RD>::read_internal(float delay_samples) const noexcept {
    if (buffer_.empty()) {
        return 0.0F;
    }
    const int sz = static_cast<int>(buffer_.size());

    const float frac = delay_samples - std::floor(delay_samples);
    // For a 4-tap Lagrange polynomial where c1=1 at frac=0, the integer-delay
    // tap is at (base-1).  Shift base by +1 so that delay_samples=d returns
    // buffer[write_pos - d] for integer d (consistent with the Linear mode).
    const int base = wrap_index(static_cast<int>(write_pos_) - static_cast<int>(std::floor(delay_samples)) + 1, sz);

    // 4-tap Lagrange coefficients
    const float f = frac;
    const float f2 = f * f;
    const float f3 = f2 * f;

    const float c0 = (-f3 + 2.0F * f2 - f) * 0.5F;
    const float c1 = (3.0F * f3 - 5.0F * f2 + 2.0F) * 0.5F;
    const float c2 = (-3.0F * f3 + 4.0F * f2 + f) * 0.5F;
    const float c3 = (f3 - f2) * 0.5F;

    const std::array<float, 4> coeffs = {c0, c1, c2, c3};
    float out = 0.0F;
    for (int tap = 0; tap < 4; ++tap) {
        const int idx = wrap_index(base - tap, sz);
        out += buffer_[static_cast<std::size_t>(idx)] * coeffs[static_cast<std::size_t>(tap)];
    }
    return out;
}

float DelayLine<DelayInterpolation::LAGRANGE3RD>::read(float delay_samples) const noexcept {
    return read_internal(delay_samples);
}

float DelayLine<DelayInterpolation::LAGRANGE3RD>::process(float in) noexcept {
    if (buffer_.empty()) {
        return 0.0F;
    }
    const float out = read_internal(delay_samples_);
    buffer_[write_pos_] = in;
    write_pos_ = (write_pos_ + 1) % buffer_.size();
    return out;
}

void DelayLine<DelayInterpolation::LAGRANGE3RD>::process_block(float* buffer, std::size_t n) noexcept {
    if (!buffer || n == 0) {
        return;
    }
    for (std::size_t i = 0; i < n; ++i) {
        buffer[i] = process(buffer[i]);
    }
}

void DelayLine<DelayInterpolation::LAGRANGE3RD>::reset() noexcept {
    std::fill(buffer_.begin(), buffer_.end(), 0.0F);
    write_pos_ = 0;
}

} // namespace sonicforge

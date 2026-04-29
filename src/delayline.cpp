/**
 * @file delayline.cpp
 * @brief DelayLine template specialisations — circular buffer with interpolation
 */

#include "sonicforge/delayline.hpp"

namespace sonicforge {

static inline int wrap_index(int idx, int size) noexcept {
    idx %= size;
    return (idx < 0) ? idx + size : idx;
}

// ===================================================================
// None
// ===================================================================

DelayLine<DelayInterpolation::None>::DelayLine(std::size_t max_samples)
    : buffer_(max_samples, 0.0F) {}

void DelayLine<DelayInterpolation::None>::set_max_delay(std::size_t max_samples) {
    buffer_.assign(max_samples, 0.0F);
    write_pos_ = 0;
}

void DelayLine<DelayInterpolation::None>::set_delay(float samples) noexcept {
    delay_samples_ = std::max(0.0F, samples);
    int_delay_ = static_cast<int>(std::round(delay_samples_));
}

void DelayLine<DelayInterpolation::None>::set_feedback(float amount) noexcept {
    feedback_ = std::max(0.0F, std::min(amount, 0.9999F));
}

float DelayLine<DelayInterpolation::None>::read() const noexcept {
    if (buffer_.empty())
        return 0.0F;
    const int sz = static_cast<int>(buffer_.size());
    const int read_pos = wrap_index(static_cast<int>(write_pos_) - int_delay_, sz);
    return buffer_[static_cast<std::size_t>(read_pos)];
}

float DelayLine<DelayInterpolation::None>::process(float in) noexcept {
    if (buffer_.empty())
        return 0.0F;
    const float out = read();
    buffer_[write_pos_] = in;
    write_pos_ = (write_pos_ + 1) % buffer_.size();
    return out;
}

void DelayLine<DelayInterpolation::None>::process_block(float* buffer, std::size_t n) noexcept {
    if (!buffer || n == 0)
        return;
    for (std::size_t i = 0; i < n; ++i)
        buffer[i] = process(buffer[i]);
}

void DelayLine<DelayInterpolation::None>::reset() noexcept {
    std::fill(buffer_.begin(), buffer_.end(), 0.0F);
    write_pos_ = 0;
}

// ===================================================================
// Linear
// ===================================================================

DelayLine<DelayInterpolation::Linear>::DelayLine(std::size_t max_samples)
    : buffer_(max_samples, 0.0F) {}

void DelayLine<DelayInterpolation::Linear>::set_max_delay(std::size_t max_samples) {
    buffer_.assign(max_samples, 0.0F);
    write_pos_ = 0;
}

void DelayLine<DelayInterpolation::Linear>::set_delay(float samples) noexcept {
    delay_samples_ = std::max(0.0F, samples);
}

void DelayLine<DelayInterpolation::Linear>::set_feedback(float amount) noexcept {
    feedback_ = std::max(0.0F, std::min(amount, 0.9999F));
}

float DelayLine<DelayInterpolation::Linear>::read_internal(float delay_samples) const noexcept {
    if (buffer_.empty())
        return 0.0F;
    const int sz = static_cast<int>(buffer_.size());

    const float frac = delay_samples - std::floor(delay_samples);
    // write_pos points to the NEXT write slot; most recent sample is at write_pos-1.
    // A delay of d samples means: index = write_pos - floor(d)
    const int idx0 =
        wrap_index(static_cast<int>(write_pos_) - static_cast<int>(std::floor(delay_samples)), sz);
    const int idx1 = wrap_index(idx0 - 1, sz);

    const float s0 = buffer_[static_cast<std::size_t>(idx0)];
    const float s1 = buffer_[static_cast<std::size_t>(idx1)];

    return s0 + frac * (s1 - s0);
}

float DelayLine<DelayInterpolation::Linear>::read(float delay_samples) const noexcept {
    return read_internal(delay_samples);
}

float DelayLine<DelayInterpolation::Linear>::process(float in) noexcept {
    if (buffer_.empty())
        return 0.0F;
    const float out = read_internal(delay_samples_);
    buffer_[write_pos_] = in;
    write_pos_ = (write_pos_ + 1) % buffer_.size();
    return out;
}

void DelayLine<DelayInterpolation::Linear>::process_block(float* buffer, std::size_t n) noexcept {
    if (!buffer || n == 0)
        return;
    for (std::size_t i = 0; i < n; ++i)
        buffer[i] = process(buffer[i]);
}

void DelayLine<DelayInterpolation::Linear>::reset() noexcept {
    std::fill(buffer_.begin(), buffer_.end(), 0.0F);
    write_pos_ = 0;
}

// ===================================================================
// Lagrange3rd
// ===================================================================

DelayLine<DelayInterpolation::Lagrange3rd>::DelayLine(std::size_t max_samples)
    : buffer_(max_samples, 0.0F) {}

void DelayLine<DelayInterpolation::Lagrange3rd>::set_max_delay(std::size_t max_samples) {
    buffer_.assign(max_samples, 0.0F);
    write_pos_ = 0;
}

void DelayLine<DelayInterpolation::Lagrange3rd>::set_delay(float samples) noexcept {
    delay_samples_ = std::max(0.0F, samples);
}

void DelayLine<DelayInterpolation::Lagrange3rd>::set_feedback(float amount) noexcept {
    feedback_ = std::max(0.0F, std::min(amount, 0.9999F));
}

float DelayLine<DelayInterpolation::Lagrange3rd>::read_internal(
    float delay_samples) const noexcept {
    if (buffer_.empty())
        return 0.0F;
    const int sz = static_cast<int>(buffer_.size());

    const float frac = delay_samples - std::floor(delay_samples);
    // For a 4-tap Lagrange polynomial where c1=1 at frac=0, the integer-delay
    // tap is at (base-1).  Shift base by +1 so that delay_samples=d returns
    // buffer[write_pos - d] for integer d (consistent with the Linear mode).
    const int base = wrap_index(
        static_cast<int>(write_pos_) - static_cast<int>(std::floor(delay_samples)) + 1, sz);

    // 4-tap Lagrange coefficients
    const float f = frac;
    const float f2 = f * f;
    const float f3 = f2 * f;

    const float c0 = (-f3 + 2.0F * f2 - f) * 0.5F;
    const float c1 = (3.0F * f3 - 5.0F * f2 + 2.0F) * 0.5F;
    const float c2 = (-3.0F * f3 + 4.0F * f2 + f) * 0.5F;
    const float c3 = (f3 - f2) * 0.5F;

    float out = 0.0F;
    for (int tap = 0; tap < 4; ++tap) {
        const int idx = wrap_index(base - tap, sz);
        const float coeffs[4] = {c0, c1, c2, c3};
        out += buffer_[static_cast<std::size_t>(idx)] * coeffs[tap];
    }
    return out;
}

float DelayLine<DelayInterpolation::Lagrange3rd>::read(float delay_samples) const noexcept {
    return read_internal(delay_samples);
}

float DelayLine<DelayInterpolation::Lagrange3rd>::process(float in) noexcept {
    if (buffer_.empty())
        return 0.0F;
    const float out = read_internal(delay_samples_);
    buffer_[write_pos_] = in;
    write_pos_ = (write_pos_ + 1) % buffer_.size();
    return out;
}

void DelayLine<DelayInterpolation::Lagrange3rd>::process_block(float* buffer,
                                                               std::size_t n) noexcept {
    if (!buffer || n == 0)
        return;
    for (std::size_t i = 0; i < n; ++i)
        buffer[i] = process(buffer[i]);
}

void DelayLine<DelayInterpolation::Lagrange3rd>::reset() noexcept {
    std::fill(buffer_.begin(), buffer_.end(), 0.0F);
    write_pos_ = 0;
}

}  // namespace sonicforge

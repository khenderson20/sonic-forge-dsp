/**
 * @file smoothed_value.hpp
 * @brief Sub-sample parameter smoothing to avoid audio artifacts
 *
 * When a parameter like frequency or gain changes instantly, it introduces
 * discontinuities that manifest as clicks or zipper noise. `SmoothedValue`
 * interpolates between the old and new value over a configurable ramp
 * duration, producing artifact-free modulation.
 *
 * ## Smoothing Modes
 *
 * | Mode              | Best for                        | Behaviour                          |
 * |-------------------|---------------------------------|------------------------------------|
 * | `Linear`          | Gain, pan, mix, cutoff          | Constant delta per sample          |
 * | `Multiplicative`  | Frequency, Q, any log-domain    | Constant ratio per sample          |
 *
 * ## Design Philosophy
 *
 * - **Header-only** — no compilation unit required; include and use.
 * - **Zero heap allocation** — all state is stored in plain `float` members.
 * - **No thread safety** — the caller must set the target from the audio
 *   thread (or synchronise externally). This keeps the `process()` path
 *   branchless and cache-friendly.
 *
 * ## Usage Example
 *
 * ```cpp
 * #include <sonicforge/smoothed_value.hpp>
 *
 * sonicforge::SmoothedValue<sonicforge::SmoothingMode::Multiplicative> freq;
 * freq.reset(440.0F, 48000.0F);       // start at 440 Hz, 48 kHz sample rate
 * freq.set_ramp_duration(0.02F);      // 20 ms ramp
 *
 * freq.set_target(880.0F);            // begin ramping to 880 Hz
 *
 * for (int i = 0; i < 256; ++i) {
 *     float smoothed = freq.process(); // advances the ramp by one sample
 *     oscillator.set_frequency(smoothed);
 * }
 * ```
 *
 * @author SonicForge DSP
 * @copyright MIT License
 */

#ifndef SONICFORGE_SMOOTHED_VALUE_HPP
#define SONICFORGE_SMOOTHED_VALUE_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sonicforge {

/**
 * @brief Smoothing modes available for parameter interpolation
 */
enum class SmoothingMode : uint8_t {
    Linear,        /**< Constant delta per sample. Use for gain, pan, etc. */
    Multiplicative /**< Constant ratio per sample. Use for frequency, Q, etc. */
};

// ===========================================================================
// Forward declaration — the concrete template is defined below.
// ===========================================================================

template <SmoothingMode Mode> class SmoothedValue;

// ===========================================================================
// Linear specialization
// ===========================================================================

template <> class SmoothedValue<SmoothingMode::Linear> {
public:
    SmoothedValue() noexcept = default;

    /**
     * @brief Construct with an initial value
     * @param initial_value  Both the starting value and the initial target
     */
    explicit SmoothedValue(float initial_value) noexcept : current_(initial_value), target_(initial_value) {}

    /**
     * @brief Reset the smoother to a known value and sample rate
     * @param initial_value  Starting value (also set as target)
     * @param sample_rate    Audio sample rate in Hz
     */
    void reset(float initial_value, float sample_rate) noexcept {
        current_ = initial_value;
        target_ = initial_value;
        sample_rate_ = sample_rate;
        is_smoothing_ = false;
        step_ = 0.0F;
    }

    /**
     * @brief Set the ramp duration in seconds
     * @param seconds  Ramp time from current value to a new target
     */
    void set_ramp_duration(float seconds) noexcept {
        ramp_seconds_ = std::max(seconds, 0.0F);
        num_steps_ = static_cast<int>(std::ceil(ramp_seconds_ * sample_rate_));
        if (num_steps_ < 1)
            num_steps_ = 1;
    }

    /**
     * @brief Set the next value to ramp toward
     * @param new_target  The destination value
     */
    void set_target(float new_target) noexcept {
        if (new_target == target_ && !is_smoothing_)
            return;
        target_ = new_target;
        step_ = (target_ - current_) / static_cast<float>(num_steps_);
        steps_left_ = num_steps_;
        is_smoothing_ = true;
    }

    /**
     * @brief Advance the ramp by one sample and return the smoothed value
     */
    [[nodiscard]] float process() noexcept {
        if (!is_smoothing_)
            return target_;
        current_ += step_;
        --steps_left_;
        if (steps_left_ <= 0) {
            current_ = target_;
            is_smoothing_ = false;
        }
        return current_;
    }

    /**
     * @brief Fill a buffer with the next `num_samples` smoothed values
     */
    void process_block(float* buffer, int num_samples) noexcept {
        for (int i = 0; i < num_samples; ++i) {
            buffer[i] = process();
        }
    }

    /**
     * @brief Snap immediately to the target value (stop smoothing)
     */
    void snap_to_target() noexcept {
        current_ = target_;
        is_smoothing_ = false;
        steps_left_ = 0;
    }

    [[nodiscard]] float get_current() const noexcept { return current_; }
    [[nodiscard]] float get_target() const noexcept { return target_; }
    [[nodiscard]] float get_ramp_duration() const noexcept { return ramp_seconds_; }
    [[nodiscard]] bool is_smoothing() const noexcept { return is_smoothing_; }

private:
    float current_ = 0.0F;
    float target_ = 0.0F;
    float sample_rate_ = 48000.0F;
    float ramp_seconds_ = 0.01F;
    float step_ = 0.0F;
    int num_steps_ = 480;
    int steps_left_ = 0;
    bool is_smoothing_ = false;
};

// ===========================================================================
// Multiplicative specialization
// ===========================================================================

template <> class SmoothedValue<SmoothingMode::Multiplicative> {
public:
    SmoothedValue() noexcept = default;

    explicit SmoothedValue(float initial_value) noexcept : current_(initial_value), target_(initial_value) {}

    void reset(float initial_value, float sample_rate) noexcept {
        current_ = initial_value;
        target_ = initial_value;
        sample_rate_ = sample_rate;
        is_smoothing_ = false;
        multiply_ = 1.0F;
    }

    void set_ramp_duration(float seconds) noexcept {
        ramp_seconds_ = std::max(seconds, 0.0F);
        num_steps_ = static_cast<int>(std::ceil(ramp_seconds_ * sample_rate_));
        if (num_steps_ < 1)
            num_steps_ = 1;
    }

    /**
     * @brief Set the next value to ramp toward
     *
     * Multiplicative smoothing cannot reach a target of exactly zero.
     * If the target is zero the ramp will approach it asymptotically
     * and snap after `num_steps_` samples.
     */
    void set_target(float new_target) noexcept {
        if (new_target == target_ && !is_smoothing_)
            return;

        target_ = new_target;

        const float safe_current = (current_ == 0.0F) ? 1e-12F : current_;
        const float ratio = target_ / safe_current;

        // Avoid NaN when ratio is negative or zero
        if (ratio > 0.0F) {
            multiply_ = std::pow(ratio, 1.0F / static_cast<float>(num_steps_));
        } else {
            multiply_ = 0.0F;
        }

        steps_left_ = num_steps_;
        is_smoothing_ = true;
    }

    [[nodiscard]] float process() noexcept {
        if (!is_smoothing_)
            return target_;
        current_ *= multiply_;
        --steps_left_;
        if (steps_left_ <= 0) {
            current_ = target_;
            is_smoothing_ = false;
        }
        return current_;
    }

    void process_block(float* buffer, int num_samples) noexcept {
        for (int i = 0; i < num_samples; ++i) {
            buffer[i] = process();
        }
    }

    void snap_to_target() noexcept {
        current_ = target_;
        is_smoothing_ = false;
        steps_left_ = 0;
    }

    [[nodiscard]] float get_current() const noexcept { return current_; }
    [[nodiscard]] float get_target() const noexcept { return target_; }
    [[nodiscard]] float get_ramp_duration() const noexcept { return ramp_seconds_; }
    [[nodiscard]] bool is_smoothing() const noexcept { return is_smoothing_; }

private:
    float current_ = 0.0F;
    float target_ = 0.0F;
    float sample_rate_ = 48000.0F;
    float ramp_seconds_ = 0.01F;
    float multiply_ = 1.0F;
    int num_steps_ = 480;
    int steps_left_ = 0;
    bool is_smoothing_ = false;
};

} // namespace sonicforge

#endif // SONICFORGE_SMOOTHED_VALUE_HPP

/**
 * @file waveshaper.hpp
 * @brief Static waveshaping and wavefolding utilities
 *
 * Provides a collection of transfer functions for distortion, soft clipping,
 * and West-Coast-style wavefolding. All functions are stateless and can be
 * used inline in hot audio loops or passed to `WaveshaperProcessor` for
 * block-level processing.
 *
 * ## Available Transfer Functions
 *
 * | Function            | Character                    | Output range   |
 * |---------------------|------------------------------|----------------|
 * | `soft_clip_tanh`    | Smooth saturation (tanh)     | [-1, 1]        |
 * | `soft_clip_poly`    | Polynomial cubic clipper     | [-1, 1]        |
 * | `hard_clip`         | Instant limiting at +/-1     | [-1, 1]        |
 * | `wavefold_buchla`   | Buchla 259-style fold        | unbounded      |
 * | `full_wave_rectify` | Absolute value with sign     | [0, 1]         |
 *
 * ## Design Philosophy
 *
 * - **Header-only, stateless functions** — no object overhead; the compiler
 *   can inline every call.
 * - **Zero allocation** — safe for real-time audio.
 * - **WaveshaperProcessor class** — optional convenience wrapper that applies
 *   any transfer function to a block with an adjustable drive (pre-gain).
 *
 * ## Usage Example
 *
 * ```cpp
 * #include <sonicforge/waveshaper.hpp>
 *
 * // Free function — inline in a loop:
 * for (int i = 0; i < n; ++i) {
 *     buf[i] = sonicforge::soft_clip_tanh(buf[i] * drive);
 * }
 *
 * // Or use the processor wrapper:
 * sonicforge::WaveshaperProcessor ws{sonicforge::WaveshaperShape::Tanh};
 * ws.set_drive(3.0F);
 * ws.process_block(buf, n);
 * ```
 *
 * @author SonicForge DSP
 * @copyright MIT License
 */

#ifndef SONICFORGE_WAVESHAPER_HPP
#define SONICFORGE_WAVESHAPER_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace sonicforge {

// ===================================================================
// Stateless transfer functions
// ===================================================================

/**
 * @brief Smooth saturation via hyperbolic tangent
 *
 * Maps any input into [-1, 1]. The transition from linear to saturated
 * is gradual — roughly linear for |x| < 0.5, fully saturated for |x| > 2.
 */
[[nodiscard]] inline float soft_clip_tanh(float x) noexcept {
    return std::tanh(x);
}

/**
 * @brief Polynomial soft clipper (cubic segment between -1 and +1)
 *
 * Identical to `tanh` for small signals but cheaper — uses only
 * multiplies and adds. Matches the classic ProDSP polynomial clipper.
 *
 * Exact: y = x for |x| <= 1/3, cubic blend to +/-1 for 1/3 < |x| < 2/3,
 * saturated for |x| >= 2/3.
 */
[[nodiscard]] inline float soft_clip_poly(float x) noexcept {
    const float ax = std::fabs(x);
    if (ax < 0.33333334F)
        return x;
    if (ax >= 0.6666667F)
        return (x > 0.0F) ? 1.0F : -1.0F;
    // Cubic transition: 1.5*x - 0.5*x^3  (scaled to [-1,1])
    return x * (1.5F - 0.5F * x * x);
}

/**
 * @brief Hard clipper — instant limiting at +/-1
 *
 * The cheapest option but produces the most harmonics (and potential
 * aliasing). Best used after an anti-aliasing filter or at low drive.
 */
[[nodiscard]] inline float hard_clip(float x) noexcept {
    return std::max(-1.0F, std::min(1.0F, x));
}

/**
 * @brief Buchla 259-style wavefolder
 *
 * Folds the input back on itself each time it exceeds the +/-1
 * boundary, producing rich, inharmonic content. The parameter
 * @p threshold controls the fold point: smaller values produce
 * more folds for a given input amplitude.
 *
 * @param x          Input sample (typically pre-gained)
 * @param threshold  Fold boundary in (0, 1]. Default = 1.0
 */
[[nodiscard]] inline float wavefold_buchla(float x, float threshold = 1.0F) noexcept {
    if (threshold <= 0.0F)
        threshold = 1.0F;
    // Normalize to the threshold, fold, then scale back
    x /= threshold;
    // Repeated folding: map x into [-1, 1] by mirroring at each boundary
    if (x > 1.0F) {
        x = 2.0F - x;
        if (x < -1.0F)
            x = -2.0F - x;  // catch double-fold
    } else if (x < -1.0F) {
        x = -2.0F - x;
        if (x > 1.0F)
            x = 2.0F - x;
    }
    return x * threshold;
}

/**
 * @brief Full-wave rectifier with optional polarity inversion
 *
 * Returns |x|. Useful as a building block for envelope followers
 * or as a waveshaper stage in itself (produces strong even harmonics).
 */
[[nodiscard]] inline float full_wave_rectify(float x) noexcept {
    return std::fabs(x);
}

// ===================================================================
// Shape enumeration for the processor wrapper
// ===================================================================

enum class WaveshaperShape : uint8_t { Tanh, Poly, HardClip, WaveFold };

// ===================================================================
// WaveshaperProcessor — block-level convenience wrapper
// ===================================================================

/**
 * @brief Applies a transfer function to a block with configurable drive
 *
 * Holds a @p drive (pre-gain) that amplifies the signal before the
 * transfer function. Higher drive produces more distortion.
 *
 * Thread safety: `set_drive()` and `set_shape()` are NOT thread-safe.
 * Set parameters before the audio callback, or synchronise externally.
 */
class WaveshaperProcessor {
public:
    explicit WaveshaperProcessor(WaveshaperShape shape = WaveshaperShape::Tanh,
                                 float drive = 1.0F) noexcept
        : shape_{shape}, drive_{drive} {}

    /**
     * @brief Process a single sample
     */
    [[nodiscard]] float process(float in) noexcept;

    /**
     * @brief Process a block of samples in-place
     */
    void process_block(float* buffer, std::size_t num_samples) noexcept;

    void set_drive(float gain) noexcept { drive_ = std::max(gain, 0.0F); }
    void set_shape(WaveshaperShape s) noexcept { shape_ = s; }

    [[nodiscard]] float get_drive() const noexcept { return drive_; }
    [[nodiscard]] WaveshaperShape get_shape() const noexcept { return shape_; }

private:
    WaveshaperShape shape_;
    float drive_;
};

// ===================================================================
// Inline implementations
// ===================================================================

inline float WaveshaperProcessor::process(float in) noexcept {
    const float driven = in * drive_;
    switch (shape_) {
        case WaveshaperShape::Tanh:
            return soft_clip_tanh(driven);
        case WaveshaperShape::Poly:
            return soft_clip_poly(driven);
        case WaveshaperShape::HardClip:
            return hard_clip(driven);
        case WaveshaperShape::WaveFold:
            return wavefold_buchla(driven);
    }
    return in;
}

inline void WaveshaperProcessor::process_block(float* buffer, std::size_t num_samples) noexcept {
    if (!buffer || num_samples == 0)
        return;
    switch (shape_) {
        case WaveshaperShape::Tanh:
            for (std::size_t i = 0; i < num_samples; ++i)
                buffer[i] = soft_clip_tanh(buffer[i] * drive_);
            break;
        case WaveshaperShape::Poly:
            for (std::size_t i = 0; i < num_samples; ++i)
                buffer[i] = soft_clip_poly(buffer[i] * drive_);
            break;
        case WaveshaperShape::HardClip:
            for (std::size_t i = 0; i < num_samples; ++i)
                buffer[i] = hard_clip(buffer[i] * drive_);
            break;
        case WaveshaperShape::WaveFold:
            for (std::size_t i = 0; i < num_samples; ++i)
                buffer[i] = wavefold_buchla(buffer[i] * drive_);
            break;
    }
}

}  // namespace sonicforge

#endif  // SONICFORGE_WAVESHAPER_HPP

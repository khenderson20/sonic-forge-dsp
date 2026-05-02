/**
 * @file delayline.hpp
 * @brief Fractional-sample delay line with configurable interpolation
 *
 * A circular-buffer delay line supporting none, linear, and 3rd-order
 * Lagrange interpolation. The interpolation type is selected at compile
 * time via a template parameter so there is zero dispatch overhead in
 * the audio path.
 *
 * ## Interpolation Modes
 *
 * | Mode            | Quality   | CPU cost | Use case                        |
 * |-----------------|-----------|----------|---------------------------------|
 * | `None`          | Lowest    | Minimal  | Integer delays, modulation-free |
 * | `Linear`        | Good      | Low      | Chorus, flanger, short delays   |
 * | `Lagrange3rd`   | Best      | Moderate | Pitch-shifting, precise delays  |
 *
 * ## Design Philosophy
 *
 * - The buffer is allocated once at construction (via `std::vector`);
 *   no heap activity in `process()` / `read()`.
 * - All parameters (`delay_samples`, `feedback`) are plain floats — set
 *   them before the audio callback or synchronise externally.
 *
 * ## Usage Example
 *
 * ```cpp
 * #include <sonicforge/delayline.hpp>
 *
 * // 0.5 second max delay at 48 kHz
 * sonicforge::DelayLine<sonicforge::DelayInterpolation::Linear> dl{24000};
 * dl.set_delay(4800.0F);   // 100 ms
 * dl.set_feedback(0.4F);
 *
 * float block[256];
 * // ... fill block ...
 * dl.process_block(block, 256);  // wet = dry + feedback loop
 *
 * // Or read without feedback (for modulation effects):
 * float tap = dl.read(2400.5F);  // fractional read
 * ```
 *
 * @author SonicForge DSP
 * @copyright MIT License
 */

#ifndef SONICFORGE_DELAYLINE_HPP
#define SONICFORGE_DELAYLINE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sonicforge {

/**
 * @brief Interpolation algorithms for fractional delay reads
 */
enum class DelayInterpolation : uint8_t {
    NONE,       /**< Nearest-sample (zero-order hold) */
    LINEAR,     /**< Linear interpolation between two samples */
    LAGRANGE3RD /**< 3rd-order Lagrange (4-tap) polynomial */
};

// Forward declarations
template <DelayInterpolation Interp> class DelayLine;

// ===================================================================
// None — integer-only delay
// ===================================================================

template <> class DelayLine<DelayInterpolation::NONE> {
public:
    /**
     * @brief Allocate a delay buffer capable of holding @p max_samples
     * @param max_samples Maximum delay length in samples
     */
    explicit DelayLine(std::size_t max_samples);

    /**
     * @brief Set the delay length in samples (rounded to nearest integer)
     */
    void set_delay(float samples) noexcept;

    /**
     * @brief Set the feedback amount [0, 1)
     */
    void set_feedback(float amount) noexcept;

    /**
     * @brief Read from the delay line at the current delay position (no feedback)
     * @return The delayed sample
     */
    [[nodiscard]] float read() const noexcept;

    /**
     * @brief Write input into the delay line and return the delayed output
     * @param in Input sample to write into the buffer
     * @return The delayed sample read from the buffer
     *
     * @note Feedback should be applied by the caller:
     *       @code
     *       float wet = dl.process(dry + feedback * wet_prev);
     *       @endcode
     */
    [[nodiscard]] float process(float in) noexcept;

    /**
     * @brief Process a block of samples in-place
     *
     * Each sample is written into the delay line and the delayed output
     * overwrites the buffer. For a feedback delay, the caller should mix
     * the feedback externally.
     */
    void process_block(float* buffer, std::size_t num_samples) noexcept;

    /**
     * @brief Reset the buffer contents to zero
     */
    void reset() noexcept;

    void set_max_delay(std::size_t max_samples);

    [[nodiscard]] float get_delay() const noexcept { return delay_samples_; }
    [[nodiscard]] float get_feedback() const noexcept { return feedback_; }
    [[nodiscard]] std::size_t get_max_delay() const noexcept { return buffer_.size(); }

private:
    std::size_t write_pos_{0};
    std::vector<float> buffer_;
    float delay_samples_{0.0F};
    float feedback_{0.0F};
    int int_delay_{0};
};

// ===================================================================
// Linear — 2-tap linear interpolation
// ===================================================================

template <> class DelayLine<DelayInterpolation::LINEAR> {
public:
    explicit DelayLine(std::size_t max_samples);

    void set_delay(float samples) noexcept;
    void set_feedback(float amount) noexcept;

    /**
     * @brief Read at an arbitrary fractional position (no feedback)
     * @param delay_samples Fractional delay length
     */
    [[nodiscard]] float read(float delay_samples) const noexcept;

    [[nodiscard]] float process(float in) noexcept;
    void process_block(float* buffer, std::size_t num_samples) noexcept;
    void reset() noexcept;

    void set_max_delay(std::size_t max_samples);

    [[nodiscard]] float get_delay() const noexcept { return delay_samples_; }
    [[nodiscard]] float get_feedback() const noexcept { return feedback_; }
    [[nodiscard]] std::size_t get_max_delay() const noexcept { return buffer_.size(); }

private:
    [[nodiscard]] float read_internal(float delay_samples) const noexcept;

    std::size_t write_pos_{0};
    std::vector<float> buffer_;
    float delay_samples_{0.0F};
    float feedback_{0.0F};
};

// ===================================================================
// Lagrange3rd — 4-tap cubic Lagrange interpolation
// ===================================================================

template <> class DelayLine<DelayInterpolation::LAGRANGE3RD> {
public:
    explicit DelayLine(std::size_t max_samples);

    void set_delay(float samples) noexcept;
    void set_feedback(float amount) noexcept;

    [[nodiscard]] float read(float delay_samples) const noexcept;

    [[nodiscard]] float process(float in) noexcept;
    void process_block(float* buffer, std::size_t num_samples) noexcept;
    void reset() noexcept;

    void set_max_delay(std::size_t max_samples);

    [[nodiscard]] float get_delay() const noexcept { return delay_samples_; }
    [[nodiscard]] float get_feedback() const noexcept { return feedback_; }
    [[nodiscard]] std::size_t get_max_delay() const noexcept { return buffer_.size(); }

private:
    [[nodiscard]] float read_internal(float delay_samples) const noexcept;

    std::size_t write_pos_{0};
    std::vector<float> buffer_;
    float delay_samples_{0.0F};
    float feedback_{0.0F};
};

} // namespace sonicforge

#endif // SONICFORGE_DELAYLINE_HPP

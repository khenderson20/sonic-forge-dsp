/**
 * @file state_variable_filter.hpp
 * @brief Resonant multi-mode state variable filter (Cytomic / Zavalishin topology)
 *
 * Provides lowpass, highpass, bandpass, and notch responses from a single
 * zero-delay feedback (ZDF) structure. The filter is numerically stable
 * across the full audible range and supports real-time modulation of cutoff
 * and resonance without zipper noise (pair with SmoothedValue for best results).
 *
 * ## Design Philosophy
 *
 * - Thread-safe parameter setters via `std::atomic` — safe to call from any
 *   thread while the audio callback is running.
 * - Zero heap allocation in the processing path.
 * - Block and sample-by-sample APIs matching the Oscillator convention.
 *
 * ## Stability
 *
 * Resonance is clamped internally to prevent self-oscillation blow-up when
 * the cutoff frequency approaches Nyquist. The clamping is transparent to
 * the caller — the stored Q value is unchanged; only the coefficient used
 * in the difference equation is reduced.
 *
 * ## Usage Example
 *
 * ```cpp
 * #include <sonicforge/state_variable_filter.hpp>
 *
 * sonicforge::StateVariableFilter filter{
 *     sonicforge::FilterMode::Lowpass, 2000.0F, 0.5F
 * };
 * filter.set_sample_rate(48000.0F);
 *
 * float block[256];
 * // ... fill block with audio ...
 * filter.process_block(block, 256);
 *
 * // Modulate from UI thread:
 * filter.set_cutoff_hz(4000.0F);
 * filter.set_resonance(0.7F);
 * ```
 *
 * @author SonicForge DSP
 * @copyright MIT License
 */

#ifndef SONICFORGE_STATE_VARIABLE_FILTER_HPP
#define SONICFORGE_STATE_VARIABLE_FILTER_HPP

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace sonicforge {

/**
 * @brief Filter response modes supported by StateVariableFilter
 */
enum class FilterMode : uint8_t {
    Lowpass,  /**< Attenuates frequencies above cutoff */
    Highpass, /**< Attenuates frequencies below cutoff */
    Bandpass, /**< Passes a narrow band around cutoff */
    Notch     /**< Rejects a narrow band around cutoff */
};

/**
 * @brief Multi-mode state variable filter using ZDF / TPT topology
 *
 * Based on the Cytomic SVF formulation (Andy Simper), itself derived from
 * Zavalishin's zero-delay feedback approach. Uses trapezoidal integration
 * instead of forward-Euler, making it stable up to Nyquist.
 *
 * ## Thread safety
 *
 * - `process()` / `process_block()` — audio thread only
 * - `set_cutoff_hz()` / `set_resonance()` / `set_mode()` — thread-safe;
 *   can be called from any thread while audio is running.
 */
class StateVariableFilter {
public:
    /**
     * @brief Construct the filter with initial parameters
     * @param mode       Filter response type
     * @param cutoff_hz  Centre / corner frequency in Hz
     * @param resonance  Resonance amount in [0, 1]  (1 = self-oscillation edge)
     * @param sample_rate Audio sample rate in Hz (default 48 kHz)
     */
    StateVariableFilter(FilterMode mode = FilterMode::Lowpass, float cutoff_hz = 1000.0F, float resonance = 0.5F,
                        float sample_rate = 48000.0F);

    // -----------------------------------------------------------------------
    // Audio processing
    // -----------------------------------------------------------------------

    /**
     * @brief Process a single sample
     * @param in  Input sample
     * @return    Filtered output (depends on current mode)
     */
    [[nodiscard]] float process(float in) noexcept;

    /**
     * @brief Process a block of samples in-place
     * @param buffer      Read/write buffer
     * @param num_samples Number of samples to process
     */
    void process_block(float* buffer, std::size_t num_samples) noexcept;

    // -----------------------------------------------------------------------
    // Parameter setters (thread-safe)
    // -----------------------------------------------------------------------

    void set_cutoff_hz(float hz) noexcept;
    void set_resonance(float q) noexcept;
    void set_mode(FilterMode mode) noexcept;
    void set_sample_rate(float sample_rate) noexcept;

    // -----------------------------------------------------------------------
    // Utilities
    // -----------------------------------------------------------------------

    /**
     * @brief Clear internal state (set delay elements to zero)
     */
    void reset() noexcept;

    // -----------------------------------------------------------------------
    // Getters
    // -----------------------------------------------------------------------

    [[nodiscard]] float get_cutoff_hz() const noexcept;
    [[nodiscard]] float get_resonance() const noexcept;
    [[nodiscard]] FilterMode get_mode() const noexcept;
    [[nodiscard]] float get_sample_rate() const noexcept;

private:
    void recalc_coefficients() noexcept;

    // Atomic parameters (written from any thread, read in audio thread)
    std::atomic<float> cutoff_hz_{1000.0F};
    std::atomic<float> resonance_{0.5F};
    std::atomic<FilterMode> mode_{FilterMode::Lowpass};
    std::atomic<float> sample_rate_{48000.0F};

    // Cached values for change detection (audio thread only)
    float cached_cutoff_hz_{-1.0F};
    float cached_resonance_{-1.0F};
    FilterMode cached_mode_{FilterMode::Highpass}; // deliberately different
    float cached_sample_rate_{-1.0F};

    // Filter coefficients (recomputed when parameters change)
    float g_ = 0.0F; // tan(pi * fc / fs)
    float R_ = 1.0F; // 1 / Q  (damping)
    float H_ = 0.0F; // 1 / (1 + R*g + g^2)

    // State variables (integrator outputs)
    float ic1eq_ = 0.0F;
    float ic2eq_ = 0.0F;
};

} // namespace sonicforge

#endif // SONICFORGE_STATE_VARIABLE_FILTER_HPP

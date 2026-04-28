/**
 * @file oscillator.hpp
 * @brief Digital oscillator implementations for audio synthesis
 *
 * This module provides various waveform generators optimized for real-time
 * audio processing. All implementations are designed to be allocation-free
 * in the audio callback path.
 *
 * ## Anti-Aliasing (PolyBLEP)
 *
 * Sawtooth and square waveforms use 2-point PolyBLEP (Polynomial Bandlimited
 * Step) to suppress harmonic aliasing at high frequencies.  A naive saw/square
 * has infinite harmonics at every discontinuity; PolyBLEP applies a polynomial
 * correction over the single sample on each side of the discontinuity,
 * effectively lowpass-filtering the step while leaving the rest of the waveform
 * unchanged.
 *
 * ## Sine Generation (LUT)
 *
 * Sine generation uses a 4096-entry lookup table with linear interpolation,
 * giving a peak error of ~2.3e-6 (~−113 dB) versus std::sin at a fraction of
 * the computational cost.  The table is initialised once at program start and
 * shared across all Oscillator instances.
 *
 * ## Thread-Safety Model
 *
 * - process() / process_block()  — call only from the audio thread
 * - set_frequency() / set_waveform() / set_sample_rate() — thread-safe; can
 *   be called from any thread while audio is running (std::atomic)
 * - get_*() / reset_phase() / set_phase() — NOT guaranteed thread-safe when
 *   called concurrently with process(); document your own synchronisation if
 *   needed
 *
 * @author SonicForge DSP
 * @copyright MIT License
 */

#ifndef SONICFORGE_OSCILLATOR_HPP
#define SONICFORGE_OSCILLATOR_HPP

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace sonicforge {

/**
 * @brief Available waveform types for the oscillator
 */
enum class Waveform : uint8_t {
    SINE,     ///< Pure sine wave — fundamental, LUT-accelerated
    SAW,      ///< Sawtooth wave — rich harmonics, PolyBLEP anti-aliased
    SQUARE,   ///< Square wave — odd harmonics only, PolyBLEP anti-aliased
    TRIANGLE  ///< Triangle wave — softer odd harmonics, naturally band-limited
};

/**
 * @brief High-performance digital oscillator for audio synthesis
 *
 * The Oscillator class generates periodic waveforms suitable for audio
 * synthesis applications. It uses phase accumulation for frequency control
 * and supports real-time parameter modulation via atomic operations.
 *
 * ## Design Philosophy
 *
 * This oscillator is designed for real-time audio with these constraints:
 * - No heap allocation in process() / process_block() calls
 * - Lock-free parameter updates using std::atomic
 * - Deterministic, bounded execution time
 *
 * ## Usage Example
 *
 * ```cpp
 * sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0F);
 * osc.set_sample_rate(48000.0F);
 *
 * // In audio callback:
 * float buffer[256];
 * osc.process_block(buffer, 256);
 * ```
 *
 * ## Technical Notes
 *
 * - Phase is maintained in the range [0, 1) to prevent floating-point
 *   precision loss over long playback times.
 * - Frequency changes are applied on the next process() call.
 * - Output is guaranteed to stay within [-1.0, 1.0].
 * - An invalid Waveform value passed to the constructor or set_waveform()
 *   is silently ignored; the oscillator keeps/defaults to Waveform::SINE.
 */
class Oscillator {
public:
    /**
     * @brief Construct an oscillator with specified waveform and frequency
     *
     * @param waveform    The type of waveform to generate
     * @param frequency   Initial frequency in Hz (default: 440.0 Hz — A4)
     * @param sample_rate Audio sample rate in Hz (default: 48000.0 Hz)
     *
     * @note  Invalid waveform values are silently replaced by Waveform::SINE.
     *        Non-positive frequency or sample_rate values are replaced by
     *        their respective defaults (440 Hz / 48000 Hz).
     */
    explicit Oscillator(Waveform waveform = Waveform::SINE, float frequency = 440.0F,
                        float sample_rate = 48000.0F);

    /**
     * @brief Generate the next audio sample
     *
     * This is the main audio processing function. Call this once per sample
     * in your audio callback to generate the waveform.
     *
     * @return float The next sample value in the range [-1.0, 1.0]
     *
     * @note This function is real-time safe: no allocations, no blocking
     */
    [[nodiscard]] float process() noexcept;

    /**
     * @brief Process a block of samples into a buffer
     *
     * More efficient than calling process() repeatedly when processing
     * larger buffers — the waveform type is resolved once before the loop,
     * avoiding per-sample dispatch overhead.
     *
     * @param buffer      Pointer to the output buffer (must not be null)
     * @param num_samples Number of samples to generate
     *
     * @note Buffer must be pre-allocated with at least num_samples floats.
     *       Passing a null pointer or num_samples == 0 is safe (no-op).
     */
    void process_block(float* buffer, std::size_t num_samples) noexcept;

    // =========================================================================
    // Parameter Setters (thread-safe for real-time modulation)
    // =========================================================================

    /**
     * @brief Set the oscillator frequency
     *
     * Thread-safe: can be called from any thread while audio is processing.
     * The new frequency takes effect on the next process() call.
     *
     * @param frequency New frequency in Hz, clamped to [0.1, Nyquist]
     */
    void set_frequency(float frequency) noexcept;

    /**
     * @brief Set the waveform type
     *
     * Thread-safe. Invalid enum values are silently ignored.
     *
     * @param waveform New waveform to generate
     */
    void set_waveform(Waveform waveform) noexcept;

    /**
     * @brief Set the audio sample rate
     *
     * Call this when your audio engine's sample rate changes.
     * Non-positive values are silently ignored.
     *
     * @param sample_rate New sample rate in Hz
     */
    void set_sample_rate(float sample_rate) noexcept;

    /**
     * @brief Reset the oscillator phase to zero
     *
     * Useful for retriggering the oscillator or synchronising
     * multiple oscillators to the same starting point.
     */
    void reset_phase() noexcept;

    /**
     * @brief Set the phase directly
     *
     * @param phase New phase value in range [0.0, 1.0); values outside this
     *              range are wrapped via subtraction of the integer part.
     */
    void set_phase(float phase) noexcept;

    // =========================================================================
    // Parameter Getters
    // =========================================================================

    /**
     * @brief Get the current frequency
     * @return float Current frequency in Hz
     */
    [[nodiscard]] float get_frequency() const noexcept;

    /**
     * @brief Get the current waveform type
     * @return Waveform Current waveform
     */
    [[nodiscard]] Waveform get_waveform() const noexcept;

    /**
     * @brief Get the current sample rate
     * @return float Current sample rate in Hz
     */
    [[nodiscard]] float get_sample_rate() const noexcept;

    /**
     * @brief Get the current phase
     * @return float Current phase in range [0.0, 1.0)
     *
     * @warning Not thread-safe when called concurrently with process().
     */
    [[nodiscard]] float get_phase() const noexcept;

private:
    /**
     * @brief Generate a sine wave sample at the current phase (LUT-based)
     * @return float Sample value in [-1.0, 1.0]
     */
    [[nodiscard]] float generate_sine() const noexcept;

    /**
     * @brief Generate a PolyBLEP-corrected sawtooth sample
     * @return float Sample value in [-1.0, 1.0]
     */
    [[nodiscard]] float generate_saw() const noexcept;

    /**
     * @brief Generate a PolyBLEP-corrected square wave sample
     * @return float Sample value in [-1.0, 1.0]
     */
    [[nodiscard]] float generate_square() const noexcept;

    /**
     * @brief Generate a triangle wave sample at the current phase
     * @return float Sample value in [-1.0, 1.0]
     */
    [[nodiscard]] float generate_triangle() const noexcept;

    /**
     * @brief Advance the phase accumulator by one sample
     *
     * Increments phase_ by (frequency / sample_rate) and wraps to [0, 1).
     */
    void advance_phase() noexcept;

    // -------------------------------------------------------------------------
    // Waveform dispatch
    // -------------------------------------------------------------------------

    /// Pointer-to-member type for the four waveform generators.
    using GeneratorFn = float (Oscillator::*)() const noexcept;

    /**
     * @brief Return the generator function pointer for @p wf.
     *
     * Centralises the dispatch table so both process() and process_block()
     * share one definition.  Declared as a static member so it can access
     * the private generate_* methods.
     */
    [[nodiscard]] static GeneratorFn resolve_generator(Waveform wf) noexcept;

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Current phase position [0.0, 1.0)
    float phase_{0.0F};

    /// Oscillator frequency in Hz — atomic for thread-safe modulation
    std::atomic<float> frequency_{440.0F};

    /// Audio sample rate in Hz — atomic for thread-safe sample-rate changes
    std::atomic<float> sample_rate_{48000.0F};

    /// Current waveform type — atomic for thread-safe waveform switching
    std::atomic<Waveform> waveform_{Waveform::SINE};

    /// Cached 2π constant
    static constexpr float TWO_PI = 2.0F * 3.14159265358979323846F;
};

}  // namespace sonicforge

#endif  // SONICFORGE_OSCILLATOR_HPP

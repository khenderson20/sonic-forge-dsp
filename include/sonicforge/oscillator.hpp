/**
 * @file oscillator.hpp
 * @brief Digital oscillator implementations for audio synthesis
 * 
 * This module provides various waveform generators optimized for real-time
 * audio processing. All implementations are designed to be allocation-free
 * in the audio callback path.
 * 
 * @author SonicForge DSP
 * @copyright MIT License
 */

#ifndef SONICFORGE_OSCILLATOR_HPP
#define SONICFORGE_OSCILLATOR_HPP

#include <cmath>
#include <atomic>
#include <cstdint>

namespace sonicforge {

/**
 * @brief Available waveform types for the oscillator
 */
enum class Waveform : uint8_t {
    SINE,      ///< Pure sine wave - fundamental waveform
    SAW,       ///< Sawtooth wave - rich in harmonics
    SQUARE,    ///< Square wave - odd harmonics only
    TRIANGLE   ///< Triangle wave - softer odd harmonics
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
 * - No heap allocation in process() calls
 * - Lock-free parameter updates using std::atomic
 * - Deterministic execution time
 * 
 * ## Usage Example
 * 
 * ```cpp
 * sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0f);
 * osc.set_sample_rate(48000.0f);
 * 
 * // In audio callback:
 * for (int i = 0; i < buffer_size; ++i) {
 *     buffer[i] = osc.process();
 * }
 * ```
 * 
 * ## Technical Notes
 * 
 * - Phase is maintained in the range [0, 1) to prevent floating-point
 *   precision issues over long playback times
 * - Frequency changes are smoothly applied on the next process() call
 * - The oscillator outputs in the range [-1.0, 1.0]
 */
class Oscillator {
public:
    /**
     * @brief Construct an oscillator with specified waveform and frequency
     * 
     * @param waveform The type of waveform to generate
     * @param frequency Initial frequency in Hz (default: 440.0 Hz - A4)
     * @param sample_rate Audio sample rate in Hz (default: 48000.0 Hz)
     * 
     * @note The sample rate should match your audio system's configuration
     */
    explicit Oscillator(Waveform waveform = Waveform::SINE,
                        float frequency = 440.0f,
                        float sample_rate = 48000.0f);
    
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
     * larger buffers, as it reduces function call overhead.
     * 
     * @param buffer Pointer to the output buffer
     * @param num_samples Number of samples to generate
     * 
     * @note Buffer must be pre-allocated with at least num_samples capacity
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
     * @param frequency New frequency in Hz (must be positive)
     */
    void set_frequency(float frequency) noexcept;
    
    /**
     * @brief Set the waveform type
     * 
     * @param waveform New waveform to generate
     */
    void set_waveform(Waveform waveform) noexcept;
    
    /**
     * @brief Set the audio sample rate
     * 
     * Call this when your audio engine's sample rate changes.
     * 
     * @param sample_rate New sample rate in Hz
     */
    void set_sample_rate(float sample_rate) noexcept;
    
    /**
     * @brief Reset the oscillator phase to zero
     * 
     * Useful for retriggering the oscillator or synchronizing
     * multiple oscillators.
     */
    void reset_phase() noexcept;
    
    /**
     * @brief Set the phase directly
     * 
     * @param phase New phase value in range [0.0, 1.0)
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
     */
    [[nodiscard]] float get_phase() const noexcept;

private:
    /**
     * @brief Generate a sine wave sample at the current phase
     * @return float Sample value in [-1.0, 1.0]
     */
    [[nodiscard]] float generate_sine() const noexcept;
    
    /**
     * @brief Generate a sawtooth wave sample at the current phase
     * @return float Sample value in [-1.0, 1.0]
     */
    [[nodiscard]] float generate_saw() const noexcept;
    
    /**
     * @brief Generate a square wave sample at the current phase
     * @return float Sample value in [-1.0, 1.0]
     */
    [[nodiscard]] float generate_square() const noexcept;
    
    /**
     * @brief Generate a triangle wave sample at the current phase
     * @return float Sample value in [-1.0, 1.0]
     */
    [[nodiscard]] float generate_triangle() const noexcept;
    
    /**
     * @brief Advance the phase by one sample
     * 
     * Increments phase based on frequency and sample rate,
     * wrapping to [0, 1) range.
     */
    void advance_phase() noexcept;

    // =========================================================================
    // Member Variables
    // =========================================================================
    
    /// Current phase position [0.0, 1.0)
    float phase_{0.0f};
    
    /// Oscillator frequency in Hz (atomic for thread-safe modulation)
    std::atomic<float> frequency_{440.0f};
    
    /// Audio sample rate in Hz
    std::atomic<float> sample_rate_{48000.0f};
    
    /// Current waveform type
    std::atomic<Waveform> waveform_{Waveform::SINE};
    
    /// Mathematical constant for 2*PI (cached for performance)
    static constexpr float TWO_PI = 2.0f * 3.14159265358979323846f;
};

} // namespace sonicforge

#endif // SONICFORGE_OSCILLATOR_HPP

/**
 * @file sine_example.cpp
 * @brief Minimal example: Generate a sine wave and output to stdout
 *
 * This example demonstrates the most basic use of the SonicForge oscillator.
 * It generates raw audio samples that can be piped to an audio player.
 *
 * ## How to Use
 *
 * Build and run:
 * ```bash
 * mkdir build && cd build
 * cmake .. && make
 * ./sine_example | aplay -f FLOAT_LE -r 48000 -c 1
 * ```
 *
 * Or save to a file and play:
 * ```bash
 * ./sine_example > sine.raw
 * aplay -f FLOAT_LE -r 48000 -c 1 sine.raw
 * ```
 *
 * ## Learning Points
 *
 * 1. Audio is just numbers! Each float represents one sample.
 * 2. Sample rate determines how many samples per second (48000 here).
 * 3. The oscillator's process() function returns one sample at a time.
 * 4. Phase accumulation creates a continuous wave from discrete samples.
 *
 * @author SonicForge DSP
 * @copyright MIT License
 */

#include <sonicforge/oscillator.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

/**
 * @brief Simple program to generate and output a sine wave
 *
 * Command line usage:
 *   sine_example [frequency] [duration_seconds]
 *
 * Defaults:
 *   frequency: 440 Hz (A4 - concert pitch)
 *   duration: 3 seconds
 */
int main(int argc, char* argv[]) {
    // ==========================================================================
    // Configuration
    // ==========================================================================

    // Parse command line arguments (optional)
    float frequency = 440.0F;  // Hz - A4 (concert pitch)
    float duration = 3.0F;     // seconds

    if (argc >= 2) {
        frequency = std::stof(argv[1]);
    }
    if (argc >= 3) {
        duration = std::stof(argv[2]);
    }

    // Audio settings
    constexpr float SAMPLE_RATE = 48000.0F;  // 48 kHz - standard professional rate

    // Calculate total samples to generate
    const std::size_t total_samples = static_cast<std::size_t>(SAMPLE_RATE * duration);

    // ==========================================================================
    // Create and Configure Oscillator
    // ==========================================================================

    // Create a sine oscillator at the specified frequency
    //
    // Learning note: The oscillator maintains internal state (phase).
    // Each call to process() advances this phase and returns the next sample.
    sonicforge::Oscillator oscillator(sonicforge::Waveform::SINE,  // Waveform type
                                      frequency,                   // Frequency in Hz
                                      SAMPLE_RATE                  // Sample rate in Hz
    );

    // ==========================================================================
    // Print Info to stderr (so it doesn't mix with audio data)
    // ==========================================================================

    std::cerr << "SonicForge DSP - Sine Wave Generator\n";
    std::cerr << "=====================================\n";
    std::cerr << "Frequency:     " << frequency << " Hz\n";
    std::cerr << "Sample Rate:   " << SAMPLE_RATE << " Hz\n";
    std::cerr << "Duration:      " << duration << " seconds\n";
    std::cerr << "Total Samples: " << total_samples << "\n";
    std::cerr << "\nGenerating audio...\n";
    std::cerr << "Pipe to: aplay -f FLOAT_LE -r 48000 -c 1\n";
    std::cerr << "=====================================\n";

    // ==========================================================================
    // Generate and Output Audio Samples
    // ==========================================================================

    // Process samples in blocks for efficiency
    // (Though for this example, block size doesn't matter much)
    constexpr std::size_t BLOCK_SIZE = 512;
    float buffer[BLOCK_SIZE];

    std::size_t samples_remaining = total_samples;

    while (samples_remaining > 0) {
        // Calculate how many samples to process this iteration
        const std::size_t samples_this_block =
            (samples_remaining > BLOCK_SIZE) ? BLOCK_SIZE : samples_remaining;

        // Generate a block of samples
        // Using process_block() is more efficient than calling process() in a loop
        oscillator.process_block(buffer, samples_this_block);

        // Write raw float samples to stdout
        // Note: This writes binary data, not text!
        //
        // Learning point: Audio data is typically handled as binary floats
        // or integers. The format must match what your playback system expects.
        std::cout.write(reinterpret_cast<const char*>(buffer),
                        static_cast<std::streamsize>(samples_this_block * sizeof(float)));

        samples_remaining -= samples_this_block;
    }

    std::cerr << "Done! Generated " << total_samples << " samples.\n";

    return 0;
}

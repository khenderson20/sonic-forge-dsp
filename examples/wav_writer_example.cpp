/**
 * @file wav_writer_example.cpp
 * @brief Generate a sine wave and save it as a WAV file
 * 
 * This example demonstrates:
 * 1. Using the SonicForge oscillator to generate audio
 * 2. Writing a proper WAV file header
 * 3. Converting float samples to 16-bit PCM
 * 
 * The output can be opened in any audio player or DAW (Audacity, VLC, etc.)
 * 
 * ## How to Use
 * 
 * ```bash
 * mkdir build && cd build
 * cmake .. && make
 * ./wav_writer_example output.wav 440 2.0
 * ```
 * 
 * This creates a 2-second 440Hz sine wave saved as output.wav
 * 
 * ## Learning Points
 * 
 * 1. WAV format is one of the simplest audio file formats
 * 2. Audio samples need to be converted from float [-1,1] to integer range
 * 3. File headers contain metadata about sample rate, bit depth, etc.
 * 4. Understanding file formats is essential for audio programming
 * 
 * @author SonicForge DSP
 * @copyright MIT License
 */

#include <sonicforge/oscillator.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cmath>

// =============================================================================
// WAV File Format Structures
// =============================================================================

/**
 * @brief WAV file header structure (44 bytes total)
 * 
 * The WAV format is organized in "chunks":
 * - RIFF chunk: File identification and size
 * - fmt chunk: Audio format specification
 * - data chunk: Raw audio samples
 * 
 * Learning note: The WAV format uses little-endian byte order
 * (least significant byte first) on most systems.
 */
#pragma pack(push, 1)  // Ensure no padding between struct members
struct WavHeader {
    // RIFF chunk descriptor
    char riff_id[4] = {'R', 'I', 'F', 'F'};   // "RIFF" identifier
    uint32_t file_size = 0;                    // File size - 8 bytes
    char wave_id[4] = {'W', 'A', 'V', 'E'};   // "WAVE" format
    
    // fmt sub-chunk (format specification)
    char fmt_id[4] = {'f', 'm', 't', ' '};    // "fmt " identifier
    uint32_t fmt_size = 16;                    // Size of fmt chunk (16 for PCM)
    uint16_t audio_format = 1;                 // Audio format (1 = PCM)
    uint16_t num_channels = 1;                 // Number of channels (1 = mono)
    uint32_t sample_rate = 48000;              // Samples per second
    uint32_t byte_rate = 0;                    // Bytes per second
    uint16_t block_align = 0;                  // Bytes per sample frame
    uint16_t bits_per_sample = 16;             // Bits per sample (16-bit)
    
    // data sub-chunk
    char data_id[4] = {'d', 'a', 't', 'a'};   // "data" identifier
    uint32_t data_size = 0;                    // Size of audio data in bytes
};
#pragma pack(pop)

/**
 * @brief Write a WAV file from float samples
 * 
 * @param filename Output filename
 * @param samples Vector of float samples in range [-1.0, 1.0]
 * @param sample_rate Sample rate in Hz
 * @return true if successful, false otherwise
 */
bool write_wav_file(const std::string& filename,
                    const std::vector<float>& samples,
                    uint32_t sample_rate) {
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << filename << "\n";
        return false;
    }
    
    // Calculate header values
    const uint32_t num_samples = static_cast<uint32_t>(samples.size());
    const uint16_t num_channels = 1;          // Mono
    const uint16_t bits_per_sample = 16;      // 16-bit audio
    const uint16_t bytes_per_sample = bits_per_sample / 8;
    
    WavHeader header;
    header.num_channels = num_channels;
    header.sample_rate = sample_rate;
    header.bits_per_sample = bits_per_sample;
    header.block_align = num_channels * bytes_per_sample;
    header.byte_rate = sample_rate * header.block_align;
    header.data_size = num_samples * bytes_per_sample;
    header.file_size = 36 + header.data_size;  // Total file size - 8
    
    // Write header
    file.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
    
    // Convert and write samples
    // Float [-1.0, 1.0] -> 16-bit signed integer [-32768, 32767]
    //
    // Learning note: This conversion is crucial in audio programming.
    // Float representation is convenient for processing (normalized range),
    // but file formats and hardware often use integer representations.
    for (float sample : samples) {
        // Clamp to valid range to prevent overflow
        sample = std::clamp(sample, -1.0f, 1.0f);
        
        // Scale to 16-bit range
        // Note: We use 32767 (not 32768) to ensure symmetric clipping
        int16_t sample_16bit = static_cast<int16_t>(sample * 32767.0f);
        
        file.write(reinterpret_cast<const char*>(&sample_16bit), sizeof(int16_t));
    }
    
    file.close();
    return true;
}

/**
 * @brief Main entry point
 * 
 * Usage: wav_writer_example <output.wav> [frequency] [duration]
 */
int main(int argc, char* argv[]) {
    // ==========================================================================
    // Parse Arguments
    // ==========================================================================
    
    std::string output_filename = "output.wav";
    float frequency = 440.0f;
    float duration = 2.0f;
    
    if (argc >= 2) {
        output_filename = argv[1];
    }
    if (argc >= 3) {
        frequency = std::stof(argv[2]);
    }
    if (argc >= 4) {
        duration = std::stof(argv[3]);
    }
    
    // ==========================================================================
    // Configuration
    // ==========================================================================
    
    constexpr uint32_t SAMPLE_RATE = 48000;
    const std::size_t total_samples = 
        static_cast<std::size_t>(static_cast<float>(SAMPLE_RATE) * duration);
    
    std::cout << "SonicForge DSP - WAV File Writer\n";
    std::cout << "================================\n";
    std::cout << "Output File:   " << output_filename << "\n";
    std::cout << "Frequency:     " << frequency << " Hz\n";
    std::cout << "Sample Rate:   " << SAMPLE_RATE << " Hz\n";
    std::cout << "Duration:      " << duration << " seconds\n";
    std::cout << "Total Samples: " << total_samples << "\n";
    std::cout << "================================\n";
    
    // ==========================================================================
    // Generate Audio
    // ==========================================================================
    
    std::cout << "Generating sine wave...\n";
    
    // Create oscillator
    sonicforge::Oscillator oscillator(
        sonicforge::Waveform::SINE,
        frequency,
        static_cast<float>(SAMPLE_RATE)
    );
    
    // Pre-allocate buffer for all samples
    // Learning note: Pre-allocation is important for performance.
    // In real-time audio, you'd never allocate during playback.
    std::vector<float> samples;
    samples.reserve(total_samples);
    
    // Generate samples
    for (std::size_t i = 0; i < total_samples; ++i) {
        samples.push_back(oscillator.process());
    }
    
    std::cout << "Generated " << samples.size() << " samples.\n";
    
    // ==========================================================================
    // Apply Simple Fade-In/Fade-Out (to avoid clicks)
    // ==========================================================================
    
    // Learning note: Abrupt starts and stops create clicks (discontinuities).
    // A fade-in/fade-out envelope smooths these transitions.
    
    const std::size_t fade_samples = static_cast<std::size_t>(0.01f * SAMPLE_RATE); // 10ms fade
    
    // Fade in
    for (std::size_t i = 0; i < fade_samples && i < samples.size(); ++i) {
        const float fade_factor = static_cast<float>(i) / static_cast<float>(fade_samples);
        samples[i] *= fade_factor;
    }
    
    // Fade out
    for (std::size_t i = 0; i < fade_samples && i < samples.size(); ++i) {
        const std::size_t idx = samples.size() - 1 - i;
        const float fade_factor = static_cast<float>(i) / static_cast<float>(fade_samples);
        samples[idx] *= fade_factor;
    }
    
    std::cout << "Applied 10ms fade-in/fade-out.\n";
    
    // ==========================================================================
    // Write WAV File
    // ==========================================================================
    
    std::cout << "Writing WAV file...\n";
    
    if (write_wav_file(output_filename, samples, SAMPLE_RATE)) {
        std::cout << "Successfully wrote: " << output_filename << "\n";
        std::cout << "\nYou can play it with:\n";
        std::cout << "  aplay " << output_filename << "\n";
        std::cout << "  or open in Audacity to visualize the waveform!\n";
        return 0;
    } else {
        std::cerr << "Failed to write WAV file.\n";
        return 1;
    }
}

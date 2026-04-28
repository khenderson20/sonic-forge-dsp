/**
 * @file sonicforge_worklet.cpp
 * @brief WebAssembly C bridge for SonicForge DSP
 *
 * Compiled to a raw Wasm binary via Emscripten (STANDALONE_WASM).
 * Loaded inside an AudioWorkletProcessor via postMessage transfer
 * and manual WebAssembly.instantiate().
 *
 * Memory layout
 * -------------
 * g_buffer is a static float array. The AudioWorklet processor accesses
 * it via a Float32Array view into the Wasm linear memory every render quantum.
 */

#include <emscripten/emscripten.h>
#include <sonicforge/oscillator.hpp>

#include <cmath>

/// Fixed-size output buffer (128 = AudioWorklet render quantum).
static float g_buffer[128];

/// Single global oscillator instance managed by the worklet.
static sonicforge::Oscillator* g_oscillator = nullptr;

extern "C" {

/**
 * @brief Return the address of the output sample buffer.
 *
 * The JS side uses this pointer to create a Float32Array view into the
 * Wasm heap, avoiding any per-quantum allocation or copy.
 *
 * @return Pointer to a 128-element float array.
 */
EMSCRIPTEN_KEEPALIVE
float* get_buffer() {
    return g_buffer;
}

/**
 * @brief Create (or replace) the oscillator.
 *
 * @param waveform    0=SINE, 1=SAW, 2=SQUARE, 3=TRIANGLE
 * @param frequency   Frequency in Hz (must be > 0)
 * @param sample_rate Audio sample rate in Hz — must match AudioContext.sampleRate
 *
 * @return  0 on success
 *         -1 if @p waveform is outside the range [0, 3]
 *         -2 if @p frequency is not a positive finite number
 *         -3 if @p sample_rate is not a positive finite number
 */
EMSCRIPTEN_KEEPALIVE
int audio_init(int waveform, float frequency, float sample_rate) {
    if (waveform < 0 || waveform > 3) return -1;
    if (frequency <= 0.0f || !std::isfinite(frequency)) return -2;
    if (sample_rate <= 0.0f || !std::isfinite(sample_rate)) return -3;

    delete g_oscillator;
    g_oscillator = new sonicforge::Oscillator(
        static_cast<sonicforge::Waveform>(waveform),
        frequency,
        sample_rate
    );
    return 0;
}

/**
 * @brief Generate one render quantum of samples into g_buffer.
 *
 * Called every 128 samples (~2.6 ms at 48 kHz) from the AudioWorklet
 * process() callback. Real-time safe: no heap allocation, no blocking.
 *
 * @param num_samples Number of samples to generate (normally 128).
 */
EMSCRIPTEN_KEEPALIVE
void audio_process(int num_samples) {
    if (g_oscillator) {
        g_oscillator->process_block(g_buffer, static_cast<std::size_t>(num_samples));
    }
}

/**
 * @brief Update the oscillator frequency.
 *
 * Thread-safe via std::atomic inside the SonicForge oscillator.
 *
 * @param frequency New frequency in Hz.
 */
EMSCRIPTEN_KEEPALIVE
void audio_set_frequency(float frequency) {
    if (g_oscillator) g_oscillator->set_frequency(frequency);
}

/**
 * @brief Change the oscillator waveform.
 *
 * @param waveform 0=SINE, 1=SAW, 2=SQUARE, 3=TRIANGLE
 *
 * @return  0 on success, -1 if @p waveform is outside [0, 3]
 */
EMSCRIPTEN_KEEPALIVE
int audio_set_waveform(int waveform) {
    if (waveform < 0 || waveform > 3) return -1;
    if (g_oscillator) {
        g_oscillator->set_waveform(static_cast<sonicforge::Waveform>(waveform));
    }
    return 0;
}

/**
 * @brief Destroy the oscillator and free its memory.
 *
 * Call this when the AudioWorkletNode is disconnected to avoid Wasm
 * heap leaks (Wasm memory is not garbage-collected by JavaScript).
 */
EMSCRIPTEN_KEEPALIVE
void audio_destroy() {
    delete g_oscillator;
    g_oscillator = nullptr;
}

} // extern "C"

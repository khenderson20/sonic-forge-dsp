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
 *
 * g_viz_buffer is a separate static float array used exclusively by the
 * visualisation path (viz_render_wavetable).  A second Wasm instance on the
 * main thread calls viz_render_wavetable(), then reads back the results via
 * get_viz_buffer() without any JS-side allocation or copy.
 */

#include <sonicforge/delayline.hpp>
#include <sonicforge/oscillator.hpp>
#include <sonicforge/smoothed_value.hpp>
#include <sonicforge/state_variable_filter.hpp>
#include <sonicforge/waveshaper.hpp>

#include <emscripten/emscripten.h>

#include <algorithm>
#include <cmath>

/// Fixed-size audio output buffer (128 = AudioWorklet render quantum).
static float g_buffer[128];

/// Per-harmonic scratch buffer — used inside audio_process to accumulate
/// each oscillator's block before mixing into g_buffer.
static float g_harm_buf[128];

// =============================================================================
// Additive-synthesis audio engine
//
// Up to MAX_HARMONICS oscillators run in parallel.  Harmonic h (1-based) runs
// at frequency = base_freq × h with amplitude gain 1/h.  The mix is
// normalised so the total peak amplitude stays within ±1 regardless of how
// many harmonics are active.
// =============================================================================
static constexpr int MAX_HARMONICS = 16;

/// Pool of oscillator pointers — allocated by audio_init(), freed by
/// audio_destroy().  All MAX_HARMONICS slots are always populated once
/// audio_init() has been called, so audio_process() never needs to branch
/// on whether an individual pointer is null.
static sonicforge::Oscillator* g_oscillators[MAX_HARMONICS] = {};

/// Number of harmonics currently mixed into the output (1 = fundamental only).
static int g_harmonics = 1;

/// Precomputed 1 / sum(1/h, h=1..g_harmonics) — ensures the summed output
/// stays within ±1.  Recalculated whenever g_harmonics changes.
static float g_norm = 1.0F;

/// Cached audio parameters so individual set_* functions can update all
/// oscillator slots without re-reading them from the callers.
static float g_base_freq = 440.0F;
static float g_sr = 48000.0F;
static sonicforge::Waveform g_wf = sonicforge::Waveform::SINE;

/// Recompute the normalisation factor after a harmonics-count change.
static void update_norm() noexcept {
    float sum = 0.0F;
    for (int h = 1; h <= g_harmonics; ++h) {
        sum += 1.0F / static_cast<float>(h);
    }
    g_norm = (sum > 0.0F) ? 1.0F / sum : 1.0F;
}

/// Maximum samples in the visualisation buffer (worst case: 32 slices × 256 pts).
static constexpr int VIZ_MAX_SLICES = 32;
static constexpr int VIZ_MAX_PTS = 256;
static constexpr int VIZ_MAX_SAMPLES = VIZ_MAX_SLICES * VIZ_MAX_PTS;  // 8 192

/// Dedicated visualisation output buffer.  Written by viz_render_wavetable()
/// and read back by JS via a Float32Array view returned by get_viz_buffer().
static float g_viz_buffer[VIZ_MAX_SAMPLES];

// =============================================================================
// FX chain — StateVariableFilter, WaveshaperProcessor, DelayLine
// =============================================================================

/// Max delay buffer: 1 second at 48 kHz (scales with actual sample rate up
/// to this cap — longer delay times are silently clamped by set_delay()).
static constexpr std::size_t DELAY_MAX_SAMPLES = 48000;

// Static instances — constructed once at Wasm startup via _initialize().
static sonicforge::StateVariableFilter g_filter;
static sonicforge::WaveshaperProcessor g_waveshaper;
static sonicforge::DelayLine<sonicforge::DelayInterpolation::Linear> g_delay{DELAY_MAX_SAMPLES};

// SmoothedValue for frequency glide (multiplicative ramp = perceptually linear).
static sonicforge::SmoothedValue<sonicforge::SmoothingMode::Multiplicative> g_freq_smooth;

// Per-module enable flags (all off by default).
static bool g_filter_enabled = false;
static bool g_waveshaper_enabled = false;
static bool g_delay_enabled = false;
static bool g_glide_enabled = false;

// Delay state: time (ms), feedback, and cross-quantum wet tracker.
static float g_delay_time_ms = 250.0F;
static float g_delay_feedback = 0.30F;
static float g_delay_prev_wet = 0.0F;

/// Recompute delay in samples from the current time and sample-rate.
static void s_update_delay_samples() noexcept {
    const float sr = (g_sr > 0.0F) ? g_sr : 48000.0F;
    const float samples = g_delay_time_ms / 1000.0F * sr;
    const float max_s = static_cast<float>(DELAY_MAX_SAMPLES - 1);
    g_delay.set_delay(std::min(samples, max_s));
}

extern "C" {

/**
 * @brief Return the address of the audio output sample buffer.
 *
 * The JS side creates a Float32Array view into the Wasm heap at this pointer
 * so it can read rendered samples without any per-quantum copy.
 *
 * @return Pointer to a 128-element float array.
 */
EMSCRIPTEN_KEEPALIVE
float* get_buffer() {
    return g_buffer;
}

// =============================================================================
// Visualisation exports
// =============================================================================

/**
 * @brief Return the address of the visualisation output buffer.
 *
 * Call get_viz_buffer() once after Wasm initialisation and store the resulting
 * pointer as a Float32Array view.  After each viz_render_wavetable() call the
 * view automatically reflects the new data — no copy required.
 *
 * @return Pointer to a VIZ_MAX_SAMPLES-element float array.
 */
EMSCRIPTEN_KEEPALIVE
float* get_viz_buffer() {
    return g_viz_buffer;
}

/**
 * @brief Return the capacity (in floats) of the visualisation buffer.
 *
 * JS callers can use this to verify that slices × n_pts does not exceed
 * the statically allocated buffer before calling viz_render_wavetable().
 *
 * @return VIZ_MAX_SAMPLES (compile-time constant).
 */
EMSCRIPTEN_KEEPALIVE
int get_viz_buffer_capacity() {
    return VIZ_MAX_SAMPLES;
}

/**
 * @brief Batch-render a wavetable visualisation grid into g_viz_buffer.
 *
 * Produces @p slices × @p n_pts float samples using the same LUT and PolyBLEP
 * algorithms as the audio-thread Oscillator, guaranteeing numerical
 * equivalence between the visualised waveform and the actual audio output.
 *
 * Each slice s (0 … slices-1) represents one copy of the waveform with a
 * linear amplitude decay applied across the slice index:
 *
 *   decay_factor(s) = 1.0 − (s / slices) × decay_pct
 *
 * Harmonics are accumulated additively at 1/h amplitude for h = 1…harmonics,
 * matching the JS additive synthesis that was replaced by this call.
 *
 * Output layout: g_viz_buffer[ s * n_pts + i ] = amplitude of point i in
 * slice s, in the normalised range [−1, +1].
 *
 * @param slices       Number of Z-axis wavetable slices (1 … VIZ_MAX_SLICES).
 * @param n_pts        Number of X-axis points per slice (1 … VIZ_MAX_PTS).
 * @param waveform_idx Waveform index: 0=SINE 1=SAW 2=SQUARE 3=TRIANGLE.
 * @param harmonics    Additive harmonic count (≥ 1; clamped to 1 if lower).
 * @param decay_pct    Decay fraction [0.0, 1.0]: 0 = flat, 1 = full decay.
 *
 * @return  0  on success.
 *         -1  if slices × n_pts exceeds VIZ_MAX_SAMPLES.
 *         -2  if waveform_idx is outside [0, 3].
 */
EMSCRIPTEN_KEEPALIVE
int viz_render_wavetable(int slices, int n_pts, int waveform_idx, int harmonics, float decay_pct) {
    // --- Validate parameters ------------------------------------------------
    if (slices <= 0 || slices > VIZ_MAX_SLICES) {
        return -1;
    }
    if (n_pts <= 0 || n_pts > VIZ_MAX_PTS) {
        return -1;
    }
    // Belt-and-suspenders: also reject any combination that would overflow the
    // flat buffer (e.g. if constants are ever changed independently).
    if (slices * n_pts > VIZ_MAX_SAMPLES) {
        return -1;
    }
    if (waveform_idx < 0 || waveform_idx > 3) {
        return -2;
    }
    if (harmonics < 1) {
        harmonics = 1;
    }

    const auto wf = static_cast<sonicforge::Waveform>(waveform_idx);

    // --- Render each slice --------------------------------------------------
    for (int s = 0; s < slices; ++s) {
        const float decay_factor =
            1.0F - (static_cast<float>(s) / static_cast<float>(slices)) * decay_pct;

        float* slice = g_viz_buffer + (s * n_pts);

        // Zero the slice before accumulating harmonics.
        for (int i = 0; i < n_pts; ++i) {
            slice[i] = 0.0F;
        }

        // Accumulate harmonics: fundamental (h=1) plus overtones.
        for (int h = 1; h <= harmonics; ++h) {
            // dt = phase increment per visualisation step for harmonic h.
            // Equivalent to frequency=h running at sample_rate=n_pts.
            const float dt = static_cast<float>(h) / static_cast<float>(n_pts);
            const float gain = 1.0F / static_cast<float>(h);

            for (int i = 0; i < n_pts; ++i) {
                // Raw phase for harmonic h at point i: wraps every n_pts/h steps.
                // raw ∈ [0, harmonics) so static_cast<int>(raw) is equivalent to
                // std::floor(raw) and avoids the libm call in the inner loop.
                const float raw = static_cast<float>(i) * dt;
                const float phase = raw - static_cast<float>(static_cast<int>(raw));

                slice[i] += sonicforge::Oscillator::sample_at(wf, phase, dt) * gain;
            }
        }

        // Apply the per-slice decay envelope.
        for (int i = 0; i < n_pts; ++i) {
            slice[i] *= decay_factor;
        }
    }

    return 0;
}

// =============================================================================
// Audio oscillator exports — additive synthesis engine
// =============================================================================

/**
 * @brief Initialise (or reinitialise) the additive synthesis engine.
 *
 * Allocates MAX_HARMONICS oscillators, all sharing the same waveform.
 * Harmonic h (1-based) runs at frequency × h.  The number of actively
 * mixed harmonics starts at 1 and can be changed later via
 * audio_set_harmonics() without stopping playback.
 *
 * @param waveform    0=SINE 1=SAW 2=SQUARE 3=TRIANGLE
 * @param frequency   Base frequency in Hz (> 0, finite)
 * @param sample_rate Audio sample rate in Hz (> 0, finite)
 *
 * @return  0 on success
 *         -1 invalid waveform
 *         -2 invalid frequency
 *         -3 invalid sample_rate
 */
EMSCRIPTEN_KEEPALIVE
int audio_init(int waveform, float frequency, float sample_rate) {
    if (waveform < 0 || waveform > 3)
        return -1;
    if (frequency <= 0.0F || !std::isfinite(frequency))
        return -2;
    if (sample_rate <= 0.0F || !std::isfinite(sample_rate))
        return -3;

    g_wf = static_cast<sonicforge::Waveform>(waveform);
    g_base_freq = frequency;
    g_sr = sample_rate;
    g_harmonics = 1;
    update_norm();

    // Allocate all oscillator slots upfront so audio_process() never branches.
    for (int h = 0; h < MAX_HARMONICS; ++h) {
        delete g_oscillators[h];
        const float harm_freq = frequency * static_cast<float>(h + 1);
        g_oscillators[h] = new sonicforge::Oscillator(g_wf, harm_freq, sample_rate);
    }

    // ── Initialise FX chain ──────────────────────────────────────────────────
    g_filter.set_sample_rate(sample_rate);
    g_filter.reset();

    g_delay.reset();
    g_delay_prev_wet = 0.0F;
    s_update_delay_samples();

    g_freq_smooth.reset(frequency, sample_rate);

    return 0;
}

/**
 * @brief Set the number of harmonics mixed into the audio output.
 *
 * The change takes effect on the next audio_process() call.  When new
 * harmonics become active their phases are synchronised to the fundamental
 * so there is no audible click on the transition.
 *
 * @param harmonics Desired count [1, MAX_HARMONICS].
 */
EMSCRIPTEN_KEEPALIVE
void audio_set_harmonics(int harmonics) {
    if (harmonics < 1)
        harmonics = 1;
    if (harmonics > MAX_HARMONICS)
        harmonics = MAX_HARMONICS;

    // Phase-sync any newly activated harmonics to the fundamental to avoid
    // a click.  Both reads/writes happen on the same AudioWorklet thread so
    // there is no data-race with audio_process().
    if (g_oscillators[0] != nullptr) {
        const float fund_phase = g_oscillators[0]->get_phase();
        for (int h = g_harmonics; h < harmonics; ++h) {
            if (g_oscillators[h] != nullptr) {
                // Phase for harmonic (h+1) = (h+1) × fundamental_phase mod 1
                float p = static_cast<float>(h + 1) * fund_phase;
                p -= static_cast<float>(static_cast<int>(p));
                g_oscillators[h]->set_phase(p);
            }
        }
    }

    g_harmonics = harmonics;
    update_norm();
}

/**
 * @brief Generate one render quantum of additive-synthesis samples.
 *
 * Mixes g_harmonics oscillators with gains 1/h, then applies the
 * normalisation factor so the output stays within ±1.
 * Real-time safe: no heap allocation, no blocking.
 *
 * @param num_samples Samples to generate (normally 128).
 */
EMSCRIPTEN_KEEPALIVE
void audio_process(int num_samples) {
    const auto n = static_cast<std::size_t>(num_samples);

    // ── Frequency glide (SmoothedValue) ─────────────────────────────────────
    // Advance the multiplicative smoother through all samples in this quantum
    // and apply the final interpolated frequency to the oscillator pool.
    if (g_glide_enabled && g_freq_smooth.is_smoothing()) {
        float smoothed = g_base_freq;
        for (int i = 0; i < num_samples; ++i) {
            smoothed = g_freq_smooth.process();
        }
        for (int h = 0; h < MAX_HARMONICS; ++h) {
            if (g_oscillators[h] != nullptr) {
                g_oscillators[h]->set_frequency(smoothed * static_cast<float>(h + 1));
            }
        }
    }

    // ── Oscillator synthesis ─────────────────────────────────────────────────
    // Zero accumulator
    for (int i = 0; i < num_samples; ++i) {
        g_buffer[i] = 0.0F;
    }

    // Accumulate each active harmonic
    for (int h = 0; h < g_harmonics; ++h) {
        if (g_oscillators[h] == nullptr)
            continue;
        const float gain = 1.0F / static_cast<float>(h + 1);
        g_oscillators[h]->process_block(g_harm_buf, n);
        for (int i = 0; i < num_samples; ++i) {
            g_buffer[i] += g_harm_buf[i] * gain;
        }
    }

    // Normalise so summed amplitude stays within ±1
    for (int i = 0; i < num_samples; ++i) {
        g_buffer[i] *= g_norm;
    }

    // ── FX chain ─────────────────────────────────────────────────────────────

    // 1. State variable filter
    if (g_filter_enabled) {
        g_filter.process_block(g_buffer, n);
    }

    // 2. Waveshaper
    if (g_waveshaper_enabled) {
        g_waveshaper.process_block(g_buffer, n);
    }

    // 3. Delay (with external feedback loop — dry/wet mix 1:1)
    if (g_delay_enabled) {
        for (int i = 0; i < num_samples; ++i) {
            const float dry = g_buffer[i];
            // Write (dry + feedback × previous wet) → read back the delayed sample.
            const float wet = g_delay.process(dry + g_delay_feedback * g_delay_prev_wet);
            g_delay_prev_wet = wet;
            g_buffer[i] = dry + wet;
        }
    }
}

/**
 * @brief Update the base frequency; all harmonic oscillators are adjusted
 *        atomically via their thread-safe set_frequency() methods.
 *
 * @param frequency New base frequency in Hz.
 */
EMSCRIPTEN_KEEPALIVE
void audio_set_frequency(float frequency) {
    if (!std::isfinite(frequency) || frequency <= 0.0F)
        return;
    g_base_freq = frequency;

    if (g_glide_enabled) {
        // Ramp toward the new target; the smoother feeds audio_process() per quantum.
        g_freq_smooth.set_target(frequency);
    } else {
        // Instant update — also reset the smoother so glide doesn't carry stale state.
        g_freq_smooth.reset(frequency, g_sr);
        for (int h = 0; h < MAX_HARMONICS; ++h) {
            if (g_oscillators[h] != nullptr) {
                g_oscillators[h]->set_frequency(frequency * static_cast<float>(h + 1));
            }
        }
    }
}

/**
 * @brief Change the waveform for all harmonic oscillators.
 *
 * @param waveform 0=SINE 1=SAW 2=SQUARE 3=TRIANGLE
 * @return 0 on success, -1 if waveform is out of range.
 */
EMSCRIPTEN_KEEPALIVE
int audio_set_waveform(int waveform) {
    if (waveform < 0 || waveform > 3)
        return -1;
    g_wf = static_cast<sonicforge::Waveform>(waveform);
    for (int h = 0; h < MAX_HARMONICS; ++h) {
        if (g_oscillators[h] != nullptr) {
            g_oscillators[h]->set_waveform(g_wf);
        }
    }
    return 0;
}

/**
 * @brief Destroy all oscillators and free their memory.
 *
 * Call when the AudioWorkletNode is disconnected.  Wasm heap memory is not
 * garbage-collected by JavaScript so explicit cleanup is required.
 */
EMSCRIPTEN_KEEPALIVE
void audio_destroy() {
    for (int h = 0; h < MAX_HARMONICS; ++h) {
        delete g_oscillators[h];
        g_oscillators[h] = nullptr;
    }
    g_harmonics = 1;
    g_norm = 1.0F;
    g_filter.reset();
    g_delay.reset();
    g_delay_prev_wet = 0.0F;
}

// =============================================================================
// FX chain exports — StateVariableFilter
// =============================================================================

/** Enable or disable the state variable filter in the audio chain. */
EMSCRIPTEN_KEEPALIVE
void audio_set_filter_enabled(int on) {
    g_filter_enabled = (on != 0);
    if (!g_filter_enabled)
        g_filter.reset();
}

/** Set the filter mode: 0=Lowpass 1=Highpass 2=Bandpass 3=Notch. */
EMSCRIPTEN_KEEPALIVE
void audio_set_filter_mode(int mode) {
    if (mode < 0 || mode > 3)
        return;
    g_filter.set_mode(static_cast<sonicforge::FilterMode>(mode));
}

/** Set the filter cutoff frequency in Hz. */
EMSCRIPTEN_KEEPALIVE
void audio_set_filter_cutoff(float hz) {
    g_filter.set_cutoff_hz(hz);
}

/** Set the filter resonance [0.0, 1.0]. */
EMSCRIPTEN_KEEPALIVE
void audio_set_filter_resonance(float q) {
    g_filter.set_resonance(q);
}

// =============================================================================
// FX chain exports — WaveshaperProcessor
// =============================================================================

/** Enable or disable the waveshaper in the audio chain. */
EMSCRIPTEN_KEEPALIVE
void audio_set_waveshaper_enabled(int on) {
    g_waveshaper_enabled = (on != 0);
}

/** Set the waveshaper shape: 0=Tanh 1=Poly 2=HardClip 3=WaveFold. */
EMSCRIPTEN_KEEPALIVE
void audio_set_waveshaper_shape(int shape) {
    if (shape < 0 || shape > 3)
        return;
    g_waveshaper.set_shape(static_cast<sonicforge::WaveshaperShape>(shape));
}

/** Set the waveshaper pre-gain (drive). Must be >= 0. */
EMSCRIPTEN_KEEPALIVE
void audio_set_waveshaper_drive(float drive) {
    g_waveshaper.set_drive(drive);
}

// =============================================================================
// FX chain exports — DelayLine
// =============================================================================

/** Enable or disable the delay line.  Disabling resets the buffer. */
EMSCRIPTEN_KEEPALIVE
void audio_set_delay_enabled(int on) {
    g_delay_enabled = (on != 0);
    if (!g_delay_enabled) {
        g_delay.reset();
        g_delay_prev_wet = 0.0F;
    }
}

/** Set the delay time in milliseconds. Clamped to [0, DELAY_MAX_SAMPLES/sr * 1000]. */
EMSCRIPTEN_KEEPALIVE
void audio_set_delay_time_ms(float ms) {
    if (ms < 0.0F)
        ms = 0.0F;
    g_delay_time_ms = ms;
    s_update_delay_samples();
}

/** Set the delay feedback amount [0.0, 0.95]. */
EMSCRIPTEN_KEEPALIVE
void audio_set_delay_feedback(float fb) {
    g_delay_feedback = std::max(0.0F, std::min(fb, 0.95F));
}

// =============================================================================
// FX chain exports — SmoothedValue (frequency glide)
// =============================================================================

/**
 * Set the frequency glide time in milliseconds.
 * A value of 0 disables glide (instant frequency changes).
 */
EMSCRIPTEN_KEEPALIVE
void audio_set_glide_ms(float ms) {
    if (ms <= 0.0F) {
        g_glide_enabled = false;
        g_freq_smooth.snap_to_target();
    } else {
        g_glide_enabled = true;
        g_freq_smooth.set_ramp_duration(ms / 1000.0F);
    }
}

}  // extern "C"

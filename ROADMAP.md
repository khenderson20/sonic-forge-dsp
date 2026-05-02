# Roadmap

Planned features, quality improvements, and known gaps for SonicForge DSP.  
Items within a milestone are ordered by priority. Milestones are ordered by release sequence.

Design constraints that apply to every item here:
- **Zero heap allocation** in audio-path code (`process` / `process_block`)
- **Lock-free parameter updates** via `std::atomic` for all tuneable parameters
- **No new runtime dependencies** — the installed library must remain dependency-free
- **Consistent API surface** — new modules follow the same construction / parameter / process pattern as existing ones

---

## v0.1.x — Polish and correctness

Small items that improve the developer experience without changing the public API.

- [ ] **Sanitizer CI job** — add a dedicated `sanitize` job running the test suite with `-fsanitize=address,undefined` on Linux/Clang; the `SONICFORGE_ENABLE_SANITIZERS` CMake option already exists, it just needs to be wired to CI
- [ ] **Code coverage reporting** — gcovr/lcov report generated in CI with a Codecov badge in the README; makes gaps in the test suite visible at a glance
- [ ] **Thread-safety example** — demonstrate safe cross-thread `SmoothedValue::set_target()` access alongside a mock audio thread, so integrators don't have to guess at the contract
- [ ] **Advanced usage examples**
  - Filter sweep with resonance modulation (SVF + SmoothedValue)
  - Delay feedback loop with saturation (DelayLine + Waveshaper)
  - Multi-oscillator unison detuning (Oscillator array + SmoothedValue)
- [ ] **Performance benchmarks** — Google Benchmark comparisons: sine LUT vs `std::sin`, all three `DelayLine` interpolation modes, `StateVariableFilter` block processing throughput

---

## v0.2.0 — Core module expansion

New DSP primitives that close the gap between a collection of building blocks and a usable synthesis toolkit.

**ADSR envelope generator**  
The single most impactful missing module. Every voice in a synthesiser needs an envelope.  
- Attack / decay / sustain / release stages with per-stage curve control (linear, exponential, logarithmic)  
- Gate input: `note_on()` / `note_off()` trigger and retrigger semantics  
- Compatible with `SmoothedValue` for click-free transitions at stage boundaries  
- `is_active()` predicate for voice-pool silence detection

**Pulse-width modulation**  
Extend `Oscillator` to support PWM on the `SQUARE` waveform.  
- `set_pulse_width(float pw)` — range 0.01–0.99, atomic update  
- PolyBLEP correction applied at both discontinuities (rise and fall edge)  
- No API change for callers not using PWM

**Wavetable oscillator** (`WavetableOscillator`)  
Companion oscillator that reads from a caller-supplied float table, enabling arbitrary waveforms.  
- Mipmapped band-limited tables to eliminate aliasing across the pitch range  
- Same atomic `set_frequency()` / `set_gain()` interface as `Oscillator`  
- `set_table(const float* data, std::size_t length)` — table stored by pointer, caller owns memory

**Envelope follower** (`EnvelopeFollower`)  
Peak and RMS amplitude detector for dynamics and modulation.  
- Selectable mode: peak or RMS  
- Configurable attack and release times in milliseconds (converted internally to per-sample coefficients)  
- Header-only, stateless except for one running accumulator  
- Useful as a sidechain source for the compressor planned in v0.3.0

**LFO utilities**  
Thin helpers layered on top of `Oscillator` for modulation-rate use.  
- Tempo-sync mode: specify rate in beats and sample rate, LFO locks to a BPM grid  
- One-shot (retriggerable) mode for envelope-style modulation  
- `get_phase()` accessor so multiple LFOs can stay in quadrature

---

## v0.3.0 — Effects

Audio effects that demonstrate how the core modules compose, and that are useful on their own.

**Reverb**  
Algorithmic reverb, zero heap allocation in the audio path.  
- FDN (feedback delay network) topology over Schroeder: better diffusion, less metallic flutter  
- Parameters: room size, damping, pre-delay, wet/dry mix  
- All internal delay buffers allocated at construction; `set_sample_rate()` reallocates (non-realtime call)  
- Mono and stereo variants

**Compressor / limiter**  
Feed-forward RMS compressor suitable for both bus compression and per-voice peak limiting.  
- Parameters: threshold (dBFS), ratio, attack (ms), release (ms), knee width (dB), make-up gain  
- `set_mode(Mode::Compressor | Mode::Limiter)` — limiter is ratio → ∞ with hard knee  
- Sidechain input: optional external key signal (pairs naturally with `EnvelopeFollower`)  
- Gain reduction metering via `get_gain_reduction_db()`

**Chorus / flanger**  
Modulated delay effect built on `DelayLine` and `Oscillator` (LFO mode).  
- Chorus: multiple delay taps (2–8), each with independent LFO phase offset, slight pitch spread  
- Flanger: single short delay (0.5–15 ms) with feedback, all-pass comb filtering  
- Shared `ChorusFlanger` class with `set_mode(Mode::Chorus | Mode::Flanger)` switch  
- Wet/dry mix and stereo spread controls

**Stereo utilities**  
Small header-only helpers for stereo and spatial processing.  
- Mid/side encode (`ms_encode`) and decode (`ms_decode`) free functions  
- Equal-power pan law (`pan_law::equal_power(float position)` → `{left_gain, right_gain}`)  
- `StereoWidth` processor: M/S matrix with width coefficient for narrowing or widening a stereo signal

---

## v0.4.0 — Performance

Explicit SIMD and benchmarking work. No API changes; only observable effect is throughput.

**SIMD block processing**  
- SSE2 and NEON paths for `Oscillator::process_block` and `StateVariableFilter::process_block`  
- CMake feature detection: `SONICFORGE_ENABLE_SIMD` (default `ON`); falls back to scalar if ISA unavailable  
- Runtime dispatch via `cpuid` / `getauxval` for portable binaries  
- Vectorised paths must pass the same test cases as the scalar paths (parametrised test fixtures)

**Benchmarking suite**  
- Google Benchmark integration via CPM.cmake (same pattern as GoogleTest)  
- Baselines: scalar sine generation, SVF block filter, full integration chain  
- CI job that runs benchmarks in `--benchmark_format=json` and archives the result as a workflow artifact for trend tracking

**Profile-guided optimisation example**  
- CMake preset (`cmake --preset pgo`) that does a two-pass PGO build using the example programs as training workload  
- Documents the workflow for users shipping release binaries of plugins or apps built on SonicForge

---

## Longer-term / under consideration

Items with clear value but significant scope, API design questions, or external dependencies that make them premature for a near-term milestone.

- **Node graph helpers** — a thin connection DSL that wires module outputs to inputs without adding overhead to the modules themselves; likely a separate optional header, not part of the core library
- **Polyphony utilities** — lightweight voice pool (fixed-size, no heap) with configurable voice-steal policy (oldest, quietest, lowest); intended as a reference design, not a prescriptive engine
- **Python bindings** — pybind11 wrapper for offline DSP prototyping, test signal generation, and Jupyter notebook exploration; kept in a separate `python/` subdirectory and not installed by the default CMake target
- **CLAP plugin wrapper example** — a minimal CLAP synth voice built from SonicForge modules (Oscillator + ADSR + SVF), demonstrating how to integrate the library into a real plugin; lives in `examples/clap_synth/` and is excluded from the default build
- **Pitch shifter** — granular or phase-vocoder pitch shifting; requires FFT (likely KissFFT or PFFFT to stay dependency-light) so it would ship as an optional `sonicforge_fft` CMake component
- **Browser MIDI input** — extend the WebAssembly demo to accept Web MIDI API events, driving `Oscillator` frequency and the ADSR gate from a connected MIDI controller
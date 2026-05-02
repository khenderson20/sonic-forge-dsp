# Roadmap

This document tracks planned features and known gaps. Items are roughly ordered by priority, not by release milestone — v0.1.x covers bug fixes and polish, v0.2.0 covers the next significant feature wave.

---

## v0.1.x — Polish and correctness

- [ ] Thread-safety example showing safe `set_target()` cross-thread access for `SmoothedValue`
- [ ] Advanced usage examples: filter sweep with resonance modulation, delay feedback loop, multi-oscillator unison detuning
- [ ] Performance benchmarks: sine LUT vs `std::sin`, interpolation mode comparison

---

## v0.2.0 — Core module expansion

**ADSR envelope generator**  
The most significant gap relative to a usable synthesis toolkit. Planned as a standalone module consistent with the zero-allocation, lock-free design of the existing modules.  
- Attack / decay / sustain / release stages with per-stage curve control  
- Compatible with `SmoothedValue` for click-free gate transitions  
- Trigger and retrigger semantics

**Pulse-width modulation**  
Add a `pulse_width` parameter (0–1) to the `SQUARE` waveform in `Oscillator`, implemented with PolyBLEP correction at both edges.

**Wavetable oscillator**  
Optional companion oscillator that reads from a user-supplied float table, with band-limited playback (mipmapped tables) and the same atomic parameter interface as `Oscillator`.

---

## v0.3.0 — Effects

**Reverb**  
Algorithmic reverb (Schroeder / Moorer style or FDN), zero heap allocation in the audio path.

**Compressor / limiter**  
Feed-forward RMS compressor with configurable attack, release, ratio, and knee; suitable for both bus compression and per-voice limiting.

---

## Longer-term / under consideration

- **SIMD block processing** — explicit SSE2/NEON paths for `process_block` in hot modules (`Oscillator`, `StateVariableFilter`)
- **Polyphony utilities** — lightweight voice pool and voice-steal policy, not a full synth engine
- **Node graph helpers** — thin connection DSL on top of the standalone modules, without adding runtime overhead to the modules themselves
- **Python bindings** — pybind11 wrapper for offline DSP prototyping and test signal generation

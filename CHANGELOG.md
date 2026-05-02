# Changelog

All notable changes to SonicForge DSP are documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).  
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Fixed
- `DelayLine<Lagrange3rd>::set_max_delay()` no longer leaves `delay_samples_` past the new buffer boundary — matches the clamp behaviour already present in the `None` and `Linear` specialisations.
- Removed unreachable `return v2` after the exhaustive `switch` in `StateVariableFilter::process()`.

### Changed
- clang-tidy CI now covers `src/state_variable_filter.cpp`, `src/delayline.cpp`, and `tests/new_modules_test.cpp` in addition to the oscillator sources.

### Added
- WebAssembly build (`wasm-build`) CI job — verifies `sonicforge_worklet.cpp` compiles with the latest Emscripten SDK on every push.
- Link-time optimisation (LTO) for Release builds via CMake `INTERPROCEDURAL_OPTIMIZATION_RELEASE`; guarded by `CheckIPOSupported` and controlled with the `SONICFORGE_ENABLE_LTO` option (default `ON`).
- Doxygen API docs are now published to GitHub Pages on every merge to `main`.

---

## [0.1.0] — 2025-01-01

### Added
- `Oscillator` — band-limited sine (4096-entry LUT), saw/square (PolyBLEP anti-aliased), and triangle waveforms; lock-free parameter updates via `std::atomic`.
- `StateVariableFilter` — resonant lowpass/highpass/bandpass/notch using the Cytomic/Zavalishin ZDF topology; four filter modes; lock-free parameter updates.
- `DelayLine` — fractional-sample circular-buffer delay with three interpolation modes: `None`, `Linear`, and 3rd-order `Lagrange3rd`.
- `Waveshaper` — `tanh` saturation, polynomial soft-clip, hard-clip, and Buchla-style wavefolder transfer functions; `WaveshaperProcessor` convenience wrapper.
- `SmoothedValue` — sub-sample parameter ramping in `Linear` and `Multiplicative` (exponential) modes; header-only.
- 61-case GoogleTest suite covering all five modules and a full integration chain (oscillator → SVF → waveshaper → delay).
- WebAssembly AudioWorklet module (`sonicforge_worklet.cpp`) built via Emscripten; Three.js 3D waveform visualisation demo.
- Multi-platform CI: GCC 13, Clang 18, MSVC 2022, Apple Clang; Debug and Release configurations.
- clang-format and clang-tidy enforcement in CI.
- CMake install targets with pkg-config support (`sonicforge.pc`).
- Doxygen documentation (`Doxyfile`).

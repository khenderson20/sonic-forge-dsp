<p align="center">
  <img src="examples/images/sonic-forge-dsp.png" alt="SonicForge DSP" width="720">
</p>

# SonicForge DSP

A lightweight C++ oscillator library with WebAssembly support for browser-based audio and visualization.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![CI](https://github.com/khenderson20/sonic-forge-dsp/actions/workflows/ci.yml/badge.svg)](https://github.com/khenderson20/sonic-forge-dsp/actions/workflows/ci.yml)

---

## Overview

**SonicForge DSP** provides a single, well-tested `Oscillator` class that generates band-limited waveforms (sine, saw, square, triangle) suitable for real-time audio. The processing path performs zero heap allocation and uses `std::atomic` for thread-safe parameter changes.

The library compiles natively on Linux, macOS, and Windows, and can be built to WebAssembly via Emscripten for use in browser AudioWorklet contexts. A companion Three.js visualization renders waveforms in 3D — note that the visualization generates its own waveforms in JavaScript and is not fed from the Wasm DSP output.

### Capabilities

| Feature | Details |
|---------|---------|
| **Waveform generation** | Sine (4096-entry LUT with linear interpolation), saw and square (PolyBLEP anti-aliased), triangle |
| **Block processing** | `process_block(float* buffer, size_t num_samples)` writes into a caller-supplied buffer |
| **Sample-by-sample processing** | `process()` returns one sample at a time for custom buffer layouts |
| **Thread-safe parameter setters** | `set_frequency()`, `set_waveform()`, `set_sample_rate()` use `std::atomic` — safe to call from any thread while processing |
| **Phase control** | `get_phase()`, `set_phase()`, `reset_phase()` — not thread-safe with concurrent `process()` calls |
| **WebAssembly** | Compiles to Wasm via Emscripten with a C bridge (`extern "C"`) for AudioWorklet integration |
| **3D visualization** | Three.js demo page in `web/public/` renders interactive 3D waveforms (JS-generated, independent from Wasm audio output) |

### What this library does NOT provide

- **No filters** — there are no lowpass, highpass, or other filter classes
- **No envelopes** — there is no ADSR or envelope generator
- **No DSP chaining** — there is no `connect()`, node graph, or modular routing API. Each `Oscillator` is standalone
- **No polyphony management** — there is no voice pool or voice stealing. You manage multiple oscillator instances yourself
- **No wavetable oscillator** — the C++ library generates waveforms algorithmically; it does not load or scan wavetables
- **No sub-sample parameter interpolation** — atomic parameter changes take effect at the next sample boundary; there is no ramping or fractional-delay interpolation

### Technical Details

- **No heap allocation in the audio path** — `process()` and `process_block()` perform zero dynamic allocation. The sine LUT is a compile-time `const std::array`
- **Lock-free parameter access** — the three mutable state members (`frequency_`, `sample_rate_`, `waveform_`) are `std::atomic` and accessed with `std::memory_order_relaxed` in the processing loop
- **C++17** — requires a conforming C++17 compiler
- **Dependencies** — the core library has zero runtime dependencies. [GoogleTest](https://github.com/google/googletest) is fetched automatically at configure time via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) and is only required when `SONICFORGE_BUILD_TESTS=ON`
- **Version 0.1.0** — early stage; the public API may change

---

## Prerequisites

### Native Build

| Requirement | Version |
|-------------|---------|
| Compiler | GCC 9+ or Clang 10+ |
| CMake | 3.15+ |
| Build System | Make or Ninja |

> **Note:** GoogleTest is fetched automatically by CPM.cmake during `cmake` configure when `SONICFORGE_BUILD_TESTS=ON`. No manual installation is required.

### WebAssembly Build

| Requirement | Version |
|-------------|---------|
| Emscripten SDK | 3.1.0+ |
| CMake | 3.15+ |
| Browser | WebAssembly + AudioWorklet support |

---

## Installation

### Native Build

```bash
# Clone the repository
git clone https://github.com/khenderson20/sonic-forge-dsp.git
cd sonic-forge-dsp

# Configure and build (GoogleTest is fetched automatically)
cmake -B build -DSONICFORGE_BUILD_TESTS=ON
cmake --build build --parallel

# Run tests
ctest --test-dir build --output-on-failure
```

#### Build with Ninja (Recommended)

```bash
cmake -B build -G Ninja -DSONICFORGE_BUILD_TESTS=ON
cmake --build build
```

#### Build Types

```bash
# Release build (optimized, -O3)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Debug build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

#### Caching CPM Downloads

Set `CPM_SOURCE_CACHE` to avoid re-downloading dependencies on every clean build:

```bash
cmake -B build -DCPM_SOURCE_CACHE=~/.cache/CPM -DSONICFORGE_BUILD_TESTS=ON
```

Or export it as an environment variable:

```bash
export CPM_SOURCE_CACHE=~/.cache/CPM
cmake -B build -DSONICFORGE_BUILD_TESTS=ON
```

### WebAssembly Build

#### Option 1: Via root CMake (recommended)

```bash
cmake -B build -DSONICFORGE_BUILD_WEB=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

#### Option 2: Standalone Emscripten build

```bash
cd web
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
```

Both produce a `.wasm` binary output to `web/public/`. Serve that directory with any static file server to run the 3D visualization.

For detailed web-specific instructions, see [web/README.md](web/README.md).

---

## Usage

### Basic Example

```cpp
#include <sonicforge/oscillator.hpp>

int main() {
    // Create a sine wave oscillator at A4 (440 Hz)
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0F);
    osc.set_sample_rate(48000.0F);

    constexpr size_t BUFFER_SIZE = 256;
    float buffer[BUFFER_SIZE];

    osc.process_block(buffer, BUFFER_SIZE);

    // Thread-safe parameter changes (safe to call from any thread)
    osc.set_frequency(880.0F);
    osc.set_waveform(sonicforge::Waveform::SAW);

    return 0;
}
```

### Hearing the Output

The examples write raw float audio that can be piped directly to a system audio device or saved as a WAV file:

```bash
# Play a 440 Hz sine wave through the system speaker (Linux)
./build/sine_example | aplay -f FLOAT_LE -r 48000 -c 1

# Save a 2-second 440 Hz sine wave as a WAV file
./build/wav_writer_example output.wav 440 2.0
```

### Linking to Your Project

**Using CMake with pkg-config:**

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(SONICFORGE REQUIRED IMPORTED_TARGET sonicforge)

target_link_libraries(your_target PRIVATE PkgConfig::SONICFORGE)
```

**Manual compilation:**

```bash
g++ -std=c++17 your_code.cpp -lsonicforge -o your_app
```

---

## Configuration

### CMake Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `SONICFORGE_BUILD_EXAMPLES` | `ON` | Build example programs |
| `SONICFORGE_BUILD_TESTS` | `ON` | Build unit tests (fetches GoogleTest via CPM) |
| `SONICFORGE_BUILD_WEB` | `OFF` | Build WebAssembly AudioWorklet module |
| `SONICFORGE_OPTIMIZE_FOR_HOST` | `OFF` | Add `-march=native` to Release builds |

---

## Testing

Tests are written with [GoogleTest](https://github.com/google/googletest) and fetched automatically at configure time via CPM.cmake. The test suite covers 19 cases across 6 suites.

| Suite | Cases | What is tested |
|-------|-------|----------------|
| `OscillatorConstruction` | 4 | Default values, custom parameters, invalid waveform/frequency fallback |
| `OscillatorOutputRange` | 4 | All waveforms stay within `[-1, 1]` for one second of output |
| `OscillatorWaveform` | 2 | Sine starts at zero, peak lands at quarter-period |
| `OscillatorParameters` | 4 | `set_frequency`, `set_waveform`, invalid waveform ignored, phase reset |
| `OscillatorProcessing` | 2 | Block path matches sample-by-sample path; phase wraps correctly |
| `OscillatorPolyBLEP` | 2 | Saw and square transitions stay smooth across 100 Hz / 1 kHz / 4.8 kHz |
| `OscillatorSineLUT` | 1 | LUT output is within `1e-4` of `std::sin` (expected peak error ≈ −113 dB) |

### Running Tests

```bash
cmake -B build -DSONICFORGE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Each `TEST()` case registers as a separate CTest entry via `gtest_discover_tests`, giving per-test pass/fail output in CI.

---

## Project Structure

```
sonic-forge-dsp/
├── include/sonicforge/          # Public API header
│   └── oscillator.hpp           # Single class: sonicforge::Oscillator
├── src/                         # Implementation
│   └── oscillator.cpp
├── tests/                       # Unit tests (GoogleTest, 19 cases)
│   └── oscillator_test.cpp
├── examples/                    # Example programs
│   ├── images/
│   │   └── sonic-forge-dsp.png  # Project banner image
│   ├── sine_example.cpp         # Outputs raw floats to stdout (pipeable to aplay)
│   └── wav_writer_example.cpp   # Generates a WAV file with fade-in/fade-out
├── cmake/                       # CMake helpers
│   ├── get_cpm.cmake            # CPM.cmake bootstrap (auto-downloads CPM)
│   └── sonicforge.pc.in         # pkg-config template
├── scripts/                     # Developer tooling
│   ├── install-hooks.sh         # Interactive pre-commit hook installer
│   ├── git-hooks/
│   │   └── pre-commit           # Standalone hook (no extra tools required)
│   └── pre-commit/              # Hook helpers for the pre-commit framework
│       ├── check-format.sh      # clang-format check on staged files
│       ├── check-tidy.sh        # clang-tidy static analysis
│       └── run-tests.sh         # cmake build + ctest
├── web/                         # WebAssembly build + 3D visualization
│   ├── CMakeLists.txt
│   ├── src/sonicforge_worklet.cpp   # C bridge for Wasm
│   └── public/                  # Three.js visualization + AudioWorklet processor
├── .github/workflows/
│   └── ci.yml                   # GitHub Actions CI (3 jobs, Node.js 24)
├── .pre-commit-config.yaml      # pre-commit framework hook configuration
├── CMakeLists.txt               # Root build configuration
├── Doxyfile                     # Documentation generator config
├── .clang-format                # Code style configuration
└── .clang-tidy                  # Static analysis rules
```

---

## Code Quality

SonicForge DSP enforces code quality at three levels: local pre-commit hooks, automated CI, and static analysis.

### clang-format

The project uses clang-format-18 based on LLVM style with the following key settings:

| Setting | Value |
|---------|-------|
| `IndentWidth` | 4 |
| `ColumnLimit` | 100 |
| `IncludeBlocks` | Regroup (project → external → STL) |
| `AllowShortIfStatementsOnASingleLine` | Never |

**Format all C++ files:**

```bash
find include src tests examples web/src \
  -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \
  | xargs clang-format -i
```

**Check formatting without modifying:**

```bash
clang-format --dry-run --Werror src/oscillator.cpp
```

### clang-tidy

Static analysis is configured in `.clang-tidy` with `HeaderFilterRegex: '.*sonicforge.*'` so only project headers produce diagnostics.

| Check group | Catches |
|-------------|---------|
| `bugprone-*` | Incorrect assertions, integer overflow |
| `cppcoreguidelines-*` | Raw pointers, C-style arrays |
| `misc-*` | Missing `const`, include hygiene |
| `modernize-*` | `nullptr`, range-based `for`, `auto` |
| `performance-*` | Unnecessary copies, inefficient algorithms |
| `readability-*` | Naming conventions, braces |
| `clang-analyzer-*` | Null dereferences, memory leaks |

**Generate compilation database and run clang-tidy:**

```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSONICFORGE_BUILD_TESTS=ON
clang-tidy --warnings-as-errors='*' -p build src/oscillator.cpp tests/oscillator_test.cpp
```

---

## Pre-commit Hooks

Pre-commit hooks mirror all three CI jobs so formatting violations, static analysis errors, and test failures are caught locally before push. Two installation modes are available:

### Option 1 — Standalone hook (no extra tools required)

A self-contained bash script is installed as a symlink into `.git/hooks/`:

```bash
bash scripts/install-hooks.sh   # select [1]
```

Selective bypassing via environment variables:

```bash
SONICFORGE_SKIP_FORMAT=1  git commit   # skip clang-format
SONICFORGE_SKIP_TIDY=1    git commit   # skip clang-tidy
SONICFORGE_SKIP_TESTS=1   git commit   # skip build + test
git commit --no-verify                 # bypass all hooks
```

### Option 2 — pre-commit framework

Requires [`pre-commit`](https://pre-commit.com) to be installed. Hook configuration is tracked in `.pre-commit-config.yaml`.

```bash
pip install pre-commit          # once per machine
bash scripts/install-hooks.sh  # select [2]
```

Or manually:

```bash
pre-commit install
```

| Hook ID | Mirrors CI job | Trigger |
|---------|---------------|---------|
| `clang-format` | `clang-format` job | Staged `*.cpp` / `*.hpp` / `*.h` files |
| `clang-tidy` | `clang-tidy` job | Files under `src/`, `tests/`, `include/` staged |
| `cmake-build-test` | `build-and-test` job | Every commit (`always_run: true`) |

**Run all hooks manually without committing:**

```bash
pre-commit run --all-files
```

**Skip a specific hook for a WIP commit:**

```bash
SKIP=cmake-build-test git commit -m "wip: ..."
```

Both options automatically locate or configure a cmake build directory and provide clear, actionable error output.

---

## Continuous Integration

Three jobs run automatically on every push and pull request to `main`/`master` via GitHub Actions (`.github/workflows/ci.yml`). All jobs use Node.js 24 compatible action versions (`actions/checkout@v5`, `actions/cache@v5`).

| Job | Runner | What it checks |
|-----|--------|----------------|
| `build-and-test` | Linux (GCC 13, Clang 18), macOS 14, Windows 2022 | Compiles library + examples + tests; runs `ctest` in Debug and Release |
| `clang-format` | Ubuntu 24.04 + clang-format-18 | All `*.cpp` / `*.hpp` files must be format-clean |
| `clang-tidy` | Ubuntu 24.04 + clang-tidy-18 | `src/oscillator.cpp` and `tests/oscillator_test.cpp` with `--warnings-as-errors='*'` |

### CPM Package Caching

GoogleTest is fetched via CPM.cmake and cached between CI runs using `actions/cache@v5`. The cache key is derived from the hash of all `CMakeLists.txt` and `*.cmake` files, so it is automatically invalidated when dependencies change.

```
Cache key: cpm-<runner.os>-<hash(CMakeLists.txt, *.cmake)>
Cache path: ~/.cache/CPM
```

---

## Documentation

Generate API documentation with Doxygen:

```bash
doxygen Doxyfile
```

Output will be available in `docs/html/index.html`.

---

## Contributing

Contributions are welcome. Please follow these steps:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Install the pre-commit hooks (`bash scripts/install-hooks.sh`)
4. Commit your changes — hooks will validate format, tidy, and tests automatically
5. Push to the branch and open a pull request

### Coding Standards

- Follow modern C++ best practices (C++17, allocation-free audio paths)
- Format all files with `clang-format` before committing
- Ensure `clang-tidy --warnings-as-errors='*'` reports no violations
- Write GoogleTest unit tests for new functionality
- Document public APIs with Doxygen comments (`@brief`, `@param`, `@return`, `@note`)

---

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

> **Note on test dependencies:** GoogleTest is used solely for the test suite and is fetched automatically by CPM.cmake. It is not linked into the installed library and does not affect the MIT licensing of `libsonicforge`.

---

## Acknowledgments

- Inspired by the modular synthesis community
- Built with insights from the Linux audio development ecosystem
- Thanks to all contributors and testers

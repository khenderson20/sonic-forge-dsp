```
  ███████╗ ██████╗ ███╗   ██╗██╗ ██████╗███████╗ ██████╗ ██████╗  ██████╗ ███████╗
  ██╔════╝██╔═══██╗████╗  ██║██║██╔════╝██╔════╝██╔═══██╗██╔══██╗██╔════╝ ██╔════╝
  ███████╗██║   ██║██╔██╗ ██║██║██║     █████╗  ██║   ██║██████╔╝██║  ███╗█████╗
  ═════██║██║   ██║██║╚██╗██║██║██║     ██╔══╝  ██║   ██║██╔══██╗██║   ██║██╔══╝
  ███████║╚██████╔╝██║ ╚████║██║╚██████╗██║     ╚██████╔╝██║  ██║╚██████╔╝███████╗
  ╚══════╝ ╚═════╝ ╚═╝  ╚═══╝╚═╝ ╚═════╝╚═╝      ╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚══════╝
                              ╔═══════════════════╗
                              ║   D  S  P  ♪ ♫   ║
                              ╚═══════════════════╝
```

# SonicForge DSP

A lightweight C++ oscillator library with WebAssembly support for browser-based audio and visualization.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

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

### Technical details

- **No heap allocation in the audio path** — `process()` and `process_block()` perform zero dynamic allocation. The sine LUT is a compile-time `const std::array`
- **Lock-free parameter access** — the three mutable state members (`frequency_`, `sample_rate_`, `waveform_`) are `std::atomic` and accessed with `std::memory_order_relaxed` in the processing loop. This avoids mutexes but provides no ordering guarantees beyond atomicity
- **C++17** — requires a conforming C++17 compiler
- **Version 0.1.0** — early stage; the public API may change

---

## Prerequisites

### Native Build

| Requirement | Version |
|-------------|---------|
| Compiler | GCC 9+ or Clang 10+ |
| CMake | 3.15+ |
| Build System | Make or Ninja |

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
git clone https://codeberg.org/Nanometer7008/sonic-forge-dsp.git
cd sonic-forge-dsp

# Configure and build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run tests
ctest --output-on-failure
```

#### Build with Ninja (Recommended)

```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
```

#### Build Types

```bash
# Release build (optimized, -O3 -march=native)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug build (with AddressSanitizer and UndefinedBehaviorSanitizer)
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

### WebAssembly Build

#### Option 1: Via root CMake (recommended)

```bash
mkdir build && cd build
cmake .. -DSONICFORGE_BUILD_WEB=ON -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
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
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0f);
    osc.set_sample_rate(48000.0f);

    constexpr size_t BUFFER_SIZE = 256;
    float buffer[BUFFER_SIZE];

    osc.process_block(buffer, BUFFER_SIZE);

    osc.set_frequency(880.0f);
    osc.set_waveform(sonicforge::Waveform::SAW);

    return 0;
}
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
| `SONICFORGE_BUILD_TESTS` | `ON` | Build unit tests |
| `SONICFORGE_BUILD_WEB` | `OFF` | Build WebAssembly AudioWorklet module |
| `SONICFORGE_OPTIMIZE_FOR_HOST` | `OFF` | Add `-march=native` to Release builds |

---

## Project Structure

```
sonic-forge-dsp/
├── include/sonicforge/      # Public API header
│   └── oscillator.hpp       # Single class: sonicforge::Oscillator
├── src/                     # Implementation
│   └── oscillator.cpp
├── tests/                   # Unit tests (17 cases, custom test harness)
│   └── oscillator_test.cpp
├── examples/                # Example programs
│   ├── sine_example.cpp     # Outputs raw floats to stdout (pipeable to aplay)
│   └── wav_writer_example.cpp # Generates a WAV file with fade-in/fade-out
├── cmake/                   # CMake helpers & pkg-config
│   └── sonicforge.pc.in
├── web/                     # WebAssembly build + 3D visualization
│   ├── CMakeLists.txt
│   ├── src/sonicforge_worklet.cpp   # C bridge for Wasm
│   └── public/              # Three.js visualization + AudioWorklet processor
├── CMakeLists.txt           # Root build configuration
├── Doxyfile                 # Documentation generator config
├── .clang-format            # Code style configuration
└── .clang-tidy              # Static analysis rules
```

---

## Code Quality

SonicForge DSP uses **clang-format** for automated code formatting and **clang-tidy** for static analysis. Both tools are configured at the repository root and enforced in CI.

### Quick Reference

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

**Generate compilation database:**

```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

**Run clang-tidy:**

```bash
clang-tidy -p build src/oscillator.cpp
clang-tidy --warnings-as-errors='*' -p build src/oscillator.cpp tests/oscillator_test.cpp
```

### Configuration Summary

**clang-format** (based on LLVM style):

| Setting | Value |
|---------|-------|
| IndentWidth | 4 |
| ColumnLimit | 100 |
| ShortIfStatements | Never on single line |
| SortIncludes | CaseSensitive |

**clang-tidy** enabled check groups:

| Group | Catches |
|-------|---------|
| `bugprone-*` | Incorrect assertions, integer overflow |
| `cppcoreguidelines-*` | Raw pointers, C-style arrays |
| `misc-*` | Missing `const`, include hygiene |
| `modernize-*` | `nullptr`, range-based `for`, `auto` |
| `performance-*` | Unnecessary copies, inefficient algorithms |
| `readability-*` | Naming conventions, braces |
| `clang-analyzer-*` | Null dereferences, memory leaks |

### Enforcing in Pre-commit Hooks

**Shell-based hook (`.git/hooks/pre-commit`):**

```bash
cat > .git/hooks/pre-commit << 'EOF'
#!/usr/bin/env bash
set -euo pipefail

STAGED=$(git diff --cached --name-only --diff-filter=ACM \
         | grep -E '\.(cpp|hpp|h)$' || true)

[ -z "$STAGED" ] && exit 0

echo "Running clang-format check..."
for FILE in $STAGED; do
  clang-format --dry-run --Werror "$FILE" || {
    echo "  $FILE is not formatted."
    echo "  Run: clang-format -i $FILE"
    exit 1
  }
done

if [ -f "build/compile_commands.json" ]; then
  echo "Running clang-tidy check..."
  clang-tidy --warnings-as-errors='*' -p build $STAGED || {
    echo "  clang-tidy found violations."
    exit 1
  }
fi
EOF
chmod +x .git/hooks/pre-commit
```

**Using the `pre-commit` framework (`.pre-commit-config.yaml`):**

```yaml
repos:
  - repo: https://github.com/pocc/pre-commit-hooks
    rev: v1.3.5
    hooks:
      - id: clang-format
        args: [--style=file]
      - id: clang-tidy
        args: [-p=build, --warnings-as-errors=*]
```

```bash
pip install pre-commit
pre-commit install
```

### CI Enforcement

Both tools run automatically on every push and pull request via GitHub Actions (`.github/workflows/ci.yml`):

| Job | Runner | What it checks |
|-----|--------|----------------|
| `build-and-test` | Linux, macOS, Windows | Compiles library, examples, tests; runs `ctest` |
| `clang-format` | Ubuntu 24.04 + clang-format-18 | All `*.cpp` / `*.hpp` files must be format-clean |
| `clang-tidy` | Ubuntu 24.04 + clang-tidy-18 | Core library and tests with `--warnings-as-errors='*'` |

---

## Documentation

Generate API documentation with Doxygen:

```bash
cd docs && doxygen ../Doxyfile
```

Output will be available in `docs/html/`.

---

## Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Commit your changes (`git commit -m 'Add your feature'`)
4. Push to the branch (`git push origin feature/your-feature`)
5. Open a pull request

### Coding Standards

- Follow modern C++ best practices (C++17, allocation-free audio paths)
- Format all files with `clang-format` before committing
- Ensure `clang-tidy --warnings-as-errors='*'` reports no violations
- Write unit tests for new functionality
- Document public APIs with Doxygen comments (`@brief`, `@param`, `@return`, `@note`)

---

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

---

## Acknowledgments

- Inspired by the modular synthesis community
- Built with insights from the Linux audio development ecosystem
- Thanks to all contributors and testers

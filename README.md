```
  ███████╗ ██████╗ ███╗   ██╗██╗ ██████╗███████╗ ██████╗ ██████╗  ██████╗ ███████╗
  ██╔════╝██╔═══██╗████╗  ██║██║██╔════╝██╔════╝██╔═══██╗██╔══██╗██╔════╝ ██╔════╝
  ███████╗██║   ██║██╔██╗ ██║██║██║     █████╗  ██║   ██║██████╔╝██║  ███╗█████╗
  ╚════██║██║   ██║██║╚██╗██║██║██║     ██╔══╝  ██║   ██║██╔══██╗██║   ██║██╔══╝
  ███████║╚██████╔╝██║ ╚████║██║╚██████╗██║     ╚██████╔╝██║  ██║╚██████╔╝███████╗
  ╚══════╝ ╚═════╝ ╚═╝  ╚═══╝╚═╝ ╚═════╝╚═╝      ╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚══════╝
                              ╔═══════════════════╗
                              ║   D  S  P  ♪ ♫   ║
                              ╚═══════════════════╝
```

# 🎛️ SonicForge DSP

A high-performance C++ DSP library that compiles natively **and** to WebAssembly for browser-based 3D audio visualization.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

---

## 📖 Overview

**SonicForge DSP** is a lightweight C++ framework focused on low-latency signal generation and processing. It serves as a showcase of modern systems programming practices, specifically addressing the unique constraints of real-time audio—where heap allocation and thread-blocking are strictly avoided to ensure signal stability.

The library extends beyond native audio pipelines with full WebAssembly support, enabling real-time DSP chains to run inside browser AudioWorklet contexts paired with Three.js-powered 3D wavetable visualization.

### ✨ Key Features

| Category | Details |
|----------|---------|
| **🎵 Waveform Generation** | Sine, Saw, Square, Triangle oscillators |
| **⚡ Real-time Processing** | Sub-sample parameter modulation |
| **🔗 Modular Architecture** | Chain oscillators, filters, envelopes |
| **📦 Zero-Copy Design** | Efficient buffer processing |
| **🔒 Thread-Safe API** | Safe parameter changes from any thread |
| **🌐 WebAssembly AudioWorklet** | Run DSP chains directly in the browser |
| **🎨 3D Visualization** | Interactive browser-based wavetable rendering via Three.js |

### 💡 Technical Highlights

- **🔧 Real-time Safe Architecture:** Lock-free and allocation-avoidant patterns prevent priority inversion and audio glitches.
- **💻 Modern C++ Standard:** Leverages C++17/20 features including smart pointers, `std::atomic`, and templates.
- **🖥️ Cross-Platform:** Builds natively on Linux, macOS, and Windows; compiles to WebAssembly via Emscripten.
- **🌍 Browser-Native Audio:** AudioWorklet integration for sample-accurate processing without main-thread interference.

---

## 📋 Prerequisites

### 🔨 Native Build

| Requirement | Version |
|-------------|---------|
| Compiler | GCC 9+ or Clang 10+ |
| CMake | 3.15+ |
| Build System | Make or Ninja |

### 🌐 WebAssembly Build

| Requirement | Version |
|-------------|---------|
| Emscripten SDK | 3.1.0+ |
| CMake | 3.15+ |
| Browser | WebAssembly + AudioWorklet support |

---

## 📦 Installation

### 🔨 Native Build

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

#### 🚀 Build with Ninja (Recommended)

```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
```

#### 🔧 Build Types

```bash
# Release build (optimized, -O3 -march=native)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug build (with AddressSanitizer and UndefinedBehaviorSanitizer)
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

### 🌐 WebAssembly Build

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

## 🚀 Usage

### 📝 Basic Example

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

### 🔗 Linking to Your Project

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

## ⚙️ Configuration

### 🔧 CMake Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `SONICFORGE_BUILD_EXAMPLES` | `ON` | Build example programs |
| `SONICFORGE_BUILD_TESTS` | `ON` | Build unit tests |
| `SONICFORGE_BUILD_WEB` | `OFF` | Build WebAssembly AudioWorklet module |
| `SONICFORGE_OPTIMIZE_FOR_HOST` | `OFF` | Add `-march=native` to Release builds |

---

## 📊 Performance Benchmarks

All benchmarks run on **AMD Ryzen 7 5800X**, Fedora 40, 48kHz sample rate:

| Metric | Value | Notes |
|--------|-------|-------|
| **⏱️ Latency** | < 1.5ms | 64-sample buffer |
| **🔋 Oscillator CPU** | ~0.02% | Per-voice sine wave |
| **🎼 Polyphony** | 64+ voices | < 3% CPU total |
| **🎛️ Parameter Modulation** | Sub-sample | Lock-free atomic updates |
| **🌐 Wasm Overhead** | < 5% | vs. native, AudioWorklet |

---

## 🎯 What You Can Build

- **🎹 Modular Synthesizers:** Chain oscillators, filters, and envelopes
- **🎚️ Audio Effects:** Process live input with custom DSP chains
- **🎮 Real-time Generators:** Procedural audio for games and interactive media
- **🔌 Audio Plugins:** VST/LV2 plugins (with additional wrapper code)
- **🌐 Browser-Based Audio Tools:** Deploy DSP chains to the web with AudioWorklet
- **🎨 3D Audio Visualizations:** Render wavetables and spectrograms with Three.js

---

## 📁 Project Structure

```
sonic-forge-dsp/
├── include/sonicforge/      # Public API headers
│   └── oscillator.hpp
├── src/                     # Implementation
│   └── oscillator.cpp
├── tests/                   # Unit tests
│   └── oscillator_test.cpp
├── examples/                # Example programs
│   ├── sine_example.cpp
│   └── wav_writer_example.cpp
├── cmake/                   # CMake helpers & pkg-config
│   └── sonicforge.pc.in
├── web/                     # WebAssembly build + 3D visualization
│   ├── CMakeLists.txt
│   ├── src/sonicforge_worklet.cpp
│   └── public/              # Three.js visualization
├── CMakeLists.txt           # Native build configuration
├── Doxyfile                 # Documentation generator config
├── .clang-format            # Code style configuration
└── .clang-tidy              # Static analysis rules
```

---

## ✅ Code Quality

SonicForge DSP uses **clang-format** for automated code formatting and **clang-tidy** for static analysis. Both tools are configured at the repository root and enforced in CI.

### 📌 Quick Reference

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

### 📐 Configuration Summary

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

### 🪝 Enforcing in Pre-commit Hooks

**Shell-based hook (`.git/hooks/pre-commit`):**

```bash
cat > .git/hooks/pre-commit << 'EOF'
#!/usr/bin/env bash
set -euo pipefail

STAGED=$(git diff --cached --name-only --diff-filter=ACM \
         | grep -E '\.(cpp|hpp|h)$' || true)

[ -z "$STAGED" ] && exit 0

echo "🔎  Running clang-format check..."
for FILE in $STAGED; do
  clang-format --dry-run --Werror "$FILE" || {
    echo "  ❌  $FILE is not formatted."
    echo "  Run: clang-format -i $FILE"
    exit 1
  }
done

if [ -f "build/compile_commands.json" ]; then
  echo "🔎  Running clang-tidy check..."
  clang-tidy --warnings-as-errors='*' -p build $STAGED || {
    echo "  ❌  clang-tidy found violations."
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

### 🤖 CI Enforcement

Both tools run automatically on every push and pull request via GitHub Actions (`.github/workflows/ci.yml`):

| Job | Runner | What it checks |
|-----|--------|----------------|
| `build-and-test` | Linux, macOS, Windows | Compiles library, examples, tests; runs `ctest` |
| `clang-format` | Ubuntu 24.04 + clang-format-18 | All `*.cpp` / `*.hpp` files must be format-clean |
| `clang-tidy` | Ubuntu 24.04 + clang-tidy-18 | Core library and tests with `--warnings-as-errors='*'` |

---

## 📚 Documentation

Generate API documentation with Doxygen:

```bash
cd docs && doxygen ../Doxyfile
```

Output will be available in `docs/html/`.

---

## 🤝 Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Commit your changes (`git commit -m 'Add your feature'`)
4. Push to the branch (`git push origin feature/your-feature`)
5. Open a pull request

### 📏 Coding Standards

- Follow modern C++ best practices (C++17, allocation-free audio paths)
- Format all files with `clang-format` before committing
- Ensure `clang-tidy --warnings-as-errors='*'` reports no violations
- Write unit tests for new functionality
- Document public APIs with Doxygen comments (`@brief`, `@param`, `@return`, `@note`)

---

## 🆕 What's New

### 📢 Recent Changes

- **🌐 WebAssembly DSP Bridge:** Full Emscripten build pipeline for browser environments
- **🎧 AudioWorklet Processor:** `sonicforge_worklet.cpp` provides a sample-accurate audio processing node
- **🎨 3D Wavetable Visualization:** Three.js-powered real-time renderer in `web/public/`
- **🔧 Web CMake Toolchain:** Dedicated `web/CMakeLists.txt` for streamlined Wasm builds

---

## ⚖️ License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

---

## 💖 Acknowledgments

- Inspired by the modular synthesis community
- Built with insights from the Linux audio development ecosystem
- Thanks to all contributors and testers

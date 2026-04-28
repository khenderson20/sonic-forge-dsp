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
![dsp-image](./examples/images/sonic-forge-dsp.png)
# SonicForge DSP

A high-performance C++ DSP library that compiles natively **and** to WebAssembly for browser-based 3D audio visualization.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## ✨ Features

- **Waveform Generation**: Sine, Saw, Square, Triangle oscillators
- **Real-time Processing**: Sub-sample parameter modulation
- **Modular Architecture**: Chain oscillators, filters, envelopes
- **Zero-Copy Design**: Efficient buffer processing
- **Thread-Safe API**: Safe parameter changes from any thread
- **WebAssembly AudioWorklet Integration**: Run DSP chains directly in the browser
- **Real-time 3D Wavetable Visualization (Three.js)**: Interactive browser-based audio rendering

## 📁 Directory Structure

```
sonic-forge-dsp/
│
├── 🎛️  CMakeLists.txt          # Native build configuration
├── 📄  LICENSE                  # MIT License
├── 📖  README.md                # You are here!
├── 🔧  Doxyfile                 # Documentation generator config
│
├── include/sonicforge/         # ┌─────────────────────────────┐
│   └── oscillator.hpp          # │  PUBLIC API HEADERS         │
│                               # │  Include these in your code │
│                               # └─────────────────────────────┘
├── src/                        # ┌─────────────────────────────┐
│   └── oscillator.cpp          # │  IMPLEMENTATION             │
│                               # │  DSP algorithms live here   │
│                               # └─────────────────────────────┘
├── tests/                      # ┌─────────────────────────────┐
│   └── oscillator_test.cpp     # │  UNIT TESTS                 │
│                               # │  Mathematical verification  │
│                               # └─────────────────────────────┘
├── examples/                   # ┌─────────────────────────────┐
│   ├── sine_example.cpp        # │  EXAMPLES                   │
│   └── wav_writer_example.cpp  # │  Learn by running these!    │
│                               # └─────────────────────────────┘
├── cmake/                      # CMake helpers & pkg-config
│   └── sonicforge.pc.in        #
│
├── web/                        # ┌─────────────────────────────┐
│   ├── CMakeLists.txt          # │  WebAssembly build config   │
│   ├── src/                    # │                             │
│   │   └── sonicforge_worklet.cpp # AudioWorklet processor     │
│   └── public/                 # │  Three.js 3D visualization  │
│                               # └─────────────────────────────┘
│
├── .clang-format               # Code style configuration
├── .clang-tidy                 # Static analysis rules
└── .gitignore                  # Git ignore patterns
```

## 🚀 Overview

**SonicForge DSP** is a lightweight C++ framework focused on low-latency signal generation and processing. This project serves as a showcase of modern systems programming practices, specifically addressing the unique constraints of real-time audio—where heap allocation and thread-blocking are strictly avoided to ensure signal stability.

The library now extends beyond native audio pipelines with full WebAssembly support, enabling real-time DSP chains to run inside browser AudioWorklet contexts paired with Three.js-powered 3D wavetable visualization.

## 🛠️ Technical Highlights

* **Real-time Safe Architecture:** Implementation follows "lock-free" and "allocation-avoidant" patterns in the audio callback to prevent priority inversion and audio glitches.
* **Modern C++ Standard:** Leverages C++17/20 features including smart pointers for memory management, `std::atomic` for thread-safe parameter modulation, and templates for efficient buffer processing.
* **Cross-Platform Compilation:** Builds natively on Linux and compiles to WebAssembly via Emscripten for in-browser execution.
* **Browser-Native Audio:** AudioWorklet integration for sample-accurate processing in modern browsers without main-thread interference.

## 📑 Prerequisites

### Native Build

* C++17/20 compatible compiler (GCC 9+ or Clang 10+)
* CMake 3.15+
* Make or Ninja build system

### WebAssembly Build

* Emscripten SDK (emsdk 3.1.0+)
* CMake 3.15+
* A modern browser with WebAssembly and AudioWorklet support

## 🔨 Building

### Native Build

```bash
# Clone the repository
git clone https://codeberg.org/Nanometer7008/sonic-forge-dsp.git
cd sonic-forge-dsp

# Configure and Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run Tests
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

#### CMake Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `SONICFORGE_BUILD_EXAMPLES` | `ON` | Build example programs |
| `SONICFORGE_BUILD_TESTS` | `ON` | Build unit tests |
| `SONICFORGE_BUILD_WEB` | `OFF` | Build WebAssembly AudioWorklet module |
| `SONICFORGE_OPTIMIZE_FOR_HOST` | `OFF` | Add `-march=native` to Release builds (faster locally, not portable) |

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

Both produce a `.wasm` binary output to `web/public/`. Serve that directory with any static file server to run the 3D visualization in your browser.

## 🎵 Usage

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

Using CMake:

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(SONICFORGE REQUIRED IMPORTED_TARGET sonicforge)

target_link_libraries(your_target PRIVATE PkgConfig::SONICFORGE)
```

Or manually:

```bash
g++ -std=c++17 your_code.cpp -lsonicforge -o your_app
```

## 📊 Performance Benchmarks

All benchmarks run on **AMD Ryzen 7 5800X**, Fedora 40, 48kHz sample rate:

| Metric | Value | Notes |
|--------|-------|-------|
| **Latency** | < 1.5ms | 64-sample buffer |
| **Oscillator CPU** | ~0.02% | Per-voice sine wave |
| **Polyphony** | 64+ voices | < 3% CPU total |
| **Parameter Modulation** | Sub-sample | Lock-free atomic updates |
| **Wasm Overhead** | < 5% | vs. native, AudioWorklet |

## 🎯 What You Can Build

- **Modular Synthesizers**: Chain oscillators, filters, and envelopes for complex sound design
- **Audio Effects**: Process live input with custom DSP chains
- **Real-time Generators**: Create procedural audio for games and interactive media
- **Audio Plugins**: Build VST/LV2 plugins (with additional wrapper code)
- **Browser-Based Audio Tools**: Deploy DSP chains to the web with AudioWorklet
- **3D Audio Visualizations**: Render wavetables and spectrograms with Three.js

---

## 🔍 Code Quality

SonicForge DSP uses **clang-format** for automated code formatting and **clang-tidy** for static analysis. Both tools are configured at the repository root (`.clang-format`, `.clang-tidy`) and are enforced in CI on every push and pull request. This section explains how to install, configure, and run each tool locally so that your changes pass CI before you push them.

---

### clang-format

`clang-format` is a source code formatter that rewrites C++ files to match a defined style. Running it before committing eliminates style-related review comments entirely.

#### Installing clang-format

Install the same major version used in CI (LLVM 18) to guarantee identical output:

**Linux (Debian / Ubuntu)**

```bash
# Add the LLVM APT repository for Ubuntu 24.04 (Noble)
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key \
  | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
sudo add-apt-repository \
  "deb http://apt.llvm.org/noble/ llvm-toolchain-noble-18 main"
sudo apt-get update && sudo apt-get install -y clang-format-18

# Verify
clang-format-18 --version
```

**Linux (Fedora / RHEL)**

```bash
sudo dnf install clang-tools-extra   # ships clang-format from the active LLVM version
```

**macOS (Homebrew)**

```bash
brew install llvm@18
# clang-format is in the keg — add it to your PATH or use the full path:
export PATH="$(brew --prefix llvm@18)/bin:$PATH"
clang-format --version
```

**Windows**

Install [Visual Studio 2022](https://visualstudio.microsoft.com/) (ships clang-format in the LLVM component) or use [winget](https://learn.microsoft.com/en-us/windows/package-manager/winget/):

```powershell
winget install LLVM.LLVM
```

---

#### Project Configuration (`.clang-format`)

The project's `.clang-format` file at the repository root defines the house style. Key settings:

| Setting | Value | Effect |
|---------|-------|--------|
| `BasedOnStyle` | `LLVM` | Starts from the LLVM style baseline |
| `IndentWidth` | `4` | 4-space indentation |
| `ColumnLimit` | `100` | Max line length before wrapping |
| `AllowShortIfStatementsOnASingleLine` | `Never` | `if` bodies always on their own line |
| `SortIncludes` | `CaseSensitive` | `#include` directives sorted alphabetically |
| `SpacesBeforeTrailingComments` | `2` | Two spaces before `//` inline comments |

You can inspect the full configuration at any time:

```bash
cat .clang-format
```

---

#### Formatting Code

**Format a single file in-place:**

```bash
clang-format -i src/oscillator.cpp
```

**Format every C++ file in the project (excluding build directories):**

```bash
find include src tests examples web/src \
  -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \
  | xargs clang-format -i
```

**Format only files that differ from the last commit (fast, useful pre-push):**

```bash
git diff --name-only HEAD | grep -E '\.(cpp|hpp|h)$' | xargs clang-format -i
```

---

#### Checking Without Modifying (Dry Run)

Use `--dry-run --Werror` to check whether a file needs formatting without changing it. This is exactly how the CI job works:

```bash
# Check a single file — exits 1 if any change would be made
clang-format --dry-run --Werror src/oscillator.cpp

# Check all project sources at once
find include src tests examples web/src \
  -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \
  | xargs clang-format --dry-run --Werror
```

A clean exit (code 0) means the file is already correctly formatted.

---

### clang-tidy

`clang-tidy` is a static analysis tool that catches bugs, modernisation opportunities, and style violations not covered by the compiler. Unlike the compiler, it needs to know how each file is compiled (flags, include paths, macros) — this is supplied via a **compilation database** (`compile_commands.json`) generated by CMake.

#### Installing clang-tidy

Install the same major version as clang-format (LLVM 18):

**Linux (Debian / Ubuntu)**

```bash
sudo apt-get install -y clang-tidy-18
clang-tidy-18 --version
```

**Linux (Fedora / RHEL)**

```bash
sudo dnf install clang-tools-extra
```

**macOS (Homebrew)**

```bash
brew install llvm@18
export PATH="$(brew --prefix llvm@18)/bin:$PATH"
clang-tidy --version
```

**Windows**

clang-tidy ships with the LLVM installer:

```powershell
winget install LLVM.LLVM
```

---

#### Project Configuration (`.clang-tidy`)

The `.clang-tidy` file at the repository root controls which checks run and how. It is automatically picked up by `clang-tidy` whenever the tool is invoked from anywhere inside the repository tree.

**Enabled check groups:**

| Group | Examples of what it catches |
|-------|-----------------------------|
| `bugprone-*` | Incorrect assertions, integer overflow, string constructor misuse |
| `cppcoreguidelines-*` | Raw owning pointers, C-style arrays, non-const globals |
| `misc-*` | Missing `const`, internal-linkage violations, include hygiene |
| `modernize-*` | Suggests `nullptr`, range-based `for`, `auto`, `std::array` |
| `performance-*` | Unnecessary copies, inefficient algorithm usage |
| `readability-*` | Naming conventions, braces, boolean simplification |
| `clang-analyzer-*` | Deep path-sensitive analysis (null dereferences, memory leaks) |

**Naming conventions enforced:**

| Identifier kind | Convention | Example |
|----------------|------------|---------|
| Classes / structs / enums | `CamelCase` | `Oscillator`, `Waveform` |
| Functions / methods / variables | `lower_case` | `process_block`, `sample_rate` |
| Private / protected members | `lower_case_` (trailing `_`) | `phase_`, `frequency_` |
| Global / static constants | `UPPER_CASE` | `TWO_PI`, `SINE_LUT_SIZE` |
| Local constants | `lower_case` | `const float dt`, `const auto idx` |
| Enum constants | `UPPER_CASE` | `Waveform::SINE`, `Waveform::SAW` |

You can inspect or adjust rules directly:

```bash
cat .clang-tidy
```

---

#### Generating a Compilation Database

`clang-tidy` requires `compile_commands.json`. CMake generates it automatically when you pass `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`:

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSONICFORGE_BUILD_TESTS=ON \
  -DSONICFORGE_BUILD_EXAMPLES=ON
```

The file is written to `build/compile_commands.json`. Many editors (VS Code with the clangd extension, CLion, Neovim + clangd) also use this file for real-time diagnostics.

> **Tip:** Re-run the CMake configure step whenever you add or rename source files to keep the compilation database up to date.

---

#### Running clang-tidy

**Analyse a single file** (using the project's `.clang-tidy` automatically):

```bash
clang-tidy -p build src/oscillator.cpp
```

**Analyse the core library and test suite** (mirrors the CI job):

```bash
clang-tidy -p build \
  src/oscillator.cpp \
  tests/oscillator_test.cpp
```

**Auto-fix safe, mechanical suggestions** (e.g. add `override`, replace `NULL` with `nullptr`):

```bash
clang-tidy -p build --fix src/oscillator.cpp
```

> **Warning:** Always review `--fix` changes with `git diff` before committing; not every automated fix is correct in context.

**Treat all warnings as errors** (identical to CI behaviour):

```bash
clang-tidy --warnings-as-errors='*' -p build \
  src/oscillator.cpp \
  tests/oscillator_test.cpp
```

**Suppress a single intentional violation inline** (use sparingly):

```cpp
// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
const auto raw = static_cast<sonicforge::Waveform>(42U);
```

---

### Enforcing in Pre-commit Hooks

A Git pre-commit hook runs automatically before every commit and rejects it if the checks fail, preventing unformatted or statically-invalid code from ever entering the history.

#### Shell-based Hook (no extra tooling required)

Create `.git/hooks/pre-commit` and make it executable:

```bash
cat > .git/hooks/pre-commit << 'EOF'
#!/usr/bin/env bash
set -euo pipefail

# ── Collect staged C++ files ──────────────────────────────────────────────
STAGED=$(git diff --cached --name-only --diff-filter=ACM \
         | grep -E '\.(cpp|hpp|h)$' || true)

if [ -z "$STAGED" ]; then
  exit 0
fi

echo "🔎  Running clang-format check..."
for FILE in $STAGED; do
  clang-format --dry-run --Werror "$FILE" || {
    echo ""
    echo "  ❌  $FILE is not formatted."
    echo "  Run: clang-format -i $FILE"
    echo ""
    exit 1
  }
done
echo "  ✅  clang-format: all staged files are correctly formatted."

# ── clang-tidy (only if compile_commands.json exists) ────────────────────
if [ -f "build/compile_commands.json" ]; then
  echo ""
  echo "🔎  Running clang-tidy check..."
  clang-tidy --warnings-as-errors='*' -p build $STAGED || {
    echo ""
    echo "  ❌  clang-tidy found violations in staged files."
    echo "  Fix the issues above, then re-stage and commit."
    echo ""
    exit 1
  }
  echo "  ✅  clang-tidy: no violations found."
fi
EOF

chmod +x .git/hooks/pre-commit
```

The hook only inspects files that are **staged for this commit**, keeping it fast even in large repositories.

---

#### Using the `pre-commit` Framework

If you prefer the [pre-commit](https://pre-commit.com/) framework, add a `.pre-commit-config.yaml` at the repository root:

```yaml
repos:
  - repo: https://github.com/pocc/pre-commit-hooks
    rev: v1.3.5
    hooks:
      - id: clang-format
        args: [--style=file]   # reads .clang-format from the repo root
      - id: clang-tidy
        args: [-p=build, --warnings-as-errors=*]
```

Install and activate:

```bash
pip install pre-commit
pre-commit install        # registers the hook in .git/hooks/pre-commit
pre-commit run --all-files  # run once on the entire codebase
```

---

### CI Enforcement

Both tools run automatically on every push and pull request via **GitHub Actions** (`.github/workflows/ci.yml`). The workflow contains three jobs:

| Job | Runner | What it checks |
|-----|--------|----------------|
| `build-and-test` | Linux (GCC 13, Clang 18), macOS (AppleClang), Windows (MSVC 2022) | Compiles the library, examples, and tests; runs `ctest` |
| `clang-format` | Ubuntu 24.04 + clang-format-18 | All `*.cpp` / `*.hpp` files in `include/`, `src/`, `tests/`, `examples/`, `web/src/` must be format-clean; fails the PR otherwise |
| `clang-tidy` | Ubuntu 24.04 + clang-tidy-18 | Analyses `src/oscillator.cpp` and `tests/oscillator_test.cpp` with `--warnings-as-errors='*'`; every diagnostic is a hard failure |

**Recommended local workflow before pushing:**

```bash
# 1. Format all C++ sources
find include src tests examples web/src \
  -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \
  | xargs clang-format -i

# 2. Regenerate the compilation database if needed
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSONICFORGE_BUILD_TESTS=ON -DSONICFORGE_BUILD_EXAMPLES=ON

# 3. Run clang-tidy with the same flags CI uses
clang-tidy --warnings-as-errors='*' -p build \
  src/oscillator.cpp tests/oscillator_test.cpp

# 4. Build and run tests
cmake --build build --parallel
cd build && ctest --output-on-failure
```

If all four steps pass locally, the corresponding CI jobs will pass too.

---

## 📚 Documentation

API documentation is generated using Doxygen:

```bash
cd docs && doxygen ../Doxyfile
```

Documentation will be available in the `docs/html/` directory.

## 📝 What's New

### Recent Changes

- **WebAssembly DSP Bridge**: Full Emscripten build pipeline enabling the core DSP library to run in browser environments
- **AudioWorklet Processor**: `sonicforge_worklet.cpp` provides a sample-accurate audio processing node for the Web Audio API
- **3D Wavetable Visualization**: Three.js-powered real-time renderer in `web/public/` for interactive audio visualization
- **Web CMake Toolchain**: Dedicated `web/CMakeLists.txt` for streamlined Wasm builds with proper export configuration

## 👥 Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a pull request

### Coding Standards

* Follow modern C++ best practices (C++17, allocation-free audio paths)
* Format all changed files with `clang-format` before committing — see [🔍 Code Quality](#-code-quality) for setup instructions
* Ensure `clang-tidy --warnings-as-errors='*'` reports no violations on `src/` and `tests/`
* Write unit tests for new functionality; aim to keep all 19 existing tests green
* Document public APIs with Doxygen comments (`@brief`, `@param`, `@return`, `@note`)

## ⚖️ License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## ❤️ Acknowledgments

* Inspired by the modular synthesis community
* Built with insights from the Linux audio development ecosystem
* Thanks to all contributors and testers

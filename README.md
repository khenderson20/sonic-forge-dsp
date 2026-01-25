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

# SonicForge DSP

A high-performance, real-time C++ Digital Signal Processing (DSP) library designed for modular synthesis and systems programming research.


[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## 🚀 Overview

**SonicForge DSP** is a lightweight C++ framework focused on low-latency signal generation and processing. This project serves as a showcase of modern systems programming practices, specifically addressing the unique constraints of real-time audio—where heap allocation and thread-blocking are strictly avoided to ensure signal stability.

## 🛠️ Technical Highlights

* **Real-time Safe Architecture:** Implementation follows "lock-free" and "allocation-avoidant" patterns in the audio callback to prevent priority inversion and audio glitches.
* **Modern C++ Standard:** Leverages C++17/20 features including smart pointers for memory management, `std::atomic` for thread-safe parameter modulation, and templates for efficient buffer processing.
* **Linux-First Development:** Optimized for Fedora environments using modern tooling including `cmake`, `clang-format`, and `gcc`.
* **Automated Quality Assurance:** Integrated **Woodpecker CI** pipeline for automated builds, linting, and unit testing on every push.

## 📂 Project Structure

```
sonic-forge-dsp/
│
├── 🎛️  CMakeLists.txt          # Build configuration
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
├── .clang-format               # Code style configuration
├── .clang-tidy                 # Static analysis rules
└── .gitignore                  # Git ignore patterns
```

### Signal Flow Architecture

```
                                                                                                                  
                                                                                                                   
                                                                                                                   
+--------------------------------------------------------------------------------------------------+               
|                                                                                                  |               
|                                                                                                  |               
|                                    SONIC-FORGE-DSP                                               |               
|                                                                                                  |               
----------------------------------------------------------------------------------------------------               
|         +------------+             +-------------+                +-----------------+            |               
|         |            |             |             |                |                 |            |               
|         | oscillator |             |   filter    |                |    envelope     |            |               
|         |            |             |             |                |                 |            |               
|         |            |             |             |                |                 |            |               
|         | - sine     |             |             |                |                 |            |               
|         | - saw      |-----------> |             |--------------> |                 |            |               
|         | - square   |             |             |                |                 |            |               
|         | - triangle |             |             |                |                 |            |               
|         |            |             |             |                |                 |            |               
|         |            |             |             |                |                 |            |               
|         +------------+             +-------------+                +-----------------+            |               
|               |                                                             |                    |               
|               |                                                             |                    |               
|               |                                                             |                    |               
|               |                                                             |                    |               
|               |                                                             |                    |               
|               |                                                             |                    |               
|               |                                                             |                    |               
|               |                     REAL-TIME SAFE                          |                    |               
|               |                  ====================                       |                    |               
|               |                  - No Heap Allocation                       |                    |               
|               |                  - Lock-free atomics                        |                    |               
|               |                                                             |                    |               
|               |--------------------------------------------------------------                    |               
|                                           |                                                      |               
|                                           |                                                      |               
|                                           |                                                      |               
|                                           |                                                      |               
|                                           v                                                      |               
|                               +-----------------------+                                          |               
|                               |                       |                                          |               
|                               |     AUDIO OUTPUT      |                                          |               
|                               |     [-1.0, +1.0]      |                                          |               
|                               |                       |                                          |               
|                               +-----------------------+                                          |               
|                                                                                                  |               
+--------------------------------------------------------------------------------------------------+               
```

## 🔧 Prerequisites

Before building SonicForge DSP, ensure you have the following installed:

* C++17/20 compatible compiler (GCC 9+ or Clang 10+)
* CMake 3.15+
* Make or Ninja build system
* Optional: pkg-config for easier dependency management

## 🧱 Building from Source

This project uses the CMake build system with support for both Make and Ninja.

### Quick Start

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

### Build with Ninja (Recommended)

Ninja offers faster incremental builds compared to Make:

```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
```

### Build Types

Specify the build type for optimized or debug builds:

```bash
# Release build (optimized, -O3 -march=native)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug build (with AddressSanitizer and UndefinedBehaviorSanitizer)
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

### CMake Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `SONICFORGE_BUILD_EXAMPLES` | `ON` | Build example programs (sine_example, wav_writer_example) |
| `SONICFORGE_BUILD_TESTS` | `ON` | Build unit tests |

Example with custom options:

```bash
# Build only the library, skip examples and tests
cmake -DSONICFORGE_BUILD_EXAMPLES=OFF -DSONICFORGE_BUILD_TESTS=OFF ..

# Full release build with examples
cmake -DCMAKE_BUILD_TYPE=Release -DSONICFORGE_BUILD_EXAMPLES=ON ..
```

### Installation

To install the library system-wide:

```bash
# From the build directory
sudo make install
```

This will install:
* Headers to `/usr/local/include/sonicforge/`
* Library binaries to `/usr/local/lib/`
* Pkg-config files to `/usr/local/lib/pkgconfig/`

## 🎵 Usage

### Basic Example

```cpp
#include <sonicforge/oscillator.hpp>

int main() {
    // Create a sine wave oscillator at A4 (440 Hz)
    sonicforge::Oscillator osc(sonicforge::Waveform::SINE, 440.0f);
    osc.set_sample_rate(48000.0f);  // Match your audio system
    
    // Process audio samples in your audio callback
    constexpr size_t BUFFER_SIZE = 256;
    float buffer[BUFFER_SIZE];
    
    // Efficient block processing
    osc.process_block(buffer, BUFFER_SIZE);
    
    // Or process sample-by-sample
    float sample = osc.process();
    
    // Change parameters in real-time (thread-safe)
    osc.set_frequency(880.0f);  // Up one octave
    osc.set_waveform(sonicforge::Waveform::SAW);
    
    return 0;
}
```

### 🔗 Linking to Your Project

Using CMake:

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(SONICFORGE REQUIRED IMPORTED_TARGET sonicforge)

target_link_libraries(your_target PRIVATE PkgConfig::SONICFORGE)
```

Or manually with compiler flags:

```bash
g++ -std=c++17 your_code.cpp -lsonicforge -o your_app
```

## 📚 Documentation

API documentation is generated using Doxygen and can be built locally:

```bash
# Install doxygen
sudo dnf install doxygen  # Fedora
# or
sudo apt install doxygen  # Ubuntu/Debian

# Generate documentation
doxygen Doxyfile
```

Documentation will be available in the `docs/` directory.

## 👥 Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a pull request

Please ensure your code follows the existing style and includes appropriate tests.

### Coding Standards

* Follow modern C++ best practices
* Use clang-format with the provided configuration
* Write unit tests for new functionality
* Document public APIs with Doxygen comments

## ⚖️ License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

```
MIT License

Copyright (c) 2026 Nanometer7008

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 📞 Contact

* Project Maintainer: Nanometer7008
* Issue Tracker: [Codeberg Issues](https://codeberg.org/Nanometer7008/sonic-forge-dsp/issues)
* Email: nanometer7008@codeberg.org

## ❤️ Acknowledgments

* Inspired by the modular synthesis community
* Built with insights from the Linux audio development ecosystem
* Thanks to all contributors and testers
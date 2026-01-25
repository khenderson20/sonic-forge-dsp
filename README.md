# SonicForge DSP

A high-performance, real-time C++ Digital Signal Processing (DSP) library designed for modular synthesis and systems programming research.

## 🚀 Overview
**SonicForge DSP** is a lightweight C++ framework focused on low-latency signal generation and processing. This project serves as a showcase of modern systems programming practices, specifically addressing the unique constraints of real-time audio—where heap allocation and thread-blocking are strictly avoided to ensure signal stability.

## 🛠️ Technical Highlights
* **Real-time Safe Architecture:** Implementation follows "lock-free" and "allocation-avoidant" patterns in the audio callback to prevent priority inversion and audio glitches.
* **Modern C++ Standard:** Leverages C++17/20 features including smart pointers for memory management, `std::atomic` for thread-safe parameter modulation, and templates for efficient buffer processing.
* **Linux-First Development:** Optimized for Fedora environments using modern tooling including `cmake`, `clang-format`, and `gcc`.
* **Automated Quality Assurance:** Integrated **Woodpecker CI** pipeline for automated builds, linting, and unit testing on every push.

## 📂 Project Structure
* `include/`: Public API headers and interface definitions.
* `src/`: Implementation of oscillators, filters, and core DSP logic.
* `tests/`: Unit testing suite ensuring mathematical accuracy of audio algorithms.
* `examples/`: Minimal CLI implementations for signal verification and synthesis testing.
* `.woodpecker/`: CI/CD pipeline configuration for Codeberg's automated infrastructure.

## 🔨 Building from Source
This project uses the CMake build system.

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
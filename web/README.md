# 🌐 SonicForge DSP — WebAssembly & 3D Visualization

Real-time audio synthesis and 3D wavetable visualization powered by **SonicForge DSP**, compiled to WebAssembly and rendered in the browser using **Three.js**.

## 🏗️ Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Browser (Main Thread)                  │
│                                                          │
│  ┌────────────┐   postMessage    ┌──────────────────┐    │
│  │  Three.js  │◄─────────────────│  AudioWorklet     │    │
│  │  (3D Viz)  │   waveform data  │  (audio thread)   │    │
│  └────────────┘                  │                   │    │
│                                  │  ┌─────────────┐  │    │
│                                  │  │ Wasm Module │  │    │
│                                  │  │ SonicForge  │  │    │
│                                  │  │ Oscillator  │  │    │
│                                  │  └─────────────┘  │    │
│                                  └────────┬──────────    │
│                                           │               │
│                                  ┌────────▼──────────┐    │
│                                  │  Audio Output     │    │
│                                  │  (Speakers)       │    │
│                                  └───────────────────┘    │
└──────────────────────────────────────────────────────────┘
```

## ✨ Features

| Feature | Description |
|---------|-------------|
| **🔷 WebAssembly DSP** | C++ oscillator core compiled to raw `.wasm` binary via Emscripten (`STANDALONE_WASM`) |
| **🎧 AudioWorklet Integration** | Runs in a separate high-priority audio thread for glitch-free playback |
| **🚀 Zero-Copy Transfer** | Wasm binary transferred via `ArrayBuffer` transferables |
| **🎨 3D Wavetable Visualization** | Real-time 3D rendering of waveform history using Three.js |
| **🐌 Decoupled Animation** | Slow-motion mode decouples audio rate from render rate |
| **🎛️ Interactive Controls** | Adjust frequency, waveform, amplitude decay, and harmonics in real-time |

## 🚀 Quick Start

### 🔨 Build

**From project root (recommended):**

```bash
mkdir build && cd build
cmake .. -DSONICFORGE_BUILD_WEB=ON -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

**Standalone Emscripten build:**

```bash
cd web
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
```

The build produces `sonicforge.wasm` in `web/public/`. No Emscripten JavaScript glue code is used; the module is a standalone Wasm binary manually instantiated inside the AudioWorklet.

### ▶️ Run

```bash
cd web/public
python3 -m http.server 8080
```

Open **http://localhost:8080** and click **Start Audio**.

## 🎨 Visualization

### 📐 3D Wavetable View

The canvas displays a "ribbon" of waveform slices along the Z-axis:

| Axis | Represents |
|------|------------|
| **X** | Signal Phase (0 → 2π) |
| **Y** | Amplitude (–1.0 to +1.0) |
| **Z** | Wavetable Position (history depth) |

### 🎛️ Controls

| Control | Range | Effect |
|---------|-------|--------|
| **🎵 Frequency** | 40Hz – 1200Hz | Sine/saw/square/triangle wave generation |
| **📉 Decay** | 0% – 100% | Linear amplitude envelope across wavetable depth |
| **🔊 Harmonics** | 1 – 16 | Additive synthesis for spectral complexity |
| **⏱️ Speed** | Variable | Slow-motion update rate for the 3D animation |

### 🐌 Slow-Motion Mode

The audio thread runs at full speed (~187Hz), but the visualization updates at a decoupled rate (configurable via the **Speed** slider). This allows you to watch the waveform propagate through 3D space without motion blur or jitter.

## 📁 File Structure

```
web/
├── CMakeLists.txt                  # Emscripten build configuration
├── src/
│   └── sonicforge_worklet.cpp      # C bridge for Wasm exports
└── public/
    ├── index.html                  # Demo page with Three.js UI overlay
    ├── main.js                     # AudioContext, Three.js scene, animation loop
    ├── sonicforge-processor.js     # AudioWorklet processor
    └── sonicforge.wasm             # Generated Wasm binary
```

## 🔧 Troubleshooting

| Issue | Solution |
|-------|----------|
| **🔇 No Sound** | Click "Start Audio" (browsers require a user gesture). Disable ad blockers for `localhost`. |
| **❌ Wasm Fetch Failed** | Ensure `sonicforge.wasm` is in the same directory as `index.html` and served by an HTTP server. |
| **⚠️ AudioWorklet Error** | Verify browser supports `AudioContext.audioWorklet` (Safari 14.1+ required). |
| **👁️ Visual Glitches** | Ensure browser supports `LineSegments` and vertex colors (standard in all modern browsers). |

## 🌍 Browser Compatibility

| Browser | Minimum Version |
|---------|-----------------|
| Chrome | 66+ |
| Firefox | 76+ |
| Safari | 14.1+ |

## ⚖️ License

MIT — same as the parent SonicForge DSP project.

/**
 * @file sonicforge-processor.js
 * @brief AudioWorklet processor for SonicForge DSP WebAssembly module.
 *
 * Runs in a separate high-priority audio thread, instantiating the raw
 * Wasm binary and feeding sample output to the audio destination.
 */

const RENDER_QUANTUM = 128;
const DEBUG = false;

class SonicForgeProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ready = false;
    this.instance = null;
    this.heapView = null;
    this.queue = [];

    this.port.onmessage = ({ data }) => {
      (async () => {
        try {
          if (!this.ready) {
            if (data.type === 'init-wasm') {
              if (DEBUG) console.log('[Worklet] Received Wasm binary, size:', data.wasmBinary.byteLength);
              await this._initWasm(data.wasmBinary);
              this.ready = true;
              if (DEBUG) console.log('[Worklet] Wasm initialized, processing queue...');
              this.queue.forEach((msg) => this._handleMessage(msg));
              this.queue = [];
              this.port.postMessage({ type: 'ready' });
            } else {
              this.queue.push(data);
            }
          } else {
            this._handleMessage(data);
          }
        } catch (err) {
          console.error('[Worklet] Fatal error:', err);
          this.port.postMessage({ type: 'error', message: err.message });
        }
      })();
    };
  }

  async _initWasm(wasmBuffer) {
    if (DEBUG) console.log('[Worklet] Instantiating WebAssembly...');

    // STANDALONE_WASM modules export their own memory.
    // Instantiate without import overrides to use the module's built-in memory.
    const { instance } = await WebAssembly.instantiate(wasmBuffer, {});

    this.instance = instance.exports;
    if (DEBUG) console.log('[Worklet] Exports:', Object.keys(this.instance));

    // Call _initialize to set up the stack and global constructors.
    // This is critical when using STANDALONE_WASM with --no-entry.
    if (this.instance._initialize) {
      this.instance._initialize();
      if (DEBUG) console.log('[Worklet] Wasm _initialize() called');
    }

    if (!this.instance.get_buffer) {
      throw new Error('Missing get_buffer export');
    }

    // Use the memory exported by the Wasm module, not a manually created one.
    const wasmMemory = this.instance.memory;
    if (!wasmMemory) {
      throw new Error('Wasm module does not export memory — compile with STANDALONE_WASM');
    }

    const bufPtr = this.instance.get_buffer();
    if (DEBUG) console.log('[Worklet] Buffer pointer:', bufPtr);

    this.heapView = new Float32Array(wasmMemory.buffer, bufPtr, RENDER_QUANTUM);
    if (DEBUG) console.log('[Worklet] Heap view created');
  }

  _handleMessage(data) {
    const { type } = data;
    if (type === 'init') {
      // audio_init now returns 0 on success, negative on error
      const result = this.instance.audio_init(data.waveform ?? 0, data.frequency ?? 440.0, sampleRate);
      if (result !== 0) {
        const reasons = { '-1': 'invalid waveform', '-2': 'invalid frequency', '-3': 'invalid sample rate' };
        const reason = reasons[String(result)] ?? `error code ${result}`;
        console.error(`[Worklet] audio_init failed: ${reason}`);
        this.port.postMessage({ type: 'error', message: `audio_init failed: ${reason}` });
      }
    } else if (type === 'frequency') {
      this.instance.audio_set_frequency(data.value);
    } else if (type === 'waveform') {
      // audio_set_waveform returns 0 on success, -1 on invalid waveform
      const result = this.instance.audio_set_waveform(data.value);
      if (result !== 0) {
        console.error(`[Worklet] audio_set_waveform failed: invalid waveform index ${data.value}`);
      }
    } else if (type === 'destroy') {
      // Free C++ oscillator memory to prevent Wasm heap leaks
      if (this.instance.audio_destroy) {
        this.instance.audio_destroy();
      }
    }
  }

  process(_inputs, outputs) {
    if (!this.ready) return true;

    const wasmMemory = this.instance.memory;
    if (this.heapView.buffer !== wasmMemory.buffer) {
      const bufPtr = this.instance.get_buffer();
      this.heapView = new Float32Array(wasmMemory.buffer, bufPtr, RENDER_QUANTUM);
    }

    this.instance.audio_process(RENDER_QUANTUM);
    outputs[0][0].set(this.heapView);

    return true;
  }
}

registerProcessor('sonicforge-processor', SonicForgeProcessor);
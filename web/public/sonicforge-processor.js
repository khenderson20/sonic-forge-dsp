/**
 * @file sonicforge-processor.js
 * @brief AudioWorklet processor for SonicForge DSP WebAssembly module.
 *
 * Runs in a separate high-priority audio thread, instantiating the raw
 * Wasm binary and feeding sample output to the audio destination.
 *
 * SharedArrayBuffer ring buffer
 * ─────────────────────────────
 * When the main thread provides a SharedArrayBuffer via an 'init-sab' message,
 * this processor writes every rendered sample into a lock-free ring.  The main
 * thread reads the ring asynchronously to drive the "Live Audio" ribbon mode.
 *
 * Layout of the SAB (provided by the main thread):
 *   Bytes  0 –  3 : Int32  — atomic write-position cursor (index into ringData)
 *   Bytes  4 –  N : Float32[] — circular sample ring (length = (N-4)/4)
 *
 * The ring length must be a power of two so the mask optimisation is valid.
 * The main thread is responsible for allocating the buffer at the right size.
 */

const RENDER_QUANTUM = 128;
const DEBUG = false;

class SonicForgeProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ready    = false;
    this.instance = null;
    this.heapView = null;
    this.queue    = [];

    // SharedArrayBuffer ring buffer (optional — requires cross-origin isolation).
    this.ringCtrl = null;   // Int32Array  [1]  — atomic write cursor
    this.ringData = null;   // Float32Array[N]  — circular sample storage
    this.ringMask = 0;      // bit-mask = ringData.length - 1

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
      // audio_init returns 0 on success, negative on error
      const result = this.instance.audio_init(data.waveform ?? 0, data.frequency ?? 440.0, sampleRate);
      if (result !== 0) {
        const reasons = { '-1': 'invalid waveform', '-2': 'invalid frequency', '-3': 'invalid sample rate' };
        const reason = reasons[String(result)] ?? `error code ${result}`;
        console.error(`[Worklet] audio_init failed: ${reason}`);
        this.port.postMessage({ type: 'error', message: `audio_init failed: ${reason}` });
        return;
      }
      // Apply harmonics immediately after init so the engine matches
      // whatever the UI slider was already showing.
      if (data.harmonics && data.harmonics > 1) {
        this.instance.audio_set_harmonics(data.harmonics);
      }
    } else if (type === 'frequency') {
      this.instance.audio_set_frequency(data.value);
    } else if (type === 'waveform') {
      const result = this.instance.audio_set_waveform(data.value);
      if (result !== 0) {
        console.error(`[Worklet] audio_set_waveform failed: invalid waveform index ${data.value}`);
      }
    } else if (type === 'harmonics') {
      // Update the number of harmonics mixed into the audio output.
      // audio_set_harmonics() phase-syncs any newly activated oscillators
      // to the fundamental to avoid a click on the transition.
      if (this.instance.audio_set_harmonics) {
        this.instance.audio_set_harmonics(data.value);
      }
    } else if (type === 'destroy') {
      if (this.instance.audio_destroy) {
        this.instance.audio_destroy();
      }
    } else if (type === 'init-sab') {
      // Accept a SharedArrayBuffer ring from the main thread.
      // Layout: [Int32 write-cursor (4 bytes)] [Float32 ring samples (rest)]
      // The ring length must be a power of two (enforced by the main thread).
      const sab = data.ringBuffer;
      const candidateData = new Float32Array(sab, 4);
      const len = candidateData.length;

      // Guard: mask optimisation is only valid for power-of-two lengths.
      if (len === 0 || (len & (len - 1)) !== 0) {
        console.error('[Worklet] SAB ring length must be a power of two, got:', len,
                      '— ring buffer NOT attached.');
        return;
      }

      this.ringCtrl = new Int32Array(sab, 0, 1);
      this.ringData = candidateData;
      this.ringMask = len - 1;
      if (DEBUG) {
        console.log('[Worklet] SAB ring buffer attached — capacity:', len, 'samples');
      }
    }
  }

  process(_inputs, outputs) {
    if (!this.ready) return true;

    // Re-create heap view if Wasm memory was grown (buffer detach).
    const wasmMemory = this.instance.memory;
    if (this.heapView.buffer !== wasmMemory.buffer) {
      const bufPtr = this.instance.get_buffer();
      this.heapView = new Float32Array(wasmMemory.buffer, bufPtr, RENDER_QUANTUM);
    }

    this.instance.audio_process(RENDER_QUANTUM);
    outputs[0][0].set(this.heapView);

    // ── SharedArrayBuffer ring buffer write ──────────────────────────────────
    // Write the freshly rendered samples into the ring so the main thread can
    // read them without any postMessage overhead.
    //
    // Atomics.load/store ensure the write cursor update is visible to the main
    // thread only after all sample values have been written.  The cursor is a
    // plain index (not a byte offset) into ringData; the mask keeps it in range.
    if (this.ringData !== null) {
      const wp   = Atomics.load(this.ringCtrl, 0);
      const mask = this.ringMask;
      for (let i = 0; i < RENDER_QUANTUM; i++) {
        this.ringData[(wp + i) & mask] = this.heapView[i];
      }
      // Advance the cursor atomically so the main thread sees a consistent snapshot.
      Atomics.store(this.ringCtrl, 0, (wp + RENDER_QUANTUM) & mask);
    }

    return true;
  }
}

registerProcessor('sonicforge-processor', SonicForgeProcessor);

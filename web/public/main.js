import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

/* ═══════════════════════════════════════════════════════════════════
   Constants
   ═══════════════════════════════════════════════════════════════════ */
const AXES_LENGTH = 120;
const AMP_SCALE = 26;
const X_AXIS_WIDTH = 220;
const Z_SLICE_SPACING = 3.0;
const GRID_SIZE = 400;
const GRID_DIVISIONS = 40;
const GROUND_Y = -28;
const CAMERA_POS = { x: 0, y: 70, z: 160 };
const CAMERA_FOV = 48;
const CAMERA_NEAR = 0.1;
const CAMERA_FAR = 2000;
const CONTROLS_MIN_DIST = 40;
const CONTROLS_MAX_DIST = 500;
const CONTROLS_TARGET = { x: 0, y: 0, z: -40 };
const PLANE_WIDTH = 240;
const PLANE_HEIGHT = 120;
const FOG_DENSITY = 0.0025;

/* ═══════════════════════════════════════════════════════════════════
   Helpers
   ═══════════════════════════════════════════════════════════════════ */
const $ = (id) => document.getElementById(id);

/**
 * Clamp a numeric value to [min, max] and verify it is a finite number.
 * Returns the clamped value, or `fallback` if the input is non-finite.
 *
 * Defence-in-depth: the C++ layer also validates, but catching bad values
 * early avoids unnecessary postMessage traffic and worklet error paths.
 */
function clampFinite(value, min, max, fallback) {
    const n = Number(value);
    if (!Number.isFinite(n)) return fallback;
    return Math.min(Math.max(n, min), max);
}

/* Create a canvas-texture sprite label */
function label(text, color, size = 44) {
    const c = document.createElement('canvas');
    c.width = 512; c.height = 96;
    const ctx = c.getContext('2d');
    ctx.clearRect(0, 0, c.width, c.height);
    ctx.font = `500 ${size}px 'JetBrains Mono', monospace`;
    ctx.fillStyle = color;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(text, 256, 48);
    const tex = new THREE.CanvasTexture(c);
    tex.minFilter = THREE.LinearFilter;
    const spr = new THREE.Sprite(new THREE.SpriteMaterial({ map: tex, transparent: true, depthTest: false }));
    spr.scale.set(5, 1, 1);
    return spr;
}

/* Single axis line */
function axisLine(a, b, color) {
    const g = new THREE.BufferGeometry().setFromPoints([a, b]);
    return new THREE.Line(g, new THREE.LineBasicMaterial({ color }));
}

/* Tick marks */
function tickMark(pos, color, length = 1.5) {
    const d = pos.clone().normalize();
    return axisLine(pos.clone().sub(d.clone().multiplyScalar(length)),
                     pos.clone().add(d.clone().multiplyScalar(length)), color);
}

/* ═══════════════════════════════════════════════════════════════════
   Wavetable visualization
   ═══════════════════════════════════════════════════════════════════ */
class SonicForgeApp {
  constructor() {
    // Audio
    this.audioCtx   = null;
    this.gainNode   = null;
    this.worklet    = null;
    this.started    = false;
    this.muted      = false;
    this.freq       = 440;
    this.wave       = 0;          // 0=sine,1=saw,2=square,3=tri
    this.decay      = 0;          // 0–100 (percentage)
    this.harmonics  = 1;          // additive harmonics count

    // Wavetable geometry
    this.SLICES     = 32;         // Z-axis wavetable positions
    this.PTS        = 256;        // X-axis resolution per slice
    this.waveData   = new Float32Array(this.SLICES * this.PTS);
    this.dirty      = false;

    // Three.js
    this.scene    = null;
    this.camera   = null;
    this.renderer = null;
    this.controls = null;
    this.ribbon   = null;         // the wavetable surface
    this.decayLine = null;        // decay envelope curve
  }

  init() {
    this._buildScene();
    this._buildAxes();
    this._buildWavetable();
    this._buildDecayEnvelope();
    this._wireUI();
    this._tick();
  }

  /* ── Scene / camera / renderer ───────────────────────────────── */
  _buildScene() {
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x06060a);
    this.scene.fog = new THREE.FogExp2(0x06060a, FOG_DENSITY);

    this.camera = new THREE.PerspectiveCamera(CAMERA_FOV, innerWidth / innerHeight, CAMERA_NEAR, CAMERA_FAR);
    this.camera.position.set(CAMERA_POS.x, CAMERA_POS.y, CAMERA_POS.z);

    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setSize(innerWidth, innerHeight);
    this.renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
    this.renderer.toneMapping = THREE.NoToneMapping;
    $('canvas-container').appendChild(this.renderer.domElement);

    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping  = true;
    this.controls.dampingFactor  = 0.06;
    this.controls.minDistance    = CONTROLS_MIN_DIST;
    this.controls.maxDistance    = CONTROLS_MAX_DIST;
    this.controls.target.set(CONTROLS_TARGET.x, CONTROLS_TARGET.y, CONTROLS_TARGET.z);

    // Subtle ground grid
    const grid = new THREE.GridHelper(GRID_SIZE, GRID_DIVISIONS, 0x101018, 0x101018);
    grid.position.y = GROUND_Y;
    this.scene.add(grid);

    addEventListener('resize', () => {
      this.camera.aspect = innerWidth / innerHeight;
      this.camera.updateProjectionMatrix();
      this.renderer.setSize(innerWidth, innerHeight);
    });
  }

  /* ── Axes & labels ───────────────────────────────────────────── */
  _buildAxes() {
    const L = AXES_LENGTH;
    const tickOffset = 1.5;
    const labelOffsetX = 6;

    // X = Phase (red)
    this.scene.add(axisLine(new THREE.Vector3(-L, GROUND_Y + 2, 0), new THREE.Vector3(L, GROUND_Y + 2, 0), 0xcc6655));
    const xl = label('Phase (0 → 2π)', '#cc6655', 38);
    xl.position.set(L + 4, GROUND_Y + 2, 0);
    this.scene.add(xl);

    // Y = Amplitude (teal)
    this.scene.add(axisLine(new THREE.Vector3(-L, GROUND_Y + 2, 0), new THREE.Vector3(-L, AMP_SCALE + 2, 0), 0x4ecdc4));
    const yl = label('Amplitude', '#4ecdc4', 38);
    yl.position.set(-L - labelOffsetX, AMP_SCALE + 6, 0);
    this.scene.add(yl);

    // Z = Wavetable slice (blue)
    this.scene.add(axisLine(new THREE.Vector3(-L, GROUND_Y + 2, 0), new THREE.Vector3(-L, GROUND_Y + 2, -L), 0x5577bb));
    const zl = label('Wavetable position', '#5577bb', 34);
    zl.position.set(-L - labelOffsetX, GROUND_Y + 2, -L - 4);
    this.scene.add(zl);

    // Y-axis ticks: -1.0, -0.5, 0.0, 0.5, 1.0
    for (const v of [-1, -0.5, 0, 0.5, 1]) {
      const y = v * AMP_SCALE;
      this.scene.add(axisLine(
        new THREE.Vector3(-L - tickOffset, y, 0),
        new THREE.Vector3(-L + tickOffset, y, 0),
        0x383850
      ));
      const tl = label(v.toFixed(1), '#505068', 30);
      tl.scale.set(1.4, 0.35, 1);
      tl.position.set(-L - labelOffsetX, y, 0);
      this.scene.add(tl);
    }

    // Zero-crossing plane (subtle)
    const planeGeo = new THREE.PlaneGeometry(PLANE_WIDTH, PLANE_HEIGHT);
    const planeMat = new THREE.MeshBasicMaterial({
      color: 0x202028, transparent: true, opacity: 0.15, side: THREE.DoubleSide, depthWrite: false
    });
    const plane = new THREE.Mesh(planeGeo, planeMat);
    plane.rotation.x = Math.PI / 2;
    plane.position.set(0, 0, CONTROLS_TARGET.z);
    this.scene.add(plane);
  }

  /* ── Wavetable ribbon surface ────────────────────────────────── */
  _buildWavetable() {
    // Ribbon geometry: each Z-slice is a continuous polyline.
    // We use LineSegments with an index buffer.
    const n = this.SLICES;
    const p = this.PTS;
    const totalVerts = n * p;

    const geo = new THREE.BufferGeometry();
    const pos = new Float32Array(totalVerts * 3);
    const col = new Float32Array(totalVerts * 3);
    const idx = new Uint32Array(n * (p - 1) * 2);

    for (let s = 0; s < n; s++) {
      const base = s * p;
      for (let i = 0; i < p - 1; i++) {
        const k = s * (p - 1) * 2 + i * 2;
        idx[k]     = base + i;
        idx[k + 1] = base + i + 1;
      }
    }

    geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    geo.setAttribute('color',    new THREE.BufferAttribute(col, 3));
    geo.setIndex(new THREE.BufferAttribute(idx, 1));

    const mat = new THREE.LineBasicMaterial({
      vertexColors: true,
      transparent: true,
      opacity: 0.85,
      depthWrite: false,
      linewidth: 1,
    });

    this.ribbon = new THREE.LineSegments(geo, mat);
    this.scene.add(this.ribbon);
    this._rebuildWaveData();
  }

  /* ── Decay envelope curve ────────────────────────────────────── */
  _buildDecayEnvelope() {
    const n = this.SLICES;
    const geo = new THREE.BufferGeometry();
    const pos = new Float32Array(n * 3);
    const idx = new Uint32Array((n - 1) * 2);
    for (let i = 0; i < n - 1; i++) { idx[i * 2] = i; idx[i * 2 + 1] = i + 1; }

    geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    geo.setIndex(new THREE.BufferAttribute(idx, 1));

    const mat = new THREE.LineBasicMaterial({
      color: 0xe06050, transparent: true, opacity: 0.6, depthWrite: false
    });

    this.decayLine = new THREE.LineSegments(geo, mat);
    this.scene.add(this.decayLine);
    this._updateDecayEnvelope();
  }

  /* ── Compute wavetable data ──────────────────────────────────── */
  _rebuildWaveData() {
    const n = this.SLICES;
    const p = this.PTS;
    const decayAmt = this.decay / 100;
    const pos = this.ribbon.geometry.attributes.position.array;
    const col = this.ribbon.geometry.attributes.color.array;

    for (let s = 0; s < n; s++) {
      const decayFactor = 1.0 - (s / n) * decayAmt;
      const zOff = -s * Z_SLICE_SPACING;

      for (let i = 0; i < p; i++) {
        const phase = (i / p) * 2 * Math.PI;
        let amp = this._waveSample(phase);

        // Additive harmonics
        for (let h = 2; h <= this.harmonics; h++) {
          amp += this._waveSample(phase * h) / h;
        }

        amp *= decayFactor;

        const x = (i / p - 0.5) * X_AXIS_WIDTH;
        const y = amp * AMP_SCALE;

        const v = s * p + i;
        pos[v * 3]     = x;
        pos[v * 3 + 1] = y;
        pos[v * 3 + 2] = zOff;

        // Color gradient: teal → warm fade based on decay
        const t = s / n;
        col[v * 3]     = 0.30 + t * 0.58;
        col[v * 3 + 1] = 0.81 - t * 0.35;
        col[v * 3 + 2] = 0.77 - t * 0.20;
      }
    }

    this.ribbon.geometry.attributes.position.needsUpdate = true;
    this.ribbon.geometry.attributes.color.needsUpdate = true;
    this._updateDecayEnvelope();
  }

  _waveSample(phase) {
    const p = ((phase % (2 * Math.PI)) + 2 * Math.PI) % (2 * Math.PI);
    switch (this.wave) {
      case 0: return Math.sin(p);
      case 1: return 2 * (p / (2 * Math.PI)) - 1;
      case 2: return p < Math.PI ? 1 : -1;
      case 3: return (p < Math.PI ? 4 * p / (2 * Math.PI) - 1 : 3 - 4 * p / (2 * Math.PI));
      default: return 0;
    }
  }

  /* ── Decay envelope visual ───────────────────────────────────── */
  _updateDecayEnvelope() {
    if (!this.decayLine) return;
    const n = this.SLICES;
    const pos = this.decayLine.geometry.attributes.position.array;
    const decayAmt = this.decay / 100;
    for (let s = 0; s < n; s++) {
      const decayFactor = 1.0 - (s / n) * decayAmt;
      pos[s * 3]     = -X_AXIS_WIDTH / 2;
      pos[s * 3 + 1] = decayFactor * AMP_SCALE;
      pos[s * 3 + 2] = -s * Z_SLICE_SPACING;
    }
    this.decayLine.geometry.attributes.position.needsUpdate = true;
  }

  /* ── Audio bootstrap ─────────────────────────────────────────── */
  async _startAudio() {
    if (this.started) return;
    const btn = $('start-btn');
    btn.textContent = 'Loading…';
    btn.disabled = true;

    try {
      this.audioCtx = new AudioContext();
      this.gainNode = this.audioCtx.createGain();
      this.gainNode.gain.value = this.muted ? 0 : 1;
      const resp = await fetch('sonicforge.wasm');
      if (!resp.ok) throw new Error(`Wasm fetch failed (${resp.status})`);
      const wasm = await resp.arrayBuffer();

      await this.audioCtx.audioWorklet.addModule('sonicforge-processor.js');
      this.worklet = new AudioWorkletNode(this.audioCtx, 'sonicforge-processor', {
        outputChannelCount: [1]
      });
      this.worklet.port.postMessage({ type: 'init-wasm', wasmBinary: wasm }, [wasm]);

       this.worklet.port.onmessage = ({ data }) => {
         if (data.type === 'ready') {
           this.worklet.port.postMessage({ type: 'init', waveform: this.wave, frequency: this.freq });
           btn.textContent = 'Audio Running';
           btn.disabled = false;
           // Show stop and mute buttons, hide start button
           $('stop-btn').style.display = 'inline-block';
           $('mute-btn').style.display = 'inline-block';
           btn.style.display = 'none';
         } else if (data.type === 'error') {
           this._onAudioError(`Worklet error: ${data.message}`, btn);
         }
       };

      this.worklet.connect(this.gainNode);
      this.gainNode.connect(this.audioCtx.destination);
      this.started = true;
    } catch (err) {
      this._onAudioError(err.message, btn);
    }
  }

   _onAudioError(message, btn) {
     console.error('[SonicForge]', message);
     btn.textContent = 'Retry';
     btn.disabled = false;
     btn.onclick = () => {
       btn.onclick = null;
       this._startAudio();
     };
   }

   /* ── Audio stop ──────────────────────────────────────────────── */
   _stopAudio() {
     if (!this.audioCtx) return;
     this.worklet.disconnect();
     this.gainNode.disconnect();
     this.audioCtx.close();
     this.audioCtx = null;
     this.gainNode = null;
     this.worklet = null;
     this.started = false;

     const startBtn = $('start-btn');
     const stopBtn = $('stop-btn');
     const muteBtn = $('mute-btn');
     startBtn.textContent = 'Start Audio';
     startBtn.style.display = 'inline-block';
     stopBtn.style.display = 'none';
     muteBtn.style.display = 'none';
     muteBtn.classList.remove('muted');
     this.muted = false;
   }

   /* ── Toggle mute ────────────────────────────────────────────── */
   _toggleMute() {
     if (!this.gainNode) return;
     this.muted = !this.muted;
     this.gainNode.gain.value = this.muted ? 0 : 1;
     const muteBtn = $('mute-btn');
     muteBtn.classList.toggle('muted', this.muted);
     muteBtn.innerHTML = this.muted ? '&#128263;' : '&#128266;';
   }

   /* ── UI wiring ───────────────────────────────────────────────── */
   _wireUI() {
     $('start-btn').addEventListener('click', () => this._startAudio());
     $('stop-btn').addEventListener('click', () => this._stopAudio());
     $('mute-btn').addEventListener('click', () => this._toggleMute());

     $('freq-slider').addEventListener('input', (e) => {
       // Clamp to the slider's own range [40, 1200] Hz — defence-in-depth
       // before the value reaches the AudioWorklet / C++ layer.
       const freq = clampFinite(e.target.value, 40, 1200, 440);
       this.freq = freq;
       $('freq-val').textContent = `${freq} Hz`;
       $('r-freq').textContent   = `${freq} Hz`;
       if (this.worklet) this.worklet.port.postMessage({ type: 'frequency', value: freq });
     });

     $('decay-slider').addEventListener('input', (e) => {
       const decay = clampFinite(e.target.value, 0, 100, 0);
       this.decay = decay;
       $('decay-val').textContent = `${decay}%`;
       $('r-decay').textContent   = `${decay}%`;
       this._rebuildWaveData();
     });

     $('harm-slider').addEventListener('input', (e) => {
       const harm = Math.round(clampFinite(e.target.value, 1, 16, 1));
       this.harmonics = harm;
       $('harm-val').textContent = harm;
       this._rebuildWaveData();
     });

     document.querySelectorAll('.wbtn').forEach((btn) => {
       btn.addEventListener('click', () => {
         // Clamp waveform index to the four valid values [0, 3]
         const wave = clampFinite(Number(btn.dataset.wave), 0, 3, 0);
         this.wave = Math.round(wave);
         document.querySelectorAll('.wbtn').forEach((b) => b.classList.remove('active'));
         btn.classList.add('active');
         $('r-wave').textContent = ['Sine', 'Saw', 'Square', 'Triangle'][this.wave];
         if (this.worklet) this.worklet.port.postMessage({ type: 'waveform', value: this.wave });
         this._rebuildWaveData();
       });
     });
   }

  /* ── Render loop ─────────────────────────────────────────────── */
  _tick() {
    requestAnimationFrame(() => this._tick());
    this.controls.update();
    this.renderer.render(this.scene, this.camera);
  }
}

const app = new SonicForgeApp();
app.init();

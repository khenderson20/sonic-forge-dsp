import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

/* ═══════════════════════════════════════════════════════════════════
   Scene geometry constants
   ═══════════════════════════════════════════════════════════════════ */
const AXES_LENGTH     = 120;
const AMP_SCALE       = 26;
const X_AXIS_WIDTH    = 220;
const Z_SLICE_SPACING = 3.0;
const GRID_SIZE       = 400;
const GRID_DIVISIONS  = 40;
const GROUND_Y        = -28;
const CAMERA_FOV      = 48;
const CAMERA_NEAR     = 0.1;
const CAMERA_FAR      = 2000;
const CONTROLS_MIN_DIST = 50;
const CONTROLS_MAX_DIST = 480;
const FOG_DENSITY     = 0.0022;

// Default camera — restored by _resetCamera() / 'R' key.
const CAM_POS    = Object.freeze({ x: -28, y: 88, z: 172 });
const CAM_TARGET = Object.freeze({ x:   0, y:  5, z: -44 });

/* ═══════════════════════════════════════════════════════════════════
   Colour palette
   ═══════════════════════════════════════════════════════════════════ */
const COL_AXIS_X  = 0xe8975a;  const HEX_AXIS_X = '#e8975a';
const COL_AXIS_Y  = 0x4cd9c8;  const HEX_AXIS_Y = '#4cd9c8';
const COL_AXIS_Z  = 0x8b7ef8;  const HEX_AXIS_Z = '#8b7ef8';

const RIB_FRONT = [0.239, 0.910, 0.812];
const RIB_BACK  = [0.333, 0.133, 0.667];

const COL_ZERO_REF   = 0x1c2a42;
const COL_AMP_BOUNDS = 0x141a2a;
const COL_PHASE_MID  = 0x151825;
const COL_GRID_MAIN  = 0x131320;
const COL_GRID_CTR   = 0x1c1c30;
const COL_DECAY_LINE = 0xf0625a;
const COL_PEAK_DOT   = 0xffffff;

/* ═══════════════════════════════════════════════════════════════════
   Cutoff frequency ↔ slider mapping (logarithmic, 20 Hz – 20 kHz)
   ═══════════════════════════════════════════════════════════════════ */
const _LOG_MIN = Math.log10(20);
const _LOG_MAX = Math.log10(20000);

function sliderToCutoffHz(v) {
    return Math.pow(10, _LOG_MIN + (v / 1000) * (_LOG_MAX - _LOG_MIN));
}
function formatCutoffHz(hz) {
    return hz >= 1000 ? `${(hz / 1000).toFixed(1)} kHz` : `${Math.round(hz)} Hz`;
}

/* ═══════════════════════════════════════════════════════════════════
   SharedArrayBuffer ring
   ═══════════════════════════════════════════════════════════════════ */
const RING_SAMPLES = 16384;
const RING_MASK    = RING_SAMPLES - 1;

/* ═══════════════════════════════════════════════════════════════════
   Helpers
   ═══════════════════════════════════════════════════════════════════ */
const $ = (id) => document.getElementById(id);

function clampFinite(value, min, max, fallback) {
    const n = Number(value);
    if (!Number.isFinite(n)) return fallback;
    return Math.min(Math.max(n, min), max);
}

/**
 * Create a canvas-texture Sprite label with a dark outline so it reads
 * clearly against both light and dark scene regions.
 *
 * The canvas + 2D context are stored in sprite.userData so the text can
 * be updated later without re-creating the texture.
 *
 * @param {string} text   - Label text
 * @param {string} color  - CSS fill colour (hex string)
 * @param {object} opts   - Optional overrides: size, width, height, scaleX, scaleY
 */
function makeLabel(text, color, opts = {}) {
    const {
        size   = 44,
        width  = 512,
        height = 96,
        scaleX = 5,
        scaleY = 1,
    } = opts;

    const c   = document.createElement('canvas');
    c.width   = width;
    c.height  = height;
    const ctx = c.getContext('2d');

    _drawLabelCanvas(ctx, text, color, size, width, height);

    const tex = new THREE.CanvasTexture(c);
    tex.minFilter = THREE.LinearFilter;

    const spr = new THREE.Sprite(
        new THREE.SpriteMaterial({ map: tex, transparent: true, depthTest: false })
    );
    spr.scale.set(scaleX, scaleY, 1);
    spr.userData.canvas = c;
    spr.userData.ctx    = ctx;
    spr.userData.width  = width;
    spr.userData.height = height;
    return spr;
}

/** Redraw the text content of a sprite created with makeLabel(). */
function updateLabel(sprite, text, color, size = 44) {
    const { canvas, ctx, width, height } = sprite.userData;
    if (!canvas || !ctx) return;
    ctx.clearRect(0, 0, width, height);
    _drawLabelCanvas(ctx, text, color, size, width, height);
    sprite.material.map.needsUpdate = true;
}

function _drawLabelCanvas(ctx, text, color, size, w, h) {
    ctx.font         = `500 ${size}px 'JetBrains Mono', monospace`;
    ctx.textAlign    = 'center';
    ctx.textBaseline = 'middle';
    // Dark outline keeps text legible in front of the waveform ribbon.
    ctx.lineWidth    = 5;
    ctx.strokeStyle  = 'rgba(4, 4, 8, 0.94)';
    ctx.strokeText(text, w / 2, h / 2);
    ctx.fillStyle    = color;
    ctx.fillText(text, w / 2, h / 2);
}

/** Thin line segment between two Vector3 points. */
function axisLine(a, b, color, opacity = 1) {
    const mat = new THREE.LineBasicMaterial({ color });
    if (opacity < 1) { mat.transparent = true; mat.opacity = opacity; }
    return new THREE.Line(new THREE.BufferGeometry().setFromPoints([a, b]), mat);
}

/**
 * Apply the ribbon colour palette to a single vertex.
 *
 * Palette: teal (#3de8cf) at the front slice → indigo (#5522aa) at the back.
 * Amplitude-based brightness simulates surface shading: peaks glow at full
 * intensity, zero-crossings recede to 35 % — no lighting model required.
 *
 * @param {Float32Array} col - BufferGeometry colour attribute array
 * @param {number}       v   - vertex index
 * @param {number}       t   - normalised slice position [0 front … 1 back]
 * @param {number}       amp - sample amplitude (may exceed ±1 with harmonics)
 */
function applyRibbonColor(col, v, t, amp) {
    const r = RIB_FRONT[0] + t * (RIB_BACK[0] - RIB_FRONT[0]);
    const g = RIB_FRONT[1] + t * (RIB_BACK[1] - RIB_FRONT[1]);
    const b = RIB_FRONT[2] + t * (RIB_BACK[2] - RIB_FRONT[2]);
    const bright = 0.35 + 0.65 * Math.min(Math.abs(amp), 1.0);
    col[v * 3]     = r * bright;
    col[v * 3 + 1] = g * bright;
    col[v * 3 + 2] = b * bright;
}

/* ═══════════════════════════════════════════════════════════════════
   JS State Variable Filter
   Mirrors the Cytomic ZDF formulation used in the C++ StateVariableFilter.
   Applied per-slice to the 3D visualisation so the ribbon reflects the
   effect of the filter on the waveform shape without requiring a Wasm rebuild.
   ═══════════════════════════════════════════════════════════════════ */
class VizSVF {
    constructor() { this.ic1 = 0; this.ic2 = 0; this.g = 0; this.R = 0; this.H = 0; }

    /**
     * @param {number} fc   Cutoff frequency in Hz
     * @param {number} res  Resonance [0, 1] — same mapping as the C++ SVF
     * @param {number} fs   Sample rate (pass n_pts for visualisation)
     */
    prepare(fc, res, fs) {
        fc  = Math.max(20, Math.min(fc, fs * 0.499));
        res = Math.max(0, Math.min(res, 0.99));
        this.g = Math.tan(Math.PI * fc / fs);
        this.R = 2 * (1 - res);
        this.H = 1 / (1 + this.R * this.g + this.g * this.g);
        this.ic1 = 0;
        this.ic2 = 0;
    }

    /** @param {number} mode  0=LP 1=HP 2=BP 3=Notch */
    process(x, mode) {
        const v3 = x - this.ic2;
        const v1 = this.H * (this.ic1 + this.g * v3);
        const v2 = this.ic2 + this.g * v1;
        this.ic1 = 2 * v1 - this.ic1;
        this.ic2 = 2 * v2 - this.ic2;
        switch (mode) {
            case 0: return v2;
            case 1: return x - this.R * v1 - v2;
            case 2: return v1;
            case 3: return x - this.R * v1;
        }
        return v2;
    }

    reset() { this.ic1 = this.ic2 = 0; }
}

/* ═══════════════════════════════════════════════════════════════════
   JS Waveshaper transfer functions
   Mirrors sonicforge/waveshaper.hpp — used for the 3D visualisation preview.
   ═══════════════════════════════════════════════════════════════════ */
function vizApplyWaveshaper(x, shape, drive) {
    const y = x * drive;
    switch (shape) {
        case 0: return Math.tanh(y);
        case 1: {
            const ay = Math.abs(y);
            if (ay < 1 / 3) return y;
            if (ay >= 2 / 3) return y > 0 ? 1 : -1;
            return y * (1.5 - 0.5 * y * y);
        }
        case 2: return Math.max(-1, Math.min(1, y));
        case 3: {
            let f = y;
            if (f > 1) { f = 2 - f; if (f < -1) f = -2 - f; }
            else if (f < -1) { f = -2 - f; if (f > 1) f = 2 - f; }
            return f;
        }
    }
    return x;
}

/* ═══════════════════════════════════════════════════════════════════
   SonicForgeApp
   ═══════════════════════════════════════════════════════════════════ */
class SonicForgeApp {
  constructor() {
    // ── Audio ──────────────────────────────────────────────────────────────
    this.audioCtx  = null;
    this.gainNode  = null;
    this.worklet   = null;
    this.started   = false;
    this.muted     = false;
    this.freq      = 440;
    this.wave      = 0;
    this.decay     = 0;
    this.harmonics = 1;

    // ── FX chain state ─────────────────────────────────────────────────────
    this.glideMs         = 0;       // SmoothedValue: glide time in ms (0 = off)

    this.filterEnabled   = false;
    this.filterMode      = 0;       // 0=LP 1=HP 2=BP 3=Notch
    this.filterCutoff    = 1000;    // Hz
    this.filterResonance = 0.50;    // [0, 1]

    this.shaperEnabled   = false;
    this.shaperShape     = 0;       // 0=Tanh 1=Poly 2=Hard 3=Fold
    this.shaperDrive     = 1.0;     // multiplier

    this.delayEnabled    = false;
    this.delayTimeMs     = 250;     // ms
    this.delayFeedback   = 0.30;    // [0, 0.95]

    // JS SVF used to preview filter effect in the 3D visualisation.
    this._vizSVF = new VizSVF();

    // ── Wavetable geometry ─────────────────────────────────────────────────
    this.SLICES = 32;
    this.PTS    = 256;

    // ── Wasm visualisation instance (main thread) ──────────────────────────
    this.vizWasm      = null;
    this.vizBufView   = null;
    this.vizWasmReady = false;
    this.vizRenderMs  = 0;

    // ── SharedArrayBuffer ring ─────────────────────────────────────────────
    this.sabAvailable  = false;
    this.ringBuffer    = null;
    this.ringCtrl      = null;
    this.ringData      = null;
    this.liveAudioMode = false;

    // ── Three.js objects ───────────────────────────────────────────────────
    this.scene      = null;
    this.camera     = null;
    this.renderer   = null;
    this.controls   = null;
    this.ribbon     = null;
    this.decayLine  = null;
    this.zeroRefGrp = null;  // reference grid / guide lines
    this.peakMarker = null;  // peak-amplitude callout

    // ── Interactive legend — maps data-target string → THREE.Object3D ──────
    this.legendTargets = {};
  }

  async init() {
    this._buildScene();
    this._buildAxes();
    this._buildReferenceGrid();
    this._buildWavetable();
    this._buildDecayEnvelope();
    this._buildPeakCallout();
    this._wireUI();
    await this._initVizWasm();
    this._tick();
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Wasm visualisation instance
     ───────────────────────────────────────────────────────────────────────── */
  async _initVizWasm() {
    try {
      const resp = await fetch('sonicforge.wasm');
      if (!resp.ok) throw new Error(`Wasm fetch failed (${resp.status})`);
      const wasmBytes = await resp.arrayBuffer();

      const { instance } = await WebAssembly.instantiate(wasmBytes, {});
      this.vizWasm = instance.exports;

      if (this.vizWasm._initialize) this.vizWasm._initialize();

      if (!this.vizWasm.get_viz_buffer || !this.vizWasm.viz_render_wavetable) {
        throw new Error('Viz Wasm exports missing — rebuild with updated EXPORTED_FUNCTIONS');
      }

      const vizPtr = this.vizWasm.get_viz_buffer();
      this.vizBufView = new Float32Array(
        this.vizWasm.memory.buffer, vizPtr, this.SLICES * this.PTS
      );

      this.vizWasmReady = true;
      this._rebuildWaveData();
      if ($('r-dsp-source')) $('r-dsp-source').textContent = 'Wasm (LUT + PolyBLEP)';
    } catch (err) {
      console.warn('[SonicForge] Viz Wasm unavailable, using JS fallback:', err);
      this.vizWasmReady = false;
      this._rebuildWaveData();
      if ($('r-dsp-source')) $('r-dsp-source').textContent = 'JS (fallback)';
    }
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Scene / camera / renderer
     ───────────────────────────────────────────────────────────────────────── */
  _buildScene() {
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x06060a);
    // Tighter fog draws back-slices into darkness — depth-perception cue.
    this.scene.fog = new THREE.FogExp2(0x06060a, FOG_DENSITY);

    this.camera = new THREE.PerspectiveCamera(
        CAMERA_FOV, innerWidth / innerHeight, CAMERA_NEAR, CAMERA_FAR
    );
    this.camera.position.set(CAM_POS.x, CAM_POS.y, CAM_POS.z);

    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setSize(innerWidth, innerHeight);
    this.renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
    this.renderer.toneMapping = THREE.NoToneMapping;
    $('canvas-container').appendChild(this.renderer.domElement);

    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;
    this.controls.dampingFactor = 0.07;
    this.controls.minDistance   = CONTROLS_MIN_DIST;
    this.controls.maxDistance   = CONTROLS_MAX_DIST;
    this.controls.target.set(CAM_TARGET.x, CAM_TARGET.y, CAM_TARGET.z);

    // Ground grid — two-tone for a subtle near/far contrast
    const grid = new THREE.GridHelper(GRID_SIZE, GRID_DIVISIONS, COL_GRID_CTR, COL_GRID_MAIN);
    grid.position.y = GROUND_Y;
    this.scene.add(grid);

    addEventListener('resize', () => {
      this.camera.aspect = innerWidth / innerHeight;
      this.camera.updateProjectionMatrix();
      this.renderer.setSize(innerWidth, innerHeight);
    });
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Axes & labels
     Three clearly colour-coded axes with tick marks and high-contrast titles.
     ───────────────────────────────────────────────────────────────────────── */
  _buildAxes() {
    const L   = AXES_LENGTH;            // 120
    const gy  = GROUND_Y + 2;           // -26  (floor of axis system)
    const hw  = X_AXIS_WIDTH / 2;       // 110  (half of data width)
    const zEnd = -(this.SLICES - 1) * Z_SLICE_SPACING;  // ≈ -93

    // ── X axis — Phase (warm amber) ──────────────────────────────────────
    this.scene.add(axisLine(
        new THREE.Vector3(-L, gy, 0), new THREE.Vector3(L, gy, 0), COL_AXIS_X
    ));

    // Phase tick marks and labels: 0, π/2, π, 3π/2, 2π
    const phaseStops = [
        { frac: 0.00, text: '0' },
        { frac: 0.25, text: 'π/2' },
        { frac: 0.50, text: 'π' },
        { frac: 0.75, text: '3π/2' },
        { frac: 1.00, text: '2π' },
    ];
    for (const { frac, text } of phaseStops) {
        const x = -hw + frac * X_AXIS_WIDTH;
        // Tick — small vertical mark at floor level
        this.scene.add(axisLine(
            new THREE.Vector3(x, gy - 3, 0),
            new THREE.Vector3(x, gy + 3, 0),
            COL_AXIS_X, 0.70
        ));
        // Label below the tick
        const tl = makeLabel(text, HEX_AXIS_X, {
            size: 64, width: 256, height: 88, scaleX: 7.0, scaleY: 1.50,
        });
        tl.position.set(x, gy - 12, 0);
        this.scene.add(tl);
    }

    // Axis title — right of the line
    const xl = makeLabel('Phase →', HEX_AXIS_X, {
        size: 64, width: 600, height: 100, scaleX: 13.0, scaleY: 2.10,
    });
    xl.position.set(L + 28, gy, 0);
    this.scene.add(xl);

    // ── Y axis — Amplitude (electric cyan) ───────────────────────────────
    this.scene.add(axisLine(
        new THREE.Vector3(-L, gy, 0), new THREE.Vector3(-L, AMP_SCALE + 6, 0), COL_AXIS_Y
    ));

    // Amplitude ticks at ±1, ±0.5, 0
    for (const v of [-1, -0.5, 0, 0.5, 1]) {
        const y = v * AMP_SCALE;
        this.scene.add(axisLine(
            new THREE.Vector3(-L - 3, y, 0),
            new THREE.Vector3(-L + 3, y, 0),
            COL_AXIS_Y, 0.72
        ));
        const tl = makeLabel(v === 0 ? '0' : v.toFixed(1), '#9090b8', {
            size: 60, width: 220, height: 80, scaleX: 5.5, scaleY: 1.10,
        });
        tl.position.set(-L - 18, y, 0);
        this.scene.add(tl);
    }

    // Axis title — above the line
    const yl = makeLabel('Amplitude ↑', HEX_AXIS_Y, {
        size: 64, width: 660, height: 100, scaleX: 13.5, scaleY: 2.10,
    });
    yl.position.set(-L, AMP_SCALE + 22, 0);
    this.scene.add(yl);

    // ── Z axis — Depth (soft violet) ─────────────────────────────────────
    this.scene.add(axisLine(
        new THREE.Vector3(-L, gy, 0), new THREE.Vector3(-L, gy, zEnd - 8), COL_AXIS_Z
    ));

    // Depth ticks every 8 slices
    for (let s = 8; s <= this.SLICES; s += 8) {
        const z = -s * Z_SLICE_SPACING;
        this.scene.add(axisLine(
            new THREE.Vector3(-L - 3, gy, z),
            new THREE.Vector3(-L + 3, gy, z),
            COL_AXIS_Z, 0.65
        ));
        const tl = makeLabel(`${s}`, '#7070a0', {
            size: 56, width: 160, height: 80, scaleX: 5.0, scaleY: 1.00,
        });
        tl.position.set(-L - 16, gy, z);
        this.scene.add(tl);
    }

    // Axis title — at the far end
    const zl = makeLabel('Depth →', HEX_AXIS_Z, {
        size: 64, width: 600, height: 100, scaleX: 13.0, scaleY: 2.10,
    });
    zl.position.set(-L - 4, gy, zEnd - 28);
    this.scene.add(zl);
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Reference grid & depth guides
     Subtle non-intrusive geometry that provides spatial orientation without
     competing with the waveform ribbon.
     ───────────────────────────────────────────────────────────────────────── */
  _buildReferenceGrid() {
    const hw   = X_AXIS_WIDTH / 2;
    const zEnd = -(this.SLICES - 1) * Z_SLICE_SPACING;   // ≈ -93

    this.zeroRefGrp = new THREE.Group();

    // ── 1. Zero-amplitude front edge line ─────────────────────────────────
    // The most important spatial reference — shows where silence lives.
    this.zeroRefGrp.add(axisLine(
        new THREE.Vector3(-hw - 6, 0, 0),
        new THREE.Vector3( hw + 6, 0, 0),
        COL_ZERO_REF, 0.88
    ));

    // ── 2. Amplitude boundary lines at ±1 (front face) ───────────────────
    for (const v of [-1, 1]) {
        const y = v * AMP_SCALE;
        this.zeroRefGrp.add(axisLine(
            new THREE.Vector3(-hw - 3, y, 0),
            new THREE.Vector3( hw + 3, y, 0),
            COL_AMP_BOUNDS, 0.60
        ));
    }

    // ── 3. Phase midpoint guide — vertical at phase = π (x = 0) ─────────
    this.zeroRefGrp.add(axisLine(
        new THREE.Vector3(0, -(AMP_SCALE + 3), 0),
        new THREE.Vector3(0,   AMP_SCALE + 3,  0),
        COL_PHASE_MID, 0.50
    ));

    // ── 4. Z-direction floor runners at left / right data edges ──────────
    for (const x of [-hw, hw]) {
        this.zeroRefGrp.add(axisLine(
            new THREE.Vector3(x, GROUND_Y + 1,  0),
            new THREE.Vector3(x, GROUND_Y + 1, zEnd),
            COL_GRID_CTR, 0.55
        ));
    }

    // ── 5. Zero-amplitude horizontal plane (full wavetable depth) ─────────
    // Semi-transparent mesh provides a clear "floor" for the waveform,
    // making depth and phase simultaneously readable.
    const planeGeo = new THREE.PlaneGeometry(X_AXIS_WIDTH + 24, Math.abs(zEnd) + 16);
    const planeMat = new THREE.MeshBasicMaterial({
        color: 0x0e1520, transparent: true, opacity: 0.22,
        side: THREE.DoubleSide, depthWrite: false,
    });
    const plane = new THREE.Mesh(planeGeo, planeMat);
    plane.rotation.x  = Math.PI / 2;
    plane.position.set(0, 0, zEnd / 2);
    this.zeroRefGrp.add(plane);

    this.scene.add(this.zeroRefGrp);
    this.legendTargets['zero-ref'] = this.zeroRefGrp;
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Wavetable ribbon surface
     ───────────────────────────────────────────────────────────────────────── */
  _buildWavetable() {
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
            const k    = s * (p - 1) * 2 + i * 2;
            idx[k]     = base + i;
            idx[k + 1] = base + i + 1;
        }
    }

    geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    geo.setAttribute('color',    new THREE.BufferAttribute(col, 3));
    geo.setIndex(new THREE.BufferAttribute(idx, 1));

    const mat = new THREE.LineBasicMaterial({
        vertexColors: true, transparent: true, opacity: 0.92, depthWrite: false,
    });

    this.ribbon = new THREE.LineSegments(geo, mat);
    this.scene.add(this.ribbon);
    this.legendTargets['waveform'] = this.ribbon;
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Decay envelope curve
     ───────────────────────────────────────────────────────────────────────── */
  _buildDecayEnvelope() {
    const n   = this.SLICES;
    const geo = new THREE.BufferGeometry();
    const pos = new Float32Array(n * 3);
    const idx = new Uint32Array((n - 1) * 2);
    for (let i = 0; i < n - 1; i++) { idx[i * 2] = i; idx[i * 2 + 1] = i + 1; }

    geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    geo.setIndex(new THREE.BufferAttribute(idx, 1));

    const mat = new THREE.LineBasicMaterial({
        color: COL_DECAY_LINE, transparent: true, opacity: 0.78, depthWrite: false,
    });

    this.decayLine = new THREE.LineSegments(geo, mat);
    this.scene.add(this.decayLine);
    this._updateDecayEnvelope();
    this.legendTargets['decay'] = this.decayLine;
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Peak-amplitude callout
     A bright dot + drop-line + value label marks the highest sample in the
     front slice, providing a direct data-point annotation.
     ───────────────────────────────────────────────────────────────────────── */
  _buildPeakCallout() {
    this.peakMarker = new THREE.Group();

    // Bright sphere at the peak point
    const dot = new THREE.Mesh(
        new THREE.SphereGeometry(1.8, 10, 7),
        new THREE.MeshBasicMaterial({ color: COL_PEAK_DOT, opacity: 0.94, transparent: true })
    );
    dot.name = 'peak-dot';
    this.peakMarker.add(dot);

    // Soft halo ring for visibility
    const haloGeo = new THREE.RingGeometry(2.4, 3.4, 24);
    const haloMat = new THREE.MeshBasicMaterial({
        color: COL_PEAK_DOT, opacity: 0.28, transparent: true,
        side: THREE.DoubleSide, depthWrite: false,
    });
    const halo = new THREE.Mesh(haloGeo, haloMat);
    halo.name = 'peak-halo';
    this.peakMarker.add(halo);

    // Vertical drop-line from the peak down to the floor
    const dropGeo = new THREE.BufferGeometry().setFromPoints([
        new THREE.Vector3(0,   0, 0),
        new THREE.Vector3(0, -60, 0),  // placeholder; updated by _updatePeakCallout
    ]);
    const dropLine = new THREE.Line(dropGeo,
        new THREE.LineBasicMaterial({ color: COL_PEAK_DOT, opacity: 0.20, transparent: true })
    );
    dropLine.name = 'peak-dropline';
    this.peakMarker.add(dropLine);

    // Value label floating above the marker
    const lbl = makeLabel('—', '#ffffff', {
        size: 52, width: 288, height: 80, scaleX: 5.0, scaleY: 1.00,
    });
    lbl.position.set(0, 14, 0);
    lbl.name = 'peak-label';
    this.peakMarker.add(lbl);

    this.peakMarker.visible = false;  // hidden until first Wasm render
    this.scene.add(this.peakMarker);
    this.legendTargets['peak'] = this.peakMarker;
  }

  /** Reposition and relabel the peak callout based on the current front slice. */
  _updatePeakCallout() {
    if (!this.peakMarker || !this.vizBufView) return;

    const p = this.PTS;
    let peakAmp = 0;
    let peakIdx = 0;
    for (let i = 0; i < p; i++) {
        const a = this.vizBufView[i];   // front slice only (s = 0)
        if (Math.abs(a) > Math.abs(peakAmp)) { peakAmp = a; peakIdx = i; }
    }

    const worldX = (peakIdx / p - 0.5) * X_AXIS_WIDTH;
    const worldY = peakAmp * AMP_SCALE;

    this.peakMarker.position.set(worldX, worldY, 0);
    this.peakMarker.visible = true;

    // Redraw value label
    const lbl = this.peakMarker.getObjectByName('peak-label');
    if (lbl) {
        const sign = peakAmp >= 0 ? '+' : '';
        updateLabel(lbl, `${sign}${peakAmp.toFixed(3)}`, '#e8e8f8', 52);
    }

    // Update drop-line endpoint so it always touches the floor
    const drop = this.peakMarker.getObjectByName('peak-dropline');
    if (drop) {
        const arr = drop.geometry.attributes.position.array;
        arr[3] = 0;  arr[4] = GROUND_Y - worldY;  arr[5] = 0;
        drop.geometry.attributes.position.needsUpdate = true;
    }
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Wavetable rebuild — dispatcher
     ───────────────────────────────────────────────────────────────────────── */
  /**
   * Dispatch to the appropriate rendering path:
   *   Live audio mode  → ribbon updated per-frame by _updateRibbonFromAudio()
   *   Wasm viz ready   → viz_render_wavetable() batch call (primary path)
   *   Fallback         → pure JS Math.sin / naive waveforms
   */
  _rebuildWaveData() {
    if (this.liveAudioMode) {
        this._updateDecayEnvelope();
        return;
    }
    if (this.vizWasmReady) {
        this._rebuildWaveDataWasm();
    } else {
        this._rebuildWaveDataJS();
    }
    this._updateDecayEnvelope();
  }

  /**
   * Wasm batch path.
   *
   * Calls viz_render_wavetable() — compiled at O3, uses the 4096-entry
   * SINE_LUT and PolyBLEP anti-aliasing, numerically identical to the
   * audio-thread output.  Result is read zero-copy from vizBufView.
   *
   * Colour: teal front → indigo back, brightness ∝ |amplitude|.
   */
  _rebuildWaveDataWasm() {
    // Re-create view if Wasm memory grew (rare)
    if (this.vizBufView.buffer !== this.vizWasm.memory.buffer) {
        const ptr = this.vizWasm.get_viz_buffer();
        this.vizBufView = new Float32Array(this.vizWasm.memory.buffer, ptr, this.SLICES * this.PTS);
    }

    const t0     = performance.now();
    const result = this.vizWasm.viz_render_wavetable(
        this.SLICES, this.PTS, this.wave, this.harmonics, this.decay / 100
    );
    this.vizRenderMs = performance.now() - t0;

    if (result !== 0) {
        console.error('[SonicForge] viz_render_wavetable error:', result);
        this._rebuildWaveDataJS();
        return;
    }

    const pos = this.ribbon.geometry.attributes.position.array;
    const col = this.ribbon.geometry.attributes.color.array;
    const n   = this.SLICES;
    const p   = this.PTS;

    for (let s = 0; s < n; s++) {
        const zOff = -s * Z_SLICE_SPACING;
        const base = s * p;
        const t    = s / n;

        // Prepare the JS SVF for this slice (reset state between slices so
        // each one is processed independently, matching the viz buffer layout).
        if (this.filterEnabled) {
            this._vizSVF.prepare(this.filterCutoff, this.filterResonance, p);
            this._vizSVF.reset();
        }

        for (let i = 0; i < p; i++) {
            const v   = base + i;
            let   amp = this.vizBufView[v];

            // Apply JS filter approximation to the visualisation
            if (this.filterEnabled) {
                amp = this._vizSVF.process(amp, this.filterMode);
            }

            // Apply JS waveshaper approximation to the visualisation
            if (this.shaperEnabled) {
                amp = vizApplyWaveshaper(amp, this.shaperShape, this.shaperDrive);
            }

            const x = (i / p - 0.5) * X_AXIS_WIDTH;
            pos[v * 3]     = x;
            pos[v * 3 + 1] = amp * AMP_SCALE;
            pos[v * 3 + 2] = zOff;

            applyRibbonColor(col, v, t, amp);
        }
    }

    this.ribbon.geometry.attributes.position.needsUpdate = true;
    this.ribbon.geometry.attributes.color.needsUpdate    = true;

    if ($('r-render-ms')) $('r-render-ms').textContent = `${this.vizRenderMs.toFixed(2)} ms`;

    this._updatePeakCallout();
  }

  /**
   * Pure-JS fallback — used only when the Wasm module is unavailable.
   * No PolyBLEP or LUT; kept so the visualisation degrades gracefully.
   */
  _rebuildWaveDataJS() {
    const n        = this.SLICES;
    const p        = this.PTS;
    const decayAmt = this.decay / 100;
    const pos      = this.ribbon.geometry.attributes.position.array;
    const col      = this.ribbon.geometry.attributes.color.array;

    for (let s = 0; s < n; s++) {
        const decayFactor = 1.0 - (s / n) * decayAmt;
        const zOff        = -s * Z_SLICE_SPACING;

        if (this.filterEnabled) {
            this._vizSVF.prepare(this.filterCutoff, this.filterResonance, p);
            this._vizSVF.reset();
        }

        for (let i = 0; i < p; i++) {
            const phase = (i / p) * 2 * Math.PI;
            let amp = this._waveSampleJS(phase);
            for (let h = 2; h <= this.harmonics; h++) {
                amp += this._waveSampleJS(phase * h) / h;
            }
            amp *= decayFactor;

            if (this.filterEnabled) {
                amp = this._vizSVF.process(amp, this.filterMode);
            }
            if (this.shaperEnabled) {
                amp = vizApplyWaveshaper(amp, this.shaperShape, this.shaperDrive);
            }

            const v = s * p + i;
            const x = (i / p - 0.5) * X_AXIS_WIDTH;

            pos[v * 3]     = x;
            pos[v * 3 + 1] = amp * AMP_SCALE;
            pos[v * 3 + 2] = zOff;

            applyRibbonColor(col, v, s / n, amp);
        }
    }

    this.ribbon.geometry.attributes.position.needsUpdate = true;
    this.ribbon.geometry.attributes.color.needsUpdate    = true;
  }

  /** JS waveform evaluator — fallback only; no anti-aliasing. */
  _waveSampleJS(phase) {
    const p = ((phase % (2 * Math.PI)) + 2 * Math.PI) % (2 * Math.PI);
    switch (this.wave) {
      case 0: return Math.sin(p);
      case 1: return 2 * (p / (2 * Math.PI)) - 1;
      case 2: return p < Math.PI ? 1 : -1;
      case 3: return (p < Math.PI ? 4 * p / (2 * Math.PI) - 1 : 3 - 4 * p / (2 * Math.PI));
      default: return 0;
    }
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Decay envelope
     ───────────────────────────────────────────────────────────────────────── */
  _updateDecayEnvelope() {
    if (!this.decayLine) return;
    const n   = this.SLICES;
    const pos = this.decayLine.geometry.attributes.position.array;
    const d   = this.decay / 100;
    for (let s = 0; s < n; s++) {
        const f        = 1.0 - (s / n) * d;
        pos[s * 3]     = -X_AXIS_WIDTH / 2;
        pos[s * 3 + 1] = f * AMP_SCALE;
        pos[s * 3 + 2] = -s * Z_SLICE_SPACING;
    }
    this.decayLine.geometry.attributes.position.needsUpdate = true;
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Live audio ribbon — SharedArrayBuffer path
     ───────────────────────────────────────────────────────────────────────── */
  /**
   * Stream real DSP samples from the AudioWorklet ring into the ribbon.
   *
   * Ring-priming guard: the first SLICES×PTS samples are skipped to avoid
   * showing a partially zero ribbon that "jumps" when real samples arrive.
   */
  _updateRibbonFromAudio() {
    if (!this.ringData || !this.ringCtrl) return;

    const wp = Atomics.load(this.ringCtrl, 0);
    if (wp < this.SLICES * this.PTS) return;

    const pos      = this.ribbon.geometry.attributes.position.array;
    const col      = this.ribbon.geometry.attributes.color.array;
    const n        = this.SLICES;
    const p        = this.PTS;
    const decayAmt = this.decay / 100;

    for (let s = 0; s < n; s++) {
        const decayFactor = 1.0 - (s / n) * decayAmt;
        const zOff        = -s * Z_SLICE_SPACING;
        const sliceEnd    = wp - s * p;

        for (let i = 0; i < p; i++) {
            const ringIdx = (sliceEnd - p + i) & RING_MASK;
            const amp     = this.ringData[ringIdx] * decayFactor;
            const x       = (i / p - 0.5) * X_AXIS_WIDTH;
            const v       = s * p + i;

            pos[v * 3]     = x;
            pos[v * 3 + 1] = amp * AMP_SCALE;
            pos[v * 3 + 2] = zOff;

            applyRibbonColor(col, v, s / n, amp);
        }
    }

    this.ribbon.geometry.attributes.position.needsUpdate = true;
    this.ribbon.geometry.attributes.color.needsUpdate    = true;
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Live mode toggle
     ───────────────────────────────────────────────────────────────────────── */
  _setLiveMode(enable) {
    if (enable && !this.sabAvailable) return;
    this.liveAudioMode = enable;

    // Peak callout is meaningless in live oscilloscope mode
    if (this.peakMarker) this.peakMarker.visible = !enable;

    const liveBtn = $('live-btn');
    if (liveBtn) {
        liveBtn.classList.toggle('active', enable);
        liveBtn.textContent = enable ? 'Live: On' : 'Live: Off';
    }

    if ($('r-dsp-source')) {
        $('r-dsp-source').textContent = enable
            ? 'Live Audio (SAB)'
            : (this.vizWasmReady ? 'Wasm (LUT + PolyBLEP)' : 'JS (fallback)');
    }

    if (!enable) this._rebuildWaveData();
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Camera reset
     ───────────────────────────────────────────────────────────────────────── */
  _resetCamera() {
    this.camera.position.set(CAM_POS.x, CAM_POS.y, CAM_POS.z);
    this.controls.target.set(CAM_TARGET.x, CAM_TARGET.y, CAM_TARGET.z);
    this.controls.update();
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Audio bootstrap
     ───────────────────────────────────────────────────────────────────────── */
  async _startAudio() {
    if (this.started) return;
    const btn = $('start-btn');
    btn.textContent = 'Loading…';
    btn.disabled    = true;

    try {
      this.audioCtx = new AudioContext();
      this.gainNode = this.audioCtx.createGain();
      this.gainNode.gain.value = this.muted ? 0 : 1;

      // Attempt SAB ring allocation — silently degrades if COOP/COEP absent.
      try {
        this.ringBuffer   = new SharedArrayBuffer(4 + RING_SAMPLES * 4);
        this.ringCtrl     = new Int32Array(this.ringBuffer, 0, 1);
        this.ringData     = new Float32Array(this.ringBuffer, 4);
        this.sabAvailable = true;
      } catch (_e) {
        this.sabAvailable = false;
        const lb = $('live-btn');
        if (lb) {
            lb.disabled = true;
            lb.title    = 'Live mode requires COOP/COEP headers. Use scripts/serve-web.py.';
        }
      }

      const resp = await fetch('sonicforge.wasm');
      if (!resp.ok) throw new Error(`Wasm fetch failed (${resp.status})`);
      const wasm = await resp.arrayBuffer();

      await this.audioCtx.audioWorklet.addModule('sonicforge-processor.js');
      this.worklet = new AudioWorkletNode(this.audioCtx, 'sonicforge-processor', {
          outputChannelCount: [1],
      });

      this.worklet.port.postMessage({ type: 'init-wasm', wasmBinary: wasm }, [wasm]);

      this.worklet.port.onmessage = ({ data }) => {
        if (data.type === 'ready') {
          this.worklet.port.postMessage({
            type: 'init',
            waveform:   this.wave,
            frequency:  this.freq,
            harmonics:  this.harmonics,
          });

          // Sync current FX state to the freshly initialised worklet.
          if (this.glideMs > 0)
              this.worklet.port.postMessage({ type: 'fx-glide', ms: this.glideMs });
          this.worklet.port.postMessage({
              type: 'fx-filter',
              enabled:   this.filterEnabled,
              mode:      this.filterMode,
              cutoff:    this.filterCutoff,
              resonance: this.filterResonance,
          });
          this.worklet.port.postMessage({
              type: 'fx-shaper',
              enabled: this.shaperEnabled,
              shape:   this.shaperShape,
              drive:   this.shaperDrive,
          });
          this.worklet.port.postMessage({
              type: 'fx-delay',
              enabled:  this.delayEnabled,
              time_ms:  this.delayTimeMs,
              feedback: this.delayFeedback,
          });

          if (this.sabAvailable) {
              this.worklet.port.postMessage({ type: 'init-sab', ringBuffer: this.ringBuffer });
              const lb = $('live-btn');
              if (lb) lb.style.display = 'inline-block';
          }

          btn.textContent = 'Audio Running';
          btn.disabled    = false;
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
    btn.disabled    = false;
    btn.onclick = () => { btn.onclick = null; this._startAudio(); };
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Audio stop
     ───────────────────────────────────────────────────────────────────────── */
  _stopAudio() {
    if (!this.audioCtx) return;

    // Restore preview mode before destroying the ring so the ribbon is
    // immediately repopulated with the clean Wasm wavetable.
    if (this.liveAudioMode) this._setLiveMode(false);

    this.worklet.disconnect();
    this.gainNode.disconnect();
    this.audioCtx.close();

    this.audioCtx     = null;
    this.gainNode     = null;
    this.worklet      = null;
    this.started      = false;
    this.ringBuffer   = null;
    this.ringCtrl     = null;
    this.ringData     = null;
    this.sabAvailable = false;

    $('start-btn').textContent   = 'Start Audio';
    $('start-btn').style.display = 'inline-block';
    $('stop-btn').style.display  = 'none';
    $('mute-btn').style.display  = 'none';
    $('mute-btn').classList.remove('muted');

    const lb = $('live-btn');
    if (lb) lb.style.display = 'none';
    this.muted = false;
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Mute toggle
     ───────────────────────────────────────────────────────────────────────── */
  _toggleMute() {
    if (!this.gainNode) return;
    this.muted = !this.muted;
    this.gainNode.gain.value = this.muted ? 0 : 1;
    const btn = $('mute-btn');
    btn.classList.toggle('muted', this.muted);
    btn.innerHTML = this.muted ? '&#128263;' : '&#128266;';
  }

  /* ─────────────────────────────────────────────────────────────────────────
     UI wiring
     ───────────────────────────────────────────────────────────────────────── */
  _wireUI() {
    $('start-btn').addEventListener('click', () => this._startAudio());
    $('stop-btn').addEventListener('click',  () => this._stopAudio());
    $('mute-btn').addEventListener('click',  () => this._toggleMute());

    const liveBtn = $('live-btn');
    if (liveBtn) liveBtn.addEventListener('click', () => this._setLiveMode(!this.liveAudioMode));

    const resetBtn = $('reset-btn');
    if (resetBtn) resetBtn.addEventListener('click', () => this._resetCamera());

    $('freq-slider').addEventListener('input', (e) => {
      const freq = clampFinite(e.target.value, 40, 1200, 440);
      this.freq = freq;
      $('freq-val').textContent = `${freq} Hz`;
      $('r-freq').textContent   = `${freq} Hz`;
      if (this.worklet) this.worklet.port.postMessage({ type: 'frequency', value: freq });
    });

    $('decay-slider').addEventListener('input', (e) => {
      const decay = clampFinite(e.target.value, 0, 100, 0);
      this.decay  = decay;
      $('decay-val').textContent = `${decay}%`;
      $('r-decay').textContent   = `${decay}%`;
      this._rebuildWaveData();
    });

    $('harm-slider').addEventListener('input', (e) => {
      const harm = Math.round(clampFinite(e.target.value, 1, 16, 1));
      this.harmonics = harm;
      $('harm-val').textContent = harm;
      if ($('r-harm')) $('r-harm').textContent = harm;
      // Update audio engine — additive synthesis adds/removes oscillators
      if (this.worklet) this.worklet.port.postMessage({ type: 'harmonics', value: harm });
      this._rebuildWaveData();
    });

    document.querySelectorAll('.wbtn').forEach((btn) => {
      btn.addEventListener('click', () => {
        const wave = clampFinite(Number(btn.dataset.wave), 0, 3, 0);
        this.wave  = Math.round(wave);
        document.querySelectorAll('.wbtn').forEach((b) => b.classList.remove('active'));
        btn.classList.add('active');
        $('r-wave').textContent = ['Sine', 'Saw', 'Square', 'Triangle'][this.wave];
        if (this.worklet) this.worklet.port.postMessage({ type: 'waveform', value: this.wave });
        this._rebuildWaveData();
      });
    });

    // ── Interactive legend ────────────────────────────────────────────────
    // Each .item[data-target] click toggles the visibility of its 3D object.
    document.querySelectorAll('#legend .item[data-target]').forEach((item) => {
      item.addEventListener('click', () => {
        const key = item.dataset.target;
        const obj = this.legendTargets[key];
        if (!obj) return;
        obj.visible = !obj.visible;
        item.classList.toggle('off', !obj.visible);
      });
    });

    // ── Keyboard shortcuts ────────────────────────────────────────────────
    window.addEventListener('keydown', (e) => {
      if ((e.key === 'r' || e.key === 'R') && !e.ctrlKey && !e.metaKey) {
        this._resetCamera();
      }
    });

    // ── FX Chain wiring ───────────────────────────────────────────────────

    // Glide (SmoothedValue)
    $('glide-slider').addEventListener('input', (e) => {
        const ms = Number(e.target.value);
        this.glideMs = ms;
        $('glide-val').textContent = `${ms} ms`;
        if (this.worklet) this.worklet.port.postMessage({ type: 'fx-glide', ms });
    });

    // Filter — LED/header click toggles enable
    $('filter-header').addEventListener('click', () => {
        this._toggleFXSection('filter-section', 'filter-led', (on) => {
            this.filterEnabled = on;
            this._updateReadoutFX();
            if (this.worklet) this.worklet.port.postMessage({ type: 'fx-filter', enabled: on });
            this._rebuildWaveData();
        });
    });

    document.querySelectorAll('[data-filter-mode]').forEach((btn) => {
        btn.addEventListener('click', () => {
            const mode = Number(btn.dataset.filterMode);
            this.filterMode = mode;
            btn.closest('.fx-mode-btns').querySelectorAll('.wbtn')
               .forEach((b) => b.classList.remove('active'));
            btn.classList.add('active');
            this._updateReadoutFX();
            if (this.worklet) this.worklet.port.postMessage({ type: 'fx-filter', mode });
            this._rebuildWaveData();
        });
    });

    $('filter-cutoff').addEventListener('input', (e) => {
        const hz = sliderToCutoffHz(Number(e.target.value));
        this.filterCutoff = hz;
        $('filter-cutoff-val').textContent = formatCutoffHz(hz);
        this._updateReadoutFX();
        if (this.worklet) this.worklet.port.postMessage({ type: 'fx-filter', cutoff: hz });
        this._rebuildWaveData();
    });

    $('filter-res').addEventListener('input', (e) => {
        const res = Number(e.target.value) / 100;
        this.filterResonance = res;
        $('filter-res-val').textContent = res.toFixed(2);
        if (this.worklet) this.worklet.port.postMessage({ type: 'fx-filter', resonance: res });
        this._rebuildWaveData();
    });

    // Waveshaper — LED/header click toggles enable
    $('shaper-header').addEventListener('click', () => {
        this._toggleFXSection('shaper-section', 'shaper-led', (on) => {
            this.shaperEnabled = on;
            this._updateReadoutFX();
            if (this.worklet) this.worklet.port.postMessage({ type: 'fx-shaper', enabled: on });
            this._rebuildWaveData();
        });
    });

    document.querySelectorAll('[data-shaper-shape]').forEach((btn) => {
        btn.addEventListener('click', () => {
            const shape = Number(btn.dataset.shaperShape);
            this.shaperShape = shape;
            btn.closest('.fx-mode-btns').querySelectorAll('.wbtn')
               .forEach((b) => b.classList.remove('active'));
            btn.classList.add('active');
            this._updateReadoutFX();
            if (this.worklet) this.worklet.port.postMessage({ type: 'fx-shaper', shape });
            this._rebuildWaveData();
        });
    });

    $('shaper-drive').addEventListener('input', (e) => {
        const drive = Number(e.target.value) / 10;
        this.shaperDrive = drive;
        $('shaper-drive-val').textContent = `${drive.toFixed(1)}×`;
        this._updateReadoutFX();
        if (this.worklet) this.worklet.port.postMessage({ type: 'fx-shaper', drive });
        this._rebuildWaveData();
    });

    // Delay — LED/header click toggles enable
    $('delay-header').addEventListener('click', () => {
        this._toggleFXSection('delay-section', 'delay-led', (on) => {
            this.delayEnabled = on;
            this._updateReadoutFX();
            if (this.worklet) this.worklet.port.postMessage({ type: 'fx-delay', enabled: on });
        });
    });

    $('delay-time').addEventListener('input', (e) => {
        const ms = Number(e.target.value);
        this.delayTimeMs = ms;
        $('delay-time-val').textContent = `${ms} ms`;
        this._updateReadoutFX();
        if (this.worklet) this.worklet.port.postMessage({ type: 'fx-delay', time_ms: ms });
    });

    $('delay-fb').addEventListener('input', (e) => {
        const fb = Number(e.target.value) / 100;
        this.delayFeedback = fb;
        $('delay-fb-val').textContent = `${e.target.value}%`;
        if (this.worklet) this.worklet.port.postMessage({ type: 'fx-delay', feedback: fb });
    });
  }

  /* ─────────────────────────────────────────────────────────────────────────
     FX helpers
     ───────────────────────────────────────────────────────────────────────── */

  /**
   * Toggle a FX section on/off.
   * @param {string}   sectionId  Element id of the .fx-section div
   * @param {string}   ledId      Element id of the .fx-led dot
   * @param {function} cb         Called with (boolean on) after state change
   */
  _toggleFXSection(sectionId, ledId, cb) {
    const section = $(sectionId);
    const led     = $(ledId);
    const on      = !section.classList.contains('enabled');
    section.classList.toggle('enabled', on);
    led.classList.toggle('active', on);
    cb(on);
  }

  /** Update the top-left readout rows for filter, shaper, and delay. */
  _updateReadoutFX() {
    const rFilter = $('r-filter');
    const rShaper = $('r-shaper');
    const rDelay  = $('r-delay');

    if (rFilter) {
        if (!this.filterEnabled) {
            rFilter.textContent = 'off';
        } else {
            const modeNames = ['LP', 'HP', 'BP', 'Notch'];
            rFilter.textContent = `${modeNames[this.filterMode]} ${formatCutoffHz(this.filterCutoff)}`;
        }
    }

    if (rShaper) {
        if (!this.shaperEnabled) {
            rShaper.textContent = 'off';
        } else {
            const shapeNames = ['Tanh', 'Poly', 'Hard', 'Fold'];
            rShaper.textContent = `${shapeNames[this.shaperShape]} ${this.shaperDrive.toFixed(1)}×`;
        }
    }

    if (rDelay) {
        rDelay.textContent = this.delayEnabled
            ? `${this.delayTimeMs} ms / ${Math.round(this.delayFeedback * 100)}%`
            : 'off';
    }
  }

  /* ─────────────────────────────────────────────────────────────────────────
     Render loop
     ───────────────────────────────────────────────────────────────────────── */
  _tick() {
    requestAnimationFrame(() => this._tick());
    this.controls.update();

    if (this.liveAudioMode && this.sabAvailable && this.started) {
        this._updateRibbonFromAudio();
    }

    this.renderer.render(this.scene, this.camera);
  }
}

const app = new SonicForgeApp();
app.init();

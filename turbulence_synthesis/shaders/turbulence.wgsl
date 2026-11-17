// Turbulence synthesis on the GPU (spec 004): one thread per froxel of one camera.
// Same formulas as the CPU reference ff::detail::TurbulenceSynthesis::velocity (spec 003):
//   u(x) = sum_{i=1..layers} curl( A(x) psi_i(x) ) = sum_i A curl(psi_i) + grad(A) x psi_i
//   psi_i = (a_i / f_i) * (N(f_i x + o_i0), N(f_i x + o_i1), N(f_i x + o_i2))
//   A(x)  = strength * h * |omega|(x) * ramp(y / d0)        (|omega|: quadratic B-spline, C1)
// Output per froxel: vec4(u, layers), index i + nx * (j + ny * k) (contracts/froxel-detail-buffer.md).
// Precision (research R3): f_i is a power of two over h, so f_i * x is exact in f32; it is split into
// an integer lattice cell and a fraction, and the layer offsets come pre-split from the CPU.
// Obstacles (spec 009): within obstacleFade / f_i of an obstacle, layer i is the central-difference
// curl of the slip-constrained potential C(A psi_i) (Bridson eq. 5, signed ramp), evaluated with split
// coordinates (x0 + d); farther away the analytic path above is unchanged. Zero inside obstacles.
// Froxel geometry: FroxelUniform and froxel_center come from the froxel grid module (W2,
// froxel_grid/shaders/froxel_mapping.wgsl), prepended by froxel_link::withFroxelGeometry.

struct LayerData {
  freqAmp: vec4<f32>,          // x: f_i, y: a_i, z: a_i / f_i
  cell: array<vec4<i32>, 3>,   // per potential channel: integer part of the noise offset
  frac: array<vec4<f32>, 3>,   // per potential channel: fractional part of the noise offset
}

struct Params {
  froxel: FroxelUniform,       // camera + froxel grid (W2, 160 bytes)
  world: vec4<f32>,           // origin xyz, w: cell size h
  worldN: vec4<u32>,           // x: n (world grid is n^3)
  synth: vec4<f32>,            // x: strength * h, y: wall distance d0, z: obstacleFade
  obstacle: vec4<u32>,         // x: number of obstacle shapes (spec 009)
  layer: array<LayerData, 8>,  // layer i (1-based) at index i - 1
}

// Same layout as shaders/curl_noise.wgsl (specs/006-curl-noise-gpu/contracts/gpu-layout.md).
struct ShapeData {
  kind: vec4<u32>,      // x: 1 sphere, 2 box, 3 container, 4 half-space, 5 grid; y: grid index
  position: vec4<f32>,
  size: vec4<f32>,      // half size or normal; w: radius
  motion: vec4<f32>,    // w: rounding (container)
  angular: vec4<f32>,
}

@group(0) @binding(0) var<uniform> P: Params;
@group(0) @binding(1) var<storage, read> perm: array<u32>;       // 512 entries (GradientNoise::permutation)
@group(0) @binding(2) var<storage, read> vort: array<f32>;       // |omega| per world cell, x fastest
@group(0) @binding(3) var<storage, read> slices: array<vec4<f32>>;  // y: layer count (x, z, w unused)
@group(0) @binding(4) var<storage, read_write> detail: array<vec4<f32>>;
@group(0) @binding(5) var<storage, read> shapes: array<ShapeData>;  // obstacles (spec 009)
@group(0) @binding(6) var<storage, read> grids: array<f32>;         // distance grids: headers + samples

// ------------------------------------------------------------------ gradient noise (detail/noise.cpp)

fn grad_vec(h: u32) -> vec3<f32> {
  switch h {
    case 0u: { return vec3<f32>(1.0, 1.0, 0.0); }
    case 1u: { return vec3<f32>(-1.0, 1.0, 0.0); }
    case 2u: { return vec3<f32>(1.0, -1.0, 0.0); }
    case 3u: { return vec3<f32>(-1.0, -1.0, 0.0); }
    case 4u: { return vec3<f32>(1.0, 0.0, 1.0); }
    case 5u: { return vec3<f32>(-1.0, 0.0, 1.0); }
    case 6u: { return vec3<f32>(1.0, 0.0, -1.0); }
    case 7u: { return vec3<f32>(-1.0, 0.0, -1.0); }
    case 8u: { return vec3<f32>(0.0, 1.0, 1.0); }
    case 9u: { return vec3<f32>(0.0, -1.0, 1.0); }
    case 10u: { return vec3<f32>(0.0, 1.0, -1.0); }
    default: { return vec3<f32>(0.0, -1.0, -1.0); }
  }
}

fn grad_at(c: vec3<i32>) -> vec3<f32> {
  let a = i32(perm[u32(c.x & 255)]);
  let b = i32(perm[u32((a + c.y) & 255)]);
  return grad_vec(perm[u32((b + c.z) & 255)] % 12u);
}

// Value (x) and analytic gradient (yzw) of the noise at lattice point cell + f; f may leave [0, 1).
fn noise(cellIn: vec3<i32>, fIn: vec3<f32>) -> vec4<f32> {
  let fl = floor(fIn);
  let c = cellIn + vec3<i32>(fl);
  let f = fIn - fl;
  let u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  let du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
  let ga = grad_at(c);
  let gb = grad_at(c + vec3<i32>(1, 0, 0));
  let gc = grad_at(c + vec3<i32>(0, 1, 0));
  let gd = grad_at(c + vec3<i32>(1, 1, 0));
  let ge = grad_at(c + vec3<i32>(0, 0, 1));
  let gf = grad_at(c + vec3<i32>(1, 0, 1));
  let gg = grad_at(c + vec3<i32>(0, 1, 1));
  let gh = grad_at(c + vec3<i32>(1, 1, 1));
  let va = dot(ga, f);
  let vb = dot(gb, f - vec3<f32>(1.0, 0.0, 0.0));
  let vc = dot(gc, f - vec3<f32>(0.0, 1.0, 0.0));
  let vd = dot(gd, f - vec3<f32>(1.0, 1.0, 0.0));
  let ve = dot(ge, f - vec3<f32>(0.0, 0.0, 1.0));
  let vf = dot(gf, f - vec3<f32>(1.0, 0.0, 1.0));
  let vg = dot(gg, f - vec3<f32>(0.0, 1.0, 1.0));
  let vh = dot(gh, f - vec3<f32>(1.0, 1.0, 1.0));
  let k1 = vb - va;
  let k2 = vc - va;
  let k3 = ve - va;
  let k4 = va - vb - vc + vd;
  let k5 = va - vc - ve + vg;
  let k6 = va - vb - ve + vf;
  let k7 = -va + vb + vc - vd + ve - vf - vg + vh;
  let value = va + u.x * k1 + u.y * k2 + u.z * k3 + u.x * u.y * k4 + u.y * u.z * k5 + u.z * u.x * k6 +
              u.x * u.y * u.z * k7;
  let grad = ga + u.x * (gb - ga) + u.y * (gc - ga) + u.z * (ge - ga) + u.x * u.y * (ga - gb - gc + gd) +
             u.y * u.z * (ga - gc - ge + gg) + u.z * u.x * (ga - gb - ge + gf) +
             u.x * u.y * u.z * (-ga + gb + gc - gd + ge - gf - gg + gh) +
             du * vec3<f32>(k1 + u.y * k4 + u.z * k6 + u.y * u.z * k7,
                            k2 + u.z * k5 + u.x * k4 + u.z * u.x * k7,
                            k3 + u.x * k6 + u.y * k5 + u.x * u.y * k7);
  return vec4<f32>(value, grad);
}

// ------------------------------------------------------------------ world field (detail/world_field.cpp)

fn inside_world(x: vec3<f32>) -> bool {
  let g = (x - P.world.xyz) / P.world.w;
  let n = f32(P.worldN.x);
  return all(g >= vec3<f32>(0.0)) && all(g <= vec3<f32>(n));
}

// |omega| with a quadratic B-spline: value (x) and gradient in world units (yzw).
fn vorticity_smooth(x: vec3<f32>) -> vec4<f32> {
  let n = i32(P.worldN.x);
  let g = (x - P.world.xyz) / P.world.w - 0.5;
  let cf = floor(g + 0.5);
  let c = vec3<i32>(cf);
  let t = g - cf;
  let w0 = 0.5 * (0.5 - t) * (0.5 - t);
  let w1 = 0.75 - t * t;
  let w2 = 0.5 * (0.5 + t) * (0.5 + t);
  let d0 = -(0.5 - t);
  let d1 = -2.0 * t;
  let d2 = 0.5 + t;
  var value = 0.0;
  var grad = vec3<f32>(0.0);
  for (var dk = 0; dk < 3; dk++) {
    let k = clamp(c.z + dk - 1, 0, n - 1);
    let wk = select(select(w2.z, w1.z, dk == 1), w0.z, dk == 0);
    let gk = select(select(d2.z, d1.z, dk == 1), d0.z, dk == 0);
    for (var dj = 0; dj < 3; dj++) {
      let j = clamp(c.y + dj - 1, 0, n - 1);
      let wj = select(select(w2.y, w1.y, dj == 1), w0.y, dj == 0);
      let gj = select(select(d2.y, d1.y, dj == 1), d0.y, dj == 0);
      for (var di = 0; di < 3; di++) {
        let i = clamp(c.x + di - 1, 0, n - 1);
        let wi = select(select(w2.x, w1.x, di == 1), w0.x, di == 0);
        let gi = select(select(d2.x, d1.x, di == 1), d0.x, di == 0);
        let f = vort[u32(i + n * (j + n * k))];
        value += f * wi * wj * wk;
        grad += f * vec3<f32>(gi * wj * wk, wi * gj * wk, wi * wj * gk);
      }
    }
  }
  return vec4<f32>(value, grad / P.world.w);
}

// ------------------------------------------------------------------ obstacles (spec 009)
// Shape functions copied from shaders/curl_noise.wgsl (spec 006); keep both in sync.

const FAR: f32 = 1e30;
const NONE: u32 = 0xffffffffu;

fn smoothstep01(r: f32) -> f32 {
  let x = clamp(r, 0.0, 1.0);
  return x * x * (3.0 - 2.0 * x);
}

fn ramp_slip(r: f32) -> f32 {
  if (r >= 1.0) { return 1.0; }
  if (r <= -1.0) { return -1.0; }
  let r2 = r * r;
  return r * (15.0 / 8.0 - r2 * (10.0 / 8.0 - r2 * (3.0 / 8.0)));
}

fn grid_sample(g: u32, rel0: vec3<f32>, d: vec3<f32>) -> f32 {
  let h = 12u * g;
  let lo = vec3<f32>(grids[h], grids[h + 1u], grids[h + 2u]);
  let inv = vec3<f32>(grids[h + 3u], grids[h + 4u], grids[h + 5u]);
  let n = vec3<i32>(i32(grids[h + 6u]), i32(grids[h + 7u]), i32(grids[h + 8u]));
  let base = u32(grids[h + 9u]);
  let u0 = (rel0 - lo) * inv - 0.5;
  let cf = floor(u0 + 0.5);
  let t = (u0 - cf) + d * inv;
  let c = vec3<i32>(cf);
  let w0 = 0.5 * (0.5 - t) * (0.5 - t);
  let w1 = 0.75 - t * t;
  let w2 = 0.5 * (0.5 + t) * (0.5 + t);
  var s = 0.0;
  for (var dk = 0; dk < 3; dk++) {
    let k = clamp(c.z + dk - 1, 0, n.z - 1);
    let wk = select(select(w2.z, w1.z, dk == 1), w0.z, dk == 0);
    for (var dj = 0; dj < 3; dj++) {
      let j = clamp(c.y + dj - 1, 0, n.y - 1);
      let wj = select(select(w2.y, w1.y, dj == 1), w0.y, dj == 0);
      for (var di = 0; di < 3; di++) {
        let ii = clamp(c.x + di - 1, 0, n.x - 1);
        let wi = select(select(w2.x, w1.x, di == 1), w0.x, di == 0);
        s += wi * wj * wk * grids[base + u32(ii + n.x * (j + n.y * k))];
      }
    }
  }
  return s;
}

fn grid_gradient(g: u32, rel0: vec3<f32>, d: vec3<f32>) -> vec3<f32> {
  let h = 12u * g;
  let lo = vec3<f32>(grids[h], grids[h + 1u], grids[h + 2u]);
  let inv = vec3<f32>(grids[h + 3u], grids[h + 4u], grids[h + 5u]);
  let n = vec3<i32>(i32(grids[h + 6u]), i32(grids[h + 7u]), i32(grids[h + 8u]));
  let base = u32(grids[h + 9u]);
  let u0 = (rel0 - lo) * inv - 0.5;
  let cf = floor(u0 + 0.5);
  let t = (u0 - cf) + d * inv;
  let c = vec3<i32>(cf);
  let w0 = 0.5 * (0.5 - t) * (0.5 - t);
  let w1 = 0.75 - t * t;
  let w2 = 0.5 * (0.5 + t) * (0.5 + t);
  let g0 = -(0.5 - t);
  let g1 = -2.0 * t;
  let g2 = 0.5 + t;
  var grad = vec3<f32>(0.0);
  for (var dk = 0; dk < 3; dk++) {
    let k = clamp(c.z + dk - 1, 0, n.z - 1);
    let wk = select(select(w2.z, w1.z, dk == 1), w0.z, dk == 0);
    let gk = select(select(g2.z, g1.z, dk == 1), g0.z, dk == 0);
    for (var dj = 0; dj < 3; dj++) {
      let j = clamp(c.y + dj - 1, 0, n.y - 1);
      let wj = select(select(w2.y, w1.y, dj == 1), w0.y, dj == 0);
      let gj = select(select(g2.y, g1.y, dj == 1), g0.y, dj == 0);
      for (var di = 0; di < 3; di++) {
        let ii = clamp(c.x + di - 1, 0, n.x - 1);
        let wi = select(select(w2.x, w1.x, di == 1), w0.x, di == 0);
        let gi = select(select(g2.x, g1.x, di == 1), g0.x, di == 0);
        let v = grids[base + u32(ii + n.x * (j + n.y * k))];
        grad += v * vec3<f32>(gi * wj * wk, wi * gj * wk, wi * wj * gk);
      }
    }
  }
  return grad * inv;
}

fn box_distance(p: vec3<f32>, b: vec3<f32>) -> f32 {
  let q = abs(p) - b;
  return length(max(q, vec3<f32>(0.0))) + min(max(q.x, max(q.y, q.z)), 0.0);
}

fn box_gradient(p: vec3<f32>, b: vec3<f32>) -> vec3<f32> {
  let q = abs(p) - b;
  let sg = select(vec3<f32>(-1.0), vec3<f32>(1.0), p >= vec3<f32>(0.0));
  let m = max(q.x, max(q.y, q.z));
  if (m > 0.0) {
    let o = max(q, vec3<f32>(0.0));
    return sg * o / length(o);
  }
  if (q.x >= q.y && q.x >= q.z) { return vec3<f32>(sg.x, 0.0, 0.0); }
  if (q.y >= q.z) { return vec3<f32>(0.0, sg.y, 0.0); }
  return vec3<f32>(0.0, 0.0, sg.z);
}

fn shape_distance(s: u32, x0: vec3<f32>, d: vec3<f32>) -> f32 {
  let sh = shapes[s];
  let rel = (x0 - sh.position.xyz) + d;
  switch sh.kind.x {
    case 1u: { return length(rel) - sh.size.w; }
    case 2u: { return box_distance(rel, sh.size.xyz); }
    case 3u: { let rc = sh.motion.w; return -(box_distance(rel, sh.size.xyz - vec3<f32>(rc)) - rc); }
    case 4u: { return dot(rel, sh.size.xyz); }
    case 5u: { return grid_sample(sh.kind.y, x0 - sh.position.xyz, d); }
    default: { return FAR; }
  }
}

fn shape_normal(s: u32, x0: vec3<f32>, d: vec3<f32>) -> vec3<f32> {
  let sh = shapes[s];
  let rel = (x0 - sh.position.xyz) + d;
  var g = vec3<f32>(0.0, 0.0, 1.0);
  switch sh.kind.x {
    case 1u: { g = rel; }
    case 2u: { g = box_gradient(rel, sh.size.xyz); }
    case 3u: { g = -box_gradient(rel, sh.size.xyz - vec3<f32>(sh.motion.w)); }
    case 4u: { g = sh.size.xyz; }
    case 5u: { g = grid_gradient(sh.kind.y, x0 - sh.position.xyz, d); }
    default: { }
  }
  let len = length(g);
  if (len > 0.0) { return g / len; }
  return vec3<f32>(0.0, 0.0, 1.0);
}

struct Query {
  dist: f32,
  nearest: u32,
  gap: f32,
}

fn query(x0: vec3<f32>, d: vec3<f32>) -> Query {
  var q = Query(FAR, NONE, FAR);
  var second = FAR;
  for (var s = 0u; s < P.obstacle.x; s++) {
    let ds = shape_distance(s, x0, d);
    if (ds < q.dist) { second = q.dist; q.dist = ds; q.nearest = s; }
    else if (ds < second) { second = ds; }
  }
  q.gap = second - q.dist;
  return q;
}

// Bridson eq. 5 with the signed ramp, blended to eq. 3 near the medial axis (detail/turbulence.cpp).
fn constrain_slip(psi: vec3<f32>, q: Query, x0: vec3<f32>, d: vec3<f32>, width: f32) -> vec3<f32> {
  if (q.nearest == NONE || q.dist >= width) { return psi; }
  let alpha = ramp_slip(q.dist / width);
  let n = shape_normal(q.nearest, x0, d);
  let tangential_fade = alpha * psi + (1.0 - alpha) * n * dot(n, psi);
  let beta = smoothstep01(q.gap / width);
  return beta * tangential_fade + (1.0 - beta) * (alpha * psi);
}

// Layer i near obstacles: u_i = curl C(A psi_i), by central differences with step h = 0.01 / f_i,
// like the CPU reference. The noise is not re-evaluated at the 6 stencil points: Psi = A psi is
// smooth there, so Psi(x +- h e_k) = Psi +- h d_k Psi, with d_k Psi = (d_k A) psi + A d_k psi from
// the analytic noise gradients (3 noise calls, as in the free path; error O(h^2), well below the
// 1e-3 tolerance). Only the obstacle query (cheap) runs at every stencil point.
fn constrained_layer(i: u32, x: vec3<f32>, A0: f32, gradA: vec3<f32>) -> vec3<f32> {
  let L = P.layer[i];
  let f = L.freqAmp.x;
  let fx = f * x;  // exact
  let fc = floor(fx);
  let base = vec3<i32>(fc);
  let fr = fx - fc;
  let n0 = noise(base + L.cell[0].xyz, fr + L.frac[0].xyz);
  let n1 = noise(base + L.cell[1].xyz, fr + L.frac[1].xyz);
  let n2 = noise(base + L.cell[2].xyz, fr + L.frac[2].xyz);
  let psi = L.freqAmp.z * vec3<f32>(n0.x, n1.x, n2.x);
  let a = A0 * L.freqAmp.y;
  let Psi = A0 * psi;
  let dPx = gradA.x * psi + a * vec3<f32>(n0.y, n1.y, n2.y);
  let dPy = gradA.y * psi + a * vec3<f32>(n0.z, n1.z, n2.z);
  let dPz = gradA.z * psi + a * vec3<f32>(n0.w, n1.w, n2.w);

  let width = P.synth.z / f;
  let h = 0.01 / f;
  let s = 1.0 / (2.0 * h);
  let ex = vec3<f32>(h, 0.0, 0.0);
  let ey = vec3<f32>(0.0, h, 0.0);
  let ez = vec3<f32>(0.0, 0.0, h);
  let gx = (constrain_slip(Psi + h * dPx, query(x, ex), x, ex, width)
          - constrain_slip(Psi - h * dPx, query(x, -ex), x, -ex, width)) * s;
  let gy = (constrain_slip(Psi + h * dPy, query(x, ey), x, ey, width)
          - constrain_slip(Psi - h * dPy, query(x, -ey), x, -ey, width)) * s;
  let gz = (constrain_slip(Psi + h * dPz, query(x, ez), x, ez, width)
          - constrain_slip(Psi - h * dPz, query(x, -ez), x, -ez, width)) * s;
  return vec3<f32>(gy.z - gz.y, gz.x - gx.z, gx.y - gy.x);
}

// ------------------------------------------------------------------ the detail velocity

fn detail_velocity(x: vec3<f32>, layers: u32) -> vec3<f32> {
  if (layers == 0u || x.y <= 0.0 || !inside_world(x)) { return vec3<f32>(0.0); }
  var dObstacle = FAR;
  if (P.obstacle.x > 0u) {
    dObstacle = query(x, vec3<f32>(0.0)).dist;
    if (dObstacle < 0.0) { return vec3<f32>(0.0); }  // inside an obstacle (spec 009)
  }
  let w = vorticity_smooth(x);
  // wall ramp in t = y / d0: smoothstep, value and slope zero at the wall (spec 003)
  let d0 = P.synth.y;
  let t = x.y / d0;
  var r = 1.0;
  var dr = 0.0;
  if (t < 1.0) {
    r = t * t * (3.0 - 2.0 * t);
    dr = 6.0 * t * (1.0 - t);
  }
  let c = P.synth.x;
  let A = c * w.x * r;
  let gradA = c * (w.yzw * r + vec3<f32>(0.0, w.x * dr / d0, 0.0));
  if (A == 0.0 && all(gradA == vec3<f32>(0.0))) { return vec3<f32>(0.0); }

  var u = vec3<f32>(0.0);
  for (var i = 0u; i < layers; i++) {
    if (dObstacle < P.synth.z / P.layer[i].freqAmp.x) {  // inside this layer's obstacle zone
      u += constrained_layer(i, x, A, gradA);
      continue;
    }
    let L = P.layer[i];
    let fx = L.freqAmp.x * x;  // exact: f is a power of two over h
    let fc = floor(fx);
    let base = vec3<i32>(fc);
    let fr = fx - fc;
    let n0 = noise(base + L.cell[0].xyz, fr + L.frac[0].xyz);
    let n1 = noise(base + L.cell[1].xyz, fr + L.frac[1].xyz);
    let n2 = noise(base + L.cell[2].xyz, fr + L.frac[2].xyz);
    let psi = L.freqAmp.z * vec3<f32>(n0.x, n1.x, n2.x);
    let a = L.freqAmp.y;
    let g0 = a * n0.yzw;
    let g1 = a * n1.yzw;
    let g2 = a * n2.yzw;
    let curlPsi = vec3<f32>(g2.y - g1.z, g0.z - g2.x, g1.x - g0.y);
    u += A * curlPsi + cross(gradA, psi);
  }
  return u;
}

@compute @workgroup_size(4, 4, 4)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
  let nx = P.froxel.nx;
  let ny = P.froxel.ny;
  let nz = P.froxel.nz;
  if (id.x >= nx || id.y >= ny || id.z >= nz) { return; }
  let s = slices[id.z];
  let layers = u32(s.y);
  let index = id.x + nx * (id.y + ny * id.z);
  if (layers == 0u) {
    detail[index] = vec4<f32>(0.0);
    return;
  }
  let x = froxel_center(P.froxel, id);  // W2 mapping, same as FroxelGrid::froxelCenter on the CPU
  detail[index] = vec4<f32>(detail_velocity(x, layers), f32(layers));
}

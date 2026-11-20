'use strict';
/* ============================================================
   raster.js  -  Phase 1 / Phase 2 toolbox, written from scratch.
   Everything draws into one Uint32Array. No canvas calls.
   Each block maps 1:1 onto a C++ file you will write:
     FB.put / hline            -> framebuffer.cpp
     FB.line  (Bresenham)      -> line.cpp
     FB.circle / disc          -> circle.cpp (midpoint)
     FB.fillPoly (scanline)    -> polygon.cpp
     bez                       -> bezier.cpp
     MStack                    -> matrix.cpp (3x3 affine + stack)
     ik                        -> skeleton.cpp (2-bone solver)
   ============================================================ */
const W = 640, H = 360;
const BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5].map(v => (v + 0.5) / 16);

const clamp = (v, a = 0, b = 1) => (v < a ? a : v > b ? b : v);
const lerp = (a, b, t) => a + (b - a) * t;
const sstep = t => { t = clamp(t); return t * t * (3 - 2 * t); };
const pack = (r, g, b) => ((255 << 24) | (b << 16) | (g << 8) | r) >>> 0;
const hex = s => { const n = parseInt(s.slice(1), 16); return pack((n >> 16) & 255, (n >> 8) & 255, n & 255); };
const cr = c => c & 255, cg = c => (c >> 8) & 255, cb = c => (c >> 16) & 255;
const mix = (a, b, t) => pack(
  Math.round(cr(a) + (cr(b) - cr(a)) * t),
  Math.round(cg(a) + (cg(b) - cg(a)) * t),
  Math.round(cb(a) + (cb(b) - cb(a)) * t));
function rng(seed) {
  let s = seed >>> 0;
  return () => { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s / 4294967296; };
}

const XS = [];
class FB {
  constructor(w = W, h = H) {
    this.w = w; this.h = h;
    this.px = new Uint32Array(w * h);
    this.dither = 1;      // 1 = opaque. <1 = ordered-dither dissolve (4x4 Bayer)
    this.wire = false;    // debug: outlines only
  }
  clear(c) { this.px.fill(c); }
  put(x, y, c) {
    x |= 0; y |= 0;
    if (x < 0 || y < 0 || x >= this.w || y >= this.h) return;
    if (this.dither < 1 && this.dither <= BAYER[((y & 3) << 2) | (x & 3)]) return;
    this.px[y * this.w + x] = c;
  }
  putA(x, y, c, a) {
    x |= 0; y |= 0;
    if (x < 0 || y < 0 || x >= this.w || y >= this.h) return;
    if (this.dither < 1 && this.dither <= BAYER[((y & 3) << 2) | (x & 3)]) return;
    const i = y * this.w + x;
    this.px[i] = mix(this.px[i], c, a);
  }
  hline(x0, x1, y, c) {
    y = Math.round(y);
    if (y < 0 || y >= this.h) return;
    x0 = Math.max(0, Math.round(x0)); x1 = Math.min(this.w - 1, Math.round(x1));
    if (x0 > x1) return;
    if (this.dither >= 1) this.px.fill(c, y * this.w + x0, y * this.w + x1 + 1);
    else for (let x = x0; x <= x1; x++) this.put(x, y, c);
  }
  hlineA(x0, x1, y, c, a) {
    y = Math.round(y);
    if (y < 0 || y >= this.h) return;
    x0 = Math.max(0, Math.round(x0)); x1 = Math.min(this.w - 1, Math.round(x1));
    for (let x = x0; x <= x1; x++) this.putA(x, y, c, a);
  }
  /* Bresenham */
  line(x0, y0, x1, y1, c) {
    x0 = Math.round(x0); y0 = Math.round(y0); x1 = Math.round(x1); y1 = Math.round(y1);
    const dx = Math.abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const dy = -Math.abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    let err = dx + dy, n = 0;
    for (;;) {
      this.put(x0, y0, c);
      if ((x0 === x1 && y0 === y1) || ++n > 2000) break;
      const e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
  /* Midpoint circle, outline */
  circle(cx, cy, r, c) {
    cx = Math.round(cx); cy = Math.round(cy); r = Math.round(r);
    let x = r, y = 0, d = 1 - r;
    while (x >= y) {
      this.put(cx + x, cy + y, c); this.put(cx + y, cy + x, c);
      this.put(cx - x, cy + y, c); this.put(cx - y, cy + x, c);
      this.put(cx + x, cy - y, c); this.put(cx + y, cy - x, c);
      this.put(cx - x, cy - y, c); this.put(cx - y, cy - x, c);
      y++;
      if (d < 0) d += 2 * y + 1; else { x--; d += 2 * (y - x) + 1; }
    }
  }
  /* Midpoint circle, filled with horizontal spans */
  disc(cx, cy, r, c, a) {
    if (this.wire) { this.circle(cx, cy, r, wc(c)); return; }
    cx = Math.round(cx); cy = Math.round(cy); r = Math.round(r);
    if (r <= 0) { this.put(cx, cy, c); return; }
    const H_ = a != null && a < 1 ? (x0, x1, y) => this.hlineA(x0, x1, y, c, a) : (x0, x1, y) => this.hline(x0, x1, y, c);
    let x = r, y = 0, d = 1 - r;
    while (x >= y) {
      H_(cx - x, cx + x, cy + y); H_(cx - x, cx + x, cy - y);
      H_(cx - y, cx + y, cy + x); H_(cx - y, cx + y, cy - x);
      y++;
      if (d < 0) d += 2 * y + 1; else { x--; d += 2 * (y - x) + 1; }
    }
  }
  /* Scanline polygon fill (even-odd, sample at pixel centre) */
  fillPoly(pts, c, a) {
    const n = pts.length;
    if (this.wire) {
      const k = wc(c);
      for (let i = 0; i < n; i++) { const p = pts[i], q = pts[(i + 1) % n]; this.line(p[0], p[1], q[0], q[1], k); }
      return;
    }
    let ymin = Infinity, ymax = -Infinity;
    for (let i = 0; i < n; i++) { const y = pts[i][1]; if (y < ymin) ymin = y; if (y > ymax) ymax = y; }
    const y0 = Math.max(0, Math.ceil(ymin - 0.5)), y1 = Math.min(this.h - 1, Math.ceil(ymax - 0.5) - 1);
    const alpha = a != null && a < 1;
    for (let y = y0; y <= y1; y++) {
      const yc = y + 0.5;
      XS.length = 0;
      for (let i = 0; i < n; i++) {
        const p = pts[i], q = pts[(i + 1) % n];
        if ((p[1] <= yc && q[1] > yc) || (q[1] <= yc && p[1] > yc))
          XS.push(p[0] + (yc - p[1]) * (q[0] - p[0]) / (q[1] - p[1]));
      }
      XS.sort((u, v) => u - v);
      for (let k = 0; k + 1 < XS.length; k += 2) {
        const xa = Math.ceil(XS[k] - 0.5), xb = Math.ceil(XS[k + 1] - 0.5) - 1;
        if (alpha) this.hlineA(xa, xb, y, c, a); else this.hline(xa, xb, y, c);
      }
    }
  }
  ellA(cx, cy, rx, ry, c, a) {
    for (let dy = -Math.ceil(ry); dy <= Math.ceil(ry); dy++) {
      const k = 1 - (dy * dy) / (ry * ry);
      if (k < 0) continue;
      const hw = rx * Math.sqrt(k);
      this.hlineA(cx - hw, cx + hw, cy + dy, c, a);
    }
  }
  /* tapered segment = quad */
  seg(ax, ay, bx, by, w0, w1, c) {
    const dx = bx - ax, dy = by - ay, l = Math.hypot(dx, dy) || 1, nx = -dy / l, ny = dx / l;
    this.fillPoly([
      [ax + nx * w0 / 2, ay + ny * w0 / 2], [bx + nx * w1 / 2, by + ny * w1 / 2],
      [bx - nx * w1 / 2, by - ny * w1 / 2], [ax - nx * w0 / 2, ay - ny * w0 / 2]], c);
  }
  limb(ax, ay, bx, by, w, c) { this.seg(ax, ay, bx, by, w, w, c); this.disc(ax, ay, w / 2, c); this.disc(bx, by, w / 2, c); }
  strip(pts, w0, w1, c) {
    const n = pts.length - 1;
    for (let i = 0; i < n; i++) {
      const a = lerp(w0, w1, i / n), b = lerp(w0, w1, (i + 1) / n);
      this.seg(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], a, b, c);
    }
  }
}
const wc = c => mix(c, 0xffffffff, 0.45);

/* Cubic Bezier sampled into a polyline */
function bez(p0, p1, p2, p3, n = 20) {
  const o = [];
  for (let i = 0; i <= n; i++) {
    const t = i / n, u = 1 - t, a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
    o.push([a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0], a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]]);
  }
  return o;
}

/* 3x3 affine matrix stack, [a b c d e f]:  x' = a x + c y + e,  y' = b x + d y + f */
class MStack {
  constructor() { this.s = [[1, 0, 0, 1, 0, 0]]; }
  get m() { return this.s[this.s.length - 1]; }
  push() { this.s.push(this.m.slice()); return this; }
  pop() { this.s.pop(); return this; }
  mul(n) {
    const m = this.m;
    this.s[this.s.length - 1] = [
      m[0] * n[0] + m[2] * n[1], m[1] * n[0] + m[3] * n[1],
      m[0] * n[2] + m[2] * n[3], m[1] * n[2] + m[3] * n[3],
      m[0] * n[4] + m[2] * n[5] + m[4], m[1] * n[4] + m[3] * n[5] + m[5]];
    return this;
  }
  translate(x, y) { return this.mul([1, 0, 0, 1, x, y]); }
  rotate(a) { const c = Math.cos(a), s = Math.sin(a); return this.mul([c, s, -s, c, 0, 0]); }
  scale(x, y = x) { return this.mul([x, 0, 0, y, 0, 0]); }
  pt(x, y) { const m = this.m; return [m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]]; }
  poly(pts) { return pts.map(p => this.pt(p[0], p[1])); }
}

/* Two-bone inverse kinematics. dir = +1 / -1 picks which side the elbow bends to. */
function ik(sx, sy, tx, ty, l1, l2, dir) {
  let dx = tx - sx, dy = ty - sy, d = Math.hypot(dx, dy);
  const maxd = l1 + l2 - 0.01, mind = Math.abs(l1 - l2) + 0.5;
  if (d > maxd) { dx *= maxd / d; dy *= maxd / d; d = maxd; }
  if (d < mind) { d = mind; if (!dx && !dy) dy = d; }
  const a = (l1 * l1 - l2 * l2 + d * d) / (2 * d);
  const h = Math.sqrt(Math.max(0, l1 * l1 - a * a));
  const ux = dx / (Math.hypot(dx, dy) || 1), uy = dy / (Math.hypot(dx, dy) || 1);
  const mx = sx + ux * a, my = sy + uy * a;
  return { ex: mx + dir * (-uy) * h, ey: my + dir * ux * h, hx: sx + ux * d, hy: sy + uy * d };
}

/* Ordered-dither gradient: N colour bands, Bayer picks between neighbours */
function bandFill(fb, x0, y0, x1, y1, topC, botC, N = 12) {
  const bands = [];
  for (let i = 0; i < N; i++) bands.push(mix(topC, botC, i / (N - 1)));
  for (let y = y0; y < y1; y++) {
    const u = ((y - y0) / Math.max(1, y1 - y0 - 1)) * (N - 1);
    const b = Math.min(N - 2, Math.floor(u)), f = u - b;
    for (let x = x0; x < x1; x++)
      fb.px[y * fb.w + x] = f > BAYER[((y & 3) << 2) | (x & 3)] ? bands[b + 1] : bands[b];
  }
}
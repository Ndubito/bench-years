/* ============================================================
   story.js  -  the bench, the people, the eight scenes.
   Everything is parametric: a scene is a function of time.
   ============================================================ */
const _hx = new Map();
function HX(s) { let v = _hx.get(s); if (v === undefined) { v = hex(s); _hx.set(s, v); } return v; }
const tintC = (c, t) => (t[0] === 1 && t[1] === 1 && t[2] === 1) ? c :
  pack(Math.min(255, Math.round(cr(c) * t[0])), Math.min(255, Math.round(cg(c) * t[1])), Math.min(255, Math.round(cb(c) * t[2])));

const CX = 320, SEAT = 266, GY = 242;
const D = [6, 6, 6, 6, 6.5, 7, 6.5, 9];            // scene lengths (s)
const TD = [2.2, 2.2, 2.2, 2.2, 2.2, 2.6, 3.2];    // gust lengths (s)

/* ---------- environment keyframes (one per life stage) ---------- */
const ENVDEF = [
  { sky: ['#7fb9e6', '#fde7c7'], sun: 20, sunC: '#ffd479', hill: ['#a9cfae', '#86bd8e'], gr: ['#92c97e', '#6fae62'], path: '#e3d3ae', leaf: ['#4f9a4a', '#6fbd5b', '#9be07c'], growth: .16, leafAmt: 1, wear: 0, cloud: '#ffffff' },
  { sky: ['#63aee6', '#d3ecf4'], sun: 42, sunC: '#ffe9a0', hill: ['#9ccaa5', '#7ab787'], gr: ['#86c176', '#68a85d'], path: '#e0d0aa', leaf: ['#46923f', '#69b955', '#98dc78'], growth: .38, leafAmt: 1, wear: .1, cloud: '#ffffff' },
  { sky: ['#4a9de6', '#c4e6f6'], sun: 72, sunC: '#fff2b8', hill: ['#94c6a0', '#74b282'], gr: ['#7fbd70', '#62a458'], path: '#decda6', leaf: ['#3f8a3a', '#62b24e', '#92d672'], growth: .62, leafAmt: 1, wear: .2, cloud: '#ffffff' },
  { sky: ['#4a96dd', '#bfe0f2'], sun: 100, sunC: '#fff2b8', hill: ['#94c6a0', '#74b282'], gr: ['#7fbd70', '#62a458'], path: '#decda6', leaf: ['#3f8a3a', '#62b24e', '#92d672'], growth: .8, leafAmt: 1, wear: .3, cloud: '#ffffff' },
  { sky: ['#5b9fd6', '#f3dfb4'], sun: 122, sunC: '#ffe08a', hill: ['#a2bf8f', '#86aa78'], gr: ['#9bb868', '#7b9d52'], path: '#d6c49c', leaf: ['#6c8f2f', '#a3a93a', '#d7c14a'], growth: .95, leafAmt: 1, wear: .45, cloud: '#fff6e6', drift: 4 },
  { sky: ['#6c86c4', '#f6b66a'], sun: 142, sunC: '#ffb347', hill: ['#b5a98a', '#98906f'], gr: ['#a9a863', '#8a8a4f'], path: '#cdb78e', leaf: ['#b4561f', '#df8a2e', '#f2b84b'], growth: 1, leafAmt: .85, wear: .6, cloud: '#ffd9b0', drift: 8 },
  { sky: ['#3a3f7c', '#ee8a68'], sun: 160, sunC: '#ff7a4a', hill: ['#6e6a86', '#5a5873'], gr: ['#6f7a52', '#58633f'], path: '#9a8d78', leaf: ['#8a3d1c', '#b5561f', '#d98a3a'], growth: 1, leafAmt: .22, wear: .75, cloud: '#d98f9a', drift: 7, tint: [.92, .86, .96] },
  { sky: ['#070b24', '#1c2850'], sun: 200, hill: ['#8d8fb0', '#757996'], gr: ['#6f8a6a', '#5b7658'], path: '#b5a98a', leaf: ['#8a3d1c', '#b5561f', '#d98a3a'], growth: 1, leafAmt: .06, wear: .85, stars: 1, night: 1, drift: 3, tint: [.4, .48, .78] }
].map(o => ({
  skyT: HX(o.sky[0]), skyB: HX(o.sky[1]), sun: o.sun, sunC: HX(o.sunC || '#ffe08a'),
  hillF: HX(o.hill[0]), hillN: HX(o.hill[1]), gr0: HX(o.gr[0]), gr1: HX(o.gr[1]), path: HX(o.path),
  leaf: o.leaf.map(HX), trunk: HX('#6b4a33'), growth: o.growth, leafAmt: o.leafAmt, wear: o.wear,
  stars: o.stars || 0, night: o.night || 0, cloud: o.cloud ? HX(o.cloud) : null, drift: o.drift || 0, tint: o.tint || [1, 1, 1]
}));

function envAt(i, t) {
  const a = ENVDEF[i], b = ENVDEF[Math.min(7, i + 1)], k = i === 7 ? 0 : 0.3 * clamp(t / D[i]), tn = a.tint;
  const M = (x, y) => tintC(mix(x, y, k), tn);
  return {
    i, skyT: mix(a.skyT, b.skyT, k), skyB: mix(a.skyB, b.skyB, k), sun: lerp(a.sun, b.sun, k), sunC: a.sunC,
    hillF: M(a.hillF, b.hillF), hillN: M(a.hillN, b.hillN), gr0: M(a.gr0, b.gr0), gr1: M(a.gr1, b.gr1), path: M(a.path, b.path),
    leaf: [0, 1, 2].map(j => M(a.leaf[j], b.leaf[j])), trunk: tintC(a.trunk, tn),
    growth: a.growth, leafAmt: a.leafAmt, wear: a.wear, stars: a.stars, night: a.night,
    cloud: a.cloud ? mix(a.cloud, b.cloud || a.cloud, k) : null, drift: a.drift, tint: tn,
    C: h => tintC(HX(h), tn)
  };
}

/* ---------- world pieces ---------- */
const HILLF = bez([-20, 236], [110, 168], [250, 196], [330, 226], 22).concat(bez([330, 226], [420, 258], [540, 176], [680, 224], 22), [[680, 270], [-20, 270]]);
const HILLN = bez([-20, 252], [100, 220], [220, 212], [320, 240], 22).concat(bez([320, 240], [430, 262], [560, 214], [680, 236], 22), [[680, 270], [-20, 270]]);
const PATH = bez([-20, 336], [140, 310], [300, 348], [690, 314], 48);
function pathY(x) {
  if (x <= PATH[0][0]) return PATH[0][1];
  for (let i = 0; i < PATH.length - 1; i++) if (x <= PATH[i + 1][0]) {
    const u = (x - PATH[i][0]) / (PATH[i + 1][0] - PATH[i][0]); return lerp(PATH[i][1], PATH[i + 1][1], u);
  }
  return PATH[PATH.length - 1][1];
}
const CLOUDS = [{ x: 60, y: 58, sc: 1, sp: 5 }, { x: 260, y: 36, sc: .8, sp: 3.5 }, { x: 430, y: 84, sc: 1.2, sp: 4 }, { x: 560, y: 46, sc: .7, sp: 6 }];
const WIREBG = hex('#0d1220');

function drawSky(fb, e) { bandFill(fb, 0, 0, W, GY + 6, e.skyT, e.skyB, 14); }

function drawCelestial(fb, e, t) {
  if (e.stars && !fb.wire) {
    const r = rng(7);
    for (let i = 0; i < 80; i++) {
      const x = r() * W, y = r() * 170, b = .5 + .5 * Math.sin(t * 2 + i * 1.7);
      fb.put(x, y, mix(e.skyT, 0xffffffff, .35 + .6 * b));
    }
  }
  if (e.night) {                       // moon: two discs make a crescent
    fb.disc(470, 66, 14, HX('#f4f1da'));
    if (!fb.wire) fb.disc(476, 62, 12, mix(e.skyT, e.skyB, .1));
  } else {                             // sun: position is a rotation about a pivot
    const a = e.sun * Math.PI / 180, sx = 320 - 255 * Math.cos(a), sy = 305 - 255 * Math.sin(a);
    if (!fb.wire) { fb.disc(sx, sy, 38, e.sunC, .09); fb.disc(sx, sy, 29, e.sunC, .13); fb.disc(sx, sy, 21, e.sunC, .22); }
    fb.disc(sx, sy, 15, e.sunC);
  }
  if (e.cloud && !fb.wire) for (const c of CLOUDS) {
    const x = ((c.x + t * c.sp + 120) % (W + 240)) - 120, y = c.y, s = c.sc;
    fb.disc(x, y, 11 * s, e.cloud); fb.disc(x + 14 * s, y - 5 * s, 15 * s, e.cloud); fb.disc(x + 30 * s, y, 12 * s, e.cloud);
    fb.disc(x - 11 * s, y + 4 * s, 8 * s, e.cloud); fb.disc(x + 42 * s, y + 5 * s, 8 * s, e.cloud);
    fb.fillPoly([[x - 12 * s, y + 3 * s], [x + 44 * s, y + 3 * s], [x + 44 * s, y + 11 * s], [x - 12 * s, y + 11 * s]], e.cloud);
  }
}

function drawGround(fb, e) {
  fb.fillPoly(HILLF, e.hillF); fb.fillPoly(HILLN, e.hillN);
  if (fb.wire) fb.fillPoly([[0, GY], [W, GY], [W, H - 1], [0, H - 1]], e.gr0);
  else bandFill(fb, 0, GY, W, H, e.gr0, e.gr1, 9);
  if (!fb.wire) {
    const r = rng(11), tc = mix(e.gr0, 0xffffffff, .18), dc = mix(e.gr1, 0xff000000, .14);
    for (let i = 0; i < 90; i++) {
      const x = r() * W, y = 250 + r() * 100, l = 3 + r() * 3;
      fb.line(x, y, x - 1, y - l, tc); fb.line(x + 2, y, x + 3, y - l + 1, dc);
    }
  }
  fb.strip(PATH, 30, 40, mix(e.path, 0xff000000, .1));
  fb.strip(PATH, 25, 34, e.path);
}

function leaf(fb, x, y, rot, sz, col, vein) {
  const c = Math.cos(rot), s = Math.sin(rot);
  const P = [[-1, 0], [-.45, -.55], [.5, -.5], [1, 0], [.5, .5], [-.45, .55]].map(p => [x + (p[0] * c - p[1] * s) * sz, y + (p[0] * s + p[1] * c) * sz]);
  fb.fillPoly(P, col);
  if (vein != null) fb.line(x - c * sz * .9, y - s * sz * .9, x + c * sz * .9, y + s * sz * .9, vein);
}

/* recursive tree: Bezier-ish trunk quads + foliage discs. growth is the only parameter. */
function drawTree(fb, e, t) {
  const g = e.growth, bx = 126, by = 300;
  const maxD = 2 + Math.round(g * 3), len0 = lerp(20, 58, g), w0 = lerp(3, 13, g);
  const r = rng(42), blobs = [];
  if (!fb.wire) fb.ellA(bx + 12, by + 3, 26 + g * 44, 4 + g * 4, 0xff000000, .16);
  (function br(x, y, ang, len, w, d) {
    const a = ang + Math.sin(t * 1.0 + d * .9 + x * .015) * .022 * (maxD - d + 1) * .6;
    const x2 = x + Math.sin(a) * len, y2 = y - Math.cos(a) * len;
    fb.seg(x, y, x2, y2, w, Math.max(1.2, w * .72), e.trunk);
    if (d <= 3) { blobs.push([x2, y2, r(), lerp(5, 15, g) * (d === 0 ? 1 : d === 1 ? .9 : .75)]); if (d === 0) return; }
    const n = d === maxD ? (g < .3 ? 2 : 3) : (r() < .35 ? 3 : 2);
    for (let k = 0; k < n; k++) {
      const sp = n === 2 ? (k ? 1 : -1) * (.38 + r() * .34) : (k - 1) * (.55 + r() * .2);
      br(x2, y2, a + sp, len * .74, w * .66, d - 1);
    }
  })(bx, by, 0, len0, w0, maxD);
  { /* filler blobs so the crown is not just a ring of tips */
    const cy = by - lerp(30, 118, g), rx = lerp(14, 78, g), ry = lerp(10, 48, g), n = Math.round(5 + g * 20);
    for (let k = 0; k < n; k++) { const a = r() * 6.283, d = Math.sqrt(r()); blobs.push([bx + 6 + Math.cos(a) * rx * d, cy + Math.sin(a) * ry * d, r(), lerp(5, 13, g)]); }
  }
  const shrink = lerp(.3, 1, Math.min(1, e.leafAmt * 2));
  for (const b of blobs) if (b[2] < e.leafAmt) {
    const R = b[3] * shrink;
    fb.disc(b[0] - 1.5, b[1] + 2, R, e.leaf[0]);
    fb.disc(b[0], b[1], R * .88, e.leaf[1]);
    fb.disc(b[0] + 2, b[1] - 2.5, R * .5, e.leaf[2]);
  }
}

function lampLevel(i, t) {
  if (i < 6) return 0;
  if (i === 6) return sstep((t - 3) / 1.6) * .55;
  if (t < 1.0) return 0;
  if (t < 1.7) return Math.sin(t * 55) > 0 ? .8 : .12;
  return 1;
}
function drawLamp(fb, e, L) {
  const dk = e.C('#2d3340');
  fb.fillPoly([[525, 298], [531, 298], [530, 178], [527, 178]], dk);
  fb.fillPoly([[521, 296], [535, 296], [535, 300], [521, 300]], dk);
  fb.seg(528.5, 180, 513, 172, 3, 3, dk);
  fb.fillPoly([[502, 171], [524, 171], [520, 181], [506, 181]], dk);
  fb.disc(513, 183, 3, L > 0 ? mix(HX('#d9d4c0'), HX('#fff1b0'), L) : e.C('#b8b8b0'));
}
function lampGlow(fb, L) {
  if (L <= 0 || fb.wire) return;
  const warm = HX('#ffd98a');
  fb.fillPoly([[507, 184], [519, 184], [575, 300], [455, 300]], warm, .11 * L);
  fb.disc(513, 184, 58, warm, .05 * L); fb.disc(513, 184, 40, warm, .07 * L); fb.disc(513, 184, 24, warm, .12 * L);
  fb.ellA(513, 304, 70, 11, warm, .22 * L);
  fb.ellA(340, 290, 140, 40, warm, .07 * L);
}

function drawBench(fb, e, plaque) {
  const wood = tintC(mix(HX('#2f9a77'), HX('#8a7456'), e.wear), e.tint), dk = mix(wood, 0xff000000, .3), lt = mix(wood, 0xffffffff, .18);
  if (!fb.wire) fb.ellA(320, 301, 104, 5, 0xff000000, .2);
  for (const x of [248, 388]) {
    fb.fillPoly([[x - 4, 266], [x + 4, 266], [x + 3, 298], [x - 3, 298]], dk);
    fb.fillPoly([[x - 7, 297], [x + 7, 297], [x + 7, 300], [x - 7, 300]], dk);
    fb.fillPoly([[x - 3, 224], [x + 3, 224], [x + 3, 268], [x - 3, 268]], dk);
  }
  for (const y of [226, 236, 246]) {
    fb.fillPoly([[238, y], [402, y], [402, y + 7], [238, y + 7]], wood);
    if (!fb.wire) fb.hline(238, 402, y, lt);
  }
  fb.fillPoly([[232, 265], [408, 265], [410, 270], [230, 270]], lt);
  fb.fillPoly([[230, 270], [410, 270], [410, 276], [230, 276]], wood);
  if (!fb.wire) {
    fb.hline(230, 410, 276, dk);
    const r = rng(5), n = Math.round(e.wear * 70);
    for (let i = 0; i < n; i++) fb.put(232 + r() * 176, r() < .5 ? 226 + r() * 28 : 266 + r() * 9, dk);
  }
  if (plaque) {
    const g = tintC(HX('#d9b44a'), e.tint), gd = mix(g, 0xff000000, .45);
    fb.fillPoly([[360, 237], [380, 237], [380, 243], [360, 243]], g);
    if (!fb.wire) { fb.hline(363, 377, 239, gd); fb.hline(365, 375, 241, gd); }
  }
}

/* ============================================================
   People.  Two rigs:
   sitFigure  - front view, torso rotates about the hip, arms by 2-bone IK
   walkFigure - side view, matrix hierarchy hip > thigh > shin > foot
   ============================================================ */
const EYE = hex('#1a1410'), MOUTH = hex('#4a1c16');

function sitFigure(fb, P) {
  const s = P.s, f = !!P.female, hs = P.hs || 1;
  const hw = (f ? 9 : 10) * s, sw = (f ? 11 : 12.5) * s, T = (33 - (P.hunch || 0) * 5) * s, hr = 8.4 * s * hs;
  const x0 = P.x, y0 = P.y - (P.bounce || 0), out = {};
  fb.dither = P.alpha == null ? 1 : P.alpha;
  const knee = P.knee || [0, 0], kneeY = 9 * s, shin = (P.shin || 26) * s, legC = f ? P.skin : P.bottom;
  [-1, 1].forEach((sd, idx) => {
    const kx = x0 + sd * hw * .52, ky = y0 + kneeY;
    if (!f) fb.fillPoly([[x0 + sd * hw * .06, y0 - 2], [x0 + sd * hw * 1.02, y0 - 2], [x0 + sd * hw * 1.02, ky + 1], [x0 + sd * hw * .06, ky + 1]], P.bottom);
    const ax = kx + Math.sin(knee[idx]) * shin, ay = ky + Math.cos(knee[idx]) * shin;
    fb.limb(kx, ky, ax, ay, 6.5 * s, legC);
    fb.fillPoly([[ax - 5 * s + sd * s, ay - 1], [ax + 5 * s + sd * s, ay - 1], [ax + 6 * s + sd * s, ay + 3.5 * s], [ax - 6 * s + sd * s, ay + 3.5 * s]], P.shoe);
  });
  if (f) fb.fillPoly([[x0 - hw - .5 * s, y0 - 5 * s], [x0 + hw + .5 * s, y0 - 5 * s], [x0 + hw + 4 * s, y0 + 9 * s], [x0 - hw - 4 * s, y0 + 9 * s]], P.top);

  const ms = new MStack(); ms.translate(x0, y0); ms.rotate(P.lean || 0);
  fb.fillPoly(ms.poly([[-hw, 0], [hw, 0], [sw, -T], [-sw, -T]]), P.top);
  const shL = ms.pt(-sw, -T + 2.5 * s), shR = ms.pt(sw, -T + 2.5 * s);
  fb.disc(shL[0], shL[1], 3.6 * s, P.top); fb.disc(shR[0], shR[1], 3.6 * s, P.top);
  fb.fillPoly(ms.poly([[-3 * s, -T + 1], [3 * s, -T + 1], [3 * s, -T - 4 * s], [-3 * s, -T - 4 * s]]), P.skin);
  if (P.scarf) {
    fb.fillPoly(ms.poly([[-sw * .55, -T + .5 * s], [sw * .55, -T + .5 * s], [sw * .38, -T + 5 * s], [-sw * .38, -T + 5 * s]]), P.scarf);
    fb.fillPoly(ms.poly([[sw * .08, -T + 4 * s], [sw * .42, -T + 4 * s], [sw * .38, -T + 17 * s], [sw * .04, -T + 17 * s]]), P.scarf);
  }
  /* head: continue the same matrix chain: neck > tilt > head centre */
  ms.translate(0, -T); ms.rotate(P.tilt || 0); ms.translate(0, -(hr + 2.5 * s));
  const turn = P.turn || 0, tu = turn * 1.6 * s, st = P.hair;
  const q = (x, y) => ms.pt(x + tu, y);
  if (st === 'puff') { const p = q(0, -.3 * hr); fb.disc(p[0], p[1], hr * 1.42, P.hairC); }
  if (st === 'twin') for (const sd of [-1, 1]) { const p = q(sd * hr * 1.0, -.3 * hr); fb.disc(p[0], p[1], hr * .42, P.hairC); }
  if (st === 'pony') { const p = q(hr * 1.05, .1 * hr); fb.disc(p[0], p[1], hr * .45, P.hairC); }
  if (st === 'short' || st === 'bun' || st === 'twin' || st === 'pony') { const p = q(0, -.25 * hr); fb.disc(p[0], p[1], hr + .8, P.hairC); }
  const fc = st === 'puff' ? q(0, .12 * hr) : st === 'bald' ? q(0, 0) : q(0, .16 * hr);
  fb.disc(fc[0], fc[1], st === 'puff' ? hr * .95 : st === 'bald' ? hr : hr - .4, P.skin);
  if (st === 'bun') { const p = q(0, -hr * 1.25); fb.disc(p[0], p[1], hr * .42, P.hairC); }
  if (st === 'bald') for (const sd of [-1, 1]) { const p = q(sd * hr * .88, -.1 * hr); fb.disc(p[0], p[1], Math.max(1, hr * .22), P.hairC); }
  for (const sd of [-1, 1]) {
    const p = q(sd * hr * .38 + turn * hr * .3, .22 * hr);
    fb.put(p[0], p[1], EYE); if (!P.blink) fb.put(p[0], p[1] + 1, EYE);
  }
  if (P.smile) {
    const a = q(-hr * .3, .58 * hr), b = q(0, .76 * hr), c = q(hr * .3, .58 * hr);
    if (P.open) fb.disc(b[0], b[1] - 1, Math.max(1, hr * .2), MOUTH); else { fb.line(a[0], a[1], b[0], b[1], MOUTH); fb.line(b[0], b[1], c[0], c[1], MOUTH); }
  }
  out.head = ms.pt(0, 0); out.hr = hr; out.earL = q(-hr, .15 * hr); out.earR = q(hr, .15 * hr);

  const ul = 15 * s, fl = 14 * s, tg = P.hand || {};
  const rest = { L: [x0 - 7 * s, y0 + s], R: [x0 + 7 * s, y0 + s] };
  [['L', shL, 1], ['R', shR, -1]].forEach(([k, sh, dir]) => {
    const t = tg[k] || rest[k], r = ik(sh[0], sh[1], t[0], t[1], ul, fl, dir);
    fb.limb(sh[0], sh[1], r.ex, r.ey, 5.6 * s, P.top);
    fb.limb(r.ex, r.ey, r.hx, r.hy, 4.8 * s, P.sleeve === 'short' ? P.skin : P.top);
    fb.disc(r.hx, r.hy, 3 * s, P.skin);
    out['hand' + k] = [r.hx, r.hy];
  });
  fb.dither = 1;
  return out;
}

function walkFigure(fb, P) {
  const s = P.s, f = !!P.female, hs = P.hs || 1, LL = 19;
  const ph = P.phase, moving = ph != null, A = moving ? (P.amp || .6) : .04;
  const hipY = P.gy - (2 * LL + 3) * s, hrL = 8.2 * hs, hr = hrL * s, legC = f ? P.skin : P.bottom;
  fb.dither = P.alpha == null ? 1 : P.alpha;
  if (!fb.wire) fb.ellA(P.x, P.gy + 1, 13 * s, 2.6 * s, 0xff000000, .22);
  const ms = new MStack(); ms.translate(P.x, hipY); ms.scale(P.facing * s, s);
  const leg = po => {
    const a = moving ? A * Math.sin(ph + po) : (po ? .06 : -.06);
    const fx = moving ? Math.max(0, Math.sin(ph + po + 1.9)) * .95 : .05;
    ms.push(); ms.rotate(-a);
    fb.fillPoly(ms.poly([[-3.4, 0], [3.4, 0], [2.9, LL], [-2.9, LL]]), legC);
    const k = ms.pt(0, LL); fb.disc(k[0], k[1], 3 * s, legC);
    ms.translate(0, LL); ms.rotate(fx);
    fb.fillPoly(ms.poly([[-2.9, 0], [2.9, 0], [2.4, LL], [-2.4, LL]]), legC);
    ms.translate(0, LL);
    fb.fillPoly(ms.poly([[-2.4, -.5], [5, 0], [5.4, 3], [-2.4, 3]]), P.shoe);
    ms.pop();
  };
  const arm = (po, front) => {
    let b = moving ? .6 * Math.sin(ph + po) : .06, fl = .22 + (moving ? .3 * Math.max(0, Math.sin(ph + po + 1.2)) : 0);
    if (front && P.raise != null) { b = P.raise; fl = P.wave != null ? .5 + .55 * Math.sin(P.wave) : .3; }
    if (front && P.cane) { b = .5; fl = .5; }
    ms.push(); ms.translate(0, -28); ms.rotate(-b);
    fb.fillPoly(ms.poly([[-2.4, 0], [2.4, 0], [2, 13], [-2, 13]]), P.top);
    ms.translate(0, 13); ms.rotate(-fl);
    fb.fillPoly(ms.poly([[-2, 0], [2, 0], [1.7, 12], [-1.7, 12]]), P.sleeve === 'long' ? P.top : P.skin);
    const h = ms.pt(0, 12.5); fb.disc(h[0], h[1], 2.7 * s, P.skin);
    if (front && P.cane) { fb.limb(h[0], h[1], h[0] + P.facing * 3 * s, P.gy, 2, P.cane); }
    ms.pop();
  };
  leg(Math.PI);
  ms.push(); ms.rotate(P.lean || 0);
  arm(0, false);
  fb.fillPoly(ms.poly([[-4.8, 0], [4.8, 0], [5.4, -30], [-5.4, -30]]), P.top);
  ms.pop();
  leg(0);
  if (f) fb.fillPoly(ms.poly([[-5, -6], [5, -6], [8.5, 14], [-8.5, 14]]), P.top);
  ms.push(); ms.rotate(P.lean || 0);
  arm(Math.PI, true);
  ms.translate(0, -30); ms.rotate(P.bow || 0); ms.translate(0, -(hrL + 1));
  const q = (x, y) => ms.pt(x, y), st = P.hair;
  if (st === 'puff') { const p = q(-.35 * hrL, -.3 * hrL); fb.disc(p[0], p[1], hr * 1.3, P.hairC); }
  if (st === 'twin') { const p = q(-.8 * hrL, -.2 * hrL); fb.disc(p[0], p[1], hr * .45, P.hairC); }
  if (st === 'short' || st === 'bun' || st === 'twin') { const p = q(-.18 * hrL, -.22 * hrL); fb.disc(p[0], p[1], hr + .5, P.hairC); }
  if (st === 'bald') { const p = q(-.7 * hrL, -.1 * hrL); fb.disc(p[0], p[1], hr * .35, P.hairC); }
  const fc = q(st === 'puff' ? .15 * hrL : .1 * hrL, .1 * hrL);
  fb.disc(fc[0], fc[1], st === 'puff' ? hr * .93 : hr - .7, P.skin);
  if (st === 'bun') { const p = q(-.7 * hrL, -.95 * hrL); fb.disc(p[0], p[1], hr * .4, P.hairC); }
  fb.fillPoly(ms.poly([[hrL * .9, -hrL * .05], [hrL + 2.2, hrL * .28], [hrL * .85, hrL * .42]]), P.skin);
  const ey = q(hrL * .5, -.05 * hrL); fb.put(ey[0], ey[1], EYE);
  const hd = ms.pt(0, 0);
  ms.pop();
  fb.dither = 1;
  return { head: hd };
}

/* ---------- shared prop / style helpers ---------- */
const PM = (C, o) => Object.assign({ female: false, skin: C('#8c5a3c'), hair: 'short', hairC: C('#1b1410'), shoe: C('#2a2430'), bottom: C('#2e3a4d'), top: C('#4a7fb5'), sleeve: 'long' }, o);
const PW = (C, o) => Object.assign({ female: true, skin: C('#a8714d'), hair: 'puff', hairC: C('#1b1410'), shoe: C('#2a2430'), bottom: C('#a8714d'), top: C('#e0607e'), sleeve: 'short' }, o);
const blink = t => ((t * .83) % 3.3) < .13 ? 1 : 0;
const RED = '#c93b3b';

function star(fb, x, y, r, c) {
  fb.line(x - r, y, x + r, y, c); fb.line(x, y - r, x, y + r, c);
  fb.line(x - r * .6, y - r * .6, x + r * .6, y + r * .6, c); fb.line(x - r * .6, y + r * .6, x + r * .6, y - r * .6, c);
}
function cup(fb, e, x, y, t, seed) {
  fb.fillPoly([[x - 3.5, y - 8], [x + 3.5, y - 8], [x + 2.7, y + 1], [x - 2.7, y + 1]], e.C('#f4f1ea'));
  fb.fillPoly([[x - 3.5, y - 8], [x + 3.5, y - 8], [x + 3.3, y - 6], [x - 3.3, y - 6]], e.C('#7a4a2a'));
  if (!fb.wire) for (let k = 0; k < 3; k++) {
    const u = ((t * .6 + k / 3 + seed) % 1);
    fb.putA(x + Math.sin(u * 8 + k * 2 + seed) * 2, y - 10 - u * 12, 0xffffffff, .7 * (1 - u));
  }
}

/* ============================================================
   Scenes.  back() draws behind the bench, front() draws on / in front of it.
   ============================================================ */
const KIDS = [
  { cx: 170, A: 120, w: 1.7, p: 0, gy: 324, top: '#e86a5a', f: 0, back: 0 },
  { cx: 470, A: 130, w: 1.3, p: 2, gy: 332, top: '#5aa0d8', f: 1, back: 0 },
  { cx: 250, A: 150, w: 1.1, p: 4, gy: 288, top: '#9b6fd0', f: 0, back: 1 },
  { cx: 420, A: 110, w: 1.9, p: 1, gy: 291, top: '#4db38a', f: 1, back: 1 },
  { cx: 560, A: 60, w: 2.1, p: 3, gy: 289, top: '#f08a3c', f: 0, back: 1 }
];
function kidRun(fb, e, t, k, i) {
  const C = e.C, ex = sstep((t - 4.2) / 1.6), arg = k.w * t + k.p;
  let x = k.cx + k.A * Math.sin(arg), face = clamp(Math.cos(arg) * 3, -1, 1);
  x = lerp(x, -60 - 70 * i, ex); face = lerp(face, -1, ex);
  const st = k.f ? PW(C, { top: C(k.top), hair: 'twin', bottom: C('#a8714d') }) : PM(C, { top: C(k.top), bottom: C('#3b4a6b') });
  walkFigure(fb, Object.assign({}, st, { x, gy: k.gy, s: k.back ? .5 : .56, hs: 1.35, facing: face, phase: t * 13 + k.p, amp: .85 }));
}
function hopKid(fb, e, t, k) {
  const C = e.C, s = k.s, gy = 314, standHip = gy - 41 * s, tRun = .9, tJ = .6;
  const st = k.f ? PW(C, { top: C(k.top), hair: 'twin', bottom: C('#a8714d') }) : PM(C, { top: C(k.top), bottom: C('#3b4a6b') });
  if (t < tRun) return walkFigure(fb, Object.assign({}, st, { x: lerp(k.x0, k.xj, t / tRun), gy, s, hs: 1.35, facing: k.face, phase: t * 14 + k.ph, amp: .85 }));
  if (t < tRun + tJ) {
    const u = (t - tRun) / tJ;
    return sitFigure(fb, Object.assign({}, st, { x: lerp(k.xj, k.xs, u), y: lerp(standHip, SEAT, u) - 20 * Math.sin(Math.PI * u), s, hs: 1.35, knee: [.7, -.6], smile: 1, open: 1 }));
  }
  const w = t * 6 + k.ph;
  return sitFigure(fb, Object.assign({}, st, {
    x: k.xs, y: SEAT, s, hs: 1.35, knee: [.55 * Math.sin(w), .55 * Math.sin(w + Math.PI)],
    bounce: 1.3 * Math.abs(Math.sin(t * 7 + k.ph)), smile: 1, open: Math.sin(t * 7 + k.ph) > 0 ? 1 : 0, blink: blink(t + k.ph),
    turn: t > 4.4 ? -.8 : 0
  }));
}

function s2Walkers(fb, e, t) {
  const C = e.C;
  walkFigure(fb, Object.assign(PM(C, { top: C('#c97b3a'), bottom: C('#37425a') }), { x: -40 + t * 100, gy: 289, s: .8, facing: 1, phase: t * 7 }));
  walkFigure(fb, Object.assign(PW(C, { top: C('#5a9a76'), hair: 'bun' }), { x: 690 - t * 80, gy: 287, s: .72, facing: -1, phase: t * 6.5 }));
}

const SC = [];

/* 1: kids */
SC.push({
  name: 'The Group', age: 'Age 7', caption: 'Morning. A tag game spills around the bench. Two kids climb on, legs not reaching the ground.',
  back(fb, e, t) { KIDS.forEach((k, i) => k.back && kidRun(fb, e, t, k, i)); },
  front(fb, e, t) {
    hopKid(fb, e, t, { x0: -30, xj: 250, xs: 292, face: 1, f: 0, top: '#e9b44c', s: .56, ph: 0 });
    hopKid(fb, e, t, { x0: 670, xj: 392, xs: 348, face: -1, f: 1, top: '#e0607e', s: .52, ph: 2 });
    KIDS.forEach((k, i) => !k.back && kidRun(fb, e, t, k, i));
  }
});

/* 2: middle school */
SC.push({
  name: 'The Gap', age: 'Age 13', caption: 'Late morning. A hand-width of space between them, one phone, one pair of earphones.',
  back(fb, e, t) { s2Walkers(fb, e, t); },
  front(fb, e, t) {
    const C = e.C, bump = Math.exp(-Math.pow(t - 3.2, 2) / .05), lg = t > 3.3 && t < 5.2;
    const share = sstep((t - .8) / .8), laugh = lg ? Math.abs(Math.sin(t * 9)) * 1.3 : 0;
    const m = sitFigure(fb, PM(C, {
      x: 300, y: SEAT, s: .8, hs: 1.1, top: C('#4a7fb5'), bottom: C('#2e3a4d'), lean: .02 - bump * .05 + (lg ? Math.sin(t * 9) * .01 : 0), tilt: .08 * share,
      knee: [.1 * Math.sin(t * 2), 0], hand: { R: [318, 262] }, blink: blink(t), smile: lg ? 1 : 0, open: lg ? 1 : 0, bounce: laugh, turn: .5 * share
    }));
    const w = sitFigure(fb, PW(C, {
      x: 340, y: SEAT, s: .78, hs: 1.1, top: C('#8c6fd0'), hair: 'pony', lean: -.02 - bump * .14, tilt: -.08 * share, scarf: null,
      knee: [0, .1 * Math.sin(t * 2 + 1)], hand: { L: [324, 262] }, blink: blink(t + 1), smile: lg ? 1 : 0, open: lg ? 1 : 0, bounce: laugh * .9, turn: -.5 * share
    }));
    fb.fillPoly([[316, 255], [326, 255], [326, 269], [316, 269]], C('#20232b'));
    fb.fillPoly([[317, 256], [325, 256], [325, 266], [317, 266]], C('#9ad0f0'));
    if (share > 0) {
      const a = m.earR, b = w.earL, mid = [(a[0] + b[0]) / 2, Math.max(a[1], b[1]) + 22];
      const cable = bez(a, [a[0] + 2, mid[1]], [b[0] - 2, mid[1]], b, 18);
      for (let i = 0; i < cable.length - 1; i++) fb.line(cable[i][0], cable[i][1], cable[i + 1][0], cable[i + 1][1], HX('#f4f4f4'));
      fb.disc(a[0], a[1], 1, HX('#ffffff')); fb.disc(b[0], b[1], 1, HX('#ffffff'));
    }
  }
});

/* 3: teens */
SC.push({
  name: 'Closer', age: 'Age 17', caption: 'Midday. Hands find each other. Her head ends up on his shoulder. A red scarf appears.',
  front(fb, e, t) {
    const C = e.C, hold = sstep((t - 1.2) / .6), arm = sstep((t - 2.6) / .8), settle = sstep((t - 1.6) / 1.2);
    sitFigure(fb, PM(C, {
      x: 306, y: SEAT, s: .97, top: C('#3f4c5e'), bottom: C('#2e3a4d'), lean: .02 + .05 * settle, tilt: .12 * settle, blink: blink(t),
      hand: { R: arm > 0 ? [lerp(323, 348, arm), lerp(269, 239, arm)] : hold > 0 ? [323, 269] : [313, 268], L: [292, 268] }, smile: settle > .5 ? 1 : 0, bounce: .4 * Math.sin(t * 2)
    }));
    sitFigure(fb, PW(C, {
      x: 336, y: SEAT, s: .95, top: C('#e07a5f'), hair: 'pony', scarf: C(RED), lean: -.2 * settle, tilt: -.18 * settle, blink: blink(t + 1.2),
      hand: { L: hold > 0 ? [323, 269] : [329, 268], R: [345, 268] }, smile: settle > .5 ? 1 : 0, bounce: .4 * Math.sin(t * 2 + 1)
    }));
  }
});

/* 4: adults, married */
SC.push({
  name: 'Coffee', age: 'Age 27', caption: 'Early afternoon. Work bag, two coffees, a ring that catches the light.',
  front(fb, e, t) {
    const C = e.C, clink = Math.exp(-Math.pow(t - 2.4, 2) / .03) * 3;
    fb.fillPoly([[251, 252], [277, 252], [277, 267], [251, 267]], C('#7b4b2a'));
    fb.line(258, 252, 258, 247, C('#4a2c16')); fb.line(258, 247, 270, 247, C('#4a2c16')); fb.line(270, 247, 270, 252, C('#4a2c16'));
    const mh = [318 + clink, 257], wh = [324 - clink, 257];
    sitFigure(fb, PM(C, { x: 303, y: SEAT, s: 1.04, top: C('#eef0f5'), bottom: C('#2b3447'), lean: .04, tilt: .06, hand: { R: mh, L: [291, 268] }, blink: blink(t), smile: 1, bounce: .4 * Math.sin(t * 2) }));
    const w = sitFigure(fb, PW(C, { x: 340, y: SEAT, s: .98, top: C('#2f8f9d'), scarf: C(RED), lean: -.05, tilt: -.09, hand: { L: wh, R: [347, 268] }, blink: blink(t + 1.5), smile: 1, bounce: .4 * Math.sin(t * 2 + 1) }));
    cup(fb, e, mh[0] + 1, mh[1] + 2, t, 0); cup(fb, e, wh[0] - 1, wh[1] + 2, t, 1.3);
    const ph = (t * .7) % 2.4;
    if (ph < .4) star(fb, w.handR[0], w.handR[1] - 1, 2 + 4 * Math.sin(ph / .4 * Math.PI), HX('#ffffff'));
  }
});

/* 5: parents */
SC.push({
  name: 'Three on a Bench', age: 'Age 35', caption: 'Late afternoon. A child between them hops down, runs off, runs back. First leaves turn.',
  front(fb, e, t) {
    const C = e.C, away = t > 1.6;
    const look = t > 2.1 ? (t < 3.8 ? .9 : .6) : 0;
    sitFigure(fb, PM(C, { x: 280, y: SEAT, s: 1.04, top: C('#5c7c4a'), bottom: C('#2b3447'), lean: away ? .03 : .06, tilt: .05, turn: look, hand: away ? { R: [292, 268] } : { R: [311, 250] }, blink: blink(t), smile: 1, bounce: .4 * Math.sin(t * 2) }));
    sitFigure(fb, PW(C, { x: 360, y: SEAT, s: .98, top: C('#c46a2f'), scarf: C(RED), lean: away ? -.03 : -.06, tilt: -.05, turn: -look, hand: away ? { L: [353, 268] } : { L: [329, 250] }, blink: blink(t + 1), smile: 1, bounce: .4 * Math.sin(t * 2 + 1) }));
    const st = PM(C, { top: C('#f2c14e'), bottom: C('#3b4a6b') }), gy = 316, s = .5, hh = gy - 41 * s;
    if (t < 1.6) {
      const w = t * 6;
      sitFigure(fb, Object.assign({}, st, { x: 320, y: SEAT, s, hs: 1.4, knee: [.5 * Math.sin(w), .5 * Math.sin(w + Math.PI)], smile: 1, open: 1, bounce: 1 * Math.abs(Math.sin(t * 7)), blink: blink(t) }));
    } else if (t < 2.1) {
      const u = (t - 1.6) / .5;
      sitFigure(fb, Object.assign({}, st, { x: lerp(320, 338, u), y: lerp(SEAT, hh, u) - 14 * Math.sin(Math.PI * u), s, hs: 1.4, knee: [.7, -.6], smile: 1, open: 1 }));
    } else {
      let x, face;
      if (t < 3.6) { x = 338 + (t - 2.1) * 290; face = 1; }
      else if (t < 3.9) { x = 775; face = 1; }
      else if (t < 5.0) { x = 775 - (t - 3.9) * 290; face = -1; }
      else { x = 456; face = -1; }
      const run = t < 5.0;
      walkFigure(fb, Object.assign({}, st, { x, gy, s, hs: 1.4, facing: face, phase: run ? t * 14 : null, amp: .85, raise: run ? null : 2.5, wave: t * 9 }));
    }
  }
});

/* shared by 6 and 7 */
function elders(fb, e, t, o) {
  const C = e.C, hunch = o.hunch;
  const mArm = o.mArm || [323, 269];
  const m = sitFigure(fb, PM(C, {
    x: 306, y: SEAT, s: 1, top: C('#7a5a45'), bottom: C('#3a3f4d'), hair: 'bald', hairC: C('#c9ccd2'), lean: o.mLean, tilt: o.mTilt, hunch, turn: o.mTurn || 0,
    hand: { R: mArm, L: [285, 262] }, blink: blink(t), smile: o.mSmile ? 1 : 0, bounce: .3 * Math.sin(t * 1.6)
  }));
  if (o.wAlpha > .02) sitFigure(fb, PW(C, {
    x: 336, y: SEAT, s: .97, top: C('#8a5a8f'), hair: 'bun', hairC: C('#cfd2dc'), scarf: C(RED), lean: -.06, tilt: -.07, hunch,
    hand: { L: [323, 269], R: o.wHand || [343, 268] }, blink: blink(t + 1.3), smile: 1, alpha: o.wAlpha, bounce: .3 * Math.sin(t * 1.6 + 1)
  }));
  return m;
}
function cane(fb, e) {
  const c = e.C('#5a3a22');
  fb.limb(284, 261, 276, 302, 2.6, c); fb.disc(284, 260, 2.6, c);
}

/* 6: elders */
SC.push({
  name: 'Golden Hour', age: 'Age 68', caption: 'Low gold light. Hands held, cane on the bench. A grown child and grandchild come by, wave, and leave.',
  back(fb, e, t) { },
  front(fb, e, t) {
    cane(fb, e);
    const wave = t > 3 && t < 4.8;
    elders(fb, e, t, { hunch: .7, mLean: .05, mTilt: .06, mTurn: wave ? .8 : 0, mSmile: wave, wAlpha: 1, wHand: wave ? [358, 233 + 5 * Math.sin(t * 9)] : null });
    const C = e.C;
    let xa, face, moving = true, raise = null;
    if (t < 3) { xa = lerp(745, 470, clamp((t - .6) / 2.4)); face = -1; }
    else if (t < 4.8) { xa = 470; face = -1; moving = false; raise = 2.5; }
    else { const u = sstep((t - 4.8) / .4), w = Math.max(0, t - 5.2); face = lerp(-1, 1, u); xa = 470 + w * w * 70 + w * 40; }
    const xc = xa + face * 26, adult = PW(C, { top: C('#e9a23b'), hair: 'puff', bottom: C('#a8714d') }), kid = PM(C, { top: C('#4db38a'), bottom: C('#3b4a6b') });
    const ph = moving ? t * 7 : null;
    walkFigure(fb, Object.assign({}, kid, { x: xc, gy: pathY(xc) - 4, s: .5, hs: 1.35, facing: face, phase: ph, amp: .7, raise: moving ? null : 2.4, wave: t * 8 }));
    walkFigure(fb, Object.assign({}, adult, { x: xa, gy: pathY(xa) - 4, s: .98, facing: face, phase: ph, amp: .6, raise: raise, wave: t * 8 }));
  }
});

/* 7: the passing */
SC.push({
  name: 'The Passing', age: 'Age 74', caption: 'Dusk. She dissolves from the bench. Her scarf stays for a while. He keeps sitting, looking at the empty half.',
  front(fb, e, t) {
    cane(fb, e);
    const wa = 1 - sstep((t - 1.6) / 2.2);
    const sa = t < 4.8 ? sstep((t - 2.2) / .8) : 1 - sstep((t - 4.8) / 1.4);
    if (sa > .02) {
      fb.dither = sa;
      const c = e.C(RED), cd = mix(c, 0xff000000, .25);
      fb.fillPoly([[326, 261], [348, 261], [353, 266], [323, 266]], c);
      fb.fillPoly([[326, 266], [337, 266], [335, 286], [327, 286]], cd);
      fb.fillPoly([[340, 266], [350, 266], [352, 280], [343, 281]], c);
      fb.dither = 1;
    }
    const look = sstep((t - 1.6) / .8) * (1 - .6 * sstep((t - 4) / 1.5));
    elders(fb, e, t, { hunch: .7 + .3 * sstep((t - 3) / 2), mLean: .05 + .04 * sstep((t - 3) / 2), mTilt: .06 + .08 * sstep((t - 4) / 1.5), mTurn: look, mSmile: false, wAlpha: wa });
  }
});

/* 8: night, empty bench */
SC.push({
  name: 'Empty Bench', age: 'Night', caption: 'Night. He stands, looks at her side of the bench, then walks away. The lamp comes on. One leaf lands where she sat.',
  front(fb, e, t) {
    const C = e.C;
    const turnU = sstep((t - 2.4) / .5), face = lerp(-1, 1, turnU);
    const dt = Math.max(0, t - 2.9), x = 428 + (dt > 0 ? Math.pow(dt, 1.15) * 48 : 0);
    const walking = dt > 0;
    walkFigure(fb, Object.assign(PM(C, { top: C('#6a4f3f'), bottom: C('#3a3f4d'), hair: 'bald', hairC: C('#c9ccd2') }), {
      x, gy: pathY(x) - 6, s: 1, facing: face, phase: walking ? (x - 428) * .13 : null, amp: .42, lean: .2, bow: t < 2.4 ? .3 : .1, cane: C('#5a3a22')
    }));
    const lt = 7.0, u = clamp((t - 5.0) / (lt - 5.0));
    if (t > 5.0) {
      const lx = lerp(300, 338, u) + (u < 1 ? Math.sin(u * 11) * 16 * (1 - u) : 0), ly = lerp(140, 262, u * u * .6 + u * .4);
      leaf(fb, lx, ly, u < 1 ? u * 9 : .5, 5.2, e.leaf[1], e.leaf[0]);
    }
  }
});

/* ---------- scene driver ---------- */
function driftLeaves(fb, e, t) {
  for (let k = 0; k < e.drift; k++) {
    const seed = k * .618, u = (t * .13 + seed) % 1;
    const x = 130 + Math.sin(seed * 30) * 70 + u * 150 + Math.sin(u * 10 + k) * 14, y = 150 + u * 150;
    leaf(fb, x, y, u * 9 + k, 3.4 + (k % 3) * .6, e.leaf[k % 3], null);
  }
}

function drawScene(fb, i, t) {
  t = Math.max(0, t);
  const S = SC[i], e = envAt(i, t), L = lampLevel(i, t);
  fb.dither = 1;
  if (fb.wire) fb.clear(WIREBG); else drawSky(fb, e);
  drawCelestial(fb, e, t);
  drawGround(fb, e);
  drawTree(fb, e, t);
  drawLamp(fb, e, L);
  if (S.back) S.back(fb, e, t);
  drawBench(fb, e, i === 7);
  S.front(fb, e, t);
  if (e.drift) driftLeaves(fb, e, t);
  lampGlow(fb, L);
}

/* ============================================================
   Timeline + gust transition (leaf wall = scissor edge + dither band)
   ============================================================ */
const SEG = []; let TOTAL = 0;
for (let i = 0; i < 8; i++) {
  SEG.push({ k: 's', i, a: TOTAL, b: TOTAL + D[i] }); TOTAL += D[i];
  if (i < 7) { SEG.push({ k: 't', i, j: i + 1, a: TOTAL, b: TOTAL + TD[i] }); TOTAL += TD[i]; }
}
const SCENE_START = SEG.filter(s => s.k === 's').map(s => s.a);
function segAt(T) { T = clamp(T, 0, TOTAL - 1e-6); return SEG.find(s => T >= s.a && T < s.b); }

const LEAVES = (() => {
  const r = rng(99), a = [];
  for (let k = 0; k < 232; k++) {
    const giant = k >= 198;
    a.push({ ox: -.95 + r() * 1.35, oy: r(), ph: r() * 6.28, sp: r(), sz: giant ? 20 + r() * 14 : 3.5 + r() * 5, rot: r() * 6.28, giant, ci: Math.floor(r() * 3) });
  }
  return a;
})();
function hash2(x, y) { let h = Math.imul(x, 374761393) ^ Math.imul(y, 668265263); h = Math.imul(h ^ (h >>> 13), 1274126177); return ((h ^ (h >>> 16)) >>> 0) / 4294967296; }

function gust(out, A, B, e, u) {
  const edge = lerp(-200, W + 200, sstep(u)), bw = 54;
  for (let y = 0; y < H; y++) {
    const ex = edge + 26 * Math.sin(y * .035 + u * 7) + 12 * Math.sin(y * .09 - u * 11);
    const row = y * W, cut = clamp(Math.round(ex), 0, W);
    out.px.set(A.px.subarray(row + cut, row + W), row + cut);
    if (cut > 0) out.px.set(B.px.subarray(row, row + cut), row);
    const x0 = Math.max(0, Math.floor(ex - bw)), x1 = Math.min(W - 1, Math.ceil(ex + bw));
    for (let x = x0; x <= x1; x++) {
      const cov = 1 - Math.abs(x - ex) / bw, rr = hash2(x >> 1, y >> 1);
      if (cov > .55 || rr < cov * 1.3) out.px[row + x] = mix(e.leaf[Math.floor(hash2(x >> 2, (y >> 2) + 7) * 3)], 0xff000000, .1 + .35 * hash2(x, y));
    }
  }
  for (const pass of [0, 1]) for (const l of LEAVES) {
    if ((l.giant ? 1 : 0) !== pass) continue;
    const spread = l.giant ? 190 : 120;
    const x = edge + l.ox * spread + 34 * Math.sin(u * 9 + l.ph);
    const y = ((l.oy * (H + 100) + u * (90 + 70 * l.sp)) % (H + 100)) - 50 + 24 * Math.sin(u * 11 + l.ph * 2);
    const col = l.giant ? mix(e.leaf[l.ci], 0xff000000, .22) : e.leaf[l.ci];
    leaf(out, x, y, l.rot + u * (8 + 8 * l.sp), l.sz, col, l.giant ? mix(col, 0xff000000, .35) : null);
  }
}

/* render the whole film at time T. out/A/B are FB instances */
function render(T, out, A, B, wire) {
  out.wire = A.wire = B.wire = !!wire;
  const s = segAt(T);
  if (s.k === 's') { drawScene(out, s.i, T - s.a); return s; }
  const u = (T - s.a) / (s.b - s.a);
  drawScene(A, s.i, D[s.i] + (T - s.a));
  drawScene(B, s.j, 0);
  gust(out, A, B, envAt(s.i, D[s.i]), u);
  return s;
}
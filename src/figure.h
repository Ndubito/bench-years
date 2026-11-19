#pragma once
// Owner: C. Seated and walking figures. Header-only.
// Every function appends Shapes to `out` in draw order (back to front).
// Points are already transformed into scene space and each Shape's xf is identity,
// so the rasterizer, the OpenGL viewer and the exporter all read them the same way.
// z rises in draw order, so later (front) parts get a larger z.
#include "shape.h"
#include "ik.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace fig {

inline uint32_t withAlpha(uint32_t c, float a) {
    a = a < 0 ? 0 : (a > 1 ? 1 : a);
    uint32_t A = uint32_t(float(c >> 24) * a + 0.5f);
    return (c & 0x00FFFFFFu) | (A << 24);
}

// Adds shapes with rising z and a global alpha (used for the fade-away).
struct Pen {
    std::vector<Shape>& out;
    float z, alpha;
    int n = 0;
    Pen(std::vector<Shape>& o, float z0, float a) : out(o), z(z0), alpha(a) {}
    Shape& add(Kind k, uint32_t col) {
        out.emplace_back();
        Shape& s = out.back();
        s.kind = k;
        s.rgba = withAlpha(col, alpha);
        s.z = z + 0.001f * float(n++);
        return s;
    }
    void poly(const std::vector<Vec2>& p, uint32_t col) { add(Kind::Poly, col).pts = p; }
    void disc(Vec2 c, float r, uint32_t col) { Shape& s = add(Kind::Disc, col); s.pts = {c}; s.r = r; }
    void limb(Vec2 a, Vec2 b, float w, uint32_t col) {
        Shape& s = add(Kind::Strip, col); s.pts = {a, b}; s.w0 = w; s.w1 = w;
    }
    void line(Vec2 a, Vec2 b, uint32_t col) {
        Shape& s = add(Kind::Line, col); s.pts = {a, b}; s.w0 = 1; s.w1 = 1;
    }
};

// Transform a list of local points by the current matrix.
inline std::vector<Vec2> xpoly(const MatrixStack& ms, std::initializer_list<Vec2> l) {
    std::vector<Vec2> v;
    for (const Vec2& p : l) v.push_back(ms.apply(p));
    return v;
}

enum class Hair { Short, Puff, Bun, Twin, Pony, Bald };

// Shared look. Use makeMan()/makeWoman() then change what you need.
struct Look {
    bool female = false;
    Hair hair = Hair::Short;
    uint32_t skin = rgb(0x8c, 0x5a, 0x3c), hairC = rgb(0x1b, 0x14, 0x10);
    uint32_t shoe = rgb(0x2a, 0x24, 0x30), bottom = rgb(0x2e, 0x3a, 0x4d), top = rgb(0x4a, 0x7f, 0xb5);
    bool shortSleeve = false;
    bool hasScarf = false; uint32_t scarf = rgb(0xc9, 0x3b, 0x3b);
    float hs = 1;  // head scale (kids use about 1.35)
};
inline Look makeMan()   { return Look(); }
inline Look makeWoman() {
    Look l; l.female = true; l.hair = Hair::Puff;
    l.skin = rgb(0xa8, 0x71, 0x4d); l.bottom = rgb(0xa8, 0x71, 0x4d); l.top = rgb(0xe0, 0x60, 0x7e);
    l.shortSleeve = true; return l;
}

/* ---------------- seated figure (front view) ---------------- */
struct SeatedPose {
    Look look;
    float x = 0, y = 0, s = 1;      // hip centre on the bench, scale
    float lean = 0, tilt = 0, turn = 0, hunch = 0, bounce = 0;
    float knee[2] = {0, 0};         // left, right lower-leg swing (radians)
    float shin = 26;
    bool blink = false, smile = false, open = false;
    bool hasHandL = false, hasHandR = false; Vec2 handL, handR;  // IK hand targets
    float alpha = 1;                // 1 = solid, 0 = gone (the fade)
    float z = 0;
};
struct SeatedOut { Vec2 head, earL, earR, handL, handR; float hr = 0; };

inline SeatedOut buildSeated(const SeatedPose& P, std::vector<Shape>& out) {
    const Look& L = P.look;
    const float s = P.s; const bool f = L.female;
    const float hw = (f ? 9.f : 10.f) * s, sw = (f ? 11.f : 12.5f) * s;
    const float T = (33.f - P.hunch * 5.f) * s, hr = 8.4f * s * L.hs;
    const float x0 = P.x, y0 = P.y - P.bounce;
    Pen pen(out, P.z, P.alpha);
    SeatedOut res;

    const float kneeY = 9 * s, shin = P.shin * s;
    const uint32_t legC = f ? L.skin : L.bottom;
    for (int i = 0; i < 2; ++i) {
        float sd = i == 0 ? -1.f : 1.f;
        float kx = x0 + sd * hw * .52f, ky = y0 + kneeY;
        if (!f) pen.poly({{x0 + sd * hw * .06f, y0 - 2}, {x0 + sd * hw * 1.02f, y0 - 2},
                          {x0 + sd * hw * 1.02f, ky + 1}, {x0 + sd * hw * .06f, ky + 1}}, L.bottom);
        float ax = kx + std::sin(P.knee[i]) * shin, ay = ky + std::cos(P.knee[i]) * shin;
        pen.limb({kx, ky}, {ax, ay}, 6.5f * s, legC);
        pen.poly({{ax - 5 * s + sd * s, ay - 1}, {ax + 5 * s + sd * s, ay - 1},
                  {ax + 6 * s + sd * s, ay + 3.5f * s}, {ax - 6 * s + sd * s, ay + 3.5f * s}}, L.shoe);
    }
    if (f) pen.poly({{x0 - hw - .5f * s, y0 - 5 * s}, {x0 + hw + .5f * s, y0 - 5 * s},
                     {x0 + hw + 4 * s, y0 + 9 * s}, {x0 - hw - 4 * s, y0 + 9 * s}}, L.top);

    MatrixStack ms; ms.translate(x0, y0); ms.rotate(P.lean);
    pen.poly(xpoly(ms, {{-hw, 0}, {hw, 0}, {sw, -T}, {-sw, -T}}), L.top);
    Vec2 shL = ms.apply({-sw, -T + 2.5f * s}), shR = ms.apply({sw, -T + 2.5f * s});
    pen.disc(shL, 3.6f * s, L.top); pen.disc(shR, 3.6f * s, L.top);
    pen.poly(xpoly(ms, {{-3 * s, -T + 1}, {3 * s, -T + 1}, {3 * s, -T - 4 * s}, {-3 * s, -T - 4 * s}}), L.skin);
    if (L.hasScarf) {
        pen.poly(xpoly(ms, {{-sw * .55f, -T + .5f * s}, {sw * .55f, -T + .5f * s},
                            {sw * .38f, -T + 5 * s}, {-sw * .38f, -T + 5 * s}}), L.scarf);
        pen.poly(xpoly(ms, {{sw * .08f, -T + 4 * s}, {sw * .42f, -T + 4 * s},
                            {sw * .38f, -T + 17 * s}, {sw * .04f, -T + 17 * s}}), L.scarf);
    }

    // head: same matrix chain: neck > tilt > head centre
    ms.translate(0, -T); ms.rotate(P.tilt); ms.translate(0, -(hr + 2.5f * s));
    const float tu = P.turn * 1.6f * s;
    auto q = [&](float x, float y) { return ms.apply({x + tu, y}); };
    const Hair h = L.hair;
    if (h == Hair::Puff) pen.disc(q(0, -.3f * hr), hr * 1.42f, L.hairC);
    if (h == Hair::Twin) for (int i = 0; i < 2; ++i) pen.disc(q((i ? 1.f : -1.f) * hr, -.3f * hr), hr * .42f, L.hairC);
    if (h == Hair::Pony) pen.disc(q(hr * 1.05f, .1f * hr), hr * .45f, L.hairC);
    if (h == Hair::Short || h == Hair::Bun || h == Hair::Twin || h == Hair::Pony)
        pen.disc(q(0, -.25f * hr), hr + .8f, L.hairC);
    Vec2 fc = h == Hair::Puff ? q(0, .12f * hr) : h == Hair::Bald ? q(0, 0) : q(0, .16f * hr);
    pen.disc(fc, h == Hair::Puff ? hr * .95f : h == Hair::Bald ? hr : hr - .4f, L.skin);
    if (h == Hair::Bun) pen.disc(q(0, -hr * 1.25f), hr * .42f, L.hairC);
    if (h == Hair::Bald) for (int i = 0; i < 2; ++i)
        pen.disc(q((i ? 1.f : -1.f) * hr * .88f, -.1f * hr), std::max(1.f, hr * .22f), L.hairC);
    const uint32_t EYE = rgb(0x1a, 0x14, 0x10), MOUTH = rgb(0x4a, 0x1c, 0x16);
    for (int i = 0; i < 2; ++i)
        pen.disc(q((i ? 1.f : -1.f) * hr * .38f + P.turn * hr * .3f, .22f * hr), (P.blink ? .5f : 1.f) * s, EYE);
    if (P.smile) {
        Vec2 a = q(-hr * .3f, .58f * hr), b = q(0, .76f * hr), c = q(hr * .3f, .58f * hr);
        if (P.open) pen.disc({b.x, b.y - 1}, std::max(1.f, hr * .2f), MOUTH);
        else { pen.line(a, b, MOUTH); pen.line(b, c, MOUTH); }
    }
    res.head = ms.apply({0, 0}); res.hr = hr;
    res.earL = q(-hr, .15f * hr); res.earR = q(hr, .15f * hr);

    // arms: two-bone IK toward the hand targets (rest pose if none given)
    const float ul = 15 * s, fl = 14 * s;
    for (int i = 0; i < 2; ++i) {
        Vec2 sh = i == 0 ? shL : shR;
        bool has = i == 0 ? P.hasHandL : P.hasHandR;
        Vec2 tgt = has ? (i == 0 ? P.handL : P.handR)
                       : Vec2{x0 + (i == 0 ? -7.f : 7.f) * s, y0 + s};
        IKResult r = ik2(sh, tgt, ul, fl, i == 0 ? 1.f : -1.f);
        pen.limb(sh, r.elbow, 5.6f * s, L.top);
        pen.limb(r.elbow, r.hand, 4.8f * s, L.shortSleeve ? L.skin : L.top);
        pen.disc(r.hand, 3 * s, L.skin);
        (i == 0 ? res.handL : res.handR) = r.hand;
    }
    return res;
}

/* ---------------- walking figure (side view) ---------------- */
struct WalkPose {
    Look look;
    float x = 0, gy = 0, s = 1;     // feet x, ground y, scale
    int facing = 1;                 // +1 right, -1 left
    bool moving = true; float phase = 0, amp = .6f;
    float lean = 0, bow = 0;
    bool hasRaise = false; float raise = 0;   // raised front arm
    bool hasWave = false;  float wave = 0;
    bool cane = false; uint32_t caneC = rgb(0x6a, 0x4f, 0x3f);
    float alpha = 1;
    float z = 0;
};

inline Vec2 buildWalking(const WalkPose& P, std::vector<Shape>& out) {
    const Look& L = P.look;
    const float s = P.s, LL = 19.f;
    const bool f = L.female, moving = P.moving;
    const float A = moving ? P.amp : .04f;
    const float hipY = P.gy - (2 * LL + 3) * s, hrL = 8.2f * L.hs, hr = hrL * s;
    const uint32_t legC = f ? L.skin : L.bottom;
    Pen pen(out, P.z, P.alpha);

    MatrixStack ms; ms.translate(P.x, hipY); ms.scale(P.facing * s, s);

    auto leg = [&](float po) {
        float a = moving ? A * std::sin(P.phase + po) : (po != 0 ? .06f : -.06f);
        float fx = moving ? std::max(0.f, std::sin(P.phase + po + 1.9f)) * .95f : .05f;
        ms.push(); ms.rotate(-a);
        pen.poly(xpoly(ms, {{-3.4f, 0}, {3.4f, 0}, {2.9f, LL}, {-2.9f, LL}}), legC);
        pen.disc(ms.apply({0, LL}), 3 * s, legC);
        ms.translate(0, LL); ms.rotate(fx);
        pen.poly(xpoly(ms, {{-2.9f, 0}, {2.9f, 0}, {2.4f, LL}, {-2.4f, LL}}), legC);
        ms.translate(0, LL);
        pen.poly(xpoly(ms, {{-2.4f, -.5f}, {5, 0}, {5.4f, 3}, {-2.4f, 3}}), L.shoe);
        ms.pop();
    };
    auto arm = [&](float po, bool front) {
        float b = moving ? .6f * std::sin(P.phase + po) : .06f;
        float fl = .22f + (moving ? .3f * std::max(0.f, std::sin(P.phase + po + 1.2f)) : 0.f);
        if (front && P.hasRaise) { b = P.raise; fl = P.hasWave ? .5f + .55f * std::sin(P.wave) : .3f; }
        if (front && P.cane) { b = .5f; fl = .5f; }
        ms.push(); ms.translate(0, -28); ms.rotate(-b);
        pen.poly(xpoly(ms, {{-2.4f, 0}, {2.4f, 0}, {2, 13}, {-2, 13}}), L.top);
        ms.translate(0, 13); ms.rotate(-fl);
        pen.poly(xpoly(ms, {{-2, 0}, {2, 0}, {1.7f, 12}, {-1.7f, 12}}), L.shortSleeve ? L.skin : L.top);
        Vec2 h = ms.apply({0, 12.5f});
        pen.disc(h, 2.7f * s, L.skin);
        if (front && P.cane) pen.limb(h, {h.x + P.facing * 3 * s, P.gy}, 2, P.caneC);
        ms.pop();
    };

    leg(3.14159265f);
    ms.push(); ms.rotate(P.lean);
    arm(0, false);
    pen.poly(xpoly(ms, {{-4.8f, 0}, {4.8f, 0}, {5.4f, -30}, {-5.4f, -30}}), L.top);
    ms.pop();
    leg(0);
    if (f) pen.poly(xpoly(ms, {{-5, -6}, {5, -6}, {8.5f, 14}, {-8.5f, 14}}), L.top);
    ms.push(); ms.rotate(P.lean);
    arm(3.14159265f, true);
    ms.translate(0, -30); ms.rotate(P.bow); ms.translate(0, -(hrL + 1));
    auto q = [&](float x, float y) { return ms.apply({x, y}); };
    const Hair h = L.hair;
    if (h == Hair::Puff) pen.disc(q(-.35f * hrL, -.3f * hrL), hr * 1.3f, L.hairC);
    if (h == Hair::Twin) pen.disc(q(-.8f * hrL, -.2f * hrL), hr * .45f, L.hairC);
    if (h == Hair::Short || h == Hair::Bun || h == Hair::Twin) pen.disc(q(-.18f * hrL, -.22f * hrL), hr + .5f, L.hairC);
    if (h == Hair::Bald) pen.disc(q(-.7f * hrL, -.1f * hrL), hr * .35f, L.hairC);
    Vec2 fc = q(h == Hair::Puff ? .15f * hrL : .1f * hrL, .1f * hrL);
    pen.disc(fc, h == Hair::Puff ? hr * .93f : hr - .7f, L.skin);
    if (h == Hair::Bun) pen.disc(q(-.7f * hrL, -.95f * hrL), hr * .4f, L.hairC);
    pen.poly(xpoly(ms, {{hrL * .9f, -hrL * .05f}, {hrL + 2.2f, hrL * .28f}, {hrL * .85f, hrL * .42f}}), L.skin);
    pen.disc(q(hrL * .5f, -.05f * hrL), 1.f * s, rgb(0x1a, 0x14, 0x10));
    Vec2 head = ms.apply({0, 0});
    ms.pop();
    return head;
}

}  // namespace fig

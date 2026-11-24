#pragma once
// Owner: C. Seated and walking figures. Header-only.
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

inline uint32_t shadeCol(uint32_t c, float factor) {
    auto ch = [&](int shift) {
        int v = int(float((c >> shift) & 0xFFu) * factor + 0.5f);
        return uint8_t(std::clamp(v, 0, 255));
    };
    return (c & 0xFF000000u) | (uint32_t(ch(16)) << 16) | (uint32_t(ch(8)) << 8) | uint32_t(ch(0));
}

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
        s.z = z + 0.02f * float(n++);
        return s;
    }
    void poly(const std::vector<Vec2>& p, uint32_t col) { add(Kind::Poly, col).pts = p; }
    void disc(Vec2 c, float r, uint32_t col) { Shape& s = add(Kind::Disc, col); s.pts = {c}; s.r = r; }
    void limb(Vec2 a, Vec2 b, float w, uint32_t col) {
        Shape& s = add(Kind::Strip, col); s.pts = {a, b}; s.w0 = w; s.w1 = w;
    }
    void taperedLimb(Vec2 a, Vec2 b, float w0, float w1, uint32_t col) {
        Shape& s = add(Kind::Strip, col); s.pts = {a, b}; s.w0 = w0; s.w1 = w1;
    }
    void dot(Vec2 p, uint32_t col) { disc(p, 0.45f, col); }
    void line(Vec2 a, Vec2 b, uint32_t col) {
        Shape& s = add(Kind::Line, col); s.pts = {a, b}; s.w0 = 1; s.w1 = 1;
    }
};

inline std::vector<Vec2> xpoly(const MatrixStack& ms, std::initializer_list<Vec2> l) {
    std::vector<Vec2> v;
    for (const Vec2& p : l) v.push_back(ms.apply(p));
    return v;
}

struct Tint {
    float r = 1, g = 1, b = 1;
    uint32_t operator()(uint32_t hex) const {
        auto ch = [](uint32_t v, float k) { float x = float(v) * k + .5f; return uint8_t(x > 255.f ? 255.f : x); };
        return rgb(ch((hex >> 16) & 255, r), ch((hex >> 8) & 255, g), ch(hex & 255, b));
    }
};

enum class Hair { Short, Puff, Bun, Twin, Pony, Braids, Bald };

struct Look {
    bool female = false;
    Hair hair = Hair::Short;
    uint32_t skin = rgb(0x6b, 0x42, 0x26), hairC = rgb(0x16, 0x11, 0x0e);
    uint32_t shoe = rgb(0x24, 0x20, 0x28), bottom = rgb(0x25, 0x30, 0x42), top = rgb(0x4a, 0x7f, 0xb5);
    uint32_t jacket = 0; // optional open jacket/cardigan color (0 = none)
    bool shortSleeve = false;
    bool hasScarf = false; uint32_t scarf = rgb(0xc9, 0x3b, 0x3b);
    float hs = 1;
};

inline Look makeMan(const Tint& c = Tint()) {
    Look l;
    l.skin = c(0x6b4226); l.hairC = c(0x16110e); l.shoe = c(0x242028); l.bottom = c(0x253042); l.top = c(0x4a7fb5);
    return l;
}

inline Look makeWoman(const Tint& c = Tint()) {
    Look l; l.female = true; l.hair = Hair::Bun;
    l.skin = c(0x7a4b2e); l.hairC = c(0x16110e); l.shoe = c(0x2c2226); l.bottom = c(0x7a4b2e); l.top = c(0xe0607e);
    l.shortSleeve = true; return l;
}

/* ---------------- seated figure (front view) ---------------- */
struct SeatedPose {
    Look look;
    float x = 0, y = 0, s = 1;
    float lean = 0, tilt = 0, turn = 0, hunch = 0, bounce = 0;
    float knee[2] = {0, 0};
    float shin = 26;
    bool blink = false, smile = false, open = false;
    bool hasHandL = false, hasHandR = false; Vec2 handL, handR;
    float alpha = 1;
    float z = 0;
};
struct SeatedOut { Vec2 head, earL, earR, handL, handR; float hr = 0; };

inline SeatedOut buildSeated(const SeatedPose& P, std::vector<Shape>& out) {
    const Look& L = P.look;
    const float s = P.s; const bool f = L.female;
    const float hw = (f ? 9.6f : 10.6f) * s, sw = (f ? 11.4f : 13.0f) * s;
    const float T = (33.f - P.hunch * 5.f) * s, hr = 8.4f * s * L.hs;
    const float x0 = P.x, y0 = P.y - P.bounce;
    Pen pen(out, P.z, P.alpha);
    SeatedOut res;

    const float kneeY = 7.5f * s, shin = (f ? 21.f : 22.f) * s;
    const uint32_t legC = f ? L.skin : L.bottom;
    const uint32_t trimC = shadeCol(L.top, 1.15f);
    const uint32_t beltC = shadeCol(L.bottom, 0.75f);

    // Connected pelvis / hip base
    pen.poly({{x0 - hw * 1.0f, y0 - 3.0f * s}, {x0 + hw * 1.0f, y0 - 3.0f * s},
              {x0 + hw * 1.02f, y0 + 5.0f * s}, {x0 - hw * 1.02f, y0 + 5.0f * s}},
             f ? ((L.bottom != L.skin) ? L.bottom : L.top) : L.bottom);

    for (int i = 0; i < 2; ++i) {
        float sd = i == 0 ? -1.f : 1.f;
        float hx = x0 + sd * hw * .52f;
        float kx = x0 + sd * hw * .56f, ky = y0 + kneeY;
        pen.taperedLimb({hx, y0 + 1.5f * s}, {kx, ky}, 7.6f * s, 6.6f * s, legC);
        pen.disc({kx, ky}, 3.3f * s, legC);
        float ax = kx + std::sin(P.knee[i]) * (shin * 0.45f);
        float ay = ky + std::cos(P.knee[i] * 0.5f) * shin;
        pen.taperedLimb({kx, ky}, {ax, ay}, 6.2f * s, 4.8f * s, legC);
        Vec2 heel{ax - 2.6f * s, ay + 1.0f * s};
        Vec2 toe{ax + 4.2f * s, ay + 1.2f * s};
        pen.limb(heel, toe, 4.6f * s, L.shoe);
        pen.disc(toe, 2.3f * s, L.shoe);
    }

    // Skirt (uses L.bottom if set differently from skin, otherwise L.top)
    if (f) {
        uint32_t skirtC = (L.bottom != L.skin) ? L.bottom : L.top;
        pen.poly({{x0 - hw - .4f * s, y0 - 5.f * s}, {x0 + hw + .4f * s, y0 - 5.f * s},
                  {x0 + hw + 4.2f * s, y0 + 9.5f * s}, {x0 - hw - 4.2f * s, y0 + 9.5f * s}}, skirtC);
    }

    MatrixStack ms; ms.translate(x0, y0 + 1.5f * s); ms.rotate(P.lean);
    pen.poly(xpoly(ms, {{-hw, 0}, {hw, 0}, {sw, -T}, {-sw, -T}}), L.top);

    // Optional open blazer / denim jacket / cardigan panels
    if (L.jacket != 0) {
        pen.poly(xpoly(ms, {{-hw * 1.03f, 0}, {-hw * .34f, 0}, {-sw * .32f, -T}, {-sw * 1.03f, -T}}), L.jacket);
        pen.poly(xpoly(ms, {{hw * .34f, 0}, {hw * 1.03f, 0}, {sw * 1.03f, -T}, {sw * .32f, -T}}), L.jacket);
    }

    // Waistband / belt
    pen.limb(ms.apply({-hw * .98f, -1.5f * s}), ms.apply({hw * .98f, -1.5f * s}), 2.5f * s, beltC);

    Vec2 shL = ms.apply({-sw, -T + 2.5f * s}), shR = ms.apply({sw, -T + 2.5f * s});
    uint32_t shoulderC = (L.jacket != 0) ? L.jacket : L.top;
    pen.disc(shL, 4.0f * s, shoulderC); pen.disc(shR, 4.0f * s, shoulderC);

    // Neck and collar
    pen.poly(xpoly(ms, {{-3.2f * s, -T + 1.5f * s}, {3.2f * s, -T + 1.5f * s},
                        {3.2f * s, -T - 4.8f * s}, {-3.2f * s, -T - 4.8f * s}}), L.skin);
    pen.limb(ms.apply({-4.8f * s, -T + 1.0f * s}), ms.apply({4.8f * s, -T + 1.0f * s}), 2.6f * s, trimC);

    if (L.hasScarf) {
        pen.poly(xpoly(ms, {{-sw * .58f, -T + .5f * s}, {sw * .58f, -T + .5f * s},
                            {sw * .40f, -T + 5.5f * s}, {-sw * .40f, -T + 5.5f * s}}), L.scarf);
        pen.poly(xpoly(ms, {{sw * .08f, -T + 4.f * s}, {sw * .44f, -T + 4.f * s},
                            {sw * .40f, -T + 18.f * s}, {sw * .04f, -T + 18.f * s}}), L.scarf);
    }

    // Head
    ms.translate(0, -T); ms.rotate(P.tilt); ms.translate(0, -(hr + 2.5f * s));
    const float tu = P.turn * 1.6f * s;
    auto q = [&](float x, float y) { return ms.apply({x + tu, y}); };
    const Hair h = L.hair;
    if (h == Hair::Puff) pen.disc(q(0, -.3f * hr), hr * 1.38f, L.hairC);
    if (h == Hair::Twin) for (int i = 0; i < 2; ++i) pen.disc(q((i ? 1.f : -1.f) * hr * .95f, -.38f * hr), hr * .44f, L.hairC);
    if (h == Hair::Pony) pen.disc(q(hr * 1.05f, .1f * hr), hr * .45f, L.hairC);
    if (h == Hair::Braids) {
        // Shoulder-length Kenyan box braids framing both sides of the face
        pen.limb(q(-hr * .92f, -.1f * hr), q(-hr * 1.02f, hr * 1.35f), 4.2f * s * L.hs, L.hairC);
        pen.limb(q( hr * .92f, -.1f * hr), q( hr * 1.02f, hr * 1.35f), 4.2f * s * L.hs, L.hairC);
    }
    if (h == Hair::Short || h == Hair::Bun || h == Hair::Twin || h == Hair::Pony || h == Hair::Braids)
        pen.disc(q(0, -.25f * hr), hr + .8f, L.hairC);

    Vec2 fc = h == Hair::Puff ? q(0, .12f * hr) : h == Hair::Bald ? q(0, 0) : q(0, .16f * hr);
    pen.disc(fc, h == Hair::Puff ? hr * .95f : h == Hair::Bald ? hr : hr - .3f, L.skin);
    if (h == Hair::Bun) pen.disc(q(0, -hr * 1.25f), hr * .45f, L.hairC);
    if (h == Hair::Bald) for (int i = 0; i < 2; ++i)
        pen.disc(q((i ? 1.f : -1.f) * hr * .88f, -.1f * hr), std::max(1.2f, hr * .24f), L.hairC);

    // Facial features
    const uint32_t EYE = rgb(0x16, 0x11, 0x0e), MOUTH = rgb(0x3e, 0x18, 0x14);
    for (int i = 0; i < 2; ++i) {
        float side = (i ? 1.f : -1.f);
        float ex = side * hr * .36f + P.turn * hr * .28f, ey = .16f * hr;
        pen.line(q(ex - 1.3f * s, ey - 2.1f * s), q(ex + 1.3f * s, ey - 2.1f * s), L.hairC);
        if (P.blink) pen.line(q(ex - 1.2f * s, ey), q(ex + 1.2f * s, ey), EYE);
        else         pen.disc(q(ex, ey), std::max(0.9f, 1.15f * s), EYE);
    }
    pen.disc(q(P.turn * hr * .35f, .36f * hr), std::max(0.8f, 1.05f * s), shadeCol(L.skin, 0.90f));
    if (P.smile) {
        Vec2 a = q(-hr * .28f, .56f * hr), b = q(0, .72f * hr), c = q(hr * .28f, .56f * hr);
        if (P.open) pen.disc({b.x, b.y - .5f * s}, std::max(1.2f, hr * .20f), MOUTH);
        else { pen.line(a, b, MOUTH); pen.line(b, c, MOUTH); }
    }
    res.head = ms.apply({0, 0}); res.hr = hr;
    res.earL = q(-hr, .15f * hr); res.earR = q(hr, .15f * hr);

    // Arms
    const float ul = 15 * s, fl = 14 * s;
    for (int i = 0; i < 2; ++i) {
        Vec2 sh = i == 0 ? shL : shR;
        bool has = i == 0 ? P.hasHandL : P.hasHandR;
        Vec2 tgt = has ? (i == 0 ? P.handL : P.handR)
                       : Vec2{x0 + (i == 0 ? -8.2f : 8.2f) * s, y0 + 5.5f * s};
        IKResult r = ik2(sh, tgt, ul, fl, i == 0 ? 1.f : -1.f);
        pen.taperedLimb(sh, r.elbow, 5.8f * s, 5.0f * s, shoulderC);
        if (L.shortSleeve) pen.disc(r.elbow, 2.6f * s, shadeCol(shoulderC, 1.15f));
        pen.taperedLimb(r.elbow, r.hand, 4.8f * s, 4.0f * s, L.shortSleeve ? L.skin : shoulderC);
        if (!L.shortSleeve) pen.disc(r.hand, 2.5f * s, shadeCol(shoulderC, 1.15f));
        pen.disc(r.hand, 3.1f * s, L.skin);
        (i == 0 ? res.handL : res.handR) = r.hand;
    }
    return res;
}

/* ---------------- walking figure (side view) ---------------- */
struct WalkPose {
    Look look;
    float x = 0, gy = 0, s = 1;
    float facing = 1;
    bool moving = true; float phase = 0, amp = .6f;
    float lean = 0, bow = 0;
    bool hasRaise = false; float raise = 0;
    bool hasWave = false;  float wave = 0;
    bool cane = false; uint32_t caneC = 0xff374e6fu;
    float alpha = 1;
    float z = 0;
};

inline Vec2 buildWalking(const WalkPose& P, std::vector<Shape>& out) {
    const Look& L = P.look;
    const float s = P.s;
    const float LL = (s < 0.72f) ? 15.8f : 19.f; // child vs. adult leg proportions
    const float torsoH = (s < 0.72f) ? 26.f : 30.f;
    const bool f = L.female, moving = P.moving;
    const float A = moving ? P.amp : .04f;
    const float hipY = P.gy - (2 * LL + 2.5f) * s, hrL = 8.4f * L.hs, hr = hrL * s;
    const uint32_t legC = f ? L.skin : L.bottom;
    const uint32_t sleeveC = (L.jacket != 0) ? L.jacket : L.top;
    const uint32_t trimC = shadeCol(sleeveC, 1.15f);
    Pen pen(out, P.z, P.alpha);
    MatrixStack ms; ms.translate(P.x, hipY); ms.scale(P.facing * s, s);

    auto leg = [&](float po) {
        float swing = std::sin(P.phase + po);
        float a = moving ? A * swing : (po != 0 ? .06f : -.06f);
        // Natural knee flexion: bends when lifting forward/back, straightens at contact
        float fx = moving ? std::max(0.08f, std::sin(P.phase + po + 1.55f) * .85f + 0.25f) : .05f;
        ms.push(); ms.rotate(-a);
        pen.taperedLimb(ms.apply({0, 0}), ms.apply({0, LL}), 6.8f * s, 5.6f * s, legC);
        pen.disc(ms.apply({0, LL}), 2.9f * s, legC);
        ms.translate(0, LL); ms.rotate(fx);
        pen.taperedLimb(ms.apply({0, 0}), ms.apply({0, LL}), 5.6f * s, 4.6f * s, legC);
        ms.translate(0, LL);
        pen.limb(ms.apply({-1.5f, 1.2f}), ms.apply({4.2f, 1.4f}), 4.5f * s, L.shoe);
        pen.disc(ms.apply({4.2f, 1.4f}), 2.2f * s, L.shoe);
        ms.pop();
    };
    auto arm = [&](float po, bool front) {
        // Elbows stay comfortably bent (~35-55 deg) while pumping forward and back
        float b = moving ? .52f * std::sin(P.phase + po) : .08f;
        float fl = .58f + (moving ? .28f * std::max(0.f, std::sin(P.phase + po + 0.8f)) : 0.f);
        if (front && P.hasRaise) { b = P.raise; fl = P.hasWave ? .65f + .35f * std::sin(P.wave) : .45f; }
        if (front && P.cane) { b = .5f; fl = .5f; }
        ms.push(); ms.translate(0, -torsoH + 2.f); ms.rotate(-b);
        Vec2 shPt = ms.apply({0, 0});
        Vec2 elPt = ms.apply({0, 11.5f});
        pen.disc(shPt, 2.8f * s, sleeveC);
        pen.taperedLimb(shPt, elPt, 4.8f * s, 4.2f * s, sleeveC);
        ms.translate(0, 11.5f); ms.rotate(-fl);
        Vec2 wrPt = ms.apply({0, 10.5f});
        if (L.shortSleeve) pen.disc(elPt, 2.2f * s, trimC);
        pen.taperedLimb(elPt, wrPt, 4.0f * s, 3.4f * s, L.shortSleeve ? L.skin : sleeveC);
        Vec2 h = ms.apply({0, 11.0f});
        pen.disc(h, 2.7f * s, L.skin);
        if (front && P.cane) {
            pen.limb(h, {h.x + P.facing * 3 * s, P.gy}, 2.4f, P.caneC);
            pen.disc(h, 2.5f * s, shadeCol(P.caneC, 1.2f));
        }
        ms.pop();
    };

    
    auto q = [&](float x, float y) { return ms.apply({x, y}); };
    const Hair h = L.hair;
    if (h == Hair::Puff) pen.disc(q(-.35f * hrL, -.3f * hrL), hr * 1.3f, L.hairC);
    if (h == Hair::Twin) pen.disc(q(-.8f * hrL, -.2f * hrL), hr * .45f, L.hairC);
    if (h == Hair::Braids) pen.limb(q(-.35f * hrL, 0), q(-.45f * hrL, hrL * 1.3f), 4.6f * s * L.hs, L.hairC);
    if (h == Hair::Short || h == Hair::Bun || h == Hair::Twin || h == Hair::Braids)
        pen.disc(q(-.18f * hrL, -.22f * hrL), hr + .5f, L.hairC);
    if (h == Hair::Bald) pen.disc(q(-.7f * hrL, -.1f * hrL), hr * .38f, L.hairC);
    Vec2 fc = q(h == Hair::Puff ? .15f * hrL : .1f * hrL, .1f * hrL);
    pen.disc(fc, h == Hair::Puff ? hr * .93f : hr - .7f, L.skin);
    if (h == Hair::Bun) pen.disc(q(-.7f * hrL, -.95f * hrL), hr * .44f, L.hairC);
    pen.poly(xpoly(ms, {{hrL * .9f, -hrL * .05f}, {hrL + 2.2f, hrL * .28f}, {hrL * .85f, hrL * .42f}}), L.skin);
    pen.line(q(hrL * .35f, -.25f * hrL), q(hrL * .65f, -.22f * hrL), L.hairC);
    pen.disc(q(hrL * .5f, -.05f * hrL), std::max(0.85f, 1.05f * s), rgb(0x16, 0x11, 0x0e));
    Vec2 head = ms.apply({0, 0});
    ms.pop();
    return head;
}

}  // namespace fig
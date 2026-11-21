// Owner: C. Characters and their actions for scenes 5 to 8 (indices 4 to 7),
// ported from the SC[4]..SC[7] blocks of story.js. buildScene calls addScenesLate().
#include "scenes_late.h"
#include "figure.h"
#include "anim.h"
#include <algorithm>
#include <cmath>

namespace {

using namespace fig;
using anim::clamp01;
using anim::easeInOut;
using anim::lerp;

constexpr float kSeat = 266.f;

// Same curve as the path in scene.cpp, so walkers keep their feet on it.
inline float pathY(float x) {
    static std::vector<Vec2> pts;
    if (pts.empty()) {
        const Vec2 a{-20.f, 336.f}, b{140.f, 310.f}, c{300.f, 348.f}, d{690.f, 314.f};
        for (int i = 0; i <= 48; ++i) {
            float u = float(i) / 48.f, m = 1 - u;
            pts.push_back({m*m*m*a.x + 3*m*m*u*b.x + 3*m*u*u*c.x + u*u*u*d.x,
                           m*m*m*a.y + 3*m*m*u*b.y + 3*m*u*u*c.y + u*u*u*d.y});
        }
    }
    if (x <= pts.front().x) return pts.front().y;
    for (size_t i = 0; i + 1 < pts.size(); ++i)
        if (x <= pts[i + 1].x) return lerp(pts[i].y, pts[i + 1].y, (x - pts[i].x) / (pts[i + 1].x - pts[i].x));
    return pts.back().y;
}

inline float blinkAt(float t) { return std::fmod(t * .83f, 3.3f) < .13f ? 1.f : 0.f; }

// Colour grading per scene (the reference tints dusk and night).
inline Tint tintFor(int scene) {
    if (scene == 6) return Tint{.92f, .86f, .96f};
    if (scene == 7) return Tint{.4f, .48f, .78f};
    return Tint{};
}

inline uint32_t mixBlack(uint32_t c, float k) {   // darken by k, keep alpha
    auto ch = [&](uint32_t v) { return uint32_t(float(v) * (1 - k)); };
    return rgb(uint8_t(ch(c & 255)), uint8_t(ch((c >> 8) & 255)), uint8_t(ch((c >> 16) & 255)));
}

// The cane leaning on the bench (scenes 6 and 7).
inline void cane(std::vector<Shape>& v, const Tint& tn, float z) {
    Pen pen(v, z, 1.f);
    pen.limb({284, 261}, {276, 302}, 2.6f, tn(0x5a3a22));
    pen.disc({284, 260}, 2.6f, tn(0x5a3a22));
}

struct ElderOpts {
    float hunch = 0, mLean = 0, mTilt = 0, mTurn = 0; bool mSmile = false; float wAlpha = 1;
    bool hasWHand = false; Vec2 wHand{343, 268};
};

// The old couple on the bench. Returns nothing; the man is drawn first, then the woman.
inline void elders(std::vector<Shape>& v, const Tint& tn, float t, const ElderOpts& o, float z) {
    SeatedPose m; m.look = makeMan(tn);
    m.look.top = tn(0x7a5a45); m.look.bottom = tn(0x3a3f4d); m.look.hair = Hair::Bald; m.look.hairC = tn(0xc9ccd2);
    m.x = 306; m.y = kSeat; m.s = 1; m.lean = o.mLean; m.tilt = o.mTilt; m.hunch = o.hunch; m.turn = o.mTurn;
    m.hasHandR = true; m.handR = {323, 269}; m.hasHandL = true; m.handL = {285, 262};
    m.blink = blinkAt(t) > 0; m.smile = o.mSmile; m.bounce = .3f * std::sin(t * 1.6f); m.z = z;
    buildSeated(m, v);
    if (o.wAlpha > .02f) {
        SeatedPose w; w.look = makeWoman(tn);
        w.look.top = tn(0x8a5a8f); w.look.hair = Hair::Bun; w.look.hairC = tn(0xcfd2dc);
        w.look.hasScarf = true; w.look.scarf = tn(0xc93b3b);
        w.x = 336; w.y = kSeat; w.s = .97f; w.lean = -.06f; w.tilt = -.07f; w.hunch = o.hunch;
        w.hasHandL = true; w.handL = {323, 269}; w.hasHandR = true; w.handR = o.hasWHand ? o.wHand : Vec2{343, 268};
        w.blink = blinkAt(t + 1.3f) > 0; w.smile = true; w.alpha = o.wAlpha;
        w.bounce = .3f * std::sin(t * 1.6f + 1.f); w.z = z + 1.f;
        buildSeated(w, v);
    }
}

/* ---- scene 5 (index 4): three on a bench ---- */
inline void parents(std::vector<Shape>& v, const Tint& tn, float t) {
    const bool away = t > 1.6f;
    const float look = t > 2.1f ? (t < 3.8f ? .9f : .6f) : 0.f;
    SeatedPose a; a.look = makeMan(tn); a.look.top = tn(0x5c7c4a); a.look.bottom = tn(0x2b3447);
    a.x = 280; a.y = kSeat; a.s = 1.04f; a.lean = away ? .03f : .06f; a.tilt = .05f; a.turn = look;
    a.hasHandR = true; a.handR = away ? Vec2{292, 268} : Vec2{311, 250};
    a.blink = blinkAt(t) > 0; a.smile = true; a.bounce = .4f * std::sin(t * 2.f); a.z = 7.f;
    buildSeated(a, v);
    SeatedPose b; b.look = makeWoman(tn); b.look.top = tn(0xc46a2f); b.look.hasScarf = true; b.look.scarf = tn(0xc93b3b);
    b.x = 360; b.y = kSeat; b.s = .98f; b.lean = away ? -.03f : -.06f; b.tilt = -.05f; b.turn = -look;
    b.hasHandL = true; b.handL = away ? Vec2{353, 268} : Vec2{329, 250};
    b.blink = blinkAt(t + 1.f) > 0; b.smile = true; b.bounce = .4f * std::sin(t * 2.f + 1.f); b.z = 8.f;
    buildSeated(b, v);

    // the child: sits, hops down, runs off, runs back and waves
    Look kid = makeMan(tn); kid.top = tn(0xf2c14e); kid.bottom = tn(0x3b4a6b); kid.hs = 1.4f;
    const float gy = 316.f, s = .5f, hh = gy - 41.f * s;
    if (t < 1.6f) {
        float w = t * 6.f;
        SeatedPose c; c.look = kid; c.x = 320; c.y = kSeat; c.s = s;
        c.knee[0] = .5f * std::sin(w); c.knee[1] = .5f * std::sin(w + 3.14159265f);
        c.smile = true; c.open = true; c.bounce = std::fabs(std::sin(t * 7.f)); c.blink = blinkAt(t) > 0; c.z = 9.f;
        buildSeated(c, v);
    } else if (t < 2.1f) {
        float u = (t - 1.6f) / .5f;
        SeatedPose c; c.look = kid; c.x = lerp(320, 338, u); c.s = s;
        c.y = lerp(kSeat, hh, u) - 14.f * std::sin(3.14159265f * u);
        c.knee[0] = .7f; c.knee[1] = -.6f; c.smile = true; c.open = true; c.z = 9.f;
        buildSeated(c, v);
    } else {
        float x, face;
        if (t < 3.6f)      { x = 338 + (t - 2.1f) * 290; face = 1; }
        else if (t < 3.9f) { x = 775; face = 1; }
        else if (t < 5.0f) { x = 775 - (t - 3.9f) * 290; face = -1; }
        else               { x = 456; face = -1; }
        const bool run = t < 5.0f;
        WalkPose k; k.look = kid; k.x = x; k.gy = gy; k.s = s; k.facing = face;
        k.moving = run; k.phase = t * 14.f; k.amp = .85f;
        k.hasRaise = !run; k.raise = 2.5f; k.hasWave = !run; k.wave = t * 9.f; k.z = 9.f;
        buildWalking(k, v);
    }
}

/* ---- scene 6 (index 5): golden hour, a visit ---- */
inline void goldenHour(std::vector<Shape>& v, const Tint& tn, float t) {
    cane(v, tn, 7.f);
    const bool wave = t > 3.f && t < 4.8f;
    ElderOpts o; o.hunch = .7f; o.mLean = .05f; o.mTilt = .06f; o.mTurn = wave ? .8f : 0.f; o.mSmile = wave; o.wAlpha = 1;
    if (wave) { o.hasWHand = true; o.wHand = {358, 233 + 5 * std::sin(t * 9.f)}; }
    elders(v, tn, t, o, 7.5f);

    float xa, face; bool moving = true, raised = false;
    if (t < 3.f)        { xa = lerp(745, 470, clamp01((t - .6f) / 2.4f)); face = -1; }
    else if (t < 4.8f)  { xa = 470; face = -1; moving = false; raised = true; }
    else {
        float u = easeInOut((t - 4.8f) / .4f), w = std::max(0.f, t - 5.2f);
        face = lerp(-1, 1, u); xa = 470 + w * w * 70 + w * 40;
    }
    const float xc = xa + face * 26;
    Look adult = makeWoman(tn); adult.top = tn(0xe9a23b);
    Look kid = makeMan(tn); kid.top = tn(0x4db38a); kid.bottom = tn(0x3b4a6b); kid.hs = 1.35f;

    WalkPose k; k.look = kid; k.x = xc; k.gy = pathY(xc) - 4; k.s = .5f; k.facing = face;
    k.moving = moving; k.phase = t * 7.f; k.amp = .7f; k.hasRaise = !moving; k.raise = 2.4f; k.hasWave = !moving; k.wave = t * 8.f; k.z = 11.f;
    buildWalking(k, v);
    WalkPose a; a.look = adult; a.x = xa; a.gy = pathY(xa) - 4; a.s = .98f; a.facing = face;
    a.moving = moving; a.phase = t * 7.f; a.amp = .6f; a.hasRaise = raised; a.raise = 2.5f; a.hasWave = raised; a.wave = t * 8.f; a.z = 12.f;
    buildWalking(a, v);
}

/* ---- scene 7 (index 6): the passing ---- */
inline void passing(std::vector<Shape>& v, const Tint& tn, float t) {
    cane(v, tn, 7.f);
    const float wa = 1.f - easeInOut((t - 1.6f) / 2.2f);
    const float sa = t < 4.8f ? easeInOut((t - 2.2f) / .8f) : 1.f - easeInOut((t - 4.8f) / 1.4f);
    if (sa > .02f) {      // her scarf stays behind a while
        Pen pen(v, 7.2f, sa);
        uint32_t c = tn(0xc93b3b), cd = mixBlack(c, .25f);
        pen.poly({{326, 261}, {348, 261}, {353, 266}, {323, 266}}, c);
        pen.poly({{326, 266}, {337, 266}, {335, 286}, {327, 286}}, cd);
        pen.poly({{340, 266}, {350, 266}, {352, 280}, {343, 281}}, c);
    }
    float look = easeInOut((t - 1.6f) / .8f) * (1.f - .6f * easeInOut((t - 4.f) / 1.5f));
    ElderOpts o; o.hunch = .7f + .3f * easeInOut((t - 3.f) / 2.f); o.mLean = .05f + .04f * easeInOut((t - 3.f) / 2.f);
    o.mTilt = .06f + .08f * easeInOut((t - 4.f) / 1.5f); o.mTurn = look; o.mSmile = false; o.wAlpha = wa;
    elders(v, tn, t, o, 8.f);
}

/* ---- scene 8 (index 7): night, he walks away ---- */
inline void night(std::vector<Shape>& v, const Tint& tn, float t) {
    const float face = lerp(-1, 1, easeInOut((t - 2.4f) / .5f));
    const float dt = std::max(0.f, t - 2.9f);
    const float x = 428 + (dt > 0 ? std::pow(dt, 1.15f) * 48.f : 0.f);
    WalkPose m; m.look = makeMan(tn); m.look.top = tn(0x6a4f3f); m.look.bottom = tn(0x3a3f4d);
    m.look.hair = Hair::Bald; m.look.hairC = tn(0xc9ccd2);
    m.x = x; m.gy = pathY(x) - 6; m.s = 1; m.facing = face; m.moving = dt > 0; m.phase = (x - 428) * .13f; m.amp = .42f;
    m.lean = .2f; m.bow = t < 2.4f ? .3f : .1f; m.cane = true; m.caneC = tn(0x5a3a22); m.z = 8.f;
    buildWalking(m, v);
    // The falling leaf is part of the environment, so it stays in scene.cpp.
}

}  // namespace

// t is seconds inside the scene. Does nothing for scenes 1 to 4.
void addScenesLate(std::vector<Shape>& v, int scene, float t) {
    const fig::Tint tn = tintFor(scene);
    switch (scene) {
        case 4: parents(v, tn, t);    break;
        case 5: goldenHour(v, tn, t); break;
        case 6: passing(v, tn, t);    break;
        case 7: night(v, tn, t);      break;
        default: break;
    }
}

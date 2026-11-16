// Owner: D. Environment: sky, sun, hills, lawn, path, tree, lamp, bench.
// Figures (C) are added later inside buildScene; keep the signature.
#include "shape.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

int sceneCount() { return 8; }

namespace {

using Scene = std::vector<Shape>;

constexpr float W = 640.f, H = 360.f;
constexpr float HORIZON = 242.f;

// z layers: sky 0, sun 0.5, far hill 1, lawn 2, path 3, tree 4, lamp 5, bench 6.
// Figures should use z >= 7.

// ---------- helpers (polygons only, so any rasterizer can draw them) ----------
void addPoly(Scene& v, std::vector<Vec2> pts, uint32_t c, float z) {
    Shape s; s.kind = Kind::Poly; s.pts = std::move(pts); s.rgba = c; s.z = z;
    v.push_back(std::move(s));
}
void addRect(Scene& v, float x0, float y0, float x1, float y1, uint32_t c, float z) {
    addPoly(v, { {x0, y0}, {x1, y0}, {x1, y1}, {x0, y1} }, c, z);
}
void addDisc(Scene& v, float x, float y, float r, uint32_t c, float z) {
    Shape s; s.kind = Kind::Disc; s.pts = { {x, y} }; s.r = r; s.rgba = c; s.z = z;
    v.push_back(std::move(s));
}
// a thick segment as a quad: width wa at a, wb at b
void addSeg(Scene& v, Vec2 a, Vec2 b, float wa, float wb, uint32_t c, float z) {
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-3f) return;
    float nx = -dy / len, ny = dx / len;
    addPoly(v, { {a.x + nx * wa * 0.5f, a.y + ny * wa * 0.5f},
                 {b.x + nx * wb * 0.5f, b.y + ny * wb * 0.5f},
                 {b.x - nx * wb * 0.5f, b.y - ny * wb * 0.5f},
                 {a.x - nx * wa * 0.5f, a.y - ny * wa * 0.5f} }, c, z);
}

// blend two 0xAABBGGRR colours channel by channel
uint32_t mix(uint32_t a, uint32_t b, float k) {
    uint32_t out = 0;
    for (int s = 0; s < 32; s += 8) {
        float x = float((a >> s) & 255u), y = float((b >> s) & 255u);
        out |= uint32_t(x + (y - x) * k + 0.5f) << s;
    }
    return out;
}
uint32_t withAlpha(uint32_t c, uint8_t a) { return (c & 0x00FFFFFFu) | (uint32_t(a) << 24); }

Vec2 bezier(Vec2 a, Vec2 b, Vec2 c, Vec2 d, float t) {
    float u = 1.f - t;
    float w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
    return { w0 * a.x + w1 * b.x + w2 * c.x + w3 * d.x,
             w0 * a.y + w1 * b.y + w2 * c.y + w3 * d.y };
}

// sample a cubic Bezier into a polyline (used for hills and path)
std::vector<Vec2> sampleBezier(Vec2 a, Vec2 b, Vec2 c, Vec2 d, int n) {
    std::vector<Vec2> out;
    out.reserve(n + 1);
    for (int i = 0; i <= n; ++i)
        out.push_back(bezier(a, b, c, d, float(i) / float(n)));
    return out;
}

// deterministic "random" in [-1, 1] so the tree is identical every frame
float rnd(int n) {
    uint32_t h = uint32_t(n) * 2654435761u;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
    return float(h & 0xffffu) / 65535.f * 2.f - 1.f;
}

// ---------- per-scene look ----------
struct Env {
    uint32_t skyTop, skyBot, sun, hillFar, lawn, leaf, trunk;
    float sunAng;   // 0..PI across the sky
    float tree;     // 0 sapling .. 1 full grown
    float leafAmt;  // 0 bare .. 1 full
    float dim;      // darkens bench and lamp pole at dusk and night
    bool  lampOn;
    bool  plaque;
};

const Env& envFor(int scene) {
    static const Env table[8] = {
      /*0 kids, morning   */ { rgb(135,195,240), rgb(255,235,205), rgb(255,230,150), rgb(150,185,170), rgb(122,184,98), rgb(124,184,86), rgb(110,82,60), 0.35f, 0.25f, 0.5f, 0.00f, false, false },
      /*1 middle school   */ { rgb(125,185,238), rgb(225,238,248), rgb(255,235,160), rgb(135,175,160), rgb(111,174,98), rgb(98,168,76),  rgb(105,78,58), 0.70f, 0.50f, 0.8f, 0.00f, false, false },
      /*2 teens, midday   */ { rgb(100,170,235), rgb(205,230,250), rgb(255,242,180), rgb(120,165,150), rgb(98,170,86),  rgb(70,150,64),  rgb(100,74,54), 1.10f, 0.80f, 1.0f, 0.00f, false, false },
      /*3 young adults    */ { rgb(80,155,235),  rgb(195,225,250), rgb(255,247,200), rgb(115,160,145), rgb(92,165,80),  rgb(60,142,58),  rgb(98,72,52),  1.57f, 1.00f, 1.0f, 0.00f, false, false },
      /*4 parents, autumn */ { rgb(115,170,225), rgb(238,226,200), rgb(255,228,150), rgb(140,160,130), rgb(135,160,80), rgb(196,150,52), rgb(96,70,50),  2.05f, 1.00f, 0.9f, 0.05f, false, false },
      /*5 elderly, golden */ { rgb(225,165,120), rgb(255,215,150), rgb(255,200,110), rgb(150,130,115), rgb(150,140,70), rgb(214,110,42), rgb(90,65,48),  2.60f, 1.00f, 0.7f, 0.15f, false, false },
      /*6 the passing     */ { rgb(110,85,140),  rgb(240,150,110), rgb(255,165,95),  rgb(95,80,100),   rgb(95,100,62),  rgb(150,90,40),  rgb(80,60,45),  2.95f, 1.00f, 0.0f, 0.30f, true,  false },
      /*7 epilogue, night */ { rgb(14,20,52),    rgb(42,52,92),    rgb(225,230,245), rgb(28,34,62),    rgb(30,48,46),   rgb(80,70,40),   rgb(60,48,40),  1.90f, 1.00f, 0.0f, 0.55f, true,  true  },
    };
    return table[std::clamp(scene, 0, 7)];
}

// ---------- sky and sun ----------
void addSky(Scene& v, const Env& e) {
    const int N = 12;
    for (int i = 0; i < N; ++i) {
        float y0 = (HORIZON + 6.f) * i / N, y1 = (HORIZON + 6.f) * (i + 1) / N + 1.f;
        addRect(v, 0, y0, W, y1, mix(e.skyTop, e.skyBot, float(i) / (N - 1)), 0.f);
    }
    float sx = W * 0.5f - std::cos(e.sunAng) * 255.f;
    float sy = 305.f - std::sin(e.sunAng) * 255.f;
    addDisc(v, sx, sy, 15.f, e.sun, 0.5f);
}

// ---------- ground: far/near hills, lawn, path ----------
void addGround(Scene& v, const Env& e) {
    // far hill
    std::vector<Vec2> hillF = sampleBezier({-20.f, 236.f}, {110.f, 168.f}, {250.f, 196.f}, {330.f, 226.f}, 22);
    auto hillF2 = sampleBezier({330.f, 226.f}, {420.f, 258.f}, {540.f, 176.f}, {680.f, 224.f}, 22);
    hillF.insert(hillF.end(), hillF2.begin() + 1, hillF2.end());
    hillF.push_back({680.f, 270.f});
    hillF.push_back({-20.f, 270.f});
    addPoly(v, std::move(hillF), e.hillFar, 1.f);

    // near hill
    std::vector<Vec2> hillN = sampleBezier({-20.f, 252.f}, {100.f, 220.f}, {220.f, 212.f}, {320.f, 240.f}, 22);
    auto hillN2 = sampleBezier({320.f, 240.f}, {430.f, 262.f}, {560.f, 214.f}, {680.f, 236.f}, 22);
    hillN.insert(hillN.end(), hillN2.begin() + 1, hillN2.end());
    hillN.push_back({680.f, 270.f});
    hillN.push_back({-20.f, 270.f});
    addPoly(v, std::move(hillN), mix(e.hillFar, rgb(70, 85, 75), 0.35f), 1.5f);

    addRect(v, 0, HORIZON, W, H, e.lawn, 2.f);

    // path: a wavy horizontal strip across the bottom (no longer crossing the bench)
    std::vector<Vec2> pathPts = sampleBezier({-20.f, 336.f}, {140.f, 310.f}, {300.f, 348.f}, {690.f, 314.f}, 48);
    uint32_t pathColor = mix(e.lawn, rgb(205, 185, 145), 0.7f);
    uint32_t pathEdge = mix(pathColor, rgb(0, 0, 0), 0.1f);
    const int Np = (int)pathPts.size() - 1;
    for (int i = 0; i < Np; ++i) {
        float t0 = float(i) / float(Np), t1 = float(i + 1) / float(Np);
        float wa = 30.f + (40.f - 30.f) * t0;
        float wb = 30.f + (40.f - 30.f) * t1;
        addSeg(v, pathPts[i], pathPts[i + 1], wa, wb, pathEdge, 3.f);
    }
    for (int i = 0; i < Np; ++i) {
        float t0 = float(i) / float(Np), t1 = float(i + 1) / float(Np);
        float wa = 25.f + (34.f - 25.f) * t0;
        float wb = 25.f + (34.f - 25.f) * t1;
        addSeg(v, pathPts[i], pathPts[i + 1], wa, wb, pathColor, 3.1f);
    }
}

// ---------- tree (recursive, polygons only) ----------
void branch(Scene& v, const Env& e, Vec2 a, float ang, float len, float wid, int depth, int id) {
    Vec2 b{ a.x + std::sin(ang) * len, a.y - std::cos(ang) * len };
    addSeg(v, a, b, wid, wid * 0.7f, e.trunk, 4.f);
    if (depth <= 1 && e.leafAmt > 0.f) {
        int n = std::max(1, int(e.leafAmt * (depth == 0 ? 4 : 2) + 0.5f));
        float r = (9.f + 5.f * e.leafAmt) * (0.6f + 0.4f * e.tree);
        for (int i = 0; i < n; ++i) {
            uint32_t c = mix(e.leaf, rgb(255, 255, 255), 0.12f * (0.5f + 0.5f * rnd(id * 17 + i)));
            addDisc(v, b.x + rnd(id * 7 + i) * 14.f, b.y + rnd(id * 11 + i) * 12.f,
                    r + 4.f * rnd(id * 13 + i), c, 4.2f);
        }
    }
    if (depth == 0) return;
    float spread = 0.45f + 0.15f * rnd(id);
    branch(v, e, b, ang - spread, len * 0.75f, wid * 0.7f, depth - 1, id * 2);
    branch(v, e, b, ang + spread, len * 0.75f, wid * 0.7f, depth - 1, id * 2 + 1);
}
void addTree(Scene& v, const Env& e) {
    int depth = 2 + int(3.f * e.tree + 0.5f);
    branch(v, e, { 126.f, 300.f }, 0.f, 25.f + 55.f * e.tree, 4.f + 11.f * e.tree, depth, 1);
}

// ---------- lamp (right side, matching the reference scene) ----------
void addLamp(Scene& v, const Env& e) {
    const float x = 528.f, z = 5.f;
    uint32_t metal = mix(rgb(52, 56, 64), rgb(10, 12, 20), e.dim);
    addRect(v, x - 3, 178, x + 3, 298, metal, z);                                    // pole
    addRect(v, x - 7, 296, x + 7, 300, metal, z);                                    // base
    addSeg(v, {x - 0.5f, 180.f}, {513.f, 172.f}, 3.f, 3.f, metal, z);              // arm
    addPoly(v, { {502.f, 171.f}, {524.f, 171.f}, {520.f, 181.f}, {506.f, 181.f} }, metal, z); // head
    if (e.lampOn) addDisc(v, 513.f, 184.f, 62.f, withAlpha(rgb(255, 225, 140), 70), z - 0.1f); // glow
    addDisc(v, 513.f, 183.f, 6.f, e.lampOn ? rgb(255, 240, 180) : rgb(205, 205, 195), z + 0.1f);
}

// ---------- bench (centered, no longer crossing the path) ----------
void addBench(Scene& v, const Env& e) {
    const float z = 6.f;
    uint32_t wood = mix(rgb(47, 154, 119), rgb(10, 20, 30), e.dim);
    uint32_t dark = mix(rgb(33, 108, 84),  rgb(10, 20, 30), e.dim);
    uint32_t light = mix(wood, rgb(255, 255, 255), 0.18f);
    for (const float x : {248.f, 388.f}) {
        addPoly(v, { {x - 4.f, 266.f}, {x + 4.f, 266.f}, {x + 3.f, 298.f}, {x - 3.f, 298.f} }, dark, z); // legs
        addRect(v, x - 7.f, 297.f, x + 7.f, 300.f, dark, z);   // feet
        addRect(v, x - 3.f, 224.f, x + 3.f, 268.f, dark, z);   // back posts
    }
    for (const float y : {226.f, 236.f, 246.f})
        addRect(v, 238.f, y, 402.f, y + 7.f, wood, z);          // back slats
    addPoly(v, { {232.f, 265.f}, {408.f, 265.f}, {410.f, 270.f}, {230.f, 270.f} }, light, z); // seat front
    addRect(v, 230.f, 270.f, 410.f, 276.f, wood, z);          // seat
    if (e.plaque) addRect(v, 360.f, 237.f, 380.f, 243.f, rgb(212, 184, 92), z + 0.1f);
}

} // namespace

std::vector<Shape> buildScene(int scene, float t) {
    (void)t;
    const Env& e = envFor(scene);
    Scene v;
    addSky(v, e);
    addGround(v, e);
    addTree(v, e);
    addLamp(v, e);
    addBench(v, e);
    // C's seated and walking figures get added here (z >= 7)
    return v;
}

// Owner: D. Environment: sky, sun, hills, lawn, path, tree, lamp, bench.
// Figures (C) are added later inside buildScene; keep the signature.
#include "shape.h"
#include "figure.h"
#include "anim.h"
#include "scene_params.h"
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
void addEllipse(Scene& v, float cx, float cy, float rx, float ry, uint32_t c, float z) {
    constexpr int samples = 64;
    constexpr float tau = 6.28318530718f;
    std::vector<Vec2> pts;
    pts.reserve(samples);
    for (int i = 0; i < samples; ++i) {
        float angle = tau * float(i) / samples;
        pts.push_back({cx + rx * std::cos(angle), cy + ry * std::sin(angle)});
    }
    addPoly(v, std::move(pts), c, z);
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
void addSmoothSeg(Scene& v, Vec2 a, Vec2 b, float wa, float wb, uint32_t c, float z) {
    addSeg(v, a, b, wa, wb, c, z);
    addDisc(v, a.x, a.y, wa * 0.5f, c, z + 0.05f);
    addDisc(v, b.x, b.y, wb * 0.5f, c, z + 0.05f);
}
void addLeaf(Scene& v, Vec2 base, float ang, float len, float width, uint32_t c, float z) {
    Vec2 d{std::sin(ang), -std::cos(ang)};
    Vec2 n{-d.y, d.x};
    auto point = [base, d, n, len, width](float along, float across) {
        return Vec2{base.x + d.x * len * along + n.x * width * across,
                    base.y + d.y * len * along + n.y * width * across};
    };
    addPoly(v, {point(0.f, 0.f), point(0.28f, 0.28f), point(0.68f, 0.5f),
                point(1.f, 0.f), point(0.68f, -0.5f), point(0.28f, -0.28f)}, c, z);
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

void addWornPatches(Scene& v, float x0, float y0, float x1, float y1, float wear,
                    uint32_t material, uint32_t variation, float z, int seed) {
    float width = x1 - x0, height = y1 - y0;
    if (wear <= 0.f || width <= 1.f || height <= 1.f) return;

    int count = std::max(1, int(std::ceil(wear * 12.f)));
    bool horizontal = width >= height;
    float longSize = horizontal ? width : height;
    float shortSize = horizontal ? height : width;
    float longStart = horizontal ? x0 : y0;
    float shortStart = horizontal ? y0 : x0;
    constexpr float tau = 6.28318530718f;

    for (int i = 0; i < count; ++i) {
        float position = 0.05f + 0.9f * (0.5f + 0.5f * rnd(seed * 31 + i));
        float centerLong = longStart + position * longSize;
        float centerShort = shortStart + (0.2f + 0.6f * (0.5f + 0.5f * rnd(seed * 17 + i))) * shortSize;
        float patchLong = longSize * wear / (count * 0.35f)
                        * (0.8f + 0.4f * (0.5f + 0.5f * rnd(seed * 47 + i)));
        float patchShort = shortSize * (0.45f + 0.5f * (0.5f + 0.5f * rnd(seed * 59 + i)));
        uint32_t color = mix(material, variation, 0.15f * (0.5f + 0.5f * rnd(seed * 71 + i)));
        std::vector<Vec2> patch;
        patch.reserve(8);
        for (int k = 0; k < 8; ++k) {
            float angle = tau * float(k) / 8.f;
            float irregularity = 0.82f + 0.3f * (0.5f + 0.5f * rnd(seed * 89 + i * 11 + k));
            float along = std::cos(angle) * patchLong * 0.5f * irregularity;
            float across = std::sin(angle) * patchShort * 0.5f * irregularity;
            float x = horizontal ? centerLong + along : centerShort + across;
            float y = horizontal ? centerShort + across : centerLong + along;
            patch.push_back({std::min(x1, std::max(x0, x)), std::min(y1, std::max(y0, y))});
        }
        addPoly(v, std::move(patch), color, z);
    }
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
    int   leafCount;
    float lampIntensity;
    float benchWear;
};

const Env& envFor(int scene) {
    static const std::array<Env, 8> table = [] {
        std::array<Env, 8> envs{};
        for (size_t i = 0; i < envs.size(); ++i) {
            const SceneParams& p = kSceneParams[i];
            envs[i] = {
                rgb(p.skyTop.r,p.skyTop.g,p.skyTop.b),
                rgb(p.skyBottom.r,p.skyBottom.g,p.skyBottom.b),
                rgb(p.sun.r,p.sun.g,p.sun.b),
                rgb(p.hill.r,p.hill.g,p.hill.b),
                rgb(p.ground.r,p.ground.g,p.ground.b),
                rgb(p.leaves.r,p.leaves.g,p.leaves.b),
                rgb(p.trunk.r,p.trunk.g,p.trunk.b),
                p.sunAngle,p.treeSize,p.leafDensity,p.darkness,
                p.lampOn,p.plaque,p.leafCount,p.lampPower,p.benchWear
            };
        }
        return envs;
    }();
    return table[std::min(std::max(scene, 0), 7)];
}

struct GrassLook {
    float dryness;
    int tufts;
    int weeds;
    int flowers;
    int dirtPatches;
    float bladeHeight;
};

const GrassLook& grassLookFor(int scene) {
    static const GrassLook table[8] = {
        /*0*/ {0.00f, 110, 0,  0,  0, 4.0f},
        /*1*/ {0.00f, 160, 0,  0,  0, 5.0f},
        /*2*/ {0.00f, 170, 30, 12, 0, 6.0f},
        /*3*/ {0.00f, 210, 48, 22, 0, 7.0f},
        /*4*/ {0.26f, 200, 0,  0,  0, 6.5f},
        /*5*/ {0.45f, 220, 0,  0,  0, 7.0f},
        /*6*/ {0.80f, 250, 0,  0,  0, 7.5f},
        /*7*/ {0.60f,  70, 0,  0, 18, 4.0f},
    };
    return table[std::min(std::max(scene, 0), 7)];
}

void addGrassTuft(Scene& v, Vec2 base, float height, uint32_t color, int seed, float z) {
    const int blades = 3 + int(2.f * (0.5f + 0.5f * rnd(seed * 13 + 1)));
    for (int i = 0; i < blades; ++i) {
        float side = float(i) - float(blades - 1) * 0.5f;
        float startX = base.x + side * 0.75f;
        float tipX = startX + side * 0.9f + 1.6f * rnd(seed * 17 + i);
        float bladeHeight = height * (0.7f + 0.3f * (0.5f + 0.5f * rnd(seed * 19 + i)));
        addSeg(v, {startX, base.y}, {tipX, base.y - bladeHeight}, 1.25f, 0.15f,
               color, z);
    }
}

void addFlower(Scene& v, Vec2 base, float height, int seed) {
    const float topY = base.y - height;
    uint32_t stem = rgb(58, 112, 42);
    addSeg(v, base, {base.x, topY}, 0.9f, 0.55f, stem, 2.22f);
    const uint32_t petals = (seed % 3 == 0) ? rgb(255, 242, 207)
                            : (seed % 3 == 1) ? rgb(255, 220, 95)
                                              : rgb(225, 194, 245);
    constexpr float tau = 6.28318530718f;
    for (int i = 0; i < 5; ++i) {
        float angle = tau * float(i) / 5.f;
        addDisc(v, base.x + std::cos(angle) * 1.6f,
                topY + std::sin(angle) * 1.6f, 1.3f, petals, 2.3f);
    }
    addDisc(v, base.x, topY, 1.f, rgb(245, 174, 49), 2.31f);
}

void addDirtPatch(Scene& v, Vec2 center, float rx, float ry, uint32_t color, int seed) {
    constexpr float tau = 6.28318530718f;
    std::vector<Vec2> patch;
    patch.reserve(9);
    for (int i = 0; i < 9; ++i) {
        float angle = tau * float(i) / 9.f;
        float uneven = 0.78f + 0.35f * (0.5f + 0.5f * rnd(seed * 23 + i));
        patch.push_back({center.x + std::cos(angle) * rx * uneven,
                         center.y + std::sin(angle) * ry * uneven});
    }
    addPoly(v, std::move(patch), color, 2.08f);
}

void addGrass(Scene& v, const Env& e, int scene) {
    const GrassLook& look = grassLookFor(scene);
    const uint32_t dryGround = scene == 7
        ? mix(rgb(190, 150, 86), rgb(38, 34, 30), e.dim)
        : rgb(201, 164, 88);
    const uint32_t green = mix(e.lawn, rgb(72, 158, 56), 0.42f);
    const uint32_t dry = mix(rgb(191, 157, 83), rgb(75, 60, 43), e.dim);
    addRect(v, 0, HORIZON, W, H, mix(e.lawn, dryGround, look.dryness), 2.f);

    for (int i = 0; i < look.dirtPatches; ++i) {
        float x = 18.f + (W - 36.f) * (0.5f + 0.5f * rnd(401 + i * 7));
        float y = 254.f + 53.f * (0.5f + 0.5f * rnd(419 + i * 11));
        float rx = 7.f + 12.f * (0.5f + 0.5f * rnd(431 + i * 13));
        float ry = 2.5f + 4.5f * (0.5f + 0.5f * rnd(443 + i * 17));
        uint32_t soil = (i % 3 == 0)
            ? mix(rgb(147, 108, 65), rgb(50, 42, 34), e.dim)
            : mix(rgb(121, 88, 55), rgb(42, 37, 32), e.dim);
        addDirtPatch(v, {x, y}, rx, ry, soil, 457 + i);
    }

    for (int i = 0; i < look.tufts; ++i) {
        float x = 14.f + (W - 28.f) * (0.5f + 0.5f * rnd(503 + i * 19));
        float y = 254.f + 53.f * (0.5f + 0.5f * rnd(521 + i * 23));
        float height = look.bladeHeight * (0.75f + 0.5f * (0.5f + 0.5f * rnd(541 + i * 29)));
        float dryMix = look.dryness * (0.7f + 0.3f * (0.5f + 0.5f * rnd(557 + i * 31)));
        uint32_t bladeColor = mix(green, dry, dryMix);
        addGrassTuft(v, {x, y}, height, bladeColor, 571 + i, 2.18f);
    }

    for (int i = 0; i < look.weeds; ++i) {
        float x = 18.f + (W - 36.f) * (0.5f + 0.5f * rnd(601 + i * 37));
        float y = 258.f + 46.f * (0.5f + 0.5f * rnd(617 + i * 41));
        float height = look.bladeHeight + 2.5f + 4.f * (0.5f + 0.5f * rnd(631 + i * 43));
        uint32_t weedColor = mix(green, rgb(49, 117, 42), 0.3f);
        addGrassTuft(v, {x, y}, height, weedColor, 647 + i, 2.2f);
    }

    for (int i = 0; i < look.flowers; ++i) {
        float x = 20.f + (W - 40.f) * (0.5f + 0.5f * rnd(701 + i * 47));
        float y = 260.f + 43.f * (0.5f + 0.5f * rnd(719 + i * 53));
        addFlower(v, {x, y}, look.bladeHeight + 4.f, i);
    }
}

// ---------- sky and sun ----------
void addSky(Scene& v, const Env& e) {
    const int rows = int(HORIZON) + 6;
    for (int y = 0; y < rows; ++y) {
        float t = float(y) / float(rows - 1);
        addRect(v, 0, float(y), W, float(y + 1),
                mix(e.skyTop, e.skyBot, t), 0.f);
    }
    float sx = W * 0.5f - std::cos(e.sunAng) * 255.f;
    float sy = 305.f - std::sin(e.sunAng) * 255.f;
    // Smooth shaded sphere (concentric discs) instead of a flat disc.
    uint32_t sunEdge = mix(e.sun, rgb(60, 40, 30), 0.55f);
    for (float r = 15.f; r >= 0.f; r -= 0.5f) {
        float t = (15.f - r) / 15.f;
        float shade = t * t;
        uint32_t c = mix(e.sun, sunEdge, shade);
        addDisc(v, sx, sy, r, c, 0.5f);
    }
}

// ---------- ground: far/near hills, lawn, path ----------
void addGround(Scene& v, const Env& e, int scene) {
    // far hill
    std::vector<Vec2> hillF = sampleBezier({-20.f, 236.f}, {110.f, 168.f}, {250.f, 196.f}, {330.f, 226.f}, 96);
    auto hillF2 = sampleBezier({330.f, 226.f}, {420.f, 258.f}, {540.f, 176.f}, {680.f, 224.f}, 96);
    hillF.insert(hillF.end(), hillF2.begin() + 1, hillF2.end());
    hillF.push_back({680.f, 270.f});
    hillF.push_back({-20.f, 270.f});
    addPoly(v, std::move(hillF), e.hillFar, 1.f);

    // near hill
    std::vector<Vec2> hillN = sampleBezier({-20.f, 252.f}, {100.f, 220.f}, {220.f, 212.f}, {320.f, 240.f}, 96);
    auto hillN2 = sampleBezier({320.f, 240.f}, {430.f, 270.f}, {560.f, 214.f}, {680.f, 236.f}, 96);
    hillN.insert(hillN.end(), hillN2.begin() + 1, hillN2.end());
    hillN.push_back({680.f, 270.f});
    hillN.push_back({-20.f, 270.f});
    addPoly(v, std::move(hillN), mix(e.hillFar, rgb(70, 85, 75), 0.35f), 1.5f);

    addGrass(v, e, scene);

    // Keep the original foreground path position; fill each edge as one polygon.
    std::vector<Vec2> pathPts = sampleBezier(
        {-20.f, 336.f}, {140.f, 310.f}, {300.f, 348.f}, {690.f, 314.f}, 96);
    uint32_t pathColor = mix(e.lawn, rgb(205, 185, 145), 0.7f);
    uint32_t pathEdge = mix(pathColor, rgb(0, 0, 0), 0.1f);
    auto pathPolygon = [&pathPts](float farWidth, float nearWidth) {
        std::vector<Vec2> left, right;
        left.reserve(pathPts.size());
        right.reserve(pathPts.size());
        for (size_t i = 0; i < pathPts.size(); ++i) {
            const Vec2 before = pathPts[i == 0 ? i : i - 1];
            const Vec2 after = pathPts[i + 1 < pathPts.size() ? i + 1 : i];
            float dx = after.x - before.x, dy = after.y - before.y;
            float len = std::hypot(dx, dy);
            if (len < 1e-3f) continue;
            float t = float(i) / float(pathPts.size() - 1);
            float halfWidth = (farWidth + (nearWidth - farWidth) * t) * 0.5f;
            float nx = -dy / len, ny = dx / len;
            left.push_back({pathPts[i].x + nx * halfWidth, pathPts[i].y + ny * halfWidth});
            right.push_back({pathPts[i].x - nx * halfWidth, pathPts[i].y - ny * halfWidth});
        }
        left.insert(left.end(), right.rbegin(), right.rend());
        return left;
    };
    addPoly(v, pathPolygon(30.f, 40.f), pathEdge, 3.f);
    addPoly(v, pathPolygon(25.f, 34.f), pathColor, 3.1f);
}

// ---------- tree (recursive, polygons only) ----------
void branch(Scene& v, const Env& e, Vec2 a, float ang, float len, float wid, int depth, int id,
            int leafNodeCount, int& leafNode) {
    Vec2 b{ a.x + std::sin(ang) * len, a.y - std::cos(ang) * len };
    Vec2 direction{std::sin(ang), -std::cos(ang)};
    Vec2 normal{std::cos(ang), std::sin(ang)};
    float bend = 0.1f * len * rnd(id * 29 + depth);
    Vec2 c1{a.x + direction.x * len / 3.f + normal.x * bend * 0.25f,
            a.y + direction.y * len / 3.f + normal.y * bend * 0.25f};
    Vec2 c2{a.x + direction.x * len * 2.f / 3.f + normal.x * bend,
            a.y + direction.y * len * 2.f / 3.f + normal.y * bend};
    const int segments = std::max(2, int(std::ceil(len / 10.f)));
    std::vector<Vec2> branchPts = sampleBezier(a, c1, c2, b, segments);
    for (int i = 0; i < segments; ++i) {
        float t0 = float(i) / segments, t1 = float(i + 1) / segments;
        addSmoothSeg(v, branchPts[i], branchPts[i + 1],
                     wid * (1.f - 0.3f * t0), wid * (1.f - 0.3f * t1), e.trunk, 4.f);
    }
    if (depth <= 1 && e.leafCount > 0) {
        int node = leafNode++;
        int firstLeaf = node * e.leafCount / leafNodeCount;
        int afterLeaf = (node + 1) * e.leafCount / leafNodeCount;
        int n = afterLeaf - firstLeaf;
        float leafLen = (8.f + 5.f * e.leafAmt) * (0.6f + 0.4f * e.tree);
        for (int i = 0; i < n; ++i) {
            float along = 0.38f + 0.56f * (float(i) + 0.5f) / n;
            Vec2 attachment = bezier(a, c1, c2, b, along);
            float side = (i % 2 == 0) ? -1.f : 1.f;
            float leafAngle = ang + side * (0.78f + 0.18f * rnd(id * 17 + i));
            float scale = 0.82f + 0.28f * (0.5f + 0.5f * rnd(id * 13 + i));
            uint32_t c = mix(e.leaf, rgb(255, 255, 255), 0.08f * (0.5f + 0.5f * rnd(id * 19 + i)));
            addLeaf(v, attachment, leafAngle, leafLen * scale, leafLen * 0.42f, c, 4.2f);
        }
    }
    if (depth == 0) return;
    float spread = 0.45f + 0.15f * rnd(id);
    branch(v, e, b, ang - spread, len * 0.75f, wid * 0.7f, depth - 1, id * 2,
           leafNodeCount, leafNode);
    branch(v, e, b, ang + spread, len * 0.75f, wid * 0.7f, depth - 1, id * 2 + 1,
           leafNodeCount, leafNode);
}
void addTree(Scene& v, const Env& e) {
    const Vec2 root{126.f, 300.f};
    uint32_t soil = mix(e.lawn, e.trunk, 0.3f);
    addPoly(v, {{root.x - 17.f, 303.f}, {root.x - 12.f, 299.f}, {root.x - 5.f, 301.f},
                {root.x, 298.f}, {root.x + 6.f, 301.f}, {root.x + 13.f, 299.f},
                {root.x + 18.f, 303.f}, {root.x + 10.f, 306.f}, {root.x - 11.f, 306.f}},
           soil, 3.8f);
    float rootWidth = 4.f + 5.f * e.tree;
    addSmoothSeg(v, root, {root.x - 16.f, 307.f}, rootWidth, 1.5f, e.trunk, 4.f);
    addSmoothSeg(v, root, {root.x + 16.f, 306.f}, rootWidth, 1.5f, e.trunk, 4.f);
    addSmoothSeg(v, {root.x, root.y + 1.f}, {root.x + 3.f, 311.f},
                 rootWidth * 0.75f, 1.5f, e.trunk, 4.f);
    int depth = 2 + int(3.f * e.tree + 0.5f);
    int leafNodeCount = 3 * (1 << (depth - 1));
    int leafNode = 0;
    branch(v, e, root, 0.f, 25.f + 55.f * e.tree, 4.f + 11.f * e.tree, depth, 1,
           leafNodeCount, leafNode);
}

// ---------- lamp (right side) ----------
void addLampLight(Scene& v, float poleX, float intensity) {
    auto alphaFor = [intensity](float alpha) {
        return uint8_t(std::min(255.f, alpha * intensity + 0.5f));
    };
    const float sourceX = poleX - 15.f, sourceY = 184.f;
    const int rows = 112;
    for (int i = 0; i < rows; ++i) {
        float t0 = float(i) / rows, t1 = float(i + 1) / rows;
        float center0 = sourceX - 8.f * t0, center1 = sourceX - 8.f * t1;
        float half0 = 3.f + 49.f * t0, half1 = 3.f + 49.f * t1;
        float mid = (t0 + t1) * 0.5f;
        uint8_t alpha = alphaFor(2.f + 7.f * (1.f - mid));
        uint32_t beam = withAlpha(rgb(255, 219, 145), alpha);
        addPoly(v, {{center0 - half0, sourceY + i}, {center1 - half1, sourceY + i + 1.f},
                    {center1 + half1, sourceY + i + 1.f}, {center0 + half0, sourceY + i}},
               beam, 3.2f);
    }

    uint32_t glow = rgb(255, 215, 135);
    for (int i = 0; i < 24; ++i) {
        float t = float(i) / 23.f;
        addEllipse(v, poleX - 21.f, 298.f, 58.f - 54.f * t, 15.f - 13.5f * t,
                   withAlpha(glow, alphaFor(2.f)), 3.25f + 0.001f * i);
    }
}

void addLamp(Scene& v, const Env& e) {
    const float x = 470.f, z = 5.f;
    uint32_t metal = mix(rgb(52, 56, 64), rgb(10, 12, 20), e.dim);
    if (e.lampOn) {
        addLampLight(v, x, e.lampIntensity);
        uint8_t haloAlpha = uint8_t(std::min(255.f, 14.f * e.lampIntensity + 0.5f));
        addDisc(v, x - 15.f, 183.f, 10.f, withAlpha(rgb(255, 225, 145), haloAlpha), z - 0.1f);
    }
    addSmoothSeg(v, {x, 181.f}, {x, 295.f}, 6.f, 6.f, metal, z);
    addRect(v, x - 7, 296, x + 7, 300, metal, z);                                    
    addSmoothSeg(v, {x - 0.5f, 180.f}, {x - 15.f, 172.f}, 3.f, 3.f, metal, z);
    addPoly(v, { {x - 26.f, 171.f}, {x - 4.f, 171.f}, {x - 8.f, 181.f}, {x - 22.f, 181.f} }, metal, z);
    addDisc(v, x - 15.f, 183.f, 6.f, e.lampOn ? rgb(255, 240, 180) : rgb(205, 205, 195), z + 0.1f);
 
}

// ---------- bench (centered) ----------
void addBench(Scene& v, const Env& e) {
    float z = 6.f;
    const float  dz = 0.05f;
    uint32_t wood = mix(rgb(47, 154, 119), rgb(10, 20, 30), e.dim);
    uint32_t dark = mix(rgb(33, 108, 84),  rgb(10, 20, 30), e.dim);
    uint32_t light = mix(wood, rgb(255, 255, 255), 0.18f);
    for (const float x : {248.f, 388.f}) {
        addPoly(v, { {x - 4.f, 266.f}, {x + 4.f, 266.f}, {x + 3.f, 298.f}, {x - 3.f, 298.f} }, dark, z); z += dz;
        addRect(v, x - 7.f, 297.f, x + 7.f, 300.f, dark, z); 
        addRect(v, x - 3.f, 224.f, x + 3.f, 268.f, dark, z);  
    }
    for (const float y : {226.f, 236.f, 246.f}) {
        addRect(v, 238.f, y, 402.f, y + 7.f, wood, z); z += dz;
    }
    addPoly(v, { {232.f, 265.f}, {408.f, 265.f}, {410.f, 270.f}, {230.f, 270.f} }, light, z); z += dz; // seat front
    addRect(v, 230.f, 270.f, 410.f, 276.f, wood, z); z += dz;           // seat
    if (e.benchWear > 0.f) {
        uint32_t exposedWood = mix(rgb(143, 102, 64), rgb(25, 23, 20), e.dim);
        uint32_t wornWood = mix(rgb(105, 75, 52), rgb(27, 24, 22), e.dim);
        uint32_t exposedMetal = mix(rgb(116, 117, 110), rgb(22, 24, 28), e.dim);
        uint32_t rust = mix(rgb(138, 83, 49), rgb(34, 28, 25), e.dim);
        const float patchZ = 6.4f;
        for (const float x : {248.f, 388.f}) {
            int seed = int(x);
            addWornPatches(v, x - 2.5f, 224.f, x + 2.5f, 267.f, e.benchWear,
                           exposedMetal, rust, patchZ, seed + 1);
            addWornPatches(v, x - 2.5f, 267.f, x + 2.5f, 296.f, e.benchWear,
                           exposedMetal, rust, patchZ, seed + 2);
            addWornPatches(v, x - 6.5f, 297.f, x + 6.5f, 299.5f, e.benchWear,
                           exposedMetal, rust, patchZ, seed + 3);
        }
        for (const float y : {226.f, 236.f, 246.f})
            addWornPatches(v, 238.f, y, 402.f, y + 7.f, e.benchWear,
                           exposedWood, wornWood, patchZ, int(y));
        addWornPatches(v, 234.f, 266.f, 406.f, 269.f, e.benchWear,
                       exposedWood, wornWood, patchZ, 271);
        addWornPatches(v, 230.f, 270.f, 410.f, 276.f, e.benchWear,
                       exposedWood, wornWood, patchZ, 277);
    }
    if (e.plaque) addRect(v, 360.f, 237.f, 380.f, 243.f, rgb(212, 184, 92), 6.45f);
    if (e.lampOn) {
        uint8_t alpha = uint8_t(8.f * e.lampIntensity + 0.5f);
        uint32_t reflectedLight = withAlpha(rgb(255, 220, 150), alpha);
        for (const float y : {226.f, 236.f, 246.f})
            addRect(v, 370.f, y, 402.f, y + 7.f, reflectedLight, 6.5f);
        addPoly(v, {{380.f, 265.f}, {408.f, 265.f}, {410.f, 270.f}, {378.f, 270.f}},
                reflectedLight, 6.5f);
        addRect(v, 365.f, 270.f, 410.f, 276.f, reflectedLight, 6.5f);
    }
}

// ---------- figures & props for Scenes 1-4 (scene indices 0..3) ----------
constexpr float SEAT_Y = 266.f;

uint32_t tintSceneColor(uint32_t c, int scene) {
    if (scene == 6) return mix(c, rgb(235, 219, 245), 0.08f);
    if (scene == 7) return mix(c, rgb(102, 122, 199), 0.55f);
    return c;
}

float sstep(float u) { return anim::easeInOut(u); }
float lerpF(float a, float b, float u) { return anim::lerp(a, b, u); }
float clampF(float v, float lo, float hi) { return std::min(hi, std::max(lo, v)); }
bool blinkAt(float t) { return std::fmod(t * 0.83f, 3.3f) < 0.13f; }

fig::Look makeBoyLook(int scene, uint32_t top, uint32_t bottom = rgb(0x2e, 0x3a, 0x4d),
                      fig::Hair hair = fig::Hair::Short, float hs = 1.f) {
    fig::Look l = fig::makeMan();
    l.skin = tintSceneColor(l.skin, scene);
    l.hairC = tintSceneColor(l.hairC, scene);
    l.shoe = tintSceneColor(l.shoe, scene);
    l.top = tintSceneColor(top, scene);
    l.bottom = tintSceneColor(bottom, scene);
    l.hair = hair;
    l.hs = hs;
    return l;
}

fig::Look makeGirlLook(int scene, uint32_t top, fig::Hair hair = fig::Hair::Puff,
                       bool scarf = false, float hs = 1.f) {
    fig::Look l = fig::makeWoman();
    l.skin = tintSceneColor(l.skin, scene);
    l.hairC = tintSceneColor(l.hairC, scene);
    l.shoe = tintSceneColor(l.shoe, scene);
    l.top = tintSceneColor(top, scene);
    l.bottom = l.skin;
    l.hair = hair;
    l.hasScarf = scarf;
    l.scarf = tintSceneColor(rgb(0xc9, 0x3b, 0x3b), scene);
    l.hs = hs;
    return l;
}

void addShadow(Scene& v, float cx, float cy, float rx, float ry, float z) {
    addEllipse(v, cx, cy, rx, ry, withAlpha(rgb(0, 0, 0), 54), z);
}

// Scene 1 (index 0): Tag game around the bench; two kids hop onto the bench
void addScene1Figures(Scene& v, float t) {
    struct KidSpec { float cx, A, w, p, gy; uint32_t top; bool female, back; };
    static const KidSpec kids[5] = {
        {170.f, 120.f, 1.7f, 0.f, 324.f, rgb(0xe8, 0x6a, 0x5a), false, false},
        {470.f, 130.f, 1.3f, 2.f, 332.f, rgb(0x5a, 0xa0, 0xd8), true,  false},
        {250.f, 150.f, 1.1f, 4.f, 288.f, rgb(0x9b, 0x6f, 0xd0), false, true},
        {420.f, 110.f, 1.9f, 1.f, 291.f, rgb(0x4d, 0xb3, 0x8a), true,  true},
        {560.f,  60.f, 2.1f, 3.f, 289.f, rgb(0xf0, 0x8a, 0x3c), false, true}
    };

    auto runKid = [&](const KidSpec& k, int idx, float zBase) {
        float ex = sstep((t - 4.2f) / 1.6f);
        float arg = k.w * t + k.p;
        float x = lerpF(k.cx + k.A * std::sin(arg), -60.f - 70.f * float(idx), ex);
        float faceRaw = lerpF(clampF(std::cos(arg) * 3.f, -1.f, 1.f), -1.f, ex);
        int facing = faceRaw >= 0.f ? 1 : -1;
        float s = k.back ? 0.50f : 0.56f;

        addShadow(v, x, k.gy + 1.f, 13.f * s, 2.6f * s, zBase - 0.05f);
        fig::WalkPose wp;
        wp.look = k.female
            ? makeGirlLook(0, k.top, fig::Hair::Twin, false, 1.35f)
            : makeBoyLook(0, k.top, rgb(0x3b, 0x4a, 0x6b), fig::Hair::Short, 1.35f);
        wp.x = x;
        wp.gy = k.gy;
        wp.s = s;
        wp.facing = facing;
        wp.moving = true;
        wp.phase = t * 13.f + k.p;
        wp.amp = 0.85f;
        wp.z = zBase;
        fig::buildWalking(wp, v);
    };

    auto hopKid = [&](float x0, float xj, float xs, int face, bool female,
                      uint32_t top, float s, float ph, float zBase) {
        const float gy = 314.f, standHip = gy - 41.f * s, tRun = 0.9f, tJ = 0.6f;
        fig::Look look = female
            ? makeGirlLook(0, top, fig::Hair::Twin, false, 1.35f)
            : makeBoyLook(0, top, rgb(0x3b, 0x4a, 0x6b), fig::Hair::Short, 1.35f);

        if (t < tRun) {
            float x = lerpF(x0, xj, t / tRun);
            addShadow(v, x, gy + 1.f, 13.f * s, 2.6f * s, zBase - 0.05f);
            fig::WalkPose wp;
            wp.look = look; wp.x = x; wp.gy = gy; wp.s = s;
            wp.facing = face; wp.moving = true; wp.phase = t * 14.f + ph; wp.amp = 0.85f;
            wp.z = zBase;
            fig::buildWalking(wp, v);
            return;
        }
        if (t < tRun + tJ) {
            float u = (t - tRun) / tJ;
            fig::SeatedPose sp;
            sp.look = look;
            sp.x = lerpF(xj, xs, u);
            sp.y = lerpF(standHip, SEAT_Y, u) - 20.f * std::sin(3.14159265f * u);
            sp.s = s;
            sp.knee[0] = 0.7f; sp.knee[1] = -0.6f;
            sp.smile = true; sp.open = true;
            sp.z = zBase;
            fig::buildSeated(sp, v);
            return;
        }
        float w = t * 6.f + ph;
        fig::SeatedPose sp;
        sp.look = look;
        sp.x = xs; sp.y = SEAT_Y; sp.s = s;
        sp.knee[0] = 0.55f * std::sin(w);
        sp.knee[1] = 0.55f * std::sin(w + 3.14159265f);
        sp.bounce = 1.3f * std::abs(std::sin(t * 7.f + ph));
        sp.smile = true;
        sp.open = std::sin(t * 7.f + ph) > 0.f;
        sp.blink = blinkAt(t + ph);
        sp.turn = t > 4.4f ? -0.8f : 0.f;
        sp.z = zBase;
        fig::buildSeated(sp, v);
    };

    // Background runners behind the bench (z = 3.5..3.9)
    for (int i = 0; i < 5; ++i)
        if (kids[i].back) runKid(kids[i], i, 3.5f + 0.1f * float(i));

    // Two kids hopping onto the bench (z = 7.0..7.6)
    hopKid(-30.f, 250.f, 292.f,  1, false, rgb(0xe9, 0xb4, 0x4c), 0.56f, 0.f, 7.0f);
    hopKid(670.f, 392.f, 348.f, -1, true,  rgb(0xe0, 0x60, 0x7e), 0.52f, 2.f, 7.6f);

    // Foreground runners in front of the bench (z = 8.4..8.8)
    for (int i = 0; i < 5; ++i)
        if (!kids[i].back) runKid(kids[i], i, 8.4f + 0.2f * float(i));
}

// Scene 2 (index 1): Age 13 - sharing one phone & earphones + background walkers
void addScene2Figures(Scene& v, float t) {
    // Background walkers behind the bench
    {
        float x1 = -40.f + t * 100.f;
        addShadow(v, x1, 290.f, 13.f * 0.8f, 2.6f * 0.8f, 3.45f);
        fig::WalkPose w1;
        w1.look = makeBoyLook(1, rgb(0xc9, 0x7b, 0x3a), rgb(0x37, 0x42, 0x5a));
        w1.x = x1; w1.gy = 289.f; w1.s = 0.8f; w1.facing = 1;
        w1.moving = true; w1.phase = t * 7.f; w1.z = 3.5f;
        fig::buildWalking(w1, v);

        float x2 = 690.f - t * 80.f;
        addShadow(v, x2, 288.f, 13.f * 0.72f, 2.6f * 0.72f, 3.65f);
        fig::WalkPose w2;
        w2.look = makeGirlLook(1, rgb(0x5a, 0x9a, 0x76), fig::Hair::Bun);
        w2.x = x2; w2.gy = 287.f; w2.s = 0.72f; w2.facing = -1;
        w2.moving = true; w2.phase = t * 6.5f; w2.z = 3.7f;
        fig::buildWalking(w2, v);
    }

    float bump = std::exp(-std::pow(t - 3.2f, 2.f) / 0.05f);
    bool lg = (t > 3.3f && t < 5.2f);
    float share = sstep((t - 0.8f) / 0.8f);
    float laugh = lg ? std::abs(std::sin(t * 9.f)) * 1.3f : 0.f;

    fig::SeatedPose mp;
    mp.look = makeBoyLook(1, rgb(0x4a, 0x7f, 0xb5), rgb(0x2e, 0x3a, 0x4d), fig::Hair::Short, 1.1f);
    mp.x = 300.f; mp.y = SEAT_Y; mp.s = 0.8f;
    mp.lean = 0.02f - bump * 0.05f + (lg ? std::sin(t * 9.f) * 0.01f : 0.f);
    mp.tilt = 0.08f * share;
    mp.knee[0] = 0.1f * std::sin(t * 2.f); mp.knee[1] = 0.f;
    mp.hasHandR = true; mp.handR = {318.f, 262.f};
    mp.blink = blinkAt(t); mp.smile = lg; mp.open = lg;
    mp.bounce = laugh; mp.turn = 0.5f * share; mp.z = 7.0f;
    fig::SeatedOut m = fig::buildSeated(mp, v);

    fig::SeatedPose wp;
    wp.look = makeGirlLook(1, rgb(0x8c, 0x6f, 0xd0), fig::Hair::Pony, false, 1.1f);
    wp.x = 340.f; wp.y = SEAT_Y; wp.s = 0.78f;
    wp.lean = -0.02f - bump * 0.14f;
    wp.tilt = -0.08f * share;
    wp.knee[0] = 0.f; wp.knee[1] = 0.1f * std::sin(t * 2.f + 1.f);
    wp.hasHandL = true; wp.handL = {324.f, 262.f};
    wp.blink = blinkAt(t + 1.f); wp.smile = lg; wp.open = lg;
    wp.bounce = laugh * 0.9f; wp.turn = -0.5f * share; wp.z = 7.6f;
    fig::SeatedOut w = fig::buildSeated(wp, v);

    // Phone & glowing screen
    addRect(v, 316.f, 255.f, 326.f, 269.f, tintSceneColor(rgb(0x20, 0x23, 0x2b), 1), 8.2f);
    addRect(v, 317.f, 256.f, 325.f, 266.f, tintSceneColor(rgb(0x9a, 0xd0, 0xf0), 1), 8.25f);

    // Shared earphones & cable
    if (share > 0.f) {
        Vec2 a = m.earR, b = w.earL;
        float midY = std::max(a.y, b.y) + 22.f;
        auto cable = sampleBezier(a, {a.x + 2.f, midY}, {b.x - 2.f, midY}, b, 18);
        for (size_t i = 0; i + 1 < cable.size(); ++i)
            addSeg(v, cable[i], cable[i + 1], 1.1f, 1.1f, rgb(0xf4, 0xf4, 0xf4), 8.3f);
        addDisc(v, a.x, a.y, 1.2f, rgb(0xff, 0xff, 0xff), 8.35f);
        addDisc(v, b.x, b.y, 1.2f, rgb(0xff, 0xff, 0xff), 8.35f);
    }
}

// Scene 3 (index 2): Age 17 - holding hands, head on shoulder, red scarf appears
void addScene3Figures(Scene& v, float t) {
    float hold = sstep((t - 1.2f) / 0.6f);
    float arm = sstep((t - 2.6f) / 0.8f);
    float settle = sstep((t - 1.6f) / 1.2f);

    fig::SeatedPose mp;
    mp.look = makeBoyLook(2, rgb(0x3f, 0x4c, 0x5e), rgb(0x2e, 0x3a, 0x4d));
    mp.x = 306.f; mp.y = SEAT_Y; mp.s = 0.97f;
    mp.lean = 0.02f + 0.05f * settle;
    mp.tilt = 0.12f * settle;
    mp.blink = blinkAt(t);
    mp.hasHandL = true; mp.handL = {292.f, 268.f};
    mp.hasHandR = true;
    mp.handR = arm > 0.f ? Vec2{lerpF(323.f, 348.f, arm), lerpF(269.f, 239.f, arm)}
             : hold > 0.f ? Vec2{323.f, 269.f}
                          : Vec2{313.f, 268.f};
    mp.smile = settle > 0.5f;
    mp.bounce = 0.4f * std::sin(t * 2.f);
    mp.z = 7.0f;
    fig::buildSeated(mp, v);

    fig::SeatedPose wp;
    wp.look = makeGirlLook(2, rgb(0xe0, 0x7a, 0x5f), fig::Hair::Pony, true);
    wp.x = 336.f; wp.y = SEAT_Y; wp.s = 0.95f;
    wp.lean = -0.20f * settle;
    wp.tilt = -0.18f * settle;
    wp.blink = blinkAt(t + 1.2f);
    wp.hasHandL = true; wp.handL = hold > 0.f ? Vec2{323.f, 269.f} : Vec2{329.f, 268.f};
    wp.hasHandR = true; wp.handR = {345.f, 268.f};
    wp.smile = settle > 0.5f;
    wp.bounce = 0.4f * std::sin(t * 2.f + 1.f);
    wp.z = 7.6f;
    fig::buildSeated(wp, v);
}

// Scene 4 (index 3): Age 27 - work bag, two steaming coffees, ring glint
void addScene4Figures(Scene& v, float t) {
    float clink = std::exp(-std::pow(t - 2.4f, 2.f) / 0.03f) * 3.f;

    // Briefcase / work bag on the left side of the bench
    uint32_t bag = tintSceneColor(rgb(0x7b, 0x4b, 0x2a), 3);
    uint32_t handle = tintSceneColor(rgb(0x4a, 0x2c, 0x16), 3);
    addRect(v, 251.f, 252.f, 277.f, 267.f, bag, 6.8f);
    addSeg(v, {258.f, 252.f}, {258.f, 247.f}, 1.4f, 1.4f, handle, 6.85f);
    addSeg(v, {258.f, 247.f}, {270.f, 247.f}, 1.4f, 1.4f, handle, 6.85f);
    addSeg(v, {270.f, 247.f}, {270.f, 252.f}, 1.4f, 1.4f, handle, 6.85f);

    Vec2 mh{318.f + clink, 257.f}, wh{324.f - clink, 257.f};

    fig::SeatedPose mp;
    mp.look = makeBoyLook(3, rgb(0xee, 0xf0, 0xf5), rgb(0x2b, 0x34, 0x47));
    mp.x = 303.f; mp.y = SEAT_Y; mp.s = 1.04f;
    mp.lean = 0.04f; mp.tilt = 0.06f;
    mp.hasHandR = true; mp.handR = mh;
    mp.hasHandL = true; mp.handL = {291.f, 268.f};
    mp.blink = blinkAt(t); mp.smile = true;
    mp.bounce = 0.4f * std::sin(t * 2.f);
    mp.z = 7.0f;
    fig::buildSeated(mp, v);

    fig::SeatedPose wp;
    wp.look = makeGirlLook(3, rgb(0x2f, 0x8f, 0x9d), fig::Hair::Puff, true);
    wp.x = 340.f; wp.y = SEAT_Y; wp.s = 0.98f;
    wp.lean = -0.05f; wp.tilt = -0.09f;
    wp.hasHandL = true; wp.handL = wh;
    wp.hasHandR = true; wp.handR = {347.f, 268.f};
    wp.blink = blinkAt(t + 1.5f); wp.smile = true;
    wp.bounce = 0.4f * std::sin(t * 2.f + 1.f);
    wp.z = 7.6f;
    fig::SeatedOut w = fig::buildSeated(wp, v);

    auto addCup = [&](float cx, float cy, float seed, float zBase) {
        addPoly(v, {{cx - 3.5f, cy - 8.f}, {cx + 3.5f, cy - 8.f},
                    {cx + 2.7f, cy + 1.f}, {cx - 2.7f, cy + 1.f}},
                tintSceneColor(rgb(0xf4, 0xf1, 0xea), 3), zBase);
        addPoly(v, {{cx - 3.5f, cy - 8.f}, {cx + 3.5f, cy - 8.f},
                    {cx + 3.3f, cy - 6.f}, {cx - 3.3f, cy - 6.f}},
                tintSceneColor(rgb(0x7a, 0x4a, 0x2a), 3), zBase + 0.05f);
        for (int k = 0; k < 3; ++k) {
            float u = std::fmod(t * 0.6f + float(k) / 3.f + seed, 1.f);
            float sx = cx + std::sin(u * 8.f + float(k) * 2.f + seed) * 2.f;
            float sy = cy - 10.f - u * 12.f;
            uint8_t a = uint8_t(180.f * (1.f - u));
            addDisc(v, sx, sy, 1.1f, withAlpha(rgb(255, 255, 255), a), zBase + 0.1f);
        }
    };
    addCup(mh.x + 1.f, mh.y + 2.f, 0.f,  8.2f);
    addCup(wh.x - 1.f, wh.y + 2.f, 1.3f, 8.4f);

    // Ring sparkle on her right hand
    float ph = std::fmod(t * 0.7f, 2.4f);
    if (ph < 0.4f) {
        float r = 2.f + 4.f * std::sin((ph / 0.4f) * 3.14159265f);
        Vec2 h = w.handR;
        float cy = h.y - 1.f;
        uint32_t white = rgb(255, 255, 255);
        addSeg(v, {h.x - r, cy}, {h.x + r, cy}, 0.9f, 0.9f, white, 8.6f);
        addSeg(v, {h.x, cy - r}, {h.x, cy + r}, 0.9f, 0.9f, white, 8.6f);
        addSeg(v, {h.x - r * 0.6f, cy - r * 0.6f}, {h.x + r * 0.6f, cy + r * 0.6f}, 0.8f, 0.8f, white, 8.6f);
        addSeg(v, {h.x - r * 0.6f, cy + r * 0.6f}, {h.x + r * 0.6f, cy - r * 0.6f}, 0.8f, 0.8f, white, 8.6f);
    }
}

void addCharacters(Scene& v, int scene, float t) {
    switch (scene) {
        case 0: addScene1Figures(v, t); break;
        case 1: addScene2Figures(v, t); break;
        case 2: addScene3Figures(v, t); break;
        case 3: addScene4Figures(v, t); break;
        default: break;
    }
}
} // namespace

std::vector<Shape> buildScene(int scene, float t) {
    const Env& e = envFor(scene);
    Scene v;
    addSky(v, e);
    addGround(v, e, scene);
    addTree(v, e);
    addLamp(v, e);
    addBench(v, e);
    addCharacters(v, scene, t);
    return v;
}
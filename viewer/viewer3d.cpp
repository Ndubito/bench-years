#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "../src/scene_params.h"
#include "../src/shape.h"
#include "../src/figure.h"

std::vector<Shape> buildScene(int scene, float t);

namespace {

struct Vec3 {
    float x, y, z;
};

struct Color {
    float r, g, b, a = 1.f;
};

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Color color;
    float emissive; // 0.0 = lit by sun/lamp, 1.0 = unlit/self-illuminated
};

struct SceneLook {
    Color sky, ground, hill, leaves, trunk;
    float sunAngle, treeSize, leafDensity, dryness, benchWear;
    float ambient, sunlight, lampPower;
    bool lampOn, plaque;
    int flowers, dirtPatches, grassTufts;
    Color skyBottom, sun;
};

constexpr Color rgb(int r, int g, int b) {
    return {r / 255.f, g / 255.f, b / 255.f};
}

Color mix(Color a, Color b, float t) {
    return {a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t};
}

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
Vec3 operator/(Vec3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}
Vec3 normalized(Vec3 v) {
    const float length = std::sqrt(dot(v, v));
    return length > 1e-6f ? v / length : Vec3{0.f, 1.f, 0.f};
}

float random01(int seed) {
    uint32_t h = uint32_t(seed) * 2654435761u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return float(h & 0xffffu) / 65535.f;
}

const std::array<SceneLook, 8> kScenes = [] {
    std::array<SceneLook, 8> scenes{};
    for (size_t i = 0; i < scenes.size(); ++i) {
        const SceneParams& p = kSceneParams[i];
        scenes[i] = SceneLook{
            rgb(p.skyTop.r, p.skyTop.g, p.skyTop.b),
            rgb(p.ground.r, p.ground.g, p.ground.b),
            rgb(p.hill.r, p.hill.g, p.hill.b),
            rgb(p.leaves.r, p.leaves.g, p.leaves.b),
            rgb(p.trunk.r, p.trunk.g, p.trunk.b),
            p.sunAngle, p.treeSize, p.leafDensity, p.darkness, p.benchWear,
            p.ambient, p.sunlight, p.lampPower,
            p.lampOn, p.plaque,
            p.flowers, p.dirtPatches, p.grassTufts,
            rgb(p.skyBottom.r, p.skyBottom.g, p.skyBottom.b),
            rgb(p.sun.r, p.sun.g, p.sun.b)
        };
    }
    return scenes;
}();

Vec3 sunPositionFor(int scene) {
    const SceneLook& look = kScenes[size_t(scene)];
    const float sunX = -std::cos(look.sunAngle) * 6.5f;
    const float sunY = scene == 7 ? 5.8f : 2.5f + std::sin(look.sunAngle) * 3.3f;
    return {sunX, sunY, -7.25f};
}

struct Mesh {
    std::vector<Vertex> vertices;

    void triangle(Vec3 a, Vec3 b, Vec3 c, Color color, Vec3 normal, float emissive = 0.f) {
        triangle(a, b, c, color, color, color, normal, emissive);
    }

    void triangle(Vec3 a, Vec3 b, Vec3 c, Color ca, Color cb, Color cc, Vec3 normal, float emissive = 0.f) {
        normal = normalized(normal);
        vertices.push_back({a, normal, ca, emissive});
        vertices.push_back({b, normal, cb, emissive});
        vertices.push_back({c, normal, cc, emissive});
    }

    void quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Color color, Vec3 normal, float emissive = 0.f) {
        quad(a, b, c, d, color, color, color, color, normal, emissive);
    }

    void quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Color ca, Color cb, Color cc, Color cd, Vec3 normal, float emissive = 0.f) {
        triangle(a, b, c, ca, cb, cc, normal, emissive);
        triangle(a, c, d, ca, cc, cd, normal, emissive);
    }

    void box(Vec3 center, Vec3 size, Color color, float emissive = 0.f) {
        const Vec3 h = size * .5f;
        const float x0 = center.x - h.x, x1 = center.x + h.x;
        const float y0 = center.y - h.y, y1 = center.y + h.y;
        const float z0 = center.z - h.z, z1 = center.z + h.z;
        quad({x0,y1,z0}, {x0,y1,z1}, {x1,y1,z1}, {x1,y1,z0}, color, {0,1,0}, emissive);
        quad({x0,y0,z1}, {x0,y0,z0}, {x1,y0,z0}, {x1,y0,z1}, color, {0,-1,0}, emissive);
        quad({x0,y0,z1}, {x1,y0,z1}, {x1,y1,z1}, {x0,y1,z1}, color, {0,0,1}, emissive);
        quad({x1,y0,z0}, {x0,y0,z0}, {x0,y1,z0}, {x1,y1,z0}, color, {0,0,-1}, emissive);
        quad({x0,y0,z0}, {x0,y0,z1}, {x0,y1,z1}, {x0,y1,z0}, color, {-1,0,0}, emissive);
        quad({x1,y0,z1}, {x1,y0,z0}, {x1,y1,z0}, {x1,y1,z1}, color, {1,0,0}, emissive);
    }

    void cylinder(Vec3 a, Vec3 b, float radiusA, float radiusB, Color color, int sides = 16, float emissive = 0.f) {
        const Vec3 axis = normalized(b - a);
        const Vec3 reference = std::abs(axis.y) < .9f ? Vec3{0,1,0} : Vec3{1,0,0};
        const Vec3 u = normalized(cross(axis, reference));
        const Vec3 v = cross(axis, u);
        constexpr float tau = 6.28318530718f;
        for (int i = 0; i < sides; ++i) {
            const float a0 = tau * float(i) / sides;
            const float a1 = tau * float(i + 1) / sides;
            const Vec3 n0 = u * std::cos(a0) + v * std::sin(a0);
            const Vec3 n1 = u * std::cos(a1) + v * std::sin(a1);
            const Vec3 p0 = a + n0 * radiusA, p1 = a + n1 * radiusA;
            const Vec3 q0 = b + n0 * radiusB, q1 = b + n1 * radiusB;
            triangle(p0, p1, q1, color, n0 + n1, emissive);
            triangle(p0, q1, q0, color, n0 + n1, emissive);
            triangle(a, p1, p0, color, axis * -1.f, emissive);
            triangle(b, q0, q1, color, axis, emissive);
        }
    }

    void sphere(Vec3 center, float radius, Color color, int slices = 16, int stacks = 12, float emissive = 0.f) {
        constexpr float pi = 3.14159265359f;
        constexpr float tau = 6.28318530718f;
        for (int row = 0; row < stacks; ++row) {
            const float p0 = -pi * .5f + pi * float(row) / stacks;
            const float p1 = -pi * .5f + pi * float(row + 1) / stacks;
            for (int col = 0; col < slices; ++col) {
                const float t0 = tau * float(col) / slices;
                const float t1 = tau * float(col + 1) / slices;
                const auto point = [center, radius](float p, float t) {
                    return Vec3{center.x + radius * std::cos(p) * std::cos(t),
                                center.y + radius * std::sin(p),
                                center.z + radius * std::cos(p) * std::sin(t)};
                };
                const Vec3 n00{std::cos(p0)*std::cos(t0), std::sin(p0), std::cos(p0)*std::sin(t0)};
                const Vec3 n01{std::cos(p0)*std::cos(t1), std::sin(p0), std::cos(p0)*std::sin(t1)};
                const Vec3 n10{std::cos(p1)*std::cos(t0), std::sin(p1), std::cos(p1)*std::sin(t0)};
                const Vec3 n11{std::cos(p1)*std::cos(t1), std::sin(p1), std::cos(p1)*std::sin(t1)};
                const Vec3 a = point(p0, t0), b = point(p0, t1);
                const Vec3 c = point(p1, t1), d = point(p1, t0);
                triangle(a, b, c, color, n00 + n01 + n11, emissive);
                triangle(a, c, d, color, n00 + n11 + n10, emissive);
            }
        }
    }
    void leaf3D(Vec3 base, Vec3 dir, Vec3 side, float len, float width, Color color) {
        dir = normalized(dir);
        side = normalized(side - dir * dot(side, dir));
        const Vec3 up = normalized(cross(side, dir));
        const Vec3 p0 = base;
        const Vec3 p1 = base + dir * (len * 0.28f) + side * (width * 0.28f) + up * (len * 0.04f);
        const Vec3 p2 = base + dir * (len * 0.68f) + side * (width * 0.50f) + up * (len * 0.06f);
        const Vec3 p3 = base + dir * len;
        const Vec3 p4 = base + dir * (len * 0.68f) - side * (width * 0.50f) + up * (len * 0.06f);
        const Vec3 p5 = base + dir * (len * 0.28f) - side * (width * 0.28f) + up * (len * 0.04f);
        const Vec3 n = normalized(up + Vec3{0.f, 0.35f, 0.25f});
        triangle(p0, p1, p2, color, n);
        triangle(p0, p2, p3, color, n);
        triangle(p0, p3, p4, color, n);
        triangle(p0, p4, p5, color, n);
    }
};

void addHills(Mesh& mesh, const SceneLook& look) {
    constexpr int segments = 64;
    for (int hill = 0; hill < 2; ++hill) {
        const float z = -10.0f + hill * .45f;
        const Color color = hill == 0
            ? mix(look.hill, rgb(205,220,205), .22f)
            : mix(look.hill, look.ground, .52f);
        for (int i = 0; i < segments; ++i) {
            const float x0 = -20.f + 40.f * float(i) / segments;
            const float x1 = -20.f + 40.f * float(i + 1) / segments;
            const float haze = hill == 0 ? .58f : .32f;
            const float y0 = 1.15f + (.62f + hill * .12f) *
                            std::sin((x0 + hill * 2.2f) * .48f);
            const float y1 = 1.15f + (.62f + hill * .12f) *
                            std::sin((x1 + hill * 2.2f) * .48f);
            const Color hazy = mix(color, rgb(205,218,216), haze);
            mesh.quad({x0,0,z}, {x0,y0,z}, {x1,y1,z}, {x1,0,z}, hazy, {0,0,1});
        }
    }
}

void addPath(Mesh& mesh, Color lawn) {
    const Color path = mix(lawn, rgb(205,185,145), .72f);
    constexpr int segments = 64;
    for (int i = 0; i < segments; ++i) {
        const float x0 = -11.f + 22.f * float(i) / segments;
        const float x1 = -11.f + 22.f * float(i + 1) / segments;
        const float z0 = 3.0f + .32f * std::sin(x0 * .42f);
        const float z1 = 3.0f + .32f * std::sin(x1 * .42f);
        mesh.quad({x0,.025f,z0-.75f}, {x1,.025f,z1-.75f},
                  {x1,.025f,z1+.75f}, {x0,.025f,z0+.75f}, path, {0,1,0});
        const Color edge = mix(path, rgb(92,78,52), .28f);
        mesh.quad({x0,.028f,z0-.86f}, {x1,.028f,z1-.86f},
                  {x1,.028f,z1-.75f}, {x0,.028f,z0-.75f}, edge, {0,1,0});
        mesh.quad({x0,.028f,z0+.75f}, {x1,.028f,z1+.75f},
                  {x1,.028f,z1+.86f}, {x0,.028f,z0+.86f}, edge, {0,1,0});
    }
}

void addBranch(Mesh& mesh, Vec3 start, float angle, float yawAngle, float length, float radius,
               int depth, int id, const SceneLook& look, int scene) {
    Vec3 previous = start;
    const int segments = 3;
    Vec3 end = start;
    float tipAngle = angle;

    for (int segment = 1; segment <= segments; ++segment) {
        const float bend = .045f * (random01(id * 29 + segment) - .5f);
        tipAngle = angle + bend * segment;
        const Vec3 stepDir{std::sin(tipAngle) * std::cos(yawAngle),
                           std::cos(tipAngle),
                           std::sin(tipAngle) * std::sin(yawAngle)};
        end = start + stepDir * (length * float(segment) / float(segments));
        mesh.cylinder(previous, end,
                      radius * (1.f - float(segment - 1) / segments * .32f),
                      radius * (1.f - float(segment) / segments * .32f),
                      look.trunk, 8);
        previous = end;
    }

    constexpr std::array<float, 8> kSceneCanopyBoost = {
        0.38f, // Scene 0: young sapling (light, fresh buds)
        0.68f, // Scene 1: growing young tree
        1.05f, // Scene 2: lush spring/early summer
        1.35f, // Scene 3: peak full canopy
        1.30f, // Scene 4: full mature canopy
        0.95f, // Scene 5: leaves starting to dry & thin
        0.58f, // Scene 6: sparser autumn foliage
        0.00f  // Scene 7: late dry
    };
    const float stageBoost = kSceneCanopyBoost[size_t(std::clamp(scene, 0, 7))];
    const float effectiveDensity = (scene == 7)
        ? 0.f
        : std::clamp(std::max(look.leafDensity, 0.25f) * stageBoost, 0.f, 1.45f);
    const int maxLeafDepth = (scene == 0) ? 1 : 2;

    if (depth <= maxLeafDepth && effectiveDensity > 0.0f) {
        const int clusterNodes = 4 + int(6.f * effectiveDensity);
        const int leavesPerCluster = 5 + int(6.f * effectiveDensity);
        const float leafLen = (.21f + .16f * std::min(effectiveDensity, 1.1f)) * (.68f + .35f * look.treeSize);
        const float clusterSpread = (.11f + .23f * std::min(effectiveDensity, 1.1f)) * (.60f + .40f * look.treeSize);

        const Vec3 branchDir = normalized(end - start);
        const Vec3 ref = std::abs(branchDir.y) < .9f ? Vec3{0.f, 1.f, 0.f} : Vec3{1.f, 0.f, 0.f};
        const Vec3 sideA = normalized(cross(branchDir, ref));
        const Vec3 sideB = cross(branchDir, sideA);

        for (int c = 0; c < clusterNodes; ++c) {
            const int cSeed = id * 53 + c * 29;
            const float along = .22f + .78f * (float(c) + .5f) / float(clusterNodes);
            const float nodeAngle = 6.2831853f * random01(cSeed + 1);
            const float nodeRadius = clusterSpread * (.35f + .65f * random01(cSeed + 2));
            const Vec3 clusterCenter = start + (end - start) * along
                                     + (sideA * std::cos(nodeAngle) + sideB * std::sin(nodeAngle)) * nodeRadius;

            for (int k = 0; k < leavesPerCluster; ++k) {
                const int seed = cSeed * 19 + k * 13;
                const float theta = 6.2831853f * (float(k) / float(leavesPerCluster) + .25f * random01(seed + 1));
                const float elevation = (random01(seed + 2) - .35f) * 1.35f;

                const Vec3 radialDir = normalized(
                    sideA * (std::cos(theta) * std::cos(elevation)) +
                    sideB * (std::sin(theta) * std::cos(elevation)) +
                    branchDir * std::sin(elevation)
                );
                const Vec3 attach = clusterCenter + radialDir * (clusterSpread * .35f * random01(seed + 3));
                const Vec3 outSide = normalized(cross(radialDir, branchDir + Vec3{0.f, 1.f, 0.f}));
                const Vec3 leafDir = normalized(radialDir * .82f + Vec3{0.f, -.16f, 0.f});

                Color leafColor = look.leaves;
                if ((id + c + k) % 4 == 0)
                    leafColor = mix(leafColor, rgb(245, 235, 165), .20f);
                else if ((id + k) % 5 == 0)
                    leafColor = mix(leafColor, rgb(32, 74, 34), .16f);
                if (look.dryness > 0.25f && (id + c + k) % 3 == 0)
                    leafColor = mix(leafColor, rgb(186, 128, 54), (look.dryness - 0.25f) * 0.65f);

                const float scale = .78f + .42f * random01(seed + 5);
                mesh.leaf3D(attach, leafDir, outSide, leafLen * scale, leafLen * .52f * scale, leafColor);
            }
        }
    }
    if (depth <= 0) return;
    const float spread = .46f + .14f * random01(id * 23 + depth);
    const float yawShift = .55f * (random01(id * 19 + depth) - .5f);
    addBranch(mesh, end, tipAngle - spread, yawAngle + yawShift, length * .74f, radius * .68f, depth - 1, id * 2, look, scene);
    addBranch(mesh, end, tipAngle + spread, yawAngle - yawShift, length * .74f, radius * .68f, depth - 1, id * 2 + 1, look, scene);
}

void addTree(Mesh& mesh, const SceneLook& look, int scene) {
    SceneLook treeLook = look;
    if (treeLook.leafDensity <= 0.f) treeLook.leafDensity = 0.22f;
    const Vec3 root{-5.25f, .06f, -.55f};
    const float scale = .48f + .68f * treeLook.treeSize;
    const float trunkLength = 1.25f + .95f * treeLook.treeSize;
    const int depth = 2 + int(2.f * treeLook.treeSize + .5f);
    mesh.cylinder({root.x, 0.f, root.z}, {root.x, .07f, root.z}, .52f, .34f,
                  mix(treeLook.ground, treeLook.trunk, .35f), 14);
    addBranch(mesh, root, 0.f, 0.2f, trunkLength, .15f * scale, depth, 1, treeLook, scene);
    mesh.cylinder(root, root + Vec3{-.42f, .04f, 0}, .11f, .025f, treeLook.trunk, 8);
    mesh.cylinder(root, root + Vec3{.43f, .04f, .05f}, .11f, .025f, treeLook.trunk, 8);
}

Color benchWood(const SceneLook& look) {
    return mix(rgb(47,154,119), rgb(12,18,24), look.ambient < .3f ? .58f : 0.f);
}

void addBench(Mesh& mesh, const SceneLook& look) {
    const float seatY = .82f;
    const float frontZ = .42f;
    const float dim = look.ambient < .3f ? .58f : 0.f;
    const Color freshPaint = rgb(47, 154, 119);
    const Color fadedPaint = rgb(98, 128, 114);
    const Color wood = mix(mix(freshPaint, fadedPaint, look.benchWear * .75f), rgb(12, 18, 24), dim);
    const Color frontLip = mix(wood, rgb(255, 255, 255), .14f * (1.f - look.benchWear * .5f));
    const Color metal = mix(rgb(42,48,52), rgb(10,12,17), look.ambient < .3f ? .55f : 0.f);
    for (float x : {-1.58f, 1.58f}) {
        mesh.cylinder({x,.08f, frontZ}, {x,seatY,frontZ}, .075f, .055f, metal);
        mesh.cylinder({x,.08f,-.42f}, {x,seatY,-.42f}, .075f, .055f, metal);
        mesh.cylinder({x,.72f,-.42f}, {x,1.72f,-.42f}, .055f, .045f, metal);
    }
    for (float z : {-.31f, .0f, .31f})
        mesh.box({0,seatY,z}, {4.15f,.14f,.22f}, wood);
    for (float y : {1.05f, 1.30f, 1.55f})
        mesh.box({0,y,-.43f}, {4.2f,.17f,.12f}, wood);
    mesh.box({0,.69f,-.03f}, {3.55f,.09f,.78f}, metal);

    if (look.benchWear > 0.f) {
        const Color exposedWood = mix(rgb(143, 102, 64), rgb(25, 23, 20), dim);
        const Color wornWood = mix(rgb(105, 75, 52), rgb(27, 24, 22), dim);
        const Color rust = mix(rgb(138, 83, 49), rgb(34, 28, 25), dim);
        const int patches = std::max(1, int(std::ceil(look.benchWear * 14.f)));

        for (int b = 0; b < 3; ++b) {
            const float slatY = 1.05f + .25f * float(b);
            for (int i = 0; i < patches; ++i) {
                const int seed = (b + 5) * 113 + i * 37;
                const float x = -1.88f + 3.76f * random01(seed + 1);
                const float w = (.12f + .28f * look.benchWear) * (.7f + .6f * random01(seed + 3));
                const float h = .10f + .05f * random01(seed + 5);
                const Color c = mix(exposedWood, wornWood, random01(seed + 7));
                mesh.box({x, slatY, -.367f}, {w, h, .008f}, c);
            }
        }
        // Rust spots on the metal legs
        for (float x : {-1.58f, 1.58f}) {
            for (int i = 0; i < std::max(1, patches / 2); ++i) {
                const int seed = int(x * 100.f) + i * 43;
                const float y = .15f + 1.45f * random01(seed + 1);
                mesh.box({x, y, -.42f}, {.12f, .08f, .12f}, rust);
            }
        }
    }   
    if (look.plaque) mesh.box({1.0f,1.31f,-.355f}, {.38f,.12f,.018f}, rgb(212,184,92));
}

// Lamp head overhangs forward (+Z) so light illuminates the front of the bench
constexpr Vec3 kLampBulbPos{4.33f, 3.26f, .58f};

float lampPowerAt(int scene, float sceneTime) {
    const SceneLook& look = kScenes[size_t(scene)];
    float p = (look.lampOn || look.lampPower > 0.f) ? look.lampPower : 0.f;
    if (scene == 7) {
        if (sceneTime >= 14.0f)     return 0.f;
        if (sceneTime >= 13.0f) {
            float k = 1.f - (sceneTime - 13.0f) / 1.5f;
            return p * (k * k); // smooth cubic/quadratic ease-out to 0
        }
        if (sceneTime >= 9.20f)     return p;
        if (sceneTime >= 9.05f)     return 0.f;      // quick blink off
        if (sceneTime >= 8.90f)     return p * 0.85f;
        if (sceneTime >= 8.70f)     return p * 0.10f; // quick blink dim
        if (sceneTime >= 8.55f)     return p * 0.90f;
        if (sceneTime >= 8.40f)     return p * 0.15f; // flicker starts
    }
    return p;
}

void addLamp(Mesh& mesh, const SceneLook& look, int scene, float t) {
    const float x = 4.75f;
    const Color metal = mix(rgb(48,52,59), rgb(11,14,20), look.ambient < .3f ? .55f : 0.f);
    mesh.cylinder({x,.05f,-.18f}, {x,3.55f,-.18f}, .07f, .045f, metal, 12);
    mesh.cylinder({x,.02f,-.18f}, {x,.14f,-.18f}, .26f, .26f, metal, 16);
    mesh.box({x,.13f,-.18f}, {.82f,.09f,.62f}, metal);
    mesh.cylinder({x,3.55f,-.18f}, {x,3.68f,-.18f}, .055f, .055f, metal, 10);
    mesh.cylinder({x,3.68f,-.18f}, {x-.30f,3.68f,.08f}, .055f, .045f, metal, 10);
    mesh.cylinder({x-.30f,3.68f,.08f}, {x-.42f,3.50f,.38f}, .045f, .035f, metal, 10);
    mesh.cylinder({x-.42f,3.50f,.38f}, {x-.42f,3.38f,.58f}, .035f, .025f, metal, 10);
    mesh.sphere({x-.42f,3.43f,.58f}, .23f, metal, 14, 8);
    mesh.box({x-.42f,3.33f,.58f}, {.48f,.10f,.36f}, metal);
    const float currentPower = lampPowerAt(scene, t);
    const float maxPower = std::max(look.lampPower, 0.001f);
    const float glow = std::clamp(currentPower / maxPower, 0.f, 1.f);
    const Color bulbOff = rgb(236, 228, 206);
    const Color bulbOn = rgb(255, 242, 185);
    mesh.sphere(kLampBulbPos, .12f, mix(bulbOff, bulbOn, glow), 10, 6, glow);
}

void addGrass(Mesh& mesh, const SceneLook& look, int scene) {
    const Color dryGround = scene == 7 ? rgb(76,64,46) : rgb(201,164,88);
    const Color ground = mix(look.ground, dryGround, look.dryness);
    mesh.quad({-20,0,-10.0f}, {20,0,-10.0f}, {20,0,20.f}, {-20,0,20.f},
              ground, {0,1,0});
    const Color foreground = mix(ground, rgb(82,151,72), .10f);
    mesh.quad({-20,.004f,7.0f}, {20,.004f,7.0f},
              {20,.004f,16.f}, {-20,.004f,16.f}, foreground, {0,1,0});

    for (int i = 0; i < look.dirtPatches; ++i) {
        const float x = -9.f + 18.f * random01(1103 + i * 17);
        const float z = i % 2 == 0
            ? -6.4f + 8.3f * random01(1123 + i * 23)
            : 4.2f + 9.5f * random01(1123 + i * 23);
        const float rx = .2f + .36f * random01(1151 + i * 29);
        const float rz = .08f + .13f * random01(1171 + i * 31);
        const Color soil = (i % 2 == 0) ? rgb(117,84,53) : rgb(144,105,66);
        constexpr int points = 10;
        for (int p = 0; p < points; ++p) {
            const float a0 = 6.2831853f * float(p) / points;
            const float a1 = 6.2831853f * float(p + 1) / points;
            const float r0 = .78f + .32f * random01(1201 + i * 37 + p);
            const float r1 = .78f + .32f * random01(1223 + i * 41 + p + 1);
            mesh.triangle({x,.012f,z},
                          {x + std::cos(a0)*rx*r0,.012f,z + std::sin(a0)*rz*r0},
                          {x + std::cos(a1)*rx*r1,.012f,z + std::sin(a1)*rz*r1},
                          soil, {0,1,0});
        }
    }

    const Color green = mix(look.ground, rgb(72,158,56), .42f);
    const Color dry = scene == 7 ? rgb(135,107,67) : rgb(187,151,78);
    const float grassHeight = scene < 2 ? .14f : scene < 4 ? .25f : scene < 6 ? .19f : .16f;
    const auto addTuft = [&](float x, float z, float height, int seed) {
        const Color color = mix(green, dry, look.dryness * (.7f + .3f * random01(seed + 11)));
        const int blades = 3 + int(2.f * random01(seed + 19));
        for (int blade = 0; blade < blades; ++blade) {
            const float side = float(blade) - float(blades - 1) * .5f;
            const Vec3 base{x + side * .025f, .018f, z};
            const Vec3 tip{x + side * .055f + (random01(seed + blade * 23)-.5f)*.1f,
                           .018f + height * (.7f + .3f * random01(seed + blade * 29 + 1)),
                           z + (random01(seed + blade * 31 + 2)-.5f)*.12f};
            const Vec3 p0{base.x - .018f, base.y, base.z};
            const Vec3 p1{base.x + .018f, base.y, base.z};
            const Vec3 faceNormal = normalized(cross(p1 - p0, tip - p0));
            const Vec3 softNormal = normalized(faceNormal + Vec3{0.f, 1.2f, 0.f});
            mesh.triangle(p0, p1, tip, color, softNormal);
        }
    };
    for (int i = 0; i < look.grassTufts; ++i) {
        const float x = -9.5f + 19.f * random01(1301 + i * 13);
        const float z = -6.8f + 8.7f * random01(1319 + i * 19);
        const float height = grassHeight * (.75f + .5f * random01(1361 + i * 23));
        addTuft(x, z, height, 1381 + i * 41);
    }
    for (int i = 0; i < look.grassTufts * 3 / 4; ++i) {
        const float x = -11.f + 22.f * random01(1601 + i * 17);
        const float z = 4.2f + 9.5f * random01(1621 + i * 23);
        const float height = grassHeight * .42f * (.75f + .5f * random01(1641 + i * 29));
        addTuft(x, z, height, 1663 + i * 43);
    }

    for (int i = 0; i < look.flowers; ++i) {
        const float x = -8.8f + 17.6f * random01(1501 + i * 47);
        const float z = -5.8f + 7.6f * random01(1523 + i * 53);
        const float top = .38f + .12f * random01(1543 + i * 59);
        mesh.cylinder({x,.02f,z}, {x,top,z}, .012f, .008f, rgb(58,112,42), 5);
        const Color flower = (i % 3 == 0) ? rgb(255,242,207)
                            : (i % 3 == 1) ? rgb(255,220,95)
                                           : rgb(225,194,245);
        mesh.sphere({x,top,z}, .045f, flower, 7, 5);
        mesh.sphere({x,top+.018f,z}, .022f, rgb(245,174,49), 6, 4);
    }
}

void addSky(Mesh& mesh, const SceneLook& look) {
    const Color top = look.sky;
    const Color bottom = mix(look.skyBottom, rgb(255, 224, 205), .25f);
    const float z = -24.f;
    // Emissive = 1.0 keeps the sky gradient crisp and unaffected by scene darkness
    mesh.quad({-40.f, -5.f, z}, {40.f, -5.f, z},
              {40.f, 25.f, z}, {-40.f, 25.f, z},
              bottom, bottom, top, top, {0, 0, 1}, 1.0f);
}

// ---------- Stylized 3D Character Builder (matches reference model & stays whole in all scenes) ----------
namespace fig3d {

inline constexpr uint32_t rgba32(int r, int g, int b, int a = 255) {
    return uint32_t(r & 0xFF)
         | (uint32_t(g & 0xFF) << 8)
         | (uint32_t(b & 0xFF) << 16)
         | (uint32_t(a & 0xFF) << 24);
}

inline Color toCol(uint32_t rgba, float alphaOverride = -1.f) {
    float a = (alphaOverride >= 0.f) ? alphaOverride : float((rgba >> 24) & 0xFFu) / 255.f;
    return {float(rgba & 0xFFu) / 255.f,
            float((rgba >> 8) & 0xFFu) / 255.f,
            float((rgba >> 16) & 0xFFu) / 255.f,
            a};
}

inline uint32_t shade(uint32_t c, float k) {
    uint32_t out = c & 0xFF000000u;
    for (int s = 0; s < 24; s += 8) {
        int v = int(float((c >> s) & 0xFFu) * k);
        out |= uint32_t(std::clamp(v, 0, 255)) << s;
    }
    return out;
}

inline void addPuffCluster(Mesh& m, Vec3 center, float r, Color hair, int seed) {
    m.sphere(center, r * 0.88f, hair, 14, 10);
    for (int i = 0; i < 10; ++i) {
        float a = 6.2831853f * float(i) / 10.f + random01(seed + i) * 0.4f;
        float e = (random01(seed + i * 7) - 0.35f) * 1.6f;
        Vec3 off{std::cos(a) * std::cos(e), std::sin(e), std::sin(a) * std::cos(e)};
        m.sphere(center + off * (r * 0.62f), r * 0.42f, hair, 10, 8);
    }
}

inline void addHead3D(Mesh& m, Vec3 headCenter, float hr, float yaw, float pitch,
                      const fig::Look& L, bool smile, bool openMouth, bool blink, float alpha = 1.f) {
    const Color skin = toCol(L.skin, alpha);
    const Color skinShadow = toCol(shade(L.skin, 0.88f), alpha);
    const Color hair = toCol(L.hairC, alpha);
    const Color white = {0.97f, 0.97f, 0.98f, alpha};
    const Color iris = {0.16f, 0.10f, 0.06f, alpha};
    const Color pupil = {0.03f, 0.03f, 0.04f, alpha};
    const Color bandY = {245.f / 255.f, 210.f / 255.f, 66.f / 255.f, alpha};

    m.sphere(headCenter, hr, skin, 20, 16);

    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const Vec3 fwd{sy, -std::sin(pitch) * 0.35f, cy};
    const Vec3 right{cy, 0.f, -sy};
    const Vec3 up{0.f, 1.f, 0.f};

    for (float sd : {-1.f, 1.f}) {
        m.sphere(headCenter + right * (sd * hr * 0.42f) - up * (hr * 0.18f) + fwd * (hr * 0.62f),
                 hr * 0.42f, skin, 12, 10);
    }

    if (L.hair != fig::Hair::Bald) {
        m.sphere(headCenter + up * (hr * 0.22f) - fwd * (hr * 0.14f), hr * 1.03f, hair, 18, 12);
    }
    if (L.hair == fig::Hair::Short) {
        for (int i = 0; i < 16; ++i) {
            float a = 6.2831853f * float(i) / 16.f;
            float ring = (i % 2 == 0) ? 0.58f : 0.86f;
            Vec3 p = headCenter + up * (hr * (1.02f - ring * 0.36f))
                   + right * (std::cos(a) * hr * ring * 0.92f)
                   + fwd * (std::sin(a) * hr * ring * 0.78f - hr * 0.10f);
            m.sphere(p, hr * 0.30f, hair, 10, 8);
        }
    } else if (L.hair == fig::Hair::Twin) {
        for (float sd : {-1.f, 1.f}) {
            Vec3 base = headCenter + up * (hr * 0.68f) + right * (sd * hr * 0.86f) - fwd * (hr * 0.10f);
            Vec3 puff = headCenter + up * (hr * 0.92f) + right * (sd * hr * 1.15f) - fwd * (hr * 0.14f);
            m.cylinder(base, puff, hr * 0.25f, hr * 0.28f, bandY, 12);
            addPuffCluster(m, puff, hr * 0.56f, hair, sd < 0 ? 11 : 37);
        }
    } else if (L.hair == fig::Hair::Bun) {
        Vec3 base = headCenter + up * (hr * 0.85f) - fwd * (hr * 0.32f);
        Vec3 bun = headCenter + up * (hr * 1.12f) - fwd * (hr * 0.44f);
        m.cylinder(base, bun, hr * 0.24f, hr * 0.26f, bandY, 12);
        addPuffCluster(m, bun, hr * 0.52f, hair, 53);
    } else if (L.hair == fig::Hair::Braids) {
        for (float sd : {-1.f, 1.f}) {
            Vec3 top = headCenter + right * (sd * hr * 0.88f) + up * (hr * 0.15f);
            Vec3 bot = headCenter + right * (sd * hr * 0.98f) - up * (hr * 1.25f);
            m.cylinder(top, bot, hr * 0.22f, hr * 0.18f, hair, 10);
            m.sphere(bot, hr * 0.19f, hair, 10, 8);
        }
    }
    if (L.hasScarf) {
        m.sphere(headCenter + up * (hr * 0.14f) - fwd * (hr * 0.08f), hr * 1.08f, toCol(L.scarf, alpha), 16, 12);
    }

    for (float sd : {-1.f, 1.f}) {
        m.sphere(headCenter + right * (sd * hr * 0.97f) - up * (hr * 0.04f), hr * 0.21f, skin, 10, 8);
    }

    for (float sd : {-1.f, 1.f}) {
        Vec3 eyePos = headCenter + fwd * (hr * 0.88f) + right * (sd * hr * 0.35f) + up * (hr * 0.08f);
        Vec3 browA = eyePos + up * (hr * 0.24f) - right * (sd * hr * 0.13f);
        Vec3 browB = eyePos + up * (hr * 0.21f) + right * (sd * hr * 0.14f);
        m.cylinder(browA, browB, hr * 0.036f, hr * 0.028f, hair, 8);

        if (blink) {
            m.cylinder(eyePos - right * (hr * 0.13f), eyePos + right * (hr * 0.13f), hr * 0.03f, hr * 0.03f, pupil, 8);
        } else {
            m.sphere(eyePos, hr * 0.155f, white, 12, 10);
            m.sphere(eyePos + fwd * (hr * 0.075f), hr * 0.105f, iris, 10, 8);
            m.sphere(eyePos + fwd * (hr * 0.125f), hr * 0.062f, pupil, 10, 8);
            m.sphere(eyePos + fwd * (hr * 0.155f) + up * (hr * 0.04f) - right * (hr * 0.03f), hr * 0.026f, white, 8, 6);
        }
    }
    m.sphere(headCenter + fwd * (hr * 0.99f) - up * (hr * 0.08f), hr * 0.115f, skinShadow, 10, 8);

    Vec3 mouthC = headCenter + fwd * (hr * 0.89f) - up * (hr * 0.34f);
    if (openMouth) {
        m.sphere(mouthC, hr * 0.13f, {0.38f, 0.09f, 0.09f, alpha}, 10, 8);
    } else {
        m.cylinder(mouthC - right * (hr * 0.14f) + up * (smile ? hr * 0.03f : 0.f),
                   mouthC + right * (hr * 0.14f) + up * (smile ? hr * 0.03f : 0.f),
                   hr * 0.024f, hr * 0.024f, {0.28f, 0.11f, 0.10f, alpha}, 8);
    }
}

inline void addWalking3D(Mesh& solid, Mesh& trans, Vec3 footPos, float yaw, float scale,
                         const fig::WalkPose& P, bool hasBriefcase = false, uint32_t caseC = 0) {
    const fig::Look& L = P.look;
    const bool isChild = (scale < 0.72f);
    const float legLen = (isChild ? 0.35f : 0.45f) * (scale / 0.55f);
    const float thighLen = legLen * 0.52f;
    const float shinLen = legLen * 0.48f;
    const float torsoH = (isChild ? 0.34f : 0.46f) * (scale / 0.55f);
    const float shoulderW = (isChild ? 0.14f : 0.18f) * (scale / 0.55f);
    const float hipW = shoulderW * 0.74f;
    const float hr = (isChild ? 0.155f : 0.150f) * L.hs * (scale / 0.55f);

    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const Vec3 fwd{sy, 0.f, cy};
    const Vec3 right{cy, 0.f, -sy};
    const Vec3 up{0.f, 1.f, 0.f};

    trans.cylinder({footPos.x, 0.029f, footPos.z}, {footPos.x, 0.033f, footPos.z},
                   shoulderW * 1.15f, shoulderW * 1.15f, {0.f, 0.f, 0.f, 0.22f}, 16);

    const float bob = P.moving ? std::abs(std::sin(P.phase)) * 0.022f : 0.f;
    const Vec3 pelvis = footPos + up * (legLen + 0.03f - bob);

    const Color topC = toCol(L.top);
    const Color botC = toCol(L.bottom);
    const Color skinC = toCol(L.skin);
    const Color shoeC = toCol(L.shoe);
    const Color sockC = {0.95f, 0.95f, 0.97f, 1.f};
    const Color sleeveC = (L.jacket != 0) ? toCol(L.jacket) : (L.female ? skinC : topC);

    solid.sphere(pelvis, hipW * 1.16f, L.female ? topC : botC, 14, 10);
    const Vec3 spineDir = normalized(up * std::cos(P.lean) + fwd * std::sin(P.lean));
    const Vec3 chestTop = pelvis + spineDir * torsoH;

    if (L.female) {
        Vec3 bodiceTop = pelvis + spineDir * (torsoH * 0.84f);
        solid.cylinder(pelvis, bodiceTop, hipW * 1.04f, shoulderW * 0.84f, topC, 16);
        solid.cylinder(bodiceTop, chestTop, shoulderW * 0.82f, shoulderW * 0.86f, skinC, 14);
        for (float sd : {-1.f, 1.f}) {
            Vec3 strapA = bodiceTop + right * (sd * shoulderW * 0.56f) + fwd * (hipW * 0.42f);
            Vec3 strapB = chestTop + right * (sd * shoulderW * 0.56f);
            Vec3 strapC = bodiceTop + right * (sd * shoulderW * 0.56f) - fwd * (hipW * 0.42f);
            solid.cylinder(strapA, strapB, 0.015f, 0.015f, topC, 8);
            solid.cylinder(strapB, strapC, 0.015f, 0.015f, topC, 8);
        }
        Vec3 skirtTop = pelvis + spineDir * (torsoH * 0.24f);
        Vec3 skirtBot = pelvis - up * (thighLen * 0.88f);
        solid.cylinder(skirtTop, skirtBot, hipW * 1.05f, hipW * 1.72f, topC, 18);
    } else {
        solid.cylinder(pelvis, chestTop, hipW * 1.05f, shoulderW * 0.96f, topC, 16);
        solid.sphere(chestTop - spineDir * 0.03f, shoulderW * 0.95f, topC, 14, 10);
    }

    const Vec3 neckTop = chestTop + spineDir * (hr * 0.48f);
    solid.cylinder(chestTop - spineDir * 0.02f, neckTop, hr * 0.34f, hr * 0.31f, skinC, 12);
    const Vec3 headCenter = neckTop + up * (hr * 0.84f) + fwd * (std::sin(P.bow) * hr * 0.35f);
    addHead3D(solid, headCenter, hr, yaw, P.bow, L, false, false, false);

    const float rThigh = hipW * 0.48f;
    const float rShin = hipW * 0.37f;
    for (int i = 0; i < 2; ++i) {
        const float sd = (i == 0) ? -1.f : 1.f;
        const float po = (i == 0) ? 0.f : 3.14159265f;
        const float swing = P.moving ? P.amp * std::sin(P.phase + po) : (sd * 0.05f);
        const float kneeBend = P.moving ? std::max(0.08f, std::sin(P.phase + po + 1.45f) * 0.85f + 0.25f) : 0.05f;

        const Vec3 hip = pelvis + right * (sd * hipW * 0.56f);
        const Vec3 thighDir = normalized(fwd * std::sin(swing) - up * std::cos(swing));
        const Vec3 knee = hip + thighDir * thighLen;
        const float shinAng = swing - kneeBend;
        const Vec3 shinDir = normalized(fwd * std::sin(shinAng) - up * std::cos(shinAng));
        const Vec3 ankle = knee + shinDir * shinLen;

        if (!L.female && isChild) {
            Vec3 shortEnd = hip + thighDir * (thighLen * 0.82f);
            solid.sphere(hip, rThigh * 1.12f, botC, 10, 8);
            solid.cylinder(hip, shortEnd, rThigh * 1.12f, rThigh * 1.05f, botC, 12);
            solid.cylinder(shortEnd, knee, rThigh * 0.88f, rShin, skinC, 12);
        } else {
            const Color legC = L.female ? skinC : botC;
            solid.sphere(hip, rThigh, legC, 10, 8);
            solid.cylinder(hip, knee, rThigh, rShin, legC, 12);
        }
        const Color lowerLegC = (L.female || isChild) ? skinC : botC;
        solid.sphere(knee, rShin, lowerLegC, 10, 8);
        Vec3 sockTop = knee + shinDir * (shinLen * 0.72f);
        solid.cylinder(knee, sockTop, rShin, rShin * 0.88f, lowerLegC, 12);
        solid.cylinder(sockTop, ankle, rShin * 0.90f, rShin * 0.86f, sockC, 12);

        solid.sphere(ankle, rShin * 0.92f, shoeC, 10, 8);
        solid.cylinder(ankle - fwd * (rShin * 0.4f), ankle + fwd * (rShin * 1.9f),
                       rShin * 0.90f, rShin * 0.80f, shoeC, 10);
        solid.sphere(ankle + fwd * (rShin * 1.9f), rShin * 0.80f, shoeC, 10, 8);
    }

    const float armUpper = torsoH * 0.48f;
    const float armFore = torsoH * 0.45f;
    const float rArm = shoulderW * 0.25f;
    for (int i = 0; i < 2; ++i) {
        const float sd = (i == 0) ? -1.f : 1.f;
        const float po = (i == 0) ? 3.14159265f : 0.f;
        float swing = P.moving ? 0.55f * std::sin(P.phase + po) : 0.08f;
        float elbowBend = 0.55f + (P.moving ? 0.30f * std::max(0.f, std::sin(P.phase + po + 0.8f)) : 0.f);
        if (i == 1 && P.hasRaise) {
            swing = P.raise;
            elbowBend = P.hasWave ? (0.55f + 0.30f * std::sin(P.wave)) : 0.45f;
        }
        if (i == 1 && P.cane) {
            swing = 0.42f;
            elbowBend = 0.45f;
        }
        if (i == 0 && hasBriefcase) {
            swing = 0.12f;
            elbowBend = 0.18f;
        }

        const Vec3 sh = chestTop - spineDir * (rArm * 1.1f) + right * (sd * (shoulderW + rArm * 0.35f));
        const Vec3 uDir = normalized(fwd * std::sin(swing) - up * std::cos(swing) + right * (sd * 0.14f));
        const Vec3 el = sh + uDir * armUpper;
        const float fAng = swing + elbowBend;
        const Vec3 fDir = normalized(fwd * std::sin(fAng) - up * std::cos(fAng) - right * (sd * 0.08f));
        const Vec3 hand = el + fDir * armFore;

        solid.sphere(sh, rArm * 1.15f, sleeveC, 10, 8);
        solid.cylinder(sh, el, rArm, rArm * 0.88f, sleeveC, 10);
        solid.sphere(el, rArm * 0.90f, L.shortSleeve ? skinC : sleeveC, 10, 8);
        solid.cylinder(el, hand, rArm * 0.86f, rArm * 0.74f, L.shortSleeve ? skinC : sleeveC, 10);
        solid.sphere(hand, rArm * 0.92f, skinC, 10, 8);

        if (i == 1 && P.cane) {
            Vec3 caneTip{hand.x + fwd.x * 0.06f, 0.028f, hand.z + fwd.z * 0.06f};
            solid.cylinder(hand, caneTip, 0.016f, 0.014f, toCol(P.caneC), 10);
            solid.sphere(hand + up * 0.02f, 0.024f, toCol(shade(P.caneC, 1.2f)), 10, 8);
        }
        if (i == 0 && hasBriefcase) {
            Color bc = toCol(caseC);
            solid.box(hand - up * 0.14f, {0.12f, 0.24f, 0.34f}, bc);
            solid.box(hand - up * 0.02f, {0.03f, 0.04f, 0.12f}, toCol(shade(caseC, 0.7f)));
        }
    }
}

inline void addSeated3D(Mesh& solid, Vec3 seatHipPos, float scale, const fig::SeatedPose& P, float alpha = 1.f) {
    if (alpha <= 0.005f) return;
    const fig::Look& L = P.look;
    const bool isChild = (scale < 0.72f);
    const float torsoH = (isChild ? 0.34f : 0.46f) * (scale / 0.55f);
    const float thighLen = (isChild ? 0.35f : 0.41f);
    const float shinLen = (isChild ? 0.36f : 0.46f);
    const float shoulderW = (isChild ? 0.14f : 0.18f) * (scale / 0.55f);
    const float hipW = shoulderW * 0.76f;
    const float hr = (isChild ? 0.155f : 0.150f) * L.hs * (scale / 0.55f);

    const Color topC = toCol(L.top, alpha);
    const Color botC = toCol(L.bottom, alpha);
    const Color skinC = toCol(L.skin, alpha);
    const Color shoeC = toCol(L.shoe, alpha);
    const Color sockC = {0.95f, 0.95f, 0.97f, alpha};
    const Color sleeveC = (L.jacket != 0) ? toCol(L.jacket, alpha) : (L.female ? skinC : topC);

    const Vec3 pelvis = seatHipPos + Vec3{0.f, P.bounce * 0.012f, 0.f};
    solid.sphere(pelvis, hipW * 1.15f, L.female ? topC : botC, 14, 10);

    const Vec3 spineDir = normalized(Vec3{std::sin(P.tilt), std::cos(P.lean) * std::cos(P.tilt), -0.16f - P.lean * 0.25f});
    const Vec3 chestTop = pelvis + spineDir * torsoH;

    if (L.female) {
        Vec3 bodiceTop = pelvis + spineDir * (torsoH * 0.84f);
        solid.cylinder(pelvis, bodiceTop, hipW * 1.04f, shoulderW * 0.84f, topC, 16);
        solid.cylinder(bodiceTop, chestTop, shoulderW * 0.82f, shoulderW * 0.86f, skinC, 14);
        for (float sd : {-1.f, 1.f}) {
            Vec3 strapA = bodiceTop + Vec3{sd * shoulderW * 0.56f, 0.f, hipW * 0.42f};
            Vec3 strapB = chestTop + Vec3{sd * shoulderW * 0.56f, 0.f, 0.f};
            Vec3 strapC = bodiceTop + Vec3{sd * shoulderW * 0.56f, 0.f, -hipW * 0.42f};
            solid.cylinder(strapA, strapB, 0.015f, 0.015f, topC, 8);
            solid.cylinder(strapB, strapC, 0.015f, 0.015f, topC, 8);
        }
        Vec3 skirtTop = pelvis + spineDir * (torsoH * 0.18f);
        Vec3 skirtFront = pelvis + Vec3{0.f, -0.02f, thighLen * 0.68f};
        solid.cylinder(skirtTop, skirtFront, hipW * 1.08f, hipW * 1.58f, topC, 18);
    } else {
        solid.cylinder(pelvis, chestTop, hipW * 1.05f, shoulderW * 0.96f, topC, 16);
        solid.sphere(chestTop - spineDir * 0.03f, shoulderW * 0.95f, topC, 14, 10);
    }

    const Vec3 neckTop = chestTop + spineDir * (hr * 0.48f);
    solid.cylinder(chestTop - spineDir * 0.02f, neckTop, hr * 0.34f, hr * 0.31f, skinC, 12);
    const Vec3 headCenter = neckTop + Vec3{std::sin(P.turn) * hr * 0.18f, hr * 0.84f, 0.04f};
    addHead3D(solid, headCenter, hr, P.turn * 0.85f, 0.f, L, P.smile, P.open, P.blink, alpha);

    const float rThigh = hipW * 0.48f;
    const float rShin = hipW * 0.37f;
    for (int i = 0; i < 2; ++i) {
        const float sd = (i == 0) ? -1.f : 1.f;
        const Vec3 hip = pelvis + Vec3{sd * hipW * 0.58f, 0.01f, 0.02f};
        const Vec3 knee = hip + Vec3{sd * 0.01f, -0.01f, thighLen};
        const float kick = std::max(-0.08f, P.knee[i] * 0.45f);
        const Vec3 shinDir = normalized(Vec3{0.f, -std::cos(kick), std::sin(kick)});
        const Vec3 ankle = knee + shinDir * shinLen;

        if (!L.female && isChild) {
            Vec3 shortEnd = hip + Vec3{sd * 0.008f, -0.008f, thighLen * 0.82f};
            solid.sphere(hip, rThigh * 1.12f, botC, 10, 8);
            solid.cylinder(hip, shortEnd, rThigh * 1.12f, rThigh * 1.05f, botC, 12);
            solid.cylinder(shortEnd, knee, rThigh * 0.88f, rShin, skinC, 12);
        } else {
            const Color legC = L.female ? skinC : botC;
            solid.sphere(hip, rThigh, legC, 10, 8);
            solid.cylinder(hip, knee, rThigh, rShin, legC, 12);
        }
        const Color lowerLegC = (L.female || isChild) ? skinC : botC;
        solid.sphere(knee, rShin, lowerLegC, 10, 8);
        Vec3 sockTop = knee + shinDir * (shinLen * 0.72f);
        solid.cylinder(knee, sockTop, rShin, rShin * 0.88f, lowerLegC, 12);
        solid.cylinder(sockTop, ankle, rShin * 0.90f, rShin * 0.86f, sockC, 12);

        solid.sphere(ankle, rShin * 0.92f, shoeC, 10, 8);
        solid.cylinder(ankle + Vec3{0.f, 0.f, -rShin * 0.4f}, ankle + Vec3{0.f, 0.f, rShin * 1.9f},
                       rShin * 0.90f, rShin * 0.80f, shoeC, 10);
        solid.sphere(ankle + Vec3{0.f, 0.f, rShin * 1.9f}, rShin * 0.80f, shoeC, 10, 8);
    }

    const float rArm = shoulderW * 0.25f;
    for (int i = 0; i < 2; ++i) {
        const float sd = (i == 0) ? -1.f : 1.f;
        const bool hasHand = (i == 0) ? P.hasHandL : P.hasHandR;
        const Vec2 h2 = (i == 0) ? P.handL : P.handR;

        const Vec3 sh = chestTop - spineDir * (rArm * 1.1f) + Vec3{sd * (shoulderW + rArm * 0.45f), 0.f, 0.02f};
        Vec3 hand = pelvis + Vec3{sd * hipW * 0.75f, 0.05f, 0.24f};
        if (hasHand) {
            hand = Vec3{(h2.x - 320.f) * 0.023f,
                        std::max(0.92f, 0.91f + (266.f - h2.y) * 0.023f),
                        0.26f};
        }
        Vec3 mid = (sh + hand) * 0.5f + Vec3{sd * 0.06f, -0.04f, 0.06f};
        solid.sphere(sh, rArm * 1.15f, sleeveC, 10, 8);
        solid.cylinder(sh, mid, rArm, rArm * 0.88f, sleeveC, 10);
        solid.sphere(mid, rArm * 0.90f, L.shortSleeve ? skinC : sleeveC, 10, 8);
        solid.cylinder(mid, hand, rArm * 0.86f, rArm * 0.74f, L.shortSleeve ? skinC : sleeveC, 10);
        solid.sphere(hand, rArm * 0.92f, skinC, 10, 8);
    }
}

} // namespace fig3d

void addSceneFigures3D(Mesh& solidMesh, Mesh& transparentMesh, int scene, float t) {
    auto mapX = [](float x) { return (x - 320.f) * 0.023f; };

    auto avoidBench = [](float x, float z) {
        if (x > -2.35f && x < 2.35f && z > -0.78f && z < 0.82f) {
            return (z < 0.0f) ? -0.78f : 0.82f;
        }
        return z;
    };

    if (scene == 0) {
        struct TagKid3D {
            float rx, rz, cx, speed, phase0;
            uint32_t top, bot;
            bool female, chaser;
        };
        static const TagKid3D kids[5] = {
            {3.05f, 1.32f, -0.35f, 0.62f, 0.38f, fig3d::rgba32(0xe8, 0x6a, 0x5a), fig3d::rgba32(0x2b, 0x3a, 0x52), false, false},
            {3.05f, 1.32f, -0.35f, 0.62f, 0.02f, fig3d::rgba32(0x5a, 0xa0, 0xd8), 0,                                  true,  true },
            {3.45f, 1.48f,  0.25f, 0.54f, 3.25f, fig3d::rgba32(0x9b, 0x6f, 0xd0), fig3d::rgba32(0x25, 0x30, 0x42), false, false},
            {3.45f, 1.48f,  0.25f, 0.54f, 2.90f, fig3d::rgba32(0x4d, 0xb3, 0x8a), 0,                                  true,  true },
            {1.15f, 0.85f,  3.55f, 0.68f, 1.40f, fig3d::rgba32(0xf0, 0x8a, 0x3c), fig3d::rgba32(0x25, 0x30, 0x42), false, false}
        };
        for (int i = 0; i < 5; ++i) {
            const auto& k = kids[i];
            const float ang = k.speed * t + k.phase0;
            const float x = k.cx + k.rx * std::cos(ang);
            const float z = avoidBench(x, 0.12f + k.rz * std::sin(ang));
            const float yaw = std::atan2(-k.rx * k.speed * std::sin(ang), k.rz * k.speed * std::cos(ang));

            fig::WalkPose wp;
            wp.look = k.female ? fig::makeWoman() : fig::makeMan();
            wp.look.top = k.top;
            wp.look.bottom = k.female ? wp.look.skin : k.bot;
            wp.look.hair = k.female ? fig::Hair::Twin : fig::Hair::Short;
            wp.look.hs = 1.25f;
            wp.look.shortSleeve = true;
            wp.moving = true;
            wp.phase = t * 6.4f + k.phase0 * 2.f;
            wp.amp = 0.62f;
            wp.lean = 0.08f;
            if (k.chaser) {
                wp.hasRaise = true;
                wp.raise = 1.05f + 0.22f * std::sin(t * 6.f);
                wp.hasWave = true;
                wp.wave = t * 7.f;
            }
            fig3d::addWalking3D(solidMesh, transparentMesh, {x, 0.026f, z}, yaw, 0.54f, wp);
        }

        auto hopHero3D = [&](float x0, float xj, float xs, bool female,
                             uint32_t top, uint32_t bot, uint32_t shoe, float s, float ph) {
            const float tRun = 1.8f, tJ = 0.65f;
            fig::Look look = female ? fig::makeWoman() : fig::makeMan();
            look.top = top;
            look.bottom = female ? look.skin : bot;
            look.shoe = shoe;
            look.hair = female ? fig::Hair::Twin : fig::Hair::Short;
            look.hs = 1.25f;
            look.shortSleeve = true;

            if (t < tRun) {
                const float u = t / tRun;
                const float x = mapX(x0 + (xj - x0) * u);
                const float z = 1.15f - 0.35f * u;
                const float yaw = (xj > x0) ? 1.35f : -1.35f;
                fig::WalkPose wp;
                wp.look = look; wp.moving = true; wp.phase = t * 6.0f + ph; wp.amp = 0.62f; wp.lean = 0.07f;
                fig3d::addWalking3D(solidMesh, transparentMesh, {x, 0.026f, z}, yaw, s, wp);
                return;
            }
            if (t < tRun + tJ) {
                const float u = (t - tRun) / tJ;
                const float x = mapX(xj + (xs - xj) * u);
                const float z = 0.80f + (0.04f - 0.80f) * u;
                const float y = 0.42f + (0.92f - 0.42f) * u + 0.36f * std::sin(3.14159265f * u);
                fig::SeatedPose sp;
                sp.look = look; sp.knee[0] = 0.35f; sp.knee[1] = -0.25f; sp.smile = true; sp.open = true;
                fig3d::addSeated3D(solidMesh, {x, y, z}, s, sp);
                return;
            }
            const float ts = t - (tRun + tJ);
            const float w = ts * 4.4f + ph;
            const bool waving = (!female && ts > 2.2f && ts < 4.5f) || (female && ts > 5.0f && ts < 7.2f);
            fig::SeatedPose sp;
            sp.look = look;
            sp.knee[0] = 0.35f * std::sin(w);
            sp.knee[1] = 0.35f * std::sin(w + 3.14159265f);
            sp.bounce = 0.7f * std::abs(std::sin(ts * 4.4f + ph));
            sp.smile = true;
            sp.open = std::sin(ts * 5.f + ph) > 0.25f;
            sp.blink = std::fmod((t + ph) * 0.83f, 3.3f) < 0.13f;
            sp.turn = waving ? (female ? -0.45f : 0.45f) : (female ? -0.65f : 0.65f);
            sp.tilt = female ? -0.06f : 0.06f;
            if (waving) {
                if (!female) { sp.hasHandL = true; sp.handL = {xs - 15.f * s, 266.f - 26.f * s}; }
                else         { sp.hasHandR = true; sp.handR = {xs + 15.f * s, 266.f - 26.f * s}; }
            }
            fig3d::addSeated3D(solidMesh, {mapX(xs), 0.92f, 0.04f}, s, sp);
        };

        hopHero3D(-25.f, 250.f, 294.f, false, fig3d::rgba32(0xf4, 0xd0, 0x3f), fig3d::rgba32(0x23, 0x35, 0x54), fig3d::rgba32(0xc8, 0x32, 0x32), 0.55f, 0.f);
        hopHero3D(665.f, 392.f, 346.f, true,  fig3d::rgba32(0xf2, 0x74, 0x6b), 0,                                  fig3d::rgba32(0x8c, 0x53, 0x2d), 0.52f, 1.8f);
        return;
    }

    auto boyLook = [](uint32_t top, uint32_t bot, uint32_t shoe = 0) {
        fig::Look L = fig::makeMan();
        L.top = top; L.bottom = bot;
        if (shoe) L.shoe = shoe;
        L.hair = fig::Hair::Short;
        return L;
    };
    auto girlLook = [](uint32_t top, fig::Hair h = fig::Hair::Twin, uint32_t shoe = 0) {
        fig::Look L = fig::makeWoman();
        L.top = top; L.bottom = L.skin; L.hair = h;
        if (shoe) L.shoe = shoe;
        return L;
    };

    if (scene == 1) {
        const float xb = 298.f;
        const float xg = 342.f;
        const float lendU = std::clamp(t / 2.6f, 0.f, 1.f);
        const bool listening = (t >= 2.6f);
        const float beat = listening ? std::sin((t - 2.6f) * 5.6f) : 0.f;

        fig::SeatedPose sb, sg;
        sb.look = boyLook(fig3d::rgba32(0x3f, 0x88, 0xc5), fig3d::rgba32(0x2c, 0x36, 0x45), fig3d::rgba32(0x33, 0x33, 0x38));
        sg.look = girlLook(fig3d::rgba32(0xf4, 0xb9, 0x42), fig::Hair::Braids);
        sb.turn = 0.48f;
        sg.turn = -0.48f;
        sb.tilt = listening ? 0.05f * beat : 0.06f * std::sin(3.14159f * lendU);
        sg.tilt = listening ? -0.05f * beat : -0.04f * std::sin(3.14159f * lendU);
        sb.bounce = listening ? 0.45f * std::abs(beat) : 0.f;
        sg.bounce = listening ? 0.45f * std::abs(std::sin((t - 2.6f) * 5.6f + 0.4f)) : 0.f;
        sb.smile = sg.smile = true;
        sb.open = std::sin(t * 4.5f) > 0.35f;
        sg.open = std::sin(t * 4.5f + 2.0f) > 0.35f;
        if (listening) {
            sb.knee[0] = 0.32f * std::sin((t - 2.6f) * 5.6f);
            sg.knee[1] = 0.32f * std::sin((t - 2.6f) * 5.6f + 0.5f);
        }

        sb.hasHandL = true;
        sb.handL = {316.f, 261.f};
        sb.hasHandR = true;
        if (!listening) {
            sb.handR = {316.f + (xg - 6.f - 316.f) * lendU, 261.f + (242.f - 261.f) * lendU};
            sg.hasHandL = true;
            sg.handL = {xg - 8.f, 256.f - 7.f * lendU};
        } else {
            sb.handR = {xb + 10.f, 262.f};
            sg.hasHandL = true;
            sg.handL = {321.f, 261.f};
        }

        fig3d::addSeated3D(solidMesh, {mapX(xb), 0.92f, 0.04f}, 0.72f, sb);
        fig3d::addSeated3D(solidMesh, {mapX(xg), 0.92f, 0.04f}, 0.70f, sg);

        Vec3 playerPos{mapX(316.f), 1.03f, 0.30f};
        solidMesh.box(playerPos, {0.10f, 0.024f, 0.15f}, rgb(35, 42, 54));
        solidMesh.box(playerPos + Vec3{0.f, 0.014f, 0.f}, {0.075f, 0.005f, 0.10f}, rgb(110, 215, 255), 0.7f);

        Vec3 earB{mapX(xb) + 0.11f, 1.48f, 0.08f};
        Vec3 earGTarget{mapX(xg) - 0.11f, 1.46f, 0.08f};
        Vec3 handReach{mapX(316.f + (xg - 6.f - 316.f) * lendU), 1.03f + (1.46f - 1.03f) * lendU, 0.24f - 0.16f * lendU};
        Vec3 earG = listening ? earGTarget : handReach;

        solidMesh.sphere(earB, 0.030f, rgb(245, 248, 252), 10, 8);
        solidMesh.sphere(earG, 0.030f, rgb(245, 248, 252), 10, 8);
        solidMesh.cylinder(playerPos, earB, 0.006f, 0.006f, rgb(235, 238, 242), 6);
        solidMesh.cylinder(playerPos, earG, 0.006f, 0.006f, rgb(235, 238, 242), 6);

        float jx = -5.5f + std::fmod(t * 1.15f, 11.f);
        fig::WalkPose jw; jw.look = girlLook(fig3d::rgba32(0x6c, 0xba, 0x99), fig::Hair::Bun);
        jw.moving = true; jw.phase = t * 6.5f; jw.amp = 0.58f;
        fig3d::addWalking3D(solidMesh, transparentMesh, {jx, 0.026f, -1.35f}, 1.57f, 0.68f, jw);
        return;
    }

    if (scene == 2) {
        const float xbSeat = 294.f;
        const float xgSeat = 346.f;
        const float tWalk = 3.2f;
        const bool arrived = (t >= tWalk);

        fig::SeatedPose sg;
        sg.look = girlLook(fig3d::rgba32(0xf2, 0xe9, 0xe4), fig::Hair::Bun);
        sg.turn = arrived ? -0.50f : -0.75f;
        sg.smile = true;
        sg.open = std::sin(t * 1.8f) > 0.45f;
        if (arrived) {
            sg.hasHandL = true;
            sg.handL = {322.f, 263.f};
        }
        fig3d::addSeated3D(solidMesh, {mapX(xgSeat), 0.92f, 0.04f}, 0.94f, sg);

        fig::Look manL = boyLook(fig3d::rgba32(0x28, 0x4b, 0x63), fig3d::rgba32(0x1b, 0x26, 0x3b));
        uint32_t caseColor = fig3d::rgba32(0x5c, 0x3a, 0x21);

        if (!arrived) {
            float u = t / tWalk;
            float x = -4.2f + (mapX(xbSeat) + 4.2f) * u;
            float z = 1.65f + (0.58f - 1.65f) * u;
            fig::WalkPose wp;
            wp.look = manL;
            wp.moving = true;
            wp.phase = t * 4.6f;
            wp.amp = 0.50f;
            fig3d::addWalking3D(solidMesh, transparentMesh, {x, 0.026f, z}, 1.32f, 0.98f, wp, true, caseColor);
        } else {
            Vec3 bcPos{mapX(xbSeat) - 0.44f, 0.19f, 0.22f};
            solidMesh.box(bcPos, {0.13f, 0.32f, 0.42f}, fig3d::toCol(caseColor));
            solidMesh.box(bcPos + Vec3{0.f, 0.18f, 0.f}, {0.03f, 0.04f, 0.14f}, rgb(42, 26, 15));

            fig::SeatedPose sb;
            sb.look = manL;
            sb.turn = 0.50f;
            sb.smile = true;
            sb.open = std::sin(t * 1.8f + 1.6f) > 0.45f;
            sb.hasHandR = true;
            sb.handR = {318.f, 263.f};
            fig3d::addSeated3D(solidMesh, {mapX(xbSeat), 0.92f, 0.04f}, 0.98f, sb);
        }
        return;
    }

    if (scene == 3) {
        const float xb = 286.f;
        const float xg = 354.f;
        fig::SeatedPose sb, sg;
        sb.look = boyLook(fig3d::rgba32(0x28, 0x4b, 0x63), fig3d::rgba32(0x1b, 0x26, 0x3b));
        sg.look = girlLook(fig3d::rgba32(0xf2, 0xe9, 0xe4), fig::Hair::Bun);
        sb.turn = 0.52f + 0.10f * std::sin(t * 1.7f);
        sg.turn = -0.52f - 0.10f * std::cos(t * 1.7f);
        sb.bounce = 0.28f * std::abs(std::sin(t * 2.8f));
        sg.bounce = 0.28f * std::abs(std::sin(t * 2.8f + 1.0f));
        sb.smile = sg.smile = true;
        sb.open = std::sin(t * 4.8f) > 0.3f;
        sg.open = std::sin(t * 4.8f + 2.3f) > 0.3f;
        sb.hasHandR = sg.hasHandL = true;
        sb.handR = {318.f, 263.f + 1.5f * std::sin(t * 2.5f)};
        sg.handL = {322.f, 263.f + 1.5f * std::sin(t * 2.5f)};
        sb.hasHandL = true;
        sb.handL = {xb - 14.f, 255.f - 5.f * std::sin(t * 3.2f)};
        sg.hasHandR = true;
        sg.handR = {xg + 12.f, 260.f};
        fig3d::addSeated3D(solidMesh, {mapX(xb), 0.92f, 0.04f}, 0.98f, sb);
        fig3d::addSeated3D(solidMesh, {mapX(xg), 0.92f, 0.04f}, 0.94f, sg);

        Vec3 bq{mapX(xg + 12.f), 1.07f, 0.30f};
        solidMesh.cylinder(bq - Vec3{0.f, 0.10f, 0.f}, bq, 0.025f, 0.055f, rgb(76, 138, 72), 10);
        solidMesh.sphere(bq + Vec3{0.f, 0.03f, 0.f}, 0.09f, rgb(252, 232, 238), 10, 8);
        solidMesh.sphere(bq + Vec3{-0.05f, 0.05f, 0.03f}, 0.055f, rgb(242, 128, 152), 8, 6);
        solidMesh.sphere(bq + Vec3{ 0.05f, 0.05f, -0.02f}, 0.055f, rgb(255, 214, 102), 8, 6);
        for (int p = 0; p < 10; ++p) {
            float px = -1.6f + 3.2f * random01(p * 31 + 3);
            float py = 2.6f - std::fmod(t * 0.42f + p * 0.31f, 2.5f);
            float pz = -0.3f + 1.1f * random01(p * 47 + 5);
            solidMesh.sphere({px + 0.15f * std::sin(t * 2.f + p), py, pz}, 0.024f, rgb(250, 198, 212), 6, 5);
        }
        return;
    }

    if (scene == 4) {
        const float xb = 288.f;
        const float xg = 352.f;
        const float toddlerWave = std::sin(t * 0.9f);
        fig::SeatedPose sb, sg;
        sb.look = boyLook(fig3d::rgba32(0x3a, 0x5a, 0x40), fig3d::rgba32(0x2b, 0x2d, 0x42));
        sg.look = girlLook(fig3d::rgba32(0xd4, 0xa3, 0x73), fig::Hair::Bun);
        sb.turn = 0.35f + 0.25f * toddlerWave;
        sg.turn = -0.35f + 0.25f * toddlerWave;
        sb.bounce = 0.25f * std::abs(std::sin(t * 2.5f));
        sg.bounce = 0.25f * std::abs(std::sin(t * 2.5f + 1.2f));
        sb.smile = sg.smile = true;
        sb.open = std::sin(t * 4.0f) > 0.25f;
        sg.open = std::sin(t * 4.0f + 1.9f) > 0.25f;
        sb.hasHandR = true;
        sb.handR = {xb + 14.f, 255.f - 6.f * std::abs(std::sin(t * 3.2f))};
        sg.hasHandL = true;
        sg.handL = {xg - 14.f, 255.f - 6.f * std::abs(std::cos(t * 3.2f))};
        fig3d::addSeated3D(solidMesh, {mapX(xb), 0.92f, 0.04f}, 1.0f, sb);
        fig3d::addSeated3D(solidMesh, {mapX(xg), 0.92f, 0.04f}, 0.96f, sg);

        float tx = mapX(320.f + 46.f * toddlerWave);
        fig::WalkPose tw; tw.look = boyLook(fig3d::rgba32(0xf7, 0xb2, 0x67), fig3d::rgba32(0x3d, 0x5a, 0x80));
        tw.look.hs = 1.25f; tw.moving = true; tw.phase = t * 6.2f; tw.amp = 0.52f;
        tw.hasRaise = true; tw.raise = 0.9f + 0.25f * std::sin(t * 5.f);
        fig3d::addWalking3D(solidMesh, transparentMesh, {tx, 0.026f, 1.35f}, std::cos(t * 0.9f) > 0 ? 1.57f : -1.57f, 0.46f, tw);

        float ballX = tx + (std::cos(t * 0.9f) > 0 ? 0.34f : -0.34f);
        float ballY = 0.14f + 0.18f * std::abs(std::sin(t * 4.2f));
        solidMesh.sphere({ballX, ballY, 1.38f}, 0.11f, rgb(225, 62, 62), 14, 10);
        return;
    }

    if (scene == 5) {
        const float xb = 278.f;
        const float xg = 326.f;
        const float xdSeat = 374.f;
        const bool seatedPhase = (t >= 8.2f);

        fig::SeatedPose sb, sg;
        sb.look = boyLook(fig3d::rgba32(0x4a, 0x57, 0x59), fig3d::rgba32(0x22, 0x22, 0x3b));
        sb.look.hairC = fig3d::rgba32(0x6c, 0x75, 0x7d);
        sg.look = girlLook(fig3d::rgba32(0xb5, 0x83, 0x8d), fig::Hair::Bun);
        sg.look.hairC = fig3d::rgba32(0x6c, 0x75, 0x7d);
        sb.turn = seatedPhase ? 0.45f : 0.15f;
        sg.turn = seatedPhase ? 0.45f : 0.10f;
        sb.smile = sg.smile = true;
        sb.open = std::sin(t * 3.8f) > 0.3f;
        sg.open = std::sin(t * 3.8f + 1.8f) > 0.3f;
        sb.hasHandR = sg.hasHandL = true;
        sb.handR = {300.f, 265.f};
        sg.handL = {304.f, 265.f};

        if (t >= 3.8f && t < 7.8f) {
            sb.hasHandL = true;
            sb.handL = {xb - 14.f, 246.f - 8.f * std::sin(t * 5.f)};
            sg.hasHandR = true;
            sg.handR = {xg + 14.f, 246.f - 8.f * std::cos(t * 5.f)};
        }
        fig3d::addSeated3D(solidMesh, {mapX(xb), 0.92f, 0.04f}, 0.98f, sb);
        fig3d::addSeated3D(solidMesh, {mapX(xg), 0.92f, 0.04f}, 0.94f, sg);

        fig::Look childL = girlLook(fig3d::rgba32(0x52, 0x79, 0x6f), fig::Hair::Bun);
        if (t < 3.8f) {
            float u = t / 3.8f;
            float vx = -5.2f + (1.25f - (-5.2f)) * u;
            fig::WalkPose vw;
            vw.look = childL; vw.moving = true; vw.phase = t * 4.8f; vw.amp = 0.52f;
            fig3d::addWalking3D(solidMesh, transparentMesh, {vx, 0.026f, 2.55f}, 1.48f, 0.94f, vw);
        } else if (t < 6.2f) {
            fig::WalkPose vw;
            vw.look = childL; vw.moving = false;
            vw.hasRaise = true; vw.raise = 1.25f; vw.hasWave = true; vw.wave = t * 6.5f;
            fig3d::addWalking3D(solidMesh, transparentMesh, {1.25f, 0.026f, 2.55f}, 3.14159f, 0.94f, vw);
        } else if (!seatedPhase) {
            float u = (t - 6.2f) / 2.0f;
            float vx = 1.25f + (mapX(xdSeat) - 1.25f) * u;
            float vz = 2.55f + (0.55f - 2.55f) * u;
            fig::WalkPose vw;
            vw.look = childL; vw.moving = true; vw.phase = t * 4.8f; vw.amp = 0.48f;
            fig3d::addWalking3D(solidMesh, transparentMesh, {vx, 0.026f, vz}, 3.14159f, 0.94f, vw);
        } else {
            fig::SeatedPose sd;
            sd.look = childL;
            sd.turn = -0.55f;
            sd.smile = true;
            sd.open = std::sin(t * 4.0f + 1.0f) > 0.3f;
            fig3d::addSeated3D(solidMesh, {mapX(xdSeat), 0.92f, 0.04f}, 0.94f, sd);
        }
        return;
    }

    if (scene == 6) {
        const float xb = 294.f;
        const float xg = 346.f;
        fig::SeatedPose sb, sg;
        sb.look = boyLook(fig3d::rgba32(0x5c, 0x67, 0x7d), fig3d::rgba32(0x2b, 0x2d, 0x42));
        sb.look.hairC = fig3d::rgba32(0xc8, 0xcd, 0xd4);
        sg.look = girlLook(fig3d::rgba32(0x9d, 0x81, 0x89), fig::Hair::Bun);
        sg.look.hairC = fig3d::rgba32(0xc8, 0xcd, 0xd4);

        sb.turn = 0.48f;
        sg.turn = -0.48f;
        sb.lean = 0.10f;
        sg.lean = 0.10f;
        sb.smile = sg.smile = true;
        sb.hasHandR = sg.hasHandL = true;
        sb.handR = {318.f, 265.f};
        sg.handL = {322.f, 265.f};

        fig3d::addSeated3D(solidMesh, {mapX(xb), 0.92f, 0.04f}, 0.95f, sb, 1.0f);

        float ladyAlpha = 1.0f;
        if (t > 4.5f) {
            float u = std::clamp((t - 4.5f) / 8.5f, 0.f, 1.f);
            ladyAlpha = 1.0f - (u * u * (3.0f - 2.0f * u));
        }
        if (ladyAlpha > 0.99f) {
            fig3d::addSeated3D(solidMesh, {mapX(xg), 0.92f, 0.04f}, 0.91f, sg, 1.0f);
        } else if (ladyAlpha > 0.005f) {
            fig3d::addSeated3D(transparentMesh, {mapX(xg), 0.92f, 0.04f}, 0.91f, sg, ladyAlpha);
        }
        return;
    }

    if (scene == 7) {
        fig::Look oldL = boyLook(fig3d::rgba32(0x49, 0x50, 0x57), fig3d::rgba32(0x21, 0x25, 0x29));
        oldL.hairC = fig3d::rgba32(0xe9, 0xec, 0xef);
        if (t < 5.5f) {
            float u = t / 5.5f;
            float x = -4.2f + (mapX(300.f) + 4.2f) * u;
            float z = 2.6f + (0.85f - 2.6f) * u;
            fig::WalkPose wp; wp.look = oldL; wp.moving = true; wp.phase = t * 3.2f;
            wp.amp = 0.36f; wp.lean = 0.16f; wp.bow = 0.14f; wp.cane = true; wp.caneC = fig3d::rgba32(0x6f, 0x4e, 0x37);
            fig3d::addWalking3D(solidMesh, transparentMesh, {x, 0.026f, z}, 1.35f, 0.92f, wp);
        } else {
            fig::SeatedPose sp; sp.look = oldL; sp.lean = 0.14f; sp.turn = 0.25f; sp.smile = true;
            fig3d::addSeated3D(solidMesh, {mapX(300.f), 0.92f, 0.04f}, 0.92f, sp);
            solidMesh.cylinder({mapX(282.f), 0.03f, 0.36f}, {mapX(285.f), 0.88f, 0.15f}, 0.016f, 0.016f, fig3d::toCol(fig3d::rgba32(0x6f, 0x4e, 0x37)), 10);
        }
        return;
    }
}

void addSceneryDetails(Mesh& solid, Mesh& trans, const SceneLook& look, int scene, float t) {
    const float dim = look.ambient < .3f ? .58f : 0.f;

    const Color bushBase = mix(look.leaves, look.ground, 0.35f);
    for (int i = 0; i < 14; ++i) {
        float bx = -11.5f + float(i) * 1.75f + (random01(i * 17) - 0.5f) * 0.5f;
        if (bx > -3.1f && bx < 3.1f) continue;
        float bz = -6.35f - random01(i * 23) * 2.1f;
        float br = 0.42f + 0.26f * random01(i * 31);
        Color bc = mix(bushBase, rgb(38, 78, 42), 0.25f * random01(i * 41));
        solid.sphere({bx, br * 0.65f, bz}, br, bc, 10, 8);
        solid.sphere({bx + br * 0.58f, br * 0.48f, bz + 0.15f}, br * 0.72f, bc, 10, 8);
    }

    const Color fenceWood = mix(rgb(118, 86, 58), rgb(24, 22, 20), dim);
    for (float fx = -10.0f; fx <= 10.0f; fx += 2.0f) {
        solid.cylinder({fx, 0.f, -5.2f}, {fx, 0.98f, -5.2f}, 0.072f, 0.065f, fenceWood, 8);
        solid.sphere({fx, 1.00f, -5.2f}, 0.070f, fenceWood, 8, 6);
        if (fx < 10.0f) {
            solid.cylinder({fx, 0.78f, -5.2f}, {fx + 2.0f, 0.78f, -5.2f}, 0.044f, 0.044f, fenceWood, 8);
            solid.cylinder({fx, 0.42f, -5.2f}, {fx + 2.0f, 0.42f, -5.2f}, 0.044f, 0.044f, fenceWood, 8);
        }
    }

    const Color binCol = mix(rgb(44, 82, 66), rgb(14, 20, 18), dim);
    solid.cylinder({5.35f, 0.02f, -0.10f}, {5.35f, 0.68f, -0.10f}, 0.19f, 0.22f, binCol, 14);
    solid.cylinder({5.35f, 0.68f, -0.10f}, {5.35f, 0.72f, -0.10f}, 0.23f, 0.23f, mix(binCol, rgb(20, 24, 22), 0.4f), 14);

    if (scene <= 5 || scene == 8) {
        Color cloudCol = mix(rgb(255, 255, 255), look.skyBottom, 0.22f);
        static const float baseX[4] = { -7.5f, -2.2f, 3.4f, 8.8f };
        static const float baseY[4] = {  6.1f,  7.2f, 6.4f, 7.0f };
        static const float speed[4] = { 0.11f, 0.08f, 0.10f, 0.07f };
        for (int c = 0; c < 4; ++c) {
            float cx = baseX[c] + t * speed[c];
            float cy = baseY[c] + 0.08f * std::sin(t * 0.35f + float(c) * 1.7f);
            float cz = -18.5f - float(c) * 1.1f;
            solid.sphere({cx, cy, cz}, 0.95f, cloudCol, 14, 10, 1.0f);
            solid.sphere({cx + 0.85f, cy - 0.10f, cz + 0.1f}, 0.74f, cloudCol, 12, 8, 1.0f);
            solid.sphere({cx - 0.80f, cy - 0.14f, cz + 0.1f}, 0.70f, cloudCol, 12, 8, 1.0f);
            solid.sphere({cx + 0.25f, cy + 0.28f, cz - 0.05f}, 0.62f, cloudCol, 12, 8, 1.0f);
        }
    }

    if (scene >= 5 && scene <= 7) {
        for (int f = 0; f < 14; ++f) {
            float fx = -5.5f + 11.f * random01(f * 37 + 9) + 0.35f * std::sin(t * 1.3f + f);
            float fy = 0.45f + 1.4f * random01(f * 53 + 3) + 0.22f * std::cos(t * 1.7f + f * 2.f);
            float fz = -2.5f + 4.5f * random01(f * 71 + 5);
            float pulse = 0.4f + 0.6f * std::max(0.f, std::sin(t * 3.2f + f * 1.9f));
            trans.sphere({fx, fy, fz}, 0.024f * pulse, {1.0f, 0.92f, 0.45f, 0.85f * pulse}, 6, 5, 1.0f);
        }
    }
}

std::vector<Vertex> buildScene3D(int scene, float t = 2.5f) {
    const SceneLook& look = kScenes[size_t(scene)];
    Mesh solidMesh, transparentMesh;
    addSky(solidMesh, look);
    addHills(solidMesh, look);
    addGrass(solidMesh, look, scene);
    addPath(solidMesh, look.ground);
    addTree(solidMesh, look, scene);
    addLamp(solidMesh, look, scene, t);
    addBench(solidMesh, look);
    solidMesh.sphere(sunPositionFor(scene), .38f, look.sun, 48, 32, 1.0f);

    addSceneFigures3D(solidMesh, transparentMesh, scene, t);
    addSceneryDetails(solidMesh, transparentMesh, look, scene, t);
    solidMesh.vertices.insert(solidMesh.vertices.end(),
                              transparentMesh.vertices.begin(),
                              transparentMesh.vertices.end());
    return std::move(solidMesh.vertices);
}

using Mat4 = std::array<float,16>;

Mat4 identity() {
    return {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 result{};
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                result[size_t(col*4+row)] += a[size_t(k*4+row)] * b[size_t(col*4+k)];
    return result;
}

Mat4 perspective(float fov, float aspect, float nearPlane, float farPlane) {
    const float f = 1.f / std::tan(fov * .5f);
    Mat4 m{};
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    m[11] = -1.f;
    m[14] = (2.f * farPlane * nearPlane) / (nearPlane - farPlane);
    return m;
}

Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 forward = normalized(target - eye);
    const Vec3 side = normalized(cross(forward, up));
    const Vec3 correctedUp = cross(side, forward);
    Mat4 m = identity();
    m[0] = side.x;        m[4] = side.y;        m[8] = side.z;
    m[1] = correctedUp.x; m[5] = correctedUp.y; m[9] = correctedUp.z;
    m[2] = -forward.x;    m[6] = -forward.y;    m[10] = -forward.z;
    m[12] = -dot(side, eye);
    m[13] = -dot(correctedUp, eye);
    m[14] = dot(forward, eye);
    return m;
}

void renderFrame(GLFWwindow* window, GLuint program, GLuint vao, size_t vertexCount,
                 int scene, float sceneTime,  float yaw, float pitch, float distance) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    float darknessFade = 1.f;
    if (scene == 7 && sceneTime >= 13.0f) {
        float u = std::clamp((sceneTime - 13.0f) / 1.5f, 0.f, 1.f);
        darknessFade = 1.f - (u * u * (3.f - 2.f * u));
    }
    const Color sky = kScenes[size_t(scene)].sky;
    glClearColor(sky.r * darknessFade, sky.g * darknessFade, sky.b * darknessFade, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    const Vec3 target{-0.25f, 1.45f, -0.35f};
    const Vec3 eye{target.x + std::sin(yaw)*distance*std::cos(pitch),
                   target.y + std::sin(pitch)*distance,
                   target.z + std::cos(yaw)*distance*std::cos(pitch)};
    const Mat4 view = lookAt(eye, target, {0,1,0});
    const Mat4 projection = perspective(.785398f, float(width)/float(height), .1f, 80.f);
    const Mat4 mvp = multiply(projection, view);
    glUniformMatrix4fv(glGetUniformLocation(program, "uMVP"), 1, GL_FALSE, mvp.data());

    const SceneLook& look = kScenes[size_t(scene)];
    const float lampPower = lampPowerAt(scene, sceneTime);
    const float ambient = look.ambient * (0.04f + 0.96f * darknessFade);

    const Vec3 sunDir = normalized(sunPositionFor(scene) - target + Vec3{0.f, 0.f, 9.5f});
    glUniform3f(glGetUniformLocation(program, "uSunDirection"), sunDir.x, sunDir.y, sunDir.z);
    glUniform3f(glGetUniformLocation(program, "uLampPosition"),
                kLampBulbPos.x, kLampBulbPos.y, kLampBulbPos.z);
    glUniform1f(glGetUniformLocation(program, "uAmbient"), ambient);
    glUniform1f(glGetUniformLocation(program, "uSunlight"), look.sunlight);
    glUniform1f(glGetUniformLocation(program, "uLampPower"), lampPower);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertexCount));
}

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(size_t(std::max(length, 1)));
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        std::cerr << "Shader compilation failed: " << log.data() << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint createProgram() {
    constexpr const char* vertexSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPosition;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec4 aColor;
        layout (location = 3) in float aEmissive;
        uniform mat4 uMVP;
        out vec3 vPosition;
        out vec3 vNormal;
        out vec4 vColor;
        out float vEmissive;
        void main() {
            vPosition = aPosition;
            vNormal = aNormal;
            vColor = aColor;
            vEmissive = aEmissive;
            gl_Position = uMVP * vec4(aPosition, 1.0);
        })";
    constexpr const char* fragmentSource = R"(
        #version 330 core
        in vec3 vPosition;
        in vec3 vNormal;
        in vec4 vColor;
        in float vEmissive;
        uniform vec3 uSunDirection;
        uniform vec3 uLampPosition;
        uniform float uAmbient;
        uniform float uSunlight;
        uniform float uLampPower;
        out vec4 FragColor;
        void main() {
            vec3 normal = normalize(vNormal);
            vec3 sunDirection = normalize(uSunDirection);
            float sun = max(dot(normal, sunDirection), 0.0) * uSunlight;
            vec3 toLamp = uLampPosition - vPosition;
            float distanceSquared = max(dot(toLamp, toLamp), 0.01);
            float lamp = max(dot(normal, normalize(toLamp)), 0.0)
                       * (uLampPower / (1.0 + 0.12 * distanceSquared));
            vec3 lit = vColor.rgb * (vec3(uAmbient + sun) + vec3(1.0, 0.74, 0.42) * lamp);
            vec3 finalColor = mix(lit, vColor.rgb, clamp(vEmissive, 0.0, 1.0));
            FragColor = vec4(clamp(finalColor, 0.0, 1.0), vColor.a);
        })";
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (!vertex) return 0;
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!fragment) {
        glDeleteShader(vertex);
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(size_t(std::max(length, 1)));
        glGetProgramInfoLog(program, length, nullptr, log.data());
        std::cerr << "Shader linking failed: " << log.data() << '\n';
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

bool savePpm(GLFWwindow* window, int scene, const std::string& requestedPath) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) return false;
    std::vector<unsigned char> pixels(size_t(width) * size_t(height) * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    const std::string path = requestedPath.empty()
        ? "out/scene3d-" + std::to_string(scene) + ".ppm"
        : requestedPath;
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Could not write screenshot: " << path << '\n';
        return false;
    }
    file << "P6\n" << width << ' ' << height << "\n255\n";
    const size_t rowSize = size_t(width) * 3;
    for (int row = height - 1; row >= 0; --row)
        file.write(reinterpret_cast<const char*>(pixels.data() + size_t(row) * rowSize),
                   std::streamsize(rowSize));
    std::cout << "Saved " << path << '\n';
    return bool(file);
}

void updateWindowTitle(GLFWwindow* window, int scene) {
    const std::string title = "Bench Years 3D - Scene " + std::to_string(scene)
        + " | arrows: scenes, WASD: orbit, Q/E: zoom, P: screenshot";
    glfwSetWindowTitle(window, title.c_str());
}

// Exports numbered 3D JSON frames to <prefix>_NNNN.json using film.cpp's timeline,
// leaving gaps during transition intervals so Blender can render smooth fades.
bool exportFrames3D(const std::string& prefix, int fps) {
    constexpr float kSceneLen[8] = { 11.f, 11.5f, 12.f, 12.f, 13.f, 14.f, 14.5f, 16.f };
    constexpr float kGustLen[7]  = { 2.8f, 2.8f, 3.0f, 3.0f, 3.2f, 3.4f, 3.8f };

    float totalLen = 0.f;
    for (float d : kSceneLen) totalLen += d;
    for (float d : kGustLen)  totalLen += d;

    const int totalFrames = int(std::ceil(totalLen * float(fps)));
    const Vec3 target{-0.25f, 1.45f, -0.35f};

    std::cout << "Exporting 3D frames (" << totalFrames << " total timeline frames at "
              << fps << " fps)...\n";

    int exported = 0;
    for (int frameIdx = 0; frameIdx < totalFrames; ++frameIdx) {
        const float T = float(frameIdx) / float(fps);

        bool inTransition = false;
        int scene = 7;
        float sceneTime = kSceneLen[7];
        float start = 0.f;
        for (int i = 0; i < 8; ++i) {
            if (T < start + kSceneLen[i]) {
                scene = i;
                sceneTime = T - start;
                break;
            }
            start += kSceneLen[i];
            if (i == 7) break;
            if (T < start + kGustLen[i]) {
                inTransition = true;
                break;
            }
            start += kGustLen[i];
        }

        if (inTransition) continue;

        std::vector<Vertex> verts = buildScene3D(scene, sceneTime);
        const SceneLook& look = kScenes[size_t(scene)];

        float darknessFade = 1.f;
        if (scene == 7 && sceneTime >= 13.0f) {
            float u = std::clamp((sceneTime - 13.0f) / 1.5f, 0.f, 1.f);
            darknessFade = 1.f - (u * u * (3.f - 2.f * u));
        }

        const float lampPower = lampPowerAt(scene, sceneTime);
        const float ambient = look.ambient * (0.04f + 0.96f * darknessFade);
        const Vec3 sunDir = normalized(sunPositionFor(scene) - target + Vec3{0.f, 0.f, 9.5f});

        char numBuf[32];
        std::snprintf(numBuf, sizeof(numBuf), "_%04d.json", frameIdx);
        const std::string outPath = prefix + numBuf;

        std::ofstream out(outPath);
        if (!out) {
            std::cerr << "Failed to open " << outPath << " for writing\n";
            return false;
        }

        out << std::fixed << std::setprecision(4);
        out << "{\n"
            << "  \"width\": 1280,\n"
            << "  \"height\": 720,\n"
            << "  \"scene\": " << scene << ",\n"
            << "  \"t\": " << sceneTime << ",\n"
            << "  \"darknessFade\": " << darknessFade << ",\n"
            << "  \"skyColor\": [" << look.sky.r * darknessFade << ", "
                                   << look.sky.g * darknessFade << ", "
                                   << look.sky.b * darknessFade << "],\n"
            << "  \"sunDir\": [" << sunDir.x << ", " << sunDir.y << ", " << sunDir.z << "],\n"
            << "  \"lampPos\": [" << kLampBulbPos.x << ", " << kLampBulbPos.y << ", " << kLampBulbPos.z << "],\n"
            << "  \"ambient\": " << ambient << ",\n"
            << "  \"sunlight\": " << look.sunlight << ",\n"
            << "  \"lampPower\": " << lampPower << ",\n"
            << "  \"vertexCount\": " << verts.size() << ",\n"
            << "  \"vertices\": [\n";

        for (size_t i = 0; i < verts.size(); ++i) {
            const Vertex& v = verts[i];
            out << "    [" << v.position.x << "," << v.position.y << "," << v.position.z << ","
                << v.normal.x << "," << v.normal.y << "," << v.normal.z << ","
                << v.color.r << "," << v.color.g << "," << v.color.b << "," << v.color.a << ","
                << v.emissive << "]" << (i + 1 < verts.size() ? ",\n" : "\n");
        }
        out << "  ]\n}\n";
        ++exported;

        if (exported % 50 == 0) {
            std::cout << "  wrote " << exported << " scene frames (up to f_"
                      << std::setw(4) << std::setfill('0') << frameIdx << ".json)\n";
        }
    }
    std::cout << "Done. Exported " << exported << " 3D scene frames.\n";
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--frames3d") {
        if (argc < 3 || argc > 4) {
            std::cerr << "Usage: bench3d --frames3d <output_prefix> [fps]\n";
            return 1;
        }
        const std::string prefix = argv[2];
        const int fps = (argc == 4) ? std::stoi(argv[3]) : 24;
        return exportFrames3D(prefix, fps) ? 0 : 1;
    }

    int scene = 0;
    std::string capturePath;
    const bool capture = argc > 1 && std::string(argv[1]) == "--capture";
    if (capture) {
        if (argc != 4) {
            std::cerr << "Usage: bench3d --capture <scene 0-7> <output.ppm>\n";
            return 1;
        }
        try {
            scene = std::stoi(argv[2]);
        } catch (const std::exception&) {
            std::cerr << "Scene must be 0..7\n";
            return 1;
        }
        capturePath = argv[3];
    } else if (argc > 1) {
        try {
            scene = std::stoi(argv[1]);
        } catch (const std::exception&) {
            std::cerr << "Usage: bench3d [scene 0-7] | --frames3d <prefix> [fps]\n";
            return 1;
        }
    }
    if (scene < 0 || scene > 7) {
        std::cerr << "Scene must be 0..7\n";
        return 1;
    }
    if (!glfwInit()) {
        std::cerr << "GLFW initialization failed\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    if (capture) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1100, 680, "Bench Years 3D", nullptr, nullptr);
    if (!window) {
        std::cerr << "Could not create an OpenGL 3.3 window/context\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!gladLoadGL(glfwGetProcAddress) || !GLAD_GL_VERSION_3_3) {
        std::cerr << "An OpenGL 3.3 core context is required\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    std::cout << "OpenGL " << reinterpret_cast<const char*>(glGetString(GL_VERSION)) << '\n';

    const GLuint program = createProgram();
    if (!program) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, color)));
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, emissive)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
    glDisable(GL_CULL_FACE);

    float sceneTime = 0.f;
    bool paused = false, spaceWasDown = false;
    std::vector<Vertex> vertices = buildScene3D(scene, sceneTime);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_DYNAMIC_DRAW);
    updateWindowTitle(window, scene);

    float yaw = 0.0f, pitch = 0.09f, distance = 17.5f;
    if (capture) {
        renderFrame(window, program, vao, vertices.size(), scene, sceneTime, yaw, pitch, distance);
        glFinish();
        const bool saved = savePpm(window, scene, capturePath);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
        glDeleteProgram(program);
        glfwDestroyWindow(window);
        glfwTerminate();
        return saved ? 0 : 1;
    }

    bool leftWasDown = false, rightWasDown = false, screenshotWasDown = false;
    double previousTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const float dt = float(std::min(now - previousTime, .05));
        previousTime = now;
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        const bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (space && !spaceWasDown) paused = !paused;
        spaceWasDown = space;

        const bool left = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
        const bool right = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
        bool sceneChanged = false;
        if (left && !leftWasDown) { scene = (scene + 7) % 8; sceneChanged = true; }
        if (right && !rightWasDown) { scene = (scene + 1) % 8; sceneChanged = true; }
        leftWasDown = left;
        rightWasDown = right;

        if (sceneChanged) {
            sceneTime = 0.f;
            updateWindowTitle(window, scene);
        } else if (!paused) {
            constexpr float kViewerSceneLen[8] = { 11.f, 18.f, 18.f, 1.f, 13.f, 14.f, 14.5f, 16.f };
            sceneTime = std::fmod(sceneTime + dt, kViewerSceneLen[scene]);
        }

        if (sceneChanged || !paused) {
            vertices = buildScene3D(scene, sceneTime);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size() * sizeof(Vertex)),
                         vertices.data(), GL_DYNAMIC_DRAW);
        }

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) yaw -= 1.1f * dt;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) yaw += 1.1f * dt;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) pitch = std::min(.9f, pitch + .8f * dt);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) pitch = std::max(-.12f, pitch - .8f * dt);
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) distance = std::min(28.f, distance + 7.f * dt);
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) distance = std::max(6.f, distance - 7.f * dt);

        const bool screenshot = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
        const bool takeScreenshot = screenshot && !screenshotWasDown;
        screenshotWasDown = screenshot;

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width == 0 || height == 0) continue;
        renderFrame(window, program, vao, vertices.size(), scene, sceneTime, yaw, pitch, distance);
        if (takeScreenshot) {
            glFinish();
            savePpm(window, scene, {});
        }
        glfwSwapBuffers(window);
    }

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
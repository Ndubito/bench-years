#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "scene_params.h"
#include "shape.h"

std::vector<Shape> buildScene(int scene, float t);

namespace {

struct Vec3 {
    float x, y, z;
};

struct Color {
    float r, g, b;
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
        0.30f  // Scene 7: late dry/winter sparse branches
    };
    const float stageBoost = kSceneCanopyBoost[size_t(std::clamp(scene, 0, 7))];
    const float effectiveDensity = std::clamp(std::max(look.leafDensity, 0.25f) * stageBoost, 0.f, 1.45f);
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

void addLamp(Mesh& mesh, const SceneLook& look) {
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
    const bool lit = look.lampOn || look.lampPower > 0.f;
    mesh.sphere(kLampBulbPos, .12f,
                lit ? rgb(255,242,185) : rgb(185,188,183), 10, 6,
                lit ? 1.0f : 0.0f);
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
// ---------- 2D Shape -> 3D Mesh bridge for scene.cpp characters (z < 4 or z > 6.6) ----------
void addSceneFigures3D(Mesh& mesh, int scene, float t) {
    const std::vector<Shape> shapes = ::buildScene(scene, t);

    auto mapX = [](float x) { return (x - 320.f) * 0.023f; };
    auto mapY = [](float y, float z) { 
        if (z > 8.35f) {
            return std::max(0.025f, 0.03f + (328.f - y) * 0.025f);
        }
        return std::max(0.01f, 0.93f + (266.f - y) * 0.025f); 
    };
    auto mapZ = [](float z) {
        if (z < 4.5f) return -1.35f + (z - 3.4f) * 0.30f; // behind the bench
        if (z > 8.35f) return 2.85f + (z - 8.4f) * 0.35f; // foreground path runners
        return 0.36f + (z - 6.8f) * 0.14f;                // seated on/in front of bench
    };
    auto toColor = [](uint32_t rgba) -> Color {
        return {float(rgba & 0xFFu) / 255.f,
                float((rgba >> 8) & 0xFFu) / 255.f,
                float((rgba >> 16) & 0xFFu) / 255.f};
    };

    for (const Shape& s : shapes) {
        const bool isBgFigure = (s.z >= 3.4f && s.z <= 3.95f);
        const bool isFgFigure = (s.z >= 6.75f);
        if (!isBgFigure && !isFgFigure) continue;

        const uint8_t alpha = uint8_t((s.rgba >> 24) & 0xFFu);
        if (alpha < 20u) continue;

        const Color col = toColor(s.rgba);
        const float zc = mapZ(s.z);

        if (alpha < 100u && s.kind == Kind::Poly && s.pts.size() >= 8) {
            float minX = 1e9f, maxX = -1e9f, sumY = 0.f;
            for (const Vec2& pt : s.pts) {
                const Vec2 p = s.xf.apply(pt);
                minX = std::min(minX, p.x);
                maxX = std::max(maxX, p.x);
                sumY += p.y;
            }
            const float cx = mapX((minX + maxX) * 0.5f);
            const float rx = std::max(0.12f, (maxX - minX) * 0.5f * 0.023f);
            const float cy = std::max(0.008f, mapY(sumY / float(s.pts.size()), s.z));
            mesh.cylinder({cx, cy, zc}, {cx, cy + 0.006f, zc}, rx, rx, col, 14);
            continue;
        }

        if (s.kind == Kind::Disc && !s.pts.empty()) {
            const Vec2 p = s.xf.apply(s.pts[0]);
            const float r3 = std::max(0.012f, s.r * 0.0235f);
            // Use spheres for heads/joints (r >= 2.5px) and thin discs for eyes/sparkles
            if (s.r >= 2.5f) {
                mesh.sphere({mapX(p.x), mapY(p.y, s.z), zc}, r3, col, 14, 10);
            } else {
                mesh.cylinder({mapX(p.x), mapY(p.y, s.z), zc - 0.02f},
                              {mapX(p.x), mapY(p.y, s.z), zc + 0.02f}, r3, r3, col, 10);
            }
        } else if ((s.kind == Kind::Strip || s.kind == Kind::Line) && s.pts.size() >= 2) {
            for (size_t i = 0; i + 1 < s.pts.size(); ++i) {
                const Vec2 a = s.xf.apply(s.pts[i]);
                const Vec2 b = s.xf.apply(s.pts[i + 1]);
                const float ra = std::max(0.012f, (s.kind == Kind::Line ? 1.2f : s.w0) * 0.0115f);
                const float rb = std::max(0.012f, (s.kind == Kind::Line ? 1.2f : s.w1) * 0.0115f);
                mesh.cylinder({mapX(a.x), mapY(a.y, s.z), zc},
                              {mapX(b.x), mapY(b.y, s.z), zc}, ra, rb, col, 10);
            }
        } else if (s.kind == Kind::Poly && s.pts.size() >= 3) {
            std::vector<Vec3> front, back;
            front.reserve(s.pts.size());
            back.reserve(s.pts.size());
            const float halfDepth = 0.085f;
            float area2 = 0.f;
            for (size_t i = 0; i < s.pts.size(); ++i) {
                const Vec2 p0 = s.xf.apply(s.pts[i]);
                const Vec2 p1 = s.xf.apply(s.pts[(i + 1) % s.pts.size()]);
                const float x0 = mapX(p0.x), y0 = mapY(p0.y, s.z);
                const float x1 = mapX(p1.x), y1 = mapY(p1.y, s.z);
                area2 += (x0 * y1 - x1 * y0);
                front.push_back({x0, y0, zc + halfDepth});
                back.push_back({x0, y0, zc - halfDepth});
            }
            // Fan-triangulate front (+Z) and back (-Z) faces
            for (size_t i = 1; i + 1 < front.size(); ++i) {
                mesh.triangle(front[0], front[i], front[i + 1], col, {0.f, 0.f, 1.f});
                mesh.triangle(back[0], back[i + 1], back[i], col, {0.f, 0.f, -1.f});
            }
            // Extrude perimeter side walls for solid 3D volume
            const float sign = area2 >= 0.f ? 1.f : -1.f;
            for (size_t i = 0; i < front.size(); ++i) {
                const size_t j = (i + 1) % front.size();
                const Vec3 edge = front[j] - front[i];
                const Vec3 sideNormal{edge.y * sign, -edge.x * sign, 0.f};
                mesh.quad(front[i], front[j], back[j], back[i], col, sideNormal);
            }
        }
    }
}

std::vector<Vertex> buildScene3D(int scene, float t = 2.5f) {
    const SceneLook& look = kScenes[size_t(scene)];
    Mesh mesh;
    addSky(mesh, look);
    addHills(mesh, look);
    addGrass(mesh, look, scene);
    addPath(mesh, look.ground);
    addTree(mesh, look, scene);
    addLamp(mesh, look);
    addBench(mesh, look);
    addSceneFigures3D(mesh, scene, t);
    // Emissive = 1.0 keeps the sun glowing across dusk and night scenes
    mesh.sphere(sunPositionFor(scene), .38f, look.sun, 48, 32, 1.0f);
    return std::move(mesh.vertices);
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
                 int scene, float yaw, float pitch, float distance) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    const Color sky = kScenes[size_t(scene)].sky;
    glClearColor(sky.r, sky.g, sky.b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    const Vec3 target{0.f, 1.35f, -.8f};
    const Vec3 eye{target.x + std::sin(yaw)*distance*std::cos(pitch),
                   target.y + std::sin(pitch)*distance,
                   target.z + std::cos(yaw)*distance*std::cos(pitch)};
    const Mat4 view = lookAt(eye, target, {0,1,0});
    const Mat4 projection = perspective(.785398f, float(width)/float(height), .1f, 80.f);
    const Mat4 mvp = multiply(projection, view);
    glUniformMatrix4fv(glGetUniformLocation(program, "uMVP"), 1, GL_FALSE, mvp.data());

    const SceneLook& look = kScenes[size_t(scene)];
    const Vec3 sunDir = normalized(sunPositionFor(scene) - target + Vec3{0.f, 0.f, 9.5f});
    glUniform3f(glGetUniformLocation(program, "uSunDirection"), sunDir.x, sunDir.y, sunDir.z);
    glUniform3f(glGetUniformLocation(program, "uLampPosition"),
                kLampBulbPos.x, kLampBulbPos.y, kLampBulbPos.z);
    glUniform1f(glGetUniformLocation(program, "uAmbient"), look.ambient);
    glUniform1f(glGetUniformLocation(program, "uSunlight"), look.sunlight);
    glUniform1f(glGetUniformLocation(program, "uLampPower"),
                (look.lampOn || look.lampPower > 0.f) ? look.lampPower : 0.f);
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
        layout (location = 2) in vec3 aColor;
        layout (location = 3) in float aEmissive;
        uniform mat4 uMVP;
        out vec3 vPosition;
        out vec3 vNormal;
        out vec3 vColor;
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
        in vec3 vColor;
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
            vec3 lit = vColor * (vec3(uAmbient + sun) + vec3(1.0, 0.74, 0.42) * lamp);
            vec3 finalColor = mix(lit, vColor, clamp(vEmissive, 0.0, 1.0));
            FragColor = vec4(clamp(finalColor, 0.0, 1.0), 1.0);
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

} // namespace

int main(int argc, char** argv) {
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
            std::cerr << "Usage: bench3d [scene 0-7]\n";
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
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, color)));
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, emissive)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glDisable(GL_CULL_FACE);

    float sceneTime = 0.f;
    bool paused = false, spaceWasDown = false;
    std::vector<Vertex> vertices = buildScene3D(scene, sceneTime);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_DYNAMIC_DRAW);
    updateWindowTitle(window, scene);

    float yaw = .1f, pitch = .12f, distance = 17.5f;
    if (capture) {
        renderFrame(window, program, vao, vertices.size(), scene, yaw, pitch, distance);
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
        

        if (left && !leftWasDown) { scene = (scene + 7) % 8; sceneChanged = true; }
        if (right && !rightWasDown) { scene = (scene + 1) % 8; sceneChanged = true; }
        leftWasDown = left;
        rightWasDown = right;

        if (sceneChanged) {
            sceneTime = 0.f;
            updateWindowTitle(window, scene);
        } else if (!paused) {
            sceneTime = std::fmod(sceneTime + dt, 6.f);
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
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) distance = std::max(9.f, distance - 7.f * dt);

        const bool screenshot = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
        const bool takeScreenshot = screenshot && !screenshotWasDown;
        screenshotWasDown = screenshot;

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width == 0 || height == 0) continue;
        renderFrame(window, program, vao, vertices.size(), scene, yaw, pitch, distance);
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
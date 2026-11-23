// Owner: A. Timeline and the leaf-gust transition between scenes.
#include "film.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "raster.h"
#include "render.h"
#include "shape.h"

namespace {

// Scene and gust lengths in seconds, taken from the JavaScript reference (story.js: D and TD).

const float kSceneLen[8] = { 11.f, 11.5f, 12.f, 12.f, 13.f, 14.f, 14.5f, 16.f };
const float kGustLen[7]  = { 2.8f, 2.8f, 3.0f, 3.0f, 3.2f, 3.4f, 3.8f };

// Leaf colours for the gust leaving each scene: dark, mid, light. Green early, autumn late.
const uint32_t kLeaf[7][3] = {
    { rgb(79, 154, 74),  rgb(111, 189, 91), rgb(155, 224, 124) },
    { rgb(70, 146, 63),  rgb(105, 185, 85), rgb(152, 220, 120) },
    { rgb(63, 138, 58),  rgb(98, 178, 78),  rgb(146, 214, 114) },
    { rgb(63, 138, 58),  rgb(98, 178, 78),  rgb(146, 214, 114) },
    { rgb(108, 143, 47), rgb(163, 169, 58), rgb(215, 193, 74) },
    { rgb(180, 86, 31),  rgb(223, 138, 46), rgb(242, 184, 75) },
    { rgb(138, 61, 28),  rgb(181, 86, 31),  rgb(217, 138, 58) },
};

float smoothstep(float t) {
    t = std::min(1.f, std::max(0.f, t));
    return t * t * (3.f - 2.f * t);
}

// Repeatable pseudo-random number in [0, 1) from two integers. Same input, same output,
// so every render of a frame is identical.
float hash2(int x, int y) {
    uint32_t h = uint32_t(x) * 374761393u ^ uint32_t(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return float((h ^ (h >> 16)) & 0xFFFFFFu) / float(0x1000000);
}

uint32_t darken(uint32_t c, float k) {   // k = 0 keeps the colour, 1 is black
    uint32_t r = uint32_t((c & 255) * (1.f - k));
    uint32_t g = uint32_t(((c >> 8) & 255) * (1.f - k));
    uint32_t b = uint32_t(((c >> 16) & 255) * (1.f - k));
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}

void renderScene(Framebuffer& fb, int scene, float t) {
    fb.clear(0xFF000000u);
    renderShapes(fb, buildScene(scene, t));
}

// One leaf: a six-point polygon around (0, 0), turned and moved by its transform.
Shape leafShape(float x, float y, float angle, float size, uint32_t colour) {
    Shape s;
    s.kind = Kind::Poly;
    s.pts = { {-1.f, 0.f}, {-0.45f, -0.55f}, {0.5f, -0.5f}, {1.f, 0.f}, {0.5f, 0.5f}, {-0.45f, 0.55f} };
    s.xf = Mat3::translate(x, y) * Mat3::rotate(angle) * Mat3::scale(size, size);
    s.rgba = colour;
    s.z = 0;
    return s;
}

// The gust: a wavy edge sweeps left to right. Left of the edge is the new scene,
// right of it the old one. A dithered band of leaf colour hides the edge, and
// loose leaves tumble along it.
void gust(Framebuffer& out, const Framebuffer& oldF, const Framebuffer& newF, int from, float u) {
    const int W = out.w, H = out.h;
    const float edge = -200.f + (W + 400.f) * smoothstep(u);
    const float band = 54.f;
    const uint32_t* leaf = kLeaf[from];

    for (int y = 0; y < H; y++) {
        float ex = edge + 26.f * std::sin(y * 0.035f + u * 7.f) + 12.f * std::sin(y * 0.09f - u * 11.f);
        int cut = std::min(W, std::max(0, int(std::lround(ex))));
        size_t row = size_t(y) * W;
        std::copy(newF.px.begin() + row, newF.px.begin() + row + cut, out.px.begin() + row);
        std::copy(oldF.px.begin() + row + cut, oldF.px.begin() + row + W, out.px.begin() + row + cut);

        int x0 = std::max(0, int(ex - band)), x1 = std::min(W - 1, int(ex + band));
        for (int x = x0; x <= x1; x++) {
            float cover = 1.f - std::fabs(x - ex) / band;         // 1 on the edge, 0 at the band's sides
            if (cover > 0.55f || hash2(x >> 1, y >> 1) < cover * 1.3f) {
                uint32_t c = leaf[int(hash2(x >> 2, (y >> 2) + 7) * 3.f) % 3];
                out.px[row + x] = darken(c, 0.1f + 0.35f * hash2(x, y));
            }
        }
    }

    std::vector<Shape> leaves;
    for (int k = 0; k < 230; k++) {
        bool giant = k >= 200;
        float ox = -0.95f + 1.35f * hash2(k, 1), oy = hash2(k, 2), ph = 6.283f * hash2(k, 3);
        float sp = hash2(k, 4), rot = 6.283f * hash2(k, 5);
        float size = giant ? 20.f + 14.f * hash2(k, 6) : 3.5f + 5.f * hash2(k, 6);
        float spread = giant ? 190.f : 120.f;
        float x = edge + ox * spread + 34.f * std::sin(u * 9.f + ph);
        float y = std::fmod(oy * (H + 100.f) + u * (90.f + 70.f * sp), H + 100.f) - 50.f + 24.f * std::sin(u * 11.f + ph * 2.f);
        uint32_t c = leaf[k % 3];
        Shape s = leafShape(x, y, rot + u * (8.f + 8.f * sp), size, giant ? darken(c, 0.22f) : c);
        s.z = giant ? 1.f : 0.f;                                   // big leaves in front
        leaves.push_back(s);
    }
    renderShapes(out, leaves);
}

} // namespace

float filmLength() {
    float total = 0.f;
    for (float d : kSceneLen) total += d;
    for (float d : kGustLen) total += d;
    return total;
}

float sceneStart(int scene) {
    float T = 0.f;
    for (int i = 0; i < scene && i < 7; i++) T += kSceneLen[i] + kGustLen[i];
    return T;
}

FilmTime filmAt(float T) {
    FilmTime f = { false, 7, kSceneLen[7], 0.f };     // past the end: hold the last frame
    if (T < 0.f) T = 0.f;
    float start = 0.f;
    for (int i = 0; i < 8; i++) {
        if (T < start + kSceneLen[i]) { f.scene = i; f.t = T - start; return f; }
        start += kSceneLen[i];
        if (i == 7) break;
        if (T < start + kGustLen[i]) {
            f.gust = true; f.scene = i;
            f.t = kSceneLen[i] + (T - start);               // the old scene keeps playing under the gust
            f.u = (T - start) / kGustLen[i];
            return f;
        }
        start += kGustLen[i];
    }
    return f;
}

void renderFilm(Framebuffer& out, float T) {
    FilmTime f = filmAt(T);
    if (!f.gust) { renderScene(out, f.scene, f.t); return; }
    Framebuffer oldF(out.w, out.h), newF(out.w, out.h);
    renderScene(oldF, f.scene, f.t);
    renderScene(newF, f.scene + 1, 0.f);
    gust(out, oldF, newF, f.scene, f.u);
}

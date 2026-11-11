// Owner: A. Each image is built to expose the usual bugs in that algorithm.
#include "algo_demo.h"
#include <cmath>
#include "raster.h"

static const uint32_t BG = rgb(16, 20, 32), FG = rgb(240, 240, 240), AC = rgb(255, 196, 64), CY = rgb(80, 200, 170);
static const float kPi = 3.14159265f;

// 72 rays from one centre cover every octant and both drawing directions.
static void lineImage(Framebuffer& fb) {
    const int cx = fb.w / 2, cy = fb.h / 2;
    for (int i = 0; i < 72; i++) {
        float a = i * 2 * kPi / 72;
        drawLine(fb, cx, cy, cx + int(300 * std::cos(a)), cy + int(170 * std::sin(a)), i % 2 ? FG : CY);
    }
    drawLine(fb, 10, 10, 630, 10, AC);          // horizontal
    drawLine(fb, 10, 20, 10, 350, AC);          // vertical
    drawLine(fb, 630, 350, 20, 340, AC);        // shallow, right to left
}

// Concentric outlines, a filled disc, and one circle that runs off the corner (clipping).
static void circleImage(Framebuffer& fb) {
    for (int r = 15; r <= 165; r += 15) drawCircle(fb, 320, 180, r, (r / 15) % 2 ? FG : CY);
    fillDisc(fb, 320, 180, 10, AC);
    drawCircle(fb, 40, 40, 70, AC);
    fillDisc(fb, 580, 300, 45, CY);
    fillDisc(fb, 580, 300, 1, BG);
}

// Pentagram (even-odd leaves the centre empty), a concave L, a thin triangle.
static void fillImage(Framebuffer& fb) {
    std::vector<Vec2> v(5);
    for (int i = 0; i < 5; i++) {
        float a = -kPi / 2 + i * 2 * kPi / 5;
        v[i] = { 170 + 150 * std::cos(a), 190 + 150 * std::sin(a) };
    }
    fillPoly(fb, { v[0], v[2], v[4], v[1], v[3] }, AC);
    fillPoly(fb, { {360, 40}, {430, 40}, {430, 230}, {600, 230}, {600, 300}, {360, 300} }, CY);
    fillPoly(fb, { {470, 60}, {620, 80}, {480, 66} }, FG);
}

// Two curves with their control polygons: an arch and an S.
static void bezierImage(Framebuffer& fb) {
    auto draw = [&](Vec2 a, Vec2 b, Vec2 c, Vec2 d, uint32_t col) {
        Vec2 p[4] = { a, b, c, d };
        for (int i = 0; i < 3; i++) drawLine(fb, int(p[i].x), int(p[i].y), int(p[i + 1].x), int(p[i + 1].y), rgb(70, 80, 110));
        auto pts = bezier(a, b, c, d, 60);
        for (size_t i = 0; i + 1 < pts.size(); i++)
            drawLine(fb, int(pts[i].x), int(pts[i].y), int(pts[i + 1].x), int(pts[i + 1].y), col);
        for (auto& q : p) fillDisc(fb, int(q.x), int(q.y), 4, FG);
    };
    draw({ 40, 320 }, { 140, 20 }, { 500, 20 }, { 600, 320 }, AC);
    draw({ 60, 60 }, { 420, 60 }, { 220, 300 }, { 580, 300 }, CY);
}

void renderAlgoImage(Framebuffer& fb, const std::string& which) {
    fb.clear(BG);
    if (which == "line") lineImage(fb);
    else if (which == "circle") circleImage(fb);
    else if (which == "fill") fillImage(fb);
    else if (which == "bezier") bezierImage(fb);
}

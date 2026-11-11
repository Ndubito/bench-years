// Owner: B.  PLACEHOLDER IMPLEMENTATIONS.
// These are deliberately naive so the whole pipeline runs on day one.
// They are NOT the algorithms the course asks for. Replace each body with the
// real one: Bresenham, midpoint circle, scanline fill, Bezier.
// The program prints a warning while any placeholder is in use.
#include "raster.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

static void warn() {
    static bool shown = false;
    if (!shown) { std::fprintf(stderr, "WARNING: placeholder rasterizer in use (src/raster.cpp)\n"); shown = true; }
}

void drawLine(Framebuffer& fb, int x0, int y0, int x1, int y1, uint32_t c) {
    warn();   // TODO (B): Bresenham, integer only
    int n = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int i = 0; i <= n; i++) {
        float t = n ? float(i) / n : 0;
        fb.put(int(std::lround(x0 + (x1 - x0) * t)), int(std::lround(y0 + (y1 - y0) * t)), c);
    }
}

void drawCircle(Framebuffer& fb, int cx, int cy, int r, uint32_t c) {
    warn();   // TODO (B): midpoint circle, eight-way symmetry
    for (int a = 0; a < 720; a++) {
        float t = a * 3.14159265f / 360.f;
        fb.put(cx + int(std::lround(r * std::cos(t))), cy + int(std::lround(r * std::sin(t))), c);
    }
}

void fillDisc(Framebuffer& fb, int cx, int cy, int r, uint32_t c) {
    warn();   // TODO (B): midpoint circle driving horizontal spans
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) fb.put(cx + x, cy + y, c);
}

void fillPoly(Framebuffer& fb, const std::vector<Vec2>& p, uint32_t c) {
    warn();   // TODO (B): scanline fill with an edge table, even-odd rule
    if (p.size() < 3) return;
    float x0 = p[0].x, x1 = p[0].x, y0 = p[0].y, y1 = p[0].y;
    for (auto& v : p) { x0 = std::min(x0, v.x); x1 = std::max(x1, v.x); y0 = std::min(y0, v.y); y1 = std::max(y1, v.y); }
    for (int y = int(y0); y <= int(y1); y++)
        for (int x = int(x0); x <= int(x1); x++) {
            bool in = false;
            for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) {
                if ((p[i].y > y + .5f) != (p[j].y > y + .5f) &&
                    x + .5f < (p[j].x - p[i].x) * (y + .5f - p[i].y) / (p[j].y - p[i].y) + p[i].x)
                    in = !in;
            }
            if (in) fb.put(x, y, c);
        }
}

std::vector<Vec2> bezier(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, int n) {
    // TODO (B): replace with the De Casteljau or Bernstein form you write yourself
    std::vector<Vec2> out;
    for (int i = 0; i <= n; i++) {
        float t = float(i) / n, u = 1 - t;
        float a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
        out.push_back({ a * p0.x + b * p1.x + c * p2.x + d * p3.x, a * p0.y + b * p1.y + c * p2.y + d * p3.y });
    }
    return out;
}

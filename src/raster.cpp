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

      //  Bresenham, integer only
      int dx = std::abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;

    int dy = -std::abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx + dy;

    while (true) {

        // Colour the current pixel
        fb.put(x0, y0, c);

        // Stop when we reach the endpoint
        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }


}

void drawCircle(Framebuffer& fb, int cx, int cy, int r, uint32_t c) {
      //  midpoint circle, eight-way symmetry
   if (r < 0) return;

    int x = 0;
    int y = r;
    int p = 1 - r;

    while (x <= y) {

        // Plot all eight symmetrical points
        fb.put(cx + x, cy + y, c);
        fb.put(cx - x, cy + y, c);
        fb.put(cx + x, cy - y, c);
        fb.put(cx - x, cy - y, c);

        fb.put(cx + y, cy + x, c);
        fb.put(cx - y, cy + x, c);
        fb.put(cx + y, cy - x, c);
        fb.put(cx - y, cy - x, c);

        x++;

        if (p < 0) {
            p += 2 * x + 1;
        }
        else {
            y--;
            p += 2 * (x - y) + 1;
        }
    }
}

void fillDisc(Framebuffer& fb, int cx, int cy, int r, uint32_t c) {
      // midpoint circle driving horizontal spans
    if (r < 0) return;

    int x = 0;
    int y = r;
    int p = 1 - r;

    while (x <= y) {

        // Draw horizontal spans on all eight symmetric sections
        for (int i = cx - x; i <= cx + x; i++) {
            fb.put(i, cy + y, c);
            fb.put(i, cy - y, c);
        }

        for (int i = cx - y; i <= cx + y; i++) {
            fb.put(i, cy + x, c);
            fb.put(i, cy - x, c);
        }

        x++;

        if (p < 0) {
            p += 2 * x + 1;
        }
        else {
            y--;
            p += 2 * (x - y) + 1;
        }
    }

}

void fillPoly(Framebuffer& fb, const std::vector<Vec2>& p, uint32_t c) {
       // scanline fill with an edge table, even-odd rule
    if (p.size() < 3) return;

    // Find the vertical boundaries of the polygon
    float minY = p[0].y;
    float maxY = p[0].y;

    for (const auto& v : p) {
        minY = std::min(minY, v.y);
        maxY = std::max(maxY, v.y);
    }

    // Process each horizontal scanline
    int startY = std::max(0, int(std::ceil(minY - 0.5f)));
    int endY = std::min(fb.h - 1, int(std::floor(maxY - 0.5f)));

    for (int y = startY; y <= endY; y++) {

        float scanY = y + 0.5f;
        std::vector<float> intersections;

        // Find where the scanline intersects polygon edges
        for (size_t i = 0; i < p.size(); i++) {

            const Vec2& a = p[i];
            const Vec2& b = p[(i + 1) % p.size()];

            // Ignore horizontal edges
            if (a.y == b.y) continue;

            // Use a half-open interval to avoid counting vertices twice
            if (scanY >= std::min(a.y, b.y) &&
                scanY < std::max(a.y, b.y)) {

                float x = a.x + (scanY - a.y) *
                          (b.x - a.x) / (b.y - a.y);

                intersections.push_back(x);
            }
        }

        // Sort intersections from left to right
        std::sort(intersections.begin(), intersections.end());

        // Fill between pairs of intersections
        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {

            int startX = std::max(0, int(std::ceil(intersections[i] - 0.5f)));
            int endX = std::min(fb.w - 1,
                                int(std::ceil(intersections[i + 1] - 0.5f)) - 1);

            for (int x = startX; x <= endX; x++) {
                fb.put(x, y, c);
            }
        }
    }

}

std::vector<Vec2> bezier(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, int n) {
    // cubic Bezier curve
    std::vector<Vec2> out;

    // Prevent invalid segment counts
    if (n <= 0) {
        out.push_back(p0);
        return out;
    }

    // Calculate points along the curve
    for (int i = 0; i <= n; i++) {

        float t = float(i) / n;
        float u = 1.0f - t;

        // Bernstein polynomial coefficients
        float a = u * u * u;
        float b = 3.0f * u * u * t;
        float c = 3.0f * u * t * t;
        float d = t * t * t;

        // Calculate the x and y coordinates
        float x = a * p0.x + b * p1.x + c * p2.x + d * p3.x;
        float y = a * p0.y + b * p1.y + c * p2.y + d * p3.y;

        out.push_back({x, y});
    }

    return out;
}

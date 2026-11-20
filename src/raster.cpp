// Owner: B. Raster algorithms.
#include "raster.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

constexpr int kCoverageSamples = 4;

void accumulateSpan(std::vector<float>& coverage, int width, float left, float right) {
    if (right <= left || right <= 0.f || left >= float(width)) return;
    int startX = std::max(0, int(std::floor(left)));
    int endX = std::min(width - 1, int(std::ceil(right)) - 1);
    for (int x = startX; x <= endX; ++x) {
        float overlap = std::min(right, float(x + 1)) - std::max(left, float(x));
        if (overlap > 0.f) coverage[size_t(x)] += overlap / kCoverageSamples;
    }
}

void putCovered(Framebuffer& fb, int x, int y, uint32_t c, float coverage) {
    uint32_t sourceAlpha = (c >> 24) & 255u;
    if (coverage >= 1.f) {
        if (sourceAlpha == 255u) fb.put(x, y, c);
        else if (sourceAlpha > 0) fb.putAlpha(x, y, c);
        return;
    }
    uint32_t alpha = uint32_t(float(sourceAlpha) * coverage + 0.5f);
    if (alpha > 0)
        fb.putAlpha(x, y, (c & 0x00FFFFFFu) | (alpha << 24));
}

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
    if (r < 0) return;
    if (r == 0) {
        fb.put(cx, cy, c);
        return;
    }

    int startY = std::max(0, int(std::floor(float(cy - r))));
    int endY = std::min(fb.h - 1, int(std::ceil(float(cy + r))) - 1);
    std::vector<float> coverage(size_t(fb.w), 0.f);
    const float radiusSquared = float(r) * r;
    for (int y = startY; y <= endY; ++y) {
        std::fill(coverage.begin(), coverage.end(), 0.f);
        for (int sample = 0; sample < kCoverageSamples; ++sample) {
            float dy = float(y) + (float(sample) + 0.5f) / kCoverageSamples - cy;
            float remaining = radiusSquared - dy * dy;
            if (remaining <= 0.f) continue;
            float halfWidth = std::sqrt(remaining);
            accumulateSpan(coverage, fb.w, float(cx) - halfWidth, float(cx) + halfWidth);
        }
        for (int x = 0; x < fb.w; ++x)
            if (coverage[size_t(x)] > 0.f) putCovered(fb, x, y, c, coverage[size_t(x)]);
    }
}

void fillPoly(Framebuffer& fb, const std::vector<Vec2>& p, uint32_t c) {
    if (p.size() < 3) return;

    float minY = p[0].y;
    float maxY = p[0].y;
    for (const auto& v : p) {
        minY = std::min(minY, v.y);
        maxY = std::max(maxY, v.y);
    }

    int startY = std::max(0, int(std::floor(minY)));
    int endY = std::min(fb.h - 1, int(std::ceil(maxY)) - 1);
    std::vector<float> coverage(size_t(fb.w), 0.f);
    std::vector<float> intersections;
    intersections.reserve(p.size());
    for (int y = startY; y <= endY; y++) {
        std::fill(coverage.begin(), coverage.end(), 0.f);
        for (int sample = 0; sample < kCoverageSamples; ++sample) {
            float scanY = float(y) + (float(sample) + 0.5f) / kCoverageSamples;
            intersections.clear();
            for (size_t i = 0; i < p.size(); i++) {
                const Vec2& a = p[i];
                const Vec2& b = p[(i + 1) % p.size()];
                if (a.y == b.y) continue;
                if (scanY >= std::min(a.y, b.y) &&
                    scanY < std::max(a.y, b.y)) {
                    intersections.push_back(a.x + (scanY - a.y) *
                                            (b.x - a.x) / (b.y - a.y));
                }
            }

            std::sort(intersections.begin(), intersections.end());
            for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
                accumulateSpan(coverage, fb.w, intersections[i], intersections[i + 1]);
            }
        }

        for (int x = 0; x < fb.w; ++x)
            if (coverage[size_t(x)] > 0.f) putCovered(fb, x, y, c, coverage[size_t(x)]);
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

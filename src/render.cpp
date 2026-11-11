// Owner: A. Turns a shape list into pixels using only the functions in raster.h.
#include "render.h"
#include <algorithm>
#include <cmath>
#include "raster.h"

static std::vector<Vec2> world(const Shape& s) {
    std::vector<Vec2> o;
    o.reserve(s.pts.size());
    for (auto p : s.pts) o.push_back(s.xf.apply(p));
    return o;
}

void renderShapes(Framebuffer& fb, std::vector<Shape> shapes) {
    std::stable_sort(shapes.begin(), shapes.end(), [](const Shape& a, const Shape& b) { return a.z < b.z; });
    for (const Shape& s : shapes) {
        auto p = world(s);
        if (p.empty()) continue;
        switch (s.kind) {
        case Kind::Poly:
            fillPoly(fb, p, s.rgba);
            break;
        case Kind::Disc: {
            float k = std::hypot(s.xf.m[0], s.xf.m[3]);       // uniform scale assumed
            fillDisc(fb, int(std::lround(p[0].x)), int(std::lround(p[0].y)), int(std::lround(s.r * k)), s.rgba);
            break;
        }
        case Kind::Line:
            for (size_t i = 0; i + 1 < p.size(); i++)
                drawLine(fb, int(std::lround(p[i].x)), int(std::lround(p[i].y)),
                         int(std::lround(p[i + 1].x)), int(std::lround(p[i + 1].y)), s.rgba);
            break;
        case Kind::Strip:
            for (size_t i = 0; i + 1 < p.size(); i++) {
                float t0 = float(i) / (p.size() - 1), t1 = float(i + 1) / (p.size() - 1);
                float a = (s.w0 + (s.w1 - s.w0) * t0) / 2, b = (s.w0 + (s.w1 - s.w0) * t1) / 2;
                float dx = p[i + 1].x - p[i].x, dy = p[i + 1].y - p[i].y, l = std::hypot(dx, dy);
                if (l < 1e-4f) continue;
                float nx = -dy / l, ny = dx / l;
                fillPoly(fb, { { p[i].x + nx * a, p[i].y + ny * a }, { p[i + 1].x + nx * b, p[i + 1].y + ny * b },
                               { p[i + 1].x - nx * b, p[i + 1].y - ny * b }, { p[i].x - nx * a, p[i].y - ny * a } }, s.rgba);
            }
            break;
        }
    }
}

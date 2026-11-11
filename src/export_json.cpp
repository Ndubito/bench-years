// Owner: E. v0: one frame, points already transformed to pixel space.
#include "export_json.h"
#include <cstdio>

static const char* kindName(Kind k) {
    switch (k) { case Kind::Poly: return "poly"; case Kind::Disc: return "disc"; case Kind::Strip: return "strip"; default: return "line"; }
}

bool writeFrameJSON(const std::string& path, const std::vector<Shape>& shapes, int scene, int frame, float t) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    std::fprintf(f, "{\"width\":%d,\"height\":%d,\"scene\":%d,\"frame\":%d,\"t\":%.4f,\"shapes\":[\n", kWidth, kHeight, scene, frame, t);
    for (size_t i = 0; i < shapes.size(); i++) {
        const Shape& s = shapes[i];
        std::fprintf(f, "{\"kind\":\"%s\",\"z\":%.3f,\"rgba\":%u,\"r\":%.3f,\"w0\":%.3f,\"w1\":%.3f,\"pts\":[",
                     kindName(s.kind), s.z, s.rgba, s.r, s.w0, s.w1);
        for (size_t j = 0; j < s.pts.size(); j++) {
            Vec2 p = s.xf.apply(s.pts[j]);
            std::fprintf(f, "%s[%.3f,%.3f]", j ? "," : "", p.x, p.y);
        }
        std::fprintf(f, "]}%s\n", i + 1 < shapes.size() ? "," : "");
    }
    std::fprintf(f, "]}\n");
    std::fclose(f);
    return true;
}

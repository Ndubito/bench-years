// Owner: D. PLACEHOLDER SCENE so the pipeline shows something on day one.
// D replaces buildScene with the real eight scenes. Keep the signature.
#include "shape.h"

int sceneCount() { return 8; }

static Shape poly(std::vector<Vec2> pts, uint32_t c, float z) {
    Shape s; s.kind = Kind::Poly; s.pts = std::move(pts); s.rgba = c; s.z = z; return s;
}

std::vector<Shape> buildScene(int scene, float t) {
    (void)t;
    std::vector<Shape> v;
    float k = scene / 7.f;                                       // 0 = morning, 1 = night
    uint32_t sky = rgb(uint8_t(127 - 110 * k), uint8_t(185 - 160 * k), uint8_t(230 - 150 * k));
    v.push_back(poly({ {0, 0}, {640, 0}, {640, 242}, {0, 242} }, sky, 0));
    v.push_back(poly({ {0, 242}, {640, 242}, {640, 360}, {0, 360} }, rgb(111, 174, 98), 1));
    Shape sun; sun.kind = Kind::Disc; sun.pts = { {120.f + 60 * scene, 90.f} }; sun.r = 15; sun.rgba = rgb(255, 212, 121); sun.z = 2;
    v.push_back(sun);
    v.push_back(poly({ {238, 226}, {402, 226}, {402, 252}, {238, 252} }, rgb(47, 154, 119), 3));      // back slats
    v.push_back(poly({ {230, 266}, {410, 266}, {410, 276}, {230, 276} }, rgb(47, 154, 119), 3));      // seat
    v.push_back(poly({ {244, 276}, {252, 276}, {252, 298}, {244, 298} }, rgb(33, 108, 84), 3));       // legs
    v.push_back(poly({ {388, 276}, {396, 276}, {396, 298}, {388, 298} }, rgb(33, 108, 84), 3));
    return v;
}

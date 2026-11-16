#pragma once
#include <vector>
#include <cmath>
#include <cstdint>

struct Vec2 { float x = 0, y = 0; };

// Row-major 3x3 affine matrix: [a c e; b d f; 0 0 1] stored as m[0..8].
struct Mat3 {
    float m[9] = {1, 0, 0,
                  0, 1, 0,
                  0, 0, 1};
    Vec2 apply(Vec2 p) const {
        return { m[0] * p.x + m[1] * p.y + m[2],
                 m[3] * p.x + m[4] * p.y + m[5] };
    }
    // Composition: (A * B).apply(p) == A.apply(B.apply(p))
    Mat3 operator*(const Mat3& o) const {
        Mat3 r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                r.m[i * 3 + j] = m[i * 3 + 0] * o.m[0 * 3 + j]
                               + m[i * 3 + 1] * o.m[1 * 3 + j]
                               + m[i * 3 + 2] * o.m[2 * 3 + j];
        return r;
    }

    static Mat3 identity() { return Mat3(); }

    static Mat3 translate(float tx, float ty) {
        Mat3 r;
        r.m[2] = tx;
        r.m[5] = ty;
        return r;
    }

    static Mat3 rotate(float radians) {
        Mat3 r;
        float c = std::cos(radians), s = std::sin(radians);
        r.m[0] = c;  r.m[1] = -s;
        r.m[3] = s;  r.m[4] = c;
        return r;
    }

    static Mat3 scale(float sx, float sy) {
        Mat3 r;
        r.m[0] = sx;
        r.m[4] = sy;
        return r;
    }
};

// Hierarchical transforms: push() before a child part, pop() after it.
struct MatrixStack {
    std::vector<Mat3> st{Mat3()};

    const Mat3& top() const { return st.back(); }
    void push() { st.push_back(st.back()); }
    void pop()  { if (st.size() > 1) st.pop_back(); }
    void reset() { st.assign(1, Mat3()); }

    // Each call multiplies onto the current top, so it affects everything drawn after.
    void mul(const Mat3& m)            { st.back() = st.back() * m; }
    void translate(float x, float y)   { mul(Mat3::translate(x, y)); }
    void rotate(float radians)         { mul(Mat3::rotate(radians)); }
    void scale(float sx, float sy)     { mul(Mat3::scale(sx, sy)); }
    Vec2 apply(Vec2 p) const           { return st.back().apply(p); }
};

constexpr uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u | (uint32_t(b) << 16) | (uint32_t(g) << 8) | r;   // 0xAABBGGRR
}

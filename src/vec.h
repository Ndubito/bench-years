#pragma once
// Owner: C (grows Mat3 into a full matrix stack). Interface changes go through A.
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
    // TODO (C): operator*, translate(), rotate(), scale(), MatrixStack
};

constexpr uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u | (uint32_t(b) << 16) | (uint32_t(g) << 8) | r;   // 0xAABBGGRR
}

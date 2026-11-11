#pragma once
// Owner: B
#include <cstdint>
#include <string>
#include <vector>

struct Framebuffer {
    int w, h;
    std::vector<uint32_t> px;
    Framebuffer(int w_, int h_) : w(w_), h(h_), px(size_t(w_) * h_, 0xFF000000u) {}
    void clear(uint32_t c) { px.assign(px.size(), c); }
    void put(int x, int y, uint32_t c) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        px[size_t(y) * w + x] = c;
    }
    bool savePPM(const std::string& path) const;   // binary P6, no external libraries
};

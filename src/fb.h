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


void putAlpha(int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;

    // Get the existing background pixel
    uint32_t bg = px[size_t(y) * w + x];

    // Extract foreground alpha
    unsigned int alpha = (c >> 24) & 255;
    unsigned int invAlpha = 255 - alpha;

    // Extract foreground RGB
    unsigned int fr = c & 255;
    unsigned int fg = (c >> 8) & 255;
    unsigned int fb = (c >> 16) & 255;

    // Extract background RGB
    unsigned int br = bg & 255;
    unsigned int bgc = (bg >> 8) & 255;
    unsigned int bb = (bg >> 16) & 255;

    // Blend the colour channels
    unsigned int r = (fr * alpha + br * invAlpha + 127) / 255;
    unsigned int g = (fg * alpha + bgc * invAlpha + 127) / 255;
    unsigned int b = (fb * alpha + bb * invAlpha + 127) / 255;

    // Store the blended pixel (opaque result)
    px[size_t(y) * w + x] =
        0xFF000000u | (b << 16) | (g << 8) | r;
}

    bool savePPM(const std::string& path) const;   // binary P6, no external libraries
};

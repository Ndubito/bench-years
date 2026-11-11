// Owner: B
#include "fb.h"
#include <cstdio>

bool Framebuffer::savePPM(const std::string& path) const {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (uint32_t c : px) {
        unsigned char rgb[3] = { (unsigned char)(c & 255),
                                 (unsigned char)((c >> 8) & 255),
                                 (unsigned char)((c >> 16) & 255) };
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    return true;
}

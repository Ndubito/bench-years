#pragma once
// Owner: C. Easing and keyframe curves. Header-only, inside namespace anim
// so it cannot clash with helper names already used in scene code.
#include <vector>

namespace anim {

inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

inline float easeIn(float t)    { t = clamp01(t); return t * t; }
inline float easeOut(float t)   { t = clamp01(t); return 1 - (1 - t) * (1 - t); }
inline float easeInOut(float t) { t = clamp01(t); return t * t * (3 - 2 * t); }  // smoothstep

struct Key { float t, v; };

// Keyframes sorted by time. at(t) eases between neighbouring keys and
// holds the first/last value outside the range.
struct Track {
    std::vector<Key> k;
    Track& add(float t, float v) { k.push_back({t, v}); return *this; }
    float at(float t) const {
        if (k.empty()) return 0;
        if (t <= k.front().t) return k.front().v;
        if (t >= k.back().t)  return k.back().v;
        for (size_t i = 0; i + 1 < k.size(); ++i)
            if (t < k[i + 1].t) {
                float u = (t - k[i].t) / (k[i + 1].t - k[i].t);
                return lerp(k[i].v, k[i + 1].v, easeInOut(u));
            }
        return k.back().v;
    }
};

}  // namespace anim

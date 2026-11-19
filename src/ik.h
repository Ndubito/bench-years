#pragma once
// Owner: C. Two-bone inverse kinematics (arms, and legs if needed).
#include "vec.h"
#include <algorithm>
#include <cmath>

struct IKResult { Vec2 elbow, hand; };

// Root at s, hand wants to reach t, bone lengths l1 (upper) and l2 (lower).
// dir = +1 or -1 chooses which side the elbow/knee bends to.
// Targets that are too far are pulled in to full stretch, too close are pushed out.
inline IKResult ik2(Vec2 s, Vec2 t, float l1, float l2, float dir) {
    float dx = t.x - s.x, dy = t.y - s.y, d = std::hypot(dx, dy);
    const float maxd = l1 + l2 - 0.01f, mind = std::fabs(l1 - l2) + 0.5f;
    if (d > maxd) { dx *= maxd / d; dy *= maxd / d; d = maxd; }
    if (d < mind) { d = mind; if (dx == 0 && dy == 0) dy = d; }
    float a = (l1 * l1 - l2 * l2 + d * d) / (2 * d);
    float h = std::sqrt(std::max(0.0f, l1 * l1 - a * a));
    float len = std::hypot(dx, dy); if (len == 0) len = 1;
    float ux = dx / len, uy = dy / len;
    float mx = s.x + ux * a, my = s.y + uy * a;
    IKResult r;
    r.elbow = { mx + dir * (-uy) * h, my + dir * ux * h };
    r.hand  = { s.x + ux * d,         s.y + uy * d };
    return r;
}

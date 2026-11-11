#pragma once
// SHARED INTERFACE. Changes only through A, announced in the group chat.
#include <vector>
#include "vec.h"

enum class Kind { Poly, Disc, Strip, Line };

struct Shape {
    Kind kind = Kind::Poly;
    std::vector<Vec2> pts;          // Poly/Strip/Line points; Disc centre in pts[0]
    float r = 0, w0 = 0, w1 = 0;    // Disc radius; Strip start and end width
    Mat3 xf;                        // transform applied to pts before drawing
    uint32_t rgba = 0xFF000000u;    // 0xAABBGGRR
    float z = 0;                    // depth layer; low z draws first
};

constexpr int kWidth = 640, kHeight = 360;   // pixel space, y points down

int sceneCount();                                      // 8 scenes in the story
std::vector<Shape> buildScene(int scene, float t);     // t = seconds since scene start

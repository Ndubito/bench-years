#pragma once
// Owner: B. Signatures are fixed. Replace the bodies in raster.cpp.
#include <vector>
#include "fb.h"
#include "vec.h"

void drawLine(Framebuffer&, int x0, int y0, int x1, int y1, uint32_t c);   // Bresenham
void drawCircle(Framebuffer&, int cx, int cy, int r, uint32_t c);          // midpoint, outline
void fillDisc(Framebuffer&, int cx, int cy, int r, uint32_t c);            // midpoint, horizontal spans
void fillPoly(Framebuffer&, const std::vector<Vec2>& pts, uint32_t c);     // scanline, even-odd
std::vector<Vec2> bezier(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, int n);       // cubic, n segments

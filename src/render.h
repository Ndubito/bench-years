#pragma once
// Owner: A
#include <vector>
#include "fb.h"
#include "shape.h"

void renderShapes(Framebuffer& fb, std::vector<Shape> shapes);   // sorts by z, draws with raster.h

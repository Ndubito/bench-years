#pragma once
// Owner: A. CP2 evidence: one test image per algorithm, drawn only through raster.h.
#include <string>
#include "fb.h"

void renderAlgoImage(Framebuffer& fb, const std::string& which);   // "line" | "circle" | "fill" | "bezier"

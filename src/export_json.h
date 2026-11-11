#pragma once
// Owner: E
#include <string>
#include <vector>
#include "shape.h"

// Writes one frame of world-space shapes. Schema in docs/SCHEMA.md.
bool writeFrameJSON(const std::string& path, const std::vector<Shape>& shapes, int scene, int frame, float t);

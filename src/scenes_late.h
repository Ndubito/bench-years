#pragma once
// Owner: C. Characters for scenes 5 to 8 (scene indices 4 to 7).
// Appends the figures to v; does nothing for scenes 1 to 4.
// t is seconds inside the scene. Call at the end of buildScene.
#include "shape.h"
#include <vector>

void addScenesLate(std::vector<Shape>& v, int scene, float t);

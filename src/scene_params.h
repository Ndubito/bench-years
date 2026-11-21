#pragma once

#include <array>
#include <cstdint>

struct Rgb8 {
    std::uint8_t r, g, b;
};

struct SceneParams {
    Rgb8 skyTop, skyBottom, sun, hill, ground, leaves, trunk;
    float sunAngle;
    float treeSize, leafDensity, darkness, benchWear;
    float ambient, sunlight, lampPower;
    bool lampOn, plaque;
    int leafCount, flowers, dirtPatches, grassTufts;
};

constexpr std::array<SceneParams, 8> kSceneParams = {{
    SceneParams{Rgb8{135,195,240},Rgb8{255,235,205},Rgb8{255,230,150},Rgb8{150,185,170},Rgb8{122,184,98},Rgb8{124,184,86},Rgb8{110,82,60}, 0.35f,0.25f,0.00f,0.00f,0.00f,0.52f,0.72f,0.0f,false,false,108,0,0,100},
    SceneParams{Rgb8{125,185,238},Rgb8{225,238,248},Rgb8{255,235,160},Rgb8{135,175,160},Rgb8{111,174,98},Rgb8{98,168,76},Rgb8{105,78,58}, 0.70f,0.50f,0.25f,0.00f,0.00f,0.52f,0.72f,0.0f,false,false,216,0,0,140},
    SceneParams{Rgb8{100,170,235},Rgb8{205,230,250},Rgb8{255,242,180},Rgb8{120,165,150},Rgb8{98,170,86},Rgb8{70,150,64},Rgb8{100,74,54}, 1.10f,0.80f,0.55f,0.00f,0.00f,0.48f,0.78f,0.0f,false,false,324,12,0,180},
    SceneParams{Rgb8{80,155,235},Rgb8{195,225,250},Rgb8{255,247,200},Rgb8{115,160,145},Rgb8{92,165,80},Rgb8{60,142,58},Rgb8{98,72,52}, 1.57f,1.00f,1.00f,0.00f,0.00f,0.48f,0.78f,0.0f,false,false,432,22,0,230},
    SceneParams{Rgb8{115,170,225},Rgb8{238,226,200},Rgb8{255,228,150},Rgb8{140,160,130},Rgb8{135,160,80},Rgb8{196,150,52},Rgb8{96,70,50}, 2.05f,1.00f,0.70f,0.26f,0.02f,0.45f,0.68f,0.0f,false,false,324,0,0,220},
    SceneParams{Rgb8{225,165,120},Rgb8{255,215,150},Rgb8{255,200,110},Rgb8{150,130,115},Rgb8{150,140,70},Rgb8{214,110,42},Rgb8{90,65,48}, 2.60f,1.00f,0.35f,0.45f,0.02f,0.43f,0.58f,0.0f,false,false,216,0,0,240},
    SceneParams{Rgb8{110,85,140},Rgb8{240,150,110},Rgb8{255,165,95},Rgb8{95,80,100},Rgb8{95,100,62},Rgb8{150,90,40},Rgb8{80,60,45}, 2.95f,1.00f,0.12f,0.80f,0.40f,0.28f,0.20f,1.5f,true,false,108,0,0,250},
    SceneParams{Rgb8{14,20,52},Rgb8{42,52,92},Rgb8{225,230,245},Rgb8{28,34,62},Rgb8{30,48,46},Rgb8{80,70,40},Rgb8{60,48,40}, 1.90f,1.00f,0.00f,0.60f,0.50f,0.20f,0.08f,3.5f,true,true,0,0,18,110},
}};

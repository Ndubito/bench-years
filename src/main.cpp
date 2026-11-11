// Owner: A.
// usage: bench [scene 0-7] [t seconds] [out.ppm] [--json out.json]
//        bench --algos out/cp2     (four algorithm test images)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "algo_demo.h"
#include "export_json.h"
#include "fb.h"
#include "render.h"
#include "shape.h"

int main(int argc, char** argv) {
    if (argc > 2 && !std::strcmp(argv[1], "--algos")) {          // bench --algos out/cp2
        for (const char* n : { "line", "circle", "fill", "bezier" }) {
            Framebuffer fb(kWidth, kHeight);
            renderAlgoImage(fb, n);
            std::string path = std::string(argv[2]) + "_" + n + ".ppm";
            if (!fb.savePPM(path)) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return 1; }
            std::printf("wrote %s\n", path.c_str());
        }
        return 0;
    }
    int scene = argc > 1 ? std::atoi(argv[1]) : 0;
    float t = argc > 2 ? float(std::atof(argv[2])) : 0.f;
    std::string out = argc > 3 && std::strncmp(argv[3], "--", 2) ? argv[3] : "out/scene.ppm";
    std::string json;
    for (int i = 1; i + 1 < argc; i++) if (!std::strcmp(argv[i], "--json")) json = argv[i + 1];
    if (scene < 0 || scene >= sceneCount()) { std::fprintf(stderr, "scene must be 0..%d\n", sceneCount() - 1); return 1; }

    auto shapes = buildScene(scene, t);
    Framebuffer fb(kWidth, kHeight);
    fb.clear(0xFF000000u);
    renderShapes(fb, shapes);
    if (!fb.savePPM(out)) { std::fprintf(stderr, "cannot write %s\n", out.c_str()); return 1; }
    std::printf("wrote %s (%u shapes)\n", out.c_str(), (unsigned)shapes.size());
    if (!json.empty()) {
        if (!writeFrameJSON(json, shapes, scene, 0, t)) { std::fprintf(stderr, "cannot write %s\n", json.c_str()); return 1; }
        std::printf("wrote %s\n", json.c_str());
    }
    return 0;
}

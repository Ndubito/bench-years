// Owner: A.
// usage: bench [scene 0-7] [t seconds] [out.ppm] [--json out.json]
//        bench --algos out/cp2     (four algorithm test images)
//        bench --film 12.5 out.ppm  (one frame of the whole film at 12.5 s)
//        bench --frames out/f 24 [from] [to] [--json]
//                                   (numbered frames f_0000.ppm ... at 24 fps,
//                                    optionally only from..to seconds;
//                                    --json also writes f_NNNN.json for every
//                                    scene frame, none for gust frames)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include "algo_demo.h"
#include "export_json.h"
#include "fb.h"
#include "film.h"
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
    if (argc > 3 && !std::strcmp(argv[1], "--film")) {           // bench --film 12.5 out.ppm
        Framebuffer fb(kWidth, kHeight);
        float T = float(std::atof(argv[2]));
        FilmTime f = filmAt(T);
        renderFilm(fb, T);
        if (!fb.savePPM(argv[3])) { std::fprintf(stderr, "cannot write %s\n", argv[3]); return 1; }
        std::printf("wrote %s (T=%.2f: %s scene %d, t=%.2f)\n", argv[3], T, f.gust ? "gust from" : "", f.scene, f.t);
        return 0;
    }
    if (argc > 3 && !std::strcmp(argv[1], "--frames")) {         // bench --frames out/f 24 [from] [to] [--json]
        bool json = false;
        const char* pos[2] = { nullptr, nullptr };              // optional from, to
        int np = 0;
        for (int i = 4; i < argc; i++) {
            if (!std::strcmp(argv[i], "--json")) json = true;
            else if (np < 2) pos[np++] = argv[i];
        }
        std::string base = argv[2];
        int fps = std::atoi(argv[3]);
        if (fps <= 0) { std::fprintf(stderr, "fps must be above 0\n"); return 1; }
        float from = pos[0] ? float(std::atof(pos[0])) : 0.f;
        float to   = pos[1] ? float(std::atof(pos[1])) : filmLength();
        int first = int(std::ceil(from * fps)), last = int(std::ceil(to * fps)) - 1;
        int jsonCount = 0;
        Framebuffer fb(kWidth, kHeight);
        for (int n = first; n <= last; n++) {
            float T = float(n) / fps;
            int k = n - first;                                   // numbered from 0 for ffmpeg
            renderFilm(fb, T);
            char name[32];
            std::sprintf(name, "_%04d.ppm", k);
            if (!fb.savePPM(base + name)) { std::fprintf(stderr, "cannot write %s%s\n", base.c_str(), name); return 1; }
            FilmTime ft = filmAt(T);
            if (json && !ft.gust) {                              // gust frames have no shape list: they mix two images
                std::sprintf(name, "_%04d.json", k);
                if (!writeFrameJSON(base + name, buildScene(ft.scene, ft.t), ft.scene, k, ft.t)) {
                    std::fprintf(stderr, "cannot write %s%s\n", base.c_str(), name); return 1;
                }
                jsonCount++;
            }
            if (k % fps == 0) std::printf("frame %d of %d (%.0f s)\n", k, last - first + 1, T);
        }
        std::printf("wrote %d frames: %s_0000.ppm ...\n", last - first + 1, base.c_str());
        if (json) std::printf("wrote %d JSON files (gust frames have none)\n", jsonCount);
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

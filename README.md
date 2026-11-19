# Bench Years

Park bench life story. Pure C++ rasterizer first, then OpenGL viewer, then Blender render.

## Build and run

Windows (Git Bash or MSYS2 UCRT64), Linux or Mac:

    g++ -std=c++14 -O2 -Wall -o bench src/*.cpp

    ./bench 0 0 out/scene0.ppm                    one scene: number 0-7, seconds into it, output
    ./bench 0 0 out/scene0.ppm --json out/s0.json the same, plus its shape list for Blender
    ./bench --algos out/cp2                       the four algorithm test images
    ./bench --film 7.2 out/gust.ppm               one frame of the whole film at 7.2 s
    mkdir -p out/frames                           the frames folder must exist first
    ./bench --frames out/frames/f 24              every frame of the film at 24 fps
    ./bench --frames out/frames/f 24 40 52        only 40 s to 52 s
    ./bench --frames out/frames/f 24 --json       also a JSON per scene frame (none for gust frames)

The whole film at 24 fps is about 1,680 frames and 1.1 GB of `.ppm` files. Delete `out/frames` when done.

## Making the film

1. `mkdir -p out/frames out/blender`, then `./bench --frames out/frames/f 24 --json`
2. Blender (Githinji): render each `out/frames/f_NNNN.json` to `out/blender/f_NNNN.png` with the same number.
3. `bash tools/assemble_film.sh` joins them into `out/film.mp4`. Frames without a Blender PNG, including every gust frame, use the C++ image.

## Ownership

| Member | Owns |
| --- | --- |
| Nathan Achar | `main.cpp`, `render.cpp`, `shape.h`, README, merges to main |
| Faith Muthoni | `fb.*`, `raster.*`, then the OpenGL viewer |
| Maria Muwale | `vec.h` (matrix stack), IK, figures |
| Alan Waithaka | `scene.cpp` and the scene scripts |
| Githinji Gathungu | `export_json.*`, `docs/SCHEMA.md`, Blender scripts under `blender/` |

`shape.h` and the signatures in `raster.h` change only through A, announced in the group chat.

## Git rules

- Branch per member: `b/raster`, `c/math`, `d/scenes`, `e/export`.
- Open a pull request into `main`. Only A merges.
- A pull request is merged when it builds from a clean clone, `./build/bench 0 0 out/check.ppm` runs, and the README Remaining list is updated.
- No generated files in commits: images, frames, JSON output, videos and logs stay on your machine. Attach a PNG to the pull request description instead.

## Remaining (updated 19 Nov)

- [x] Faith: Bresenham line, midpoint circle, scanline fill, Bezier
- [ ] Faith: OpenGL viewer (window drawing `buildScene`, keys to scrub time and jump scenes)
- [x] Maria: Mat3 operations and matrix stack
- [x] Maria: two-bone IK, seated and walking figures (PR #6)
- [x] Alan: environment for all eight scenes
- [ ] Alan and Maria: figures placed and animated in scenes 1 to 8
- [x] Githinji: Blender render of one frame
- [ ] Githinji: Blender render of every scene frame from `out/frames/f_NNNN.json` (PR #5)
- [x] Nathan: timeline, leaf-gust transitions, alpha blending, JSON per frame, film assembly
- [ ] Nathan: final film, demo

## Changes

- C: `vec.h` now has `Mat3::operator*`, `translate`, `rotate`, `scale` and a `MatrixStack` (push, pop, translate, rotate, scale, apply).
- Tested with a two-segment swinging arm (`out/matrix_test.png`).
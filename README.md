# Bench Years

Park bench life story. Pure C++ rasterizer first, then OpenGL viewer, then Blender render.

## Build and run

    cmake -S . -B build
    cmake --build build
    ./build/bench 0 0 out/scene0.ppm --json out/scene0.json

The program prints `WARNING: placeholder rasterizer in use` while `src/raster.cpp` still holds the naive placeholders. That warning must be gone before CP2 is submitted.

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
- Output images committed under `out/` should be PNG (`magick out/x.ppm out/x.png`).

## Remaining (update daily)

- [ ] B: Bresenham line, midpoint circle, scanline fill, Bezier (replace placeholders)
- [x] C: Mat3 operations and matrix stack
- [ ] C: two-bone IK, seated and walking figures
- [ ] D: environment still, then scenes 1 to 8
- [ ] E: importer for `docs/SCHEMA.md` frames in Blender
- [ ] A: timeline controller and leaf gust transition

## Changes

- C: `vec.h` now has `Mat3::operator*`, `translate`, `rotate`, `scale` and a `MatrixStack` (push, pop, translate, rotate, scale, apply).
- Tested with a two-segment swinging arm (`out/matrix_test.png`).
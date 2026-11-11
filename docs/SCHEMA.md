# Frame JSON, v0 (owner: E)

One file per frame. Points are already in world space (the shape transform is applied by the exporter), so the Blender importer needs no matrix maths.

    {
      "width": 640, "height": 360,     pixel space, origin top-left, y points down
      "scene": 0, "frame": 0, "t": 0.0,
      "shapes": [
        { "kind": "poly" | "disc" | "strip" | "line",
          "z": 0.0,                    depth layer, low z is behind
          "rgba": 4294967295,          0xAABBGGRR as one unsigned integer
          "r": 0, "w0": 0, "w1": 0,    disc radius; strip start and end width
          "pts": [[x, y], ...] }       disc centre in pts[0]
      ]
    }

Importer rules (Blender): flip y (`y_blender = height - y`), scale so the frame is 16 x 9 units, place each shape at `z * 0.01`.
v1 (E, from 16 Nov): stable shape ids and per-frame transforms so Blender can keyframe objects instead of rebuilding them.

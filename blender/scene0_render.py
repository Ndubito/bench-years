import bpy, json, math, sys, os


JSON_PATH   = "out/scene0.json"
OUT_PNG     = "out/scene0_render.png"
COLOR_ORDER = "ABGR"
Z_STEP      = 0.01
LINE_WIDTH  = 1.0       


if "--" in sys.argv:
    extra = sys.argv[sys.argv.index("--") + 1:]
    if len(extra) >= 1: JSON_PATH = extra[0]
    if len(extra) >= 2: OUT_PNG = extra[1]

def unpack(v):
    a = (v >> 24) & 255
    if COLOR_ORDER == "ABGR":
        r, g, b = v & 255, (v >> 8) & 255, (v >> 16) & 255
    else:
        r, g, b = (v >> 16) & 255, (v >> 8) & 255, v & 255
    return r / 255, g / 255, b / 255, a / 255

def to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

_mats = {}
def get_material(rgba):
    if rgba in _mats:
        return _mats[rgba]
    r, g, b, a = unpack(rgba)
    mat = bpy.data.materials.new(f"m_{rgba}")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    emit = nt.nodes.new("ShaderNodeEmission")
    emit.inputs[0].default_value = (to_linear(r), to_linear(g), to_linear(b), 1.0)
    if a < 0.999:  # semi-transparent shape
        trans = nt.nodes.new("ShaderNodeBsdfTransparent")
        mix = nt.nodes.new("ShaderNodeMixShader")
        mix.inputs[0].default_value = a
        nt.links.new(trans.outputs[0], mix.inputs[1])
        nt.links.new(emit.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs["Surface"])
        try: mat.surface_render_method = 'BLENDED'
        except Exception: mat.blend_method = 'BLEND'
    else:
        nt.links.new(emit.outputs[0], out.inputs["Surface"])
    _mats[rgba] = mat
    return mat

def make_polys(name, polys, z, mat, H):
    """One mesh object made of one or more polygons (each a list of (x, y) in JSON pixel space)."""
    verts, faces = [], []
    for pts in polys:
        base = len(verts)
        verts += [(x, H - y, z) for x, y in pts]      # flip y
        faces.append(list(range(base, base + len(pts))))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    mesh.materials.append(mat)
    return obj

def circle_pts(cx, cy, r, n=64):
    return [(cx + r * math.cos(2 * math.pi * i / n),
             cy + r * math.sin(2 * math.pi * i / n)) for i in range(n)]

def strip_quads(pts, w0, w1):
    """One quad per segment of the polyline `pts`.
    Full width goes linearly from w0 (first point) to w1 (last point), measured along the path length."""
    segs = list(zip(pts, pts[1:]))
    lengths = [math.hypot(x1 - x0, y1 - y0) for (x0, y0), (x1, y1) in segs]
    total = sum(lengths)
    quads, acc = [], 0.0
    for ((x0, y0), (x1, y1)), L in zip(segs, lengths):
        if L < 1e-9:          # zero-length segment, nothing to draw
            continue
        t0, t1 = acc / total, (acc + L) / total
        acc += L
        ha = (w0 + (w1 - w0) * t0) / 2.0     # half-width at segment start
        hb = (w0 + (w1 - w0) * t1) / 2.0     # half-width at segment end
        nx, ny = -(y1 - y0) / L, (x1 - x0) / L   # unit normal of the segment
        quads.append([
            (x0 + nx * ha, y0 + ny * ha),
            (x1 + nx * hb, y1 + ny * hb),
            (x1 - nx * hb, y1 - ny * hb),
            (x0 - nx * ha, y0 - ny * ha),
        ])
    return quads

def build(data):
    W, H = data["width"], data["height"]

    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)

    for i, s in enumerate(data["shapes"]):
        kind = s["kind"]
        z = s["z"] * Z_STEP + i * 1e-5
        mat = get_material(s["rgba"])
        if kind == "poly":
            make_polys(f"poly_{i}", [s["pts"]], z, mat, H)
        elif kind == "disc":
            cx, cy = s["pts"][0]
            make_polys(f"disc_{i}", [circle_pts(cx, cy, s["r"])], z, mat, H)
        elif kind in ("strip", "line"):
            pts = s["pts"]
            if len(pts) < 2:
                print(f"[warn] {kind} (shape {i}) has fewer than 2 points, skipped")
                continue
            if kind == "strip":
                quads = strip_quads(pts, s.get("w0", 0.0), s.get("w1", 0.0))
            else:
                quads = strip_quads(pts, LINE_WIDTH, LINE_WIDTH)
            if not quads:
                print(f"[warn] {kind} (shape {i}) produced no geometry (zero length?), skipped")
                continue
            make_polys(f"{kind}_{i}", quads, z, mat, H)
        else:
            print(f"[warn] unknown kind '{kind}' (shape {i}) skipped")

    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = 'ORTHO'
    cam_data.sensor_fit = 'HORIZONTAL'
    cam_data.ortho_scale = W
    cam_data.clip_end = 100
    cam = bpy.data.objects.new("cam", cam_data)
    cam.location = (W / 2, H / 2, 10)
    bpy.context.collection.objects.link(cam)

    sc = bpy.context.scene
    sc.camera = cam
    r = sc.render
    r.resolution_x, r.resolution_y, r.resolution_percentage = W, H, 100
    r.image_settings.file_format = 'PNG'
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "CYCLES"):
        try:
            r.engine = eng
            break
        except TypeError:
            continue
    try: sc.view_settings.view_transform = 'Standard'
    except Exception: pass
    return sc

with open(JSON_PATH) as f:
    data = json.load(f)

scene = build(data)
scene.render.filepath = os.path.abspath(OUT_PNG)
bpy.ops.render.render(write_still=True)
print("Rendered ->", scene.render.filepath)
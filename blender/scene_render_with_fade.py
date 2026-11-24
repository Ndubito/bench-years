import bpy, json, math, sys, os, glob, re, time

# Reads out/frames/f_NNNN.json and writes out/blender/f_NNNN.png.
# Missing frame gaps (previously gust transitions) are automatically filled with a smooth cross-fade.

# ---------------- config ----------------
_txt = getattr(getattr(bpy.context, "space_data", None), "text", None)
_src = bpy.path.abspath(_txt.filepath) if _txt and _txt.filepath else __file__
BASE = os.path.dirname(os.path.dirname(os.path.abspath(_src)))

JSON_GLOB   = "out/frames/f_*.json"
OUT_DIR     = os.environ.get("BENCH_RENDER_OUT", os.path.join(BASE, "out", "blender"))
COLOR_ORDER = "ABGR"
Z_STEP      = 0.01
LINE_WIDTH  = 1.0

SKIP_FIRST    = 0
MAX_FRAMES    = None
SKIP_EXISTING = True

# Fade configuration:
# "CROSSFADE" = smooth dissolve from previous scene's last frame into next scene's first frame
# "FADE_BLACK" = fade to black halfway through the gap, then fade in the next scene
FADE_MODE     = "CROSSFADE"

ONE_MESH_PER_FRAME    = False
RESOLUTION_PERCENTAGE = 100
# ----------------------------------------

if "--" in sys.argv:
    extra = sys.argv[sys.argv.index("--") + 1:]
    if len(extra) >= 1: JSON_GLOB = extra[0]
    if len(extra) >= 2: OUT_DIR = extra[1]

def absp(p):
    return p if os.path.isabs(p) else os.path.join(BASE, p)

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
    if a < 0.999:
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
    verts, faces = [], []
    for pts in polys:
        base = len(verts)
        verts += [(x, H - y, z) for x, y in pts]
        faces.append(list(range(base, base + len(pts))))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    mesh.materials.append(mat)
    return obj

def make_merged(name, items, H):
    verts, faces, face_slot = [], [], []
    slots = {}
    for i, s, polys in items:
        z = s["z"] * Z_STEP + i * 1e-5
        if s["rgba"] not in slots:
            slots[s["rgba"]] = len(slots)
        slot = slots[s["rgba"]]
        for pts in polys:
            base = len(verts)
            verts += [(x, H - y, z) for x, y in pts]
            faces.append(list(range(base, base + len(pts))))
            face_slot.append(slot)
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    for rgba, _ in sorted(slots.items(), key=lambda kv: kv[1]):
        mesh.materials.append(get_material(rgba))
    for poly, slot in zip(mesh.polygons, face_slot):
        poly.material_index = slot
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj

def circle_pts(cx, cy, r, n=64):
    return [(cx + r * math.cos(2 * math.pi * i / n),
             cy + r * math.sin(2 * math.pi * i / n)) for i in range(n)]

def strip_quads(pts, w0, w1):
    segs = list(zip(pts, pts[1:]))
    lengths = [math.hypot(x1 - x0, y1 - y0) for (x0, y0), (x1, y1) in segs]
    total = sum(lengths)
    quads, acc = [], 0.0
    for ((x0, y0), (x1, y1)), L in zip(segs, lengths):
        if L < 1e-9:
            continue
        t0, t1 = acc / total, (acc + L) / total
        acc += L
        ha = (w0 + (w1 - w0) * t0) / 2.0
        hb = (w0 + (w1 - w0) * t1) / 2.0
        nx, ny = -(y1 - y0) / L, (x1 - x0) / L
        quads.append([
            (x0 + nx * ha, y0 + ny * ha),
            (x1 + nx * hb, y1 + ny * hb),
            (x1 - nx * hb, y1 - ny * hb),
            (x0 - nx * ha, y0 - ny * ha),
        ])
    return quads

def shape_polys(i, s):
    kind = s["kind"]
    if kind == "poly":
        return [s["pts"]]
    if kind == "disc":
        cx, cy = s["pts"][0]
        return [circle_pts(cx, cy, s["r"])]
    if kind in ("strip", "line"):
        pts = s["pts"]
        if len(pts) < 2:
            return None
        quads = strip_quads(pts, s.get("w0", 0.0), s.get("w1", 0.0)) if kind == "strip" else strip_quads(pts, LINE_WIDTH, LINE_WIDTH)
        return quads if quads else None
    return None

def clear_scene():
    for o in list(bpy.data.objects):   bpy.data.objects.remove(o, do_unlink=True)
    for m in list(bpy.data.meshes):    bpy.data.meshes.remove(m)
    for m in list(bpy.data.materials): bpy.data.materials.remove(m)
    for c in list(bpy.data.cameras):   bpy.data.cameras.remove(c)
    for img in list(bpy.data.images):  bpy.data.images.remove(img)
    _mats.clear()

def configure_render_settings(sc, W, H):
    r = sc.render
    r.resolution_x, r.resolution_y, r.resolution_percentage = W, H, RESOLUTION_PERCENTAGE
    r.image_settings.file_format = 'PNG'
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "CYCLES"):
        try:
            r.engine = eng
            break
        except TypeError:
            continue
    try: sc.view_settings.view_transform = 'Standard'
    except Exception: pass

def build(data):
    W, H = data["width"], data["height"]
    clear_scene()
    sc = bpy.context.scene
    sc.use_nodes = False

    items = []
    for i, s in enumerate(data["shapes"]):
        polys = shape_polys(i, s)
        if polys:
            items.append((i, s, polys))

    if ONE_MESH_PER_FRAME:
        merged   = [it for it in items if unpack(it[1]["rgba"])[3] >= 0.999]
        separate = [it for it in items if unpack(it[1]["rgba"])[3] < 0.999]
        if merged:
            make_merged("frame", merged, H)
    else:
        separate = items

    for i, s, polys in separate:
        z = s["z"] * Z_STEP + i * 1e-5
        make_polys(f"{s['kind']}_{i}", polys, z, get_material(s["rgba"]), H)

    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = 'ORTHO'
    cam_data.sensor_fit = 'HORIZONTAL'
    cam_data.ortho_scale = W
    cam_data.clip_end = 100
    cam = bpy.data.objects.new("cam", cam_data)
    cam.location = (W / 2, H / 2, 10)
    bpy.context.collection.objects.link(cam)
    sc.camera = cam

    configure_render_settings(sc, W, H)
    return sc

def smoothstep(u):
    u = max(0.0, min(1.0, u))
    return u * u * (3.0 - 2.0 * u)

def render_fade_gap(out_dir, img_a_path, img_b_path, gap_numbers, pad_len, mode="CROSSFADE"):
    """Uses Blender's Compositor to blend img_a_path into img_b_path across gap_numbers."""
    clear_scene()
    sc = bpy.context.scene
    sc.use_nodes = True
    nt = sc.node_tree
    nt.nodes.clear()

    img_a = bpy.data.images.load(img_a_path)
    img_b = bpy.data.images.load(img_b_path)
    W, H = img_a.size[0], img_a.size[1]
    configure_render_settings(sc, W, H)

    n_a = nt.nodes.new("CompositorNodeImage")
    n_a.image = img_a
    n_b = nt.nodes.new("CompositorNodeImage")
    n_b.image = img_b
    n_out = nt.nodes.new("CompositorNodeComposite")

    total = len(gap_numbers)
    if mode == "FADE_BLACK":
        # Fade A -> Black in first half, Black -> B in second half
        n_black = nt.nodes.new("CompositorNodeRGB")
        n_black.outputs[0].default_value = (0.0, 0.0, 0.0, 1.0)
        n_mix = nt.nodes.new("CompositorNodeMixRGB")
        n_mix.blend_type = 'MIX'
        nt.links.new(n_mix.outputs[0], n_out.inputs[0])

        for idx, frame_num in enumerate(gap_numbers):
            out_png = os.path.join(out_dir, f"f_{str(frame_num).zfill(pad_len)}.png")
            if SKIP_EXISTING and os.path.exists(out_png):
                continue
            u = (idx + 1) / (total + 1)
            nt.links.clear()
            nt.links.new(n_mix.outputs[0], n_out.inputs[0])
            if u < 0.5:
                fac = smoothstep(u * 2.0)
                n_mix.inputs[0].default_value = fac
                nt.links.new(n_a.outputs[0], n_mix.inputs[1])
                nt.links.new(n_black.outputs[0], n_mix.inputs[2])
            else:
                fac = smoothstep((u - 0.5) * 2.0)
                n_mix.inputs[0].default_value = fac
                nt.links.new(n_black.outputs[0], n_mix.inputs[1])
                nt.links.new(n_b.outputs[0], n_mix.inputs[2])
            sc.render.filepath = out_png
            bpy.ops.render.render(write_still=True)
    else:
        # Smooth CROSSFADE directly from A to B
        n_mix = nt.nodes.new("CompositorNodeMixRGB")
        n_mix.blend_type = 'MIX'
        nt.links.new(n_a.outputs[0], n_mix.inputs[1])
        nt.links.new(n_b.outputs[0], n_mix.inputs[2])
        nt.links.new(n_mix.outputs[0], n_out.inputs[0])

        for idx, frame_num in enumerate(gap_numbers):
            out_png = os.path.join(out_dir, f"f_{str(frame_num).zfill(pad_len)}.png")
            if SKIP_EXISTING and os.path.exists(out_png):
                continue
            u = (idx + 1) / (total + 1)
            n_mix.inputs[0].default_value = smoothstep(u)
            sc.render.filepath = out_png
            bpy.ops.render.render(write_still=True)

_NUM = re.compile(r"f_(\d+)\.json$", re.IGNORECASE)

def list_frames():
    frames = []
    for path in glob.glob(absp(JSON_GLOB)):
        m = _NUM.search(os.path.basename(path))
        if m:
            frames.append((int(m.group(1)), m.group(1), path))
    frames.sort()
    return frames

# ---- main ----
for h in list(bpy.app.handlers.frame_change_pre):
    if getattr(h, "__name__", "") == "_on_frame":
        bpy.app.handlers.frame_change_pre.remove(h)

all_frames = list_frames()
if not all_frames:
    raise RuntimeError(f"No f_NNNN.json files matched: {absp(JSON_GLOB)}")

pad_len = len(all_frames[0][1])
frames = all_frames[SKIP_FIRST:]
if MAX_FRAMES is not None:
    frames = frames[:MAX_FRAMES]

test_run = MAX_FRAMES is not None
out_dir = os.path.join(OUT_DIR, "test") if test_run else OUT_DIR
os.makedirs(out_dir, exist_ok=True)

# 1. Render all JSON scene frames
for k, (num, digits, path) in enumerate(frames):
    out_png = os.path.join(out_dir, f"f_{digits}.png")
    if SKIP_EXISTING and not test_run and os.path.exists(out_png):
        continue
    with open(path) as fh:
        data = json.load(fh)
    sc = build(data)
    sc.render.filepath = out_png
    bpy.ops.render.render(write_still=True)
    print(f"[{k + 1}/{len(frames)}] Rendered scene frame f_{digits}.png")

# 2. Fill transition gaps with smooth fades instead of gust transitions
if not test_run:
    for (num_a, dig_a, _), (num_b, dig_b, _) in zip(frames, frames[1:]):
        if num_b - num_a > 1:
            gap_nums = list(range(num_a + 1, num_b))
            img_a_path = os.path.join(out_dir, f"f_{dig_a}.png")
            img_b_path = os.path.join(out_dir, f"f_{dig_b}.png")
            if os.path.exists(img_a_path) and os.path.exists(img_b_path):
                print(f"Rendering {FADE_MODE} for frames {gap_nums[0]}-{gap_nums[-1]}...")
                render_fade_gap(out_dir, img_a_path, img_b_path, gap_nums, pad_len, mode=FADE_MODE)
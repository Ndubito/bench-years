import bpy, json, math, sys, os, glob, re, time

# Reads out/frames/f_NNNN.json and writes out/blender/f_NNNN.png with the SAME number.
# Numbers with no JSON (the gust frames) are skipped; tools/assemble_film.sh fills them from the C++ images.


# ---------------- config ----------------
# Repo root, found relative to this script (script lives in <repo>/blender/).
_txt = getattr(getattr(bpy.context, "space_data", None), "text", None)   # Blender GUI: the open text block
_src = bpy.path.abspath(_txt.filepath) if _txt and _txt.filepath else __file__   # command line (-P): the script file
BASE = os.path.dirname(os.path.dirname(os.path.abspath(_src)))

JSON_GLOB   = "out/frames/f_*.json"   # written by ./bench --frames ... --json
OUT_DIR     = os.environ.get("BENCH_RENDER_OUT", os.path.join(BASE, "out", "blender"))   # where assemble_film.sh looks
COLOR_ORDER = "ABGR"
Z_STEP      = 0.01
LINE_WIDTH  = 1.0                     # "line" shapes are drawn as quads this many pixels wide

SKIP_FIRST    = 0      
MAX_FRAMES    = None
SKIP_EXISTING = True    # full render only: don't re-render PNGs that already exist (resume after stopping)

ONE_MESH_PER_FRAME    = False   # True = all opaque shapes in ONE mesh with material slots (faster in Blender)
                                #        False = one object per shape (original behavior)
RESOLUTION_PERCENTAGE = 100     # 100 = size from the JSON (640x360); 200 = 1280x720.
                                # Before using 200, check tools/assemble_film.sh can mix 1280x720 with the C++ images.
# ----------------------------------------


if "--" in sys.argv:  # command line only: blender -b -P script.py -- "<glob>" "<outdir>"
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

def make_merged(name, items, H):
    """ONE mesh object for many shapes. Each shape keeps its own z; faces get the material slot of their color.
    items = [(shape_index, shape_dict, polygons), ...]"""
    verts, faces, face_slot = [], [], []
    slots = {}                                   # rgba -> material slot index
    for i, s, polys in items:
        z = s["z"] * Z_STEP + i * 1e-5
        if s["rgba"] not in slots:
            slots[s["rgba"]] = len(slots)
        slot = slots[s["rgba"]]
        for pts in polys:
            base = len(verts)
            verts += [(x, H - y, z) for x, y in pts]      # flip y
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

def shape_polys(i, s):
    """The polygons (lists of (x, y) pixel points) that make up one shape, or None if nothing can be drawn."""
    kind = s["kind"]
    if kind == "poly":
        return [s["pts"]]
    if kind == "disc":
        cx, cy = s["pts"][0]
        return [circle_pts(cx, cy, s["r"])]
    if kind in ("strip", "line"):
        pts = s["pts"]
        if len(pts) < 2:
            print(f"[warn] {kind} (shape {i}) has fewer than 2 points, skipped")
            return None
        if kind == "strip":
            quads = strip_quads(pts, s.get("w0", 0.0), s.get("w1", 0.0))
        else:
            quads = strip_quads(pts, LINE_WIDTH, LINE_WIDTH)
        if not quads:
            print(f"[warn] {kind} (shape {i}) produced no geometry (zero length?), skipped")
            return None
        return quads
    print(f"[warn] unknown kind '{kind}' (shape {i}) skipped")
    return None

def clear_scene():
    """Remove the previous frame completely: objects first, then the datablocks they used.
    Removing only the objects leaves their meshes/materials/cameras behind (memory grows every frame)."""
    for o in list(bpy.data.objects):   bpy.data.objects.remove(o, do_unlink=True)
    for m in list(bpy.data.meshes):    bpy.data.meshes.remove(m)
    for m in list(bpy.data.materials): bpy.data.materials.remove(m)
    for c in list(bpy.data.cameras):   bpy.data.cameras.remove(c)
    _mats.clear()   # the cache points at the materials we just deleted, so it must be reset too

def build(data):
    W, H = data["width"], data["height"]

    clear_scene()

    items = []
    for i, s in enumerate(data["shapes"]):
        polys = shape_polys(i, s)
        if polys:
            items.append((i, s, polys))

    if ONE_MESH_PER_FRAME:
        # Opaque shapes go into one mesh. Translucent ones stay separate objects: Blender sorts
        # transparency per object, so merging them would blend in the wrong order.
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

    sc = bpy.context.scene
    sc.camera = cam
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
    return sc

_NUM = re.compile(r"f_(\d+)\.json$", re.IGNORECASE)

def list_frames():
    """Find every f_NNNN.json and return (number, digits_as_written, path), sorted by number.
    Numbers with no JSON (the gust frames) simply don't appear, so they are skipped."""
    frames = []
    for path in glob.glob(absp(JSON_GLOB)):
        m = _NUM.search(os.path.basename(path))
        if not m:
            print(f"[warn] ignoring {path}: name is not f_NNNN.json")
            continue
        frames.append((int(m.group(1)), m.group(1), path))
    frames.sort()
    return frames

def fmt_gaps(numbers):
    """Human-readable list of the frame numbers missing between the first and last JSON."""
    gaps = [(a + 1, b - 1) for a, b in zip(numbers, numbers[1:]) if b - a > 1]
    if not gaps:
        return "none"
    return ", ".join(f"{a}" if a == b else f"{a}-{b}" for a, b in gaps)

# ---- main ----
# Drop the frame-change handler the old timeline script may have left registered in this Blender session
for h in list(bpy.app.handlers.frame_change_pre):
    if getattr(h, "__name__", "") == "_on_frame":
        bpy.app.handlers.frame_change_pre.remove(h)

if bpy.data.filepath and not bpy.app.background:
    print(f"[warn] this script deletes every object in the open file ({bpy.data.filepath}). "
          f"Don't save it afterwards; use File > New first if it holds work you care about.")

all_frames = list_frames()
if not all_frames:
    # RuntimeError (not SystemExit) so Blender shows an error instead of closing itself
    raise RuntimeError(f"No f_NNNN.json files matched: {absp(JSON_GLOB)}  (run ./bench --frames ... --json first)")

print(f"Found {len(all_frames)} JSON frames, numbers {all_frames[0][0]}-{all_frames[-1][0]}")
print(f"Missing numbers (no JSON, e.g. gust frames): {fmt_gaps([n for n, _, _ in all_frames])}")
print(f"Settings: ONE_MESH_PER_FRAME={ONE_MESH_PER_FRAME}, RESOLUTION_PERCENTAGE={RESOLUTION_PERCENTAGE}, MAX_FRAMES={MAX_FRAMES}")

frames = all_frames[SKIP_FIRST:]
if MAX_FRAMES is not None:
    frames = frames[:MAX_FRAMES]
if not frames:
    raise RuntimeError("SKIP_FIRST / MAX_FRAMES left no frames to render")

test_run = MAX_FRAMES is not None
out_dir = os.path.join(OUT_DIR, "test") if test_run else OUT_DIR
os.makedirs(out_dir, exist_ok=True)
print(f"Output folder: {out_dir}")

times, failed, skipped = [], [], 0
for k, (num, digits, path) in enumerate(frames):
    out_png = os.path.join(out_dir, f"f_{digits}.png")    # same number as f_NNNN.json

    if SKIP_EXISTING and not test_run and os.path.exists(out_png):
        skipped += 1
        continue
    t0 = time.perf_counter()
    try:
        with open(path) as fh:
            data = json.load(fh)
        sc = build(data)
        sc.render.filepath = out_png
        bpy.ops.render.render(write_still=True)
    except Exception as e:
        failed.append((path, e))
        print(f"[error] {os.path.basename(path)}: {e}")
        continue
    dt = time.perf_counter() - t0
    times.append(dt)
    print(f"[{k + 1}/{len(frames)}] {os.path.basename(path)} -> {out_png}   {dt:.2f}s")

if times:
    first, rest = times[0], times[1:]
    avg = sum(rest) / len(rest) if rest else first
    rr = bpy.context.scene.render
    print("---- timing ----")
    print(f"settings        : ONE_MESH_PER_FRAME={ONE_MESH_PER_FRAME}, engine={rr.engine}, "
          f"output {rr.resolution_x * rr.resolution_percentage // 100}x{rr.resolution_y * rr.resolution_percentage // 100}, "
          f"headless={bpy.app.background}")
    print(f"frames rendered : {len(times)}  (skipped existing: {skipped}, failed: {len(failed)})")
    print(f"first frame     : {first:.2f}s  (includes startup)")
    print(f"average, others : {avg:.2f}s/frame")
    print(f"total           : {sum(times):.1f}s")
    print(f"full render est.: {avg * len(all_frames) / 60:.1f} min for {len(all_frames)} JSON frames")

if failed:
    raise RuntimeError(f"{len(failed)} frames failed, first: {failed[0][0]}: {failed[0][1]}")
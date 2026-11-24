import bpy, json, math, sys, os, glob, re, time
from mathutils import Vector

# Reads out/frames3d/f_NNNN.json and writes out/blender3d/f_NNNN.png with the SAME number.
# Missing frame numbers (the transition intervals) are rendered in Blender as smooth fades.

# ---------------- config ----------------
_txt = getattr(getattr(bpy.context, "space_data", None), "text", None)
_src = bpy.path.abspath(_txt.filepath) if _txt and _txt.filepath else __file__
BASE = os.path.dirname(os.path.dirname(os.path.abspath(_src)))

JSON_GLOB   = "out/frames3d/f_*.json"
OUT_DIR     = os.environ.get("BENCH3D_RENDER_OUT", os.path.join(BASE, "out", "blender3d"))

SKIP_FIRST    = 0
MAX_FRAMES    = None
SKIP_EXISTING = True

# Fade mode for the gaps between scenes:
#   "CROSSFADE"  = smooth dissolve from the end of Scene N into the start of Scene N+1
#   "FADE_BLACK" = smooth fade to black halfway through the gap, then fade in Scene N+1
FADE_MODE     = "CROSSFADE"

RESOLUTION_PERCENTAGE = 100   # 100 = 1280x720
# ----------------------------------------

if "--" in sys.argv:
    extra = sys.argv[sys.argv.index("--") + 1:]
    if len(extra) >= 1: JSON_GLOB = extra[0]
    if len(extra) >= 2: OUT_DIR = extra[1]

def absp(p):
    return p if os.path.isabs(p) else os.path.join(BASE, p)

def gl_to_blender(x, y, z):
    # OpenGL: X right, Y up, Z toward camera (+Z front, -Z back)
    # Blender: X right, Y depth (+Y toward camera, -Y back), Z up
    return (x, z, y)

def to_linear(c):
    c = max(0.0, min(1.0, c))
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

def clear_scene():
    for o in list(bpy.data.objects):   bpy.data.objects.remove(o, do_unlink=True)
    for m in list(bpy.data.meshes):    bpy.data.meshes.remove(m)
    for m in list(bpy.data.materials): bpy.data.materials.remove(m)
    for c in list(bpy.data.cameras):   bpy.data.cameras.remove(c)
    for l in list(bpy.data.lights):    bpy.data.lights.remove(l)
    for img in list(bpy.data.images):  bpy.data.images.remove(img)

def configure_render(sc, W, H):
    r = sc.render
    r.resolution_x = W
    r.resolution_y = H
    r.resolution_percentage = RESOLUTION_PERCENTAGE
    r.image_settings.file_format = 'PNG'
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "CYCLES"):
        try:
            r.engine = eng
            break
        except TypeError:
            continue
    # Enable soft shadows in Eevee if available
    if hasattr(sc, "eevee"):
        try:
            sc.eevee.use_soft_shadows = True
        except Exception:
            pass
    try:
        sc.view_settings.view_transform = 'AgX'
        sc.view_settings.look = 'AgX - Medium High Contrast'
    except Exception:
        try:
            sc.view_settings.view_transform = 'Filmic'
        except Exception:
            pass

def create_lit_3d_material(name, is_transparent=False):
    """Creates a true 3D Principled BSDF material driven by vertex colors + Emissive attribute."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True

    if is_transparent:
        try: mat.surface_render_method = 'BLENDED'
        except Exception: mat.blend_method = 'BLEND'
    else:
        try: mat.surface_render_method = 'DITHERED'
        except Exception: mat.blend_method = 'OPAQUE'

    nt = mat.node_tree
    nt.nodes.clear()

    out = nt.nodes.new("ShaderNodeOutputMaterial")

    attr_col = nt.nodes.new("ShaderNodeAttribute")
    attr_col.attribute_name = "Col"

    attr_emit = nt.nodes.new("ShaderNodeAttribute")
    attr_emit.attribute_name = "Emissive"

    # 3D shaded surface (Principled BSDF)
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Roughness"].default_value = 0.72
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.25
    nt.links.new(attr_col.outputs["Color"], bsdf.inputs["Base Color"])

    # Unlit emissive shader for sky, sun, clouds, glowing bulb, fireflies
    emit = nt.nodes.new("ShaderNodeEmission")
    emit.inputs["Strength"].default_value = 1.0
    nt.links.new(attr_col.outputs["Color"], emit.inputs["Color"])

    # Blend between 3D lit BSDF (emissive=0) and unlit Emission (emissive=1)
    mix_surf = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(attr_emit.outputs["Fac"], mix_surf.inputs[0])
    nt.links.new(bsdf.outputs["BSDF"], mix_surf.inputs[1])
    nt.links.new(emit.outputs["Emission"], mix_surf.inputs[2])

    if is_transparent:
        trans = nt.nodes.new("ShaderNodeBsdfTransparent")
        mix_alpha = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(attr_col.outputs["Alpha"], mix_alpha.inputs[0])
        nt.links.new(trans.outputs["BSDF"], mix_alpha.inputs[1])
        nt.links.new(mix_surf.outputs["Shader"], mix_alpha.inputs[2])
        nt.links.new(mix_alpha.outputs["Shader"], out.inputs["Surface"])
    else:
        nt.links.new(mix_surf.outputs["Shader"], out.inputs["Surface"])

    return mat

def build_mesh_object(name, tris, mat):
    """Builds a clean Blender mesh object from a list of triangle vertex tuples."""
    if not tris:
        return None
    num_tris = len(tris)
    coords = []
    normals = []
    flat_colors = []
    flat_emit = []

    for tri in tris:
        for v in tri:
            coords.append(gl_to_blender(v[0], v[1], v[2]))
            normals.append(gl_to_blender(v[3], v[4], v[5]))
            flat_colors.extend((to_linear(v[6]), to_linear(v[7]), to_linear(v[8]), v[9]))
            flat_emit.append(v[10])

    faces = [(i * 3, i * 3 + 1, i * 3 + 2) for i in range(num_tris)]

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(coords, [], faces)

    # Smooth normals across polygons while respecting custom split normals
    for poly in mesh.polygons:
        poly.use_smooth = True

    if len(normals) == len(mesh.loops):
        mesh.normals_split_custom_set(normals)

    col_attr = mesh.color_attributes.new(name="Col", type='FLOAT_COLOR', domain='CORNER')
    emit_attr = mesh.attributes.new(name="Emissive", type='FLOAT', domain='CORNER')
    col_attr.data.foreach_set("color", flat_colors)
    emit_attr.data.foreach_set("value", flat_emit)

    mesh.update()
    mesh.materials.append(mat)

    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj

def build_3d(data):
    W, H = data["width"], data["height"]
    clear_scene()
    sc = bpy.context.scene
    sc.use_nodes = False

    verts_raw = data["vertices"]
    num_tris = len(verts_raw) // 3

    opaque_tris = []
    trans_tris = []

    for i in range(num_tris):
        v0, v1, v2 = verts_raw[i * 3], verts_raw[i * 3 + 1], verts_raw[i * 3 + 2]
        # Skip degenerate zero-area triangles so Blender never drops faces
        if (v0[0] == v1[0] and v0[1] == v1[1] and v0[2] == v1[2]) or \
           (v1[0] == v2[0] and v1[1] == v2[1] and v1[2] == v2[2]) or \
           (v0[0] == v2[0] and v0[1] == v2[1] and v0[2] == v2[2]):
            continue
        min_alpha = min(v0[9], v1[9], v2[9])
        if min_alpha < 0.99:
            trans_tris.append((v0, v1, v2))
        else:
            opaque_tris.append((v0, v1, v2))

    opaque_mat = create_lit_3d_material("Opaque3DMat", is_transparent=False)
    trans_mat  = create_lit_3d_material("Trans3DMat",  is_transparent=True)

    build_mesh_object("Scene3D_Opaque", opaque_tris, opaque_mat)
    build_mesh_object("Scene3D_Trans",  trans_tris,  trans_mat)

    # 1. Ambient World Light (matches uAmbient)
    if sc.world is None:
        sc.world = bpy.data.worlds.new("World")
    sc.world.use_nodes = True
    bg = sc.world.node_tree.nodes.get("Background")
    if bg:
        sr, sg, sb = data["skyColor"]
        amb = float(data["ambient"])
        bg.inputs[0].default_value = (to_linear(sr), to_linear(sg), to_linear(sb), 1.0)
        bg.inputs[1].default_value = max(0.05, amb * 1.35)

    # 2. Directional Sun Light (matches uSunDirection & uSunlight)
    sun_strength = float(data["sunlight"])
    if sun_strength > 0.001:
        sun_data = bpy.data.lights.new(name="SunLight", type='SUN')
        sun_data.energy = sun_strength * 3.2
        sun_data.color = (1.0, 0.97, 0.92)
        sun_data.angle = math.radians(4.0)  # soft natural shadows
        sun_obj = bpy.data.objects.new(name="SunLight", object_data=sun_data)
        bpy.context.collection.objects.link(sun_obj)

        sx, sy, sz = data["sunDir"]
        sun_dir_bl = Vector(gl_to_blender(sx, sy, sz)).normalized()
        # Sun light shines along -Z of the object, so point -Z opposite to sun_dir_bl
        sun_obj.rotation_euler = (-sun_dir_bl).to_track_quat('-Z', 'Y').to_euler()

    # 3. Warm Street Lamp Point Light (matches kLampBulbPos & uLampPower)
    lamp_power = float(data["lampPower"])
    if lamp_power > 0.001:
        lamp_data = bpy.data.lights.new(name="StreetLamp", type='POINT')
        lamp_data.energy = lamp_power * 45.0
        lamp_data.color = (1.0, 0.76, 0.44)
        lamp_data.shadow_soft_size = 0.25
        lamp_obj = bpy.data.objects.new(name="StreetLamp", object_data=lamp_data)
        lx, ly, lz = data["lampPos"]
        lamp_obj.location = gl_to_blender(lx, ly, lz)
        bpy.context.collection.objects.link(lamp_obj)

    # 4. Perspective Camera (looking from +Y toward -Y, Z up)
    cam_data = bpy.data.cameras.new("Cam3D")
    cam_data.type = 'PERSP'
    cam_data.lens_unit = 'FOV'
    cam_data.angle = 0.785398
    cam_data.sensor_fit = 'VERTICAL'
    cam_data.clip_start = 0.1
    cam_data.clip_end = 100.0

    cam = bpy.data.objects.new("Cam3D", cam_data)
    yaw, pitch, dist = 0.0, 0.09, 17.5
    tx, ty, tz = -0.25, 1.45, -0.35
    ex = tx + math.sin(yaw) * dist * math.cos(pitch)
    ey = ty + math.sin(pitch) * dist
    ez = tz + math.cos(yaw) * dist * math.cos(pitch)

    eye_bl = Vector(gl_to_blender(ex, ey, ez))
    tgt_bl = Vector(gl_to_blender(tx, ty, tz))
    cam.location = eye_bl
    direction = (tgt_bl - eye_bl).normalized()
    # Point camera -Z along direction, with +Z as world up
    cam.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()

    bpy.context.collection.objects.link(cam)
    sc.camera = cam

    configure_render(sc, W, H)
    return sc

def smoothstep(u):
    u = max(0.0, min(1.0, u))
    return u * u * (3.0 - 2.0 * u)

def render_fade_gap(out_dir, img_a_path, img_b_path, gap_numbers, pad_len, mode="CROSSFADE"):
    """Blends img_a_path into img_b_path across gap_numbers using Blender's Compositor."""
    clear_scene()
    sc = bpy.context.scene
    sc.use_nodes = True
    nt = sc.node_tree
    nt.nodes.clear()

    img_a = bpy.data.images.load(img_a_path)
    img_b = bpy.data.images.load(img_b_path)
    W, H = img_a.size[0], img_a.size[1]
    configure_render(sc, W, H)

    n_a = nt.nodes.new("CompositorNodeImage")
    n_a.image = img_a
    n_b = nt.nodes.new("CompositorNodeImage")
    n_b.image = img_b
    n_out = nt.nodes.new("CompositorNodeComposite")

    total = len(gap_numbers)
    if mode == "FADE_BLACK":
        n_black = nt.nodes.new("CompositorNodeRGB")
        n_black.outputs[0].default_value = (0.0, 0.0, 0.0, 1.0)
        n_mix = nt.nodes.new("CompositorNodeMixRGB")
        n_mix.blend_type = 'MIX'

        for idx, frame_num in enumerate(gap_numbers):
            out_png = os.path.join(out_dir, f"f_{str(frame_num).zfill(pad_len)}.png")
            if SKIP_EXISTING and os.path.exists(out_png):
                continue
            u = (idx + 1) / (total + 1)
            nt.links.clear()
            nt.links.new(n_mix.outputs[0], n_out.inputs[0])
            if u < 0.5:
                n_mix.inputs[0].default_value = smoothstep(u * 2.0)
                nt.links.new(n_a.outputs[0], n_mix.inputs[1])
                nt.links.new(n_black.outputs[0], n_mix.inputs[2])
            else:
                n_mix.inputs[0].default_value = smoothstep((u - 0.5) * 2.0)
                nt.links.new(n_black.outputs[0], n_mix.inputs[1])
                nt.links.new(n_b.outputs[0], n_mix.inputs[2])
            sc.render.filepath = out_png
            bpy.ops.render.render(write_still=True)
    else:
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

def fmt_gaps(numbers):
    gaps = [(a + 1, b - 1) for a, b in zip(numbers, numbers[1:]) if b - a > 1]
    if not gaps:
        return "none"
    return ", ".join(f"{a}" if a == b else f"{a}-{b}" for a, b in gaps)

# ---- main ----
for h in list(bpy.app.handlers.frame_change_pre):
    if getattr(h, "__name__", "") == "_on_frame":
        bpy.app.handlers.frame_change_pre.remove(h)

if bpy.data.filepath and not bpy.app.background:
    print(f"[warn] this script deletes every object in the open file ({bpy.data.filepath}). "
          f"Don't save it afterwards; use File > New first if it holds work you care about.")

all_frames = list_frames()
if not all_frames:
    raise RuntimeError(f"No f_NNNN.json files matched: {absp(JSON_GLOB)}  (run ./bench3d --frames3d out/frames3d/f 24 first)")

print(f"Found {len(all_frames)} 3D JSON frames, numbers {all_frames[0][0]}-{all_frames[-1][0]}")
print(f"Transition gaps to fill with {FADE_MODE}: {fmt_gaps([n for n, _, _ in all_frames])}")

pad_len = len(all_frames[0][1])
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
    out_png = os.path.join(out_dir, f"f_{digits}.png")
    if SKIP_EXISTING and not test_run and os.path.exists(out_png):
        skipped += 1
        continue
    t0 = time.perf_counter()
    try:
        with open(path) as fh:
            data = json.load(fh)
        sc = build_3d(data)
        sc.render.filepath = out_png
        bpy.ops.render.render(write_still=True)
    except Exception as e:
        failed.append((path, e))
        print(f"[error] {os.path.basename(path)}: {e}")
        continue
    dt = time.perf_counter() - t0
    times.append(dt)
    print(f"[{k + 1}/{len(frames)}] {os.path.basename(path)} -> {out_png}   {dt:.2f}s")

if not test_run:
    for (num_a, dig_a, _), (num_b, dig_b, _) in zip(frames, frames[1:]):
        if num_b - num_a > 1:
            gap_nums = list(range(num_a + 1, num_b))
            img_a_path = os.path.join(out_dir, f"f_{dig_a}.png")
            img_b_path = os.path.join(out_dir, f"f_{dig_b}.png")
            if os.path.exists(img_a_path) and os.path.exists(img_b_path):
                print(f"Rendering {FADE_MODE} for transition frames {gap_nums[0]}-{gap_nums[-1]}...")
                render_fade_gap(out_dir, img_a_path, img_b_path, gap_nums, pad_len, mode=FADE_MODE)

if times:
    first, rest = times[0], times[1:]
    avg = sum(rest) / len(rest) if rest else first
    rr = bpy.context.scene.render
    print("---- timing ----")
    print(f"settings        : engine={rr.engine}, "
          f"output {rr.resolution_x * rr.resolution_percentage // 100}x{rr.resolution_y * rr.resolution_percentage // 100}, "
          f"headless={bpy.app.background}")
    print(f"frames rendered : {len(times)}  (skipped existing: {skipped}, failed: {len(failed)})")
    print(f"first frame     : {first:.2f}s  (includes startup)")
    print(f"average, others : {avg:.2f}s/frame")
    print(f"total           : {sum(times):.1f}s")

if failed:
    raise RuntimeError(f"{len(failed)} frames failed, first: {failed[0][0]}: {failed[0][1]}")
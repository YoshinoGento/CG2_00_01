import bpy
import math
import os

ROOT = r"C:\2026_10Days\project"
SOURCE_DIR = os.path.join(ROOT, "Assets", "Blender")
EXPORT_DIR = os.path.join(ROOT, "Resources", "stage")
RADIUS = 20.0
SEGMENTS = 128
SIZE = 512
os.makedirs(SOURCE_DIR, exist_ok=True)
os.makedirs(EXPORT_DIR, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)

def smoothstep(value):
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)

def segment_distance(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    length_squared = dx * dx + dy * dy
    t = 0.0 if length_squared == 0.0 else max(
        0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_squared))
    closest_x, closest_y = ax + dx * t, ay + dy * t
    return math.hypot(px - closest_x, py - closest_y)

def pole_letter_mask(px, py, letter):
    # Five simple strokes keep N/S readable even when the camera is zoomed out.
    strokes = []
    if letter == "N":
        strokes = [(-0.055, -0.09, -0.055, 0.09),
                   (0.055, -0.09, 0.055, 0.09),
                   (-0.055, 0.09, 0.055, -0.09)]
    else:
        strokes = [(-0.055, 0.09, 0.055, 0.09),
                   (-0.055, 0.0, 0.055, 0.0),
                   (-0.055, -0.09, 0.055, -0.09),
                   (-0.055, 0.0, -0.055, 0.09),
                   (0.055, -0.09, 0.055, 0.0)]
    return min(segment_distance(px, py, *stroke) for stroke in strokes) < 0.014

image = bpy.data.images.new("MagnetGroundTexture", width=SIZE, height=SIZE, alpha=True)
pixels = [0.0] * (SIZE * SIZE * 4)
blue, center, red = (0.31, 0.43, 0.58), (0.43, 0.40, 0.49), (0.58, 0.36, 0.40)
for y in range(SIZE):
    ny = ((y + 0.5) / SIZE) * 2.0 - 1.0
    for x in range(SIZE):
        u = (x + 0.5) / SIZE
        nx = u * 2.0 - 1.0
        if u < 0.5:
            t = smoothstep(u * 2.0)
            color = tuple(blue[i] * (1.0 - t) + center[i] * t for i in range(3))
        else:
            t = smoothstep((u - 0.5) * 2.0)
            color = tuple(center[i] * (1.0 - t) + red[i] * t for i in range(3))
        radius = math.sqrt(nx * nx + ny * ny)
        ring = 0.012 * math.cos(radius * math.pi * 10.0) * smoothstep(radius)
        edge = smoothstep((radius - 0.82) / 0.18)
        color = tuple(max(0.0, min(1.0, c + ring - edge * 0.055)) for c in color)

        # Clearly marked magnetic poles, kept translucent enough for gameplay.
        left_pole = smoothstep((0.25 - math.hypot(nx + 0.68, ny)) / 0.07)
        right_pole = smoothstep((0.25 - math.hypot(nx - 0.68, ny)) / 0.07)
        pole_strength = max(left_pole, right_pole) * 0.34
        pole_color = blue if left_pole >= right_pole else red
        color = tuple(c * (1.0 - pole_strength) + pole_color[i] * pole_strength
                      for i, c in enumerate(color))

        # Dipole-style contour lines communicate magnetic force without a hard seam.
        phase = math.atan2(ny, nx + 0.55) - math.atan2(ny, nx - 0.55)
        line_amount = 1.0 - smoothstep(abs(math.sin(phase * 3.5)) / 0.075)
        line_amount *= smoothstep((0.94 - radius) / 0.12) * 0.075
        color = tuple(c * (1.0 - line_amount) + 0.78 * line_amount for c in color)

        if pole_letter_mask(nx + 0.68, ny, "N") or pole_letter_mask(nx - 0.68, ny, "S"):
            color = tuple(c * 0.28 + 0.86 * 0.72 for c in color)
        offset = (y * SIZE + x) * 4
        pixels[offset:offset + 4] = (*color, 1.0)
image.pixels.foreach_set(pixels)
image.filepath_raw = os.path.join(EXPORT_DIR, "MagnetGround.png")
image.file_format = "PNG"
image.save()

vertices = [(0.0, 0.0, 0.0)]
uvs = [(0.5, 0.5)]
for index in range(SEGMENTS):
    angle = math.tau * index / SEGMENTS
    x, y = math.cos(angle) * RADIUS, math.sin(angle) * RADIUS
    vertices.append((x, y, 0.0))
    uvs.append((x / (RADIUS * 2.0) + 0.5, y / (RADIUS * 2.0) + 0.5))
faces = [(0, index + 1, (index + 1) % SEGMENTS + 1) for index in range(SEGMENTS)]
mesh = bpy.data.meshes.new("MagnetGroundMesh")
mesh.from_pydata(vertices, [], faces)
mesh.update()
ground = bpy.data.objects.new("MagnetGround", mesh)
bpy.context.collection.objects.link(ground)
uv_layer = mesh.uv_layers.new(name="UVMap")
for polygon in mesh.polygons:
    for loop_index in polygon.loop_indices:
        uv_layer.data[loop_index].uv = uvs[mesh.loops[loop_index].vertex_index]
material = bpy.data.materials.new("MagnetGroundMaterial")
material.use_nodes = True
nodes, links = material.node_tree.nodes, material.node_tree.links
principled = next(node for node in nodes if node.type == "BSDF_PRINCIPLED")
texture = nodes.new("ShaderNodeTexImage")
texture.image = image
links.new(texture.outputs["Color"], principled.inputs["Base Color"])
principled.inputs["Roughness"].default_value = 0.82
mesh.materials.append(material)

# A shallow raised ring gives the arena a manufactured magnet-device silhouette.
rim_vertices, rim_uvs, rim_faces = [], [], []
for index in range(SEGMENTS):
    angle = math.tau * index / SEGMENTS
    for radius in (RADIUS - 0.65, RADIUS):
        x, y = math.cos(angle) * radius, math.sin(angle) * radius
        rim_vertices.append((x, y, 0.055))
        rim_uvs.append((x / (RADIUS * 2.0) + 0.5, y / (RADIUS * 2.0) + 0.5))
for index in range(SEGMENTS):
    next_index = (index + 1) % SEGMENTS
    rim_faces.append((index * 2, next_index * 2, next_index * 2 + 1, index * 2 + 1))
rim_mesh = bpy.data.meshes.new("MagnetGroundRimMesh")
rim_mesh.from_pydata(rim_vertices, [], rim_faces)
rim_mesh.update()
rim = bpy.data.objects.new("MagnetGroundRim", rim_mesh)
bpy.context.collection.objects.link(rim)
rim_layer = rim_mesh.uv_layers.new(name="UVMap")
for polygon in rim_mesh.polygons:
    for loop_index in polygon.loop_indices:
        rim_layer.data[loop_index].uv = rim_uvs[rim_mesh.loops[loop_index].vertex_index]
rim_mesh.materials.append(material)
rim.select_set(True)
ground.select_set(True)
bpy.context.view_layer.objects.active = ground
bpy.ops.object.shade_smooth()
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(SOURCE_DIR, "MagnetGround.blend"))
bpy.ops.wm.obj_export(
    filepath=os.path.join(EXPORT_DIR, "MagnetGround.obj"),
    export_selected_objects=True, export_materials=True,
    export_uv=True, export_normals=True,
    forward_axis="NEGATIVE_Z", up_axis="Y")

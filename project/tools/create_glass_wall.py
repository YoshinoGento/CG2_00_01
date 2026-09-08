import bpy
import math
import os

ROOT = r"C:\2026_10Days\project"
SOURCE_DIR = os.path.join(ROOT, "Assets", "Blender")
EXPORT_DIR = os.path.join(ROOT, "Resources", "stage")
RADIUS = 20.0
HEIGHT = 1.4
SEGMENTS = 128

os.makedirs(SOURCE_DIR, exist_ok=True)
os.makedirs(EXPORT_DIR, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)

# Blender's native Z-up cylinder exports as the game's Y-up glass wall.
vertices = []
uvs = []
for index in range(SEGMENTS):
    angle = math.tau * index / SEGMENTS
    x, y = math.cos(angle) * RADIUS, math.sin(angle) * RADIUS
    vertices.extend(((x, y, 0.0), (x, y, HEIGHT)))
    u = index / SEGMENTS
    uvs.extend(((u, 0.0), (u, 1.0)))

faces = []
for index in range(SEGMENTS):
    next_index = (index + 1) % SEGMENTS
    faces.append((index * 2, next_index * 2, next_index * 2 + 1, index * 2 + 1))

mesh = bpy.data.meshes.new("ArenaGlassWallMesh")
mesh.from_pydata(vertices, [], faces)
mesh.update()
wall = bpy.data.objects.new("ArenaGlassWall", mesh)
bpy.context.collection.objects.link(wall)

uv_layer = mesh.uv_layers.new(name="UVMap")
for polygon in mesh.polygons:
    for loop_index in polygon.loop_indices:
        uv_layer.data[loop_index].uv = uvs[mesh.loops[loop_index].vertex_index]

# A mostly transparent blue glass texture with a gentle vertical sheen.
width, height = 256, 64
image = bpy.data.images.new("ArenaGlassTexture", width=width, height=height, alpha=True)
pixels = [0.0] * (width * height * 4)
for py in range(height):
    v = (py + 0.5) / height
    sheen = math.exp(-((v - 0.68) / 0.17) ** 2)
    for px in range(width):
        glint = math.exp(-((((px + 0.5) / width) - 0.30) / 0.035) ** 2)
        alpha = 0.15 + sheen * 0.07 + glint * 0.08
        offset = (py * width + px) * 4
        pixels[offset:offset + 4] = (0.46 + sheen * 0.12, 0.78 + sheen * 0.10, 0.90, alpha)
image.pixels.foreach_set(pixels)
image.filepath_raw = os.path.join(EXPORT_DIR, "ArenaGlass.png")
image.file_format = "PNG"
image.save()

material = bpy.data.materials.new("ArenaGlassMaterial")
material.use_nodes = True
material.diffuse_color = (0.46, 0.78, 0.90, 0.20)
material.surface_render_method = "DITHERED"
nodes, links = material.node_tree.nodes, material.node_tree.links
principled = next(node for node in nodes if node.type == "BSDF_PRINCIPLED")
texture = nodes.new("ShaderNodeTexImage")
texture.image = image
links.new(texture.outputs["Color"], principled.inputs["Base Color"])
links.new(texture.outputs["Alpha"], principled.inputs["Alpha"])
principled.inputs["Roughness"].default_value = 0.12
principled.inputs["Metallic"].default_value = 0.04
mesh.materials.append(material)

wall.select_set(True)
bpy.context.view_layer.objects.active = wall
bpy.ops.object.shade_smooth()
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(SOURCE_DIR, "ArenaGlassWall.blend"))
bpy.ops.wm.obj_export(
    filepath=os.path.join(EXPORT_DIR, "ArenaGlassWall.obj"),
    export_selected_objects=True, export_materials=True,
    export_uv=True, export_normals=True,
    forward_axis="NEGATIVE_Z", up_axis="Y")

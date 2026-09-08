from __future__ import annotations

import math
from pathlib import Path

import bpy
from mathutils import Vector


OUTPUT_DIR = Path(__file__).resolve().parent
BODY_OBJECTS: list[bpy.types.Object] = []
CHAIN_OBJECTS: list[bpy.types.Object] = []
CHAIN_FRAME_COUNT = 4
CHAIN_TOOTH_COUNT = 24


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for datablocks in (bpy.data.meshes, bpy.data.curves, bpy.data.materials,
                       bpy.data.cameras, bpy.data.lights):
        for datablock in list(datablocks):
            if datablock.users == 0:
                datablocks.remove(datablock)


def write_texture(name: str, width: int, height: int, pixel_function) -> bpy.types.Image:
    image = bpy.data.images.new(name, width=width, height=height, alpha=True)
    pixels: list[float] = []
    for y in range(height):
        for x in range(width):
            pixels.extend(pixel_function(x, y, width, height))
    image.pixels.foreach_set(pixels)
    image.filepath_raw = str(OUTPUT_DIR / f"{name}.png")
    image.file_format = "PNG"
    image.save()
    return image


def solid_pixel(color):
    return lambda _x, _y, _w, _h: color


def chain_pixel(x: int, y: int, width: int, height: int):
    period = max(1, width // 8)
    cell = (x // period) % 2
    edge = y < height * 0.18 or y > height * 0.82
    diagonal = ((x + y * 2) // max(1, period // 2)) % 2 == 0
    if edge:
        return (0.10, 0.11, 0.13, 1.0)
    if cell == 0:
        return (0.72, 0.78, 0.84, 1.0) if diagonal else (0.30, 0.34, 0.39, 1.0)
    return (1.0, 0.26, 0.035, 1.0) if diagonal else (0.20, 0.22, 0.25, 1.0)


def make_material(name: str, image: bpy.types.Image, metallic: float, roughness: float,
                  emission_strength: float = 0.0) -> bpy.types.Material:
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    texture = nodes.new("ShaderNodeTexImage")
    texture.image = image
    texture.interpolation = "Closest"
    texture.extension = "REPEAT"
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = roughness
    links.new(texture.outputs["Color"], shader.inputs["Base Color"])
    if emission_strength > 0.0:
        links.new(texture.outputs["Color"], shader.inputs["Emission Color"])
        shader.inputs["Emission Strength"].default_value = emission_strength
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])
    return material


def apply_mesh_finish(obj: bpy.types.Object, bevel: float = 0.0) -> None:
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if bevel > 0.0:
        modifier = obj.modifiers.new("EdgeSoftening", "BEVEL")
        modifier.width = bevel
        modifier.segments = 2
        modifier.limit_method = "ANGLE"
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    bpy.ops.object.shade_smooth_by_angle()
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66.0), island_margin=0.02)
    bpy.ops.object.mode_set(mode="OBJECT")
    obj.select_set(False)


def create_box(name: str, location, dimensions, material: bpy.types.Material,
               bevel: float = 0.03, rotation=(0.0, 0.0, 0.0), chain=False):
    bpy.ops.mesh.primitive_cube_add(location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    obj.data.materials.append(material)
    apply_mesh_finish(obj, bevel)
    (CHAIN_OBJECTS if chain else BODY_OBJECTS).append(obj)
    return obj


def create_capsule_rail(material: bpy.types.Material) -> None:
    left_x = -0.46
    tip_center_x = 1.22
    radius = 0.255
    half_depth = 0.095
    outline = [(left_x, radius), (tip_center_x, radius)]
    for segment in range(1, 13):
        angle = math.pi * 0.5 - math.pi * segment / 12.0
        outline.append((tip_center_x + math.cos(angle) * radius,
                        math.sin(angle) * radius))
    outline.append((left_x, -radius))

    vertices = []
    for y in (-half_depth, half_depth):
        vertices.extend((x, y, z) for x, z in outline)
    count = len(outline)
    faces = []
    faces.append(tuple(range(count)))
    faces.append(tuple(range(count, count * 2)))
    for index in range(count):
        next_index = (index + 1) % count
        faces.append((index, next_index, count + next_index, count + index))
    mesh = bpy.data.meshes.new("BladeRailMesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(material)
    obj = bpy.data.objects.new("BladeRail", mesh)
    bpy.context.collection.objects.link(obj)
    apply_mesh_finish(obj, 0.025)
    BODY_OBJECTS.append(obj)


def capsule_path(left_center: float, right_center: float, radius: float,
                 arc_segments: int = 12):
    points = [(left_center, radius), (right_center, radius)]
    for segment in range(1, arc_segments + 1):
        angle = math.pi * 0.5 - math.pi * segment / arc_segments
        points.append((right_center + math.cos(angle) * radius,
                       math.sin(angle) * radius))
    points.append((left_center, -radius))
    for segment in range(1, arc_segments + 1):
        angle = -math.pi * 0.5 - math.pi * segment / arc_segments
        points.append((left_center + math.cos(angle) * radius,
                       math.sin(angle) * radius))
    return points


def sample_closed_path(path, cumulative, perimeter: float, distance: float):
    distance %= perimeter
    count = len(path)
    for index in range(count):
        segment_start = cumulative[index]
        segment_end = cumulative[index + 1] if index + 1 < count else perimeter
        if distance > segment_end:
            continue
        start_x, start_z = path[index]
        end_x, end_z = path[(index + 1) % count]
        segment_length = max(segment_end - segment_start, 1.0e-6)
        ratio = (distance - segment_start) / segment_length
        tangent_x = (end_x - start_x) / segment_length
        tangent_z = (end_z - start_z) / segment_length
        return (
            start_x + (end_x - start_x) * ratio,
            start_z + (end_z - start_z) * ratio,
            tangent_x,
            tangent_z,
        )
    raise RuntimeError("Failed to sample closed chain path")


def create_chain_loop(material: bpy.types.Material,
                      tooth_material: bpy.types.Material,
                      frame_index: int) -> None:
    path = capsule_path(-0.31, 1.20, 0.315, 14)
    count = len(path)
    half_width = 0.058
    half_depth = 0.135
    cumulative = [0.0]
    for index in range(1, count):
        x0, z0 = path[index - 1]
        x1, z1 = path[index]
        cumulative.append(cumulative[-1] + math.hypot(x1 - x0, z1 - z0))
    closing = math.hypot(path[0][0] - path[-1][0], path[0][1] - path[-1][1])
    perimeter = cumulative[-1] + closing

    vertices = []
    uvs = []
    for y in (-half_depth, half_depth):
        for index, (x, z) in enumerate(path):
            prev_x, prev_z = path[(index - 1) % count]
            next_x, next_z = path[(index + 1) % count]
            tangent_x = next_x - prev_x
            tangent_z = next_z - prev_z
            tangent_length = max(math.hypot(tangent_x, tangent_z), 1.0e-6)
            tangent_x /= tangent_length
            tangent_z /= tangent_length
            normal_x = -tangent_z
            normal_z = tangent_x
            u = cumulative[index] / max(perimeter, 1.0e-6) * 12.0
            vertices.append((x + normal_x * half_width, y, z + normal_z * half_width))
            vertices.append((x - normal_x * half_width, y, z - normal_z * half_width))
            uvs.extend(((u, 1.0), (u, 0.0)))

    layer_stride = count * 2
    faces = []
    face_uvs = []
    for index in range(count):
        next_index = (index + 1) % count
        outer_front = index * 2
        inner_front = outer_front + 1
        outer_next_front = next_index * 2
        inner_next_front = outer_next_front + 1
        outer_back = layer_stride + outer_front
        inner_back = layer_stride + inner_front
        outer_next_back = layer_stride + outer_next_front
        inner_next_back = layer_stride + inner_next_front
        quad_sets = [
            (outer_front, outer_next_front, inner_next_front, inner_front),
            (outer_back, inner_back, inner_next_back, outer_next_back),
            (outer_front, outer_back, outer_next_back, outer_next_front),
            (inner_front, inner_next_front, inner_next_back, inner_back),
        ]
        faces.extend(quad_sets)
        face_uvs.extend(quad_sets)

    mesh = bpy.data.meshes.new(f"AnimatedChainLoopMesh_{frame_index}")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(material)
    uv_layer = mesh.uv_layers.new(name="ChainUV")
    for polygon, uv_indices in zip(mesh.polygons, face_uvs):
        for loop_index, vertex_index in zip(polygon.loop_indices, uv_indices):
            uv_layer.data[loop_index].uv = uvs[vertex_index]
    obj = bpy.data.objects.new(f"AnimatedChainLoop_{frame_index}", mesh)
    bpy.context.collection.objects.link(obj)
    CHAIN_OBJECTS.append(obj)

    frame_offset = frame_index / CHAIN_FRAME_COUNT
    for tooth_index in range(CHAIN_TOOTH_COUNT):
        distance = (tooth_index + frame_offset) / CHAIN_TOOTH_COUNT * perimeter
        x, z, tangent_x, tangent_z = sample_closed_path(
            path, cumulative, perimeter, distance)
        normal_x = -tangent_z
        normal_z = tangent_x
        tooth_x = x + normal_x * 0.076
        tooth_z = z + normal_z * 0.076
        angle_y = math.atan2(-tangent_z, tangent_x)
        create_box(
            f"MovingTooth_{frame_index}_{tooth_index:02d}",
            (tooth_x, 0.0, tooth_z),
            (0.055, 0.08, 0.045),
            tooth_material,
            bevel=0.0,
            rotation=(0.0, angle_y, 0.0),
            chain=True,
        )


def join_objects(objects: list[bpy.types.Object], name: str) -> bpy.types.Object:
    if not objects:
        raise RuntimeError(f"Cannot join empty object group: {name}")
    if len(objects) == 1:
        result = objects[0]
        result.name = name
        result.data.name = f"{name}Mesh"
        return result
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    result = bpy.context.object
    result.name = name
    result.data.name = f"{name}Mesh"
    result.select_set(False)
    return result


def rotate_blade_vertical(objects: list[bpy.types.Object]) -> None:
    for obj in objects:
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        obj.rotation_euler[1] = -math.pi * 0.5
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
        obj.select_set(False)


def normalize_runtime_bounds(objects: list[bpy.types.Object]) -> None:
    coordinates = [vertex.co.copy() for obj in objects for vertex in obj.data.vertices]
    minimum = Vector((
        min(value.x for value in coordinates),
        min(value.y for value in coordinates),
        min(value.z for value in coordinates),
    ))
    maximum = Vector((
        max(value.x for value in coordinates),
        max(value.y for value in coordinates),
        max(value.z for value in coordinates),
    ))
    center = (minimum + maximum) * 0.5
    source_size = maximum - minimum
    # Blender is Z-up. OBJ export converts this to an engine Y-up source whose
    # complete blade bounds are X=0.65, Y=3.0, Z=0.35.
    target_size = Vector((0.65, 0.35, 3.0))
    scale = Vector((
        target_size.x / source_size.x,
        target_size.y / source_size.y,
        target_size.z / source_size.z,
    ))
    for obj in objects:
        for vertex in obj.data.vertices:
            vertex.co = Vector((
                (vertex.co.x - center.x) * scale.x,
                (vertex.co.y - center.y) * scale.y,
                (vertex.co.z - center.z) * scale.z,
            ))


def export_obj(obj: bpy.types.Object, filename: str) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.wm.obj_export(
        filepath=str(OUTPUT_DIR / filename),
        export_selected_objects=True,
        export_materials=True,
        apply_modifiers=True,
        export_triangulated_mesh=True,
        forward_axis="NEGATIVE_Z",
        up_axis="Y",
    )
    obj.select_set(False)


def point_camera(camera: bpy.types.Object, target=(0.0, 0.0, 0.0)) -> None:
    direction = Vector(target) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def add_preview_scene() -> None:
    world = bpy.context.scene.world
    world.color = (0.006, 0.009, 0.016)
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.006, 0.009, 0.018, 1.0)
    background.inputs["Strength"].default_value = 0.14

    bpy.ops.object.camera_add(location=(3.4, -6.5, 2.5))
    camera = bpy.context.object
    camera.name = "PreviewCamera"
    point_camera(camera, (0.0, 0.0, 0.04))
    camera.data.lens = 58.0
    bpy.context.scene.camera = camera

    for name, location, energy, color, size in (
        ("KeyLight", (-1.0, -3.0, 4.5), 1050.0, (1.0, 0.20, 0.05), 4.0),
        ("RimLight", (3.5, 1.5, 3.0), 950.0, (0.10, 0.55, 1.0), 3.0),
        ("FillLight", (-3.0, 1.0, 1.0), 650.0, (0.5, 0.1, 1.0), 3.0),
    ):
        light_data = bpy.data.lights.new(name, "AREA")
        light_data.energy = energy
        light_data.color = color
        light_data.shape = "DISK"
        light_data.size = size
        light = bpy.data.objects.new(name, light_data)
        bpy.context.collection.objects.link(light)
        light.location = location
        point_camera(light, (0.0, 0.0, 0.0))

    # One fifth of the three-unit blade remains below this floor plane.
    bpy.ops.mesh.primitive_plane_add(size=20.0, location=(0.0, 0.0, -0.90))
    floor = bpy.context.object
    floor.name = "PreviewFloor"
    floor_material = bpy.data.materials.new("PreviewFloorMaterial")
    floor_material.diffuse_color = (0.015, 0.02, 0.03, 1.0)
    floor.data.materials.append(floor_material)

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 960
    scene.render.resolution_y = 540
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(OUTPUT_DIR / "chainsaw_preview.png")
    scene.render.film_transparent = False
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.render.image_settings.color_mode = "RGBA"


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    clear_scene()

    metal = write_texture("blade_metal", 8, 8, solid_pixel((0.68, 0.74, 0.80, 1.0)))
    chain = write_texture("chain_motion", 128, 32, chain_pixel)

    metal_material = make_material("BladeMetal", metal, 0.88, 0.16)
    tooth_material = make_material("ChainToothMetal", metal, 0.92, 0.12)
    chain_material = make_material("MovingChain", chain, 0.92, 0.12, 0.14)

    create_capsule_rail(metal_material)
    body = join_objects(BODY_OBJECTS, "ChainsawBody")

    chain_frames = []
    for frame_index in range(CHAIN_FRAME_COUNT):
        CHAIN_OBJECTS.clear()
        create_chain_loop(chain_material, tooth_material, frame_index)
        chain_frames.append(join_objects(
            CHAIN_OBJECTS, f"ChainsawChainFrame_{frame_index}"))

    rotate_blade_vertical([body, *chain_frames])
    normalize_runtime_bounds([body, *chain_frames])
    export_obj(body, "chainsaw.obj")
    for frame_index, chain_frame in enumerate(chain_frames):
        filename = "chainsaw_chain.obj" if frame_index == 0 \
            else f"chainsaw_chain_{frame_index}.obj"
        export_obj(chain_frame, filename)
        chain_frame.hide_render = frame_index != 0

    add_preview_scene()
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT_DIR / "chainsaw.blend"))
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    main()

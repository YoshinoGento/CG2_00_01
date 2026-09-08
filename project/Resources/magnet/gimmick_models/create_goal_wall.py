from __future__ import annotations

import math
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[1]
GOAL_DIR = ROOT / "goal"
WALL_DIR = ROOT / "wall"


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def write_solid_texture(directory: Path, name: str, color) -> bpy.types.Image:
    directory.mkdir(parents=True, exist_ok=True)
    image = bpy.data.images.new(name, width=8, height=8, alpha=True)
    image.pixels.foreach_set(list(color) * 64)
    image.filepath_raw = str(directory / f"{name}.png")
    image.file_format = "PNG"
    image.save()
    return image


def make_material(name: str, image: bpy.types.Image, metallic: float,
                  roughness: float, emission: float = 0.0) -> bpy.types.Material:
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
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = roughness
    if emission > 0.0:
        links.new(texture.outputs["Color"], shader.inputs["Emission Color"])
        shader.inputs["Emission Strength"].default_value = emission
    links.new(texture.outputs["Color"], shader.inputs["Base Color"])
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])
    return material


def finish_mesh(obj: bpy.types.Object, bevel: float = 0.0) -> bpy.types.Object:
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if bevel > 0.0:
        modifier = obj.modifiers.new("ReadableEdge", "BEVEL")
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
    return obj


def box(name: str, location, dimensions, material: bpy.types.Material,
        bevel: float = 0.02, rotation=(0.0, 0.0, 0.0)) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    obj.data.materials.append(material)
    return finish_mesh(obj, bevel)


def cylinder(name: str, location, radius: float, depth: float,
             material: bpy.types.Material, vertices: int = 32,
             rotation=(0.0, 0.0, 0.0)) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices, radius=radius, depth=depth,
        location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    return finish_mesh(obj, 0.008)


def torus(name: str, location, major_radius: float, minor_radius: float,
          material: bpy.types.Material,
          rotation=(0.0, 0.0, 0.0)) -> bpy.types.Object:
    bpy.ops.mesh.primitive_torus_add(
        major_radius=major_radius, minor_radius=minor_radius,
        major_segments=40, minor_segments=8,
        location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    return finish_mesh(obj)


def join_and_normalize(objects: list[bpy.types.Object], name: str) -> bpy.types.Object:
    if not objects:
        raise RuntimeError(f"empty object group: {name}")
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    result = bpy.context.object
    result.name = name
    result.data.name = f"{name}Mesh"

    coordinates = [vertex.co.copy() for vertex in result.data.vertices]
    minimum = Vector((
        min(coordinate.x for coordinate in coordinates),
        min(coordinate.y for coordinate in coordinates),
        min(coordinate.z for coordinate in coordinates),
    ))
    maximum = Vector((
        max(coordinate.x for coordinate in coordinates),
        max(coordinate.y for coordinate in coordinates),
        max(coordinate.z for coordinate in coordinates),
    ))
    dimensions = maximum - minimum
    if min(dimensions) <= 1.0e-6:
        raise RuntimeError(f"degenerate bounds: {name}")
    center = (minimum + maximum) * 0.5
    for vertex in result.data.vertices:
        vertex.co.x = (vertex.co.x - center.x) / dimensions.x
        vertex.co.y = (vertex.co.y - center.y) / dimensions.y
        vertex.co.z = (vertex.co.z - center.z) / dimensions.z
    result.data.update()
    result.select_set(False)
    return result


def export_obj(obj: bpy.types.Object, path: Path) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.wm.obj_export(
        filepath=str(path), export_selected_objects=True,
        export_materials=True, apply_modifiers=True,
        export_triangulated_mesh=True,
        forward_axis="NEGATIVE_Z", up_axis="Y")
    obj.select_set(False)


def point_at(obj: bpy.types.Object, target) -> None:
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def render_preview(directory: Path, filename: str, target, camera_location,
                   neutral_lighting: bool = False) -> None:
    world = bpy.context.scene.world
    if world is None:
        world = bpy.data.worlds.new("StructurePreviewWorld")
        bpy.context.scene.world = world
    world.use_nodes = True
    background = next(
        (node for node in world.node_tree.nodes if node.type == "BACKGROUND"), None)
    background.inputs["Color"].default_value = (0.003, 0.008, 0.018, 1.0)
    background.inputs["Strength"].default_value = 0.18

    bpy.ops.object.camera_add(location=camera_location)
    camera = bpy.context.object
    camera.name = "PreviewCamera"
    camera.data.lens = 58.0
    point_at(camera, target)
    bpy.context.scene.camera = camera

    lights = (
        ("NeutralKey", (-3.5, -4.0, 5.5), 1100.0, (0.92, 0.95, 1.0), 4.0),
        ("NeutralRim", (4.5, 1.0, 4.0), 780.0, (0.72, 0.78, 0.86), 3.0),
        ("NeutralFill", (0.0, -1.5, 7.0), 650.0, (0.82, 0.86, 0.92), 4.0),
    ) if neutral_lighting else (
        ("CyanKey", (-3.5, -4.0, 5.5), 1050.0, (0.05, 0.65, 1.0), 4.0),
        ("OrangeRim", (4.5, 1.0, 4.0), 850.0, (1.0, 0.15, 0.025), 3.0),
        ("WhiteFill", (0.0, -1.5, 7.0), 650.0, (0.72, 0.88, 1.0), 4.0),
    )
    for name, location, energy, color, size in lights:
        data = bpy.data.lights.new(name, "AREA")
        data.energy = energy
        data.color = color
        data.shape = "DISK"
        data.size = size
        light = bpy.data.objects.new(name, data)
        bpy.context.collection.objects.link(light)
        light.location = location
        point_at(light, target)

    bpy.ops.mesh.primitive_plane_add(size=20.0, location=(0.0, 0.0, -0.51))
    floor = bpy.context.object
    floor_material = bpy.data.materials.new("PreviewFloorMaterial")
    floor_material.diffuse_color = (0.008, 0.014, 0.028, 1.0)
    floor.data.materials.append(floor_material)

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 960
    scene.render.resolution_y = 540
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(directory / filename)
    scene.view_settings.look = "AgX - Medium High Contrast"
    bpy.ops.render.render(write_still=True)


def create_goal() -> None:
    reset_scene()
    GOAL_DIR.mkdir(parents=True, exist_ok=True)
    dark = write_solid_texture(GOAL_DIR, "goal_dark", (0.01, 0.025, 0.060, 1.0))
    cyan = write_solid_texture(GOAL_DIR, "goal_cyan", (0.01, 0.72, 1.0, 1.0))
    orange = write_solid_texture(GOAL_DIR, "goal_orange", (1.0, 0.12, 0.025, 1.0))
    mat_dark = make_material("GoalDarkMetal", dark, 0.82, 0.18)
    mat_cyan = make_material("GoalMagneticGuide", cyan, 0.34, 0.16, 1.4)
    mat_orange = make_material("GoalScoreCore", orange, 0.25, 0.16, 1.8)

    parts: list[bpy.types.Object] = []
    parts.append(box("LeftPylon", (-0.44, 0.0, 0.0), (0.12, 0.30, 0.88), mat_dark, 0.025))
    parts.append(box("RightPylon", (0.44, 0.0, 0.0), (0.12, 0.30, 0.88), mat_dark, 0.025))
    parts.append(box("TopBridge", (0.0, 0.0, 0.44), (1.0, 0.30, 0.12), mat_dark, 0.025))
    parts.append(box("FloorThreshold", (0.0, 0.0, -0.46), (1.0, 0.30, 0.08), mat_dark, 0.018))
    parts.append(box("LeftGuide", (-0.35, -0.17, 0.0), (0.035, 0.035, 0.68), mat_cyan, 0.008))
    parts.append(box("RightGuide", (0.35, -0.17, 0.0), (0.035, 0.035, 0.68), mat_cyan, 0.008))
    parts.append(box("TopGuide", (0.0, -0.17, 0.35), (0.68, 0.035, 0.035), mat_cyan, 0.008))
    parts.append(box("ThresholdGuide", (0.0, -0.17, -0.38), (0.68, 0.035, 0.025), mat_cyan, 0.006))
    parts.append(torus(
        "ScoreHalo", (0.0, -0.19, 0.42), 0.105, 0.024,
        mat_cyan, rotation=(math.pi * 0.5, 0.0, 0.0)))
    parts.append(cylinder(
        "ScoreCore", (0.0, -0.20, 0.42), 0.060, 0.055,
        mat_orange, 28, rotation=(math.pi * 0.5, 0.0, 0.0)))
    for side, x in enumerate((-0.47, 0.47)):
        for level, z in enumerate((-0.26, 0.0, 0.26)):
            parts.append(box(
                f"FluxCell_{side}_{level}", (x, -0.17, z),
                (0.055, 0.035, 0.095),
                mat_orange if level == 1 else mat_cyan, 0.008))

    model = join_and_normalize(parts, "Goal")
    export_obj(model, GOAL_DIR / "goal.obj")
    bpy.ops.wm.save_as_mainfile(filepath=str(GOAL_DIR / "goal.blend"))
    render_preview(GOAL_DIR, "goal_preview.png", (0.0, 0.0, 0.0), (2.3, -4.4, 2.1))


def create_wall() -> None:
    reset_scene()
    WALL_DIR.mkdir(parents=True, exist_ok=True)
    metal = write_solid_texture(
        WALL_DIR, "wall_metal", (0.70, 0.72, 0.75, 1.0))
    metal_light = write_solid_texture(
        WALL_DIR, "wall_metal_light", (0.78, 0.80, 0.83, 1.0))
    seam = write_solid_texture(
        WALL_DIR, "wall_seam", (0.025, 0.030, 0.035, 1.0))
    mat_metal = make_material("WallMetalPanel", metal, 0.58, 0.38)
    mat_metal_light = make_material(
        "WallMetalPanelLight", metal_light, 0.58, 0.36)
    mat_seam = make_material("WallNarrowJoint", seam, 0.38, 0.58)

    parts: list[bpy.types.Object] = []
    parts.append(box(
        "NarrowJointCore", (0.0, 0.0, 0.0),
        (0.996, 0.984, 0.996), mat_seam, 0.001))
    for face_index, face_y in enumerate((-0.496, 0.496)):
        for row, z in enumerate((-0.245, 0.245)):
            for column, x in enumerate((-0.33, 0.0, 0.33)):
                material = mat_metal_light if (row + column) % 2 == 0 else mat_metal
                parts.append(box(
                    f"FlatPanel_{face_index}_{row}_{column}",
                    (x, face_y, z),
                    (0.321, 0.008, 0.481), material, 0.001))
    for side, x in enumerate((-0.498, 0.498)):
        parts.append(box(
            f"PlainSide_{side}", (x, 0.0, 0.0),
            (0.004, 0.984, 0.996), mat_metal, 0.001))
    for cap, z in enumerate((-0.498, 0.498)):
        parts.append(box(
            f"PlainCap_{cap}", (0.0, 0.0, z),
            (0.996, 0.984, 0.004), mat_metal, 0.001))

    model = join_and_normalize(parts, "Wall")
    export_obj(model, WALL_DIR / "wall.obj")
    bpy.ops.wm.save_as_mainfile(filepath=str(WALL_DIR / "wall.blend"))
    render_preview(
        WALL_DIR, "wall_preview.png", (0.0, 0.0, 0.0),
        (2.2, -5.2, 1.35), neutral_lighting=True)


def main() -> None:
    create_goal()
    create_wall()


if __name__ == "__main__":
    main()

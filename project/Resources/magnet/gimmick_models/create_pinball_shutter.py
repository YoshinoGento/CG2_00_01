from __future__ import annotations

import math
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[1]
PINBALL_DIR = ROOT / "pinball"
SHUTTER_DIR = ROOT / "shutter"


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
        bevel: float = 0.03, rotation=(0.0, 0.0, 0.0)) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    obj.data.materials.append(material)
    return finish_mesh(obj, bevel)


def cylinder(name: str, location, radius: float, depth: float,
             material: bpy.types.Material, vertices: int = 48,
             rotation=(0.0, 0.0, 0.0)) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices, radius=radius, depth=depth,
        location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    return finish_mesh(obj, 0.012)


def torus(name: str, location, major_radius: float, minor_radius: float,
          material: bpy.types.Material, rotation=(0.0, 0.0, 0.0)) -> bpy.types.Object:
    bpy.ops.mesh.primitive_torus_add(
        major_radius=major_radius, minor_radius=minor_radius,
        major_segments=48, minor_segments=10,
        location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    return finish_mesh(obj)


def sphere(name: str, location, scale, material: bpy.types.Material) -> bpy.types.Object:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=20, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(material)
    return finish_mesh(obj)


def join(objects: list[bpy.types.Object], name: str) -> bpy.types.Object:
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


def add_preview(directory: Path, filename: str, target, camera_location) -> None:
    world = bpy.context.scene.world
    if world is None:
        world = bpy.data.worlds.new("GimmickPreviewWorld")
        bpy.context.scene.world = world
    world.use_nodes = True
    background = next(
        (node for node in world.node_tree.nodes if node.type == "BACKGROUND"), None)
    if background is None:
        background = world.node_tree.nodes.new("ShaderNodeBackground")
        output = next(
            (node for node in world.node_tree.nodes if node.type == "OUTPUT_WORLD"), None)
        if output is None:
            output = world.node_tree.nodes.new("ShaderNodeOutputWorld")
        world.node_tree.links.new(background.outputs["Background"], output.inputs["Surface"])
    background.inputs["Color"].default_value = (0.004, 0.008, 0.018, 1.0)
    background.inputs["Strength"].default_value = 0.16

    bpy.ops.object.camera_add(location=camera_location)
    camera = bpy.context.object
    camera.name = "PreviewCamera"
    camera.data.lens = 56.0
    point_at(camera, target)
    bpy.context.scene.camera = camera

    for name, location, energy, color, size in (
        ("CyanKey", (-3.5, -4.0, 5.5), 950.0, (0.08, 0.68, 1.0), 4.0),
        ("OrangeRim", (4.0, 1.0, 4.5), 800.0, (1.0, 0.22, 0.04), 3.0),
        ("WhiteFill", (0.0, -1.5, 7.0), 700.0, (0.72, 0.88, 1.0), 4.0),
    ):
        data = bpy.data.lights.new(name, "AREA")
        data.energy = energy
        data.color = color
        data.shape = "DISK"
        data.size = size
        light = bpy.data.objects.new(name, data)
        bpy.context.collection.objects.link(light)
        light.location = location
        point_at(light, target)

    bpy.ops.mesh.primitive_plane_add(size=20.0, location=(0.0, 0.0, -0.01))
    floor = bpy.context.object
    floor.name = "PreviewFloor"
    floor_material = bpy.data.materials.new("PreviewFloorMaterial")
    floor_material.diffuse_color = (0.008, 0.014, 0.025, 1.0)
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


def create_pinball() -> None:
    reset_scene()
    PINBALL_DIR.mkdir(parents=True, exist_ok=True)
    navy = write_solid_texture(PINBALL_DIR, "bumper_navy", (0.012, 0.035, 0.075, 1.0))
    cyan = write_solid_texture(PINBALL_DIR, "bumper_cyan", (0.02, 0.72, 1.0, 1.0))
    white = write_solid_texture(PINBALL_DIR, "bumper_white", (0.82, 0.96, 1.0, 1.0))
    orange = write_solid_texture(PINBALL_DIR, "bumper_orange", (1.0, 0.12, 0.025, 1.0))
    metal = write_solid_texture(PINBALL_DIR, "bumper_metal", (0.58, 0.72, 0.82, 1.0))

    mat_navy = make_material("BumperNavy", navy, 0.68, 0.22)
    mat_cyan = make_material("BumperCyanGlow", cyan, 0.35, 0.18, 1.2)
    mat_white = make_material("BumperWhiteGlow", white, 0.18, 0.20, 0.55)
    mat_orange = make_material("BumperImpactOrange", orange, 0.25, 0.20, 1.6)
    mat_metal = make_material("BumperRingMetal", metal, 0.92, 0.12)

    body: list[bpy.types.Object] = []
    body.append(cylinder("FloorBase", (0.0, 0.0, 0.10), 0.86, 0.20, mat_navy))
    body.append(cylinder("GlowBand", (0.0, 0.0, 0.25), 0.77, 0.16, mat_cyan))
    bpy.ops.mesh.primitive_cone_add(
        vertices=64, radius1=0.88, radius2=0.62, depth=0.16,
        location=(0.0, 0.0, 0.41))
    skirt = bpy.context.object
    skirt.name = "ContactSkirt"
    skirt.data.materials.append(mat_white)
    body.append(finish_mesh(skirt, 0.012))
    body.append(cylinder("CoreTower", (0.0, 0.0, 0.64), 0.56, 0.42, mat_navy))
    body.append(torus("CapHalo", (0.0, 0.0, 0.84), 0.55, 0.055, mat_cyan))
    body.append(sphere("LightCap", (0.0, 0.0, 0.91), (0.62, 0.62, 0.22), mat_white))
    body.append(cylinder("CenterTarget", (0.0, 0.0, 1.085), 0.22, 0.035, mat_cyan))

    for spoke in range(16):
        angle = math.tau * spoke / 16.0
        radius = 0.47
        material = mat_orange if spoke % 2 == 0 else mat_cyan
        body.append(box(
            f"EnergySpoke_{spoke:02d}",
            (math.cos(angle) * radius, math.sin(angle) * radius, 1.078),
            (0.24, 0.055, 0.025), material, bevel=0.008,
            rotation=(0.0, 0.0, angle)))

    ring_parts: list[bpy.types.Object] = []
    ring_parts.append(torus("PopRing", (0.0, 0.0, 0.61), 0.70, 0.055, mat_metal))
    for rod in range(4):
        angle = math.tau * rod / 4.0
        ring_parts.append(cylinder(
            f"PopRod_{rod}",
            (math.cos(angle) * 0.70, math.sin(angle) * 0.70, 0.39),
            0.028, 0.44, mat_metal, vertices=16))

    body_model = join(body, "PinballBumperBody")
    ring_model = join(ring_parts, "PinballBumperRing")
    export_obj(body_model, PINBALL_DIR / "pinball_bumper.obj")
    export_obj(ring_model, PINBALL_DIR / "pinball_bumper_ring.obj")
    bpy.ops.wm.save_as_mainfile(filepath=str(PINBALL_DIR / "pinball_bumper.blend"))
    add_preview(PINBALL_DIR, "pinball_bumper_preview.png", (0.0, 0.0, 0.55), (2.8, -4.2, 2.7))


def create_shutter() -> None:
    reset_scene()
    SHUTTER_DIR.mkdir(parents=True, exist_ok=True)
    navy = write_solid_texture(SHUTTER_DIR, "shutter_navy", (0.012, 0.028, 0.065, 1.0))
    cyan = write_solid_texture(SHUTTER_DIR, "shutter_cyan", (0.015, 0.65, 1.0, 1.0))
    metal = write_solid_texture(SHUTTER_DIR, "shutter_metal", (0.46, 0.58, 0.68, 1.0))
    white = write_solid_texture(SHUTTER_DIR, "shutter_white", (0.78, 0.92, 1.0, 1.0))
    orange = write_solid_texture(SHUTTER_DIR, "shutter_orange", (1.0, 0.11, 0.02, 1.0))

    mat_navy = make_material("ShutterNavy", navy, 0.78, 0.20)
    mat_cyan = make_material("ShutterGuideGlow", cyan, 0.35, 0.18, 1.0)
    mat_metal = make_material("ShutterMetal", metal, 0.90, 0.13)
    mat_white = make_material("ShutterPanelWhite", white, 0.48, 0.20)
    mat_orange = make_material("ShutterWarning", orange, 0.30, 0.18, 1.35)

    frame: list[bpy.types.Object] = []
    frame.append(box("LeftRail", (-1.86, 0.0, 2.55), (0.28, 0.68, 5.10), mat_navy, 0.055))
    frame.append(box("RightRail", (1.86, 0.0, 2.55), (0.28, 0.68, 5.10), mat_navy, 0.055))
    frame.append(box("LeftGuide", (-1.66, -0.35, 2.42), (0.08, 0.07, 4.84), mat_cyan, 0.018))
    frame.append(box("RightGuide", (1.66, -0.35, 2.42), (0.08, 0.07, 4.84), mat_cyan, 0.018))
    frame.append(box("TopHousing", (0.0, 0.0, 5.08), (4.0, 0.78, 0.48), mat_navy, 0.075))
    frame.append(box("TopGlow", (0.0, -0.42, 4.98), (2.75, 0.06, 0.10), mat_cyan, 0.018))
    for pulley, x in enumerate((-1.15, 0.0, 1.15)):
        frame.append(cylinder(
            f"DrivePulley_{pulley}", (x, -0.43, 5.10), 0.17, 0.09,
            mat_metal, vertices=32, rotation=(math.pi * 0.5, 0.0, 0.0)))
        frame.append(torus(
            f"PulleyGlow_{pulley}", (x, -0.49, 5.10), 0.12, 0.025,
            mat_orange, rotation=(math.pi * 0.5, 0.0, 0.0)))
    for side, x in enumerate((-1.62, 1.62)):
        frame.append(cylinder(
            f"LiftCable_{side}", (x, -0.38, 3.60), 0.018, 2.55,
            mat_metal, vertices=12))
        frame.append(sphere(
            f"WarningLamp_{side}", (x, -0.43, 4.72),
            (0.12, 0.07, 0.12), mat_orange))

    panel: list[bpy.types.Object] = []
    panel.append(box("DoorCore", (0.0, 0.0, 1.0), (3.34, 0.38, 2.0), mat_navy, 0.04))
    for slat in range(8):
        z = 0.17 + slat * 0.235
        material = mat_white if slat % 2 == 0 else mat_metal
        panel.append(box(
            f"DoorSlat_{slat:02d}", (0.0, -0.225, z),
            (3.08, 0.08, 0.13), material, 0.026))
    panel.append(box("BottomWarning", (0.0, -0.23, 0.10), (3.18, 0.09, 0.16), mat_orange, 0.025))
    panel.append(torus(
        "MagneticLockRing", (0.0, -0.25, 1.05), 0.29, 0.055,
        mat_cyan, rotation=(math.pi * 0.5, 0.0, 0.0)))
    panel.append(cylinder(
        "MagneticLockCore", (0.0, -0.29, 1.05), 0.13, 0.08,
        mat_orange, vertices=32, rotation=(math.pi * 0.5, 0.0, 0.0)))

    frame_model = join(frame, "TimedShutterFrame")
    panel_model = join(panel, "TimedShutterPanel")
    export_obj(frame_model, SHUTTER_DIR / "timed_shutter_frame.obj")
    export_obj(panel_model, SHUTTER_DIR / "timed_shutter_panel.obj")
    bpy.ops.wm.save_as_mainfile(filepath=str(SHUTTER_DIR / "timed_shutter.blend"))
    add_preview(SHUTTER_DIR, "timed_shutter_preview.png", (0.0, 0.0, 2.4), (6.6, -8.8, 5.8))


def main() -> None:
    create_pinball()
    create_shutter()


if __name__ == "__main__":
    main()

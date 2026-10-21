"""Offline icon baking for this project's generated triangular OBJ crop meshes.

This is not the game's renderer. An orthographic thumbnail with fixed matte
lighting keeps the existing geometry readable independently of scene time.
Only the small, known OBJ subset emitted by generate_farm_crop_meshes is accepted.
"""
from __future__ import annotations

from hashlib import sha256
from pathlib import Path
import math

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
CELL = 64
SUPERSAMPLE = 3
FRAME = 50

# Mature scale=1 from BuildFarmCropMeshParts. Lift the root out of the soil.
# Fruit harvests omit plant foliage, matching FarmHarvestVisualSystem::Start.
PARTS = {
    "carrot": (
        ("crop_carrot.obj", (.18, .55, .18), (0, 0, 0), (.94, .43, .10)),
        ("crop_carrot_leaves.obj", (.32, .37, .32), (0, .55 * .96, 0), (.25, .54, .18)),
    ),
    "tomato": (("crop_tomato.obj", (.42, .60, .42), (0, 0, 0), (.87, .12, .07)),),
    "pumpkin": (("crop_pumpkin.obj", (.46, .65, .46), (0, 0, 0), (.12, .36, .16)),),
}
PACKETS = {
    "seed_carrot": ("farm_seed_packets.png", 1),
    "seed_tomato": ("farm_seed_packets_formal.png", 0),
    "seed_pumpkin": ("farm_seed_packets_formal.png", 1),
}


def source_hashes():
    paths = {ROOT / "project/Resources/farm" / part[0] for parts in PARTS.values() for part in parts}
    paths.update(ROOT / "project/Resources/ui" / name for name, _ in PACKETS.values())
    paths.update(ROOT / path for path in (
        "project/application/farm/render/FarmMeshLayout.h",
        "project/application/farm/system/FarmHarvestVisualSystem.h",
        "project/application/farm/ui/FarmRuntimeUI.cpp",
        "tools/ui/farm_item_icons.py",
        "tools/ui/generate_farm_hud_icons.py",
    ))
    return {path.relative_to(ROOT).as_posix(): sha256(path.read_bytes()).hexdigest()
            for path in sorted(paths)}


def read_triangles(path):
    vertices, faces = [], []
    for line in path.read_text(encoding="ascii").splitlines():
        fields = line.split()
        if not fields or fields[0].startswith("#"):
            continue
        if fields[0] == "v":
            if len(fields) != 4:
                raise ValueError(f"Expected xyz vertex in {path}")
            vertex = tuple(map(float, fields[1:]))
            if not all(math.isfinite(x) for x in vertex):
                raise ValueError(f"Nonfinite vertex in {path}")
            vertices.append(vertex)
        elif fields[0] == "f":
            if len(fields) != 4:
                raise ValueError(f"Expected triangular face in {path}")
            indices = tuple(int(field.split("/")[0]) - 1 for field in fields[1:])
            if any(i < 0 or i >= len(vertices) for i in indices):
                raise ValueError(f"Invalid vertex index in {path}")
            faces.append(indices)
        elif fields[0] not in {"o", "s", "vt", "vn"}:
            raise ValueError(f"Unsupported OBJ record {fields[0]} in {path}")
        if len(vertices) > 100000 or len(faces) > 100000:
            raise ValueError(f"Thumbnail mesh exceeds offline budget: {path}")
    if not vertices or not faces:
        raise ValueError(f"Empty mesh in {path}")
    return np.array(vertices, dtype=np.float64)[np.array(faces)]


def outlined(image):
    alpha = image.getchannel("A")
    outline = Image.new("RGBA", image.size, (35, 49, 43, 0))
    outline.putalpha(alpha.filter(ImageFilter.MaxFilter(3)))
    return Image.alpha_composite(outline, image)


def crop_icon(name):
    triangles, colors = [], []
    angle = math.radians(-24 if name == "carrot" else 0)
    rotation = np.array(((math.cos(angle), -math.sin(angle), 0),
                         (math.sin(angle), math.cos(angle), 0), (0, 0, 1)))
    for filename, scale, offset, color in PARTS[name]:
        mesh = read_triangles(ROOT / "project/Resources/farm" / filename)
        mesh = (mesh * scale + offset) @ rotation.T
        triangles.extend(mesh)
        colors.extend([color] * len(mesh))
    world = np.array(triangles)
    toward_camera = np.array((.25, .32, 1.0))
    toward_camera /= np.linalg.norm(toward_camera)
    right = np.cross((0, 1, 0), toward_camera)
    right /= np.linalg.norm(right)
    up = np.cross(toward_camera, right)
    projected = world @ np.stack((right, -up, toward_camera)).T
    low = projected[:, :, :2].min(axis=(0, 1))
    high = projected[:, :, :2].max(axis=(0, 1))
    extent = max(high - low)
    if not math.isfinite(extent) or extent < 1e-8:
        raise ValueError(f"Degenerate crop bounds: {name}")
    size = CELL * SUPERSAMPLE
    projected[:, :, :2] = ((projected[:, :, :2] - (high + low) / 2)
                           * (FRAME * SUPERSAMPLE / extent) + size / 2)
    pixels = np.zeros((size, size, 4), dtype=np.uint8)
    depth = np.full((size, size), -np.inf)
    light = np.array((-.55, .8, 1.0))
    light /= np.linalg.norm(light)
    for tri, points, base in zip(world, projected, colors):
        normal = np.cross(tri[1] - tri[0], tri[2] - tri[0])
        length = np.linalg.norm(normal)
        if length < 1e-12:
            continue
        normal /= length
        shade = .64 + .36 * max(0.0, float(normal @ light))
        # Material RGB is linear; PNG output is sRGB, as expected by the atlas loader.
        linear = np.clip(np.array(base) * shade, 0, 1)
        srgb = np.where(linear <= .0031308, linear * 12.92, 1.055 * linear ** (1 / 2.4) - .055)
        color = np.round(srgb * 255).astype(np.uint8)
        x0, y0 = np.maximum(0, np.floor(points[:, :2].min(axis=0)).astype(int))
        x1, y1 = np.minimum(size - 1, np.ceil(points[:, :2].max(axis=0)).astype(int))
        a, b, c = points
        denominator = (b[1]-c[1])*(a[0]-c[0]) + (c[0]-b[0])*(a[1]-c[1])
        if abs(denominator) < 1e-10:
            continue
        yy, xx = np.mgrid[y0:y1+1, x0:x1+1] + .5
        wa = ((b[1]-c[1])*(xx-c[0]) + (c[0]-b[0])*(yy-c[1])) / denominator
        wb = ((c[1]-a[1])*(xx-c[0]) + (a[0]-c[0])*(yy-c[1])) / denominator
        wc = 1 - wa - wb
        z = wa*a[2] + wb*b[2] + wc*c[2]
        region = depth[y0:y1+1, x0:x1+1]
        visible = (wa >= -1e-9) & (wb >= -1e-9) & (wc >= -1e-9) & (z > region)
        region[visible] = z[visible]
        pixels[y0:y1+1, x0:x1+1][visible] = (*color, 255)
    image = Image.fromarray(pixels).resize((CELL, CELL), Image.Resampling.LANCZOS)
    # Suppress low-alpha resampling tails so the one-pixel outline has a firm gutter.
    image.putalpha(image.getchannel("A").point(lambda a: 0 if a < 12 else a))
    return outlined(image)


def packet_icon(name):
    filename, slot = PACKETS[name]
    with Image.open(ROOT / "project/Resources/ui" / filename) as source:
        if source.size != (1536, 1024):
            raise ValueError(f"Shop packet atlas layout changed: {filename}")
        front = source.convert("RGBA").crop((slot*768, 0, (slot+1)*768, 1024))
    front = front.resize((39, 52), Image.Resampling.LANCZOS)
    image = Image.new("RGBA", (CELL, CELL))
    image.paste(front, (12, 6))
    return outlined(image)


def item_icon(name):
    if name in PARTS:
        return crop_icon(name)
    if name in PACKETS:
        return packet_icon(name)
    raise ValueError(f"Not an item-derived icon: {name}")

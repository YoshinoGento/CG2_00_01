"""Bake existing farm meshes/packet fronts into an offline-only UI atlas.

Run export_farm_item_parts.cmd first. Its data comes from BuildFarmCropMeshParts,
so crop proportions and material colors are not independently redesigned here.
The triangle-only OBJ reader intentionally rejects unsupported face topology.
"""
from pathlib import Path
import hashlib
import json
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
RES = ROOT / "project/Resources"
OUT = RES / "ui"
CELL = 128
SUPERSAMPLE = 2
SIZE = CELL * SUPERSAMPLE
NAMES = ["Hoe", "Water", "Seed", "Harvest", "Carrot", "Tomato", "Pumpkin",
         "SeedCarrot", "SeedTomato", "SeedPumpkin"]


def read_triangles(path):
    vertices, faces = [], []
    for line in path.read_text(encoding="ascii").splitlines():
        fields = line.split()
        if not fields or fields[0].startswith("#"):
            continue
        if fields[0] == "v":
            assert len(fields) == 4
            vertices.append([float(v) for v in fields[1:]])
        elif fields[0] == "f":
            assert len(fields) == 4, "Only generated triangle OBJ assets are supported"
            indices = [int(v.split("/")[0]) - 1 for v in fields[1:]]
            assert all(0 <= i < len(vertices) for i in indices)
            faces.append(indices)
    result = np.asarray(vertices, dtype=float)[np.asarray(faces)]
    assert result.size and np.isfinite(result).all()
    return result


def crop_image(crop):
    parts = []
    for part in crop["parts"]:
        triangles = read_triangles(RES / "farm" / part["mesh"])
        triangles = triangles * part["scale"] + part["position"]
        parts.append((triangles, np.array(part["color"])))
    # Orthographic studio angle: the complete carrot root remains visible.
    yaw, pitch = math.radians(24), math.radians(13)
    right = np.array([math.cos(yaw), 0, -math.sin(yaw)])
    toward = np.array([math.sin(yaw)*math.cos(pitch), math.sin(pitch), math.cos(yaw)*math.cos(pitch)])
    up = np.cross(toward, right)
    basis = np.stack([right, -up, toward], axis=1)
    projected = [tri @ basis for tri, _ in parts]
    all_points = np.concatenate(projected).reshape(-1, 3)
    lo, hi = all_points[:, :2].min(axis=0), all_points[:, :2].max(axis=0)
    scale = (SIZE - 40) / max(hi-lo)
    center = (lo+hi)/2
    depth = np.full((SIZE, SIZE), -np.inf)
    pixels = np.zeros((SIZE, SIZE, 4), dtype=np.uint8)
    light = np.array([-0.5, 0.8, 0.9]); light /= np.linalg.norm(light)
    for (triangles, color), projected_part in zip(parts, projected):
        for tri, projection in zip(triangles, projected_part):
            normal = np.cross(tri[1]-tri[0], tri[2]-tri[0])
            length = np.linalg.norm(normal)
            if length < 1e-12:
                continue
            normal /= length
            if normal @ toward <= 0:
                continue
            xy = (projection[:, :2]-center)*scale+SIZE/2
            x0, y0 = np.maximum(np.floor(xy.min(axis=0)).astype(int), 0)
            x1, y1 = np.minimum(np.ceil(xy.max(axis=0)).astype(int)+1, SIZE)
            if x1 <= x0 or y1 <= y0:
                continue
            yy, xx = np.mgrid[y0:y1, x0:x1]; xx = xx+.5; yy = yy+.5
            a, b, c = xy
            divisor = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(divisor) < 1e-10:
                continue
            u = ((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/divisor
            v = ((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/divisor
            w = 1-u-v
            z = u*projection[0, 2]+v*projection[1, 2]+w*projection[2, 2]
            visible = (u >= 0) & (v >= 0) & (w >= 0) & (z > depth[y0:y1, x0:x1])
            shade = .48 + .52*max(0, normal @ light)
            rgb = np.clip(np.power(color*shade, 1/2.2)*255, 0, 255).astype(np.uint8)
            pixels[y0:y1, x0:x1][visible] = [*rgb, 255]
            depth[y0:y1, x0:x1][visible] = z[visible]
    image = Image.fromarray(pixels)
    # A one-pixel neutral edge protects the green pumpkin against the farm grass.
    alpha = image.getchannel("A")
    edge = Image.new("RGBA", image.size, (22, 34, 30, 0))
    edge.putalpha(alpha.filter(ImageFilter.MaxFilter(5)))
    edge.alpha_composite(image)
    return edge.resize((CELL, CELL), Image.Resampling.LANCZOS)


def packet_image(path, index):
    image = Image.open(path).convert("RGBA")
    assert image.size == (1536, 1024)
    image = image.crop((index*768, 0, (index+1)*768, 1024))
    bounds = image.getbbox(); assert bounds
    image = image.crop(bounds)
    image.thumbnail((98, 110), Image.Resampling.LANCZOS)
    result = Image.new("RGBA", (CELL, CELL))
    result.alpha_composite(image, ((CELL-image.width)//2, (CELL-image.height)//2))
    return result


def tool_image(kind):
    image = Image.new("RGBA", (CELL, CELL))
    draw = ImageDraw.Draw(image)
    ink = (32, 41, 36)
    if kind == "Hoe":
        draw.line([(34, 111), (82, 21)], fill=ink, width=14)
        draw.line([(34, 109), (81, 22)], fill=(168, 116, 61), width=9)
        draw.line([(35, 108), (80, 24)], fill=(214, 166, 94), width=3)
        draw.polygon([(64, 20), (103, 31), (108, 57), (68, 46)], fill=(73, 92, 99), outline=ink, width=3)
        draw.polygon([(65, 22), (103, 33), (99, 39), (68, 30)], fill=(181, 200, 202))
        draw.line([(70, 44), (107, 55)], fill=(194, 208, 204), width=3)
    elif kind == "Water":
        draw.ellipse((16, 25, 64, 83), outline=ink, width=13)
        draw.ellipse((17, 26, 63, 82), outline=(93, 159, 158), width=7)
        draw.polygon([(72, 65), (100, 39), (108, 43), (82, 88)], fill=(82, 161, 160), outline=ink, width=3)
        draw.ellipse((96, 31, 117, 47), fill=(179, 216, 202), outline=ink, width=3)
        draw.rounded_rectangle((29, 45, 82, 104), radius=7, fill=(65, 136, 142), outline=ink, width=3)
        draw.ellipse((29, 38, 82, 57), fill=(135, 195, 188), outline=ink, width=3)
        draw.ellipse((41, 44, 69, 51), fill=ink)
        draw.line([(39, 62), (39, 91)], fill=(143, 211, 204), width=4)
        for x in [101, 107, 112]: draw.point((x, 38), fill=ink)
    else:
        draw.arc((28, 12, 100, 93), 180, 360, fill=ink, width=11)
        draw.arc((28, 12, 100, 93), 180, 360, fill=(203, 157, 91), width=6)
        draw.polygon([(18, 58), (110, 58), (99, 109), (29, 109)], fill=(161, 108, 56), outline=ink, width=3)
        for y in [69, 80, 91, 102]: draw.line([(26, y), (101, y)], fill=(218, 170, 101), width=3)
        for x in [37, 51, 65, 79, 93]: draw.line([(x, 62), (x, 105)], fill=(119, 77, 43), width=2)
        draw.line([(20, 58), (108, 58)], fill=(229, 185, 116), width=6)
    return image


def main():
    part_path = ROOT / "generated/farm_item_icons/parts.json"
    parts = json.loads(part_path.read_text())
    assert [p["cropSlot"] for p in parts] == [1, 2, 3]
    crops = [crop_image(c) for c in parts]
    packets = [packet_image(OUT/"farm_seed_packets.png", 1),
               packet_image(OUT/"farm_seed_packets_formal.png", 0),
               packet_image(OUT/"farm_seed_packets_formal.png", 1)]
    icons = [tool_image("Hoe"), tool_image("Water"), packets[0], tool_image("Harvest"), *crops, *packets]
    atlas = Image.new("RGBA", (CELL*4, CELL*3))
    for i, icon in enumerate(icons):
        assert icon.getbbox() is not None
        atlas.alpha_composite(icon, ((i % 4)*CELL, (i // 4)*CELL))
    atlas.save(OUT/"farm_item_icons.png", optimize=True)
    sources = [RES/"farm"/p["mesh"] for crop in parts for p in crop["parts"]]
    sources += [OUT/"farm_seed_packets.png", OUT/"farm_seed_packets_formal.png",
                ROOT/"project/application/farm/render/FarmMeshLayout.h", part_path]
    manifest = {"cell": CELL, "columns": 4, "size": list(atlas.size), "icons": NAMES,
                "cropParts": parts, "sources": {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
                "notes": "Crop OBJ geometry/transforms/colors are exported from game code; lighting is fixed icon lighting. Packets reuse shop fronts. Tools are original UI-only artwork, not existing world models."}
    (OUT/"farm_item_icons.json").write_text(json.dumps(manifest, indent=2)+"\n", encoding="utf-8")
    preview = Image.new("RGB", (640, 320), (35, 47, 43))
    draw = ImageDraw.Draw(preview)
    for i, icon in enumerate(icons):
        x, y = (i%5)*128, (i//5)*160
        preview.paste(icon, (x, y), icon)
        draw.text((x+8, y+133), NAMES[i], fill=(242, 244, 232))
    preview.save(ROOT/"generated/farm_item_icons/preview.png")
    print("Baked 10 icons from six crop OBJ parts and shop packets")


if __name__ == "__main__":
    main()

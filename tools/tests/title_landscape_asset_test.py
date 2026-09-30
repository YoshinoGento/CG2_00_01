"""Check exported rural geometry and atlas against the existing Model loader."""
from pathlib import Path
import importlib.util
import json
import math
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("landscape", ROOT/"tools/title/generate_title_landscape.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
mesh = generator.build()
assert mesh.triangles == generator.build().triangles
assert len(mesh.triangles) < generator.MAX_TRIANGLES


def overlaps_rectangle(points, rectangle):
    triangle = [(x,z) for x,_,z in points]
    x0,x1,z0,z1 = rectangle
    corners = ((x0,z0),(x0,z1),(x1,z1),(x1,z0))
    axes = [(1,0),(0,1)]
    for a,b in zip(triangle,triangle[1:]+triangle[:1]):
        axes.append((a[1]-b[1],b[0]-a[0]))
    for ax,az in axes:
        if abs(ax)+abs(az) < 1e-10:
            continue
        t = [x*ax+z*az for x,z in triangle]
        r = [x*ax+z*az for x,z in corners]
        if max(t) < min(r) or max(r) < min(t):
            return False
    return True


# Regression: all three vertices can lie outside while a road face crosses the farm.
assert overlaps_rectangle(((-8,0,6),(8,0,6),(0,0,12)),generator.FARM_EXCLUSION)
sections = {t[4] for t in mesh.triangles}
assert not sections.intersection({"paddy_water","paddy_rice","paddy_banks","rice_leaves","neighbor_field","neighbor_rows"})
assert len(generator.TREE_POSITIONS) == 8 and len(generator.BUSH_POSITIONS) == 4
for i,(x,z) in enumerate(generator.TREE_POSITIONS):
    for a,b in generator.TREE_POSITIONS[i+1:]:
        assert math.hypot(x-a,z-b) > 6
for points, _, _, _, section in mesh.triangles:
    if section != "ground":
        assert not overlaps_rectangle(points,generator.FARM_EXCLUSION), (section,points)
    for x,y,z in points:
        assert -50 <= x <= 50 and -74 <= z <= 86 and -.3 < y < 7
        if section in ("road","road_median","road_verge"):
            assert y > generator.ground_height(x,z), (section,x,y,z)
assert generator.ground_height(0,6) == generator.GROUND_Y
assert generator.ground_height(6,20) == generator.GROUND_Y

asset = ROOT/"project/Resources/title"
vertices,uv,normals,faces = [],[],[],[]
for line in (asset/"countryside.obj").read_text(encoding="ascii").splitlines():
    parts = line.split()
    if not parts or parts[0].startswith("#"):
        continue
    if parts[0] in ("v","vn","vt"):
        values = tuple(map(float,parts[1:]))
        assert all(math.isfinite(x) for x in values)
        {"v":vertices,"vn":normals,"vt":uv}[parts[0]].append(values)
    elif parts[0] == "f":
        assert len(parts) == 4
        faces.append([tuple(map(int,p.split("/"))) for p in parts[1:]])
assert len(faces) == len(mesh.triangles)
for index, face in enumerate(faces):
    material = mesh.triangles[index][3]
    for v,t,n in face:
        assert 1 <= v <= len(vertices) and 1 <= t <= len(uv) and 1 <= n <= len(normals)
        u,w = uv[t-1]
        column,row = material%3, material//3
        assert column+.024 <= u*3 <= column+.976
        assert row+.024 <= (1-w)*2 <= row+.976
    a,b,c = (vertices[p[0]-1] for p in face)
    u,v = ([b[j]-a[j] for j in range(3)],[c[j]-a[j] for j in range(3)])
    cross = (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
    normal = normals[face[0][2]-1]
    assert sum(cross[j]*normal[j] for j in range(3)) > 1e-7
    assert abs(sum(x*x for x in normal)-1) < 1e-6
    # Reflection plus Assimp winding reversal must recover the intended runtime face.
    exported = [vertices[face[j][0]-1] for j in (0,2,1)]
    expected = mesh.triangles[index][0]
    for p,q in zip(exported,expected):
        assert max(abs(p[j]*(-1 if j == 0 else 1)-q[j]) for j in range(3)) < 1e-6
metadata = json.loads((asset/"countryside_mesh.json").read_text())
assert metadata["triangles"] == len(faces) and metadata["materials"] == [0,1,2,4]
with Image.open(asset/"countryside_atlas.png") as image:
    assert image.size == (1536,1024)
    assert image.convert("RGBA").getchannel("A").getextrema() == (255,255)
print(f"PASS rural landscape: {len(faces)} triangles; deterministic, finite, UV gutters, winding/index, farm clearance, opaque atlas")

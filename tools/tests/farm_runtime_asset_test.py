"""Validate original submission meshes against the loader's vertex contract."""
from pathlib import Path
import math
ROOT = Path(__file__).resolve().parents[2]
for name in ("ground", "player_marker"):
    vertices, normals, uv, faces = [], [], [], []
    for line in (ROOT / "project/Resources/farm" / (name + ".obj")).read_text().splitlines():
        parts = line.split()
        if not parts or parts[0].startswith("#"):
            continue
        if parts[0] in ("v", "vn", "vt"):
            values = tuple(map(float,parts[1:]))
            assert all(math.isfinite(x) for x in values)
            {"v":vertices,"vn":normals,"vt":uv}[parts[0]].append(values)
        if parts[0] == "f":
            assert len(parts) == 4
            faces.append([tuple(map(int,p.split("/"))) for p in parts[1:]])
    assert vertices and normals and uv and faces
    for face in faces:
        for v,t,n in face:
            assert 1 <= v <= len(vertices) and 1 <= t <= len(uv) and 1 <= n <= len(normals)
        a,b,c = (vertices[p[0]-1] for p in face)
        u,v = ([b[j]-a[j] for j in range(3)], [c[j]-a[j] for j in range(3)])
        cross = (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
        n = normals[face[0][2]-1]
        assert sum(cross[j]*n[j] for j in range(3)) > 1e-8
        assert abs(sum(x*x for x in n)-1) < 1e-6
    print(f"PASS {name}: {len(faces)} triangles, finite unit normals, valid UV/index/winding")

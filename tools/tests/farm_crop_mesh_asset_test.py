"""Validate the original crop foliage geometry without a graphics device."""
import importlib.util
import math
from collections import Counter
from pathlib import Path

spec = importlib.util.spec_from_file_location("crop_generator", Path(__file__).parents[1] / "generate_farm_crop_meshes.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)

def validate(triangles):
    assert 0 < len(triangles) <= 800
    edges = Counter()
    volume = 0
    for a, b, c in triangles:
        for p in (a, b, c):
            assert all(math.isfinite(v) for v in p)
            assert -.000001 <= p[1] <= 1.000001
            assert math.hypot(p[0], p[2]) <= 1.000001 # OBJ stores seven decimal places.
        u = [b[i]-a[i] for i in range(3)]
        v = [c[i]-a[i] for i in range(3)]
        cross = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
        assert sum(v*v for v in cross) > 1e-16
        volume += sum(a[i]*cross[i] for i in range(3))/6
        for p, q in ((a,b), (b,c), (c,a)):
            edges[(p,q)] += 1
    assert volume > 0
    assert all(count == edges[(q,p)] for (p,q),count in edges.items())

turnip = generator.crop_foliage_mesh()
carrot = generator.crop_foliage_mesh(True)
assert turnip != carrot and turnip == generator.crop_foliage_mesh()
for name, triangles in (("crop_turnip_leaves", turnip), ("crop_carrot_leaves", carrot),
                        ("crop_tomato", generator.tomato_mesh()),
                        ("crop_tomato_stems", generator.tomato_stems()),
                        ("crop_pumpkin", generator.pumpkin_mesh()),
                        ("crop_pumpkin_vines", generator.transformed(generator.leaves_mesh(), (1,.25,1), (0,0,0)) +
                         generator.transformed(generator.root_mesh([(0,.10),(1,.07)],6), (.6,1,.6), (0,0,0)))):
    validate(triangles)
    lines = (generator.ROOT / (name + ".obj")).read_text(encoding="ascii").splitlines()
    points = [tuple(map(float, line.split()[1:])) for line in lines if line.startswith("v ")]
    assert len(points) == len(triangles)*3
    rounded = [tuple(round(v,7) for v in p) for tri in triangles for p in tri]
    assert points == rounded
    validate([tuple(points[i:i+3]) for i in range(0,len(points),3)])
    print(f"PASS: {name}: {len(triangles)} triangles, finite/unit bounds, closed winding, deterministic OBJ")

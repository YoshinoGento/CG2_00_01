"""Surface conservation, bounded inputs and integration checks for authored voxel meshes."""
from pathlib import Path
import math
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/title'))
import voxel_scenery as voxel
import generate_title_landscape as landscape
import generate_title_pond as pond


def unit_faces(cells):
    result = set()
    for p in cells:
        for axis in range(3):
            for sign in (-1, 1):
                q = tuple(p[i]+(sign if i == axis else 0) for i in range(3))
                if q not in cells:
                    result.add((axis, sign, p[axis]+(sign > 0), p[(axis+1) % 3], p[(axis+2) % 3]))
    return result


def expanded_faces(quads):
    result = set()
    for corners in quads:
        a, b, c = corners[:3]
        u, v = [b[i]-a[i] for i in range(3)], [c[i]-a[i] for i in range(3)]
        cross = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
        axis = next(i for i in range(3) if cross[i])
        assert sum(n != 0 for n in cross) == 1
        sign = 1 if cross[axis] > 0 else -1
        x, y = (axis+1) % 3, (axis+2) % 3
        for i in range(min(p[x] for p in corners), max(p[x] for p in corners)):
            for j in range(min(p[y] for p in corners), max(p[y] for p in corners)):
                face = (axis, sign, a[axis], i, j)
                assert face not in result, 'Duplicate surface'
                result.add(face)
    return result


class VoxelSceneryTest(unittest.TestCase):
    def test_finer_cells_and_restrained_palette(self):
        from PIL import Image
        self.assertEqual(voxel.FOLIAGE_CELL, .24)
        self.assertEqual(voxel.STONE_CELL, .08)
        with Image.open(ROOT/'project/Resources/title/countryside_atlas.png') as atlas:
            colors = []
            for x, y in voxel.FOLIAGE_PATCHES:
                pixels = [atlas.getpixel((1024+x+u,y+v))[:3] for v in range(4) for u in range(4)]
                colors.append(tuple(sum(p[i] for p in pixels)/16 for i in range(3)))
        for channel in range(3):
            self.assertLess(max(c[channel] for c in colors)-min(c[channel] for c in colors), 14)
        self.assertTrue(all(g > r > b for r,g,b in colors))
        points = ((0,0,0),(0,1,0),(1,1,0),(1,0,0))
        self.assertEqual(voxel.foliage_uv(points),voxel.foliage_uv(tuple(reversed(points))))

    def test_branch_outward_normals(self):
        mesh = landscape.Mesh()
        start, end = (0,0,0), (.85,1.3,.2)
        mesh.branch(start,end,.11,.055)
        center = tuple((a+b)*.5 for a,b in zip(start,end))
        self.assertEqual(len(mesh.triangles), 12)
        for points, _, normal, _, _ in mesh.triangles:
            p = tuple(sum(v[i] for v in points)/3-center[i] for i in range(3))
            self.assertGreater(sum(p[i]*normal[i] for i in range(3)), 0)

    def test_surface_conservation_and_winding(self):
        fixtures = [{(0,0,0)}, {(x,y,z) for x in range(-2,3) for y in range(3) for z in range(2)},
                    {(0,0,0), (1,0,0), (0,1,0), (0,0,1)},
                    {(x,y,z) for x in range(3) for y in range(3) for z in range(3)}-{(1,1,1)},
                    voxel.ellipsoid_union((((0,2,0),(2,1.4,1.5)), ((1,1.8,0),(.9,1,1))), .32)]
        for cells in fixtures:
            quads = voxel.surface_quads(cells)
            self.assertEqual(unit_faces(cells), expanded_faces(quads))
            self.assertEqual(quads, voxel.surface_quads(set(reversed(sorted(cells)))))
            self.assertLessEqual(len(quads), len(unit_faces(cells)))
        self.assertEqual(len(voxel.surface_quads(fixtures[1])), 6)

    def test_guarded_inputs(self):
        for cell in (0, -.1, math.inf, math.nan, 1e-8, 1e-310):
            with self.assertRaises(ValueError):
                voxel.ellipsoid_union((((0,0,0),(1,1,1)),), cell)
        for volume in ((), (((0,0,0),(1,-1,1)),), (((0,0,0),(1,math.nan,1)),)):
            with self.assertRaises(ValueError):
                voxel.ellipsoid_union(volume, .3)
        with self.assertRaises(ValueError):
            voxel.surface_quads(set())
        with self.assertRaises(ValueError):
            voxel.surface_quads({(.2,0,0)})
        with self.assertRaises(ValueError):
            voxel.surface_quads({(x,0,0) for x in range(voxel.MAX_SOLID_CELLS+1)})

    def test_reflection_is_exact_subset(self):
        mesh = landscape.build()
        subset = [t for t in mesh.triangles if t[4] in ('forest', 'forest_trunks', 'road_posts')]
        path = ROOT/'project/Resources/title/reflection_scenery.obj'
        vertices = [tuple(map(float,line.split()[1:])) for line in path.read_text().splitlines() if line.startswith('v ')]
        self.assertEqual(len(vertices), len(subset)*3)
        for i, (points,_,_,_,_) in enumerate(subset):
            for j, p in enumerate(points):
                q = vertices[i*3+(0,2,1)[j]]
                self.assertLess(max(abs(a-b) for a,b in zip(p,(-q[0],q[1],q[2]))), 1e-6)
        for points, _, normal, _, section in mesh.triangles:
            if section == 'forest':
                self.assertEqual(sum(abs(v) > .001 for v in normal), 1)
                for p in points:
                    local = (p[0],p[1]-landscape.GROUND_Y,p[2])
                    self.assertTrue(all(abs(v/voxel.FOLIAGE_CELL-round(v/voxel.FOLIAGE_CELL)) < 1e-9 for v in local))

    def test_boulders_preserve_water_opening(self):
        import json
        config = json.loads((ROOT/'project/Resources/title/pond_layout.json').read_text())
        px, _, pz = config['center']
        rx, rz = config['radii']
        rocks = [t for t in pond.build().triangles if t[4] == 'boulder']
        self.assertTrue(rocks)
        for points, _, normal, _, _ in rocks:
            self.assertTrue(abs(normal[1]) < 1e-6 or abs(abs(normal[1])-1) < 1e-6)
            for x, y, z in points:
                self.assertGreaterEqual(((x-px)/rx)**2+((z-pz)/rz)**2, .9999)
                self.assertFalse(abs(x-config['outfall'][-1][0]) < config['outfallHalfWidth']+.04 and z > pz+rz*.5)
                self.assertLess(y, landscape.GROUND_Y+.29)


if __name__ == '__main__':
    unittest.main()

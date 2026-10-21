"""Branch support, rooted geometry, silhouette variation and offline budgets."""
import math
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/title'))
import tree_scenery as tree
import voxel_scenery as voxel
import generate_title_landscape as landscape
import generate_farm_arrival as arrival


def terrain_triangle_height(x,z):
    x0,z0 = -50+math.floor((x+50)/5)*5,-74+math.floor((z+74)/5)*5
    u,v = (x-x0)/5,(z-z0)/5
    a,b,c,d = (landscape.ground_height(px,pz) for px,pz in
               ((x0,z0),(x0,z0+5),(x0+5,z0+5),(x0+5,z0)))
    return a*(1-v)+b*(v-u)+c*u if v>=u else a*(1-u)+c*v+d*(u-v)


class TreeSceneryTest(unittest.TestCase):
    def test_supported_leaf_groups_and_connected_branches(self):
        for seed in range(32):
            shape = tree.make_tree_shape(5.6,1.85,seed)
            self.assertEqual(shape,tree.make_tree_shape(5.6,1.85,seed))
            self.assertEqual(len(shape.canopies),6)
            self.assertEqual(len(shape.branches),11)
            reached = {(0,-tree.ROOT_EMBED,0)}
            for start,end,radius,tip in shape.branches:
                self.assertIn(start,reached)
                reached.add(end)
                self.assertGreater(end[1],start[1])
                self.assertTrue(0 < tip <= radius <= .21)
            for center,radii in shape.canopies:
                self.assertIn(center,reached)
                self.assertGreater(center[1]-radii[1],2.5)
                self.assertLessEqual(center[1]+radii[1],5.6+1e-8)
            ends = {b[1] for b in shape.branches}-{b[0] for b in shape.branches}
            self.assertEqual(ends,{center for center,_ in shape.canopies})
            cells = voxel.ellipsoid_union(shape.canopies,voxel.FOLIAGE_CELL)
            for end in ends:
                self.assertIn(tuple(math.floor(v/voxel.FOLIAGE_CELL) for v in end),cells)

    def test_real_gaps_and_varied_silhouettes(self):
        silhouettes = set()
        for seed in range(16):
            shape = tree.make_tree_shape(5.6,1.85,seed)
            cells = voxel.ellipsoid_union(shape.canopies,voxel.FOLIAGE_CELL)
            silhouettes.add(frozenset((x,y) for x,y,_ in cells))
            # At least one lower-crown section exposes sky between leaf groups in X or Z.
            split = False
            for y in range(math.ceil(5.6*.52/voxel.FOLIAGE_CELL),math.floor(5.6*.70/voxel.FOLIAGE_CELL)):
                for axis in (0,2):
                    row = sorted({p[axis] for p in cells if p[1] == y})
                    split |= any(b-a >= 2 for a,b in zip(row,row[1:]))
            self.assertTrue(split,seed)
        self.assertEqual(len(silhouettes),16)

    def test_authored_meshes_rooted_bounded_and_no_degenerate_faces(self):
        cases = [(x,z,5.6,1.85,i) for i,(x,z) in enumerate(landscape.TREE_POSITIONS)]
        cases += [(x,z,4.3,1.45,50+i) for i,(x,z) in enumerate(arrival.TREE_POSITIONS)]
        for x,z,height,spread,seed in cases:
            mesh = landscape.Mesh()
            mesh.tree(x,z,height,spread,seed)
            self.assertLessEqual(len(mesh.triangles),tree.MAX_TREE_TRIANGLES)
            ground = landscape.ground_height(x,z)
            trunk = [p for points,_,_,_,section in mesh.triangles if section=='forest_trunks' for p in points]
            self.assertLess(min(p[1] for p in trunk),ground-.02)
            # The first branch's base cap must be buried in the exported triangles, not only the height formula.
            for points,_,_,_,_ in mesh.triangles[8:10]:
                for px,py,pz in points:
                    self.assertLess(py,terrain_triangle_height(px,pz))
            for points,uv,normal,_,_ in mesh.triangles:
                self.assertAlmostEqual(sum(v*v for v in normal),1,places=6)
                self.assertTrue(all(math.isfinite(v) for p in (*points,*uv) for v in p))
                for px,py,pz in points:
                    self.assertLess(math.hypot(px-x,pz-z),spread*1.65)
                    self.assertLessEqual(py,ground+height+voxel.FOLIAGE_CELL)
        self.assertLess(len(landscape.build().triangles),landscape.MAX_TRIANGLES)
        self.assertLess(len(arrival.build_scenery().triangles),10000)

    def test_invalid_input_and_extremes(self):
        for height,spread,seed in ((math.nan,1.8,1),(math.inf,1.8,1),(0,1.8,1),
                (6.5,1.8,1),(5,math.inf,1),(5,0,1),(5,2.3,1),(5,1.8,-1),
                (5,1.8,2**32),(5,1.8,True),(5,1.8,.5)):
            with self.assertRaises(ValueError):
                tree.make_tree_shape(height,spread,seed)
        for height in tree.HEIGHT_RANGE:
            for spread in tree.SPREAD_RANGE:
                for seed in (0,2**32-1):
                    shape = tree.make_tree_shape(height,spread,seed)
                    cells = voxel.ellipsoid_union(shape.canopies,voxel.FOLIAGE_CELL)
                    self.assertLessEqual(len(cells),voxel.MAX_SOLID_CELLS)
                    self.assertLessEqual(len(voxel.surface_quads(cells)),voxel.MAX_QUADS)
        for x,z in ((math.nan,0),(0,math.inf),(46,0)):
            with self.assertRaises(ValueError):
                landscape.Mesh().tree(x,z,5,1.8,1)


if __name__ == '__main__':
    unittest.main()

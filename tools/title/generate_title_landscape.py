"""Build original, static rural scenery in runtime coordinates, then export OBJ."""
from collections import Counter
from pathlib import Path
import json
import math
import random
from voxel_scenery import append_volume, FOLIAGE_CELL
from tree_scenery import make_tree_shape, MAX_TREE_TRIANGLES

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "project/Resources/title"
GROUND_Y = -0.235
ATLAS_COLUMNS, ATLAS_ROWS = 3, 2
ATLAS_INSET = 0.025
GRASS, DIRT, FOLIAGE, RICE, WOOD, WATER = range(6)
MAX_TRIANGLES = 12000
# Conservative scenery keepout includes the original plot and its pond-side staging.
FARM_EXCLUSION = (-4.25, 4.25, 2.4, 9.6)
TREE_POSITIONS = ((-19,22),(-11,30),(-25,38),(-1,18),(18,24),(27,32),(8,39),(24,43))
BUSH_POSITIONS = ((-16,17),(-7,24),(22,18),(29,39))


def ground_height(x, z):
    # Keep the farm and entire road flat; only the distant meadow rolls gently.
    distance = max(abs(x)-18, z-32, 0)
    t = min(distance/18, 1)
    weight = t*t*(3-2*t)
    return GROUND_Y + weight*(.45+.16*math.sin(x*.12)+.12*math.cos(z*.11))


class Mesh:
    def __init__(self):
        self.triangles = []

    def triangle(self, points, uv, material, section):
        assert 0 <= material < ATLAS_COLUMNS * ATLAS_ROWS
        u = tuple(points[1][j] - points[0][j] for j in range(3))
        v = tuple(points[2][j] - points[0][j] for j in range(3))
        n = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
        length = math.sqrt(sum(x*x for x in n))
        assert length > 1e-7 and all(math.isfinite(x) for p in points for x in p)
        self.triangles.append((points, uv, tuple(x/length for x in n), material, section))

    def quad(self, points, material, section, uv=((0,0), (0,1), (1,1), (1,0))):
        for indexes in ((0,1,2), (0,2,3)):
            self.triangle(tuple(points[i] for i in indexes), tuple(uv[i] for i in indexes), material, section)

    def ground(self):
        # Mirror adjacent atlas patches: both sides of each shared edge read identical texels.
        for ix, x in enumerate(range(-50,50,5)):
            for iz, z in enumerate(range(-74,86,5)):
                points = tuple((a,ground_height(a,b),b) for a,b in
                               ((x,z),(x,z+5),(x+5,z+5),(x+5,z)))
                uv = tuple((1-u if ix%2 else u,1-v if iz%2 else v)
                           for u,v in ((0,0),(0,1),(1,1),(1,0)))
                self.quad(points,GRASS,"ground",uv)

    def rectangle(self, x0, x1, z0, z1, y, material, section, tile=2):
        nx, nz = math.ceil((x1-x0)/tile), math.ceil((z1-z0)/tile)
        for ix in range(nx):
            for iz in range(nz):
                a, b = x0+(x1-x0)*ix/nx, x0+(x1-x0)*(ix+1)/nx
                c, d = z0+(z1-z0)*iz/nz, z0+(z1-z0)*(iz+1)/nz
                self.quad(((a,y,c),(a,y,d),(b,y,d),(b,y,c)), material, section)

    def box(self, x, y, z, rx, ry, rz, material, section):
        a,b,c,d = (x-rx,y+ry,z-rz),(x-rx,y+ry,z+rz),(x+rx,y+ry,z+rz),(x+rx,y+ry,z-rz)
        e,f,g,h = (x-rx,y-ry,z-rz),(x-rx,y-ry,z+rz),(x+rx,y-ry,z+rz),(x+rx,y-ry,z-rz)
        for points in ((a,b,c,d),(b,f,g,c),(d,h,e,a),(a,e,f,b),(c,g,h,d),(e,h,g,f)):
            self.quad(points, material, section)

    def crown(self, x, y, z, rx, ry, rz):
        self.canopy((((x,y,z),(rx,ry,rz)),))

    def branch(self, start, end, radius, tip_radius, section="forest_trunks"):
        direction = tuple(b-a for a, b in zip(start, end))
        length = math.sqrt(sum(v*v for v in direction))
        if not math.isfinite(length) or length < .001 or not 0 < tip_radius <= radius < 1:
            raise ValueError("Invalid branch")
        axis = tuple(v/length for v in direction)
        helper = (1,0,0) if abs(axis[0]) < .8 else (0,0,1)
        def cross(a, b):
            return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
        u = cross(axis, helper)
        norm = math.sqrt(sum(v*v for v in u))
        u = tuple(v/norm for v in u)
        v = cross(axis, u)
        def ring(center, size):
            return [tuple(center[i]+size*(a*u[i]+b*v[i]) for i in range(3))
                    for a, b in ((-1,-1),(1,-1),(1,1),(-1,1))]
        base, tip = ring(start, radius), ring(end, tip_radius)
        for i in range(4):
            j = (i+1) % 4
            self.quad((base[i],base[j],tip[j],tip[i]), WOOD, section)
        self.quad(tuple(reversed(base)), WOOD, section)
        self.quad(tuple(tip), WOOD, section)

    def canopy(self, volumes):
        rooted = tuple(((x,y-GROUND_Y,z),radii) for (x,y,z),radii in volumes)
        append_volume(self, rooted, FOLIAGE_CELL, FOLIAGE, "forest",
                      origin=(0,GROUND_Y,0), flat_color=True)

    def tree(self, x, z, height, spread, seed, trunk_section="forest_trunks"):
        if not all(math.isfinite(v) and abs(v) <= 45 for v in (x,z)):
            raise ValueError("Tree origin outside scenery bounds")
        shape = make_tree_shape(height,spread,seed)
        y = ground_height(x,z)
        def world(p):
            return (x+p[0],y+p[1],z+p[2])
        first = len(self.triangles)
        for start,end,radius,tip in shape.branches:
            self.branch(world(start),world(end),radius,tip,trunk_section)
        self.canopy(tuple((world(center),radii) for center,radii in shape.canopies))
        if len(self.triangles)-first > MAX_TREE_TRIANGLES:
            raise ValueError("Tree mesh budget exceeded")

    def save(self, name="countryside", metadata=True):
        assert len(self.triangles) <= MAX_TRIANGLES
        lines = ["# Generated by tools/title/generate_title_landscape.py; original geometry.","o RuralTitleLandscape","s off"]
        for points, uv, normal, material, _ in self.triangles:
            # Model loader reflects X and flips winding/UV: export its inverse here.
            for index in (0,2,1):
                p = points[index]
                lines.append(f"v {-p[0]:.7f} {p[1]:.7f} {p[2]:.7f}")
                u,v = uv[index]
                u = (material%ATLAS_COLUMNS+ATLAS_INSET+u*(1-2*ATLAS_INSET))/ATLAS_COLUMNS
                v = (material//ATLAS_COLUMNS+ATLAS_INSET+v*(1-2*ATLAS_INSET))/ATLAS_ROWS
                lines.append(f"vt {u:.7f} {1-v:.7f}")
            lines.append(f"vn {-normal[0]:.7f} {normal[1]:.7f} {normal[2]:.7f}")
        for i in range(len(self.triangles)):
            lines.append("f " + " ".join(f"{i*3+j+1}/{i*3+j+1}/{i+1}" for j in range(3)))
        OUT.mkdir(parents=True, exist_ok=True)
        destination = OUT/(name+".obj")
        content = "\n".join(lines)+"\n"
        if not destination.exists() or destination.read_text(encoding="ascii") != content:
            temporary = destination.with_suffix(".obj.tmp")
            temporary.write_text(content,encoding="ascii")
            temporary.replace(destination)
        if not metadata:
            return
        metadata = {"triangles":len(self.triangles),"sections":dict(Counter(t[4] for t in self.triangles)),
                    "materials":sorted(set(t[3] for t in self.triangles)),"groundY":GROUND_Y,
                    "atlasColumns":ATLAS_COLUMNS,"atlasRows":ATLAS_ROWS,"atlasInset":ATLAS_INSET}
        (OUT/"countryside_mesh.json").write_text(json.dumps(metadata,indent=2)+"\n",encoding="ascii")
        print(json.dumps(metadata))


def build():
    mesh = Mesh()
    mesh.ground()
    road = ((11,-16),(11,-3),(9.5,4),(7,11),(8.5,20),(12,32))
    road_y, half_width, median = GROUND_Y+.02, .95, .18
    for (x0,z0),(x1,z1) in zip(road,road[1:]):
        count = math.ceil((z1-z0)/2)
        for step in range(count):
            a,b = step/count,(step+1)/count
            xa,za = x0+(x1-x0)*a,z0+(z1-z0)*a
            xb,zb = x0+(x1-x0)*b,z0+(z1-z0)*b
            mesh.quad(((xa-half_width,road_y,za),(xb-half_width,road_y,zb),
                       (xb+half_width,road_y,zb),(xa+half_width,road_y,za)),DIRT,"road")
            mesh.quad(((xa-median,road_y+.008,za),(xb-median,road_y+.008,zb),
                       (xb+median,road_y+.008,zb),(xa+median,road_y+.008,za)),GRASS,"road_median")
            for side in (-1,1):
                edge = side*(half_width+.14)
                mesh.quad(((xa+edge-.12,road_y+.004,za),(xb+edge-.12,road_y+.004,zb),
                           (xb+edge+.12,road_y+.004,zb),(xa+edge+.12,road_y+.004,za)),GRASS,"road_verge")
        if z0 >= 3:
            mesh.box(x0+half_width+.35,GROUND_Y+.25,z0,.035,.28,.035,WOOD,"road_posts")
    rng = random.Random(20260930)
    for index,(x,z) in enumerate(TREE_POSITIONS):
        height = rng.uniform(4.8,6.2)
        mesh.tree(x,z,height,rng.uniform(1.6,2.1),20261021+index)
    for x,z in BUSH_POSITIONS:
        mesh.crown(x,ground_height(x,z)+.55,z,.85,.55,.65)
    return mesh


if __name__ == "__main__":
    landscape = build()
    landscape.save()
    reflection = Mesh()
    # The flat ground underside must not occlude the reflected sky below the water plane.
    reflection.triangles = [t for t in landscape.triangles if t[4] in ("forest", "forest_trunks", "road_posts")]
    reflection.save("reflection_scenery", metadata=False)

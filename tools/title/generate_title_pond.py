"""Build original retained stone shoreline from the runtime pond configuration."""
from pathlib import Path
import json
import math
import random
from generate_title_landscape import Mesh, GRASS, GROUND_Y
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "project/Resources/title"
SEGMENTS = 64


def build():
    config = json.loads((OUT / "pond_layout.json").read_text(encoding="utf-8"))
    x, water_y, z = config["center"]
    rx, rz = config["radii"]
    width = config["shoreWidth"]
    mesh = Mesh()
    randomizer = random.Random(20261014)
    for index in range(SEGMENTS):
        a, b = math.tau * (index+.025) / SEGMENTS, math.tau * (index + .975) / SEGMENTS
        middle = (a+b)*.5
        # Leave a real opening for the incoming water instead of drawing a ring across it.
        if abs(x+rx*math.cos(middle)-config["outfall"][-1][0]) < config["outfallHalfWidth"]+.25 and math.sin(middle)>.5:
            continue
        height = GROUND_Y + randomizer.uniform(.06, .10)
        crown = width*randomizer.uniform(.72,.92)
        def point(angle, offset, y):
            return (x + (rx + offset) * math.cos(angle), y,
                    z + (rz + offset) * math.sin(angle))
        rings = [(0,water_y-.08),(.045,height-.08),(crown*.4,height),
                 (crown,height-.04),(width,GROUND_Y-.01)]
        for ring, ((ra,ya),(rb,yb)) in enumerate(zip(rings,rings[1:])):
            mesh.quad((point(a,ra,ya),point(b,ra,ya),point(b,rb,yb),point(a,rb,yb)),GRASS,
                      "stone_inner" if ring == 0 else "stone_top")
        for angle, reverse in ((a,False),(b,True)):
            outline = [point(angle,r,h) for r,h in rings]
            for i in range(1,len(outline)-1):
                points = (outline[0],outline[i],outline[i+1])
                if reverse:
                    points = points[::-1]
                mesh.triangle(points,((0,1),(.5,0),(1,1)),GRASS,"stone_joint")
        # Individual rounded rocks break up the continuous retaining soil beneath them.
        radial = width*.5
        cx,cz = x+(rx+radial)*math.cos(middle),z+(rz+radial)*math.sin(middle)
        half_length = math.hypot(rx*math.sin(middle),rz*math.cos(middle))*math.pi/SEGMENTS*.98
        rock_height = randomizer.uniform(.11,.23)
        facets = 7
        rotation = randomizer.uniform(-.15,.15)
        rings = ((water_y-.035,.70),(GROUND_Y+.045,1.0),(GROUND_Y+rock_height,.50))
        def rock_point(level, angle_index):
            cy,scale = rings[level]
            angle = math.tau*angle_index/facets+rotation
            tangent = math.cos(angle)*half_length*scale
            outward = math.sin(angle)*radial*.98*scale
            return (cx-tangent*math.sin(middle)+outward*math.cos(middle),cy,
                    cz+tangent*math.cos(middle)+outward*math.sin(middle))
        for level in range(2):
            for side in range(facets):
                mesh.quad((rock_point(level,side),rock_point(level+1,side),
                           rock_point(level+1,side+1),rock_point(level,side+1))[::-1],GRASS,"boulder")
        for side in range(facets):
            mesh.triangle(((cx,GROUND_Y+rock_height+.025,cz),rock_point(2,side),rock_point(2,side+1)),
                          ((.5,.5),(0,1),(1,1)),GRASS,"boulder")
    # A lined, downhill channel from the final farm cell to the receiving pond.
    half_width = config["outfallHalfWidth"]
    for start,end in zip(config["outfall"],config["outfall"][1:]):
        xa,ya,za = start
        xb,yb,zb = end
        mesh.quad(((xa-half_width,ya-.018,za),(xb-half_width,yb-.018,zb),
                   (xb+half_width,yb-.018,zb),(xa+half_width,ya-.018,za))[::-1],GRASS,"outfall_bed")
        for side in (-1,1):
            inner,outer = side*half_width,side*(half_width+.15)
            low_a,low_b = ya-.12,yb-.12
            top_a,top_b = ya+.14,yb+.14
            mesh.quad(((xa+min(inner,outer),top_a,za),(xb+min(inner,outer),top_b,zb),
                       (xb+max(inner,outer),top_b,zb),(xa+max(inner,outer),top_a,za))[::-1],GRASS,"outfall_bank")
            inner_face = ((xa+inner,low_a,za),(xb+inner,low_b,zb),(xb+inner,top_b,zb),(xa+inner,top_a,za))
            outer_face = ((xa+outer,top_a,za),(xb+outer,top_b,zb),(xb+outer,low_b,zb),(xa+outer,low_a,za))
            mesh.quad(inner_face if side<0 else inner_face[::-1],GRASS,"outfall_side")
            mesh.quad(outer_face if side<0 else outer_face[::-1],GRASS,"outfall_side")
    return mesh


def save():
    mesh = build()
    lines = ["# Original stone shoreline; tools/title/generate_title_pond.py", "o TitlePondShore", "s off"]
    for triangle_index, (points, uv, normal, _, section) in enumerate(mesh.triangles):
        tile = (triangle_index // 14) % 8
        for index in (0, 2, 1):
            x, y, z = points[index]
            u, v = uv[index]
            u = (tile % 4 + .04 + u * .92) / 4
            v = (tile // 4 + .04 + v * .92) / 2
            lines.extend((f"v {-x:.7f} {y:.7f} {z:.7f}", f"vt {u:.7f} {1-v:.7f}"))
        lines.append(f"vn {-normal[0]:.7f} {normal[1]:.7f} {normal[2]:.7f}")
    for index in range(len(mesh.triangles)):
        lines.append("f " + " ".join(f"{index*3+j+1}/{index*3+j+1}/{index+1}" for j in range(3)))
    (OUT / "pond_shore.obj").write_text("\n".join(lines) + "\n", encoding="ascii")
    atlas = Image.new("RGB", (128, 64))
    rng = random.Random(20261014)
    for tile in range(8):
        base = rng.randrange(118, 156)
        patches = [[rng.randrange(-14,15) for _ in range(8)] for _ in range(8)]
        for y in range(32):
            for x in range(32):
                grain = rng.randrange(-4, 5)+patches[y//4][x//4]
                fissure = -18 if (x+int(3*math.sin(y*.26))+tile*3)%29 < 1 else 0
                value = base+grain+fissure
                moss = 7 if y>22 and patches[y//4][x//4]<0 else 0
                atlas.putpixel((tile%4*32+x,tile//4*32+y),(value+5-moss,value+8,value-3-moss))
    atlas.save(OUT / "pond_stone.png", optimize=True)
    print(f"PASS original pond shore: {len(mesh.triangles)} triangles")


if __name__ == "__main__":
    save()

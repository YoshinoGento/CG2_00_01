"""Check opaque grass mesh, loader transforms and conservative wind-swept placement."""
import json
import math
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools/title"))
import generate_title_grass as grass
import generate_title_landscape as landscape


def overlap_box(points, rectangle):
    x0,x1,z0,z1 = rectangle
    return max(p[0] for p in points) >= x0 and min(p[0] for p in points) <= x1 and \
        max(p[2] for p in points) >= z0 and min(p[2] for p in points) <= z1


def wave_deform(point, time, strength):
    direction = (math.cos(-.45),-math.sin(-.45))
    phase = time-(point[0]*direction[0]+point[2]*direction[1])*.85
    crest = .5+.5*math.sin(phase)
    height = max(point[1],0)
    gust = .08+.92*crest*crest
    derivative = .92*crest*math.cos(phase)
    gradient = (-direction[0]*.85*derivative*height*height/grass.BEND_HEIGHT,
                2*height/grass.BEND_HEIGHT*gust,-direction[1]*.85*derivative*height*height/grass.BEND_HEIGHT)
    wind = (strength*direction[0],0,strength*direction[1])
    weight = height*height/grass.BEND_HEIGHT*gust
    return tuple(a+b*weight for a,b in zip(point,wind)),gradient,wind,crest


def check_traveling_wave():
    direction = (math.cos(-.45),-math.sin(-.45))
    for strength in (-grass.MAX_WIND,0,grass.MAX_WIND):
        for height in (0,.1,grass.MAX_HEIGHT*1.2):
            for phase in (0,.4,1.5,3,5,math.tau):
                point = (3,height,7)
                result,gradient,wind,crest = wave_deform(point,phase,strength)
                determinant = 1+sum(a*b for a,b in zip(wind,gradient))
                assert determinant > .65 and all(math.isfinite(v) for v in result)
                normal = (.3,.8,.4)
                factor = sum(a*b for a,b in zip(wind,normal))/determinant
                corrected = tuple(a-b*factor for a,b in zip(normal,gradient))
                for tangent in ((normal[1],-normal[0],0),(0,-normal[2],normal[1])):
                    epsilon = 1e-6
                    ahead = wave_deform(tuple(p+epsilon*t for p,t in zip(point,tangent)),phase,strength)[0]
                    behind = wave_deform(tuple(p-epsilon*t for p,t in zip(point,tangent)),phase,strength)[0]
                    bent_tangent = tuple((a-b)/(2*epsilon) for a,b in zip(ahead,behind))
                    tolerance = 2e-7 if height == 0 else 1e-8
                    assert abs(sum(a*b for a,b in zip(corrected,bent_tangent))) < tolerance
                # Advancing time transports the same crest along the cloud axis, not all at once.
                for step in (-1,1):
                    moved = (point[0]+direction[0]*step,point[1],point[2]+direction[1]*step)
                    assert abs(wave_deform(moved,phase+.85*step,strength)[3]-crest) < 1e-12
                if height == 0: assert result == point


def check_slope_normal():
    direction = (math.cos(-.45),-math.sin(-.45))
    for x,z in ((-31.3,40.2),(-20.7,58.6),(27.1,35.7)):
        epsilon = 1e-5
        base = grass.terrain_height(x,z)
        gx = (grass.terrain_height(x+epsilon,z)-grass.terrain_height(x-epsilon,z))/(2*epsilon)
        gz = (grass.terrain_height(x,z+epsilon)-grass.terrain_height(x,z-epsilon))/(2*epsilon)
        for height in (0,.25,grass.MAX_HEIGHT*1.05):
            for time in (0,.9,2,3.7,5):
                # Compose terrain shear followed by height-relative wave deformation, as in the VS.
                def deform(p):
                    ground = grass.terrain_height(p[0],p[2])
                    bent = wave_deform((p[0],p[1],p[2]),time,grass.MAX_WIND)[0]
                    return bent[0],ground+p[1],bent[2]
                point = (x,height,z)
                _,flat_gradient,wind,_ = wave_deform(point,time,grass.MAX_WIND)
                gradient = (flat_gradient[0]-gx*flat_gradient[1],flat_gradient[1],flat_gradient[2]-gz*flat_gradient[1])
                normal = (.3,.8,.4)
                ground_normal = (normal[0]-gx*normal[1],normal[1],normal[2]-gz*normal[1])
                determinant = 1+sum(a*b for a,b in zip(wind,gradient))
                assert determinant > .65
                factor = sum(a*b for a,b in zip(wind,ground_normal))/determinant
                corrected = tuple(a-b*factor for a,b in zip(ground_normal,gradient))
                for tangent in ((normal[1],-normal[0],0),(0,-normal[2],normal[1])):
                    ahead = deform(tuple(p+1e-6*t for p,t in zip(point,tangent)))
                    behind = deform(tuple(p-1e-6*t for p,t in zip(point,tangent)))
                    derivative = tuple((a-b)/2e-6 for a,b in zip(ahead,behind))
                    assert abs(sum(a*b for a,b in zip(corrected,derivative))) < 4e-7
                if height == 0: assert deform(point) == (x,base,z)


def main():
    check_traveling_wave()
    check_slope_normal()
    mesh = grass.build()
    assert mesh.triangles == grass.build().triangles
    triangles = grass.TUFT_COUNT*grass.BLADE_COUNT*(2*grass.SEGMENT_COUNT-1)
    assert len(mesh.triangles) == triangles
    road_triangles = [t[0] for t in landscape.build().triangles if t[4] in ("road","road_median","road_verge","road_posts")]
    road_bounds = [(min(p[0] for p in t)-.1,max(p[0] for p in t)+.1,
                    min(p[2] for p in t)-.1,max(p[2] for p in t)+.1) for t in road_triangles]
    assets = ROOT/"project/Resources/title"
    layout = json.loads((assets/"grass_layout.json").read_text())
    metadata = json.loads((assets/"grass_mesh.json").read_text())
    far = grass.build(3,2)
    assert len(far.triangles) == metadata["farTrianglesPerPatch"] == 864
    assert far.triangles == grass.build(3,2).triangles
    assert all(math.isfinite(value) for triangle in far.triangles for p in triangle[0] for value in p)
    assert max(p[1] for triangle in far.triangles for p in triangle[0]) <= grass.MAX_HEIGHT
    far_lines = (assets/"grass_far_patch.obj").read_text(encoding="ascii").splitlines()
    assert sum(line.startswith("f ") for line in far_lines) == len(far.triangles)
    assert layout["version"] == 1 and layout["patches"] == grass.patch_layout()
    assert len(layout["patches"]) <= grass.MAX_PATCHES
    assert len(layout["patches"]) >= 300 and max(p["position"][2] for p in layout["patches"]) > 70
    distances = [math.hypot(p["position"][0],p["position"][2]-6) for p in layout["patches"]]
    assert distances == sorted(distances)
    assert layout["shadowCount"] == sum(d <= grass.SHADOW_DISTANCE for d in distances)
    assert metadata["shadowInstances"] == layout["shadowCount"]
    assert grass.MAX_HEIGHT*1.05 < .5  # Trees are approximately 2.6--5.2 units tall.
    assert metadata["trianglesPerPatch"] == len(mesh.triangles) and metadata["opaque"]
    for _,_,normal,_,_ in mesh.triangles:
        assert abs(sum(n*n for n in normal)-1)<1e-6
    points = [p for triangle in mesh.triangles for p in triangle[0]]
    # Whole-patch AABB plus maximum bend is conservative even between sparse blade vertices.
    for patch in layout["patches"]:
        x,y,z = patch["position"]
        assert y == grass.terrain_height(x,z)
        scale, yaw = patch["scale"],patch["yaw"]
        assert .75 <= scale <= 1.2 and patch["tint"][3] == 1
        c,s = math.cos(yaw),math.sin(yaw)
        transformed = [(x+scale*(p[0]*c+p[2]*s),y+scale*p[1],z+scale*(-p[0]*s+p[2]*c)) for p in points]
        reach = grass.MAX_WIND*max((p[1]*scale)**2/grass.BEND_HEIGHT for p in points)
        swept = [(min(p[0] for p in transformed)-reach,y,min(p[2] for p in transformed)-reach),
                 (max(p[0] for p in transformed)+reach,y,max(p[2] for p in transformed)+reach)]
        assert not overlap_box(swept,landscape.FARM_EXCLUSION), patch
        assert not any(overlap_box(swept,bounds) for bounds in road_bounds), patch
    # Barycentric heights match every actual landscape triangle, including rolling hills.
    for triangle in landscape.build().triangles:
        if triangle[4] != "ground": continue
        a,b,c = triangle[0]
        x,z = sum(p[0] for p in (a,b,c))/3,sum(p[2] for p in (a,b,c))/3
        assert abs(grass.terrain_height(x,z)-(a[1]+b[1]+c[1])/3) < 1e-12
    vertices,uv,normals,faces = [],[],[],[]
    for line in (assets/"grass_patch.obj").read_text(encoding="ascii").splitlines():
        values = line.split()
        if not values or values[0].startswith("#"): continue
        if values[0] in ("v","vt","vn"):
            result = tuple(map(float,values[1:])); assert all(math.isfinite(v) for v in result)
            {"v":vertices,"vt":uv,"vn":normals}[values[0]].append(result)
        elif values[0] == "f":
            assert len(values) == 4
            faces.append([tuple(map(int,v.split("/"))) for v in values[1:]])
    assert len(faces) == len(mesh.triangles) and min(p[1] for p in vertices) == 0
    for i,face in enumerate(faces):
        for j,(v,t,n) in enumerate(face):
            assert 1 <= v <= len(vertices) and 1 <= t <= len(uv) and 1 <= n <= len(normals)
            source_index = (0,2,1)[j]
            source = mesh.triangles[i][0][source_index]
            exported = vertices[v-1]
            assert max(abs(exported[k]*(-1 if k==0 else 1)-source[k]) for k in range(3))<1e-6
            assert uv[t-1][0] in (.125,.375,.625,.875) and 0 <= uv[t-1][1] <= 1
        a,b,c = (vertices[v-1] for v,_,_ in face)
        u,w = ([b[k]-a[k] for k in range(3)],[c[k]-a[k] for k in range(3)])
        cross = (u[1]*w[2]-u[2]*w[1],u[2]*w[0]-u[0]*w[2],u[0]*w[1]-u[1]*w[0])
        assert sum(cross[k]*normals[face[0][2]-1][k] for k in range(3)) > 1e-8
    for slope in (-grass.MAX_WIND,0,grass.MAX_WIND):
        for height in (0,.1,grass.MAX_HEIGHT*1.2):
            bend = slope*height*height/grass.BEND_HEIGHT
            assert math.isfinite(bend) and (height != 0 or bend == 0)
            assert abs(bend) <= grass.MAX_WIND*(grass.MAX_HEIGHT*1.2)**2/grass.BEND_HEIGHT + 1e-12
    # The inverse-transpose bend normal must remain perpendicular to both bent tangents.
    for height in (0,.1,.6):
        for wx,wz in ((.22,0),(-.1,.15),(0,0)):
            derivative = 2*height/grass.BEND_HEIGHT
            normal = (.3,.8,.4)
            bent_normal = (normal[0],normal[1]-derivative*(wx*normal[0]+wz*normal[2]),normal[2])
            for tangent in ((normal[1],-normal[0],0),(0,-normal[2],normal[1])):
                bent_tangent = (tangent[0]+wx*derivative*tangent[1],tangent[1],
                                tangent[2]+wz*derivative*tangent[1])
                assert abs(sum(a*b for a,b in zip(bent_normal,bent_tangent))) < 1e-12
    ns = {"m":"http://schemas.microsoft.com/developer/msbuild/2003"}
    tree = ET.parse(ROOT/"project/CG2_00_01.vcxproj")
    for entry in ("application\\title\\TitleGrassRenderer.cpp","application\\title\\TitleGrassRenderer.h",
                  "application\\title\\TitleGrassPresentation.h","Resources\\shader\\TitleGrass.VS.hlsl",
                  "Resources\\shader\\TitleGrass.PS.hlsl","Resources\\title\\grass_patch.obj"):
        assert len(tree.findall(f".//*[@Include='{entry}']",ns)) == 1, entry
    print(f"PASS grass:{triangles} triangles/{len(layout['patches'])} patches/{grass.TUFT_COUNT*len(layout['patches'])} tufts, deterministic/index/UV/normals/winding, wind-swept farm/road clearance, root anchors, XML")


if __name__ == "__main__":
    main()

"""Deterministic shoreline, staging clearance and root/Shader layout contracts."""
import json
import math
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools/title"))
import generate_title_pond as pond
import generate_title_landscape as landscape
import generate_title_grass as grass

assets = ROOT/"project/Resources/title"
config = json.loads((assets/"pond_layout.json").read_text())
px,wy,pz = config["center"]
rx,rz = config["radii"]
shore = config["shoreWidth"]
mesh = pond.build()
assert mesh.triangles == pond.build().triangles
assert 2500 <= len(mesh.triangles) <= 4000
assert pz+rz+shore < landscape.FARM_EXCLUSION[2]
assert -1 < wy < landscape.GROUND_Y
vertices,faces = [],[]
for line in (assets/"pond_shore.obj").read_text(encoding="ascii").splitlines():
    words = line.split()
    if words and words[0] == "v":
        vertices.append(tuple(map(float,words[1:])))
    elif words and words[0] == "f":
        faces.append([int(word.split("/")[0]) for word in words[1:]])
assert len(faces) == len(mesh.triangles)
for index,(points,_,normal,_,section) in enumerate(mesh.triangles):
    assert all(math.isfinite(value) for p in points for value in p)
    assert abs(sum(value*value for value in normal)-1)<1e-6
    if section in ("stone_inner","stone_top","outfall_bed","outfall_bank"):
        assert normal[1] > 0
    for j in range(3):
        exported = vertices[faces[index][(0,2,1)[j]]-1]
        expected = points[j]
        assert max(abs(exported[k]*(-1 if k == 0 else 1)-expected[k]) for k in range(3))<1e-6
    if section.startswith("stone_"):
        assert all(((p[0]-px)/rx)**2+((p[2]-pz)/rz)**2 >= .99999 for p in points)

outfall = config["outfall"]
assert outfall[0] == [0,.145,2.64]
for a,b in zip(outfall,outfall[1:]):
    assert a[0] == b[0] and a[1] > b[1] and a[2] > b[2]
mouth = outfall[-1]
assert mouth[1] == wy and math.hypot((mouth[0]-px)/rx,(mouth[2]-pz)/rz)<.98
assert any(t[4]=="outfall_bed" for t in mesh.triangles)

def in_triangle(point, triangle):
    a,b,c = triangle
    area = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    if abs(area)<1e-10:
        return False
    signs = []
    for a,b in zip(triangle,triangle[1:]+triangle[:1]):
        signs.append((b[0]-a[0])*(point[1]-a[1])-(b[1]-a[1])*(point[0]-a[0]))
    return min(signs)>=-1e-8 or max(signs)<=1e-8

# Sample the shoreline against every road triangle, not just its large enclosing box.
road = [[(p[0],p[2]) for p in t[0]] for t in landscape.build().triangles
        if t[4] in ("road","road_median","road_verge","road_posts")]
for index in range(512):
    angle = math.tau*index/512
    for width in (0,shore*.5,shore):
        point = (px+(rx+width)*math.cos(angle),pz+(rz+width)*math.sin(angle))
        overlaps = [triangle for triangle in road if in_triangle(point,triangle)]
        assert not overlaps, (point,overlaps[0])
for patch in grass.patch_layout():
    x,_,z = patch["position"]
    scale = patch["scale"]
    extent = scale*(grass.PATCH_RADIUS+grass.MAX_LEAN+grass.MAX_WIDTH/2)+grass.MAX_WIND*(grass.MAX_HEIGHT*scale)**2/grass.BEND_HEIGHT
    assert not (x+extent >= px-rx-shore and x-extent <= px+rx+shore and
                z+extent >= pz-rz-shore and z-extent <= pz+rz+shore)

for name in ("CG2_00_01.vcxproj","CG2_00_01.vcxproj.filters"):
    entries = [entry.attrib["Include"] for entry in ET.parse(ROOT/"project"/name).iter() if "Include" in entry.attrib]
    assert len(entries) == len(set(entries))
    for suffix in ("TitlePondSystem.cpp","TitlePondRenderer.cpp","TitlePondSystem.h","TitlePondRenderer.h","TitlePond.PS.hlsl","pond_shore.obj","pond_layout.json","pond_stone.png"):
        assert sum(path.endswith(suffix) for path in entries) == 1
renderer = (ROOT/"project/application/scene/RippleTransitionRenderer.cpp").read_text()
shader = (ROOT/"project/Resources/shader/RippleTransition.PS.hlsl").read_text()
assert "8+kMaximumWaterDrops*4+4+4+4+12" in renderer
assert "values[pondMappingOffset+row*4+column]" in renderer and "float4 pondX, pondZ, pondDivisor;" in shader
assert "PondMask(input.texcoord)" in shader and "pondFlight.xy*resolution" in shader
assert "pondFlight.z > .5 ? WipeRadius()" in shader
assert "inside *= smoothstep(kPondCoverOpacityStart,kPondCoverOpacityEnd,progress)" in shader
scene = (ROOT/"project/application/scene/TitleScene.cpp").read_text()
assert "pondReady_ = pondLayoutReady && worldReady" in scene
assert "if (!pondReady_ || pond_.RequestStart()) presentation_.RequestStart();" in scene
assert "if (!pondReady_ || pond_.RequestStart(settings_.GetWaterClick())) presentation_.RequestStart();" in scene
assert "if (pondReady_) pondRenderer_.Draw(pond_.GetFrame(),renderer_.GetReflectionSrv());" in scene
for name in ("TitleReflectionRenderer.cpp","TitleReflectionRenderer.h","TitleReflection.hlsli","TitleReflection.VS.hlsl","TitleReflection.PS.hlsl","TitleGrassReflection.PS.hlsl","reflection_scenery.obj"):
    assert sum(p.endswith(name) for p in entries)==1
reflection = (ROOT/"project/application/title/TitleReflectionRenderer.cpp").read_text()
pond_renderer = (ROOT/"project/application/title/TitlePondRenderer.cpp").read_text()
grass_renderer = (ROOT/"project/application/title/TitleGrassRenderer.cpp").read_text()
farm_renderer = (ROOT/"project/application/title/TitleFarmRenderer.cpp").read_text()
assert "kConstantCount = 36" in pond_renderer and "parameters[3].DescriptorTable" in pond_renderer
assert "D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE" in reflection
assert "commands->OMSetRenderTargets(0,nullptr,FALSE,nullptr)" in reflection
assert "dx_->SetSceneRenderTarget()" in reflection and "srv_->Release(srvIndex_)" in reflection
assert "*reflectionMapped_ = *mainMapped_" in grass_renderer
assert "farModel_->Draw(commands,count_)" in grass_renderer
assert "reflectionConstants_->Unmap" in grass_renderer
assert farm_renderer.index("common_->EndShadowPass()") < farm_renderer.index("reflection_.Begin()")
assert "i<activeGlintCount_" in farm_renderer and "part.water ? canalColor" in farm_renderer

# CPU reference only: mirror-plane invariants and nonuniform/sheared surface normals.
def dot(a,b):
    return sum(x*y for x,y in zip(a,b))

def cross(a,b):
    return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])

for elevation in (-2,wy,0,.145,1,8):
    reflected = 2*wy-elevation
    assert abs((2*wy-reflected)-elevation)<1e-12
    assert abs((reflected-wy)+(elevation-wy))<1e-12
for sx,sy,sz in ((1,1,1),(.36,.004,.56),(.11,.65,.18)):
    for slope in (-.8,0,.28,.9):
        x_axis,z_axis = (sx,0,0),(0,sz*slope,sz)
        normal = cross(z_axis,x_axis)
        assert dot(normal,normal)>0
        assert abs(dot(normal,x_axis))<1e-12 and abs(dot(normal,z_axis))<1e-12
print(f"PASS pond {len(mesh.triangles)} triangles / finite-winding / downhill outfall / farm-road-grass clearance / registrations")
print("PASS reflection CPU geometry / independent-buffer and Barrier source contracts (not GPU validation)")

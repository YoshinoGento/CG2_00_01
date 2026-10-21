"""Opt-in constant layouts, reflection parity and bounded hemispherical fill; not GPU readback."""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
def read(path):
    return (ROOT/path).read_text(encoding='utf-8')

obj = read('project/engine/3d/Object3d.cpp')
header = read('project/engine/3d/Object3d.h')
shared = read('project/Resources/shader/OutdoorLighting.hlsli')
main = read('project/Resources/shader/Object3D.PS.hlsl')
reflection = read('project/Resources/shader/TitleReflection.PS.hlsl')
renderer = read('project/application/title/TitleReflectionRenderer.cpp')
assert 'materialData_->outdoorAmbient = {}' in obj
assert '!materialData_ || !std::isfinite(skyTint.x)' in obj
assert 'offsetof(Material, outdoorAmbient) == 112' in header and 'sizeof(Material) == 128' in header
for shader in (main, reflection):
    assert '#include "OutdoorLighting.hlsli"' in shader and 'EvaluateAmbientLight(' in shader
assert 'if (skyTint.w <= 0.0f) return legacy;' in shared
assert 'values.data()+48,&outdoorAmbient_' in renderer
assert 'kRootConstants = 52' in read('project/application/title/TitleReflectionRenderer.h')
assert 'kRootConstants+1 <= 64' in read('project/application/title/TitleReflectionRenderer.h')
for path in ('project/application/title/TitleFarmRenderer.cpp',
             'project/application/farm/render/FarmArrivalRenderer.cpp'):
    source = read(path)
    assert 'BuildOutdoorAmbientTint(' in source and 'SetOutdoorAmbient(' in source
for project in ('project/CG2_00_01.vcxproj','project/CG2_00_01.vcxproj.filters'):
    entries = [e.attrib['Include'].replace('\\','/') for e in ET.parse(ROOT/project).iter() if 'Include' in e.attrib]
    for suffix in ('FarmOutdoorLighting.h','OutdoorLighting.hlsli'):
        assert sum(e.endswith(suffix) for e in entries) == 1

# Analytic reference of the shared shader: zero intensity stays black; undersides stay weaker.
assert 'ambientResponse = 0.18f' in shared
assert 'float3(0.42f, 0.38f, 0.30f)' in shared
previous = None
for i in range(1001):
    exposure = i/1000
    top = tuple(.18*exposure*t for t in (.84,.90,.96))
    bottom = tuple(a*b for a,b in zip(top,(.42,.38,.30)))
    assert all(0 <= b <= a <= .18 for a,b in zip(top,bottom))
    if previous is not None:
        assert all(0 <= a-b < .0002 for a,b in zip(top,previous))
    previous = top
print('PASS outdoor ambient opt-in/ABI/reflection parity/1001 bounded exposure steps; GPU QA separate')

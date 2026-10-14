"""Guard the scoped matte-material fix; this is not a GPU image test."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
renderer = (ROOT / "project/application/farm/render/FarmRenderer.cpp").read_text(encoding="utf-8")
shader = (ROOT / "project/Resources/shader/Object3d.PS.hlsl").read_text(encoding="utf-8")
assert renderer.count("object.SetSpecularType(Object3d::SpecularType::None);") == 1
assert "SpecularType::BlinnPhong" not in renderer
assert "object.SetEnableLighting(!part.water" in renderer
assert "void FarmRenderer::DrawShadow()" in renderer
assert "gMaterial.specularType == kSpecularTypeNone" in shader
assert "return 0.0f;" in shader.split("gMaterial.specularType == kSpecularTypeNone", 1)[1].split("}", 1)[0]
print("PASS farm material policy: pooled parts matte, diffuse/shadow paths retained; GPU check separate")

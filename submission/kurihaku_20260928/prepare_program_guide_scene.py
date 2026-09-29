"""Prepare a labeled illustration fixture; never alter the shipped starter save."""
import json
import zipfile
from prepare_submission import OUT, digest, write_json

archive = OUT / "SuidoNogyo_FreeFarming_20260928_04.zip"
target = OUT / "program_guide_scene_02"
if target.exists():
    raise RuntimeError("Illustration fixture exists; do not overwrite")
if digest(archive) != "dcd01a745ca9e7f140d5b5d661950f946e4881ea93ee6339be691239afe66042":
    raise RuntimeError("Unexpected delivery archive")
with zipfile.ZipFile(archive) as z:
    if z.testzip() is not None:
        raise RuntimeError("Bad archive CRC")
    z.extractall(target)
runtime = target / "SuidoNogyo"
save = next((runtime / "Settings/farm/saves").glob("*.json"))
doc = json.loads(save.read_text(encoding="utf-8"))
doc["date"]["timeScale"] = 1
doc["grid"]["selectedIndex"] = 3
for index in (4,9,14,19):
    doc["tiles"][index].update(feature="WaterSource" if index == 19 else "Canal",
                               height=1, waterAmount=0.85)
for index,crop,growth,moisture in ((3,"Carrot",0.95,0.65),(8,"Tomato",0.85,0.55),(13,"Pumpkin",0.90,0.65)):
    doc["tiles"][index].update(state="Planted",crop=crop,growth=growth,moisture=moisture,
                               soilNutrients=0.75)
write_json(save, doc)
write_json(target / "ILLUSTRATION_FIXTURE.json", {
    "purpose": "Configured illustration state for technical document; not a natural playthrough or benchmark",
    "archive_sha256": digest(archive), "pause_via_ui_after_start": True,
    "features": "source19; canals4,9,14 height1; crops3,8,13",
})
print(runtime)

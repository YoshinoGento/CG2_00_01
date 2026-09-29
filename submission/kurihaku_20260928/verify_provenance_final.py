"""Check the immutable delivery separately from the intentionally changed QA save."""
import hashlib
import json
import zipfile
from package_demo import original_saves, require
from package_provenance_runtime import TARGET, ARCHIVE, QA, BUILD
from prepare_submission import OUT, ROOT, digest, git, write_json

manifest = json.loads((TARGET / "MANIFEST.json").read_text(encoding="utf-8"))
with zipfile.ZipFile(ARCHIVE) as z:
    require(z.testzip() is None, "Bad CRC")
    for item in manifest["files"]:
        require(digest(TARGET / item["path"]) == item["sha256"], "Delivery changed")
        require(hashlib.sha256(z.read("SuidoNogyo/" + item["path"])).hexdigest() == item["sha256"], "ZIP mismatch")
    require(z.read("SuidoNogyo/MANIFEST.json") == (TARGET / "MANIFEST.json").read_bytes(), "Manifest mismatch")
    require(len(z.namelist()) == len(manifest["files"]) + 1, "Unexpected ZIP contents")
saved = list((TARGET / "Settings/farm/saves").glob("*.json"))
require(len(saved) == 1, "More than one delivery save")
initial = json.loads(saved[0].read_text(encoding="utf-8"))
require(initial["schemaVersion"] == 17 and initial["playMode"] == "FreeFarming", "Wrong mode")
require(initial["date"]["day"] == 1 and initial["economy"]["money"] == 300
        and initial["economy"]["seedCounts"] == [0,5,5,5], "Played delivery save")
require(all(t["state"] == "Empty" for t in initial["tiles"]), "Delivery farm modified")
qa = json.loads((QA / "SuidoNogyo/Settings/farm/saves" / saved[0].name).read_text(encoding="utf-8"))
require(qa["economy"]["money"] == 240 and qa["economy"]["seedCounts"] == [0,6,5,5]
        and qa["tiles"][0]["state"] == "Tilled", "QA save differs from live observations")
before = json.loads((OUT / "provenance_original_saves.json").read_text(encoding="utf-8"))
require(before == original_saves(), "Original saves changed")
old_manifest = json.loads((BUILD / "dependency_overrides.json").read_text(encoding="utf-8"))["original_manifest"]
for item in old_manifest["files"]:
    if item["path"].startswith(("project/engine/", "project/application/", "project/externals/")):
        require(digest(ROOT / item["path"]) == item["sha256"], "Production source/dependency changed")
require(git("rev-parse", "HEAD") == manifest["head"], "HEAD changed")
require(git("branch", "--show-current") == "個人就職作品", "Branch changed")
require(not git("diff", "--cached", "--name-only"), "Index changed")
result = json.loads((OUT / "provenance_package_result.json").read_text(encoding="utf-8"))
result.update({"sha256": digest(ARCHIVE), "runtime_check": "PASS same-PC launch/till/purchase/save/relaunch/restore",
               "original_source_dependencies_saves_unchanged": True, "pristine_single_save": True,
               "ui": "1280x720 client, startup/farm/shop/save menu",
               "not_tested": ["second PC", "full harvest-contest-30-day run with these binaries", "other GPU/DPI"],
               "school_foundation": "permitted per user confirmation; school models excluded"})
write_json(OUT / "provenance_package_result.json", result)
print(json.dumps(result, ensure_ascii=False, indent=2))

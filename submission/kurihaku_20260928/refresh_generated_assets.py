"""Explicit allowlisted presentation revision, with a before/after hash audit."""
import sys
import json
from prepare_submission import OUT, ROOT, copy_file, digest, write_json

build = OUT / "asset_clean_build"
manifest_path = build / "source_manifest.json"
manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
revision = sys.argv[1] if len(sys.argv) > 1 else "02"
assert revision in {"02", "03"}
record = build / f"resource_revision_{revision}.json"
assert not record.exists(), "Revision already recorded"
allowed = ({"project/Resources/farm/ground.obj", "project/Resources/farm/player_marker.obj",
            "tools/generate_farm_runtime_assets.py"} if revision == "02" else {
            "tools/generate_farm_runtime_assets.py", "tools/generate_farm_runtime_labels.py",
            "project/Resources/ui/font/ascii_bitmap_font.png", "project/Resources/ui/farm_runtime_menu.png",
            "project/application/farm/ui/FarmRuntimeLabels.h"})
changes = []
for item in manifest["files"]:
    src = ROOT / item["path"]
    snap = build / "work/source" / item["path"]
    if digest(src) == item["sha256"]:
        continue
    assert item["path"] in allowed, ("Unexpected source edit", item["path"])
    # Permit resuming only a copy of these exact current bytes after interruption.
    assert digest(snap) in {item["sha256"], digest(src)}, "Unexpected snapshot bytes"
    changes.append({"path": item["path"], "before": item["sha256"], "after": digest(src)})
assert {x["path"] for x in changes} == allowed, "Expected exact revision file set"
for item in manifest["files"]:
    if item["path"] not in allowed:
        continue
    snap = build / "work/source" / item["path"]
    copy_file(ROOT / item["path"], snap)
    item["sha256"], item["bytes"] = digest(snap), snap.stat().st_size
write_json(record, {"reason": "Add UVs" if revision == "02" else "Noto weight500 for readable glyphs",
    "changes": changes, "rebuild_required": revision == "03"})
write_json(manifest_path, manifest)
print(f"PASS: allowlisted revision {revision}; {len(changes)} file hashes recorded")

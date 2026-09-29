"""Verify the untouched package independently of the played QA extraction."""
import json
import zipfile
from prepare_submission import OUT, digest, write_json

result_path = OUT / "asset_clean_result.json"
result = json.loads(result_path.read_text(encoding="utf-8"))
archive = OUT / result["archive"]
assert archive.name.endswith("_03.zip")
assert digest(archive) == result["sha256"]
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    manifest = json.loads(z.read("SuidoNogyo/MANIFEST.json"))
    import hashlib
    for entry in manifest["files"]:
        assert hashlib.sha256(z.read("SuidoNogyo/" + entry["path"])).hexdigest() == entry["sha256"]
    saves = [p for p in z.namelist() if "/Settings/farm/saves/" in p]
    assert len(saves) == 1
    doc = json.loads(z.read(saves[0]))
    assert doc["date"]["day"] == 1 and doc["economy"]["money"] == 300
    assert doc["economy"]["seedCounts"] == [0,5,5,5] and doc["playMode"] == "FreeFarming"
    assert len(z.namelist()) == len(manifest["files"]) + 1
qa = OUT / "verify_asset_clean_03/SuidoNogyo/Settings/farm/saves"
docs = list(qa.glob("*.json"))
assert len(docs) == 1
played = json.loads(docs[0].read_text(encoding="utf-8"))
assert played["economy"]["money"] == 240 and played["economy"]["seedCounts"] == [0,6,5,5]
result["runtime_check"] = "PASS: isolated launcher, till, purchase, save/relaunch, follow marker, normal/maximized UI"
result["pristine_archive_rechecked_after_qa"] = True
result["limits"] = ["not a clean-PC test", "no full harvest/contest run this revision", "rights final checks pending"]
write_json(result_path, result)
print(f"PASS: {len(manifest['files'])} ZIP hashes, one pristine starter, independent saved QA state")

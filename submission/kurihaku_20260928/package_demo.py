"""Package exactly one approved starter save; never copy development Settings."""
from pathlib import Path
import json
import re
import zipfile
from prepare_submission import ROOT, OUT, SOURCE, copy_file, digest, write_json


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def original_saves() -> dict[str, str]:
    root = ROOT / "project/Settings"
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob("*") if p.is_file()}


def main() -> None:
    target = OUT / "company_demo/FarmGame_Runtime"
    archive = OUT / "FarmGame_Demo_20260928_candidate.zip"
    extracted = OUT / "verify_demo/企業向け 試遊確認"
    require(not target.exists() and not archive.exists() and not extracted.exists(), "Do not overwrite a previous demo")
    before = original_saves()
    preset = OUT / "demo_preset/farm"
    catalog = json.loads((preset / "farm_documents.json").read_text(encoding="utf-8"))
    save_id = catalog["activeDocumentId"]
    require(re.fullmatch(r"farm_[0-9]+_[0-9]+", save_id) is not None, "Unsafe generated save id")
    save_path = preset / "saves" / (save_id + ".json")
    saves = list((preset / "saves").glob("*.json"))
    require(saves == [save_path], "Preset must contain exactly one save")
    document = json.loads(save_path.read_text(encoding="utf-8"))
    require(document["schemaVersion"] == 16 and document["playMode"] == "ContestSeason", "Unexpected save format/mode")
    require(document["economy"]["seedCounts"] == [0, 5, 5, 5], "Wrong seed counts")
    require(document["economy"]["money"] == 300 and document["date"]["day"] == 1, "Wrong starter conditions")
    require(document["document"]["id"] == save_id and document["document"]["displayName"] == "企業向け体験用 種各5個", "Wrong starter metadata")
    source_manifest = json.loads((OUT / "source_manifest.json").read_text(encoding="utf-8"))
    for entry in source_manifest["files"]:
        if entry["path"].startswith(("project/application/", "project/engine/")):
            require(digest(ROOT / entry["path"]) == entry["sha256"], "Production source changed since runtime build")
    old_runtime = OUT / "runtime/FarmGame_Runtime"
    runtime_manifest = json.loads((old_runtime / "MANIFEST.json").read_text(encoding="utf-8"))
    for entry in runtime_manifest["files"]:
        rel = Path(entry["path"])
        require(not rel.is_absolute() and ".." not in rel.parts, "Unsafe runtime manifest path")
        require(rel.parts[0] != "Settings", "Do not import previous settings")
        require(digest(old_runtime / rel) == entry["sha256"], "Previous runtime differs from verified manifest")
        copy_file(old_runtime / rel, target / rel)
    copy_file(Path(__file__).parent / "DEMO_README.txt", target / "README.txt")
    copy_file(preset / "farm_documents.json", target / "Settings/farm/farm_documents.json")
    copy_file(save_path, target / "Settings/farm/saves" / save_path.name)
    allowed_settings = {"Settings/farm/farm_documents.json", "Settings/farm/saves/" + save_path.name}
    actual_settings = {p.relative_to(target).as_posix() for p in (target / "Settings").rglob("*") if p.is_file()}
    require(actual_settings == allowed_settings, "Unexpected company-visible save data")
    files = [{"path": p.relative_to(target).as_posix(), "sha256": digest(p), "bytes": p.stat().st_size}
             for p in sorted(target.rglob("*")) if p.is_file()]
    write_json(target / "MANIFEST.json", {"head": runtime_manifest["head"],
        "status": "LOCAL_CANDIDATE_LICENSE_REVIEW_PENDING", "preset": save_id,
        "money_clear": False, "contest_days": [10, 20, 30], "files": files})
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for p in sorted(target.rglob("*")):
            if p.is_file():
                z.write(p, "FarmGame_Runtime/" + p.relative_to(target).as_posix())
    with zipfile.ZipFile(archive) as z:
        require(z.testzip() is None, "ZIP CRC error")
        archived_saves = [name for name in z.namelist() if "/Settings/" in name]
        require(set(archived_saves) == {"FarmGame_Runtime/" + p for p in allowed_settings}, "ZIP contains unrelated Settings")
        z.extractall(extracted)
    for entry in files:
        require(digest(extracted / "FarmGame_Runtime" / entry["path"]) == entry["sha256"], "Extraction hash mismatch")
    require(before == original_saves(), "Original Settings changed")
    write_json(OUT / "demo_package_result.json", {"zip": archive.name, "bytes": archive.stat().st_size,
        "sha256": digest(archive), "saved_documents": 1, "verified_files": len(files),
        "original_settings_unchanged": True, "original_settings_files": len(before),
        "runtime_check": "pending", "rights_review": "pending"})
    print(f"PASS: one demo save only; seeds 5/5/5; CRC/hash match; original Settings unchanged. {archive}")


if __name__ == "__main__":
    main()

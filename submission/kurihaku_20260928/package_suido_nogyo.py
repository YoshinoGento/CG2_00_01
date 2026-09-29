"""Package the titled Release build with verified assets and one starter save."""
from pathlib import Path, PurePosixPath
import json
import re
import zipfile

from package_demo import original_saves, require
from prepare_submission import OUT, ROOT, copy_file, digest, write_json


def safe_path(name: str) -> Path:
    p = PurePosixPath(name)
    require(name.isascii() and "\\" not in name and ":" not in name,
            f"Non-portable name: {name}")
    require(not p.is_absolute() and all(part not in (".", "..") for part in p.parts),
            f"Unsafe manifest name: {name}")
    return Path(*p.parts)


def main() -> None:
    source = OUT / "company_demo/FarmGame_Runtime"
    target = OUT / "free_farming/SuidoNogyo"
    archive = OUT / "SuidoNogyo_FreeFarming_20260928.zip"
    extracted = OUT / "verify_free_farming"
    require(not target.exists() and not archive.exists() and not extracted.exists(),
            "Do not overwrite an existing package or play data")
    before = original_saves()
    previous = json.loads((source / "MANIFEST.json").read_text(encoding="utf-8"))
    manifest_paths = {entry["path"] for entry in previous["files"]}
    actual_paths = {p.relative_to(source).as_posix() for p in source.rglob("*") if p.is_file()}
    require(actual_paths == manifest_paths | {"MANIFEST.json"}, "Unexpected files in source runtime")
    build_root = OUT / "free_build"
    code = json.loads((build_root / "source_manifest.json").read_text(encoding="utf-8"))
    build = json.loads((build_root / "build_result.json").read_text(encoding="utf-8"))
    require(build["exit_code"] == 0 and build["head"] == code["head"], "Matching successful build required")
    for entry in code["files"]:
        require(digest(build_root / "work/source" / entry["path"]) == entry["sha256"], "Build snapshot changed")
        if entry["path"].startswith(("project/application/", "project/engine/")):
            require(digest(ROOT / entry["path"]) == entry["sha256"], "Source changed since verified build")
    for entry in previous["files"]:
        relative = safe_path(entry["path"])
        require(digest(source / relative) == entry["sha256"], f"Source hash changed: {relative}")
        if entry["path"] in ("StartGame.cmd", "README.txt", "CG2_00_01.exe", "dxcompiler.dll", "dxil.dll"):
            continue
        if entry["path"].startswith("Settings/"):
            continue
        if entry["path"].startswith("Resources/"):
            copy_file(build_root / "work/source/project" / relative, target / relative)
        else:
            copy_file(source / relative, target / relative)
    preset = OUT / "free_preset/farm"
    catalog = json.loads((preset / "farm_documents.json").read_text(encoding="utf-8"))
    save_id = catalog["activeDocumentId"]
    require(re.fullmatch(r"farm_[0-9]+_[0-9]+", save_id), "Invalid preset id")
    copy_file(preset / "farm_documents.json", target / "Settings/farm/farm_documents.json")
    copy_file(preset / "saves" / (save_id + ".json"), target / "Settings/farm/saves" / (save_id + ".json"))
    binaries = build_root / "work/source/generated/outputs/Release"
    copy_file(binaries / "CG2_00_01.exe", target / "SuidoNogyo.exe")
    for name in ("dxcompiler.dll", "dxil.dll"):
        copy_file(binaries / name, target / name)
    copy_file(Path(__file__).parent / "StartSuidoNogyo.cmd", target / "StartGame.cmd")
    readme = (Path(__file__).parent / "FREE_README.txt").read_text(encoding="utf-8")
    (target / "README.txt").write_text(readme, encoding="utf-8-sig")
    settings = {p.relative_to(target).as_posix() for p in (target / "Settings").rglob("*") if p.is_file()}
    save_files = list((target / "Settings/farm/saves").glob("*.json"))
    require(len(save_files) == 1, "Exactly one starter save required")
    require(re.fullmatch(r"farm_[0-9]+_[0-9]+\.json", save_files[0].name), "Unexpected save filename")
    require(settings == {"Settings/farm/farm_documents.json", "Settings/farm/saves/" + save_files[0].name},
            "Unexpected settings included")
    doc = json.loads(save_files[0].read_text(encoding="utf-8"))
    require(doc["economy"]["seedCounts"] == [0, 5, 5, 5] and doc["economy"]["money"] == 300
            and doc["date"]["day"] == 1 and doc["playMode"] == "FreeFarming"
            and doc["schemaVersion"] == 17, "Wrong starter save")
    files = []
    for p in sorted(target.rglob("*")):
        if p.is_file():
            name = p.relative_to(target).as_posix()
            safe_path(name)
            require(p.suffix.lower() not in {".pdb", ".ilk", ".lib", ".cpp", ".blend", ".log"},
                    f"Development artifact: {name}")
            files.append({"path": name, "sha256": digest(p), "bytes": p.stat().st_size})
    require(digest(target / "SuidoNogyo.exe") == digest(binaries / "CG2_00_01.exe"), "Binary changed")
    write_json(target / "MANIFEST.json", {"title": "水道農業", "runtime_name": "SuidoNogyo",
               "head": code["head"], "status": "LOCAL_CANDIDATE_LICENSE_REVIEW_PENDING",
               "files": files, "play_mode": "FreeFarming", "money_clear": False, "day_clear": False})
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for p in sorted(target.rglob("*")):
            if p.is_file():
                z.write(p, "SuidoNogyo/" + p.relative_to(target).as_posix())
    with zipfile.ZipFile(archive) as z:
        require(z.testzip() is None, "ZIP CRC failed")
        for name in z.namelist():
            safe_path(name)
        z.extractall(extracted)
    for entry in files:
        require(digest(extracted / "SuidoNogyo" / entry["path"]) == entry["sha256"], "Extracted hash mismatch")
    require(original_saves() == before, "Development saves changed")
    write_json(OUT / "free_farming_result.json", {"archive": archive.name, "bytes": archive.stat().st_size,
               "sha256": digest(archive), "verified_files": len(files), "all_paths_ascii": True,
               "starter_saves": 1, "original_settings_unchanged": True,
               "renamed_runtime_check": "pending", "rights_review": "pending"})
    print(f"PASS: {archive}; {len(files)} file hashes; ASCII paths; one starter save; original Settings unchanged")


if __name__ == "__main__":
    main()

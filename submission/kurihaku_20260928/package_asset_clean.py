"""Allowlist a new runtime bundle, keeping development and prior archives intact."""
from pathlib import Path
import json
import re
import zipfile
from package_demo import original_saves, require
from package_suido_nogyo import safe_path
from prepare_submission import OUT, ROOT, copy_file, digest, write_json


def main():
    build = OUT / "asset_clean_build"
    source = build / "work/source"
    target = OUT / "asset_clean_03/SuidoNogyo"
    archive = OUT / "SuidoNogyo_FreeFarming_20260928_AssetClean_03.zip"
    extracted = OUT / "verify_asset_clean_03"
    require(not target.exists() and not archive.exists() and not extracted.exists(), "Refusing overwrite")
    before = original_saves()
    code = json.loads((build / "source_manifest.json").read_text(encoding="utf-8"))
    for name in ("build_result.json", "debug_build_result.json"):
        result = json.loads((build / name).read_text(encoding="utf-8"))
        require(result["exit_code"] == 0 and result["head"] == code["head"], "Successful matching builds required")
    allowed_dirs = {"farm", "ui", "fonts", "shader", "generated"}
    omitted = []
    for entry in code["files"]:
        rel = Path(entry["path"])
        require(digest(source / rel) == entry["sha256"], "Build snapshot changed")
        if entry["path"].startswith(("project/application/", "project/engine/", "project/Resources/")):
            require(digest(ROOT / rel) == entry["sha256"], "Workspace changed since build")
        if rel.parts[:2] != ("project", "Resources"):
            continue
        tail = Path(*rel.parts[2:])
        allowed = tail.as_posix() in {"text_textures.json", "levels/farm_scene.json"}
        allowed |= tail.parts[0] in allowed_dirs and tail.suffix.lower() in {".png", ".obj", ".json", ".ttf", ".txt", ".hlsl", ".hlsli"}
        if allowed:
            safe_path(tail.as_posix())
            copy_file(source / rel, target / "Resources" / tail)
        else:
            omitted.append(tail.as_posix())
    # Only the approved unplayed preset; never enumerate development Settings.
    preset = OUT / "free_preset/farm"
    catalog = json.loads((preset / "farm_documents.json").read_text(encoding="utf-8"))
    save_id = catalog["activeDocumentId"]
    require(re.fullmatch(r"farm_[0-9]+_[0-9]+", save_id), "Invalid starter id")
    for tail in ("farm_documents.json", "saves/" + save_id + ".json"):
        copy_file(preset / tail, target / "Settings/farm" / tail)
    save = json.loads((target / "Settings/farm/saves" / (save_id + ".json")).read_text(encoding="utf-8"))
    require(save["schemaVersion"] == 17 and save["playMode"] == "FreeFarming"
            and save["economy"]["seedCounts"] == [0,5,5,5] and save["economy"]["money"] == 300
            and save["date"]["day"] == 1, "Not the approved fresh preset")
    binaries = source / "generated/outputs/Release"
    copy_file(binaries / "CG2_00_01.exe", target / "SuidoNogyo.exe")
    for name in ("dxcompiler.dll", "dxil.dll"):
        copy_file(binaries / name, target / name)
    local = Path(__file__).parent
    copy_file(local / "StartSuidoNogyo.cmd", target / "StartGame.cmd")
    readme = (local / "FREE_README.txt").read_text(encoding="utf-8").replace("NOTICES_PENDING.txt", "CREDITS.txt")
    (target / "README.txt").write_text(readme, encoding="utf-8-sig")
    (target / "CREDITS.txt").write_text((local / "ASSET_CREDITS.txt").read_text(encoding="utf-8"), encoding="utf-8-sig")
    for p in (OUT / "asset_notices").rglob("*"):
        if p.is_file():
            copy_file(p, target / "Licenses" / p.relative_to(OUT / "asset_notices"))
    copy_file(OUT / "asset_notices/NotoSansJP_OFL.txt", target / "Resources/fonts/OFL.txt")
    # Preserve the local Assimp header copyright in addition to upstream's older root notice.
    header = (source / "project/externals/assimp/include/assimp/version.h").read_text(encoding="utf-8-sig")
    copyright_block = header[header.index("/*")+2:header.index("*/")]
    (target / "Licenses/Assimp_local_header.txt").write_text(copyright_block.strip() + "\n", encoding="utf-8")
    files = []
    forbidden = {"human", "AnimatedCube", "simpleSkin", "tl1", "terrain"}
    for p in sorted(target.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(target)
        safe_path(rel.as_posix())
        require(not any(part in forbidden for part in rel.parts), "School asset included")
        require(p.suffix.lower() not in {".pdb", ".lib", ".cpp", ".blend", ".gltf", ".fbx", ".dds"}, "Unapproved runtime file")
        files.append({"path": rel.as_posix(), "bytes": p.stat().st_size, "sha256": digest(p)})
    write_json(target / "MANIFEST.json", {"title": "水道農業", "head": code["head"],
        "status": "ASSETS_CLEANED_RIGHTS_FINAL_CHECK_PENDING", "files": files, "play_mode": "FreeFarming"})
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for p in sorted(target.rglob("*")):
            if p.is_file():
                z.write(p, "SuidoNogyo/" + p.relative_to(target).as_posix())
    with zipfile.ZipFile(archive) as z:
        require(z.testzip() is None, "Bad ZIP CRC")
        for name in z.namelist():
            safe_path(name)
        z.extractall(extracted)
    for entry in files:
        require(digest(extracted / "SuidoNogyo" / entry["path"]) == entry["sha256"], "Extraction mismatch")
    require(original_saves() == before, "Original saves changed")
    write_json(OUT / "asset_clean_result.json", {"archive": archive.name, "sha256": digest(archive),
        "bytes": archive.stat().st_size, "verified_files": len(files), "omitted_resources": omitted,
        "original_saves_unchanged": True, "runtime_check": "pending", "external_submission": "not yet cleared"})
    print(f"PASS {archive}: {len(files)} hashes verified, {len(omitted)} resources excluded")


if __name__ == "__main__":
    main()

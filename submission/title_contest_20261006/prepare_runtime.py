"""Build and verify the school runtime without touching older submissions."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "submission/kurihaku_20260928"))
import build_provenance_runtime as pinned
import prepare_submission as prep
from package_demo import original_saves, require
from package_suido_nogyo import safe_path

OUT = ROOT / "generated/title_contest_20261006"
BUILD = OUT / "build"
NAME = "LE3C_26_ヨシノ_ゲント_水路農業"
TARGET = OUT / "ready" / NAME
ARCHIVE = OUT / "ready" / (NAME + ".zip")
QA = OUT / "verify 日本語 path"
OLD = ROOT / "generated/kurihaku_20260928"


def build():
    require(not BUILD.exists(), "Refusing to overwrite an existing build")
    dependencies = json.loads((pinned.DEP / "assimp_build.json").read_text(encoding="utf-8"))
    for entry in dependencies["libraries"]:
        require(prep.digest(pinned.DEP / entry["path"]) == entry["sha256"], "Pinned library changed")
    prep.OUT = BUILD
    prep.SOURCE = BUILD / "work/source"
    pinned.main()


def package():
    require(not TARGET.exists() and not ARCHIVE.exists() and not QA.exists(), "Refusing overwrite")
    before = original_saves()
    prep.write_json(OUT / "original_saves.json", before)
    manifest = json.loads((BUILD / "source_manifest.json").read_text(encoding="utf-8"))
    result = json.loads((BUILD / "build_result.json").read_text(encoding="utf-8"))
    require(result["exit_code"] == 0 and result["head"] == manifest["head"], "Matching build required")
    source = BUILD / "work/source"
    omitted = []
    allowed_dirs = {"farm", "ui", "shader", "generated", "title"}
    extensions = {".png", ".obj", ".json", ".txt", ".hlsl", ".hlsli", ".wav"}
    for entry in manifest["files"]:
        rel = Path(entry["path"])
        require(prep.digest(source / rel) == entry["sha256"], "Build snapshot changed")
        if entry["path"].startswith(("project/application/", "project/engine/", "project/Resources/")):
            require(prep.digest(ROOT / rel) == entry["sha256"], "Workspace changed after snapshot")
        if rel.parts[:2] != ("project", "Resources"):
            continue
        tail = Path(*rel.parts[2:])
        allowed = tail.as_posix() in {"text_textures.json", "levels/farm_scene.json", "fonts/NotoSansJP-VF.ttf", "fonts/OFL.txt", "fonts/NotoSerifJP-OFL.txt"}
        allowed |= tail.parts[0] in allowed_dirs and tail.suffix.lower() in extensions
        if allowed:
            safe_path(tail.as_posix())
            prep.copy_file(source / rel, TARGET / "Resources" / tail)
        else:
            omitted.append(tail.as_posix())

    baseline = OLD / "provenance_runtime/SuidoNogyo"
    old_manifest = json.loads((baseline / "MANIFEST.json").read_text(encoding="utf-8"))
    for entry in old_manifest["files"]:
        if entry["path"].startswith(("Licenses/", "Settings/farm/")):
            require(prep.digest(baseline / entry["path"]) == entry["sha256"], "Reviewed baseline changed")
            if entry["path"] != "Licenses/SOURCES.json":
                prep.copy_file(baseline / entry["path"], TARGET / entry["path"])
    saves = list((TARGET / "Settings/farm/saves").glob("*.json"))
    require(len(saves) == 1, "Only one fresh starter save permitted")
    starter = json.loads(saves[0].read_text(encoding="utf-8"))
    require(starter["schemaVersion"] == 17 and starter["playMode"] == "FreeFarming"
            and starter["economy"]["seedCounts"] == [0, 5, 5, 5]
            and starter["economy"]["money"] == 300 and starter["date"]["day"] == 1, "Invalid starter")
    require(all(tile["state"] == "Empty" for tile in starter["tiles"]), "Starter has been played")
    binaries = source / "generated/outputs/Release"
    prep.copy_file(binaries / "CG2_00_01.exe", TARGET / "SuiroNogyo.exe")
    for name in ("dxcompiler.dll", "dxil.dll"):
        official = pinned.DEP / "dxc-release/bin/x64" / name
        require(prep.digest(binaries / name) == prep.digest(official), "Pinned DXC mismatch")
        prep.copy_file(official, TARGET / name)
    notices = json.loads((baseline / "Licenses/SOURCES.json").read_text(encoding="utf-8"))
    notices["binaries"] = {p.name: prep.digest(p) for p in TARGET.iterdir() if p.suffix in {".exe", ".dll"}}
    prep.write_json(TARGET / "Licenses/SOURCES.json", notices)
    for name in ("README.txt", "CREDITS.txt", "StartGame.cmd"):
        prep.copy_file(Path(__file__).parent / name, TARGET / name)
    forbidden = {".pdb", ".lib", ".cpp", ".h", ".py", ".blend", ".gltf", ".fbx", ".mp4", ".mp3", ".zip"}
    files = []
    for p in sorted(TARGET.rglob("*")):
        if p.is_file():
            rel = p.relative_to(TARGET).as_posix()
            safe_path(rel)
            require(p.suffix.lower() not in forbidden, f"Non-runtime file: {rel}")
            files.append({"path": rel, "bytes": p.stat().st_size, "sha256": prep.digest(p)})
    prep.write_json(TARGET / "MANIFEST.json", {"title": "水路農業", "head": manifest["head"], "files": files,
        "configuration": "Release x64", "second_pc": "untested", "snapshot": "Includes current uncommitted work"})
    with zipfile.ZipFile(ARCHIVE, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(TARGET.rglob("*")):
            if p.is_file():
                z.write(p, NAME + "/" + p.relative_to(TARGET).as_posix())
    with zipfile.ZipFile(ARCHIVE) as z:
        require(z.testzip() is None, "ZIP CRC error")
        for name in z.namelist():
            require(name.startswith(NAME + "/"), "Unexpected ZIP root")
            safe_path(name.removeprefix(NAME + "/"))
        z.extractall(QA)
    for item in files:
        require(prep.digest(QA / NAME / item["path"]) == item["sha256"], "Extraction hash mismatch")
    require(original_saves() == before, "Original saves changed")
    prep.write_json(OUT / "package_result.json", {"archive": str(ARCHIVE), "sha256": prep.digest(ARCHIVE),
        "bytes": ARCHIVE.stat().st_size, "uncompressed_bytes": sum(f["bytes"] for f in files),
        "verified_files": len(files), "omitted_resources": omitted, "original_saves_unchanged": True,
        "runtime_check": "pending", "external_upload": "not performed"})
    print(f"PASS archive={ARCHIVE} bytes={ARCHIVE.stat().st_size} verified_files={len(files)}")


def verify():
    manifest = json.loads((TARGET / "MANIFEST.json").read_text(encoding="utf-8"))
    result = json.loads((OUT / "package_result.json").read_text(encoding="utf-8"))
    require(prep.digest(ARCHIVE) == result["sha256"], "Archive changed")
    with zipfile.ZipFile(ARCHIVE) as z:
        require(z.testzip() is None, "ZIP CRC error")
        expected = {NAME + "/" + f["path"] for f in manifest["files"]} | {NAME + "/MANIFEST.json"}
        require(set(z.namelist()) == expected and len(z.namelist()) == len(expected), "Unexpected ZIP contents")
        for f in manifest["files"]:
            require(prep.digest(TARGET / f["path"]) == f["sha256"], "Pristine delivery changed")
            require(hashlib.sha256(z.read(NAME + "/" + f["path"])).hexdigest() == f["sha256"], "ZIP member changed")
        require(z.read(NAME + "/MANIFEST.json") == (TARGET / "MANIFEST.json").read_bytes(), "Manifest changed")
    before = json.loads((OUT / "original_saves.json").read_text(encoding="utf-8"))
    require(original_saves() == before, "Original saves changed")
    require(not (TARGET / "Settings/runtime").exists(), "QA settings leaked into delivery")
    require(prep.git("rev-parse", "HEAD") == manifest["head"], "HEAD changed")
    require(not prep.git("diff", "--cached", "--name-only"), "Index changed")
    print(f"PASS immutable ZIP/pristine delivery/original saves: {len(manifest['files'])} files")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("step", choices=["build", "package", "verify"])
    globals()[parser.parse_args().step]()

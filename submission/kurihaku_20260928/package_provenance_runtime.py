"""Keep the reviewed asset allowlist; replace only pinned dependencies and notices."""
import json
from pathlib import Path
import re
import zipfile
from prepare_submission import OUT, ROOT, copy_file, digest, write_json
from package_demo import original_saves, require
from package_suido_nogyo import safe_path

DEP = OUT / "known_dependencies"
BUILD = OUT / "provenance_build"
TARGET = OUT / "provenance_runtime/SuidoNogyo"
ARCHIVE = OUT / "SuidoNogyo_FreeFarming_20260928_04.zip"
QA = OUT / "verify_provenance_04"


def main():
    require(not TARGET.exists() and not ARCHIVE.exists() and not QA.exists(), "Refusing overwrite")
    saves = original_saves()
    write_json(OUT / "provenance_original_saves.json", saves)
    code = json.loads((BUILD / "source_manifest.json").read_text(encoding="utf-8"))
    result = json.loads((BUILD / "build_result.json").read_text(encoding="utf-8"))
    require(result["exit_code"] == 0 and result["head"] == code["head"], "Matching build required")
    source = BUILD / "work/source"
    for entry in code["files"]:
        require(digest(source / entry["path"]) == entry["sha256"], "Build snapshot changed")
        if entry["path"].startswith(("project/application/", "project/engine/", "project/Resources/")):
            require(digest(ROOT / entry["path"]) == entry["sha256"], "Workspace changed")
    baseline = OUT / "asset_clean_03/SuidoNogyo"
    previous = json.loads((baseline / "MANIFEST.json").read_text(encoding="utf-8"))
    for entry in previous["files"]:
        rel = entry["path"]
        require(digest(baseline / rel) == entry["sha256"], "Baseline changed")
        if rel.startswith(("Resources/", "Settings/")) or rel == "StartGame.cmd":
            copy_file(baseline / rel, TARGET / rel)
    binaries = source / "generated/outputs/Release"
    copy_file(binaries / "CG2_00_01.exe", TARGET / "SuidoNogyo.exe")
    for name in ("dxcompiler.dll", "dxil.dll"):
        official = DEP / "dxc-release/bin/x64" / name
        require(digest(binaries / name) == digest(official), "DLL mismatch")
        copy_file(official, TARGET / name)
    local = Path(__file__).parent
    readme = (local / "FREE_README.txt").read_text(encoding="utf-8")
    readme = readme.replace("素材利用条件・出典の確認はNOTICES_PENDING.txtに記載。権利確認が残るローカル候補です。",
                            "素材・ライブラリの出典と確認範囲はCREDITS.txtとLicensesを参照してください。別PC動作は未確認です。")
    (TARGET / "README.txt").write_text(readme, encoding="utf-8-sig")
    (TARGET / "CREDITS.txt").write_text((local / "ASSET_CREDITS.txt").read_text(encoding="utf-8"), encoding="utf-8-sig")
    notices = []
    def notice(p, rel, origin):
        copy_file(p, TARGET / "Licenses" / rel)
        notices.append({"path": rel, "source": origin, "sha256": digest(p)})
    old_sources = json.loads((OUT / "asset_notices/SOURCES.json").read_text(encoding="utf-8"))
    for name in ("DirectXTex.txt", "ImGui.txt", "nlohmann_json_3.11.3.txt", "NotoSansJP_OFL.txt"):
        origin = next((item.get("url", "local project license") for item in old_sources["files"] if item["path"] == name), "local project license")
        notice(OUT / "asset_notices" / name, name, origin)
    assimp = DEP / "assimp-source"
    notice(assimp / "LICENSE", "Assimp/LICENSE.txt", "assimp-5.3.0.zip/LICENSE")
    for p in (assimp / "contrib").rglob("*"):
        if p.is_file() and p.name.lower().startswith(("license", "unlicense", "copying")):
            rel = p.relative_to(assimp).as_posix()
            notice(p, "Assimp/" + rel, "assimp-5.3.0.zip/" + rel)
    for rel in ("include/assimp/version.h", "contrib/Open3DGC/o3dgcCommon.h",
                "contrib/pugixml/src/pugixml.hpp", "contrib/stb/stb_image.h",
                "contrib/unzip/unzip.h", "contrib/unzip/ioapi.h", "contrib/zip/src/miniz.h"):
        text = (assimp / rel).read_text(encoding="utf-8-sig")
        blocks = [b for b in re.findall(r"/\*.*?\*/", text, re.S)
                  if "copyright" in b.lower()]
        require(bool(blocks), f"No embedded license: {rel}")
        tail = "Assimp/embedded/" + rel.replace("/", "_") + ".txt"
        p = TARGET / "Licenses" / tail
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("\n\n".join(blocks) + "\n", encoding="utf-8")
        notices.append({"path": tail, "source": "assimp-5.3.0.zip/" + rel, "sha256": digest(p)})
    for name in ("LICENSE-LLVM.txt", "LICENSE-MIT.txt", "LICENSE-MS.txt", "ReleaseNotes.md"):
        notice(DEP / "dxc-release" / name, "DXC/" + name, "dxc_2025_02_20.zip/" + name)
    write_json(TARGET / "Licenses/SOURCES.json", {
        "downloads": json.loads((DEP / "downloads.json").read_text(encoding="utf-8")),
        "assimp_build": json.loads((DEP / "assimp_build.json").read_text(encoding="utf-8")),
        "dxc_license_mapping": "ReleaseNotes.md: LICENSE-LLVM.txt applies to DLLs; all original license files retained.",
        "files": notices,
        "binaries": {name: digest(TARGET / name) for name in ("SuidoNogyo.exe", "dxcompiler.dll", "dxil.dll")},
    })
    files = []
    for p in sorted(TARGET.rglob("*")):
        if p.is_file():
            rel = p.relative_to(TARGET).as_posix()
            safe_path(rel)
            files.append({"path": rel, "bytes": p.stat().st_size, "sha256": digest(p)})
    write_json(TARGET / "MANIFEST.json", {"title": "水道農業", "head": code["head"],
        "status": "PROVENANCE_MATCHED_SECOND_PC_UNTESTED", "play_mode": "FreeFarming", "files": files})
    with zipfile.ZipFile(ARCHIVE, "x", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for p in sorted(TARGET.rglob("*")):
            if p.is_file(): z.write(p, "SuidoNogyo/" + p.relative_to(TARGET).as_posix())
    with zipfile.ZipFile(ARCHIVE) as z:
        require(z.testzip() is None, "CRC error")
        for name in z.namelist(): safe_path(name)
        z.extractall(QA)
    for item in files:
        require(digest(QA / "SuidoNogyo" / item["path"]) == item["sha256"], "Extraction mismatch")
    require(original_saves() == saves, "Original saves changed")
    write_json(OUT / "provenance_package_result.json", {
        "archive": ARCHIVE.name, "sha256": digest(ARCHIVE), "bytes": ARCHIVE.stat().st_size,
        "verified_files": len(files), "original_saves_unchanged": True,
        "runtime_check": "pending", "second_pc": "not available; untested",
    })
    print(f"PASS {ARCHIVE}: {len(files)} hashes / CRC / original saves")


if __name__ == "__main__":
    main()

"""Prepare a local, review-only submission without touching source or Git state."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "generated/kurihaku_20260928"
SOURCE = OUT / "work/source"
BUNDLE = OUT / "bundle"
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
EXTS = {".cpp", ".h", ".hpp", ".inl", ".c", ".hlsl", ".hlsli"}


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT).decode("utf-8").strip()


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def copy_file(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def snapshot() -> None:
    if (OUT / "source_manifest.json").exists():
        raise RuntimeError("Snapshot already exists; use its manifest, do not silently replace it.")
    tracked = set(subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode("utf-8").split("\0"))
    paths: set[Path] = set()
    roots = ["project/application", "project/engine", "project/Resources",
             "project/externals/assimp", "project/externals/DirectXTex",
             "project/externals/imgui", "project/externals/nlohmann"]
    for folder in roots:
        for p in (ROOT / folder).rglob("*"):
            if not p.is_file():
                continue
            rel = p.relative_to(ROOT)
            tail = p.relative_to(ROOT / folder)
            if p.is_symlink() or getattr(p, "is_junction", lambda: False)():
                raise RuntimeError(f"Do not package links: {rel}")
            if any(part.lower() in {".git", ".vs", "__pycache__", "x64", "generated"}
                   for part in tail.parts[:-1]) and not str(rel).replace("\\", "/").startswith("project/Resources/generated/"):
                continue
            if p.suffix.lower() in {".obj", ".mtl"} and "Resources" in rel.parts:
                paths.add(rel)
                continue
            if p.suffix.lower() in {".obj", ".pdb", ".ilk", ".log", ".tlog", ".user", ".pyc", ".idb", ".pch", ".lastbuildstate"}:
                continue
            if p.suffix.lower() in {".exe", ".zip"}:
                continue
            paths.add(rel)
    for name in ["CG2_00_01.sln", "CG2_00_01.vcxproj", "CG2_00_01.vcxproj.filters", ".editorconfig"]:
        paths.add(Path("project") / name)
    for folder in ["tools", "docs"]:
        for p in (ROOT / folder).rglob("*"):
            if p.is_file() and p.suffix in {".py", ".cpp", ".h", ".ps1", ".cmd", ".md"}:
                paths.add(p.relative_to(ROOT))
    missing = []
    project = ET.parse(ROOT / "project/CG2_00_01.vcxproj")
    for kind in ["ClCompile", "ClInclude", "FxCompile"]:
        for item in project.findall(f".//m:{kind}[@Include]", NS):
            name = item.attrib["Include"]
            if "$" in name:
                raise RuntimeError(f"Unresolved project reference: {name}")
            p = (ROOT / "project" / name.replace("\\", "/")).resolve()
            rel = p.relative_to(ROOT)
            if not p.is_file():
                missing.append(str(rel))
            paths.add(rel)
    if missing:
        raise RuntimeError(f"Missing source files: {missing}")
    files = []
    for rel in sorted(paths):
        copy_file(ROOT / rel, SOURCE / rel)
        files.append({"path": rel.as_posix(), "sha256": digest(SOURCE / rel),
                      "bytes": (SOURCE / rel).stat().st_size,
                      "git_tracked": rel.as_posix() in tracked})
    write_json(OUT / "source_manifest.json", {
        "head": git("rev-parse", "HEAD"), "branch": git("branch", "--show-current"),
        "files": files, "purpose": "Local submission candidate; not uploaded"})
    ignored_code = [f["path"] for f in files if not f["git_tracked"] and Path(f["path"]).suffix in EXTS
                    and not f["path"].startswith("project/externals/")]
    print(json.dumps({"files": len(files), "untracked_code_included": ignored_code}, ensure_ascii=False))


def build(configuration: str = "Release") -> None:
    if configuration not in {"Release", "Debug"}:
        raise ValueError("Unsupported build configuration")
    manifest = json.loads((OUT / "source_manifest.json").read_text(encoding="utf-8"))
    for f in manifest["files"]:
        if digest(SOURCE / f["path"]) != f["sha256"]:
            raise RuntimeError(f"Snapshot modified: {f['path']}")
    if configuration == "Debug":
        # Assimp's static Debug library requires its matching PDB under /WX.
        library = Path("project/externals/assimp/lib/Debug/assimp-vc143-mtd.lib")
        symbols = library.with_suffix(".pdb")
        if digest(ROOT / library) != digest(SOURCE / library):
            raise RuntimeError("Assimp library changed since snapshot")
        copy_file(ROOT / symbols, SOURCE / "generated/outputs/Debug" / symbols.name)
        write_json(OUT / "debug_symbols.json", {"source": symbols.as_posix(), "sha256": digest(ROOT / symbols)})
    msbuild = Path("C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe")
    if not msbuild.is_file():
        raise RuntimeError("Visual Studio 18/v145 MSBuild not found")
    # The host can expose duplicate Path/PATH keys; normalize inside child cmd.
    runner = OUT / f"work/build_{configuration.lower()}.cmd"
    runner.write_text('@echo off\nset "SUBMISSION_PATH=%PATH%"\nset Path=\nset "PATH=%SUBMISSION_PATH%"\n'
                      f'"{msbuild}" CG2_00_01.sln /t:Build /p:Configuration={configuration} /p:Platform=x64 /p:CL_MPCount=2 /m:2 /nr:false /v:normal /nologo\n'
                      'exit /b %errorlevel%\n', encoding="ascii")
    log = OUT / f"work/{configuration.lower()}_build.log"
    with log.open("wb") as stream:
        result = subprocess.run(["cmd.exe", "/d", "/c", str(runner)], cwd=SOURCE / "project", stdout=stream, stderr=subprocess.STDOUT)
    result_name = "build_result.json" if configuration == "Release" else "debug_build_result.json"
    write_json(OUT / result_name, {"exit_code": result.returncode, "head": manifest["head"], "log": str(log)})
    print(f"{configuration} build exit={result.returncode}; log={log}")
    if result.returncode:
        raise SystemExit(result.returncode)


def assemble() -> None:
    manifest = json.loads((OUT / "source_manifest.json").read_text(encoding="utf-8"))
    result = json.loads((OUT / "build_result.json").read_text(encoding="utf-8"))
    if result["exit_code"] != 0 or result["head"] != manifest["head"]:
        raise RuntimeError("Matching successful build required")
    if (BUNDLE / "MANIFEST.json").exists():
        raise RuntimeError("Bundle exists; do not silently overwrite a reviewed candidate")
    for f in manifest["files"]:
        p = SOURCE / f["path"]
        if digest(p) != f["sha256"]:
            raise RuntimeError(f"Source changed since snapshot: {f['path']}")
        copy_file(p, BUNDLE / "02_Source" / f["path"])
        rel = Path(f["path"])
        if rel.parts[:2] == ("project", "Resources") and p.suffix.lower() not in {".blend", ".blend1"}:
            copy_file(p, BUNDLE / "01_Runtime" / Path(*rel.parts[1:]))
    for name in ["CG2_00_01.exe", "dxcompiler.dll", "dxil.dll"]:
        copy_file(SOURCE / "generated/outputs/Release" / name, BUNDLE / "01_Runtime" / name)
    runtime = BUNDLE / "01_Runtime"
    (runtime / "StartGame.cmd").write_text('@echo off\ncd /d "%~dp0"\nstart "" /wait "CG2_00_01.exe"\n', encoding="ascii")
    for folder in ["03_Documents", "04_Video", "05_Profile"]:
        (BUNDLE / folder).mkdir(parents=True, exist_ok=True)
    profile = Path("C:/Users/K024G/OneDrive/デスクトップ/クリ博/お名前フルネーム（学校名_学部名）_自己PRシートフォーマット.xlsx")
    if profile.is_file():
        copy_file(profile, BUNDLE / "05_Profile/SelfPR_DRAFT.xlsx")
    for name in ["PLAY_GUIDE.md", "SUBMISSION_CHECKLIST.md", "VIDEO_PLAN.md"]:
        copy_file(Path(__file__).parent / name, BUNDLE / name)
    write_json(BUNDLE / "MANIFEST.json", {"head": manifest["head"], "status": "DRAFT_NOT_FOR_UPLOAD",
        "files": [{"path": p.relative_to(BUNDLE).as_posix(), "bytes": p.stat().st_size, "sha256": digest(p)}
                  for p in sorted(BUNDLE.rglob("*")) if p.is_file()]})
    print(f"Candidate prepared: {BUNDLE}")


def runtime() -> None:
    manifest = json.loads((OUT / "source_manifest.json").read_text(encoding="utf-8"))
    result = json.loads((OUT / "build_result.json").read_text(encoding="utf-8"))
    if result["exit_code"] != 0 or result["head"] != manifest["head"]:
        raise RuntimeError("Matching successful build required")
    target = OUT / "runtime/FarmGame_Runtime"
    if target.exists():
        raise RuntimeError("Runtime already exists; do not overwrite a tested package")
    # Not referenced by application/engine or resource JSON/MTL at this revision.
    unused = {"bgm.wav", "Player.mp3", "IMG_0264.PNG", "魚パン.png", "仏顔.png"}
    omitted = []
    for f in manifest["files"]:
        rel = Path(f["path"])
        if rel.parts[:2] != ("project", "Resources"):
            continue
        if rel.suffix.lower() in {".blend", ".blend1"} or (len(rel.parts) == 3 and rel.name in unused):
            omitted.append(rel.as_posix())
            continue
        source = SOURCE / rel
        if digest(source) != f["sha256"]:
            raise RuntimeError(f"Snapshot changed: {rel}")
        copy_file(source, target / Path(*rel.parts[1:]))
    for name in ["CG2_00_01.exe", "dxcompiler.dll", "dxil.dll"]:
        copy_file(SOURCE / "generated/outputs/Release" / name, target / name)
    (target / "StartGame.cmd").write_text('@echo off\ncd /d "%~dp0"\nstart "" /wait "CG2_00_01.exe"\n', encoding="ascii")
    copy_file(Path(__file__).parent / "RUNTIME_README.txt", target / "README.txt")
    copy_file(Path(__file__).parent / "RUNTIME_NOTICES.txt", target / "NOTICES_PENDING.txt")
    copy_file(SOURCE / "project/externals/imgui/LICENSE.txt", target / "Licenses/ImGui.txt")
    copy_file(SOURCE / "project/Resources/fonts/OFL.txt", target / "Licenses/OFL.txt")
    write_json(OUT / "runtime_preparation.json", {"head": manifest["head"], "omitted_resources": omitted,
                                               "status": "LOCAL_CANDIDATE_LICENSE_REVIEW_PENDING"})
    print(f"Runtime prepared: {target}")


def archive_runtime() -> None:
    target = OUT / "runtime/FarmGame_Runtime"
    archive = OUT / "FarmGame_Runtime_20260928_candidate.zip"
    extracted = OUT / "verify/提出確認 空白あり"
    if archive.exists() or extracted.exists():
        raise RuntimeError("Archive/test copy already exists; do not overwrite")
    if (target / "Settings").exists():
        raise RuntimeError("Do not include playtest saves")
    files = []
    for p in sorted(target.rglob("*")):
        if not p.is_file():
            continue
        if p.is_symlink() or p.suffix.lower() in {".pdb", ".lib", ".log", ".user", ".blend", ".blend1"}:
            raise RuntimeError(f"Unexpected runtime file: {p}")
        files.append({"path": p.relative_to(target).as_posix(), "bytes": p.stat().st_size, "sha256": digest(p)})
    prep = json.loads((OUT / "runtime_preparation.json").read_text(encoding="utf-8"))
    write_json(target / "MANIFEST.json", {"head": prep["head"], "status": prep["status"], "files": files})
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for p in sorted(target.rglob("*")):
            if p.is_file():
                z.write(p, "FarmGame_Runtime/" + p.relative_to(target).as_posix())
    with zipfile.ZipFile(archive) as z:
        bad = z.testzip()
        if bad:
            raise RuntimeError(f"Corrupt member: {bad}")
        z.extractall(extracted)
    for f in files:
        if digest(extracted / "FarmGame_Runtime" / f["path"]) != f["sha256"]:
            raise RuntimeError(f"Extracted hash mismatch: {f['path']}")
    write_json(OUT / "archive_result.json", {"archive": archive.name, "bytes": archive.stat().st_size,
        "sha256": digest(archive), "verified_files": len(files), "extracted": str(extracted),
        "runtime_test": "pending", "rights_review": "pending"})
    print(f"Archive and extraction hash check passed: {archive}; files={len(files)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("step", choices=["snapshot", "build", "assemble", "runtime", "archive_runtime"])
    globals()[parser.parse_args().step]()

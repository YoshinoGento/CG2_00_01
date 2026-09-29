"""Acquire official release archives into an isolated, hash-recorded workspace."""
from pathlib import Path, PurePosixPath
import json
import urllib.request
import zipfile
from prepare_submission import OUT, digest, write_json

DEP = OUT / "known_dependencies"
SOURCES = {
    "assimp-5.3.0.zip": "https://codeload.github.com/assimp/assimp/zip/refs/tags/v5.3.0",
    "dxc_2025_02_20.zip": "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2502/dxc_2025_02_20.zip",
}


def main():
    DEP.mkdir(parents=True, exist_ok=True)
    manifest_path = DEP / "downloads.json"
    previous = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    records = {}
    for name,url in SOURCES.items():
        archive = DEP / name
        if archive.exists():
            assert name in previous and digest(archive) == previous[name]["sha256"], "Unverified existing archive"
        else:
            request = urllib.request.Request(url, headers={"User-Agent": "SuidoNogyo-dependency-provenance"})
            with urllib.request.urlopen(request, timeout=90) as response, archive.open("xb") as output:
                import shutil
                shutil.copyfileobj(response, output)
        records[name] = {"url":url, "sha256":digest(archive), "bytes":archive.stat().st_size}
        write_json(manifest_path, {**previous, **records})
        destination = DEP / ("assimp-source" if name.startswith("assimp") else "dxc-release")
        if not destination.exists() or (name.startswith("assimp") and not (destination / "cmake-modules").exists()):
            destination.mkdir(exist_ok=True)
            with zipfile.ZipFile(archive) as z:
                assert z.testzip() is None
                for info in z.infolist():
                    path = PurePosixPath(info.filename)
                    assert not path.is_absolute() and ".." not in path.parts and ":" not in info.filename
                    assert ((info.external_attr >> 16) & 0o170000) != 0o120000, "Symlink rejected"
                    if name.startswith("assimp"):
                        path = PurePosixPath(*path.parts[1:])
                        if not path.parts or (len(path.parts)>1 and path.parts[0] not in {"code","contrib","include","cmake-modules","port"}):
                            continue
                    if info.is_dir():
                        continue
                    target = destination.joinpath(*path.parts)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    if not target.exists():
                        target.write_bytes(z.read(info))
        print(name, records[name], flush=True)


if __name__ == "__main__":
    main()

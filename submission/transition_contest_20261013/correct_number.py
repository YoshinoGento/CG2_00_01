"""Correct the submission number without changing runtime or video payloads."""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "generated/transition_contest_20261013"
READY = OUT / "ready"
OLD = "LE3C_26_ヨシノ_ゲント_水路農業"
NEW = "LE3C_28_ヨシノ_ゲント_水路農業"


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def main():
    backup = OUT / "superseded_number26"
    sources = [READY / OLD, READY / f"{OLD}.zip", READY / f"{OLD}.mp4"]
    targets = [READY / NEW, READY / f"{NEW}.zip", READY / f"{NEW}.mp4"]
    for path in sources + targets + [backup]:
        if not path.resolve().is_relative_to(ROOT.resolve()):
            raise ValueError("Target outside workspace")
    if not all(path.exists() for path in sources) or any(path.exists() for path in targets + [backup]):
        raise FileExistsError("Require untouched number26 inputs and unused number28 outputs")
    package_path = OUT / "package_result.json"
    video_path = OUT / "video_result.json"
    package = json.loads(package_path.read_text(encoding="utf-8"))
    video = json.loads(video_path.read_text(encoding="utf-8"))
    assert digest(sources[1]) == package["sha256"]
    assert digest(sources[2]) == video["sha256"]
    with zipfile.ZipFile(sources[1]) as old:
        assert old.testzip() is None
        assert all(item.filename.startswith(OLD + "/") for item in old.infolist())
        with zipfile.ZipFile(targets[1], "x", zipfile.ZIP_DEFLATED, compresslevel=9) as new:
            for item in old.infolist():
                renamed = zipfile.ZipInfo(NEW + item.filename[len(OLD):], item.date_time)
                renamed.compress_type = item.compress_type
                renamed.external_attr = item.external_attr
                new.writestr(renamed, old.read(item))
        with zipfile.ZipFile(targets[1]) as new:
            assert new.testzip() is None
            assert len(new.infolist()) == len(old.infolist())
            for item in old.infolist():
                assert old.read(item) == new.read(NEW + item.filename[len(OLD):])
    backup.mkdir()
    shutil.copy2(package_path, backup / package_path.name)
    shutil.copy2(video_path, backup / video_path.name)
    sources[0].rename(targets[0])
    sources[2].rename(targets[2])
    sources[1].rename(backup / sources[1].name)
    assert digest(targets[2]) == video["sha256"]
    package.update(archive=str(targets[1]), sha256=digest(targets[1]), bytes=targets[1].stat().st_size,
                   naming_correction="Number26 to28; every uncompressed member unchanged; CRC passed")
    video["output"] = str(targets[2])
    video["naming_correction"] = "Number26 to28; video SHA256 unchanged"
    for path, value in ((package_path, package), (video_path, video)):
        path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: number28 ZIP root, all payloads unchanged, video unchanged; ZIP={package['sha256']}")


if __name__ == "__main__":
    main()

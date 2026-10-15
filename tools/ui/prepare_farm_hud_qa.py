"""Prepare an isolated current HUD runtime without changing source saves."""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "generated/codex_checks/farm_icon_hud_20261015"
RUNTIME = OUTPUT / "runtime"
BASE = ROOT / "generated/codex_checks/title_pond_polish_20261015/runtime"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def protected():
    paths = list((ROOT / "project/Settings").rglob("*"))
    paths += list((ROOT / "generated/transition_contest_20261013/ready").glob("*"))
    return {str(path.relative_to(ROOT)): digest(path) for path in paths if path.is_file()}


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    baseline = OUTPUT / "protected_before.json"
    if not baseline.exists():
        baseline.write_text(json.dumps(protected(), indent=2), encoding="utf-8")
    if not RUNTIME.exists():
        shutil.copytree(BASE, RUNTIME, ignore=shutil.ignore_patterns("*.exe", "MANIFEST.json", "StartGame.cmd"))
    for name in ("ui", "generated/text"):
        shutil.copytree(ROOT / "project/Resources" / name, RUNTIME / "Resources" / name, dirs_exist_ok=True)
    executable = ROOT / "generated/outputs/Release/CG2_00_01.exe"
    shutil.copy2(executable, RUNTIME / "FarmHudQA.exe")
    assert protected() == json.loads(baseline.read_text(encoding="utf-8")), "Protected files changed"
    print(json.dumps({"runtime": str(RUNTIME), "exe_sha256": digest(executable), "protected_files": len(protected())}))


if __name__ == "__main__":
    main()

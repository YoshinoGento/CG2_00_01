"""Create a separate start-contest delivery using the reviewed runtime packager."""
import argparse
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "contest_runtime", ROOT / "submission/transition_contest_20261013/prepare_runtime.py")
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)
runtime.OUT = ROOT / "generated/start_contest_20261020"
runtime.BUILD = runtime.OUT / "build"
runtime.TARGET = runtime.OUT / "ready" / runtime.NAME
runtime.ARCHIVE = runtime.OUT / "ready" / (runtime.NAME + ".zip")
runtime.QA = runtime.OUT / "verify 日本語 path"


def protected_files():
    files = list((ROOT / "project/Settings").rglob("*"))
    for folder in ("generated/title_contest_20261006/ready",
                   "generated/transition_contest_20261013/ready"):
        files.extend((ROOT / folder).rglob("*"))
    return {p.relative_to(ROOT).as_posix(): runtime.prep.digest(p)
            for p in files if p.is_file()}


def preserve():
    path = runtime.OUT / "protected_before.json"
    actual = protected_files()
    if path.exists():
        runtime.require(json.loads(path.read_text(encoding="utf-8")) == actual,
                        "Original Settings or prior delivery changed")
    else:
        runtime.prep.write_json(path, actual)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("step", choices=("build", "package", "verify"))
    step = parser.parse_args().step
    preserve()
    if step == "package":
        runtime.package(Path(__file__).parent)
        required = ("Resources/farm/arrival/dew.wav", "Resources/farm/arrival/morning.wav",
                    "Resources/ui/farm_hud_icons.png", "Resources/title/pond_layout.json")
        for name in required:
            runtime.require((runtime.TARGET / name).is_file(), f"Missing start asset: {name}")
    else:
        getattr(runtime, step)()
    preserve()


if __name__ == "__main__":
    main()

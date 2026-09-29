"""Create an isolated day32/1000G QA copy, never the submission preset."""
from pathlib import Path
import json
import shutil
import zipfile

from package_suido_nogyo import safe_path
from prepare_submission import ROOT, OUT, copy_file

target = OUT / "verify_free_boundary"
if target.exists():
    raise RuntimeError("Refusing to overwrite QA folder")
fixtures = list((ROOT / "generated/codex_checks").glob("free_mode_*/farm_documents.json"))
if not fixtures:
    raise RuntimeError("Run the free-mode test first")
fixture = max(fixtures, key=lambda p: p.stat().st_mtime).parent
catalog = json.loads((fixture / "farm_documents.json").read_text(encoding="utf-8"))
save_name = catalog["activeDocumentId"] + ".json"
safe_path(save_name)
save = json.loads((fixture / "saves" / save_name).read_text(encoding="utf-8"))
if save["date"]["day"] != 32 or save["economy"]["money"] != 1000 or save["playMode"] != "FreeFarming":
    raise RuntimeError("Unexpected boundary fixture")
with zipfile.ZipFile(OUT / "SuidoNogyo_FreeFarming_20260928.zip") as archive:
    for entry in archive.infolist():
        relative = safe_path(entry.filename)
        if entry.is_dir() or entry.filename.startswith("SuidoNogyo/Settings/"):
            continue
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        with archive.open(entry) as src, destination.open("xb") as dst:
            shutil.copyfileobj(src, dst)
settings = target / "SuidoNogyo/Settings/farm"
copy_file(fixture / "farm_documents.json", settings / "farm_documents.json")
copy_file(fixture / "saves" / save_name, settings / "saves" / save_name)
print("QA only:", target)

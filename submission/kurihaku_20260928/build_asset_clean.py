"""Build a fresh snapshot after removal of sample runtime asset dependencies."""
import prepare_submission as prep

prep.OUT = prep.OUT / "asset_clean_build"
prep.SOURCE = prep.OUT / "work/source"
if not (prep.OUT / "source_manifest.json").exists():
    prep.snapshot()
prep.build()
prep.build("Debug")

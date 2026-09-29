"""Snapshot and validate the free-farming Release and Debug configurations."""
import prepare_submission as prep

prep.OUT = prep.OUT / "free_build"
prep.SOURCE = prep.OUT / "work/source"
prep.snapshot()
prep.build()
prep.build("Debug")

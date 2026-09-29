"""Build a fresh source snapshot for the named game without altering earlier evidence."""
import prepare_submission as prep

prep.OUT = prep.OUT / "named_build"
prep.SOURCE = prep.OUT / "work/source"
prep.snapshot()
prep.build()

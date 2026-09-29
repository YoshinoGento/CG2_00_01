"""Retry Debug using the unchanged free-farming source snapshot."""
import prepare_submission as prep

prep.OUT = prep.OUT / "free_build"
prep.SOURCE = prep.OUT / "work/source"
prep.copy_file(prep.OUT / "work/debug_build.log", prep.OUT / "work/debug_build_initial.log")
prep.build("Debug")

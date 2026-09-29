"""Build a source-pinned dependency without changing workspace libraries."""
import subprocess
from prepare_submission import OUT, digest, write_json

DEP = OUT / "known_dependencies"
VS = "C:/Program Files/Microsoft Visual Studio/18/Community"
CMAKE = VS + "/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
NINJA = VS + "/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
options = {
    "CMAKE_BUILD_TYPE": "Release", "CMAKE_POLICY_VERSION_MINIMUM": "3.5",
    "CMAKE_MAKE_PROGRAM": NINJA, "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreaded",
    "CMAKE_CXX_FLAGS": "/DWIN32 /D_WINDOWS /W3 /GR /EHsc /utf-8",
    "CMAKE_C_FLAGS": "/DWIN32 /D_WINDOWS /W3 /utf-8",
    "CMAKE_CXX_FLAGS_RELEASE": "/O2 /Ob2 /DNDEBUG /MT",
    "CMAKE_C_FLAGS_RELEASE": "/O2 /Ob2 /DNDEBUG /MT",
    "BUILD_SHARED_LIBS": "OFF", "USE_STATIC_CRT": "ON",
    "ASSIMP_BUILD_ASSIMP_TOOLS": "OFF", "ASSIMP_BUILD_SAMPLES": "OFF",
    "ASSIMP_BUILD_TESTS": "OFF", "ASSIMP_BUILD_DOCS": "OFF",
    "ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT": "OFF",
    "ASSIMP_BUILD_OBJ_IMPORTER": "ON", "ASSIMP_BUILD_GLTF_IMPORTER": "ON",
    "ASSIMP_NO_EXPORT": "ON", "ASSIMP_BUILD_ZLIB": "ON",
    "ASSIMP_INSTALL": "OFF", "ASSIMP_BUILD_DRACO": "OFF",
    "ASSIMP_IGNORE_GIT_HASH": "ON",
}
runner = DEP / "build_assimp.cmd"
args = " ".join(f'"-D{k}={v}"' for k, v in options.items())
runner.write_text(
    '@echo off\nset "SAVED_PATH=%PATH%"\nset Path=\nset "PATH=%SAVED_PATH%"\n'
    f'call "{VS}/Common7/Tools/VsDevCmd.bat" -arch=x64 -host_arch=x64\n'
    'if errorlevel 1 exit /b %errorlevel%\n'
    f'"{CMAKE}" -S assimp-source -B assimp-build -G Ninja {args}\n'
    'if errorlevel 1 exit /b %errorlevel%\n'
    f'"{CMAKE}" --build assimp-build --target assimp --parallel 2\n'
    'exit /b %errorlevel%\n', encoding="ascii")
with (DEP / "assimp_build.log").open("wb") as log:
    result = subprocess.run(["cmd.exe", "/d", "/c", str(runner)], cwd=DEP,
                            stdout=log, stderr=subprocess.STDOUT)
write_json(DEP / "assimp_build.json", {
    "exit_code": result.returncode, "options": options,
    "source_archive_sha256": digest(DEP / "assimp-5.3.0.zip"),
    "libraries": [{"path": p.relative_to(DEP).as_posix(), "sha256": digest(p)}
                  for p in (DEP / "assimp-build").rglob("*.lib")],
})
print(f"Assimp build exit={result.returncode}", flush=True)
raise SystemExit(result.returncode)

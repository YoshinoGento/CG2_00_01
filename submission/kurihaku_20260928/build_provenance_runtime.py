"""Build Release with official Assimp headers/libraries in an isolated snapshot."""
import json
import xml.etree.ElementTree as ET
import prepare_submission as prep

BASE = prep.OUT
DEP = BASE / "known_dependencies"
prep.OUT = BASE / "provenance_build"
prep.SOURCE = prep.OUT / "work/source"


def main():
    result = json.loads((DEP / "assimp_build.json").read_text(encoding="utf-8"))
    if result["exit_code"] != 0:
        raise RuntimeError("Successful Assimp build required")
    manifest_path = prep.OUT / "source_manifest.json"
    if not manifest_path.exists():
        prep.snapshot()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not (prep.OUT / "dependency_overrides.json").exists():
        for item in manifest["files"]:
            if prep.digest(prep.SOURCE / item["path"]) != item["sha256"]:
                raise RuntimeError("Snapshot changed before substitution")
        old = {item["path"]: item for item in manifest["files"]}
        overrides = {}
        # Use a fresh include root, never mix old and official ABI headers.
        for root in (DEP / "assimp-source/include", DEP / "assimp-build/include"):
            for p in root.rglob("*"):
                if p.is_file():
                    rel = "project/externals/assimp-official/include/" + p.relative_to(root).as_posix()
                    prep.copy_file(p, prep.SOURCE / rel)
                    overrides[rel] = str(p.relative_to(BASE))
        libraries = list((DEP / "assimp-build/lib").glob("assimp*.lib"))
        if len(libraries) != 1:
            raise RuntimeError(f"Expected one Assimp library: {libraries}")
        for p, name in ((libraries[0], "assimp-official.lib"),
                        (DEP / "assimp-build/contrib/zlib/zlibstatic.lib", "zlibstatic.lib")):
            rel = "project/externals/assimp-official/lib/" + name
            prep.copy_file(p, prep.SOURCE / rel)
            overrides[rel] = str(p.relative_to(BASE))
        project_rel = "project/CG2_00_01.vcxproj"
        project = prep.SOURCE / project_rel
        tree = ET.parse(project)
        ns = prep.NS
        ET.register_namespace("", ns["m"])
        for group in tree.findall("m:ItemDefinitionGroup", ns):
            if "Release|x64" not in group.attrib.get("Condition", ""):
                continue
            for node in group.findall("m:ClCompile/m:AdditionalIncludeDirectories", ns):
                node.text = node.text.replace("externals\\assimp\\include", "externals\\assimp-official\\include")
            for node in group.findall("m:Link/m:AdditionalDependencies", ns):
                node.text = node.text.replace("assimp-vc143-mt.lib", "assimp-official.lib;zlibstatic.lib")
            for node in group.findall("m:Link/m:AdditionalLibraryDirectories", ns):
                node.text = node.text.replace("externals\\assimp\\lib\\Release", "externals\\assimp-official\\lib")
            for node in group.findall("m:PostBuildEvent/m:Command", ns):
                node.text = "\n".join(f'copy /Y "{DEP / "dxc-release/bin/x64" / name}" "$(TargetDir){name}"'
                                      for name in ("dxcompiler.dll", "dxil.dll"))
        tree.write(project, encoding="utf-8", xml_declaration=True)
        overrides[project_rel] = "isolated Release dependency paths and post-build DLLs only"
        for rel in overrides:
            p = prep.SOURCE / rel
            old[rel] = {"path": rel, "sha256": prep.digest(p), "bytes": p.stat().st_size,
                        "git_tracked": False, "submission_override": True}
        prep.write_json(prep.OUT / "dependency_overrides.json", {
            "files": overrides, "original_manifest": manifest,
            "downloads": json.loads((DEP / "downloads.json").read_text(encoding="utf-8")),
        })
        manifest["files"] = sorted(old.values(), key=lambda item: item["path"])
        prep.write_json(manifest_path, manifest)
    if not (prep.OUT / "utf8_header_additions.json").exists():
        for item in manifest["files"]:
            if prep.digest(prep.SOURCE / item["path"]) != item["sha256"]:
                raise RuntimeError("Snapshot changed before UTF-8 header completion")
        additions = []
        root = DEP / "assimp-source/contrib/utf8cpp"
        for p in root.rglob("*"):
            if not p.is_file():
                continue
            rel = "project/externals/assimp-official/contrib/utf8cpp/" + p.relative_to(root).as_posix()
            prep.copy_file(p, prep.SOURCE / rel)
            additions.append({"path": rel, "sha256": prep.digest(p), "bytes": p.stat().st_size,
                              "git_tracked": False, "submission_override": True})
        manifest["files"].extend(additions)
        prep.write_json(manifest_path, manifest)
        prep.write_json(prep.OUT / "utf8_header_additions.json", additions)
    prep.build("Release")


if __name__ == "__main__":
    main()

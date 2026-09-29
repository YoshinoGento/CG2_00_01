"""Download public license texts only; record URLs, revisions and SHA256."""
from pathlib import Path
import json
import urllib.request
from prepare_submission import OUT, ROOT, digest, write_json

DEST = OUT / "asset_notices"


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "SuidoNogyo-local-license-audit"})
    with urllib.request.urlopen(request, timeout=45) as response:
        return response.read()


def main():
    DEST.mkdir(parents=True, exist_ok=True)
    sources = {
        "DirectXTex.txt": "https://raw.githubusercontent.com/microsoft/DirectXTex/main/LICENSE",
        "nlohmann_json_3.11.3.txt": "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/LICENSE.MIT",
        "Assimp_5.3.0.txt": "https://raw.githubusercontent.com/assimp/assimp/v5.3.0/LICENSE",
        "NotoSansJP_OFL.txt": "https://raw.githubusercontent.com/google/fonts/main/ofl/notosansjp/OFL.txt",
        "DXC_1.8.2502.txt": "https://raw.githubusercontent.com/microsoft/DirectXShaderCompiler/v1.8.2502/LICENSE.TXT",
    }
    tree = json.loads(fetch("https://api.github.com/repos/assimp/assimp/git/trees/v5.3.0?recursive=1"))
    assert not tree.get("truncated"), "Incomplete license inventory"
    for item in tree["tree"]:
        p = Path(item["path"])
        if item["type"] == "blob" and item["path"].startswith("contrib/") and any(
                word in p.name.lower() for word in ("license", "copying", "copyright")):
            sources["Assimp_contrib/" + item["path"][8:]] = (
                "https://raw.githubusercontent.com/assimp/assimp/v5.3.0/" + item["path"])
    records = []
    for name,url in sources.items():
        target = DEST / name
        target.parent.mkdir(parents=True, exist_ok=True)
        data = fetch(url)
        assert data and b"<html" not in data[:200].lower(), url
        target.write_bytes(data)
        records.append({"path": name, "url": url, "sha256": digest(target)})
    imgui = ROOT / "project/externals/imgui/LICENSE.txt"
    (DEST / "ImGui.txt").write_bytes(imgui.read_bytes())
    records.append({"path": "ImGui.txt", "local_source": imgui.relative_to(ROOT).as_posix(),
                    "sha256": digest(imgui)})
    write_json(DEST / "SOURCES.json", {"files": records,
        "assimp_binary": "5.3.0, revision 4528f6dc, main; exact source commit unresolved",
        "assimp_notice_scope": "upstream 5.3.0 and all contrib license files; not proof of local build configuration",
        "dxc_binary": "Windows SDK 1.8.2502.11; source notice 1.8.2502 only, SDK distribution terms still require verification"})
    print(f"Collected {len(records)} license texts with source URLs and hashes")


if __name__ == "__main__":
    main()

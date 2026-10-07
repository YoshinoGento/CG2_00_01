import hashlib
import importlib.util
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("prepare", ROOT / "tools/title/prepare_title_ambience.py")
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)
assets = ROOT / "project/Resources/title/audio"
manifest = json.loads((assets / "audio_manifest.json").read_text())
assert set(manifest) == {"morning", "day", "evening", "night", "water"}
total_pcm = 0
for name, record in manifest.items():
    path = assets / f"{name}.wav"
    assert hashlib.sha256(path.read_bytes()).hexdigest() == record["output_sha256"]
    pcm, rate = prepare.read_pcm(path)
    assert rate == record["sample_rate"] and pcm.shape == (record["frames"], record["channels"])
    assert 10 < len(pcm) / rate <= prepare.MAX_SECONDS
    assert np.max(np.abs(pcm)) < prepare.PEAK_LIMIT + 1 / 32768
    assert 0.02 < np.sqrt(np.mean(pcm * pcm)) <= prepare.TARGET_RMS + 1 / 32768
    # A wrap jump must be comparable to ordinary sample-to-sample change, not an edit click.
    differences = np.abs(np.diff(pcm, axis=0))
    seam = float(np.max(np.abs(pcm[0] - pcm[-1])))
    limit = float(np.quantile(differences, .9999)) * 2 + 1 / 32768
    assert seam <= limit, (name, seam, limit)
    total_pcm += record["pcm_bytes"]
    print(f"{name}: {len(pcm)/rate:.3f}s; wrap jump {seam:.6f}, guard {limit:.6f}")
for invalid in [np.zeros((5000, 1)), np.ones((5, 1))]:
    try:
        prepare.loop_pcm(invalid, 44100)
    except ValueError:
        pass
    else:
        raise AssertionError("Silent/short input accepted")
credits = (assets / "AUDIO_CREDITS.txt").read_text(encoding="utf-8")
assert "On-Jin" in credits and "二次配布・無断利用を禁止" in credits and "Sound Effect Lab" in credits
for file in [ROOT / "project/CG2_00_01.vcxproj", ROOT / "project/CG2_00_01.vcxproj.filters"]:
    tree = ET.parse(file)
    entries = [entry.attrib["Include"] for entry in tree.iter() if "Include" in entry.attrib]
    for name in manifest:
        assert entries.count(f"Resources\\title\\audio\\{name}.wav") == 1
print(f"Title audio assets PASS; WAV PCM {total_pcm} bytes; engine forward+reverse expectation {total_pcm*2} bytes")

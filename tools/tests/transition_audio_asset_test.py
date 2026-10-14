import array
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import sys
import wave
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("transition_generator", ROOT / "tools/audio/generate_transition_audio.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
directory = ROOT / "project/Resources/transition/audio"
manifest = json.loads((directory / "manifest.json").read_text())
assert len(manifest["clips"]) == 7
total = 0
for index, ((name, duration, kind), record) in enumerate(zip(generator.SPECS, manifest["clips"], strict=True)):
    path = directory / (name + ".wav")
    assert record["file"] == path.name
    assert hashlib.sha256(path.read_bytes()).hexdigest() == record["sha256"]
    with wave.open(str(path), "rb") as clip:
        assert (clip.getnchannels(), clip.getsampwidth(), clip.getframerate(), clip.getcomptype()) == (1, 2, 48000, "NONE")
        assert clip.getnframes() == round(duration * 48000) == record["frames"]
        pcm = clip.readframes(clip.getnframes())
    assert pcm == generator.synthesize(index, duration, kind)
    assert pcm == generator.synthesize(index, duration, kind)
    values = array.array("h", pcm)
    if sys.byteorder != "little":
        values.byteswap()
    peak = max(abs(x) for x in values) / 32767
    rms = math.sqrt(sum((x / 32767) ** 2 for x in values) / len(values))
    assert values[0] == values[-1] == 0
    assert .51 <= peak <= .53 and .015 < rms < .23
    assert abs(sum(values) / len(values) / 32767) < .004
    assert max(abs(x) for x in values[:48]) < 900
    assert max(abs(x) for x in values[-48:]) < 900
    assert len(pcm) == record["pcm_bytes"]
    total += len(pcm)
    print(name, "peak", round(peak, 3), "RMS", round(rms, 3))
assert total < 300000
for filename in ("CG2_00_01.vcxproj", "CG2_00_01.vcxproj.filters"):
    project = ET.parse(ROOT / "project" / filename)
    items = [node.attrib["Include"] for node in project.findall(".//*[@Include]")]
    required = ["application\\scene\\TransitionAudioSystem.cpp",
                "application\\scene\\TransitionAudioSystem.h", "application\\scene\\TransitionSound.h"]
    required += ["Resources\\transition\\audio\\" + record["file"] for record in manifest["clips"]]
    for item in required:
        assert items.count(item) == 1, (filename, item)
print("Transition PCM format/hash/determinism/levels/edges/DC/budget PASS:", total, "bytes")

"""Deterministic original synthesis: no sampled third-party audio or training data."""
import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import random
import sys
import wave

RATE = 48000
SPECS = (
    ("water_drop", .32, "water"),
    ("water_sweep", .55, "wash"),
    ("water_reveal", .65, "wash_return"),
    ("shop_enter", .42, "paper"),
    ("display_enter", .28, "wood"),
    ("farm_return", .36, "shuffle"),
    ("ledger", .30, "page"),
)


def synthesize(index, duration, kind):
    rng = random.Random(7319 + index)
    count = round(duration * RATE)
    result = []
    low = 0.0
    phase = 0.0
    for i in range(count):
        t = i / RATE
        u = t / duration
        noise = rng.uniform(-1.0, 1.0)
        low += .075 * (noise - low)
        if kind == "water":
            phase += 2 * math.pi * (1550 - 1050 * min(t / .15, 1)) / RATE
            value = .45 * math.sin(phase) * math.exp(-t * 22)
            value += .12 * math.sin(phase * 1.43) * math.exp(-t * 32)
            value += .85 * low * math.exp(-t * 35)
        elif kind in ("wash", "wash_return"):
            envelope = math.sin(math.pi * u) ** 1.5
            pulse = 1 + .2 * math.sin(2 * math.pi * (6 if kind == "wash" else 4) * t)
            value = low * envelope * pulse
            if kind == "wash_return":
                value += .04 * math.sin(2 * math.pi * 210 * t) * envelope
        elif kind == "wood":
            value = (.30 * math.sin(2 * math.pi * 175 * t) * math.exp(-t * 36)
                     + .14 * math.sin(2 * math.pi * 430 * t) * math.exp(-t * 60)
                     + .4 * low * math.exp(-t * 90))
        else:
            # Distinct short friction pulses, not an electronic confirmation tone.
            pulses = ((.03, .045), (.18, .065)) if kind == "paper" else (
                ((.04, .07), (.21, .045)) if kind == "shuffle" else ((.08, .075),))
            envelope = sum(math.exp(-((t - center) / width) ** 2) for center, width in pulses)
            texture = noise - low if kind == "paper" else low + .2 * noise
            value = texture * envelope * (.6 + .4 * math.sin(2 * math.pi * 17 * t) ** 2)
        result.append(value)
    mean = sum(result) / count
    # Raised-cosine edges prevent impulses; normalization is offline, never in Play.
    for i in range(count):
        edge = min(i / (RATE * .012), (count - 1 - i) / (RATE * .018), 1.0)
        result[i] = (result[i] - mean) * (.5 - .5 * math.cos(math.pi * max(edge, 0)))
    peak = max(abs(v) for v in result)
    scale = .52 / peak
    pcm = array.array("h", (round(v * scale * 32767) for v in result))
    if sys.byteorder != "little":
        pcm.byteswap()
    return pcm.tobytes()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[2] /
                        "project/Resources/transition/audio")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {"provenance": "Original deterministic procedural synthesis; no external samples",
                "sample_rate": RATE, "channels": 1, "bits": 16, "clips": []}
    for index, (name, duration, kind) in enumerate(SPECS):
        pcm = synthesize(index, duration, kind)
        path = args.output / (name + ".wav")
        with wave.open(str(path), "wb") as output:
            output.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
            output.writeframes(pcm)
        manifest["clips"].append({"file": path.name, "frames": len(pcm) // 2,
                                  "pcm_bytes": len(pcm), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="ascii")
    print("Generated 7 original transition cues:", sum(c["pcm_bytes"] for c in manifest["clips"]), "PCM bytes")


if __name__ == "__main__":
    main()

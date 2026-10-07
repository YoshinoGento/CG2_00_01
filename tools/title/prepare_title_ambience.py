"""Prepare game-incorporated ambience loops from authorized sources, not standalone assets."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import wave

import numpy as np

SOURCES = {
    "morning": "sparrow-morning1.mp3",
    "day": "hyalessa-maculaticollis-thickets1.mp3",
    "evening": "summer-mountain2.mp3",
    "night": "summer-night-country1.mp3",
}
MAX_SECONDS = 32.0
OVERLAP_SECONDS = 0.4
TARGET_RMS = 0.12
PEAK_LIMIT = 0.85


def read_pcm(path):
    with wave.open(str(path), "rb") as wav:
        if wav.getsampwidth() != 2 or wav.getcomptype() != "NONE":
            raise ValueError("Expected PCM16")
        rate, channels = wav.getframerate(), wav.getnchannels()
        pcm = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").reshape(-1, channels)
    if channels not in (1, 2) or not pcm.size:
        raise ValueError("Empty/unsupported audio")
    return pcm.astype(np.float64) / 32768, rate


def loop_pcm(pcm, rate):
    audible = np.flatnonzero(np.max(np.abs(pcm), axis=1) > 4 / 32768)
    if not audible.size:
        raise ValueError("Silent source")
    pcm = pcm[audible[0]:audible[-1] + 1]
    maximum = round(MAX_SECONDS * rate)
    start = round(2 * rate) if len(pcm) > maximum + 2 * rate else 0
    pcm = pcm[start:start + maximum]
    overlap = round(OVERLAP_SECONDS * rate)
    if len(pcm) <= 2 * overlap:
        raise ValueError("Source too short for overlap")
    # The final overlap returns to the first retained sample with unity-sum weights.
    weight = np.linspace(0, 1, overlap)[:, None]
    weight = weight * weight * (3 - 2 * weight)
    joined = pcm[-overlap:] * (1 - weight) + pcm[:overlap] * weight
    result = np.concatenate((pcm[overlap:-overlap], joined))
    peak = float(np.max(np.abs(result)))
    rms = float(np.sqrt(np.mean(result * result)))
    gain = min(TARGET_RMS / rms, PEAK_LIMIT / peak)
    return np.rint(result * gain * 32767).astype("<i2"), start / rate, gain


def prepare(source_dir, water_source, destination):
    destination.mkdir(parents=True, exist_ok=True)
    manifest = {}
    sources = {name: source_dir / filename for name, filename in SOURCES.items()}
    sources["water"] = water_source
    for name, source in sources.items():
        output = destination / f"{name}.wav"
        probe = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "a:0",
                                "-show_entries", "stream=sample_rate,channels", "-of", "json", str(source)],
                               check=True, capture_output=True)
        stream = json.loads(probe.stdout)["streams"][0]
        rate, channels = int(stream["sample_rate"]), stream["channels"]
        if channels not in (1, 2):
            raise ValueError("Unsupported channel count")
        decoded = subprocess.run(["ffmpeg", "-v", "error", "-i", str(source),
                                  "-f", "s16le", "-c:a", "pcm_s16le", "pipe:1"],
                                 check=True, capture_output=True)
        decoded_pcm = np.frombuffer(decoded.stdout, dtype="<i2").reshape(-1, channels).astype(np.float64) / 32768
        pcm, excerpt_start, gain = loop_pcm(decoded_pcm, rate)
        with wave.open(str(output), "wb") as wav:
            wav.setnchannels(pcm.shape[1])
            wav.setsampwidth(2)
            wav.setframerate(rate)
            wav.writeframes(pcm.tobytes())
        manifest[name] = {
            "source_file": source.name,
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "output_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
            "sample_rate": rate, "channels": pcm.shape[1], "frames": len(pcm),
            "pcm_bytes": pcm.nbytes, "excerpt_start_seconds": excerpt_start,
            "normalization_gain": gain,
            "processing": "PCM16; trim silence padding; max32s excerpt; 400ms wrap overlap; RMS/peak normalization",
        }
    (destination / "audio_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source_dir", type=Path)
    parser.add_argument("water_source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    prepare(args.source_dir, args.water_source, args.destination)

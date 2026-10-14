"""Extract actual transitions at original speed, preserving recorded game audio."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "generated/transition_contest_20261013"
NAME = "LE3C_28_ヨシノ_ゲント_水路農業"
TAKES = ("recording01", "recording03", "recording04")
CLIPS = (
    (0, 56.5, 65.5, "Title to start guide"),
    (0, 83.0, 90.0, "Start guide to farm"),
    (1, 35.0, 42.0, "Farm to seed shop"),
    (1, 67.0, 72.0, "Seed shop to farm"),
    (2, 9.0, 15.0, "Farm to empty harvest display"),
    (2, 40.0, 46.0, "Harvest display to farm"),
)


def run(args):
    return subprocess.run(args, check=True, capture_output=True, text=True,
                          encoding="utf-8", errors="replace")


def main():
    target = OUT / "ready" / f"{NAME}.mp4"
    if target.exists():
        raise FileExistsError(target)
    inputs = [OUT / take / "capture.mp4" for take in TAKES]
    if not all(source.is_file() for source in inputs):
        raise FileNotFoundError("A verified recording is missing")
    graph = []
    for index in range(len(inputs)):
        graph.extend((f"[{index}:v]split=2[v{index}a][v{index}b]",
                      f"[{index}:a]asplit=2[a{index}a][a{index}b]"))
    used = [0] * len(inputs)
    for index, (take, start, end, _) in enumerate(CLIPS):
        branch = "ab"[used[take]]
        used[take] += 1
        graph.extend((f"[v{take}{branch}]trim=start={start}:end={end},setpts=PTS-STARTPTS[v{index}]",
                      f"[a{take}{branch}]atrim=start={start}:end={end},asetpts=PTS-STARTPTS[a{index}]"))
    graph.append("".join(f"[v{i}][a{i}]" for i in range(len(CLIPS))) +
                 f"concat=n={len(CLIPS)}:v=1:a=1[video][audio]")
    args = ["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error"]
    for source in inputs:
        args.extend(("-i", str(source)))
    args.extend(("-filter_complex", ";".join(graph), "-map", "[video]", "-map", "[audio]",
                 "-c:v", "libx264", "-preset", "medium", "-crf", "21", "-pix_fmt", "yuv420p",
                 "-r", "30", "-fps_mode", "cfr", "-c:a", "aac", "-b:a", "128k",
                 "-movflags", "+faststart", str(target)))
    run(args)
    decoded = run(["ffmpeg", "-nostdin", "-hide_banner", "-v", "error", "-i", str(target),
                   "-map", "0:v:0", "-map", "0:a:0", "-f", "null", "NUL"])
    if decoded.stderr.strip():
        raise RuntimeError(decoded.stderr)
    metadata = json.loads(run(["ffprobe", "-v", "error", "-show_streams", "-show_format",
                               "-of", "json", str(target)]).stdout)
    duration = sum(end - start for _, start, end, _ in CLIPS)
    streams = metadata["streams"]
    video = next(stream for stream in streams if stream["codec_type"] == "video")
    audio = next(stream for stream in streams if stream["codec_type"] == "audio")
    if (video["codec_name"], video["width"], video["height"], video["r_frame_rate"],
            audio["codec_name"]) != ("h264", 1280, 720, "30/1", "aac"):
        raise RuntimeError("Unexpected delivery streams")
    if abs(float(metadata["format"]["duration"]) - duration) > .1:
        raise RuntimeError("Unexpected delivery duration")
    levels = run(["ffmpeg", "-nostdin", "-hide_banner", "-i", str(target), "-vn",
                  "-af", "volumedetect", "-f", "null", "NUL"]).stderr
    (OUT / "video_levels.log").write_text(levels, encoding="utf-8")
    with target.open("rb") as source:
        digest = hashlib.file_digest(source, "sha256").hexdigest()
    result = {"output": str(target), "bytes": target.stat().st_size,
              "sha256": digest,
              "duration_seconds": duration, "streams": metadata, "decode_check": "passed",
              "clips": [{"take": TAKES[take], "start": start, "end": end, "route": route}
                        for take, start, end, route in CLIPS],
              "editing": "Idle time removed; transitions and recorded game-only audio remain at 1x; no added visual effects or sounds",
              "visual_review": "pending", "external_upload": "not performed"}
    (OUT / "video_result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"PASS: {target} seconds={duration} bytes={target.stat().st_size}")


if __name__ == "__main__":
    main()

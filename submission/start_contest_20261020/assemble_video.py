"""Keep the recorded opening at 1x and remove only idle time around it."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "generated/start_contest_20261020"
NAME = "LE3C_28_ヨシノ_ゲント_水路農業"
CLIPS = ((16.0, 29.5, "Title click and water-mirror transition"),
         (42.0, 62.5, "Start selection, complete 14.4-second opening and HUD"),
         (88.0, 93.0, "First till action after the opening"))


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(args):
    return subprocess.run(args, check=True, capture_output=True, text=True,
                          encoding="utf-8", errors="replace")


def main():
    source = OUT / "recording01/capture.mp4"
    target = OUT / "ready" / (NAME + ".mp4")
    if target.exists():
        raise FileExistsError(target)
    verified = json.loads((source.parent / "verification.json").read_text(encoding="utf-8"))
    if verified["decode_check"] != "passed":
        raise RuntimeError("Verified process-only recording required")
    graph = ["[0:v]split=3[v0][v1][v2]", "[0:a]asplit=3[a0][a1][a2]"]
    for i, (start, end, _) in enumerate(CLIPS):
        graph.extend((f"[v{i}]trim=start={start}:end={end},setpts=PTS-STARTPTS[vt{i}]",
                      f"[a{i}]atrim=start={start}:end={end},asetpts=PTS-STARTPTS,"
                      f"afade=t=in:d=0.03,afade=t=out:st={end-start-.03}:d=0.03[at{i}]"))
    graph.append("".join(f"[vt{i}][at{i}]" for i in range(3)) + "concat=n=3:v=1:a=1[v][a]")
    run(["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-i", str(source),
         "-filter_complex", ";".join(graph), "-map", "[v]", "-map", "[a]",
         "-c:v", "libx264", "-preset", "medium", "-crf", "21", "-pix_fmt", "yuv420p",
         "-r", "30", "-fps_mode", "cfr", "-c:a", "aac", "-b:a", "128k",
         "-movflags", "+faststart", str(target)])
    decoded = run(["ffmpeg", "-nostdin", "-v", "error", "-i", str(target),
                   "-map", "0:v:0", "-map", "0:a:0", "-f", "null", "NUL"])
    if decoded.stderr.strip():
        raise RuntimeError(decoded.stderr)
    metadata = json.loads(run(["ffprobe", "-v", "error", "-show_streams", "-show_format",
                               "-of", "json", str(target)]).stdout)
    video = next(s for s in metadata["streams"] if s["codec_type"] == "video")
    audio = next(s for s in metadata["streams"] if s["codec_type"] == "audio")
    duration = sum(end - start for start, end, _ in CLIPS)
    if (video["codec_name"], video["width"], video["height"], video["r_frame_rate"],
        audio["codec_name"], int(video["nb_frames"])) != ("h264", 1280, 720, "30/1", "aac", int(duration*30)):
        raise RuntimeError("Unexpected delivery streams/frames")
    if abs(float(metadata["format"]["duration"]) - duration) > .1:
        raise RuntimeError("Wrong delivery duration")
    levels = run(["ffmpeg", "-nostdin", "-hide_banner", "-i", str(target), "-vn",
                  "-af", "volumedetect", "-f", "null", "NUL"]).stderr
    (OUT / "video_levels.log").write_text(levels, encoding="utf-8")
    peak = re.search(r"max_volume: (-?[0-9.]+) dB", levels)
    if peak is None or not -80 < float(peak[1]) < 0:
        raise RuntimeError("Silent or clipped recorded audio")
    result = {"output": str(target), "bytes": target.stat().st_size, "sha256": digest(target),
              "source": str(source), "source_sha256": digest(source),
              "runtime_exe_sha256": digest(OUT / "ready" / NAME / "SuiroNogyo.exe"),
              "duration_seconds": duration, "decode_check": "passed", "streams": metadata,
              "clips": [{"start": start, "end": end, "content": label} for start, end, label in CLIPS],
              "editing": "1x, no added visuals or sounds; idle sections removed; 30ms audio edge fades only",
              "audio": verified["audio"], "alignment": verified["alignment"],
              "visual_review": "pending", "external_upload": "not performed"}
    (OUT / "video_result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"PASS {duration}s, {target.stat().st_size} bytes, H264/AAC, full decode")


if __name__ == "__main__":
    main()

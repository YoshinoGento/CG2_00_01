"""Bounded transition window capture with audio from the same game PID."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "generated/transition_contest_20261013"
NAME = "LE3C_28_ヨシノ_ゲント_水路農業"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("pid", type=int)
    parser.add_argument("--take", default="recording01")
    parser.add_argument("--seconds", type=int, default=90)
    parser.add_argument("--window-exe", default="TransitionContestCapture.exe")
    args = parser.parse_args()
    if not 30 <= args.seconds <= 115 or Path(args.take).name != args.take:
        parser.error("Use a simple take name and 30..115 seconds (audio helper limit: 120)")
    if re.fullmatch(r"[A-Za-z0-9_-]+\.exe", args.window_exe) is None:
        parser.error("Use a simple ASCII executable name")
    work = OUT / args.take
    work.mkdir(exist_ok=False)
    with (work / "audio.log").open("w") as audio_log, (work / "video.log").open("w") as video_log:
        audio = subprocess.Popen([str(ROOT / "generated/title_contest_20261006/capture_audio.exe"),
                                  str(args.pid), str(args.seconds + 5), str(work / "audio.wav")],
                                 stdout=audio_log, stderr=subprocess.STDOUT)
        video = None
        try:
            deadline = time.monotonic() + 12
            while "audio_start_qpc=" not in (work / "audio.log").read_text():
                if audio.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("Process-only audio capture failed")
                time.sleep(.02)
            values = (work / "audio.log").read_text().splitlines()[0].split()
            audio_start = int(values[0].split("=")[1]) / int(values[1].split("=")[1])
            video_launch = time.perf_counter()
            video = subprocess.Popen(["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "warning", "-f", "lavfi",
                "-i", f"gfxcapture=window_exe={args.window_exe}:capture_cursor=0:capture_border=0:max_framerate=30",
                "-vf", "hwdownload,format=bgra", "-t", str(args.seconds), "-an", "-c:v", "libx264", "-preset", "veryfast",
                "-crf", "21", "-pix_fmt", "yuv420p", "-r", "30", "-fps_mode", "cfr", str(work / "video.mp4")],
                stdout=video_log, stderr=subprocess.STDOUT)
            if video.wait(timeout=args.seconds + 30) != 0:
                raise RuntimeError("Window recording failed")
            if audio.wait(timeout=20) != 0:
                raise RuntimeError("Audio recording failed")
        finally:
            for process in (video, audio):
                if process is not None and process.poll() is None:
                    process.terminate()
                    process.wait(timeout=10)
    offset = max(0, video_launch - audio_start)
    target = work / "capture.mp4"
    subprocess.run(["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-i", str(work / "video.mp4"),
        "-ss", f"{offset:.6f}", "-i", str(work / "audio.wav"), "-map", "0:v:0", "-map", "1:a:0", "-t", str(args.seconds),
        "-c:v", "copy", "-c:a", "aac", "-b:a", "128k", "-af", f"afade=t=in:d=0.25,afade=t=out:st={args.seconds - .5}:d=0.5",
        "-movflags", "+faststart", str(target)], check=True)
    with (work / "decode.log").open("w") as log:
        subprocess.run(["ffmpeg", "-nostdin", "-hide_banner", "-v", "error", "-i", str(target),
                        "-map", "0:v:0", "-map", "0:a:0", "-f", "null", "NUL"], stdout=log, stderr=log, check=True)
    metadata = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(target)]))
    (work / "verification.json").write_text(json.dumps({"pid": args.pid, "output": str(target),
        "bytes": target.stat().st_size, "decode_check": "passed", "streams": metadata,
        "audio": "Actual target process-tree loopback; no microphone or unrelated app sessions",
        "alignment": "Audio starts before video; trimmed by QPC/perf_counter launch difference; first-video-frame capture latency not independently measured",
        "audio_offset_seconds": offset, "visual_review": "pending"}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"PASS transition video: {target} bytes={target.stat().st_size}")


if __name__ == "__main__":
    main()

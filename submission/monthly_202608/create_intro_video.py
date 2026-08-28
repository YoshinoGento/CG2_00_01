from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "submission" / "monthly_202608"
WORK = BASE / ".work" / "video"
OUT = BASE / "ready" / "LE3C_26_ヨシノ_ゲント_作品紹介動画.mp4"

VIDEO1 = Path(r"C:\Users\K024G\Videos\Captures\CG2 2026-08-28 10-18-44.mp4")
VIDEO2 = Path(r"C:\Users\K024G\Videos\Captures\CG2 2026-08-28 10-20-29.mp4")
FONT_REGULAR = Path(r"C:\Windows\Fonts\NotoSansJP-VF.ttf")
FONT_BOLD = Path(r"C:\Windows\Fonts\NotoSansJP-VF.ttf")

WIDTH = 1600
HEIGHT = 900


def run(args: list[str]) -> None:
    subprocess.run(args, check=True)


def draw_caption(path: Path, title: str, detail: str, *, cover: bool = False) -> None:
    image = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    if cover:
        draw.rectangle((0, 0, WIDTH, HEIGHT), fill=(12, 18, 24, 205))
        title_font = ImageFont.truetype(str(FONT_BOLD), 64)
        detail_font = ImageFont.truetype(str(FONT_REGULAR), 28)
        kicker_font = ImageFont.truetype(str(FONT_REGULAR), 22)
        draw.text((100, 250), "INDIVIDUAL GAME PROJECT", font=kicker_font, fill=(115, 207, 164, 255))
        draw.text((100, 300), title, font=title_font, fill=(255, 255, 255, 255))
        draw.text((104, 395), detail, font=detail_font, fill=(221, 231, 237, 255))
        draw.rectangle((100, 468, 490, 475), fill=(232, 181, 66, 255))
        draw.text((104, 510), "C++ / DirectX 12 / ImGui Debug Editor", font=detail_font, fill=(255, 255, 255, 255))
        draw.text((104, 560), "ヨシノ ゲント  |  LE3C_26", font=detail_font, fill=(255, 255, 255, 255))
    else:
        top = HEIGHT - 128
        draw.rectangle((0, top, WIDTH, HEIGHT), fill=(13, 20, 27, 225))
        draw.rectangle((0, top, 14, HEIGHT), fill=(60, 166, 118, 255))
        title_font = ImageFont.truetype(str(FONT_BOLD), 37)
        detail_font = ImageFont.truetype(str(FONT_REGULAR), 23)
        draw.text((48, top + 18), title, font=title_font, fill=(255, 255, 255, 255))
        draw.text((50, top + 72), detail, font=detail_font, fill=(210, 221, 228, 255))
    image.save(path)


def make_segment(ffmpeg: str, index: int, source: Path, start: float, duration: float, overlay: Path) -> Path:
    output = WORK / f"segment_{index:02d}.mp4"
    filter_graph = (
        "[0:v]scale=1600:843:flags=lanczos,"
        "pad=1600:900:0:28:color=black,fps=30000/1001[base];"
        "[base][1:v]overlay=0:0:format=auto[v]"
    )
    run(
        [
            ffmpeg,
            "-hide_banner",
            "-loglevel",
            "error",
            "-ss",
            str(start),
            "-t",
            str(duration),
            "-i",
            str(source),
            "-loop",
            "1",
            "-i",
            str(overlay),
            "-filter_complex",
            filter_graph,
            "-map",
            "[v]",
            "-map",
            "0:a?",
            "-t",
            str(duration),
            "-c:v",
            "libx264",
            "-preset",
            "medium",
            "-crf",
            "20",
            "-pix_fmt",
            "yuv420p",
            "-r",
            "30000/1001",
            "-c:a",
            "aac",
            "-b:a",
            "160k",
            "-ar",
            "48000",
            "-ac",
            "2",
            "-movflags",
            "+faststart",
            "-y",
            str(output),
        ]
    )
    return output


def main() -> None:
    ffmpeg = shutil.which("ffmpeg.exe") or shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg was not found")

    WORK.mkdir(parents=True, exist_ok=True)
    OUT.parent.mkdir(parents=True, exist_ok=True)

    descriptions = [
        ("DirectX12 3D農業サンドボックス", "地形の高さと用水路を設計し、作物の成長へつなげる", True),
        ("水源と用水路", "同じ高さ、または低い用水路だけへ水が届きます", False),
        ("状態を安全に戻す", "水源・用水路・高さの変更はUndo／RedoとSave／Loadに対応", False),
        ("種を購入", "所持金と在庫を検証し、購入した作物名と個数を表示", False),
        ("作物を選んで植える", "CキーのPie Menuでカブ／ニンジンを方向選択", False),
        ("成長と品質を可視化", "水分・成長段階・品質5軸をHUDとInspectorで確認", False),
        ("収穫して販売", "作物別在庫と売価を保持し、選択販売／全販売へつなげます", False),
    ]
    overlays = []
    for i, (title, detail, cover) in enumerate(descriptions):
        path = WORK / f"overlay_{i:02d}.png"
        draw_caption(path, title, detail, cover=cover)
        overlays.append(path)

    # The cuts intentionally show complete interactions instead of isolated UI states.
    cuts = [
        (VIDEO1, 8.0, 4.0),
        (VIDEO1, 16.0, 23.0),
        (VIDEO1, 42.0, 10.0),
        (VIDEO2, 29.0, 16.0),
        (VIDEO2, 53.0, 22.0),
        (VIDEO2, 88.0, 20.0),
        (VIDEO2, 117.0, 26.0),
    ]
    segments = [make_segment(ffmpeg, i, source, start, duration, overlays[i]) for i, (source, start, duration) in enumerate(cuts)]

    concat = WORK / "concat.txt"
    concat.write_text("".join(f"file '{path.as_posix()}'\n" for path in segments), encoding="utf-8")
    run(
        [
            ffmpeg,
            "-hide_banner",
            "-loglevel",
            "error",
            "-f",
            "concat",
            "-safe",
            "0",
            "-i",
            str(concat),
            "-c",
            "copy",
            "-movflags",
            "+faststart",
            "-y",
            str(OUT),
        ]
    )
    print(OUT)


if __name__ == "__main__":
    main()

from pathlib import Path
import argparse
from PIL import Image
from title_text_raster import label, save_asset, SERIF

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "project/Resources/title"

parser = argparse.ArgumentParser()
mode = parser.add_mutually_exclusive_group()
mode.add_argument("--reflection-only", action="store_true")
mode.add_argument("--logo-only", action="store_true")
args = parser.parse_args()

OUT.mkdir(parents=True, exist_ok=True)
logo = Image.new("RGBA", (456,168))
mask = label(logo, (10,16), "水路農業", 94, (248,247,222,255), font_path=SERIF, weight=800)
label(logo, (16,130), "SUIRO NOGYO", 20, (245,241,211,255))
if not args.reflection_only:
    save_asset(logo, OUT / "logo.png")
reflection = Image.new("RGBA",logo.size,(255,255,255,0))
reflection.putalpha(mask)
save_asset(reflection, OUT / "logo_reflection.png")
points = []
for column in range(4):
    pixels = [(x+1,y+1) for y in range(0,128,2) for x in range(10+column*94,10+(column+1)*94,2)
              if mask.getpixel((x+1,y+1)) == 255]
    assert len(pixels) >= 16
    points.extend(pixels[(index*2+1)*len(pixels)//32] for index in range(16))
header = "\n".join(["#pragma once", "// Generated from the Japanese glyph mask; regenerate with generate_title_text.py.",
                    '#include "math/Struct.h"', "#include <array>", "namespace title {",
                    "inline constexpr std::array<Vector2,64> kLogoRipplePoints{{",
                    *[f"    {{{x}.0f,{y}.0f}}," for x,y in points], "}};", "}", ""])
header_path = ROOT / "project/application/title/TitleLogoRipplePoints.h"
if not header_path.exists() or header_path.read_text(encoding="utf-8") != header:
    temporary = header_path.with_suffix(".tmp.h")
    temporary.write_text(header,encoding="utf-8",newline="\n")
    temporary.replace(header_path)
start = Image.new("RGBA", (280,80))
label(start, (140,2), "はじめる", 30, (255,250,224,255), "mt")
label(start, (140,48), "SPACE / ENTER / A", 20, (232,238,217,255), "mt")
if not args.reflection_only and not args.logo_only:
    save_asset(start, OUT / "start.png")
print("PASS title reflection mask/impact points" if args.reflection_only else
      "PASS smooth title logo/reflection; start untouched" if args.logo_only else
      "PASS smooth Serif title / Sans prompts and glyph-interior ripple sites")

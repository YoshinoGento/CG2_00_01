from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "project/Resources/title"
FONT = ROOT / "project/Resources/fonts/NotoSansJP-VF.ttf"

def font(size, weight=600):
    value = ImageFont.truetype(str(FONT), size)
    value.set_variation_by_axes([weight])
    return value

def label(draw, pos, text, face, fill, anchor="lt"):
    bounds = draw.textbbox(pos, text, font=face, anchor=anchor, stroke_width=2)
    assert bounds[0] >= 0 and bounds[1] >= 0 and bounds[2] <= draw.im.size[0] and bounds[3] <= draw.im.size[1], (text, bounds)
    draw.text(pos, text, font=face, fill=fill, anchor=anchor,
              stroke_width=2, stroke_fill=(18, 32, 27, 220))

OUT.mkdir(parents=True, exist_ok=True)
logo = Image.new("RGBA", (456,168))
draw = ImageDraw.Draw(logo)
label(draw, (10,16), "水路農業", font(94), (248,247,222,255))
label(draw, (16,130), "SUIRO NOGYO", font(20,500), (245,241,211,255))
logo.save(OUT / "logo.png")
start = Image.new("RGBA", (280,80))
draw = ImageDraw.Draw(start)
label(draw, (140,2), "はじめる", font(30), (255,250,224,255), "mt")
label(draw, (140,48), "SPACE / ENTER / A", font(16,500), (232,238,217,255), "mt")
start.save(OUT / "start.png")
print("PASS title text: Japanese logo/start, Noto Sans JP")

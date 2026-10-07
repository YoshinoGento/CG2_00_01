from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
SANS = ROOT / "project/Resources/fonts/NotoSansJP-VF.ttf"
SERIF = ROOT / "project/Resources/fonts/NotoSerifJP-VF.ttf"
SUPERSAMPLE = 4


def text_mask(canvas_size, position, text, size, anchor="lt", font_path=SANS, weight=600):
    face = ImageFont.truetype(str(font_path), size * SUPERSAMPLE)
    face.set_variation_by_axes([weight])
    high = Image.new("L", tuple(value * SUPERSAMPLE for value in canvas_size))
    draw = ImageDraw.Draw(high)
    pos = tuple(value * SUPERSAMPLE for value in position)
    bounds = draw.textbbox(pos, text, font=face, anchor=anchor)
    assert 0 <= bounds[0] < bounds[2] <= high.width
    assert 0 <= bounds[1] < bounds[3] <= high.height
    draw.text(pos, text, font=face, fill=255, anchor=anchor)
    return high


def label(image, position, text, size, fill, anchor="lt", font_path=SANS, weight=600, outline=True):
    high = text_mask(image.size, position, text, size, anchor, font_path, weight)
    if outline:
        border = high.filter(ImageFilter.MaxFilter(17)).resize(image.size, Image.Resampling.LANCZOS)
        image.paste((18, 32, 27, 230), (0, 0), border)
    mask = high.resize(image.size, Image.Resampling.LANCZOS)
    image.paste(fill, (0, 0), mask)
    return mask


def save_asset(image, path):
    if path.exists():
        with Image.open(path) as current:
            if current.size == image.size and current.convert("RGBA").tobytes() == image.tobytes():
                return
    temporary = path.with_suffix(".tmp.png")
    image.save(temporary, format="PNG")
    temporary.replace(path)

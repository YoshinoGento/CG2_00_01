from pathlib import Path
import math
import re
from PIL import Image, ImageDraw, ImageFont, ImageFilter

root = Path(__file__).resolve().parents[2]
logo = Image.open(root / "project/Resources/title/logo.png").convert("RGBA")
reflection = Image.open(root / "project/Resources/title/logo_reflection.png").convert("RGBA")
start = Image.open(root / "project/Resources/title/start.png").convert("RGBA")
assert logo.size == reflection.size == (456,168)
assert start.size == (280,80)

def pixel_mask(size,position,text,font_size,anchor="lt"):
    face = ImageFont.truetype(str(root / "project/Resources/fonts/NotoSansJP-VF.ttf"),font_size//2)
    face.set_variation_by_axes([600])
    low = Image.new("L",(size[0]//2,size[1]//2))
    pos = (position[0]//2,position[1]//2)
    draw = ImageDraw.Draw(low)
    bounds = draw.textbbox(pos,text,font=face,anchor=anchor,stroke_width=1)
    assert 0 <= bounds[0] < bounds[2] <= low.width and 0 <= bounds[1] < bounds[3] <= low.height
    draw.text(pos,text,font=face,fill=255,anchor=anchor)
    low = low.point(lambda alpha: 255 if alpha >= 128 else 0)
    return low,low.resize(size,Image.Resampling.NEAREST)

for asset in (logo,start):
    for y in range(0,asset.height,2):
        for x in range(0,asset.width,2):
            assert len({asset.getpixel((x+dx,y+dy)) for dx in (0,1) for dy in (0,1)}) == 1
_,glyph = pixel_mask(logo.size,(10,16),"水路農業",94)
assert reflection.getchannel("A").tobytes() == glyph.tobytes()
assert reflection.convert("RGB").tobytes() == bytes([255])*(456*168*3)
assert glyph.crop((0,128,456,168)).getbbox() is None
for column in range(4):
    bounds = glyph.crop((10+column*94,16,10+(column+1)*94,120))
    assert sum(alpha > 0 for alpha in bounds.tobytes()) >= 1500
for y in range(128):
    for x in range(456):
        if glyph.getpixel((x,y)):
            assert logo.getpixel((x,y)) == (248,247,222,255)
_,start_mask = pixel_mask(start.size,(140,2),"はじめる",30,"mt")
assert sum(alpha > 0 for alpha in start_mask.tobytes()) > 900
for y in range(46):
    for x in range(280):
        if start_mask.getpixel((x,y)):
            assert start.getpixel((x,y)) == (255,250,224,255)

def verify_pixel_english(image,position,text,size,fill,region,anchor="lt"):
    low,mask = pixel_mask(image.size,position,text,size,anchor)
    for char in set(text)-{" "}:
        _,char_mask = pixel_mask((size*2,size*2),(8,8),char,size)
        assert sum(alpha > 0 for alpha in char_mask.tobytes()) >= 12,char
    expected = Image.new("RGBA",image.size)
    outline = low.filter(ImageFilter.MaxFilter(3)).resize(image.size,Image.Resampling.NEAREST)
    expected.paste((18,32,27,220),(0,0),outline)
    expected.paste(fill,(0,0),mask)
    assert expected.crop(region).tobytes() == image.crop(region).tobytes()
    assert sum(alpha > 0 for alpha in mask.tobytes()) > 200

verify_pixel_english(logo,(16,130),"SUIRO NOGYO",20,(245,241,211,255),(0,128,456,168))
verify_pixel_english(start,(140,48),"SPACE / ENTER / A",20,(232,238,217,255),(0,46,280,80),"mt")
header = (root / "project/application/title/TitleLogoRipplePoints.h").read_text(encoding="utf-8")
points = [(int(x),int(y)) for x,y in re.findall(r"\{(\d+)\.0f,(\d+)\.0f\}",header)]
assert len(points) == len(set(points)) == 64
for column in range(4):
    assert sum(10+column*94 <= x < 10+(column+1)*94 for x,y in points) == 16
for x,y in points:
    assert 0 <= x < 456 and 0 <= y < 128 and glyph.getpixel((x,y)) == 255

def reference_wave(radius,front):
    distance = radius-front
    return min(1,math.exp(-.5*(distance/12)**2)+.45*math.exp(-.5*((distance+40)/12)**2))

# CPU reference only; GPU shader compilation and normal-size visual review are separate.
opaque = [(x,y) for y in range(1,128,2) for x in range(1,456,2) if glyph.getpixel((x,y))]
for center in points:
    radii = [math.hypot(p[0]-center[0],(p[1]-center[1])*1.6) for p in opaque]
    far = max(radii)
    fronts = range(0,600,2)
    near_peak = max(fronts,key=lambda front: reference_wave(0,front))
    far_peak = max(fronts,key=lambda front: reference_wave(far,front))
    assert far_peak > near_peak+100
    for strength in (.86,.64):
        peak = max(sum(reference_wave(r,front)*strength*(248-.02*255) >= 24 for r in radii)*4
                   for front in range(30,360,30))
        assert peak >= 500,(center,strength,peak)
print("PASS pixel Japanese/English, confined glyph mask, 64 opaque impact sites, outward CPU reference/night contrast")

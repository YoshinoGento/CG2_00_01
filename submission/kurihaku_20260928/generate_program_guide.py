"""Build a source-backed Japanese guide; never modifies the delivery archive."""
from pathlib import Path
from html import escape
import hashlib
import json
import re

from reportlab.pdfgen import canvas
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import Paragraph
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.utils import ImageReader
import pypdfium2 as pdfium
from pypdf import PdfReader

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).with_name('SuidoNogyo_ProgramGuide.md')
OUT = ROOT / 'generated/kurihaku_20260928/program_guide'
OUT.mkdir(parents=True, exist_ok=True)
PDF = OUT / 'プログラム説明資料（水道農業）.pdf'
pdfmetrics.registerFont(TTFont('JP', 'C:/Windows/Fonts/meiryo.ttc', subfontIndex=0))
pdfmetrics.registerFont(TTFont('JPBold', 'C:/Windows/Fonts/meiryob.ttc', subfontIndex=0))
W, H = 842, 595
INK = colors.HexColor('#202F32')
GREEN = colors.HexColor('#267758')
CYAN = colors.HexColor('#247F9D')
RED = colors.HexColor('#BA5349')
GRAY = colors.HexColor('#607174')
PALE = colors.HexColor('#F0F5F3')
RULE = colors.HexColor('#CBD9D5')
c = canvas.Canvas(str(PDF), pagesize=(W, H), pageCompression=1)
c.setTitle('水道農業 | プログラム説明資料')
c.setSubject('灌漑・編集プレビュー・品質計算の設計と検証 / 2026-09-28')
c.setAuthor('水道農業 制作資料')
rects = []


def line(x1, y1, x2, y2, color=RULE, width=1):
    c.setStrokeColor(color)
    c.setLineWidth(width)
    c.line(x1, H-y1, x2, H-y2)


def box(x, y, w, h, fill=PALE, stroke=None):
    c.setFillColor(fill)
    c.setStrokeColor(stroke or fill)
    c.rect(x, H-y-h, w, h, stroke=bool(stroke), fill=1)


def label(s, x, y, size=12, color=INK, centered=False):
    c.setFillColor(color)
    font = 'JPBold' if size >= 13 else 'JP'
    c.setFont(font, size)
    if centered:
        c.drawCentredString(x, H-y-size, s)
    else:
        c.drawString(x, H-y-size, s)
    width = pdfmetrics.stringWidth(s, font, size)
    left = x-width/2 if centered else x
    assert left >= 18 and left+width <= W-18, (s, left, width)


def paragraph(s, x, y, w, max_h, size=11.5, color=INK):
    style = ParagraphStyle('body', fontName='JP', fontSize=size, leading=17,
                           textColor=color, wordWrap='CJK', splitLongWords=True)
    p = Paragraph(escape(s), style)
    _, height = p.wrap(w, max_h)
    assert height <= max_h, (s[:30], height, max_h)
    p.drawOn(c, x, H-y-height)
    rects.append([page_no, x, y, w, height])


def arrow(x1, y1, x2, y2, color=CYAN):
    import math
    line(x1, y1, x2, y2, color, 1.8)
    angle = math.atan2(y2-y1, x2-x1)
    for offset in (-0.55, 0.55):
        line(x2, y2, x2-7*math.cos(angle+offset), y2-7*math.sin(angle+offset), color, 1.8)


def node(text, x, y, w=146, h=44, color=GREEN):
    box(x, y, w, h, colors.white, RULE)
    box(x, y, 4, h, color)
    label(text, x+w/2, y+(h-13)/2-2, 13, centered=True)


def picture(name, x, y, w, h):
    path = OUT / name
    if not path.exists():
        raise FileNotFoundError(f'Required real screenshot missing: {path}')
    image = ImageReader(str(path))
    iw, ih = image.getSize()
    scale = min(w/iw, h/ih)
    c.drawImage(image, x+(w-iw*scale)/2, H-y-ih*scale,
                width=iw*scale, height=ih*scale)


def diagram(n):
    if n == 1:
        picture('farm.png', 38, 109, 472, 190)
        label('設計の中心', 534, 112, 16, GREEN)
        for yy, a, b in [(145, '01  つながる道を判定', '高低差・接続・到達範囲'),
                         (194, '02  残った水を配分', '貯水量・要求量・給水停止'),
                         (243, '03  確認してから変更', '独立したプレビュー・履歴')]:
            label(a, 534, yy, 15)
            label(b, 534, yy+22, 11, GRAY)
        label('説明用に配置したRelease実画面 / 2026.09.28', 50, 301, 9, GRAY)
    elif n == 2:
        node('Release HUD', 50, 118, 155)
        node('ImGui 開発画面', 50, 178, 155)
        node('Bridge', 257, 146, 128)
        node('各 System', 440, 146, 134)
        node('FarmGrid', 638, 146, 145)
        arrow(205, 140, 257, 162)
        arrow(205, 200, 257, 178)
        arrow(385, 169, 440, 169)
        arrow(574, 169, 638, 169)
        label('操作要求', 220, 115, 10, GRAY)
        label('実行・検証', 391, 115, 10, GRAY)
        label('状態の参照・変更', 597, 215, 10, GRAY)
        line(50, 240, 790, 240)
        label('FixedUpdate', 50, 262, 13, GREEN)
        for x, text in [(218, '給水'), (354, '成長'), (490, '比較の記録'), (662, '日付')]:
            label(text, x, 262, 14)
        for x in (270, 403, 598):
            arrow(x, 274, x+48, 274)
        label('主要経路の要約。Scene / Bridge の依存整理は継続中。', 50, 301, 9, GRAY)
    elif n == 3:
        for x, y, name, height, color in [(57,128,'水源','H1',CYAN),(252,128,'用水路','H1',CYAN),
                                        (447,169,'用水路','H0',CYAN),(642,128,'用水路','H1',RED)]:
            box(x, y, 138, 67, colors.white, color)
            label(name, x+69, y+8, 14, color, True)
            label(height, x+69, y+33, 16, color, True)
        arrow(195,162,252,162)
        arrow(390,164,447,200)
        line(585,199,642,163,RED,1.8)
        label('×',605,158,25,RED)
        label('同じ高さ：通る',208,109,10,GRAY)
        label('低い方向：通る',400,245,11,CYAN)
        label('高い方向：通らない',624,220,11,RED)
        label('四近傍のキュー探索 + 訪問済み管理 + 上流を1つ記録', 57, 275, 15, GREEN)
        label('接続条件の模式図。矢印は流量や実際の勾配角度を表すものではありません。',57,303,9,GRAY)
    elif n == 4:
        node('水路の残量 0.4', 51, 157, 175, 66, CYAN)
        arrow(226,179,316,143)
        arrow(226,202,316,238)
        node('畑 A  要求 0.3', 316, 117, 177, 54)
        node('畑 B  要求 0.3', 316, 211, 177, 54)
        arrow(493,145,556,145)
        arrow(493,238,556,238)
        label('給水 0.2', 575, 129, 22, CYAN)
        label('給水 0.2', 575, 221, 22, CYAN)
        label('配分比率 = min(1, 0.4 / 0.6) = 2/3', 51, 278, 15, GREEN)
        label('説明用の計算例。実測値ではありません。各量はゲーム内の正規化量です。',51,303,9,GRAY)
    elif n == 5:
        node('元の農場', 48, 130, 143)
        arrow(191,151,240,151)
        node('別の FarmGrid', 240, 130, 177)
        arrow(417,151,471,151)
        node('開始時と照合', 471, 130, 160)
        arrow(631,151,682,151)
        node('確定',682,130,105)
        label('Snapshot + 世代番号', 65, 200, 13, GREEN)
        label('ここだけを変更',259,193,12,GRAY)
        line(557,174,557,233,RED,1.6)
        arrow(557,233,440,233,RED)
        label('不一致なら拒否', 304, 224, 13, RED)
        label('キャンセル時：元の農場には反映せず、プレビューを破棄',48,276,15,GREEN)
        label('確定は FarmToolActionSystem の操作・履歴へ渡します。',48,303,9,GRAY)
    elif n == 6:
        picture('quality.png', 42, 109, 403, 191)
        label('作物ごとの適正水分', 474, 111, 15, GREEN)
        start, width = 575, 212
        for y, name, low, high, col in [(160,'ニンジン',35,80,RED),(210,'トマト',45,65,GREEN),(260,'かぼちゃ',50,80,CYAN)]:
            label(name,473,y-9,12)
            box(start,y,width,6,RULE)
            box(start+width*low/100,y-4,width*(high-low)/100,14,col)
            label(f'{low} - {high}%',start,y+12,11,GRAY)
        label('品質画面：説明用配置 / Release実画面',49,303,9,GRAY)
        label('FarmRules の暫定設定。実際の栽培条件ではありません。',474,303,8.5,GRAY)
    elif n == 7:
        rows = [('検証対象', '確認した条件', '今回の結果'),
                ('灌漑・給水停止', '1 / 2 / 4 倍速・水量保存', 'PASS'),
                ('履歴・プレビュー', '範囲外・世代更新・状態変更', 'PASS'),
                ('HUD・カメラ', '入力境界・文字領域・162条件', 'PASS'),
                ('配布版の保存復元', '同じPC / 別フォルダー / 再起動', '確認済み')]
        for i,(a,b,d) in enumerate(rows):
            y=112+i*36
            if i==0: box(48,y,743,33,PALE)
            line(48,y+34,791,y+34)
            label(a,60,y+6,12,GREEN if i==0 else INK)
            label(b,272,y+6,12,INK)
            label(d,702,y+6,12,GREEN)
        label('機能の正しさを調べるテスト。描画性能や全環境動作の保証ではありません。',48,303,9,GRAY)
    else:
        columns=[(48,'ゲーム固有の機能','灌漑・生育・品質\n編集操作・HUD',GREEN),
                 (306,'利用した基盤','授業の描画・入力基盤\nOSS / フォント',CYAN),
                 (564,'支援を使った部分','実装・検証・資料整理\n画像生成',RED)]
        for x,head,body,col in columns:
            box(x,116,224,4,col)
            label(head,x,137,17,col)
            for j,s in enumerate(body.split('\n')): label(s,x,177+j*27,13)
        line(48,246,790,246)
        label('利用した基盤・支援を明確にし、実装した仕組みと課題を説明する。',48,272,16,GREEN)


parts = re.split(r'\n---\s*\n', SOURCE.read_text(encoding='utf-8'))
assert len(parts)==8
for page_no, part in enumerate(parts, 1):
    lines=part.strip().splitlines()
    title=lines[0].removeprefix('# ')
    subtitle=lines[1]
    sections=[]
    for block in re.split(r'\n## ', '\n'.join(lines[2:]))[1:]:
        heading, body=block.split('\n',1)
        sections.append((heading,body.strip().replace('\n',' ')))
    box(0,0,W,H,colors.white)
    label('PROGRAM GUIDE  /  水道農業',38,18,9,GREEN)
    label('2026.09.28',735,18,9,GRAY)
    label(title,38,38,26)
    label(subtitle,39,77,12,GRAY)
    diagram(page_no)
    line(38,326,804,326)
    for j,(heading,body) in enumerate(sections):
        if page_no==1:
            x=38+j*260; y=345; width=242; max_h=164
        else:
            x=38+(j%2)*396; y=341+(j//2)*112; width=370; max_h=85
        label(heading,x,y,13,GREEN)
        paragraph(body,x,y+23,width,max_h)
    line(38,566,804,566)
    label('個人制作 / プログラム説明資料',38,574,8.5,GRAY)
    label(f'{page_no:02d} / 08',756,573,9,GRAY)
    c.showPage()
c.save()
reader=PdfReader(PDF)
assert len(reader.pages)==8
for i,page in enumerate(reader.pages,1):
    text=page.extract_text()
    assert parts[i-1].strip().splitlines()[0].removeprefix('# ') in text
    fonts=page['/Resources']['/Font'].get_object()
    assert any('/FontFile2' in f.get_object().get('/FontDescriptor', {}).get_object()
               for f in fonts.values() if f.get_object().get('/FontDescriptor'))
doc=pdfium.PdfDocument(str(PDF))
for i in range(len(doc)):
    doc[i].render(scale=1.7).to_pil().save(OUT/f'page_{i+1:02d}.png')
metadata={'pages':8,'pdf_filename':PDF.name,'pdf_bytes':PDF.stat().st_size,
          'pdf_sha256':hashlib.sha256(PDF.read_bytes()).hexdigest(),
          'markdown_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
          'font':'Meiryo regular/bold embedded subsets','paragraph_bounds_checked':len(rects),
          'personal_wording_review':'pending','performance_measurement':'not performed'}
(OUT/'verification.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(metadata,ensure_ascii=False,indent=2))

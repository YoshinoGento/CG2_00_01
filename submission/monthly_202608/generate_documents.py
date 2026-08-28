from __future__ import annotations

from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    Flowable,
    Image,
    KeepTogether,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / "submission" / "monthly_202608" / ".work"
OUT = ROOT / "submission" / "monthly_202608" / "ready"
FRAMES = WORK / "frames"
FONT = ROOT / "project" / "Resources" / "fonts" / "NotoSansJP-VF.ttf"

ACCOUNT = "LE3C_26_ヨシノ_ゲント"
PORTFOLIO = OUT / f"{ACCOUNT}_ポートフォリオ.pdf"
PROGRAM = OUT / f"{ACCOUNT}_プログラム説明資料.pdf"

PAGE = landscape(A4)
PW, PH = PAGE
MARGIN = 15 * mm

NAVY = colors.HexColor("#17212B")
INK = colors.HexColor("#20262E")
MUTED = colors.HexColor("#5E6874")
PAPER = colors.HexColor("#F5F7F8")
GREEN = colors.HexColor("#2E7D5B")
GREEN_LIGHT = colors.HexColor("#DDEFE7")
BLUE = colors.HexColor("#3178A5")
BLUE_LIGHT = colors.HexColor("#DDEAF2")
GOLD = colors.HexColor("#C28A22")
GOLD_LIGHT = colors.HexColor("#F5E8C8")
RED = colors.HexColor("#A9483E")
WHITE = colors.white


def register_fonts() -> None:
    pdfmetrics.registerFont(TTFont("NotoJP", str(FONT)))
    pdfmetrics.registerFont(TTFont("NotoJP-Bold", str(FONT)))


def styles() -> dict[str, ParagraphStyle]:
    base = getSampleStyleSheet()
    return {
        "title": ParagraphStyle(
            "title",
            parent=base["Title"],
            fontName="NotoJP-Bold",
            fontSize=25,
            leading=31,
            textColor=WHITE,
            alignment=TA_LEFT,
            wordWrap="CJK",
            spaceAfter=4 * mm,
        ),
        "subtitle": ParagraphStyle(
            "subtitle",
            parent=base["Normal"],
            fontName="NotoJP",
            fontSize=11,
            leading=17,
            textColor=colors.HexColor("#DDE5EB"),
            wordWrap="CJK",
        ),
        "h1": ParagraphStyle(
            "h1",
            parent=base["Heading1"],
            fontName="NotoJP-Bold",
            fontSize=19,
            leading=24,
            textColor=NAVY,
            wordWrap="CJK",
            spaceAfter=4 * mm,
        ),
        "h2": ParagraphStyle(
            "h2",
            parent=base["Heading2"],
            fontName="NotoJP-Bold",
            fontSize=12,
            leading=16,
            textColor=NAVY,
            wordWrap="CJK",
            spaceAfter=1.5 * mm,
        ),
        "body": ParagraphStyle(
            "body",
            parent=base["BodyText"],
            fontName="NotoJP",
            fontSize=9.2,
            leading=14.2,
            textColor=INK,
            wordWrap="CJK",
            spaceAfter=2 * mm,
        ),
        "small": ParagraphStyle(
            "small",
            parent=base["BodyText"],
            fontName="NotoJP",
            fontSize=7.5,
            leading=11,
            textColor=MUTED,
            wordWrap="CJK",
        ),
        "card": ParagraphStyle(
            "card",
            parent=base["BodyText"],
            fontName="NotoJP",
            fontSize=8.5,
            leading=13,
            textColor=INK,
            wordWrap="CJK",
        ),
        "card_title": ParagraphStyle(
            "card_title",
            parent=base["Heading3"],
            fontName="NotoJP-Bold",
            fontSize=10.2,
            leading=14,
            textColor=NAVY,
            wordWrap="CJK",
            spaceAfter=1.2 * mm,
        ),
        "center": ParagraphStyle(
            "center",
            parent=base["BodyText"],
            fontName="NotoJP-Bold",
            fontSize=9,
            leading=13,
            alignment=TA_CENTER,
            textColor=INK,
            wordWrap="CJK",
        ),
        "mono": ParagraphStyle(
            "mono",
            parent=base["Code"],
            fontName="NotoJP",
            fontSize=7.8,
            leading=12,
            textColor=INK,
            backColor=colors.HexColor("#EDF0F2"),
            borderPadding=7,
            wordWrap="CJK",
        ),
    }


S = {}


class PageNumber(Flowable):
    def draw(self) -> None:
        pass


def footer(canvas, doc) -> None:
    canvas.saveState()
    canvas.setFillColor(PAPER)
    canvas.rect(0, 0, PW, PH, fill=1, stroke=0)
    canvas.setFillColor(NAVY)
    canvas.rect(0, PH - 8 * mm, PW, 8 * mm, fill=1, stroke=0)
    canvas.setFillColor(MUTED)
    canvas.setFont("NotoJP", 7)
    canvas.drawString(MARGIN, 6 * mm, "DirectX12 3D農業サンドボックスゲーム")
    canvas.drawRightString(PW - MARGIN, 6 * mm, f"{ACCOUNT}  |  {doc.page}")
    canvas.restoreState()


def p(text: str, style: str = "body") -> Paragraph:
    return Paragraph(text, S[style])


def bullets(items: list[str], style: str = "body") -> list[Paragraph]:
    return [Paragraph(f"• {item}", S[style]) for item in items]


def section_title(index: str, title: str, lead: str | None = None) -> list:
    out = [p(f"{index}  {title}", "h1")]
    if lead:
        out.append(p(lead, "body"))
    return out


def card(title: str, body: str, color=GREEN_LIGHT) -> Table:
    cell = [p(title, "card_title"), p(body, "card")]
    t = Table([[cell]], colWidths=[82 * mm])
    t.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), color),
                ("BOX", (0, 0), (-1, -1), 0.6, colors.HexColor("#C7D0D6")),
                ("LEFTPADDING", (0, 0), (-1, -1), 10),
                ("RIGHTPADDING", (0, 0), (-1, -1), 10),
                ("TOPPADDING", (0, 0), (-1, -1), 9),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 9),
            ]
        )
    )
    return t


def cards_row(items: list[tuple[str, str, colors.Color]]) -> Table:
    cells = []
    for title, body, color in items:
        cells.append([p(title, "card_title"), p(body, "card")])
    widths = [(PW - 2 * MARGIN - (len(items) - 1) * 4 * mm) / len(items)] * len(items)
    t = Table([cells], colWidths=widths, hAlign="LEFT")
    commands = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("BOX", (0, 0), (-1, -1), 0.6, colors.HexColor("#C7D0D6")),
        ("INNERGRID", (0, 0), (-1, -1), 4 * mm, PAPER),
        ("LEFTPADDING", (0, 0), (-1, -1), 9),
        ("RIGHTPADDING", (0, 0), (-1, -1), 9),
        ("TOPPADDING", (0, 0), (-1, -1), 9),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 9),
    ]
    for i, (_, _, color) in enumerate(items):
        commands.append(("BACKGROUND", (i, 0), (i, 0), color))
    t.setStyle(TableStyle(commands))
    return t


def image(path: Path, width: float, height: float) -> Image:
    im = Image(str(path), width=width, height=height)
    im.hAlign = "LEFT"
    return im


def captioned_image(path: Path, caption: str, width: float, height: float) -> Table:
    t = Table([[image(path, width, height)], [p(caption, "small")]], colWidths=[width])
    t.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), WHITE),
                ("BOX", (0, 0), (-1, -1), 0.6, colors.HexColor("#C7D0D6")),
                ("LEFTPADDING", (0, 0), (-1, -1), 4),
                ("RIGHTPADDING", (0, 0), (-1, -1), 4),
                ("TOPPADDING", (0, 0), (-1, -1), 4),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
            ]
        )
    )
    return t


def flow_row(labels: list[str], colors_list: list[colors.Color]) -> Table:
    cells = []
    for i, label in enumerate(labels):
        cells.append(p(label, "center"))
        if i != len(labels) - 1:
            cells.append(p("→", "center"))
    widths = []
    label_width = (PW - 2 * MARGIN - (len(labels) - 1) * 8 * mm) / len(labels)
    for i in range(len(cells)):
        widths.append(label_width if i % 2 == 0 else 8 * mm)
    t = Table([cells], colWidths=widths, rowHeights=[20 * mm])
    cmds = [("VALIGN", (0, 0), (-1, -1), "MIDDLE")]
    for i, color in enumerate(colors_list):
        col = i * 2
        cmds.extend(
            [
                ("BACKGROUND", (col, 0), (col, 0), color),
                ("BOX", (col, 0), (col, 0), 0.7, colors.HexColor("#AEB9C1")),
                ("LEFTPADDING", (col, 0), (col, 0), 5),
                ("RIGHTPADDING", (col, 0), (col, 0), 5),
            ]
        )
    t.setStyle(TableStyle(cmds))
    return t


def cover_story(title: str, subtitle: str, hero_path: Path, footer_text: str) -> list:
    hero = image(hero_path, PW - 2 * MARGIN, 91 * mm)
    header = Table(
        [[[p(title, "title"), p(subtitle, "subtitle")]]],
        colWidths=[PW - 2 * MARGIN],
    )
    header.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), NAVY),
                ("LEFTPADDING", (0, 0), (-1, -1), 16),
                ("RIGHTPADDING", (0, 0), (-1, -1), 16),
                ("TOPPADDING", (0, 0), (-1, -1), 14),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 14),
            ]
        )
    )
    return [header, Spacer(1, 5 * mm), hero, Spacer(1, 3 * mm), p(footer_text, "small")]


def build_portfolio() -> None:
    story = []
    story += cover_story(
        "DirectX12 3D農業サンドボックスゲーム",
        "土地の高さと用水路を設計し、水の流れを作物の成長へつなげる自作C++ゲーム",
        FRAMES / "v2_07_120.png",
        "個人制作 / C++ / DirectX 12 / ImGui Debug Editor / 2026年8月時点プロトタイプ",
    )
    story.append(PageBreak())

    story += section_title(
        "01",
        "作品概要",
        "農作業だけでなく、地形の高低差と水路設計をゲーム判断にすることを目標とした3D農業サンドボックスです。現在は、栽培・販売・品質評価に加え、水源から同じ高さまたは低い用水路へ水が届く垂直スライスまで実装しています。",
    )
    story.append(
        flow_row(
            ["土地を選ぶ", "高さを変える", "水源・用水路", "種を植える", "品質を確認", "収穫・販売"],
            [GREEN_LIGHT, BLUE_LIGHT, BLUE_LIGHT, GREEN_LIGHT, GOLD_LIGHT, GOLD_LIGHT],
        )
    )
    story.append(Spacer(1, 5 * mm))
    story.append(
        cards_row(
            [
                ("現在遊べる範囲", "5×4農地、耕す・水やり・種まき・成長・収穫、カブ／ニンジン、種購入、品質・売価、選択販売／全販売、Undo／Redo、Save／Load。", GREEN_LIGHT),
                ("独自性の核", "H0〜H2の高さ、土地機能としての水源・用水路、4近傍接続、同じ高さ／下り方向だけへ通水する判定。", BLUE_LIGHT),
                ("今後の完成像", "用水路から土壌への浸潤、水量と流向、品質・サイズへの影響、30日目の巨大作物コンテスト。", GOLD_LIGHT),
            ]
        )
    )
    story.append(Spacer(1, 5 * mm))
    story.append(
        Table(
            [[p("担当", "h2"), p("企画、ゲームロジック、エディタ、DirectX12描画基盤、デバッグ可視化、資料作成", "body")],
             [p("開発環境", "h2"), p("Visual Studio / C++20 / DirectX 12 / HLSL / ImGui / JSON", "body")]],
            colWidths=[32 * mm, PW - 2 * MARGIN - 32 * mm],
            style=TableStyle([
                ("BACKGROUND", (0, 0), (0, -1), NAVY),
                ("TEXTCOLOR", (0, 0), (0, -1), WHITE),
                ("BOX", (0, 0), (-1, -1), 0.5, colors.HexColor("#C7D0D6")),
                ("INNERGRID", (0, 0), (-1, -1), 0.5, colors.HexColor("#D9E0E4")),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("LEFTPADDING", (0, 0), (-1, -1), 8),
                ("RIGHTPADDING", (0, 0), (-1, -1), 8),
                ("TOPPADDING", (0, 0), (-1, -1), 7),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
            ]),
        )
    )
    story.append(PageBreak())

    story += section_title("02", "実装したゲームプレイ", "同じ実行環境で、農作業、経済、品質、用水路を確認できます。画面内HUDはゲーム用Sprite表示、左右と下部は検証用ImGuiです。")
    w = (PW - 2 * MARGIN - 5 * mm) / 2
    story.append(
        Table(
            [[captioned_image(FRAMES / "v1_04_036.png", "水源から接続した用水路へ通水。畑マップでも供給状態を色分け。", w, 50 * mm),
              captioned_image(FRAMES / "v2_03_056.png", "種購入後、CキーのPie Menuでカブ／ニンジンを選択して植える。", w, 50 * mm)],
             [captioned_image(FRAMES / "v2_06_104.png", "収穫物を大きさ・形・色・水分・成長で採点し、レーダーチャートで表示。", w, 50 * mm),
              captioned_image(FRAMES / "v2_07_120.png", "作物別在庫と売価を保持し、選択販売または全販売へつなげる。", w, 50 * mm)]],
            colWidths=[w, w],
            style=TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0), ("RIGHTPADDING", (0, 0), (-1, -1), 0), ("TOPPADDING", (0, 0), (-1, -1), 2), ("BOTTOMPADDING", (0, 0), (-1, -1), 2)]),
        )
    )
    story.append(PageBreak())

    story += section_title("03", "設計と実装で重視したこと", "機能追加でSceneが肥大化しないよう、状態変更、進行統括、表示を分離しています。")
    story.append(
        flow_row(
            ["Input<br/>要求だけ生成", "GamePlayScene<br/>処理順を統括", "Farm Systems<br/>検証と状態変更", "FarmGrid<br/>正規データ", "HUD / Inspector<br/>表示だけ"],
            [BLUE_LIGHT, PAPER, GREEN_LIGHT, GOLD_LIGHT, PAPER],
        )
    )
    story.append(Spacer(1, 5 * mm))
    story.append(
        cards_row(
            [
                ("責務分離", "FarmToolActionSystemが操作、FarmGrowthSystemが成長、FarmEconomySystemが購入・販売、FarmIrrigationSystemが通水判定を担当。UIはViewDataを描画し、状態を直接変更しません。", GREEN_LIGHT),
                ("安全性", "OOB、不正enum、非有限値、負数、金額overflow、未初期化、壊れたJSONを検証。UndoとLoadは複数Systemの整合性を保って復元します。", GOLD_LIGHT),
                ("DX12境界", "Descriptor、ResourceBarrier、Fence、VRAM所有権を農業ロジックへ露出させず、既存Sprite／Object3d／LineDrawer経由で表示します。", BLUE_LIGHT),
            ]
        )
    )
    story.append(Spacer(1, 6 * mm))
    story.append(p("用水路の判定規則", "h2"))
    story.append(p("水源を起点に4近傍を探索し、次のマスが用水路で、かつ高さが現在以下の場合だけ供給済みにします。斜め接続と上り方向は拒否し、供給状態は保存せず、地形から再計算します。処理量はO(tile count)です。", "body"))
    story.append(PageBreak())

    story += section_title("04", "改善の進め方と今後", "一度に大規模な水シミュレーションへ進まず、動画で確認できる小さな垂直スライスを積み重ねています。")
    roadmap = [
        [p("段階", "center"), p("成果", "center"), p("状態", "center")],
        [p("栽培MVP", "card"), p("耕す→水やり→植える→成長→収穫→販売", "card"), p("実装済み", "center")],
        [p("品質と選択", "card"), p("作物選択、種購入、品質5軸、レーダーチャート、作物別売価", "card"), p("実装済み", "center")],
        [p("用水路 Stage 1", "card"), p("水源、用水路、高さ、接続、乾燥／通水表示、Undo／Save", "card"), p("実装済み", "center")],
        [p("用水路 Stage 2", "card"), p("隣接土壌への浸潤、水量、流向、蒸発、成長・品質との接続", "card"), p("次段階", "center")],
        [p("ゲーム完成", "card"), p("30日制、コンテスト、巨大作物、結果画面、ゲームバランス", "card"), p("計画", "center")],
    ]
    t = Table(roadmap, colWidths=[38 * mm, PW - 2 * MARGIN - 68 * mm, 30 * mm])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), NAVY),
        ("TEXTCOLOR", (0, 0), (-1, 0), WHITE),
        ("BACKGROUND", (0, 1), (-1, -1), WHITE),
        ("GRID", (0, 0), (-1, -1), 0.5, colors.HexColor("#CBD3D9")),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("TOPPADDING", (0, 0), (-1, -1), 7),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
        ("BACKGROUND", (2, 1), (2, 3), GREEN_LIGHT),
        ("BACKGROUND", (2, 4), (2, 4), GOLD_LIGHT),
        ("BACKGROUND", (2, 5), (2, 5), PAPER),
    ]))
    story.append(t)
    story.append(Spacer(1, 6 * mm))
    story.append(cards_row([
        ("GitHub", "https://github.com/YoshinoGento/CG2_00_01", BLUE_LIGHT),
        ("応募者", "ヨシノ ゲント / LE3C_26", GREEN_LIGHT),
        ("資料基準", "2026年8月28日の個人就職作品ブランチ", GOLD_LIGHT),
    ]))

    OUT.mkdir(parents=True, exist_ok=True)
    doc = SimpleDocTemplate(str(PORTFOLIO), pagesize=PAGE, leftMargin=MARGIN, rightMargin=MARGIN, topMargin=12 * mm, bottomMargin=12 * mm, title="DirectX12 3D農業サンドボックスゲーム ポートフォリオ", author="ヨシノ ゲント")
    doc.build(story, onFirstPage=footer, onLaterPages=footer)


def build_program() -> None:
    story = []
    story += cover_story(
        "プログラム説明資料",
        "DirectX12 3D農業サンドボックスゲーム / 責務分離・状態遷移・安全性・用水路設計",
        FRAMES / "v1_04_036.png",
        "対象コミット: a1d925e / 個人就職作品 / 2026年8月28日",
    )
    story.append(PageBreak())

    story += section_title("01", "全体アーキテクチャ", "Gameは起動とメインループ、Sceneは進行統括、Systemは状態変更、UIは表示と入力通知だけを担当します。")
    story.append(flow_row(
        ["WinApp / Game<br/>起動・Main Loop", "GamePlayScene<br/>処理順・ViewData", "Farm Systems<br/>規則・状態変更", "FarmGrid / Data<br/>正規状態", "HUD / Editor<br/>表示・要求"],
        [PAPER, BLUE_LIGHT, GREEN_LIGHT, GOLD_LIGHT, PAPER],
    ))
    story.append(Spacer(1, 5 * mm))
    rows = [
        ["層", "主なクラス", "責務"],
        ["Application", "Game / SceneManager / GamePlayScene", "起動、シーン切替、各Systemの処理順、ViewData作成"],
        ["Farm Data", "FarmGrid / FarmTypes / FarmRules", "Tile配列、範囲検証、enum、設定値、派生状態"],
        ["Farm Systems", "ToolAction / Growth / Economy / Irrigation / Document", "入力要求の検証、状態変更、Undo、保存、通水探索"],
        ["Presentation", "FarmHUD / FarmControllerWindow / FarmVisualSystem", "読み取り専用Snapshot表示、操作要求の通知、簡易3D可視化"],
        ["Engine", "DirectXCommon / Object3d / Sprite / LineDrawer", "DX12 Resource、Descriptor、Shader、描画コマンド"],
    ]
    table = Table([[p(c, "card") for c in row] for row in rows], colWidths=[30 * mm, 78 * mm, PW - 2 * MARGIN - 108 * mm])
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), NAVY), ("TEXTCOLOR", (0, 0), (-1, 0), WHITE),
        ("GRID", (0, 0), (-1, -1), 0.5, colors.HexColor("#CAD2D8")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 7),
        ("RIGHTPADDING", (0, 0), (-1, -1), 7), ("TOPPADDING", (0, 0), (-1, -1), 6),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
    ]))
    story.append(table)
    story.append(PageBreak())

    story += section_title("02", "FarmGridと状態モデル", "栽培状態と土地機能を分離し、将来の水シミュレーションを既存の栽培ループへ無理に混ぜない構造です。")
    story.append(cards_row([
        ("FarmTileState", "Empty / Tilled / Planted。耕作の状態遷移を表す。", GREEN_LIGHT),
        ("FarmTileFeature", "None / Canal / WaterSource。栽培とは直交する土地機能。", BLUE_LIGHT),
        ("派生状態", "GrowthStage、MoistureStatus、CanalSupply。再計算可能な値は保存しない。", GOLD_LIGHT),
    ]))
    story.append(Spacer(1, 5 * mm))
    story.append(p("主要データ", "h2"))
    story.extend(bullets([
        "heightLevel: H0〜H2。変更時に範囲をClampし、通水探索を再実行。",
        "crop / moisture / growth: 作物種、正規化水分量、正規化成長度。非有限値と範囲外を拒否。",
        "feature: 水源または用水路。栽培データとの重複を禁止。",
        "selectedTile: indexと座標変換を一元化し、空GridとOOBは失敗値を返す。",
    ]))
    story.append(Spacer(1, 3 * mm))
    story.append(p("状態遷移の例", "h2"))
    story.append(flow_row(["Empty", "Hoe", "Tilled", "Seed", "Planted", "Growth 100%", "Harvest"], [PAPER, GREEN_LIGHT, GREEN_LIGHT, GOLD_LIGHT, GREEN_LIGHT, GOLD_LIGHT, PAPER]))
    story.append(PageBreak())

    story += section_title("03", "用水路と通水判定", "FarmIrrigationSystemはGridを変更せず、地形と土地機能から供給状態を導出します。")
    story.append(
        Table(
            [[captioned_image(FRAMES / "v1_03_028.png", "水源と用水路を配置。高さと接続状態をInspectorで確認。", 125 * mm, 66 * mm),
              [p("処理順", "h2"), *bullets([
                  "1. 全Tileの供給状態を未供給で初期化。",
                  "2. WaterSourceを探索開始点としてQueueへ追加。",
                  "3. 4近傍のCanalだけを候補にする。",
                  "4. neighbor.height <= current.height の場合だけ供給。",
                  "5. 訪問済みを記録し、Grid全体を一度だけ処理。",
              ], "card")]]],
            colWidths=[125 * mm, PW - 2 * MARGIN - 125 * mm],
            style=TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5)]),
        )
    )
    story.append(Spacer(1, 4 * mm))
    story.append(cards_row([
        ("計算量", "O(tile count)。毎frameのfile IO、GPU allocation、Descriptor確保は行わない。", GREEN_LIGHT),
        ("保存方針", "WaterSourceとCanalは保存。Supplied/Dryは派生状態なのでLoad後に再計算。", BLUE_LIGHT),
        ("現在の制限", "水量、流向、滞留、蒸発、土壌浸潤は未実装。表示はLineDrawerによる検証用。", GOLD_LIGHT),
    ]))
    story.append(PageBreak())

    story += section_title("04", "作物・品質・経済", "作物ごとの成長プロファイルと収穫品質を分離し、将来の作物追加とバランス調整に備えています。")
    story.append(
        Table(
            [[captioned_image(FRAMES / "v2_06_104.png", "品質5軸をレーダーチャートで可視化。", 120 * mm, 64 * mm),
              [p("主要System", "h2"), *bullets([
                  "FarmGrowthSystem: 作物別の成長速度、水分閾値、収穫予測。",
                  "FarmQualitySystem: 大きさ・形・色・水分・成長の5軸評価。",
                  "FarmEconomySystem: 種購入、作物別在庫、選択販売、全販売。",
                  "FarmFeedbackSystem: 直近の購入・収穫・販売結果を境界付きで保持。",
              ], "card")]]],
            colWidths=[120 * mm, PW - 2 * MARGIN - 120 * mm],
            style=TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5)]),
        )
    )
    story.append(Spacer(1, 4 * mm))
    story.append(flow_row(["種を購入", "作物を選択", "成長プロファイル", "品質を採点", "在庫へ追加", "販売してG獲得"], [GOLD_LIGHT, GREEN_LIGHT, GREEN_LIGHT, BLUE_LIGHT, GOLD_LIGHT, GOLD_LIGHT]))
    story.append(Spacer(1, 4 * mm))
    story.append(p("実行時安全性", "h2"))
    story.extend(bullets([
        "未対応CropType、在庫0、価格0以下、不正なindexを状態変更前に拒否。",
        "所持金加算はoverflowを確認し、失敗時は在庫を減らさない。",
        "品質値は有限性と範囲を検証し、Undo時は収穫前の品質履歴と在庫価値まで復元。",
    ]))
    story.append(PageBreak())

    story += section_title("05", "Undo / RedoとFarm Document", "操作履歴と永続化を別責務にし、編集操作とファイル復元のどちらでも複数Systemの整合性を守ります。")
    story.append(cards_row([
        ("Command History", "Tile変更、収穫、販売に必要なBefore/After Snapshotを保持。適用失敗時は履歴位置を進めない。", GREEN_LIGHT),
        ("Document Schema 4", "Grid、feature、economy、seed inventory、selected crop、last qualityをJSON保存。旧Schemaも読み込み可能。", BLUE_LIGHT),
        ("Atomic Save", "一時ファイルへ書き、成功後に置換。破損JSON、型不一致、範囲外値を拒否。", GOLD_LIGHT),
    ]))
    story.append(Spacer(1, 6 * mm))
    story.append(p("Load処理", "h2"))
    story.append(flow_row(["JSON Parse", "Schema検証", "一時Snapshot", "Grid復元", "Economy復元", "派生状態再構築"], [PAPER, GOLD_LIGHT, PAPER, GREEN_LIGHT, GREEN_LIGHT, BLUE_LIGHT]))
    story.append(Spacer(1, 5 * mm))
    story.append(p("失敗時の扱い", "h2"))
    story.append(p("各SystemのRestoreを個別に試行し、途中失敗では元SnapshotへRollbackします。短絡評価で復元処理が飛ばないようにし、失敗をUIへ通知します。保存対象ではない通水状態や成長ステージは、復元後の正規データから再計算します。", "body"))
    story.append(PageBreak())

    story += section_title("06", "HUDとDebug Editor", "プレイヤー向けHUDはSprite系、開発者向けInspectorはImGuiで分離しています。")
    story.append(
        Table(
            [[captioned_image(FRAMES / "v2_07_120.png", "中央Game View内がRuntime HUD。左右と下部はDebug Editor。", 132 * mm, 70 * mm),
              [p("Runtime HUD", "h2"), p("日本語ラベル、Day、所持金、選択Tile、道具、作物、在庫、品質、売却結果をSprite／BitmapFont系で表示。Releaseでも使用可能。", "card"), Spacer(1, 3 * mm),
               p("Debug Editor", "h2"), p("Farm Map、Inspector、品質レーダー、Save／Load、Undo／Redo、検証状態をImGuiで表示。ゲーム状態は直接書き換えず、typed requestをSceneへ通知。", "card")]]],
            colWidths=[132 * mm, PW - 2 * MARGIN - 132 * mm],
            style=TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5)]),
        )
    )
    story.append(Spacer(1, 4 * mm))
    story.append(cards_row([
        ("可読性", "情報を左上・右上・左下・下中央へ分散し、背景パネルと余白を固定。日本語の長さに合わせて領域を確保。", GREEN_LIGHT),
        ("検証性", "同じSnapshotをHUD、Map、Inspectorへ渡し、選択Tile・水分・成長・在庫・通水の不一致を発見しやすくする。", BLUE_LIGHT),
        ("境界", "UIに成長式、売価計算、通水探索、Save処理を置かない。UIは描画と入力通知だけ。", GOLD_LIGHT),
    ]))
    story.append(PageBreak())

    story += section_title("07", "DirectX12境界と性能リスク", "農業機能の追加で描画資源の所有権や同期規則を崩さないことを優先しています。")
    checks = [
        ["確認項目", "現在の方針", "今後のリスク"],
        ["Resource / VRAM", "Texture・Model・Sprite・LineDrawer側が所有。FarmはHandle／ViewDataのみ。", "Asset unloadとgeneration管理の全経路適用。"],
        ["Descriptor", "上位ゲームロジックへDescriptor indexを公開しない。", "動的Asset増加時のHeap枯渇監視。"],
        ["Barrier / Fence", "今回のFarm更新ではGPU Resource状態を変更しない。", "水面描画追加時のRenderTarget／Compute同期。"],
        ["CPU負荷", "Grid更新と通水探索はO(tile count)。固定長配列と再利用vectorを使用。", "Grid拡大時はdirty region化を検討。"],
        ["DrawCall", "検証用LineDrawerはTile単位。", "完成描画ではinstancing／batchingが必要。"],
        ["Allocation / IO", "通常Update中にfile IOとGPU allocationを行わない。", "Editor Asset importの非同期化。"],
    ]
    t = Table([[p(c, "card") for c in row] for row in checks], colWidths=[35 * mm, 112 * mm, PW - 2 * MARGIN - 147 * mm])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), NAVY), ("TEXTCOLOR", (0, 0), (-1, 0), WHITE),
        ("GRID", (0, 0), (-1, -1), 0.5, colors.HexColor("#CAD2D8")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 7),
        ("RIGHTPADDING", (0, 0), (-1, -1), 7), ("TOPPADDING", (0, 0), (-1, -1), 6),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
    ]))
    story.append(t)
    story.append(PageBreak())

    story += section_title("08", "検証状況と残課題", "完成済みと未検証を明示し、採用資料で実装範囲を過大に見せないようにしています。")
    story.append(cards_row([
        ("確認済み", "Debug x64 build、起動smoke test、JSON schema、OOB／高低差通水のfocused test、HUD画像の表示確認。", GREEN_LIGHT),
        ("今回再確認するもの", "Release x64、Release exe起動、Git由来ソースとResources、最終ZIP CRC、紹介動画再生。", BLUE_LIGHT),
        ("未実装", "水量保存、流向、土壌浸潤、蒸発、作物サイズへの最終影響、30日制コンテスト。", GOLD_LIGHT),
    ]))
    story.append(Spacer(1, 6 * mm))
    story.append(p("次の実装順", "h2"))
    story.append(flow_row(["隣接土壌へ浸潤", "直接水やりと役割分担", "成長・品質へ接続", "水面と流向表示", "30日制コンテスト"], [BLUE_LIGHT, PAPER, GREEN_LIGHT, BLUE_LIGHT, GOLD_LIGHT]))
    story.append(Spacer(1, 6 * mm))
    story.append(p("リポジトリ", "h2"))
    story.append(p("https://github.com/YoshinoGento/CG2_00_01", "body"))
    story.append(p("本資料は個人就職作品ブランチのGitコミット a1d925e と、2026年8月28日に撮影した実行動画を基準に作成しています。", "small"))

    OUT.mkdir(parents=True, exist_ok=True)
    doc = SimpleDocTemplate(str(PROGRAM), pagesize=PAGE, leftMargin=MARGIN, rightMargin=MARGIN, topMargin=12 * mm, bottomMargin=12 * mm, title="DirectX12 3D農業サンドボックスゲーム プログラム説明資料", author="ヨシノ ゲント")
    doc.build(story, onFirstPage=footer, onLaterPages=footer)


def main() -> None:
    global S
    register_fonts()
    S = styles()
    build_portfolio()
    build_program()
    print(PORTFOLIO)
    print(PROGRAM)


if __name__ == "__main__":
    main()

from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
labels = json.loads(r'''[["Menu","農場メニュー"],["Farm","農作業・売買"],["Terrain","地形・用水路"],["Records","セーブ・ロード"],["Observe","観察ノート"],["Settings","時間・カメラ"],["Resume","農場に戻る"],["Hoe","クワを選ぶ"],["Water","じょうろを選ぶ"],["Seed","種を選ぶ"],["Harvest","収穫道具を選ぶ"],["Turnip","カブ"],["Carrot","ニンジン"],["Buy","選んだ種を１個買う"],["Sell","選んだ作物を売る"],["SellAll","収穫物をすべて売る"],["Raise","土地を高くする"],["Lower","土地を低くする"],["Canal","用水路を設置・撤去"],["Source","水源を設置・撤去"],["Path","用水路をつなげる"],["RemovePath","用水路をまとめて撤去"],["Confirm","この変更を確定"],["Cancel","キャンセル"],["Undo","元に戻す"],["Redo","やり直す"],["Save","現在の農場を上書き保存"],["SaveCopy","新しい記録として保存"],["Load","選択した記録を開く"],["Previous","前の記録"],["Next","次の記録"],["Restart","農場を最初からやり直す"],["ConfirmDestructive","未保存の変更は失われます"],["Accept","変更を破棄して続行"],["Success","操作しました"],["Failure","現在の状態では実行できません"],["Saved","保存しました"],["Loaded","読み込みました"],["NoSaves","保存された記録はありません"],["PinA","選択マスを観察Ａに登録"],["PinB","選択マスを観察Ｂに登録"],["Start","生育比較を開始"],["Stop","比較を終了"],["Clear","比較記録をリセット"],["Pause","時間を止める"],["Play","時間を進める"],["Speed1","通常速度"],["Speed2","２倍速"],["Speed4","４倍速"],["Follow","プレイヤーを追う"],["Overview","農場を見渡す"],["Inventory","種の所持数 ／ 収穫物の個数 ／ 販売予定額"],["Money","所持金 ／ 選択中の種の価格"],["Tile","選択マス ／ 高さ ／ 水分％ ／ 成長％"],["Quality","成熟度 ／ 水分バランス ／ 地形適性"],["Score","品質点 ／ 収穫時の予想単価"],["Compare","観察Ａ ／ 観察Ｂ ／ 計測時間（秒）"],["WaterStats","接続水路数 ／ 給水対象マス数"],["Record","選択中の記録番号 ／ 保存件数"],["Day","日付 ／ 時間倍率"],["Paused","一時停止中"],["Preview","地形変更プレビュー"],["NoCrop","選択マスに作物はありません"],["Running","生育を計測中"],["NotRunning","計測していません"],["CompareIssue","同じ作物・高さ・成長度の２マスが必要です"],["Dirty","未保存の変更があります"],["Clean","保存済み"],["Ready","農場の記録"],["ClearLocked","目標達成済み"],["PathActive","用水路の配置"],["RemoveActive","用水路の撤去"],["Changed","変更マス数"],["Close","閉じる"],["SaveTime","保存日時"],["Selected","選択中"],["Exit","ゲームを終了"],["PauseCapture","時間・移動は停止中"],["Status","水分％ ／ 成長％"]]''')
labels.append(["LayoutLibrary", "配置だけを保存・再現"])
font = ImageFont.truetype("C:/Windows/Fonts/YuGothM.ttc", 26)
cell_w, cell_h = 640, 44
ascii_y = ((len(labels) + 1) // 2) * cell_h
height = ascii_y + 128
atlas = Image.new("RGBA", (1280, height))
draw = ImageDraw.Draw(atlas)
rects = []
for i, (name, text) in enumerate(labels):
    x, y = (i % 2) * cell_w, (i // 2) * cell_h
    width = int(draw.textlength(text, font=font)) + 4
    assert width <= cell_w, (name, width)
    draw.text((x + 2, y + 4), text, fill="white", font=font, anchor="lt")
    rects.append((x, y, width, 38))
ascii_font = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 24)
for i in range(95):
    x, y = (i % 40) * 32, ascii_y + (i // 40) * 40
    # A shared baseline keeps decimal points and punctuation below the digits.
    draw.text((x, y + 28), chr(32 + i), font=ascii_font, fill="white", anchor="ls")
asset = ROOT / "project/Resources/ui/farm_runtime_menu.png"
asset.parent.mkdir(parents=True, exist_ok=True)
atlas.save(asset)
header = "#pragma once\n#include <array>\nnamespace farmui {\nenum class Label {\n"
header += "".join("    " + name + ",\n" for name, _ in labels) + "};\n"
header += "struct AtlasRect { float x, y, width, height; };\n"
header += f"inline constexpr std::array<AtlasRect, {len(rects)}> kLabels = {{{{\n"
header += "".join("    {" + ", ".join(str(v) + ".0f" for v in r) + "},\n" for r in rects) + "}};\n"
header += f"inline constexpr float kAsciiY = {ascii_y}.0f;\n}}\n"
(ROOT / "project/application/farm/ui/FarmRuntimeLabels.h").write_text(header, encoding="utf-8")
print(asset, atlas.size, len(labels))

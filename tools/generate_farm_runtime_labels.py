from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
labels = json.loads(r'''[["Menu","農場メニュー"],["Farm","農作業・売買"],["Terrain","地形・用水路"],["Records","セーブ・ロード"],["Observe","観察ノート"],["Settings","時間・カメラ"],["Resume","農場に戻る"],["Hoe","クワを選ぶ"],["Water","じょうろを選ぶ"],["Seed","種を選ぶ"],["Harvest","収穫道具を選ぶ"],["Turnip","カブ"],["Carrot","ニンジン"],["Buy","選んだ種を１個買う"],["Sell","選んだ作物を売る"],["SellAll","収穫物をすべて売る"],["Raise","土地を高くする"],["Lower","土地を低くする"],["Canal","用水路を設置・撤去"],["Source","水源を設置・撤去"],["Path","用水路をつなげる"],["RemovePath","用水路をまとめて撤去"],["Confirm","この変更を確定"],["Cancel","キャンセル"],["Undo","元に戻す"],["Redo","やり直す"],["Save","現在の農場を上書き保存"],["SaveCopy","新しい記録として保存"],["Load","選択した記録を開く"],["Previous","前の記録"],["Next","次の記録"],["Restart","農場を最初からやり直す"],["ConfirmDestructive","未保存の変更は失われます"],["Accept","変更を破棄して続行"],["Success","操作しました"],["Failure","現在の状態では実行できません"],["Saved","保存しました"],["Loaded","読み込みました"],["NoSaves","保存された記録はありません"],["PinA","選択マスを観察Ａに登録"],["PinB","選択マスを観察Ｂに登録"],["Start","生育比較を開始"],["Stop","比較を終了"],["Clear","比較記録をリセット"],["Pause","時間を止める"],["Play","時間を進める"],["Speed1","通常速度"],["Speed2","２倍速"],["Speed4","４倍速"],["Follow","プレイヤーを追う"],["Overview","農場を見渡す"],["Inventory","種の所持数 ／ 収穫物の個数 ／ 販売予定額"],["Money","所持金 ／ 選択中の種の価格"],["Tile","選択マス ／ 高さ ／ 水分％ ／ 成長％"],["Quality","成熟度 ／ 水分バランス ／ 地形適性"],["Score","品質点 ／ 収穫時の予想単価"],["Compare","観察Ａ ／ 観察Ｂ ／ 計測時間（秒）"],["WaterStats","接続水路数 ／ 給水対象マス数"],["Record","選択中の記録番号 ／ 保存件数"],["Day","日付 ／ 時間倍率"],["Paused","一時停止中"],["Preview","地形変更プレビュー"],["NoCrop","選択マスに作物はありません"],["Running","生育を計測中"],["NotRunning","計測していません"],["CompareIssue","同じ作物・高さ・成長度の２マスが必要です"],["Dirty","未保存の変更があります"],["Clean","保存済み"],["Ready","農場の記録"],["ClearLocked","目標達成済み"],["PathActive","用水路の配置"],["RemoveActive","用水路の撤去"],["Changed","変更マス数"],["Close","閉じる"],["SaveTime","保存日時"],["Selected","選択中"],["Exit","ゲームを終了"],["PauseCapture","時間・移動は停止中"],["Status","水分％ ／ 成長％"]]''')
labels.append(["LayoutLibrary", "配置だけを保存・再現"])
labels.extend([
    ["NextTool", "次の道具へ"], ["SwitchHoe", "クワに持ち替え"],
    ["SwitchWater", "じょうろに変更"], ["SwitchSeed", "種に持ち替え"],
    ["SwitchHarvest", "収穫道具に変更"], ["BuyAndReturn", "１個買って畑へ"],
])
labels.extend([
    ["ApplyHoe", "このマスを耕す"], ["ApplyWater", "水やりする"],
    ["ApplySeed", "種を植える"], ["ApplyHarvest", "収穫する"],
    ["OpenSeedShop", "種の購入へ"], ["ActionReady", "実行できます"],
    ["ActionNoTile", "マスが未選択"], ["ActionWaterFull", "水分は満タン"],
    ["ActionNoSeed", "種がありません"], ["ActionGrowing", "まだ成長中です"],
    ["ActionOtherTool", "別の道具を選択"],
])
labels.extend([
    ["TrialStart", "農場体験を始める"], ["TrialComplete", "目標金額を達成しました"],
    ["TrialGoal", "今回の目標：作物を売って所持金を増やす"],
    ["TrialMoney", "所持金 ／ 目標金額"], ["TrialDay", "農場の日付"],
    ["TrialGrow", "種を買い、耕した畑で作物を育てる"],
    ["TrialWater", "水不足と水過多を避け、生育を整える"],
    ["TrialSell", "育った作物を収穫し、売って目標へ"],
    ["TrialBegin", "この農場でプレイする"], ["TrialReview", "達成した農場を見る"],
    ["TrialResultNote", "収穫・販売の目標達成"],
    ["TrialRemaining", "未販売の作物 ／ 販売予定額"],
    ["TrialGoalOnly", "体験版の金額目標（コンテストとは別）"],
])
labels.extend([
    ["ObserveField", "畑を見ながら観察"], ["ObserveExit", "農作業に戻る"],
    ["PickA", "Ａを選び直す"], ["PickB", "Ｂを選び直す"],
    ["JumpA", "Ａのマスを選択"], ["JumpB", "Ｂのマスを選択"],
    ["PickingA", "観察Ａのマスを選択中"], ["PickingB", "観察Ｂのマスを選択中"],
    ["ObservationA", "観察Ａ"], ["ObservationB", "観察Ｂ"],
    ["Moisture", "水分"], ["Growth", "成長"], ["ReadySeconds", "収穫秒"],
    ["Elapsed", "計測時間"], ["ObservationEmpty", "未登録"],
    ["ObservationIdle", "比較の準備"], ["ObservationStopped", "計測終了・結果を保持"],
    ["ObservationCompleted", "両方が収穫可能・計測完了"],
    ["ObservationInvalid", "畑の変更により計測無効"],
    ["IssuePickTwo", "観察ＡとＢを登録してください"],
    ["IssueSame", "ＡとＢに別のマスを選んでください"],
    ["IssueNotGrowing", "両方に成長途中の作物が必要です"],
    ["IssueCrop", "同じ種類の作物を選んでください"],
    ["IssueHeight", "同じ高さのマスを選んでください"],
    ["IssueGrowth", "開始時の成長度が揃っていません"],
    ["IssueData", "畑の値が無効です・再登録してください"],
    ["CompareReady", "比較を開始できます"],
    ["KeepResults", "表示は計測終了時の値です"],
])
font = ImageFont.truetype("C:/Windows/Fonts/YuGothM.ttc", 26)
labels.extend([
    ["SoilCare", "土づくり・養分を確認"], ["Compost", "堆肥を混ぜる（無料）"],
    ["SoilNutrients", "土の養分（０～１００）"], ["NutrientTarget", "この作物の養分目安"],
    ["NutrientUse", "成長０～１００％で使う養分"], ["NutrientQuality", "育成中の養分充足度"],
    ["SoilHelp", "不足すると品質が低下。植える前に堆肥で準備"],
    ["SoilGrowing", "生育中：収穫後に堆肥を混ぜられます"],
    ["SoilFull", "養分は十分です。種を植えられます"],
    ["SoilTill", "まず土のマスを選び、クワで耕してください"],
    ["SoilReady", "堆肥を混ぜて養分を１００にできます"],
    ["SoilApplied", "堆肥を混ぜました。養分１００"],
    ["SoilSelectedCrop", "選択マスの作物（未植付けは選択中の種）"],
])
cell_w, cell_h = 640, 44
labels.extend([
    ["QualityOpen", "作物の品質を見る"],
    ["QualityCurrent", "選択マスの品質予測"], ["QualityHarvest", "直前の収穫結果"],
    ["QualityNoHarvest", "まだ収穫記録がありません"],
    ["QualityMaturity", "１　成熟度"], ["QualityWater", "２　水分バランス"],
    ["QualityTerrain", "３　地形適性"], ["QualityNutrient", "４　養分充足度"],
    ["QualityScale", "外周１００・１目盛２５"],
    ["QualityEstimate", "収穫前の予測です。生育状況で変化します"],
    ["QualityRecorded", "収穫時の記録です。不明な値は -- 表示"],
    ["QualityFinalPrice", "収穫時の品質点 ／ 確定単価"],
])
labels.extend([
    ["HintMaturity", "成熟度：成長１００％まで待って収穫"],
    ["HintMaturityNext", "次回：成長１００％まで育てて収穫"],
    ["HintWater", "水分：育成中の水不足と水過多を避ける"],
    ["HintTerrain", "地形：次回は植付け前に作物向けの高さへ"],
    ["HintNutrients", "養分：次回は植付け前に堆肥を混ぜる"],
    ["HintBalanced", "記録された各項目は良好です"],
    ["HintUnknown", "不明な項目あり。評価は参考値です"],
    ["HintPartialRecord", "収穫記録：一部の項目は不明です"],
    ["HintPartialEstimate", "品質予測：一部の項目は不明です"],
])
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

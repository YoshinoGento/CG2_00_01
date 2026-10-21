# 実物に合わせた農作業アイコン

## 対象

- ニンジン：既存の根と葉のOBJを、収穫して土から出た状態で画像化する。
- トマト・カボチャ：既存の収穫演出と同じく、実のOBJだけを画像化する。
- 種：種屋で表示する種袋画像と同じ面を使う。野菜そのものと袋を区別する。
- クワ・じょうろ・汎用の種袋・収穫かご・操作記号：既存の専用アイコンを維持する。確認した農具には対応する実物モデルがないため、モデル由来とは説明しない。

## 生成と責務

`tools/ui/farm_item_icons.py` は制作時だけ使う。ゲームの描画処理ではない。
既存OBJを読み、成熟時の `FarmMeshLayout.h` の寸法・色で配置し、固定の正射影・照明・Depth判定で64pxの透過画像にする。色はlinearからsRGBへ変換する。
ニンジンのみ、根と葉の輪郭を見せるため傾ける。種袋は店頭アトラスの同じUV領域から縮小する。

`generate_farm_hud_icons.py` が従来どおり32枠の512×256アトラスにまとめる。
ゲーム側は既存SpriteとUVを使用し、常時3Dプレビュー・新しいRenderTarget・Barrier・Descriptor・DrawCallは追加しない。選択、個数、ツールチップ、Systemへの入力通知も変更しない。

生成コマンド（Python + Pillow + NumPy）：

```powershell
python tools/ui/generate_farm_hud_icons.py --preview
python tools/ui/generate_farm_hud_icons.py --check
python tools/tests/farm_item_icons_test.py
python tools/tests/farm_hud_icons_test.py
```

`.sources.json` は生成元と出力のSHA256を記録する。元モデル・種袋・生成処理を変更したら再生成する。
寸法・色はゲームコードの値を転記しているため、`FarmMeshLayout.h` を変更した場合は転記値も確認する。ハッシュは変更を検出するだけで、自動的な同期を保証しない。

## 限界

固定照明によるサムネイルであり、DirectX12の実画面を撮ったものではない。トマトが複数の実、カボチャが茎なしなのは現行の収穫モデルに合わせた結果。モデル自体の改良は別の作業とする。
小さい袋の文字を読ませる設計ではない。袋の色と絵で選び、必要な名前は既存の選択・hover表示で補う。
アトラスの寸法と枠数は維持するが、PNG容量と実際のVRAM使用量は別。今回GPU性能の改善値は主張しない。

# 水道農業 実行版04 確認結果

## 最新の実行版
- `generated/kurihaku_20260928/SuidoNogyo_FreeFarming_20260928_04.zip`
- 22,392,702 bytes / SHA-256: `dcd01a745ca9e7f140d5b5d661950f946e4881ea93ee6339be691239afe66042`
- 実行ファイル一式のみ。応募用PDF・動画・自己PR・ソース一式を含む最終提出ZIPではない。
- 外部送信、Gitのstage・commit・push・mergeはしていない。

## 今回の変更
- 学校の基盤コードは利用可能とのユーザー確認を記録。学校配布モデルは旧候補03と同様に除外。学校の書面を独立確認したという意味ではない。
- Assimpは公式v5.3.0ソースZIPからVS18/v145、Release/static CRTでビルド。OBJ/glTF importのみ、exportなし。旧ライブラリのビルド元不明を解消。
- DXCは公式v1.8.2502配布ZIPのx64 DLLへ置換。ReleaseNotesのライセンス対応表と同梱ライセンスを保持。
- 原本の第三者ライブラリ・ソース・開発セーブ・過去ZIPは変更しない。提出専用コピーのinclude/lib/プロジェクト参照だけ変更。
- CREDITS・取得URL・ダウンロードとバイナリのハッシュを同梱。学校モデルを含まない既存の素材許可リストを引き継ぐ。

## 確認済み
- 最終Release/x64ビルド：警告0、エラー0（1分08.76秒）。
- Assimp：公式5.3.0/flags16、提出対象OBJ14ファイル・14メッシュを読み込み。null、頂点・法線・UV欠落、非有限値、非三角形、範囲外index検査が合格。
- EXEの直接依存はWindows系DLLと同梱DXC。DXCはWindows系DLLとUCRT API sets。Debug CRT・Assimp DLL・Visual Studio専用DLLへの直接依存なし。
- ZIP CRC、ASCIIパス、342ファイルのハッシュ照合、余分なZIPエントリーなし。
- 配布セーブ：schema17/FreeFarming、1日目、300G、ニンジン・トマト・かぼちゃの種各5個、未耕作20マス、初期保存名1件。
- 別フォルダーへ展開したコピーで、異なるカレントディレクトリーからStartGame.cmdで起動。
- 1280×720の実画面で、フリー開始・畑描画・耕す・種屋・購入・保存名表示を確認。
- 購入で300→240G、ニンジン5→6個。保存後に正常終了、旧プロセス0件を確認して再起動。240G/種6個/畑#0耕し済みが画面と保存JSONで復元された。
- 検証ゲーム終了済み。検証セーブは配布ZIPに戻していない。元のSettingsと開発用ライブラリはハッシュ不変。
- branch `個人就職作品`、HEAD `f68a0b5f1175b83763aa0fec8e126a324c3b53ce`、stage空。

## 未確認・制限
- 別PCなし。別GPU、別DPI、開発環境のないPCでの起動は未確認。手順はSECOND_PC_CHECKLIST.md。
- この依存ライブラリ版で収穫から大会・30日までの全操作を再走査してはいない。
- Assimp上流のCMake定義とCMake4の互換警告は残る。ソース改変・警告全体の無効化はしていない。UTF-8/例外処理/static CRTのフラグを明示してビルド。
- 初回は展開対象のcmake-modules、続いて公開ヘッダー依存utf8cppの不足で失敗したが補完済み。原本ゲームコードの変更で回避していない。
- DX12 Barrier/Fence/Descriptor/VRAM管理や長時間性能の包括監査ではない。既存の初期化失敗時安全性や仮キャラクター表現は別課題。
- 素材・ライブラリの出典追跡と本文同梱の確認であり、包括的な法的保証ではない。

## 再現用ファイル
`prepare_known_dependencies.py`、`build_known_assimp.py`、`verify_official_assimp.cpp/.cmd`、`build_provenance_runtime.py`、`package_provenance_runtime.py`、`verify_provenance_final.py`。
ビルド出力はgenerated/kurihaku_20260928/known_dependenciesとprovenance_buildに保存。
ダウンロードZIPと生成済み提出物は黙って上書きしない。

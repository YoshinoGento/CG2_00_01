# 提出用素材の整理（2026-09-28）

## 対象と許可
- ユーザーは学校配布の人物・画像・サンプル素材の除外を許可。エンジンコードの削除は求めていない。
- 元のResources、開発用Settings、以前のZIPは保管。新しいRelease配布フォルダーは許可リストで構成。
- エンジン利用許可を得たことにはしていない。企業提出可否は学校の条件を確認する必要がある。

## 実装
- ReleaseのGamePlayScene初期化とプレイヤー表示から学校配布アニメーションへの依存を除去。Debugは既存アニメーションの検証を継続可能。
- 農業レベルのプレイヤーは生成した144三角形の静止マーカー、地面は2三角形の平面へ変更。最終キャラクターではない。
- 白画像・粒子画像・ノイズ画像を生成スクリプトに集約。共有白画像参照と初期化時の画像・モデル参照を変更。
- テクスチャ配列縮小に伴う旧index1参照を、既存の白TextureHandleに変更してOOBを防止。
- Noto Sans JPでメニューとASCIIアトラスを再生成。Weight100では細すぎたため500を明示。
- 新しいSystemやUIロジックを追加していない。農業ルール・状態遷移・schema17・元のセーブは不変。
- Descriptor/Barrier/Fence/所有権の方式を変更していない。学校のスキンモデルを読み込まないため、その分のロードは減るが、VRAM/CPU/GPU負荷の定量計測は未実施。

## 検証経過
- 初回AssetClean候補はUV欠落の新規OBJをModel::LoadModelFileへ渡し、起動に失敗。提出不可。
- 原因は既存ローダーのHasTextureCoords前提。生成側にUVを追加し、有限値・法線・三角形・UV・index・windingテストを追加。
- この既存ローダーは不正入力をassertだけで防ぐ箇所があり、Releaseの安全性改善は別途残る。今回、エンジン全体を安全と認定していない。
- 02で実起動・フリーモード開始・畑描画を確認。その後、細すぎた文字を調整した03を最新候補とする。
- 今回作成した初回・02のZIPには誤提出防止の`DO_NOT_SUBMIT_`接頭辞を付けた。過去の別作業のZIPは変更していない。
- 生成物の差し替えは許可したパスのみ、前後ハッシュを記録。03の生成ヘッダー変更後は両構成を再ビルド。
- 自動テスト：runtime-view（162カメラ姿勢、クリック範囲、無効操作、atlas）、FreeFarming（境界、モード、永続化）、新規素材検査が合格。
- 最終Release/Debug x64は警告0・エラー0（生成ヘッダー更新後の増分ビルド20.49秒／17.65秒）。
- 最新ZIP：`generated/kurihaku_20260928/SuidoNogyo_FreeFarming_20260928_AssetClean_03.zip`、20,976,087 bytes、SHA256 `d8b15472f2c7c489dd9ffad5f9776492b703401bc971d5628777019b6022c7b8`。
- 333ファイルのハッシュ・ZIP CRC・ASCII内部パスを確認。許可リストにより57リソースを除外。Day1/300G/種各5個/schema17/FreeFarmingの初期記録1件のみ。
- 実起動：別cwdから同梱ランチャーで開始、畑#0を耕す、ニンジンの種購入300->240G/5->6個、保存名・1/1件・保存済み表示を確認。終了・再起動後に同じ耕作状態と240G/6個へ復元。
- 追従カメラで青い静止マーカーが見え、学校の白い人物は表示されない。通常1280x720と最大化1920x1008のHUD、通常サイズのメニュー・種屋・保存画面で文字見切れなし。全画面や全メニューを網羅した検証ではない。
- 操作は検証用の展開コピーのみ。配布ZIPと配布フォルダーは未プレイ状態を維持。検証ゲームは終了。
- 起動ログの近ゼロScale拒否、未対応文字の警告は残る。ローダー安全性・全GPU負荷・別PC・全機能の通しプレイを合格扱いにしていない。
- 個人就職作品/HEAD f68a0b5f1175b83763aa0fec8e126a324c3b53ceと空のstageを維持。commit/push/merge/uploadなし。

## ライセンスの確認範囲
- DirectXTex、nlohmann JSON3.11.3、Assimp5.3.0とcontrib、DXC1.8.2502、Noto Sans JP、ローカルImGuiの本文を収集。取得元とSHA256をLicenses/SOURCES.jsonに保存。
- Assimp実ライブラリのAPIは5.3.0/revision4528f6dc/mainを返すが、そのrevisionは公開リポジトリで一致せず。ローカルヘッダーの2006-2023表記も別途保存。全ビルド設定の照合は未完了。
- DXC DLLはWindows SDK由来1.8.2502.11。上流1.8.2502ライセンス本文とSDK DLLを完全一致と扱っていない。配布条件の最終照合は未完了。
- フォント本体は既存Notoを保持。OFLに著作権表示を補完し、Windowsフォントの個別字形アトラスを置換。
- 種袋・展示画像はAI生成を含むこと、学校基盤は保持することをCREDITSに明記。

一次資料：
- [Microsoft Font FAQ](https://learn.microsoft.com/en-us/typography/fonts/font-faq)
- [Noto Sans JP OFL](https://github.com/google/fonts/blob/main/ofl/notosansjp/OFL.txt)
- [Assimp 5.3.0](https://github.com/assimp/assimp/blob/v5.3.0/LICENSE)
- [DXC 1.8.2502](https://github.com/microsoft/DirectXShaderCompiler/blob/v1.8.2502/LICENSE.TXT)

## 残る提出条件
1. 学校由来基盤の企業提出条件確認。
2. Assimp実バイナリの出典とSDK版DXC DLLの配布条件の最終確認。
3. 別PCでの起動確認。
4. 正式提出一式のPDF・動画・自己PR・ソースは、この実行版ZIPとは別。

この作業だけをもって「企業へ無条件に提出可能」とは判定しない。

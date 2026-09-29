# 水道農業 フリー農業版の検証

最新のローカル配布候補は `generated/kurihaku_20260928/SuidoNogyo_FreeFarming_20260928.zip`。
旧 `SuidoNogyo_Runtime_20260928_02.zip` は大会版なので、今回はこちらへ置き換える。

- 34,538,105 bytes
- SHA256: `9a80409a409489cdf6f4b62fbae6c8b92733e5d22b95ccb25ed23d7cee174e63`
- 本体: `SuidoNogyo/SuidoNogyo.exe`。入口: `SuidoNogyo/StartGame.cmd`
- 全内部パスASCII、ソース/PDB/LIB/制作途中Blend/開発ログを除外。
- CRC検査、展開353ファイルのSHA256一致。配布資産は新Releaseビルドと同じスナップショット。
- schema17の初期セーブ1件とカタログだけを同梱。Day1/300G/正式3種各5個/FreeFarming。
- Release/Debugビルド成功。DebugのAssimp PDB補完は検証フォルダー限定。
- 実機の開始表示、保存名1/1、保存・終了・再起動・復元、モード切替、やり直しを確認。
- 別のQAコピーでDay32/1000Gでも耕作可能、2倍速でDay35まで継続。合成検証用データはZIPに含めない。

実装と確認範囲は `docs/testing/FarmFreeFarming_20260928.md` を参照。
元の開発セーブ、元の古いZIP、Gitのブランチ/HEAD/ステージは変更しない。提出・アップロードは未実施。

素材利用条件の監査と別PC試験は未完了。実行版候補の完成と、企業へ送信可能な全提出物の完成は区別する。PDF・動画・自己PR・ソース一式・公式指定の外側ZIP名は別作業。

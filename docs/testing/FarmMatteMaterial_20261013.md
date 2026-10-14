# 畑の白い帯の修正

## 原因と範囲
水路の岸にも既定のBlinn-Phong Specularが設定されていた。
Object3d.PS.hlslはMaterial色とは別に白いSpecularを加算するため、光源・岸の法線・Cameraの方向が揃うと岸が白く飽和して見えた。
前回の種屋遷移変更前にも同じ帯が再現した。水面の形状や画面遷移のマスクではない。

FarmRenderer::Prepareで毎回SpecularType::Noneを設定し、プール再利用時にもマットな材質を維持する。
水面・選択枠のUnlit設定と、土・岸のDiffuse/Shadowは残す。TitleFarmRendererや共有Object3d Shaderは変更していない。
Scene/Game/UI・農場の状態・セーブ形式の変更はない。

## 確認
- Debug/Release x64ビルド成功。
- farm_material_policy_test.py: 材質設定・既存Diffuse/Shadow経路のソース契約を確認。GPUテストではない。
- 独立FarmMatteQA、以前の種屋検証と同じSettings/Camera、1280x720で帯が消えたことを確認。
- 証拠: generated/codex_checks/farm_matte_20261013/after.png。以前の記録: shop_short_transition_20261013/shop01.mp4。
- SceneTransition100回反復、種屋・展示・Runtime View・FreeFarming・音声素材・波紋参照テスト成功。
- 提出用の独立Release x64ビルド成功。ZIPのCRC・全メンバーのハッシュ・原本Settingsの全ファイル不変を確認。
- 日本語を含むパスへ展開した別コピーで起動・購入・保存・正常終了・再起動を確認。保存した210G・トマト種6個が復元された。
- 画面遷移提出動画は40秒、1280x720/30fps/H264/AAC。実ゲーム音声を録音し、待ち時間だけ編集。全デコードとコマ送り確認成功。

## 安全性と限界
Object3d所有の既存Material CB整数だけを変更。追加のTexture/Descriptor/Barrier/Fence/DrawCall/動的確保はない。
土と岸の反射表現も省略する。将来の濡れた材質は、適切なSpecular強度・Roughness設計と計測を伴う別タスクにする。
実機比較はこのPCのRelease通常ウィンドウだけ。別GPU/DPI、GPU時間、エンジン全体のDX12安全性は未検証。

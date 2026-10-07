# タイトルの環境音

## 目的と素材
「水路農業」の農村を水・鳥・虫の音で表現する。タイトル限定で、元の録音・セーブ・提出済みZIPは変更しない。
朝は「スズメが鳴く朝」、昼は「ミンミンゼミが鳴く雑木林」、夕は「夏の山2 / ヒグラシ」、夜は「夏の田舎の夜」。4素材とも効果音ラボ。
常時、ユーザー指定のOn-Jin ～音人～の川の水音を重ねる。夏寄りの演出で、四季の再現ではない。
出典・利用条件・加工内容は `project/Resources/title/audio/AUDIO_CREDITS.txt` に記載し、提出するゲームにも同梱する。
効果音ラボはゲーム組み込み・形式変換を許可するが素材単体の再配布は禁止。On-Jinの露出素材には配布元と二次配布・無断利用禁止を表記する。
素材・音だけの紹介動画は作らない。

## 責務と処理順
1. オフラインの `prepare_title_ambience.py` がPCM16 WAVを生成する。
2. `TitlePresentationSystem` が既存の40秒の時刻と入退場fadeから `AudioMix` を作る。
   `TitleAudioSettingsSystem` がプレイヤー指定の環境音・水音の音量を適用する。
3. `TitleAudioSystem::Initialize` がFramework所有のAudioを借り、専用Clip/Loop Voiceを5個確保する。
4. `TitleAudioSystem::Update` はMixを検査し、音量・Pause/Resumeだけを更新する。
5. 開始時は画面と一緒に0.8秒で消音し、Scene切り替えでVoice停止後にPCMを解放する。

Sceneは呼び出し順の統括のみ、View/UIは音声制御を持たない。
FrameworkのAudioはSceneより長く生存し、同じ所有スレッドから呼ぶ。
音源パスはタイトル専用。既存AudioのCacheには参照カウントがないため、他Systemで共有するとUnloadが干渉するので共有しない。

## 時計と音量
朝昼夕夜は各10秒、最後2.5秒で次の音へSmoothstepでCrossfadeする。夜から朝も同じ。描画と同じ時計で、別時計によるずれを作らない。
環境音Gain合計0.45以下、水音0.30以下。Unity-sum Crossfadeで重なってもGainを増やさない。
音設定の初期値は両方40%。初期状態の実Gain上限は環境音合計0.18、水音0.12。100%は従来の最大音量、0%は消音。
Mixは有限値・範囲・合計Gainを検査する。Start点滅は変更しない。
無音VoiceはPauseし、次の時間帯に再生位置からResumeする。毎回先頭に戻らない。

## ループ加工と負荷
最大32秒を抜粋し、前後400msを重みの和1で重ねる。短い夜素材は取得できた範囲から作る。
RMS目標0.12、Peak上限0.85で音量差を調整する。sample rate/channelsを維持、元MP3は変更しない。
出力は31.6秒、夜のみ約24.53秒。Source/Output SHA256と加工設定をmanifestに残す。
短い抜粋はメモリ節約と引き換えに反復が認識されやすい。必要なら区間を延ばす。
初期化時に5Clip/5Voiceを確保し、更新中にDecode/Play/ファイル読込/動的確保しない。通常の再生は環境音2個以下と水音。
既存AudioはForward/Reverse両PCMを持つ。実エンジンのStatisticsでこの素材群のPCM保持量48,658,648bytesを確認した。Decoder一時領域やXAudio2内部メモリは含まない。
部分失敗では確保済みVoice/Clipを解放し、無音でStartを継続する。StopAllVoices/GlobalTemporalStateは変更しない。
DX12のDrawCall/Descriptor/Barrier/Fence/VRAMに変更なし。音声の負荷と描画の負荷は別に検証する。
後から追加した音設定UIには固定Sprite26個と640x320の文字Atlasがある。閉じている間は2Draw、開いている間は最大26Drawを追加する。既存Sprite描画・TextureManager・PostDraw同期を使い、独自PSO/Descriptor管理は追加しない。実VRAM・GPU時間は未計測。

## 音設定メニュー
タイトル文字の最新指定に合わせ、音設定のAtlasもNoto Sans JPの4倍描画/LANCZOS縮小による滑らかな字形へ変更。640x320、行・数字のSource Rect、Sprite数と入力領域は維持する。ドット用二値化/Nearest2xは使わない。
タイトル右上の「音設定」またはEsc/Gamepad Startで開閉する。スライダーのクリック・ドラッグで即時反映、キーボード左右/D-padで5%ずつ変更し、上下/Tabで対象を切り替える。Enter/Space/A/Bで戻る。設定が開いている間はStartを通知しない。
`TitleAudioSettingsView` は固定Spriteの表示と入力値の通知のみ。`TitleAudioSettingsSystem` が開閉・選択・Drag capture・音量・保存状態を持つ。Sceneは入力通知、既存Presentationの開始、音量適用、Audio更新を順に呼ぶ。EditorShellはImGuiのゲーム画像内座標とfocusを渡すだけで、音量に触れない。UIの表示座標とHit Rectは`TitleAudioSettingsLayout.h`で共通化。
`Settings/runtime/title_audio.json`に整数Percentを保存し、デバッグのエディター設定・農場セーブと分離する。閉じる時とScene Finalize時のみ保存し、ドラッグ中はDisk I/Oをしない。既存JsonFileを使用し、一時ファイルの書き込み後にWindowsのMoveFileExで置換する。破損・型違い・未知Versionは初期値、範囲外は0..100にClamp。書き込み失敗でもそのセッションの音量は維持し、再度閉じる時に再試行する。強制終了では未保存の変更が失われる場合がある。Gameplayの一般音量設定ではなく、現時点ではタイトルの環境音専用。

## 検証方針
素材Hash、PCM形式・長さ・Peak/RMS・Wrap差分・Project XMLを自動検査する。
Mixは600秒のGain合計、最大2音、境界連続性、NaN/Inf/負値、入退場fadeをテストする。
別Harnessで実XAudio2/Media Foundationの不足素材・部分失敗・再入場・PCM解放・他Voice維持を検証する。
2026-10-05: 素材検査、600秒のMix/既存タイトル回帰、実XAudio2/Media Foundationの3回の再入場・部分失敗・他Voice保持・無効Voice時の停止テストはPASS。最初のHarnessはCOM初期化不足で失敗し、ゲームと同じCOM初期化前提に修正した。
最終Debug/Release x64ビルド成功。ロゴと雲のCPU回帰検査もPASS。
独立Release runtime02のPID2552で、対象ゲームのAudio Sessionだけの最大Peak0.245856を確認した。録音は行っていない。
再起動PID21876では開始ボタンクリックから本編の導入画面へ移り、タイトル解放ログと5秒間の最大Peak0を確認した。
自動Return入力では遷移が観測できなかったが、クリックでは遷移した。Enter自体の不具合か、短い合成入力の取りこぼしかは未特定で、入力処理は変更していない。
別PC/聴感での継ぎ目・時間帯の識別しやすさ・長時間の聴き疲れは未検証。数値検査だけで音質を保証しない。

音設定追加後: C++設定テストと文字Atlas/Project XML検査、既存600秒タイトル回帰、素材検査がPASS。最終Debug/Releaseビルド成功。独立Releaseの1280x720で両スライダーのDrag、初期値40%への復帰、保存JSON、再起動時の環境音0%/水音40%復元を確認した。PID1988で両方0%の音声Peak0、設定を閉じた後のStartクリックによる本編導入への遷移・タイトル音解放後のPeak0を確認した。録音はしていない。QA実行ファイルは最終ReleaseとHash一致。
実Gamepad、キーボードの機器入力、Debugドック内の実操作、他DPI/Window sizeは未検証。自動テストは入力通知と値の変化を確認するもので、実機検証の代わりにはしない。

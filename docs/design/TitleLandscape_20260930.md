# タイトル周辺の田舎風景

## 目的
ユーザーの農道写真を構成の参考に、緑一色だった周囲へ土の道、草の縁、遠景の疎らな木を加える。
中央の展示用の畑と用水路、空、ロゴ、開始操作は維持する。写真自体・人物・車は使用しない。
ドット寄りの表面を持つ3D背景の試作であり、完成したHD2Dアートとは扱わない。

## 責務と構成
- tools/title/generate_title_landscape.py: 固定seedから静的な地形・農道・木のOBJと検証metadataを生成する。プレイ中には実行しない。
- TitleFarmRenderer: Managerから借りたModel/Textureと、保持するObject3d一個で背景を描く。毎フレームは既存カメラに対する行列更新だけ。
- TitlePresentationSystem: 展示畑・昼夜時計・色と地面の露出を管理する。TitleScene/TitleView/Gameの振る舞いは景観修正では変更しない。保存・経済・成長・日付・操作UIへ背景を接続しない。

農道は二本の土の走行部分と中央の草、道端の草・低い木杭で構成する。
ユーザーの指摘により田んぼ・隣の畑・稲は撤去した。
農道幅は1.9 world units。中央畑(5x4、pitch1.30、中心x0/z6)に約1unitの余白を加えた
x[-4.25,4.25]/z[2.4,9.6]に道・木を置かない。旧頂点だけの検証では面の横断を検出できず不十分だった。
現在はXZ平面の三角形と余白込み矩形をSATで照合する。
8本の木と4組の低木を固定した離れた位置へ置き、樹冠の大きさだけseedでばらつかせる。
木同士の中心間隔は6units超。葉は静的。農道と中央畑は平地、遠景だけ最大約0.73unitの起伏を加える。

## 描画・寿命
元の緑の地面Objectを削除し、草地を含むcountryside.objで置き換える。
376個の既存畑パーツ + 景観一個 + 8本の反射線となるため、元の地面一個を含む構成と
主描画Object数は同じ。追加頂点・Texture負荷はあるため、性能改善とは主張しない。
景観も既存Shadow passとObject passへ登録し、朝昼夕夜のDirectional lightを受ける。
景観用PSO、Root Signature、Descriptor allocator、Barrier/Fenceの経路は追加しない。
Model/TextureはManager所有、景観ObjectはTitleFarmRenderer所有でFinalize時に解放する。

## メッシュとTexture
- countryside.obj: 3,620 triangles、10,860 exported vertices。前案の10,504trianglesから削減したが性能は未計測。
- countryside_atlas.png: 1536x1024、3列x2行。草・土・葉 / 稲・木・水の六素材。
- RGBA8の元レベルは6MiB、mip込みでは約8MiB相当。実際のGPU allocation/VRAMは未計測。
- 全画素は不透明。透明な四角板のDepth write/ソート問題を避け、形は実際の三角形で作る。
- atlas各セルの内側2.5%からUVを取る。遠景の小さいmipでのセル混色は今後の確認対象。
- loaderのX反転・Assimp winding/UV反転に対応する形式で出力する。
- 草地と道は高さを少しずらし、同一平面を重ねない。杭の根元は地中に埋める。中央畑の配置領域と余白を空ける。

## 素材の由来
専用の六素材atlasはOpenAI built-in imagegenで新規生成した。ユーザーの手描きとは申告しない。
生成時に写真は渡しておらず、写真から読み取った環境構成だけを参考にした。
生成ID: exec-a660a6f5-a00d-47c7-939c-ad8c487d239c。
元の生成結果を残し、プロジェクトのResources/titleへコピーした。
メッシュは上記generatorによる独自の固定形状。第三者の学校配布モデルは追加しない。

### 生成プロンプト
Use case: stylized-concept. Asset type: one original opaque material texture atlas for an HD-2D Japanese countryside farming title scene, 1536x1024 landscape, precisely THREE equal columns and TWO equal rows, six 512x512 square material cells touching without frames or gutters. Every cell is flat seamless surface texture, orthographic straight-on/top-down, no perspective and no 3D objects. Top row left to right: (1) mixed short meadow grass with small irregular moss-green/sage-green pixel clusters and occasional muted straw, no flowers; (2) pale gray-brown compacted farm-road earth with scattered tiny gravel and wheel-worn compact soil, NO drawn road or tracks, no grass; (3) dense broadleaf tree foliage surface, layered dark forest green, olive and sage small leaf clusters, not an isolated tree. Bottom row left to right: (4) upright young rice field seen from above, small regularly spaced green seedling clumps over muted wet earth, seamless repeating; (5) weathered gray tan timber bark grain, flat; (6) quiet shallow irrigation/paddy water texture, muted aqua gray green with subtle sparse horizontal ripple highlights, no shore. Style: natural painterly pixel art with clearly visible tidy pixel clusters like 128x128 art per cell scaled up with crisp pixels, a little realistic surface detail but NOT a photo, not noisy, no black outlines, no lighting gradients, no shadows, no shiny effects. All six cells fill their entire region. Soft neutral diffuse daylight. No sky, people, faces, vehicles, signs, logos, letters, numbers, borders, UI, or transparent regions. This is an ORIGINAL six-material game texture atlas, not a scene or contact sheet of objects. Exact row/column layout is critical for mesh UV mapping.

## 検証
CPU検証: 田んぼ・隣の畑の不在、木の間隔、再生成の一致、triangle上限、finite頂点/法線、単位法線、非退化面、
UVのセル境界余白、index範囲、loader反転後の座標、余白込み畑と景観面の非交差、不透明atlasを確認。
昼夜・風・天体・開始状態の既存CPUテストも実行する。
Debug/Releaseのビルドと、元のセーブを使わないReleaseでの表示確認を分けて記録する。
修正後もDebug/Release x64 Build成功。分離Releaseの44.966秒/1280x720録画で
畑から離れた農道、疎らな木、太陽と反対側へ伸びる昼の影、夜の投影影の消去を確認した。
朝夕の光量逆転も別途修正し、最新録画で昼の明るさ・夜の暗さと滑らかな推移を確認。
開始クリックでゲーム導入へ遷移し、最後の検証用ゲームPID36552は通常終了/exit0を確認。
最新録画はgenerated/codex_checks/title_realism_20260930/verification03.mp4、昼夜画像は同所daytime03.png/night03.png。
別GPU/DPI、Debug docked view、VRAM/FPSの計測は未実施。
提出済みZIPは更新しない。今回の背景は開発中のタイトルだけに適用する。


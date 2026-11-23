# シンプルグラデーションストライプ / Simple Gradient Stripes

OBS Studio 向けの、グラデーションのしま模様が流れるシンプルな入力ソースのプラグインです。

A simple input source plugin for OBS Studio that draws flowing gradient stripes.

[配信プラグイン倉庫くん](https://soko-kun.allegromoltov.jp/plugins/allegromoltov/simple-gradient-stripes) にも掲載しています。 / Also listed on [Plugin Soko-kun](https://soko-kun.allegromoltov.jp/plugins/allegromoltov/simple-gradient-stripes?lang=en).

## Features / 機能

しまの色から地の色へなめらかに移り変わるしま模様を描き、一定の速さで流す入力ソースです。配信の背景や、映像に重ねる装飾に使えます。角度・間隔・流れる速さ・色・ぼかしを、OBS の設定画面でそれぞれ独立に調整できます。地の色の不透明度を 0 にすると、しまだけをほかのソースの上に重ねられます。描画はすべて GPU のシェーダーで行います。このプラグインはネットワークに何も送りません。

An input source that draws stripes fading from the stripe color to the background color and moves them at a constant speed. Use it as a stream background or as an overlay on other video. You can adjust the angle, spacing, speed, colors, and softness independently in the OBS properties. Set the opacity of the background color to 0 to layer only the stripes over other sources. All drawing runs in a GPU shader. The plugin sends nothing over the network.

- **Angle・角度** - しまの線の傾き
- **Spacing・間隔** - しま 1 周期の長さ
- **Speed・流れる速さ** - しまが流れる速さと向き
- **Stripe color・しまの色** - しまの色と不透明度
- **Background color・地の色** - しまの間の色と不透明度
- **Softness・ぼかし** - しまの縁のなめらかさ

## Installation / インストール

### Windows

Windows 版は OBS Studio 31.1 以降向けにビルドしています。

#### 通常版 OBS への導入

プラグインを OBS Studio の共通プラグインフォルダーへ配置します。

1. OBS Studio を終了してください。
2. [Releases](../../releases) から Windows 用の zip ファイルをダウンロードし、展開してください。
3. エクスプローラーのアドレスバーへ `%PROGRAMDATA%` を入力し、Enter キーを押してください。隠しフォルダーの表示設定を変更する必要はありません。
4. `obs-studio` フォルダーを開き、その中の `plugins` フォルダーを開いてください。存在しないフォルダーは順に作成してください。
5. 展開した `simple-gradient-stripes` フォルダーを `plugins` へコピーしてください。管理者の許可を求められた場合は許可してください。
6. OBS Studio を起動し、ソースの「+」からシンプルグラデーションストライプを追加してください。

導入後の構成を次に示します。`simple-gradient-stripes` フォルダーを二重に作らないでください。PDB ファイルが同梱されている場合も、製品フォルダーごとコピーします。

```text
%PROGRAMDATA%\obs-studio\plugins\
└─ simple-gradient-stripes\
   ├─ simple-gradient-stripes.dll
   └─ data\
      ├─ gradient-stripes.effect
      └─ locale\
         ├─ en-US.ini
         └─ ja-JP.ini
```

更新するときは、OBS Studio を終了し、同じ手順で `simple-gradient-stripes` フォルダーを上書きコピーしてください。設定やシーンは削除しないでください。

#### ポータブル版 OBS

ポータブル版では、OBS Studio のルートフォルダーにある `plugins` へ製品フォルダーをコピーします。通常版の `%PROGRAMDATA%` への導入では読み込まれません。

OBS Studio を終了し、`bin` や `data` と同じ階層に `plugins` フォルダーを作成してください。展開した製品フォルダーをコピーし、`plugins\simple-gradient-stripes\simple-gradient-stripes.dll` と `plugins\simple-gradient-stripes\data` を配置してから OBS Studio を起動します。

### Linux

Ubuntu では、Releases の `simple-gradient-stripes-<version>-x86_64-linux-gnu.deb` を `sudo apt install ./<ファイル名>` で導入します。この .deb は、OBS Studio の公式の PPA (`ppa:obsproject/obs-studio`) から apt で導入した OBS Studio を前提にしています。

On Ubuntu, install `simple-gradient-stripes-<version>-x86_64-linux-gnu.deb` from Releases with `sudo apt install ./<file name>`. The package expects OBS Studio installed with apt from the official PPA (`ppa:obsproject/obs-studio`).

GitHub Actions 上の Ubuntu 24.04 (Mesa の OpenGL) で、しま模様の描画を自動で検証しています。作者は Linux の実機を持たないため、実機の GPU と OBS Studio では動作を確認していません。

### macOS

macOS 向けのビルドは GitHub Actions で生成し、macOS 15 (OpenGL) で描画を自動で検証しています。作者は macOS の実機で動作を確認していないため、配信プラグイン倉庫くんには macOS 版を載せていません。ソースからのビルドは「Build / ビルド」を参照してください。

## Usage / 使い方

1. OBS Studio を起動
2. ソースの **+** ボタン → **Simple Gradient Stripes** (または **シンプルグラデーションストライプ**) を追加
3. プロパティでパラメータを調整
4. 背景に使うときはソースの一覧の下のほうに置く。装飾として重ねるときは、地の色の不透明度を 0 にして、重ねたい映像より上に置く

配信中にしま模様を出し入れするには、OBS Studio 本体の機能を使います。

- **ホットキー**: 設定 → ホットキー で、シーン名の見出しの下にある、このソースの表示と非表示の項目にキーを割り当てます。
- **フェード**: ソースを右クリック → 表示トランジション・非表示トランジション に「フェード」を設定すると、表示と非表示の切替えがフェードします。

To show or hide the stripes while streaming, use OBS Studio's built-in features. Assign keys to the show and hide items for this source under the scene's heading in Settings → Hotkeys. To fade the stripes in and out, right-click the source and set Show Transition and Hide Transition to Fade.

## Parameters / パラメータ

| Parameter | 日本語 | Range | Default |
|-----------|--------|-------|---------|
| Angle | 角度 | 0° ~ 360° | 30° |
| Spacing | 間隔 | 4 ~ 8192 px | 120 px |
| Speed | 流れる速さ | 0 ~ 2000 px/s | 100 px/s |
| Stripe color | しまの色 | - | Light blue #9FE7FF, opaque |
| Background color | 地の色 | - | Indigo #3A3FD6, opaque |
| Softness | ぼかし | 0% ~ 200% | 50% |
| Match canvas size | キャンバスの大きさに合わせる | On / Off | On |
| Width / Height | 幅・高さ | 1 ~ 8192 | 1920 / 1080 |

角度は、しまの線の傾きです。0° で水平の線になり、値を増やすと反時計回りに回ります。しまは線に垂直な向きへ流れ、0° では下から上へ流れます。逆向きに流すときは、角度に 180° を足してください。流れる速さを 0 にすると止まります。

間隔は、しまの色と地の色をひと組とした 1 周期の長さです。ぼかしが 0% のとき、しまと地は半周期ずつのくっきりした帯になります。ぼかしを増やすと縁がなめらかになり、100% でしまの中央から地の中央までが途切れなくつながります。100% を超えると、しまの濃淡そのものが薄れ、200% で濃淡が半分になります。ぼかしは式の中で計算するため、量を大きくしても処理は重くなりません。

幅と高さは、「キャンバスの大きさに合わせる」をオフにしたときに使います。

The angle is the slope of the stripe lines. At 0°, the lines are horizontal, and larger values rotate them counterclockwise. The stripes move perpendicular to the lines; at 0°, they move upward. To reverse the direction, add 180° to the angle. A speed of 0 stops the stripes. The spacing is the length of one period, a stripe and the background next to it. At 0% softness, the stripe and the background are sharp bands of half a period each. More softness smooths the edges, and at 100% the colors blend continuously from the center of a stripe to the center of the background. Above 100%, the contrast itself fades, down to half at 200%. The softness is computed in closed form, so larger values do not cost more. Width and height are used when Match canvas size is off.

## Known Behavior / 既知の挙動

- しまの色と地の色の間の色は、線形の色空間で混ぜます。白と黒の中間は、sRGB の値で混ぜた場合より明るく見えます。
- 間隔と流れる速さは、ソースの画素で数えます。シーンの中でソースを拡大や縮小すると、見た目の間隔と速さも同じ倍率で変わります。
- 「キャンバスの大きさに合わせる」がオンのとき、ソースの大きさは OBS Studio の基本 (キャンバス) 解像度に従います。解像度を変えると、ソースの大きさも変わります。

## Build / ビルド

### Requirements / 必要環境

| Platform | Tool |
|----------|------|
| Windows | Visual Studio 17 2022 |
| Windows | CMake 3.28+ |
| macOS | Xcode 16.0, CMake 3.28+ |
| Ubuntu 24.04 | ninja-build, pkg-config, build-essential, CMake 3.28+ |

### Build Commands / ビルドコマンド

```bash
# Windows
cmake --preset windows-x64
cmake --build build_x64 --config RelWithDebInfo

# macOS
cmake --preset macos
cmake --build build_macos --config RelWithDebInfo

# Ubuntu
cmake --preset ubuntu-x86_64
cmake --build build_x86_64 --config RelWithDebInfo
```

Windows で生成物を取り出すには、`cmake --install build_x64 --config RelWithDebInfo --prefix release/RelWithDebInfo` を実行してください。生成した製品フォルダーを前述の手順で導入してください。

Windows では、描画テストを Direct3D 11 と OpenGL の両方で CTest に登録しています。OBS のグラフィックスの DLL を `cmake --build .deps/obs-studio-31.1.1/build_x64 --config Release --target libobs-d3d11 libobs-opengl` で作ってから、`ctest --test-dir build_x64 -C RelWithDebInfo` を実行してください。

## AI利用開示 / AI Disclosure

### 日本語

v1.0.0 の制作では、Anthropic Claude Code を、調査、ソースコード、ビルド設定、テスト、文書を含むほぼすべての工程で使用しました。コード読解による厳密な人間レビューは実施していません。

### English

Anthropic Claude Code was used for almost the entire development process of v1.0.0, including research, source code, build configuration, tests, and documentation. The author did not perform a rigorous human review of the source code.

## License / ライセンス

GPL-2.0 License

このプラグインは [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) をベースにしています。

## Author / 作者

AllegroMoltoV - https://www.allegromoltov.jp

## Changelog / 更新情報

### v1.0.0

- 最初の公開版です。

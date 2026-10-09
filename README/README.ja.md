# Anti-Productivity Confetti (APC) 説明書

## 概要

効率化の波に乗り続けるあなたへ、今度は画面に本物の波（紙吹雪）を起こしませんか。

**Anti-Productivity Confetti (APC)** は、キー入力やクリックをきっかけにカラフルな紙吹雪を舞わせるWindowsデスクトップオーバーレイです。画面上端の外から紙吹雪が降り続け、入力すると各モニターの左右下部にクラッカーが1秒間現れます。クラッカーから飛び出した紙片は放物線を描きます。作業を止める機能はありません。つい眺めてしまう時間については、何も約束できません。

## 動作環境

- Windows 10 / Windows 11
- Visual Studio 2022 と Windows SDK（ビルドする場合）

## 使い方

`APC.exe` を起動します。紙吹雪の表示中もマウス操作は背後のアプリケーションへ透過します。

- キーを押す、またはマウスの左・右・中央ボタンをクリックすると、左右のクラッカーから紙吹雪が飛び出します。
- **F1** / **F2** / **F3** でクラッカーのデザインを切り替えて、見た目を比較できます。切り替えと同時に紙吹雪も発射されます。
- **Esc** キーを押すと終了します。

## 設定

実行ファイルと同じフォルダーの `settings.ini` を編集します。ファイルが存在しない場合は、初回起動時に既定値で作成します。設定は起動時に読み込みます。範囲外または数値以外の値は既定値に戻り、警告を表示します。

| セクション | キー | 既定値 | 有効範囲 | 説明 |
|---|---|---:|---:|---|
| Confetti | RainIntervalMs | 50 | 20〜1000 | 上から降る紙片の出現間隔（ミリ秒）。小さいほど多くなります |
| Confetti | PiecesPerCracker | 55 | 1〜200 | 入力1回あたり、クラッカー1個から発射する紙片の数 |
| Confetti | CrackerVisibleMs | 1000 | 100〜2000 | クラッカーの表示時間（ミリ秒）。入力があるたびに表示時間を延長します |
| Confetti | CrackerStyle | PopSpring | PaperCone / FoilTube / PopSpring | 起動時のデザイン（紙コーン／箔巻きチューブ／ポップ・スプリング） |
| Confetti | MaxConfetti | 10000 | 50〜20000 | 画面上に同時に存在できる紙片の最大数 |

クラッカーのデザイン案: [3案の比較画像](../docs/cracker-designs/cracker-design-samples.png)（[紙コーン](../docs/cracker-designs/01-paper-cone.png) / [箔巻きチューブ](../docs/cracker-designs/02-foil-tube.png) / [ポップ・スプリング](../docs/cracker-designs/03-pop-spring.png)）

`CrackerStyle` は `PaperCone`、`FoilTube`、`PopSpring` のいずれかを指定します。旧設定との互換性のため、`1`、`2`、`3` もそれぞれ同じ順番で指定できます。

## ビルド

Visual Studio 2022 の「C++ によるデスクトップ開発」と Windows SDK が必要です。

```powershell
MSBuild APC.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

## プライバシーとライセンス

画面内容のキャプチャやネットワーク通信は行いません。ライセンスは [MIT License](../LICENSE) です。

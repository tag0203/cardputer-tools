# cardputer-tools

M5Stack Cardputer向けに作成したツールを管理するリポジトリです。

## Apps

| アプリ | 概要 | ソース | ドキュメント |
| --- | --- | --- | --- |
| Focus Wallet | 集中すると自由時間を獲得できるポモドーロ、JST時計、Wi-Fi設定 | [`src/focus_wallet`](src/focus_wallet) | [仕様と操作](docs/focus-wallet/README.md) |

## ディレクトリ構成

```text
cardputer-tools/
├── src/                         Cardputerへ書き込むスケッチ
│   └── focus_wallet/
├── docs/                        アプリ別の仕様・操作説明
│   └── focus-wallet/
└── assets/                      埋め込み画像の原本など
    └── focus-wallet/
```

`lib/`は、複数アプリから再利用できる独立した部品ができた時点で追加します。現在の画面ファイルはFocus Walletの状態へ密接に依存するため、スケッチと同じフォルダで管理しています。

## Focus Walletのビルド

Arduino IDEで[`src/focus_wallet/focus_wallet.ino`](src/focus_wallet/focus_wallet.ino)を開き、ボードに`M5Stack > M5Cardputer`を選択して検証・書き込みします。

CLIで確認する場合:

```sh
'/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli' compile \
  --fqbn m5stack:esp32:m5stack_cardputer \
  --build-path /tmp/cardputer-focus-wallet-build \
  src/focus_wallet
```

Wi-Fiプリセットを使う場合は、`src/focus_wallet/wifi_presets.example.h`を同じ場所の`wifi_presets.h`へコピーして編集します。認証情報を含む`wifi_presets.h`はGit管理されません。

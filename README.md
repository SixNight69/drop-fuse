# Drop Fuse — Build-up 蓄力巨集（Logic Pro AU）

一顆 **BUILD 0–100** 就控制全部：

| BUILD | Gate（對齊 BPM） | Low Cut | 音高 | 聲像 |
|---|---|---|---|---|
| 0 | 關 | 20 Hz（透明） | 0 | 100% 立體 |
| 5–35 | 1/4，深度漸強 | 慢慢往上 | 0 | 開始收 |
| 35–60 | 1/8 | ~50–150 Hz | 開始升 | 收窄 |
| 60–85 | 1/16 | ~150–530 Hz，帶共振 | 升更快 | 更窄 |
| 85–100 | 1/32 | 到 LOW CUT MAX | 到 PITCH RISE | 單聲道 |

**拉回 0 = drop**：低頻、寬度、完整律動同時回來；介面上的炸藥會爆炸。

只有一顆旋鈕，其餘固定在：Gate 完全切斷、Low Cut 最高 1.2 kHz、音高最多 +5 半音、最後收成單聲道。
旋鈕上下拖拉調整，雙擊歸零。

## 怎麼用
1. 掛在 build up 的匯流排（Bus／Summing Stack）或 master 的 Audio FX。
2. 按 `A` 打開 automation，選 **Drop Fuse → Build**。
3. 從 build up 開頭畫 0，往 drop 前一拍畫到 100（曲線往上彎會更有爆發感）。
4. drop 第一拍畫一個**垂直的落差**直接回 0。

> Gate 是跟著 Logic 的小節格線切的，所以要在播放中才會對拍。

## 編譯（Mac）
```bash
xcode-select --install        # 只需一次
brew install cmake            # 只需一次
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8
auval -v aufx Dfus Riot       # 看到 AU VALIDATION SUCCEEDED 就 OK
```
編好會自動複製到 `~/Library/Audio/Plug-Ins/Components/`。
Logic 若沒看到：設定 → Plug-in Manager → Reset & Rescan Selection。

## 做成安裝檔 .pkg
```bash
./make_installer.sh
```
產生 `dist/DropFuse-1.0.0-mac.pkg`。要給別人下載需要簽名＋公證，
做法跟 Riot Crush 的 README 一樣（設定 DEV_APP_ID / DEV_INSTALLER_ID / NOTARY_PROFILE 後再跑）。

## Windows（Ableton Live / FL Studio / Cubase）
雲端會一起編出 `DropFuse-1.0.0-win-setup.exe`：雙擊安裝，會放到
`C:\Program Files\Common Files\VST3\Drop Fuse.vst3`，DAW 重新掃描外掛後就能用。
也有 `DropFuse-1.0.0-win-vst3.zip`，解壓縮後手動把 `Drop Fuse.vst3` 放進同一個資料夾也可以。

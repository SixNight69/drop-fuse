# Drop TNT v2 — Build-up 蓄力巨集（AU / VST3）

一顆 **BUILD 0–100** 旋鈕。往上推是蓄力，拉回 0 就是 drop。

| BUILD | Gate（對齊 BPM） | Low Cut | Reverb（填 gate 空隙） | 聲像 |
|---|---|---|---|---|
| 0 | 關 | 關（直通，幾乎 0 CPU） | 0 | 100% 立體 |
| 5–35 | 1/4，深度漸強 | ~20–50 Hz | 漸濕 | 開始收 |
| 35–60 | 1/8 | ~50–150 Hz | 中 | 收窄 |
| 60–85 | 1/16 | ~150–530 Hz，帶共振 | 濕 | 更窄 |
| 85–100 | 1/32 | 到 1.2 kHz | 最濕、空間最大 | 單聲道 |

**拉回 0 = drop**：低頻、寬度、完整律動一次回來；reverb 尾巴大約 1 秒內自然收掉。

### v2 改了什麼
- 改名 Drop TNT（安裝時會自動移除舊的 Drop Fuse）
- 拿掉 pitch
- Gate 後面接 reverb send：切掉的空隙由 reverb 尾巴填滿，不會乾
- 省資源：BUILD = 0 時完全直通；推起來約佔單核 0.4%；介面不動時不重畫

## 用法
1. 掛在 build up 的 bus 或 master 的 Audio FX。
2. 按 `A` 打開 automation，選 **Drop TNT → Build**。
3. build up 開頭畫 0，往 drop 前畫到 100；drop 第一拍垂直拉回 0。

## 安裝
到 GitHub 的 **Releases** 下載：
- Mac：`DropTNT-2.0.0-mac.pkg` → 雙擊安裝（AU + VST3）
- Windows：`DropTNT-2.0.0-win-setup.exe`（VST3，Ableton / FL Studio / Cubase）

第一次開啟若被擋：Mac → 系統設定 › 隱私權與安全性 › 仍要打開；Windows → 其他資訊 › 仍要執行。

## 自己編譯（Mac）
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8
./make_installer.sh     # 產生 dist/DropTNT-2.0.0-mac.pkg
```

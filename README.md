# Drop TNT v3.2 — Build-up 蓄力巨集（AU / VST3）

一顆 **BUILD 0–100** 旋鈕。往上推是蓄力，拉回 0 就是 drop。

| BUILD | Crunch 失真 | Low Cut | Echo（DJ 式） | 空間 | 擠壓 | 聲像 |
|---|---|---|---|---|---|---|
| 0 | 關 | 關（直通，幾乎 0 CPU） | 關 | 關 | 關 | 立體 |
| 5–35 | 輕微飽和 | ~20–60 Hz | 1/2 拍起跳 | 大 | 輕 | 開始收 |
| 35–60 | 開始咬 | ~60–200 Hz | 越來越短，音高往上滑 | 變小 | 中 | 收窄 |
| 60–85 | 明顯 crunch | ~200–700 Hz，共振 | 1/32 左右，回授變強 | 小 | 重 | 更窄 |
| 85–100 | +20 dB 最髒 | 到 1.8 kHz | 幾毫秒 → 尖銳嗡鳴 | 極小 | 8:1 | 單聲道 |

**拉回 0 = drop**：所有東西一次放開。

### v3.2
- Gate 回來：1/4 → 1/8 → 1/16（不超過 1/16），深度溫和漸進＋音量補償，0–100 音量維持在約 ±1.5 dB
- 介面只剩炸彈＋一顆圓形旋鈕（左下角一條基準刻度），下方顯示 0–100%
- 動畫：三根炸藥往中間靠攏、被擠成一塊，拉回 0 爆炸

### v3.1
- 拿掉 Gate（1–30 的頓挫感和掉音量）
- 換成 Crunch 失真：tanh 飽和 + 硬削波，驅動 0 → +20 dB，有自動音量補償
- 整段 0–100 音量維持在 ±1 dB 內

### v3
- 加 DJ 式 Echo：延遲時間隨 Build 從 1/2 拍縮到幾毫秒、回授變強，像在 DJ 機上把 Echo 時間轉短 → 很尖、很刺、很緊繃
- 空間反過來：大 → 小，最後像被關進小盒子
- 加動態擠壓（compressor），越推越扁、越沒呼吸
- Low cut 推到 1.8 kHz，聲音更細更刺
- 畫面：黑色光圈往內收、紅色危險圈脈動；旋鈕換成像素炸藥

### v2
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
- Mac：`DropTNT-3.2.0-mac.pkg` → 雙擊安裝（AU + VST3）
- Windows：`DropTNT-3.2.0-win-setup.exe`（VST3，Ableton / FL Studio / Cubase）

第一次開啟若被擋：Mac → 系統設定 › 隱私權與安全性 › 仍要打開；Windows → 其他資訊 › 仍要執行。

## 自己編譯（Mac）
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8
./make_installer.sh     # 產生 dist/DropTNT-3.2.0-mac.pkg
```

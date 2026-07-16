# ESP32-MQTT-Energy-Meter

這是一個以 ESP32 為核心的智慧電表與環境監測專案，整合了：

- PZEM-004T 用電量測
- DHT11 溫濕度感測
- 光敏電阻亮度偵測
- OLED 三頁式資訊顯示
- MQTT 雲端上傳與訂閱控制
- 繼電器控制
- SG90 伺服馬達控制
- NTP 時間同步
- Node-RED / 手機儀表板監控

這個 repo 主要整理本次專案的最終成果與程式碼說明，方便後續展示、維護與延伸。

## 網站展示

這個 repo 也包含一個可直接部署到 GitHub Pages 的靜態首頁，放在專案根目錄：

- `index.html`
- `styles.css`
- `app.js`

首頁會整合專案簡介、架構圖、實體照片與文件連結，適合拿來做成果展示。

### GitHub Pages 啟用方式

在 repo 的 `Settings > Pages` 中，把來源設定成：

- `Build and deployment`
- `Source: GitHub Actions`

之後每次推送到 `main`，網站都會自動重新部署。

## 專案特色

- 即時顯示用電資訊、環境資訊與系統狀態
- MQTT 雙向通訊
- 可透過手機 APP 或儀表板遠端操作繼電器與伺服馬達
- 支援 Wi-Fi / MQTT 自動重連
- OLED 採三頁輪播與卡片式排版
- 整合完成後可直接部署於實體裝置

## 專案結構

```text
ESP32-MQTT-Energy-Meter/
├─ README.md
├─ .gitignore
├─ sketch/
│  ├─ 20_oled_show.ino
│  └─ secrets.example.h
├─ assets/
│  ├─ node_red_flow.png
│  ├─ mqtt_flow.png
│  ├─ dashboard_layout.png
│  ├─ 2.png
│  ├─ 3.png
│  ├─ 4.png
│  ├─ 5.png
│  ├─ WIN_20260716_13_28_54_Pro.jpg
│  ├─ WIN_20260716_13_29_21_Pro.jpg
│  └─ 747941411_1286448426694019_3807202194689559046_n.jpg
└─ docs/
   ├─ 結案報告.docx
   └─ 結案報告_含流程圖.docx
```

## 主要程式

主程式位於：

- `sketch/20_oled_show.ino`

這份程式整合了以下功能流程：

1. 開機初始化各模組
2. 連線 Wi-Fi
3. 連線 MQTT Broker
4. 同步 NTP 時間
5. 讀取 PZEM-004T、DHT11 與光敏資料
6. 封裝成 JSON 後發布至 MQTT
7. 訂閱控制主題，接收繼電器與伺服馬達命令
8. 透過 OLED 顯示三頁資訊

## 顯示頁面

### 第 1 頁：用電資訊
- 電壓
- 電流
- 頻率
- 功率
- 累積度數

### 第 2 頁：環境資訊
- 溫度
- 濕度
- 光照亮度

### 第 3 頁：系統狀態
- 繼電器狀態
- SG90 角度
- 日期
- 時間

## MQTT 資料格式

上傳資料為 JSON，概念如下：

```json
{"V":112.3,"I":0.05,"W":0.5,"kWh":1.2,"PF":0.85,"temp":25,"humi":58,"light":85}
```

## MQTT 控制格式

控制主題接收的資料格式：

```json
{"RELAY":"ON"}
{"RELAY":"OFF"}
{"SG90":180}
{"SG90":35}
```

## 硬體對應

| 模組 | 腳位 |
|---|---|
| OLED SDA | GPIO21 |
| OLED SCL | GPIO22 |
| PZEM RX | GPIO16 |
| PZEM TX | GPIO17 |
| 繼電器 | GPIO18 |
| SG90 | GPIO5 |
| DHT11 | GPIO14 |
| 光敏電阻 | GPIO33 |

## 如何設定

1. 複製 `sketch/secrets.example.h` 為 `sketch/secrets.h`
2. 填入你的 Wi-Fi 與 MQTT 設定
3. 用 Arduino IDE 或 Arduino CLI 開啟 `sketch/20_oled_show.ino`
4. 選擇你的 ESP32 板型與序列埠
5. 編譯並燒錄

### `secrets.h` 範例

```cpp
#pragma once

#define SECRET_WIFI_SSID "your-ssid"
#define SECRET_WIFI_PASSWORD "your-password"
#define SECRET_MQTT_HOST "mqttgo.io"
#define SECRET_MQTT_PORT 1883
#define SECRET_MQTT_USERNAME "your-user"
#define SECRET_MQTT_PASSWORD "your-password"
#define SECRET_MQTT_TOPIC "your/topic/data"
#define SECRET_MQTT_CTRL_TOPIC "your/topic/ctrl"
```

## 建議開發工具

- Arduino IDE
- Arduino CLI
- MQTT 測試工具
- Node-RED

## 圖片素材

此 repo 也保留了部分專案展示圖，包含：

- Node-RED 流程圖
- MQTT 資料流向圖
- 儀表板版面架構圖
- OLED 實體顯示圖
- PCB 與硬體整合照片

## 注意事項

- `secrets.h` 不要提交到 GitHub
- 如果更換 OLED 尺寸、模組腳位或感測器型號，需要同步調整程式中的座標與設定
- 若要公開 repo，建議再次檢查是否還有任何帳密或私人資訊
- `docs/` 內含本次專案的結案報告 Word 檔，可直接作為成果文件

## 專案狀態

目前這份程式已完成編譯與實機燒錄驗證，屬於可展示版本。

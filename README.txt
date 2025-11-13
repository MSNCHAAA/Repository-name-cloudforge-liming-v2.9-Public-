CloudForge 雲匠智慧住宅主控板 v4.1
日期：2025-10-30

使用方式：
1) 將 src/CloudForge_Home_v4_1.ino 用 Arduino IDE 打開（板子選 ESP32 Dev Module / NodeMCU-32S）。
2) 燒錄後：
   - 若已存設定 → 直接連 Wi‑Fi 與 HiveMQ，上傳 CloudForge Master Online。
   - 若找不到設定/連線失敗 → 建立 AP：CloudForge_Setup（密碼 12345678），手機瀏覽器 http://192.168.4.1 進入設定頁，輸入 Wi‑Fi 與 MQTT（預填 MSNCHHOME / CloudForge2025），儲存後自動重啟上雲。
3) OLED 顯示 Wi‑Fi 名稱、IP、MQTT 狀態；LED2 每秒閃爍代表心跳。
4) 接線請看 docs/CloudForge_Home_Wiring.svg

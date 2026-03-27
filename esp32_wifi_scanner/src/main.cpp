/*
 * =====================================================
 *  ESP32 WiFi Scanner + Firebase Realtime Database
 *  Author: Lê Tiến Đạt
 *  Mô tả: Quét WiFi và gửi dữ liệu lên Firebase
 *         để hiển thị trên trang web dashboard
 * =====================================================
 * 
 * HƯỚNG DẪN:
 * 1. Thay WIFI_SSID, WIFI_PASSWORD (WiFi nhà bạn)
 * 2. Thay FIREBASE_HOST (Database URL từ Firebase Console)
 * 3. Thay FIREBASE_AUTH (Database Secret hoặc để trống nếu Test mode)
 * 4. Upload code lên ESP32
 * 5. Mở Serial Monitor (115200) để xem trạng thái
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ==================== CẤU HÌNH WiFi ====================
// ⚠️ THAY ĐỔI THÔNG TIN WiFi CỦA BẠN
const char* WIFI_SSID     = "226.1-226.2-226.3";
const char* WIFI_PASSWORD = "0778844247zalo";

// ==================== CẤU HÌNH FIREBASE ====================
// ⚠️ THAY ĐỔI THÔNG TIN FIREBASE CỦA BẠN
// Lấy từ Firebase Console → Project Settings → Realtime Database
const char* FIREBASE_HOST = "scanwifi-6d460-default-rtdb.asia-southeast1.firebasedatabase.app";
const char* FIREBASE_AUTH = "";  // ← Để trống nếu dùng Test mode, hoặc điền Database Secret

// ==================== CẤU HÌNH SCAN ====================
const unsigned long SCAN_INTERVAL = 15000;  // Quét mỗi 15 giây
unsigned long lastScanTime = 0;

// ==================== HÀM ƯỚC TÍNH TỐC ĐỘ ====================
String estimateSpeed(int rssi) {
  if (rssi >= -30) return "300+ Mbps";
  else if (rssi >= -50) return "150-300 Mbps";
  else if (rssi >= -60) return "72-150 Mbps";
  else if (rssi >= -70) return "36-72 Mbps";
  else if (rssi >= -80) return "11-36 Mbps";
  else if (rssi >= -90) return "1-11 Mbps";
  else return "<1 Mbps";
}

int signalQuality(int rssi) {
  if (rssi >= -30) return 100;
  else if (rssi <= -100) return 0;
  else return 2 * (rssi + 100);
}

String getEncryptionType(wifi_auth_mode_t encType) {
  switch (encType) {
    case WIFI_AUTH_OPEN:            return "Open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
    default:                        return "Unknown";
  }
}

// ==================== GỬI DATA LÊN FIREBASE ====================
bool sendToFirebase(String jsonData) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  
  // Tạo URL Firebase REST API
  String url = "https://" + String(FIREBASE_HOST) + "/wifi_scan.json";
  if (strlen(FIREBASE_AUTH) > 0) {
    url += "?auth=" + String(FIREBASE_AUTH);
  }

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  Serial.println("📤 Đang gửi dữ liệu lên Firebase...");
  
  int httpCode = http.PUT(jsonData);  // PUT để ghi đè data
  
  if (httpCode == 200) {
    Serial.println("✅ Gửi Firebase thành công!");
    http.end();
    return true;
  } else {
    Serial.printf("❌ Lỗi Firebase: HTTP %d\n", httpCode);
    String response = http.getString();
    Serial.println("Response: " + response);
    http.end();
    return false;
  }
}

// ==================== QUÉT WiFi VÀ GỬI ====================
void scanAndSend() {
  Serial.println("\n📡 Đang quét WiFi...");
  int n = WiFi.scanNetworks();
  Serial.printf("✅ Tìm thấy %d mạng WiFi\n", n);

  // Tạo JSON
  DynamicJsonDocument doc(8192);
  doc["device"] = "ESP32";
  doc["ip"] = WiFi.localIP().toString();
  doc["mac"] = WiFi.macAddress();
  doc["connectedSSID"] = WiFi.SSID();
  doc["connectedRSSI"] = WiFi.RSSI();
  doc["totalNetworks"] = n;
  doc["timestamp"] = millis();
  doc["uptimeSeconds"] = millis() / 1000;
  doc["freeHeap"] = ESP.getFreeHeap();

  JsonArray networks = doc.createNestedArray("networks");

  for (int i = 0; i < n && i < 30; i++) {  // Max 30 networks
    JsonObject network = networks.createNestedObject();
    network["ssid"]           = WiFi.SSID(i);
    network["rssi"]           = WiFi.RSSI(i);
    network["channel"]        = WiFi.channel(i);
    network["encryption"]     = getEncryptionType(WiFi.encryptionType(i));
    network["signalQuality"]  = signalQuality(WiFi.RSSI(i));
    network["estimatedSpeed"] = estimateSpeed(WiFi.RSSI(i));
    network["bssid"]          = WiFi.BSSIDstr(i);
  }

  WiFi.scanDelete();

  // Chuyển JSON thành string
  String jsonString;
  serializeJson(doc, jsonString);

  Serial.printf("📊 JSON size: %d bytes\n", jsonString.length());

  // Gửi lên Firebase
  if (sendToFirebase(jsonString)) {
    Serial.println("🎉 Data đã được cập nhật trên Firebase!");
  }
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==========================================");
  Serial.println("  ESP32 WiFi Scanner + Firebase");
  Serial.println("==========================================\n");

  // Kết nối WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("🔗 Đang kết nối WiFi: %s", WIFI_SSID);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Kết nối WiFi thành công!");
    Serial.printf("📍 IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("📶 RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\n❌ Không thể kết nối WiFi! Khởi động lại...");
    ESP.restart();
  }

  Serial.printf("\n🔥 Firebase Host: %s\n", FIREBASE_HOST);
  Serial.printf("⏱️  Scan interval: %lu ms\n", SCAN_INTERVAL);
  Serial.println("==========================================\n");

  // Quét lần đầu ngay lập tức
  scanAndSend();
  lastScanTime = millis();
}

// ==================== LOOP ====================
void loop() {
  // Quét WiFi định kỳ
  if (millis() - lastScanTime >= SCAN_INTERVAL) {
    scanAndSend();
    lastScanTime = millis();
  }

  // Kiểm tra kết nối WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Mất kết nối WiFi! Đang kết nối lại...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\n✅ Đã kết nối lại! IP: %s\n", WiFi.localIP().toString().c_str());
    }
  }

  delay(100);
}

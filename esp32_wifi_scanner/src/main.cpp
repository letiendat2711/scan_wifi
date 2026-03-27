/*
 * =====================================================
 *  ESP32 WiFi Scanner - Web Server API
 *  Author: Lê Tiến Đạt
 *  Mô tả: Quét WiFi xung quanh và gửi dữ liệu qua HTTP API
 *         để hiển thị trên trang web dashboard
 * =====================================================
 * 
 * HƯỚNG DẪN:
 * 1. Thay đổi WIFI_SSID và WIFI_PASSWORD bên dưới
 * 2. Upload code lên ESP32 bằng Arduino IDE
 * 3. Mở Serial Monitor (115200 baud) để xem IP của ESP32
 * 4. Nhập IP đó vào trang dashboard.html
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>  // Cần cài thư viện ArduinoJson (v6+)

// ==================== CẤU HÌNH WiFi ====================
// ⚠️ THAY ĐỔI THÔNG TIN WiFi CỦA BẠN TẠI ĐÂY
const char* WIFI_SSID     = "226.1-226.2-226.3";
const char* WIFI_PASSWORD = "0778844247zalo";

// ==================== WEB SERVER ====================
WebServer server(80);
unsigned long startTime;

// ==================== HÀM ƯỚC TÍNH TỐC ĐỘ ====================
// Ước tính tốc độ lý thuyết dựa trên RSSI (cường độ tín hiệu)
String estimateSpeed(int rssi) {
  if (rssi >= -30) return "300+ Mbps";       // Rất mạnh
  else if (rssi >= -50) return "150-300 Mbps"; // Mạnh
  else if (rssi >= -60) return "72-150 Mbps";  // Khá
  else if (rssi >= -70) return "36-72 Mbps";   // Trung bình
  else if (rssi >= -80) return "11-36 Mbps";   // Yếu
  else if (rssi >= -90) return "1-11 Mbps";    // Rất yếu
  else return "<1 Mbps";                        // Gần như mất kết nối
}

// Tính chất lượng tín hiệu (0-100%)
int signalQuality(int rssi) {
  if (rssi >= -30) return 100;
  else if (rssi <= -100) return 0;
  else return 2 * (rssi + 100);
}

// Lấy tên loại mã hóa
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

// ==================== CORS HEADERS ====================
void sendCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ==================== API: QUÉT WiFi ====================
void handleScan() {
  sendCorsHeaders();

  Serial.println("📡 Đang quét WiFi...");
  int n = WiFi.scanNetworks();
  Serial.printf("✅ Tìm thấy %d mạng WiFi\n", n);

  // Tạo JSON document
  DynamicJsonDocument doc(4096);
  doc["device"] = "ESP32";
  doc["ip"] = WiFi.localIP().toString();
  doc["totalNetworks"] = n;
  doc["timestamp"] = millis();

  JsonArray networks = doc.createNestedArray("networks");

  for (int i = 0; i < n; i++) {
    JsonObject network = networks.createNestedObject();
    network["ssid"]           = WiFi.SSID(i);
    network["rssi"]           = WiFi.RSSI(i);
    network["channel"]        = WiFi.channel(i);
    network["encryption"]     = getEncryptionType(WiFi.encryptionType(i));
    network["signalQuality"]  = signalQuality(WiFi.RSSI(i));
    network["estimatedSpeed"] = estimateSpeed(WiFi.RSSI(i));
    network["bssid"]          = WiFi.BSSIDstr(i);
  }

  // Xóa kết quả scan cũ
  WiFi.scanDelete();

  // Chuyển JSON thành string và gửi
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);

  Serial.println("📤 Đã gửi dữ liệu WiFi scan");
}

// ==================== API: TRẠNG THÁI ESP32 ====================
void handleStatus() {
  sendCorsHeaders();

  DynamicJsonDocument doc(512);
  doc["device"]     = "ESP32";
  doc["status"]     = "online";
  doc["ip"]         = WiFi.localIP().toString();
  doc["mac"]        = WiFi.macAddress();
  doc["rssi"]       = WiFi.RSSI();  // Tín hiệu WiFi của chính ESP32
  doc["uptime"]     = (millis() - startTime) / 1000;  // Giây
  doc["freeHeap"]   = ESP.getFreeHeap();
  doc["ssid"]       = WiFi.SSID();

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ==================== HANDLE OPTIONS (CORS Preflight) ====================
void handleOptions() {
  sendCorsHeaders();
  server.send(204);
}

// ==================== TRANG CHỦ ESP32 ====================
void handleRoot() {
  sendCorsHeaders();
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<title>ESP32 WiFi Scanner</title></head><body>";
  html += "<h1>ESP32 WiFi Scanner API</h1>";
  html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
  html += "<p><a href='/api/scan'>GET /api/scan</a> - Quét WiFi</p>";
  html += "<p><a href='/api/status'>GET /api/status</a> - Trạng thái ESP32</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n====================================");
  Serial.println("  ESP32 WiFi Scanner - Starting...");
  Serial.println("====================================\n");

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
    Serial.printf("📍 IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("📶 RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\n❌ Không thể kết nối WiFi! Khởi động lại...");
    ESP.restart();
  }

  startTime = millis();

  // Cấu hình routes
  server.on("/", handleRoot);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/scan", HTTP_OPTIONS, handleOptions);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/status", HTTP_OPTIONS, handleOptions);

  server.begin();
  Serial.println("\n🌐 Web Server đã khởi động!");
  Serial.println("📡 Mở trình duyệt và truy cập:");
  Serial.printf("   http://%s\n", WiFi.localIP().toString().c_str());
  Serial.printf("   http://%s/api/scan\n", WiFi.localIP().toString().c_str());
  Serial.println("====================================\n");
}

// ==================== LOOP ====================
void loop() {
  server.handleClient();

  // Kiểm tra kết nối WiFi, tự kết nối lại nếu mất
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
}

#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Isi dengan Wi-Fi yang sama dengan laptop tempat Node.js berjalan.
const char* WIFI_SSID = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";
// Ganti alamat ini dengan IPv4 laptop, misalnya http://192.168.1.10:3000/api/data
const char* SERVER_URL = "http://192.168.1.10:3000/api/data";

#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 26
#define LORA_BAND 433E6
#define BUZZER_PIN 33
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
unsigned long buzzerOffAt = 0;
unsigned long lastWiFiAttempt = 0;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Menghubungkan Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) { delay(400); Serial.print('.'); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi OK, IP receiver: "); Serial.println(WiFi.localIP());
    showMessage("Wi-Fi ONLINE", WiFi.localIP().toString());
  } else { Serial.println("Wi-Fi belum terhubung; LoRa tetap berjalan."); showMessage("Wi-Fi OFFLINE", "Coba sambung ulang"); }
}

void showMessage(String line1, String line2) {
  if (!display.width()) return;
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.println("SAFEVEST RECEIVER"); display.println("---------------------");
  display.println(line1); display.println(line2); display.display();
}

bool postPacket(String payload, long rssi, float snr) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  http.setTimeout(2500);
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  String json = "{\"payload\":\"" + payload + "\",\"rssi\":" + String(rssi) + ",\"snr\":" + String(snr, 1) + "}";
  int code = http.POST(json);
  String response = code > 0 ? http.getString() : "";
  Serial.printf("HTTP POST: %d %s\n", code, response.c_str());
  http.end();
  return code >= 200 && code < 300;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  Wire.begin();
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) showMessage("Memulai...", "LoRa + Wi-Fi");
  connectWiFi();
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_BAND)) { Serial.println("LoRa gagal dimulai!"); showMessage("ERROR", "LoRa init gagal"); while (true) delay(1000); }
  Serial.println("LoRa receiver siap pada 433 MHz."); showMessage("LoRa ONLINE", WiFi.status() == WL_CONNECTED ? "Wi-Fi ONLINE" : "Wi-Fi OFFLINE");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED && millis() - lastWiFiAttempt > 10000) { lastWiFiAttempt = millis(); WiFi.disconnect(); WiFi.begin(WIFI_SSID, WIFI_PASSWORD); }
  if (buzzerOffAt && millis() >= buzzerOffAt) { digitalWrite(BUZZER_PIN, LOW); buzzerOffAt = 0; }
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;
  String payload;
  while (LoRa.available()) payload += (char)LoRa.read();
  long rssi = LoRa.packetRssi(); float snr = LoRa.packetSnr();
  payload.trim();
  Serial.printf("LoRa RX: %s | RSSI %ld dBm | SNR %.1f\n", payload.c_str(), rssi, snr);
  if (payload == "EMERGENCY") {
    digitalWrite(BUZZER_PIN, HIGH); buzzerOffAt = millis() + 5000;
    showMessage("!!! EMERGENCY !!!", "Sinyal diterima");
  } else if (payload.startsWith("DATA,")) {
    showMessage("Data sensor diterima", "RSSI " + String(rssi) + " dBm");
  } else {
    Serial.println("Format paket tidak dikenal; tidak diteruskan.");
    return;
  }
  if (!postPacket(payload, rssi, snr)) Serial.println("Gagal meneruskan ke backend. Periksa Wi-Fi, IP laptop, firewall, dan server Node.js.");
}

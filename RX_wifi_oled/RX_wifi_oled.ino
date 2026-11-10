
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* WIFI_SSID = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";

const char* SERVER_URL =
  "http://111.111.1.111:3000/api/data";


#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS     5
#define LORA_RST   14
#define LORA_DIO0  26
#define LORA_BAND  433E6

#define BUZZER_PIN 33

const unsigned long BUZZER_DURATION = 5000;
unsigned long buzzerActiveUntil = 0;
unsigned long emergencyDisplayUntil = 0;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

bool oledReady = false;

const unsigned long DISPLAY_INTERVAL = 4000;
unsigned long lastDisplay = 0;

bool displayPage = false;

float latitude = 0.0;
float longitude = 0.0;
float uvIndex = 0.0;
float suhu = -127.0;

float bpm = 0.0;
int beatAvg = 0;

bool gpsAvailable = false;
bool emergencyStatus = false;


unsigned long lastWiFiAttempt = 0;

void showMessage(String line1, String line2) {

  if (!oledReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.println("SAFETY VEST RECEIVER");
  display.println("--------------------");
  display.println(line1);
  display.println(line2);

  display.display();
}

void connectWiFi() {

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Menghubungkan Wi-Fi");

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 15000) {

    delay(400);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("Wi-Fi berhasil terhubung!");
    Serial.print("IP Receiver: ");
    Serial.println(WiFi.localIP());

    showMessage(
      "Wi-Fi ONLINE",
      WiFi.localIP().toString()
    );

  } else {

    Serial.println("Wi-Fi belum terhubung.");

    showMessage(
      "Wi-Fi OFFLINE",
      "Coba sambung ulang"
    );
  }
}

bool parseSensorData(String payload) {

  if (!payload.startsWith("DATA,")) {
    return false;
  }
  payload.remove(0, 5);
  String data[6];
  int index = 0;

  while (index < 6) {

    int commaIndex = payload.indexOf(',');

    if (commaIndex == -1) {
      data[index] = payload;
      index++;
      break;
    }
    data[index] = payload.substring(0, commaIndex);
    payload.remove(0, commaIndex + 1);
    index++;
  }

  if (index != 6) {
    Serial.println("Format data sensor tidak lengkap!");
    return false;
  }

  // Membaca GPS
  if (data[0] == "NO_FIX") {

    gpsAvailable = false;

  } else {

    latitude = data[0].toFloat();
    longitude = data[1].toFloat();

    gpsAvailable = true;
  }

  uvIndex = data[2].toFloat();
  suhu = data[3].toFloat();
  bpm = data[4].toFloat();
  beatAvg = data[5].toInt();

  Serial.println("Data sensor berhasil diproses.");

  return true;
}

void displayPage1() {

  if (!oledReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.println("SAFETY VEST MONITOR");
  display.println("--------------------");

  if (gpsAvailable) {

    display.print("Lat: ");
    display.println(latitude, 6);

    display.print("Lon: ");
    display.println(longitude, 6);

  } else {

    display.println("GPS: NO FIX");
    display.println("Koordinat belum ada");
  }

  display.print("UV   : ");
  display.println(uvIndex, 1);

  display.print("Temp : ");

  if (suhu > -100) {

    display.print(suhu, 1);
    display.println(" C");

  } else {

    display.println("--");
  }

  display.display();
}

void displayPage2() {

  if (!oledReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.println("HEART RATE MONITOR");
  display.println("--------------------");

  display.print("BPM       : ");

  if (bpm > 0) {
    display.println(bpm, 0);
  } else {
    display.println("--");
  }

  display.print("Average   : ");

  if (beatAvg > 0) {
    display.println(beatAvg);
  } else {
    display.println("--");
  }

  display.println();

  if (emergencyStatus) {
    display.println("!!! EMERGENCY !!!");
  } else {
    display.println("Status: NORMAL");
  }
  display.display();
}

void displayEmergency() {
  if (!oledReady) return;

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(10, 5);
  display.println("WARNING!");

  display.setTextSize(1);
  display.setCursor(20, 35);
  display.println("EMERGENCY");

  display.setCursor(15, 50);
  display.println("CHECK WORKER!");

  display.display();
}
bool postPacket(String payload, long rssi, float snr) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi tidak tersedia.");
    return false;
  }
  HTTPClient http;
  http.setTimeout(2500);
  http.begin(SERVER_URL);
  http.addHeader(
    "Content-Type",
    "application/json"
  );

  String json = "{\"payload\":\"" + payload +
                "\",\"rssi\":" + String(rssi) +
                ",\"snr\":" + String(snr, 1) + "}";

  int httpCode = http.POST(json);

  if (httpCode > 0) {
    Serial.print("HTTP POST: ");
    Serial.println(httpCode);
    Serial.print("Response: ");
    Serial.println(http.getString());
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(http.errorToString(httpCode));
  }
  http.end();
  return (httpCode >= 200 && httpCode < 300);
}

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("      SAFETY VEST RECEIVER");
  Serial.println("================================");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(21, 22);

  if (display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDR
      )) {

    oledReady = true;

    showMessage(
      "Memulai sistem...",
      "LoRa + Wi-Fi"
    );
  } else {
    Serial.println("OLED gagal dimulai!");
  }
  delay(1000);

  connectWiFi();

  SPI.begin(
    LORA_SCK,
    LORA_MISO,
    LORA_MOSI,
    LORA_SS
  );

  LoRa.setPins(
    LORA_SS,
    LORA_RST,
    LORA_DIO0
  );

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("LoRa gagal dimulai!");
    showMessage(
      "LORA ERROR",
      "LoRa gagal dimulai"
    );
    while (true) {
      delay(1000);
    }
  }
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();

  Serial.println("LoRa berhasil dimulai.");
  Serial.println("Frekuensi: 433 MHz");
  Serial.println("SF: 7");
  Serial.println("BW: 125 kHz");
  Serial.println("CR: 4/5");

  Serial.println();
  Serial.println("Menunggu data transmitter...");

  showMessage(
    "LoRa ONLINE",
    WiFi.status() == WL_CONNECTED
      ? "Wi-Fi ONLINE"
      : "Wi-Fi OFFLINE"
  );
  lastDisplay = millis();
}

void loop() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED &&
      now - lastWiFiAttempt >= 10000) {
    lastWiFiAttempt = now;
    Serial.println("Mencoba menghubungkan Wi-Fi...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  if (buzzerActiveUntil != 0 &&
      (long)(buzzerActiveUntil - now) > 0) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActiveUntil = 0;
  }

  int packetSize = LoRa.parsePacket();

  if (packetSize > 0) {
    String payload = "";

    while (LoRa.available()) {
      payload += (char)LoRa.read();
    }
    long rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    payload.trim();

    Serial.println();
    Serial.println("======= PAKET DITERIMA =======");

    Serial.print("Data : ");
    Serial.println(payload);

    Serial.print("RSSI : ");
    Serial.print(rssi);
    Serial.println(" dBm");

    Serial.print("SNR  : ");
    Serial.print(snr);
    Serial.println(" dB");

    bool validPacket = false;

    if (payload == "EMERGENCY") {
      Serial.println("!!! PERINGATAN DARURAT !!!");
      emergencyStatus = true;
      buzzerActiveUntil = millis() + BUZZER_DURATION;
      emergencyDisplayUntil = millis() + BUZZER_DURATION;
      displayEmergency();
      validPacket = true;
    }
    else if (payload.startsWith("DATA,")) {
      Serial.println("Paket data sensor diterima.");
      if (parseSensorData(payload)) {
        emergencyStatus = false;
        emergencyDisplayUntil = 0;
        validPacket = true;
      }
    }
    else {
      Serial.println("Format paket tidak dikenal.");
    }

    if (validPacket) {

      if (postPacket(payload, rssi, snr)) {
        Serial.println("Data berhasil dikirim ke website.");
      } else {
        Serial.println("Gagal mengirim data ke website.");
      }
    }
    Serial.println("==============================");
  }

  now = millis();

  if (emergencyDisplayUntil != 0) {
    if ((long)(emergencyDisplayUntil - now) > 0) {
    } else {
      emergencyDisplayUntil = 0;
      lastDisplay = now;

      if (displayPage) {
        displayPage2();
      } else {
        displayPage1();
      }
    }
  }

  else if (now - lastDisplay >= DISPLAY_INTERVAL) {
    lastDisplay = now;
    displayPage = !displayPage;
    if (displayPage) {
      displayPage2();
    } else {
      displayPage1();
    }
  }
}

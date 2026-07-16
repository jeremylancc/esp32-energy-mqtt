#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32_Servo.h>
#include <DHTesp.h>
#include <time.h>
#include "secrets.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C

#define PZEM_RX 16
#define PZEM_TX 17
#define RELAY_PIN 18
#define SERVO_PIN 5
#define DHT_PIN 14
#define PHOTO_PIN 33

#ifndef SECRET_WIFI_SSID
#define SECRET_WIFI_SSID "YOUR_WIFI_SSID"
#endif
#ifndef SECRET_WIFI_PASSWORD
#define SECRET_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif
#ifndef SECRET_MQTT_HOST
#define SECRET_MQTT_HOST "mqttgo.io"
#endif
#ifndef SECRET_MQTT_PORT
#define SECRET_MQTT_PORT 1883
#endif
#ifndef SECRET_MQTT_USERNAME
#define SECRET_MQTT_USERNAME "YOUR_MQTT_USERNAME"
#endif
#ifndef SECRET_MQTT_PASSWORD
#define SECRET_MQTT_PASSWORD "YOUR_MQTT_PASSWORD"
#endif
#ifndef SECRET_MQTT_TOPIC
#define SECRET_MQTT_TOPIC "your/topic/data"
#endif
#ifndef SECRET_MQTT_CTRL_TOPIC
#define SECRET_MQTT_CTRL_TOPIC "your/topic/ctrl"
#endif

const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;

const char* MQTT_HOST = SECRET_MQTT_HOST;
const uint16_t MQTT_PORT = SECRET_MQTT_PORT;
const char* MQTT_USERNAME = SECRET_MQTT_USERNAME;
const char* MQTT_PASSWORD = SECRET_MQTT_PASSWORD;
const char* MQTT_TOPIC = SECRET_MQTT_TOPIC;
const char* MQTT_CTRL_TOPIC = SECRET_MQTT_CTRL_TOPIC;

const unsigned long PUBLISH_INTERVAL_MS = 10UL * 1000UL;
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 15UL * 1000UL;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5UL * 1000UL;
const unsigned long PAGE_SWITCH_INTERVAL_MS = 5UL * 1000UL;
const unsigned long BOOT_SCREEN_MS = 3000UL;
const unsigned long WIFI_CONNECTED_SCREEN_MS = 3000UL;
const unsigned long COMMAND_SCREEN_MS = 3000UL;
const long NTP_GMT_OFFSET_SEC = 8L * 3600L;
const int NTP_DAYLIGHT_OFFSET_SEC = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Servo sg90;
DHTesp dht;

byte Xbuffer[50];
String mqttClientId;
String ipText = "--.--.--.--";
String rssiText = "-- dBm";
String relayStateText = "RELAY:ON";
String servoStateText = "SG90: 0 Degree";
String ntpDateText = "NTP: --/--/--";
String ntpTimeText = "TIME: --:--:--";
String commandLine1;
String commandLine2;
bool relayIsOn = true;
int servoAngle = 0;

bool dataOk = false;
float lastV = 0;
float lastA = 0;
float lastW = 0;
float lastkWh = 0;
float lastHz = 0;
float lastPF = 0;
int lastTemp = -1;
int lastHumi = -1;
int lastLight = -1;

unsigned long lastWiFiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long lastPublishMs = 0;
unsigned long lastPageSwitchMs = 0;
unsigned long bootUntilMs = 0;
unsigned long wifiConnectedUntilMs = 0;
unsigned long commandUntilMs = 0;
unsigned long lastNtpUpdateMs = 0;

int currentPage = 0;
bool wasWiFiConnected = false;

String ReadData() {
  Serial2.write(0xF8);
  Serial2.write(0x04);
  Serial2.write(0x00);
  Serial2.write(0x00);
  Serial2.write(0x00);
  Serial2.write(0x0A);
  Serial2.write(0x64);
  Serial2.write(0x64);
  delay(100);

  int i = 0;
  String rtext;
  memset(Xbuffer, 0, sizeof(Xbuffer));

  while (Serial2.available() && i < (int)sizeof(Xbuffer)) {
    byte b = Serial2.read();
    Xbuffer[i] = b;
    if (b < 0x10) {
      rtext += "0" + String(b, HEX) + " ";
    } else {
      rtext += String(b, HEX) + " ";
    }
    i++;
  }
  return rtext;
}

String getToken(const String& rtext, int targetIndex) {
  int tokenIndex = 0;
  int start = -1;

  for (int i = 0; i <= rtext.length(); i++) {
    char ch = (i < rtext.length()) ? rtext[i] : ' ';
    bool isSeparator = (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');

    if (!isSeparator && start < 0) {
      start = i;
    }

    if (isSeparator && start >= 0) {
      if (tokenIndex == targetIndex) {
        return rtext.substring(start, i);
      }
      tokenIndex++;
      start = -1;
    }
  }

  return "";
}

float decodeV(String rtext) {
  char CharV[5];
  String StringV = getToken(rtext, 3) + getToken(rtext, 4);
  StringV.toCharArray(CharV, sizeof(CharV));
  int intV = strtol(CharV, NULL, 16);
  return intV * 0.1;
}

float decodeA(String rtext) {
  char CharA[9];
  String StringA = getToken(rtext, 7) + getToken(rtext, 8) + getToken(rtext, 5) + getToken(rtext, 6);
  StringA.toCharArray(CharA, sizeof(CharA));
  int intA = strtol(CharA, NULL, 16);
  return intA * 0.001;
}

float decodeW(String rtext) {
  char CharW[9];
  String StringW = getToken(rtext, 11) + getToken(rtext, 12) + getToken(rtext, 9) + getToken(rtext, 10);
  StringW.toCharArray(CharW, sizeof(CharW));
  int intW = strtol(CharW, NULL, 16);
  return intW * 0.1;
}

float decodekWh(String rtext) {
  char CharWh[9];
  String StringWh = getToken(rtext, 15) + getToken(rtext, 16) + getToken(rtext, 13) + getToken(rtext, 14);
  StringWh.toCharArray(CharWh, sizeof(CharWh));
  int intWh = strtol(CharWh, NULL, 16);
  return intWh * 0.001;
}

float decodeHz(String rtext) {
  char CharHz[5];
  String StringHz = getToken(rtext, 17) + getToken(rtext, 18);
  StringHz.toCharArray(CharHz, sizeof(CharHz));
  int intHz = strtol(CharHz, NULL, 16);
  return intHz * 0.1;
}

float decodePF(String rtext) {
  char CharPF[5];
  String StringPF = getToken(rtext, 19) + getToken(rtext, 20);
  StringPF.toCharArray(CharPF, sizeof(CharPF));
  int intPF = strtol(CharPF, NULL, 16);
  return intPF * 0.01;
}

int readBrightnessPercent() {
  long sum = 0;
  const int samples = 16;

  for (int n = 0; n < samples; n++) {
    sum += analogRead(PHOTO_PIN);
    delayMicroseconds(200);
  }

  int raw = sum / samples;
  raw = constrain(raw, 0, 4095);

  int percent = map(raw, 0, 4095, 0, 100);
  percent = constrain(percent, 0, 100);
  return 100 - percent;
}

void drawCenteredLine(const String& text, int16_t y, uint8_t size) {
  display.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  int16_t x = (SCREEN_WIDTH - (int16_t)w) / 2;
  if (x < 0) {
    x = 0;
  }
  display.setCursor(x, y);
  display.print(text);
}

void drawFrame(int x, int y, int w, int h, uint8_t radius = 4) {
  display.drawRoundRect(x, y, w, h, radius, SSD1306_WHITE);
}

void drawHeaderBar(const String& title, int pageIndex) {
  display.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print(title);

  String pageText = String(pageIndex + 1) + "/3";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(pageText.c_str(), 0, 0, &x1, &y1, &w, &h);
  display.setCursor(SCREEN_WIDTH - w - 4, 2);
  display.print(pageText);
  display.setTextColor(SSD1306_WHITE);
}

void drawPageDots(int activePage) {
  const int baseX = 58;
  const int y = 60;
  for (int i = 0; i < 3; i++) {
    if (i == activePage) {
      display.fillCircle(baseX + i * 8, y, 2, SSD1306_WHITE);
    } else {
      display.drawCircle(baseX + i * 8, y, 2, SSD1306_WHITE);
    }
  }
}

void drawRightAligned(const String& text, int16_t rightX, int16_t y, uint8_t size) {
  display.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  int16_t x = rightX - (int16_t)w;
  if (x < 0) {
    x = 0;
  }
  display.setCursor(x, y);
  display.print(text);
}

void drawCenteredInBox(const String& text, int16_t x, int16_t y, int16_t w, uint8_t size) {
  display.setTextSize(size);
  int16_t x1, y1;
  uint16_t tw, th;
  display.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &tw, &th);
  int16_t tx = x + (w - (int16_t)tw) / 2;
  if (tx < x) {
    tx = x;
  }
  display.setCursor(tx, y);
  display.print(text);
}

void drawWifiIcon(int x, int y) {
  display.drawCircle(x + 6, y + 7, 1, SSD1306_WHITE);
  display.drawCircleHelper(x + 6, y + 7, 4, 1, SSD1306_WHITE);
  display.drawCircleHelper(x + 6, y + 7, 4, 2, SSD1306_WHITE);
  display.drawCircleHelper(x + 6, y + 7, 4, 4, SSD1306_WHITE);
  display.drawLine(x + 2, y + 11, x + 6, y + 7, SSD1306_WHITE);
  display.drawLine(x + 10, y + 11, x + 6, y + 7, SSD1306_WHITE);
}

void drawPlugIcon(int x, int y) {
  display.drawRect(x + 3, y + 4, 8, 6, SSD1306_WHITE);
  display.drawLine(x + 5, y + 2, x + 5, y + 4, SSD1306_WHITE);
  display.drawLine(x + 9, y + 2, x + 9, y + 4, SSD1306_WHITE);
  display.drawLine(x + 7, y + 10, x + 7, y + 13, SSD1306_WHITE);
  display.drawLine(x + 5, y + 13, x + 9, y + 13, SSD1306_WHITE);
}

void drawBoltIcon(int x, int y) {
  display.drawLine(x + 6, y + 1, x + 2, y + 7, SSD1306_WHITE);
  display.drawLine(x + 2, y + 7, x + 6, y + 7, SSD1306_WHITE);
  display.drawLine(x + 6, y + 7, x + 4, y + 13, SSD1306_WHITE);
  display.drawLine(x + 4, y + 13, x + 10, y + 5, SSD1306_WHITE);
  display.drawLine(x + 10, y + 5, x + 6, y + 5, SSD1306_WHITE);
  display.drawLine(x + 6, y + 5, x + 8, y + 1, SSD1306_WHITE);
}

void drawWaveIcon(int x, int y) {
  display.drawLine(x + 1, y + 8, x + 4, y + 5, SSD1306_WHITE);
  display.drawLine(x + 4, y + 5, x + 7, y + 9, SSD1306_WHITE);
  display.drawLine(x + 7, y + 9, x + 10, y + 4, SSD1306_WHITE);
  display.drawLine(x + 10, y + 4, x + 13, y + 8, SSD1306_WHITE);
}

void drawThermoIcon(int x, int y) {
  display.drawCircle(x + 6, y + 11, 3, SSD1306_WHITE);
  display.drawRect(x + 5, y + 1, 2, 10, SSD1306_WHITE);
  display.drawLine(x + 4, y + 4, x + 8, y + 4, SSD1306_WHITE);
}

void drawDropIcon(int x, int y) {
  display.drawCircle(x + 6, y + 7, 4, SSD1306_WHITE);
  display.drawLine(x + 6, y + 1, x + 2, y + 7, SSD1306_WHITE);
  display.drawLine(x + 6, y + 1, x + 10, y + 7, SSD1306_WHITE);
}

void drawSunIcon(int x, int y) {
  display.drawCircle(x + 6, y + 6, 3, SSD1306_WHITE);
  display.drawLine(x + 6, y + 0, x + 6, y + 2, SSD1306_WHITE);
  display.drawLine(x + 6, y + 10, x + 6, y + 12, SSD1306_WHITE);
  display.drawLine(x + 0, y + 6, x + 2, y + 6, SSD1306_WHITE);
  display.drawLine(x + 10, y + 6, x + 12, y + 6, SSD1306_WHITE);
  display.drawLine(x + 2, y + 2, x + 3, y + 3, SSD1306_WHITE);
  display.drawLine(x + 9, y + 9, x + 10, y + 10, SSD1306_WHITE);
  display.drawLine(x + 2, y + 10, x + 3, y + 9, SSD1306_WHITE);
  display.drawLine(x + 9, y + 3, x + 10, y + 2, SSD1306_WHITE);
}

void drawSwitchIcon(int x, int y, bool on) {
  display.drawRect(x + 1, y + 4, 14, 8, SSD1306_WHITE);
  if (on) {
    display.fillRect(x + 8, y + 5, 5, 6, SSD1306_WHITE);
  } else {
    display.fillRect(x + 3, y + 5, 5, 6, SSD1306_WHITE);
  }
}

void drawServoIcon(int x, int y) {
  display.drawCircle(x + 7, y + 7, 5, SSD1306_WHITE);
  display.drawLine(x + 7, y + 7, x + 12, y + 3, SSD1306_WHITE);
  display.drawLine(x + 7, y + 7, x + 2, y + 11, SSD1306_WHITE);
}

void drawClockIcon(int x, int y) {
  display.drawCircle(x + 6, y + 6, 6, SSD1306_WHITE);
  display.drawLine(x + 6, y + 6, x + 6, y + 3, SSD1306_WHITE);
  display.drawLine(x + 6, y + 6, x + 9, y + 7, SSD1306_WHITE);
}

void drawPlugIconMini(int x, int y) {
  display.drawRoundRect(x + 1, y + 1, 8, 6, 2, SSD1306_WHITE);
  display.drawLine(x + 3, y, x + 3, y + 1, SSD1306_WHITE);
  display.drawLine(x + 7, y, x + 7, y + 1, SSD1306_WHITE);
}

void drawBoltIconMini(int x, int y) {
  display.drawLine(x + 5, y + 1, x + 2, y + 5, SSD1306_WHITE);
  display.drawLine(x + 2, y + 5, x + 5, y + 5, SSD1306_WHITE);
  display.drawLine(x + 5, y + 5, x + 4, y + 9, SSD1306_WHITE);
}

void drawWaveIconMini(int x, int y) {
  display.drawLine(x + 1, y + 5, x + 3, y + 3, SSD1306_WHITE);
  display.drawLine(x + 3, y + 3, x + 5, y + 6, SSD1306_WHITE);
  display.drawLine(x + 5, y + 6, x + 7, y + 2, SSD1306_WHITE);
}

void drawClockIconMini(int x, int y) {
  display.drawCircle(x + 4, y + 4, 4, SSD1306_WHITE);
  display.drawLine(x + 4, y + 4, x + 4, y + 2, SSD1306_WHITE);
  display.drawLine(x + 4, y + 4, x + 6, y + 5, SSD1306_WHITE);
}

void drawGaugeBar(int x, int y, int w, int h, int percent) {
  percent = constrain(percent, 0, 100);
  display.drawRoundRect(x, y, w, h, 3, SSD1306_WHITE);
  int fillW = map(percent, 0, 100, 0, w - 2);
  if (fillW > 0) {
    display.fillRoundRect(x + 1, y + 1, fillW, h - 2, 2, SSD1306_WHITE);
  }
}

String formatFloatOrDash(bool ok, float value, uint8_t digits) {
  return ok ? String(value, (unsigned int)digits) : String("-");
}

String formatIntOrDash(int value) {
  return value >= 0 ? String(value) : String("-");
}

void setCommandOverlay(const String& line1, const String& line2) {
  commandLine1 = line1;
  commandLine2 = line2;
  commandUntilMs = millis() + COMMAND_SCREEN_MS;
}

void drawBootScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  drawFrame(10, 16, 108, 32);
  drawWifiIcon(56, 20);
  drawCenteredLine("System Starting...", 36, 1);
  display.display();
}

void drawWiFiConnectingScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  drawFrame(8, 16, 112, 32);
  drawWifiIcon(56, 20);
  drawCenteredLine("WiFi connecting...", 36, 1);
  display.display();
}

void drawIpScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  drawHeaderBar("WiFi", 0);
  drawFrame(6, 18, 116, 18);
  drawFrame(6, 38, 116, 18);
  drawWifiIcon(10, 20);
  display.setTextSize(1);
  display.setCursor(28, 24);
  display.print("IP:");
  display.print(ipText);
  display.setCursor(28, 44);
  display.print("RSSI:");
  display.print(rssiText);
  drawPageDots(0);
  display.display();
}

void drawCommandScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  drawHeaderBar("Action", currentPage);
  drawFrame(8, 18, 112, 34);
  drawCenteredLine(commandLine1, 24, 1);
  drawCenteredLine(commandLine2, 40, 1);
  drawPageDots(currentPage);
  display.display();
}

void drawEnergyScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  drawHeaderBar("Energy", 0);

  display.drawRoundRect(4, 14, 58, 18, 4, SSD1306_WHITE);
  display.drawRoundRect(66, 14, 58, 18, 4, SSD1306_WHITE);
  display.drawRoundRect(4, 34, 58, 14, 4, SSD1306_WHITE);
  display.drawRoundRect(66, 34, 58, 14, 4, SSD1306_WHITE);
  display.drawRoundRect(4, 50, 120, 12, 4, SSD1306_WHITE);

  drawPlugIconMini(6, 17);
  drawBoltIconMini(68, 17);
  drawWaveIconMini(6, 35);
  drawBoltIconMini(68, 35);
  drawClockIconMini(6, 52);

  display.setTextSize(1);
  display.setCursor(24, 16);
  display.print("V");
  display.setCursor(86, 16);
  display.print("I");
  // 第一、二列統一內縮規則，保留圖示與文字間距
  display.setCursor(24, 34);
  display.print("Hz");
  display.setCursor(86, 34);
  display.print("W");
  display.setCursor(20, 52);
  display.print("kWh");

  drawRightAligned(formatFloatOrDash(dataOk, lastV, 1) + "V", 58, 23, 1);

  drawRightAligned(formatFloatOrDash(dataOk, lastA, 2) + "A", 120, 23, 1);

  drawRightAligned(formatFloatOrDash(dataOk, lastHz, 1), 56, 38, 1);

  drawRightAligned(formatFloatOrDash(dataOk, lastW, 1), 118, 38, 1);

  display.setCursor(60, 52);
  display.print(formatFloatOrDash(dataOk, lastkWh, 3));

  display.display();
}

void drawEnvironmentScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  drawHeaderBar("Environment", 1);

  const int rowTop[3] = {14, 30, 46};
  const int rowLine[3] = {28, 44, 60};
  const String rowLabel[3] = {"Temp", "Humi", "Light"};
  const String rowValue[3] = {
    formatIntOrDash(lastTemp) + " C",
    formatIntOrDash(lastHumi) + " %",
    formatIntOrDash(lastLight) + " %"
  };

  for (int i = 0; i < 3; i++) {
    display.drawRoundRect(4, rowTop[i], 120, 13, 3, SSD1306_WHITE);
    display.drawFastHLine(4, rowLine[i], 120, SSD1306_WHITE);
  }

  drawThermoIcon(5, 14);
  drawDropIcon(5, 30);
  drawSunIcon(5, 46);

  for (int i = 0; i < 3; i++) {
    display.setTextSize(1);
    display.setCursor(20, rowTop[i] + 3);
    display.print(rowLabel[i]);
    display.print(":");
    drawRightAligned(rowValue[i], 120, rowTop[i] + 1, 2);
  }

  display.display();
}

void drawStatusScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  drawHeaderBar("System", 2);

  drawFrame(4, 14, 120, 12);
  drawSwitchIcon(6, 13, relayIsOn);
  display.setTextSize(1);
  display.setCursor(24, 17);
  display.print(relayStateText);

  drawFrame(4, 28, 120, 12);
  drawServoIcon(6, 27);
  display.setTextSize(1);
  display.setCursor(24, 31);
  display.print(servoStateText);

  drawFrame(4, 42, 120, 18);
  drawClockIcon(6, 44);
  display.setTextSize(1);
  display.setCursor(24, 46);
  display.print(ntpDateText);
  display.setCursor(24, 54);
  display.print(ntpTimeText);

  display.display();
}

void setRelay(bool on) {
  digitalWrite(RELAY_PIN, on ? LOW : HIGH);
  relayIsOn = on;
  relayStateText = on ? "RELAY:ON" : "RELAY:OFF";
}

void setServoAngle(int angle) {
  if (angle < 0 || angle > 180) {
    return;
  }
  sg90.write(angle);
  servoAngle = angle;
  servoStateText = String("SG90: ") + String(angle) + " Degree";
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String topicText = String(topic);
  if (topicText != MQTT_CTRL_TOPIC) {
    return;
  }

  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();

  if (msg.indexOf("\"RELAY\":\"ON\"") >= 0 || msg.indexOf("\"RELAY\":\"on\"") >= 0) {
    setRelay(true);
    setCommandOverlay("Relay ON", "IO18 LOW");
    Serial.println("[RELAY] ON");
  } else if (msg.indexOf("\"RELAY\":\"OFF\"") >= 0 || msg.indexOf("\"RELAY\":\"off\"") >= 0) {
    setRelay(false);
    setCommandOverlay("Relay OFF", "IO18 HIGH");
    Serial.println("[RELAY] OFF");
  }

  int sg90Pos = -1;
  int keyPos = msg.indexOf("\"SG90\":");
  if (keyPos >= 0) {
    int valueStart = keyPos + 7;
    while (valueStart < (int)msg.length() && msg[valueStart] == ' ') {
      valueStart++;
    }
    int valueEnd = valueStart;
    while (valueEnd < (int)msg.length() && isDigit(msg[valueEnd])) {
      valueEnd++;
    }
    String valueText = msg.substring(valueStart, valueEnd);
    sg90Pos = valueText.toInt();
    if (sg90Pos >= 0 && sg90Pos <= 180) {
      setServoAngle(sg90Pos);
      setCommandOverlay(String("Servo ") + String(sg90Pos), String("Move to ") + String(sg90Pos));
      Serial.print("[SG90] ");
      Serial.println(sg90Pos);
    }
  }
}

bool connectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  unsigned long now = millis();
  if (now - lastWiFiAttemptMs < WIFI_RECONNECT_INTERVAL_MS) {
    return false;
  }
  lastWiFiAttemptMs = now;

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  return false;
}

bool connectMqttIfNeeded() {
  if (WiFi.status() != WL_CONNECTED) {
    mqttClient.disconnect();
    return false;
  }

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(192);
  mqttClient.setKeepAlive(45);
  mqttClient.setSocketTimeout(5);
  mqttClient.setCallback(onMqttMessage);

  if (mqttClient.connected()) {
    return true;
  }

  unsigned long now = millis();
  if (now - lastMqttAttemptMs < MQTT_RECONNECT_INTERVAL_MS) {
    return false;
  }
  lastMqttAttemptMs = now;

  mqttClient.disconnect();
  bool ok = mqttClient.connect(mqttClientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
  if (ok) {
    bool subOk = mqttClient.subscribe(MQTT_CTRL_TOPIC);
    Serial.print("[MQTT-SUB] ");
    Serial.println(subOk ? "OK" : "FAIL");
  }
  return ok;
}

void updateNtpText() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 100)) {
    char dateBuf[16];
    char timeBuf[16];
    strftime(dateBuf, sizeof(dateBuf), "%Y/%m/%d", &timeinfo);
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &timeinfo);
    ntpDateText = String("NTP: ") + dateBuf;
    ntpTimeText = String("TIME: ") + timeBuf;
  } else {
    ntpDateText = "NTP: syncing...";
    ntpTimeText = "TIME: --:--:--";
  }
}

String buildJsonPayload(bool ok, float v, float a, float w, float kwh, float pf, int temp, int humi, int light) {
  if (!ok) {
    return "{\"V\":null,\"I\":null,\"W\":null,\"kWh\":null,\"PF\":null,\"temp\":null,\"humi\":null,\"light\":null}";
  }

  String payload = "{\"V\":";
  payload += String(v, 1);
  payload += ",\"I\":";
  payload += String(a, 2);
  payload += ",\"W\":";
  payload += String(w, 1);
  payload += ",\"kWh\":";
  payload += String(kwh, 3);
  payload += ",\"PF\":";
  payload += String(pf, 2);
  payload += ",\"temp\":";
  payload += String(temp);
  payload += ",\"humi\":";
  payload += String(humi);
  payload += ",\"light\":";
  payload += String(light);
  payload += "}";
  return payload;
}

bool publishJson(const String& payload) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  if (!mqttClient.connected()) {
    return false;
  }

  bool ok = mqttClient.publish(MQTT_TOPIC, payload.c_str(), false);
  Serial.print("[MQTT] topic=");
  Serial.print(MQTT_TOPIC);
  Serial.print(" payload=");
  Serial.println(payload);
  return ok;
}

void updateDisplay(unsigned long now) {
  if (now < bootUntilMs) {
    drawBootScreen();
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    drawWiFiConnectingScreen();
    return;
  }

  if (now < wifiConnectedUntilMs) {
    drawIpScreen();
    return;
  }

  if (now < commandUntilMs) {
    drawCommandScreen();
    return;
  }

  if (currentPage == 0) {
    drawEnergyScreen();
  } else if (currentPage == 1) {
    drawEnvironmentScreen();
  } else {
    drawStatusScreen();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== 20_OLED_SHOW BOOT ===");

  Serial2.begin(9600, SERIAL_8N1, PZEM_RX, PZEM_TX);
  analogReadResolution(12);
  analogSetPinAttenuation(PHOTO_PIN, ADC_11db);
  dht.setup(DHT_PIN, DHTesp::DHT11);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  relayIsOn = true;
  relayStateText = "RELAY:ON";

  sg90.setTimerWidth(16);
  sg90.attach(SERVO_PIN, 500, 2400);
  setServoAngle(0);

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 init failed");
    while (true) {
      delay(1000);
    }
  }

  display.setRotation(2);
  display.clearDisplay();
  display.display();

  mqttClientId = "esp32-";
  mqttClientId += String((uint32_t)esp_random(), HEX);
  mqttClientId += String((uint32_t)(ESP.getEfuseMac() & 0xFFFF), HEX);
  Serial.print("MQTT clientId: ");
  Serial.println(mqttClientId);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  configTime(NTP_GMT_OFFSET_SEC, NTP_DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.google.com", "time.windows.com");

  bootUntilMs = millis() + BOOT_SCREEN_MS;
  drawBootScreen();
}

void loop() {
  unsigned long now = millis();
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (!wifiConnected) {
    connectWiFiIfNeeded();
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (!wifiConnected) {
      wasWiFiConnected = false;
      ipText = "--.--.--.--";
      rssiText = "-- dBm";
      mqttClient.disconnect();
    }
  }

  if (wifiConnected) {
    ipText = WiFi.localIP().toString();
    rssiText = String(WiFi.RSSI()) + " dBm";

    if (!wasWiFiConnected) {
      wifiConnectedUntilMs = now + WIFI_CONNECTED_SCREEN_MS;
      wasWiFiConnected = true;
    }

    connectMqttIfNeeded();
    mqttClient.loop();
  }

  if (wifiConnected && now - lastNtpUpdateMs >= 1000UL) {
    lastNtpUpdateMs = now;
    updateNtpText();
  }

  if (now - lastPublishMs >= PUBLISH_INTERVAL_MS) {
    lastPublishMs = now;

    String rtext = ReadData();
    dataOk = false;
    lastV = 0;
    lastA = 0;
    lastW = 0;
    lastkWh = 0;
    lastHz = 0;
    lastPF = 0;

    if (rtext.length() >= 50) {
      dataOk = true;
      lastV = decodeV(rtext);
      lastA = decodeA(rtext);
      lastW = decodeW(rtext);
      lastkWh = decodekWh(rtext);
      lastHz = decodeHz(rtext);
      lastPF = decodePF(rtext);
    }

    TempAndHumidity th = dht.getTempAndHumidity();
    if (isnan(th.temperature) || isnan(th.humidity)) {
      lastTemp = -1;
      lastHumi = -1;
    } else {
      lastTemp = (int)round(th.temperature);
      lastHumi = (int)round(th.humidity);
    }

    lastLight = readBrightnessPercent();

    String payload = buildJsonPayload(dataOk, lastV, lastA, lastW, lastkWh, lastPF, lastTemp, lastHumi, lastLight);
    bool pubOk = publishJson(payload);

    Serial.print("V=");
    Serial.print(dataOk ? String(lastV, 1) : String("-"));
    Serial.print(" I=");
    Serial.print(dataOk ? String(lastA, 2) : String("-"));
    Serial.print(" W=");
    Serial.print(dataOk ? String(lastW, 1) : String("-"));
    Serial.print(" kWh=");
    Serial.print(dataOk ? String(lastkWh, 3) : String("-"));
    Serial.print(" HZ=");
    Serial.print(dataOk ? String(lastHz, 1) : String("-"));
    Serial.print(" PF=");
    Serial.print(dataOk ? String(lastPF, 2) : String("-"));
    Serial.print(" temp=");
    Serial.print(formatIntOrDash(lastTemp));
    Serial.print(" humi=");
    Serial.print(formatIntOrDash(lastHumi));
    Serial.print(" light=");
    Serial.print(formatIntOrDash(lastLight));
    Serial.print(" PUB=");
    Serial.println(pubOk ? "OK" : "FAIL");
  }

  if (now - lastPageSwitchMs >= PAGE_SWITCH_INTERVAL_MS) {
    lastPageSwitchMs = now;
    currentPage = (currentPage + 1) % 3;
  }

  updateDisplay(now);
  delay(20);
}

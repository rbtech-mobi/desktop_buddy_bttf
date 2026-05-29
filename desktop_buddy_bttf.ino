/**
 * Project: Desktop Buddy BTTF - MQTT Time Terminal
 * Hardware: WT32-SC01 Plus (ESP32-S3)
 * Libraries: LovyanGFX, PubSubClient, ArduinoJson
 */

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"
#include "BTTF_Screens.h" 
#include "_delorean_codegen.h"

static const char* PROJECT_NAME = "Desktop Buddy BTTF";

enum ScreenState { STATE_TERMINAL, STATE_CLOCK };
ScreenState currentScreen = STATE_TERMINAL;

enum WiFiMode { WIFI_DISCONNECTED, WIFI_PRIMARY, WIFI_SECONDARY };
WiFiMode currentWiFiMode = WIFI_DISCONNECTED;
unsigned long lastPrimaryRetryAt = 0;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_Parallel8 _bus_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_FT5x06 _touch_instance;

 public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.freq_write = 40000000;
      cfg.pin_wr = 47;
      cfg.pin_rd = -1;
      cfg.pin_rs = 0;
      cfg.pin_d0 = 9;
      cfg.pin_d1 = 46;
      cfg.pin_d2 = 3;
      cfg.pin_d3 = 8;
      cfg.pin_d4 = 18;
      cfg.pin_d5 = 17;
      cfg.pin_d6 = 16;
      cfg.pin_d7 = 15;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = -1;
      cfg.pin_rst = 4;
      cfg.pin_busy = -1;
      cfg.memory_width = 320;
      cfg.memory_height = 480;
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = 45;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }
    {
      auto cfg = _touch_instance.config();
      cfg.i2c_port = 1;
      cfg.i2c_addr = 0x38;
      cfg.pin_sda = 6;
      cfg.pin_scl = 5;
      cfg.freq = 400000;
      cfg.x_min = 0;
      cfg.x_max = 319;
      cfg.y_min = 0;
      cfg.y_max = 479;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

static LGFX tft;
static TimeCircuitPanel clockPanel(tft);  // Mantido sem alteracoes.
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// MQTT topics
static const char* TOPIC_RX_PRIMARY = "home/llm/inbound";
static const char* TOPIC_RX_COMMAND = "home/llm/command";
static const char* TOPIC_TX_RESPONSE = "home/llm/response";
static const char* TOPIC_TX_STATUS = "home/llm/status";
static const char* DEVICE_ID = "DESKTOP-BUDDY-BTTF";

// UI geometry and layout constants
struct Rect {
  int x;
  int y;
  int w;
  int h;
};

struct UiButton {
  Rect r;
  const char* label;
  bool pressed;
  uint32_t pressedUntil;
};

// Screen transition hotspots
Rect clockFluxHotspot = {112, 386, 96, 82};    // Enlarged flux capacitor tap area
Rect clockMsgIconHotspot = {282, 362, 24, 20}; // Small envelope icon
Rect terminalClockHotspot = {94, 46, 132, 18}; // Discreet label tap area for terminal→clock

Rect statusBar = {10, 10, 300, 24};
Rect panelTop = {10, 40, 300, 330};
Rect terminalRect = {18, 66, 284, 296};
Rect panelBottom = {10, 376, 300, 76};
Rect selectorBox = {20, 406, 174, 30};
Rect sendBox = {204, 406, 96, 30};

UiButton btnSend = {sendBox, "SEND", false, 0};

// Action selector options (user can cycle through these with touch)
static const char* ACTION_OPTIONS[] = {"UNDO", "YES", "NO", "STOP"};
static const uint8_t ACTION_COUNT = 4;
uint8_t selectedActionIndex = 0;

// Color palette (RGB565 format) - initialized at startup
uint16_t C_BG;
uint16_t C_BG_DARK;
uint16_t C_PANEL_METAL;
uint16_t C_PANEL_HI;
uint16_t C_PANEL_LO;
uint16_t C_BLACK;
uint16_t C_BLACK_SOFT;
uint16_t C_WHITE;
uint16_t C_LED_GREEN;
uint16_t C_LED_RED;
uint16_t C_LED_YELLOW;
uint16_t C_LED_OFF;
uint16_t C_TEXT_GREEN;
uint16_t C_TEXT_PROMPT;
uint16_t C_CTRL_BLUE;
uint16_t C_CTRL_BLUE_HI;
uint16_t C_CTRL_BLUE_LO;
uint16_t C_CTRL_GRAY;
uint16_t C_CTRL_GRAY_HI;
uint16_t C_CTRL_GRAY_LO;

// Runtime configuration
static const uint8_t SCREEN_BRIGHTNESS_ON = 255;
static const unsigned long TOUCH_DEBOUNCE_MS = 180;      // Milliseconds to ignore repeated touches
static const unsigned long BUTTON_PRESS_MS = 120;        // Visual feedback duration
static const uint8_t MAX_TERMINAL_LINES = 20;            // Scrolling message buffer size
String terminalLines[MAX_TERMINAL_LINES];
uint8_t terminalLineCount = 0;

String lastAction = "NONE";
String lastInboundSource = "SYS";
String lastInboundPreview = "Waiting for broker...";
String lastRxSignature = "";
unsigned long lastRxAt = 0;

bool pendingStatusPublish = false;
int lastMqttStateCode = -1;
unsigned long lastTouchAt = 0;
unsigned long lastMqttAttemptAt = 0;
unsigned long lastStatusRefreshAt = 0;
unsigned long lastHeartbeatAt = 0;
bool insecureFallbackEnabled = false;
uint8_t mqttFailCount = 0;

// Cached state for draw optimization (only redraw if changed)
bool lastDrawWifi = false;
bool lastDrawMqtt = false;
int lastDrawRc = -999;

bool hitRect(const Rect& r, int px, int py) {
  return px >= r.x && px < (r.x + r.w) && py >= r.y && py < (r.y + r.h);
}

String sanitizeText(String s) {
  s.replace("\r", " ");
  s.replace("\n", " ");
  while (s.indexOf("  ") >= 0) s.replace("  ", " ");
  s.trim();
  if (s.length() == 0) s = "...";
  return s;
}

void pushTerminalLine(const String& line) {
  if (terminalLineCount < MAX_TERMINAL_LINES) {
    terminalLines[terminalLineCount++] = line;
    return;
  }
  for (uint8_t i = 1; i < MAX_TERMINAL_LINES; i++) terminalLines[i - 1] = terminalLines[i];
  terminalLines[MAX_TERMINAL_LINES - 1] = line;
}

void pushWrapped(const String& source, const String& message) {
  // Wrap and push message to terminal: break at word boundaries or 40-char lines
  String full = "> " + source + ": " + sanitizeText(message);
  const int maxChars = 40;

  while (full.length() > maxChars) {
    int cut = full.lastIndexOf(' ', maxChars);
    if (cut < 8) cut = maxChars;
    pushTerminalLine(full.substring(0, cut));
    full = full.substring(cut);
    full.trim();
    if (full.length() > 0) full = "  " + full;  // Indent continuation lines
  }
  pushTerminalLine(full);
}

void initPalette() {
  C_BG = tft.color565(140, 146, 154);
  C_BG_DARK = tft.color565(92, 97, 105);
  C_PANEL_METAL = tft.color565(106, 115, 123);
  C_PANEL_HI = tft.color565(163, 177, 198);
  C_PANEL_LO = tft.color565(48, 51, 56);
  C_BLACK = tft.color565(5, 5, 6);
  C_BLACK_SOFT = tft.color565(34, 34, 38);
  C_WHITE = 0xFFFF;
  C_LED_GREEN = tft.color565(51, 255, 51);
  C_LED_RED = tft.color565(255, 51, 51);
  C_LED_YELLOW = tft.color565(255, 204, 0);
  C_LED_OFF = tft.color565(70, 70, 72);
  C_TEXT_GREEN = tft.color565(51, 255, 51);
  C_TEXT_PROMPT = tft.color565(255, 204, 0);

  C_CTRL_BLUE = tft.color565(48, 84, 130);
  C_CTRL_BLUE_HI = tft.color565(82, 126, 182);
  C_CTRL_BLUE_LO = tft.color565(24, 48, 78);
  C_CTRL_GRAY = tft.color565(62, 68, 76);
  C_CTRL_GRAY_HI = tft.color565(104, 110, 122);
  C_CTRL_GRAY_LO = tft.color565(28, 30, 34);
}

void drawMetalBackground() {
  tft.fillScreen(C_BG);
  for (int y = 0; y < 480; y++) {
    int v = 132 + ((y * 11) % 19) - 8;
    if (v < 95) v = 95;
    if (v > 158) v = 158;
    uint16_t c = tft.color565(v, v + 4, v + 8);
    tft.drawFastHLine(0, y, 320, c);
  }
  for (int y = 0; y < 480; y += 4) tft.drawFastHLine(0, y, 320, C_BG_DARK);
}

void drawLabelCentered(int y, int w, const char* text) {
  tft.setTextSize(1);
  int tw = tft.textWidth(text);
  int x = (320 - w) / 2;
  int lx = x + (w - tw - 10) / 2;
  tft.fillRect(lx, y, tw + 10, 13, C_BLACK_SOFT);
  tft.drawRect(lx, y, tw + 10, 13, tft.color565(68, 70, 74));
  tft.setTextColor(C_WHITE, C_BLACK_SOFT);
  tft.setCursor(lx + 5, y + 3);
  tft.print(text);
}

void drawPanelFrame(const Rect& r) {
  tft.fillRect(r.x, r.y, r.w, r.h, C_PANEL_METAL);
  tft.drawFastHLine(r.x, r.y, r.w, C_PANEL_HI);
  tft.drawFastVLine(r.x, r.y, r.h, C_PANEL_HI);
  tft.drawFastHLine(r.x, r.y + r.h - 1, r.w, C_PANEL_LO);
  tft.drawFastVLine(r.x + r.w - 1, r.y, r.h, C_PANEL_LO);
}

void drawTerminalBox() {
  tft.fillRect(terminalRect.x, terminalRect.y, terminalRect.w, terminalRect.h, C_BLACK);
  tft.drawRect(terminalRect.x, terminalRect.y, terminalRect.w, terminalRect.h, tft.color565(24, 24, 24));
  tft.drawRect(terminalRect.x + 1, terminalRect.y + 1, terminalRect.w - 2, terminalRect.h - 2, tft.color565(12, 12, 12));
}

void renderTerminalLines() {
  drawTerminalBox();
  tft.setTextSize(1);
  int y = terminalRect.y + 8;
  const int lineStep = 12;
  const int maxVisible = (terminalRect.h - 14) / lineStep;

  int start = 0;
  if (terminalLineCount > maxVisible) start = terminalLineCount - maxVisible;

  for (int i = start; i < terminalLineCount; i++) {
    String line = terminalLines[i];
    if (line.startsWith("> ")) {
      int colon = line.indexOf(':');
      if (colon > 1) {
        String left = line.substring(0, colon + 1);
        String right = line.substring(colon + 1);
        tft.setTextColor(C_TEXT_PROMPT, C_BLACK);
        tft.setCursor(terminalRect.x + 8, y);
        tft.print(left);
        tft.setTextColor(C_TEXT_GREEN, C_BLACK);
        tft.print(right);
      } else {
        tft.setTextColor(C_TEXT_PROMPT, C_BLACK);
        tft.setCursor(terminalRect.x + 8, y);
        tft.print(line);
      }
    } else {
      tft.setTextColor(C_TEXT_GREEN, C_BLACK);
      tft.setCursor(terminalRect.x + 8, y);
      tft.print(line);
    }
    y += lineStep;
  }

  tft.setTextColor(C_TEXT_GREEN, C_BLACK);
  tft.setCursor(terminalRect.x + 8, terminalRect.y + terminalRect.h - 12);
  tft.print("_");
}

void drawStatusBar(bool force) {
  bool wifiOk = WiFi.status() == WL_CONNECTED;
  bool mqttOk = mqttClient.connected();
  int rc = lastMqttStateCode;

  // Only redraw if state changed
  if (!force && wifiOk == lastDrawWifi && mqttOk == lastDrawMqtt && rc == lastDrawRc && mqttOk) return;

  lastDrawWifi = wifiOk;
  lastDrawMqtt = mqttOk;
  lastDrawRc = rc;

  tft.fillRect(statusBar.x, statusBar.y, statusBar.w, statusBar.h, C_BLACK_SOFT);
  tft.drawRect(statusBar.x, statusBar.y, statusBar.w, statusBar.h, tft.color565(66, 66, 72));
  tft.setTextSize(1);

  tft.setTextColor(C_LED_YELLOW, C_BLACK_SOFT);
  tft.setCursor(statusBar.x + 8, statusBar.y + 8);
  tft.print("MQTT: ");
  tft.print(mqttOk ? "CONNECTED" : "DISCONNECTED");

  // Show WiFi status with PRIMARY/SECONDARY indicator
  tft.setTextColor(C_WHITE, C_BLACK_SOFT);
  tft.setCursor(statusBar.x + 174, statusBar.y + 8);
  tft.print("NET:");
  if (currentWiFiMode == WIFI_PRIMARY) {
    tft.setTextColor(C_LED_GREEN, C_BLACK_SOFT);  // Green for primary
    tft.print("PRIMARY");
  } else if (currentWiFiMode == WIFI_SECONDARY) {
    tft.setTextColor(C_LED_YELLOW, C_BLACK_SOFT);  // Amber for secondary
    tft.print("SECOND");
  } else {
    tft.setTextColor(C_LED_RED, C_BLACK_SOFT);    // Red for disconnected
    tft.print("OFF");
  }

  tft.setTextColor(C_WHITE, C_BLACK_SOFT);
  tft.setCursor(statusBar.x + 230, statusBar.y + 8);
  tft.print("rc:");
  tft.print(rc);
}

void drawRaisedControl(const Rect& r, uint16_t fill, uint16_t hi, uint16_t lo) {
  tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, fill);
  tft.drawFastHLine(r.x + 1, r.y + 1, r.w - 2, hi);
  tft.drawFastVLine(r.x + 1, r.y + 1, r.h - 2, hi);
  tft.drawFastHLine(r.x + 1, r.y + r.h - 2, r.w - 2, lo);
  tft.drawFastVLine(r.x + r.w - 2, r.y + 1, r.h - 2, lo);
  tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, tft.color565(22, 24, 28));
}

void drawSelector(bool force) {
  // Render action selector control with current selection and ">" indicator
  (void)force;
  drawRaisedControl(selectorBox, C_CTRL_GRAY, C_CTRL_GRAY_HI, C_CTRL_GRAY_LO);
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, C_CTRL_GRAY);
  tft.setCursor(selectorBox.x + 10, selectorBox.y + 10);
  tft.print(ACTION_OPTIONS[selectedActionIndex]);
  tft.setTextSize(1);
  tft.setTextColor(C_LED_YELLOW, C_CTRL_GRAY);
  tft.setCursor(selectorBox.x + selectorBox.w - 22, selectorBox.y + 11);
  tft.print(">");
}

void drawUiButton(UiButton& b, uint16_t txt, uint16_t base, uint16_t hi, uint16_t lo, bool force) {
  bool pressed = b.pressed && millis() < b.pressedUntil;
  if (!force && !pressed && !b.pressed) return;

  drawRaisedControl(b.r, base, pressed ? lo : hi, pressed ? hi : lo);
  tft.setTextSize(2);
  tft.setTextColor(txt, base);
  int tw = tft.textWidth(b.label);
  tft.setCursor(b.r.x + (b.r.w - tw) / 2, b.r.y + (b.r.h - 16) / 2 + 2);
  tft.print(b.label);
}

void drawControlArea(bool force) {
  // Render action selector and SEND button (bottom panel)
  if (force) {
    drawPanelFrame(panelBottom);
    drawLabelCentered(panelBottom.y + 6, panelBottom.w, "INPUT REQUIRED");
  }
  drawSelector(force);
  drawUiButton(btnSend, C_LED_YELLOW, C_CTRL_BLUE, C_CTRL_BLUE_HI, C_CTRL_BLUE_LO, force);
}

void drawClockMessageIcon() {
  // Envelope pequeno abaixo do "PRESENT TIME", sem alterar o layout base.
  const int x = 287;
  const int y = 368;
  const int w = 14;
  const int h = 10;
  uint16_t bg = tft.color565(10, 10, 12);
  uint16_t fg = tft.color565(220, 220, 226);
  tft.fillRect(x, y, w, h, bg);
  tft.drawRect(x, y, w, h, fg);
  tft.drawLine(x, y, x + w / 2, y + h / 2, fg);
  tft.drawLine(x + w - 1, y, x + w / 2, y + h / 2, fg);
}

void drawLoadingDelorean(uint16_t frameMs) {
  // Pixel-art transition animation with DeLorean sprite
  tft.fillScreen(tft.color565(16, 18, 24));
  const int scale = 2;
  const int offX = 0;
  const int offY = 100;

  tft.startWrite();
  for (int y = 0; y < DELOREAN_H; y++) {
    for (int x = 0; x < DELOREAN_W; x++) {
      uint8_t idx = DELOREAN_IDX[y * DELOREAN_W + x];
      uint16_t c = DELOREAN_PALETTE_565[idx];
      tft.fillRect(offX + x * scale, offY + y * scale, scale, scale, c);
    }
  }
  tft.endWrite();

  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, tft.color565(16, 18, 24));
  tft.setCursor(94, 364);
  tft.print("LOADING");

  for (int i = 0; i < 3; i++) {
    tft.fillRect(236, 364, 24, 16, tft.color565(16, 18, 24));
    tft.setCursor(236, 364);
    if (i == 0) tft.print(".");
    if (i == 1) tft.print("..");
    if (i == 2) tft.print("...");
    delay(frameMs / 3);
  }
}

void drawTerminalScreen() {
  // Render complete terminal screen (main UI)
  drawMetalBackground();
  drawPanelFrame(panelTop);
  drawLabelCentered(panelTop.y + 6, panelTop.w, "LLM TRANSMISSION");
  renderTerminalLines();
  drawControlArea(true);
  drawStatusBar(true);
}

void appendSystemLine(const String& source, const String& text) {
  // Add line to terminal history and redraw if visible
  pushWrapped(source, text);
  if (currentScreen == STATE_TERMINAL) renderTerminalLines();
}

void publishDeviceStatus() {
  // Publish device status heartbeat (retained, published every 45 sec or on action)
  if (!mqttClient.connected()) {
    pendingStatusPublish = true;
    return;
  }

  StaticJsonDocument<384> doc;
  doc["device"] = DEVICE_ID;
  doc["project"] = PROJECT_NAME;
  doc["screen"] = currentScreen == STATE_TERMINAL ? "terminal" : "clock";
  doc["wifi"] = WiFi.status() == WL_CONNECTED;
  doc["mqtt"] = mqttClient.connected();
  doc["last_action"] = lastAction;
  doc["selected_action"] = ACTION_OPTIONS[selectedActionIndex];
  doc["last_source"] = lastInboundSource;
  doc["last_preview"] = lastInboundPreview;
  doc["uptime_s"] = (uint32_t)(millis() / 1000UL);

  char buffer[420];
  size_t n = serializeJson(doc, buffer, sizeof(buffer));
  if (n > 0 && n < sizeof(buffer)) {
    bool ok = mqttClient.publish(TOPIC_TX_STATUS, buffer, true);  // retain=true
    pendingStatusPublish = !ok;
    if (ok) lastHeartbeatAt = millis();
  } else {
    pendingStatusPublish = true;
  }
}

void publishSelectedAction() {
  // Publish user's selected action to MQTT response topic
  const char* action = ACTION_OPTIONS[selectedActionIndex];
  lastAction = action;

  StaticJsonDocument<256> doc;
  doc["action"] = action;
  doc["source"] = "esp32_touch";
  doc["device"] = DEVICE_ID;
  doc["ts_ms"] = millis();

  char buffer[300];
  size_t n = serializeJson(doc, buffer, sizeof(buffer));
  bool ok = false;
  if (mqttClient.connected() && n > 0 && n < sizeof(buffer)) {
    ok = mqttClient.publish(TOPIC_TX_RESPONSE, buffer, false);
  }

  String feedback = "Payload sent: ";
  feedback += action;
  feedback += ok ? " (OK)" : " (PENDING)";
  appendSystemLine("ESP", feedback);
  publishDeviceStatus();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // MQTT message handler: extract JSON fields with fallback to raw text
  String topicStr = topic;
  // Ignore our own published messages (status/response)
  if (topicStr == TOPIC_TX_STATUS || topicStr == TOPIC_TX_RESPONSE) return;

  String raw;
  raw.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) raw += (char)payload[i];
  raw = sanitizeText(raw);

  String src = "SYS";
  String msg = raw;

  // Try JSON parsing
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (!err) {
    // Ignore loopback messages (from this device)
    if (doc["device"].is<const char*>() && String((const char*)doc["device"]) == DEVICE_ID) return;
    // Extract source (try multiple field names)
    if (doc["source"].is<const char*>()) src = (const char*)doc["source"];
    else if (doc["from"].is<const char*>()) src = (const char*)doc["from"];
    else if (doc["agent"].is<const char*>()) src = (const char*)doc["agent"];

    // Extract message (try multiple field names)
    if (doc["message"].is<const char*>()) msg = (const char*)doc["message"];
    else if (doc["text"].is<const char*>()) msg = (const char*)doc["text"];
    else if (doc["prompt"].is<const char*>()) msg = (const char*)doc["prompt"];
    else if (doc["question"].is<const char*>()) msg = (const char*)doc["question"];
    else {
      // Fallback: echo entire JSON as text
      String jsonEcho;
      serializeJson(doc, jsonEcho);
      msg = jsonEcho;
    }
  } else {
    // JSON parse failed, infer source from topic name
    if (topicStr.indexOf("n8n") >= 0) src = "n8n";
    else if (topicStr.indexOf("llm") >= 0) src = "LLM";
  }

  msg = sanitizeText(msg);
  // Deduplication: ignore same message within 1.5 seconds
  String sig = topicStr + "|" + src + "|" + msg;
  if (sig == lastRxSignature && millis() - lastRxAt < 1500) return;
  lastRxSignature = sig;
  lastRxAt = millis();

  lastInboundSource = src;
  lastInboundPreview = msg.length() > 80 ? msg.substring(0, 80) : msg;
  appendSystemLine(src, msg);
}

bool connectWiFiNetwork(const char* ssid, const char* password, unsigned long timeout) {
  // Try to connect to a single WiFi network with timeout
  if (ssid == nullptr || ssid[0] == '\0') return false;  // Skip if SSID is empty

  // Disconnect first to clear any previous connection state
  WiFi.disconnect(true);  // true = turn off WiFi
  delay(500);  // Wait for clean state

  Serial.printf("Trying WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi OK on %s: %s\n", ssid, WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println("WiFi failed.");
  return false;
}

void connectWiFiDualNetwork() {
  // Try primary, then secondary. Prefer primary but use secondary as fallback.
  Serial.println("\n--- WiFi Failover ---");

  // Try primary first
  if (connectWiFiNetwork(WIFI_SSID_PRIMARY, WIFI_PASS_PRIMARY, WIFI_CONNECT_TIMEOUT_MS)) {
    currentWiFiMode = WIFI_PRIMARY;
    lastPrimaryRetryAt = millis();
    return;
  }

  // Primary failed, try secondary
  if (connectWiFiNetwork(WIFI_SSID_SECONDARY, WIFI_PASS_SECONDARY, WIFI_CONNECT_TIMEOUT_MS)) {
    currentWiFiMode = WIFI_SECONDARY;
    Serial.println("-> Using secondary WiFi network");
    lastPrimaryRetryAt = millis();
    return;
  }

  // Both failed
  currentWiFiMode = WIFI_DISCONNECTED;
  Serial.println("-> All WiFi networks unavailable");
}

void checkPrimaryWiFi() {
  // If on secondary, periodically check if primary is back online
  if (currentWiFiMode != WIFI_SECONDARY) return;
  if (millis() - lastPrimaryRetryAt < WIFI_PRIMARY_RETRY_MS) return;

  lastPrimaryRetryAt = millis();

  // Do a quick scan to see if primary SSID is available
  Serial.println("Checking if primary WiFi is back...");
  int networks = WiFi.scanNetworks();

  bool primaryFound = false;
  for (int i = 0; i < networks; i++) {
    if (strcmp(WiFi.SSID(i).c_str(), WIFI_SSID_PRIMARY) == 0) {
      primaryFound = true;
      Serial.printf("Primary SSID '%s' detected!\n", WIFI_SSID_PRIMARY);
      break;
    }
  }

  if (primaryFound) {
    // Try to switch back to primary
    Serial.println("Attempting to switch to primary WiFi...");
    WiFi.disconnect();
    if (connectWiFiNetwork(WIFI_SSID_PRIMARY, WIFI_PASS_PRIMARY, WIFI_CONNECT_TIMEOUT_MS)) {
      currentWiFiMode = WIFI_PRIMARY;
      Serial.println("-> Switched back to primary WiFi!");
    }
  }
}

void syncNTP() {
  // Sync system time from NTP (timezone: UTC-3)
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  int retries = 0;
  while (now < 24 * 3600 && retries < 20) {
    delay(300);
    now = time(nullptr);
    retries++;
  }
  (void)now;
}

bool attemptMqttConnect() {
  // Attempt MQTT connection; enable insecure mode after 3 failures
  if (WiFi.status() != WL_CONNECTED) return false;

  String clientId = "DesktopBuddyBTTF-";
  clientId += String((uint32_t)random(0xffff), HEX);
  bool ok = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS);
  if (ok) {
    lastMqttStateCode = 0;
    mqttFailCount = 0;
    mqttClient.subscribe(TOPIC_RX_PRIMARY, 1);
    mqttClient.subscribe(TOPIC_RX_COMMAND, 1);
    pendingStatusPublish = true;
    drawStatusBar(true);
    return true;
  }

  lastMqttStateCode = mqttClient.state();
  mqttFailCount++;
  if (!insecureFallbackEnabled && mqttFailCount >= 3) {
    // Fallback: allow self-signed certificates
    espClient.setInsecure();
    insecureFallbackEnabled = true;
  }
  drawStatusBar(true);
  return false;
}

void pressButton(UiButton& b) {
  b.pressed = true;
  b.pressedUntil = millis() + BUTTON_PRESS_MS;
}

void handleTerminalTouch(uint16_t x, uint16_t y) {
  // Terminal screen touch handler: selector rotation, send action, or transition to clock
  if (hitRect(terminalClockHotspot, x, y)) {
    // Tap label to transition to clock screen
    currentScreen = STATE_CLOCK;
    clockPanel.begin();
    drawClockMessageIcon();
    return;
  }

  if (hitRect(selectorBox, x, y)) {
    // Tap selector to cycle through actions
    selectedActionIndex = (selectedActionIndex + 1) % ACTION_COUNT;
    drawControlArea(true);
    return;
  }

  if (hitRect(btnSend.r, x, y)) {
    // Tap SEND button to publish selected action
    pressButton(btnSend);
    drawUiButton(btnSend, C_LED_YELLOW, C_CTRL_BLUE, C_CTRL_BLUE_HI, C_CTRL_BLUE_LO, true);
    publishSelectedAction();
    return;
  }
}

void handleClockTouch(uint16_t x, uint16_t y) {
  // Clock screen touch handler: back button or pass through to clock controls
  if (hitRect(clockFluxHotspot, x, y) || hitRect(clockMsgIconHotspot, x, y)) {
    // Tap flux capacitor or envelope icon to return to terminal with animation
    drawLoadingDelorean(540);
    currentScreen = STATE_TERMINAL;
    drawTerminalScreen();
    return;
  }
  // All other touches handled by clock panel (button/tag presses)
  clockPanel.handleButtonsAndTags(x, y);
}

void handleTouch() {
  lgfx::touch_point_t tp;
  if (!tft.getTouch(&tp)) return;

  unsigned long now = millis();
  if (now - lastTouchAt < TOUCH_DEBOUNCE_MS) return;
  lastTouchAt = now;

  if (currentScreen == STATE_TERMINAL) handleTerminalTouch(tp.x, tp.y);
  else if (currentScreen == STATE_CLOCK) handleClockTouch(tp.x, tp.y);
}

void refreshButtonRelease() {
  if (currentScreen != STATE_TERMINAL) return;
  UiButton* buttons[1] = {&btnSend};
  uint32_t now = millis();
  for (uint8_t i = 0; i < 1; i++) {
    UiButton* b = buttons[i];
    if (b->pressed && now >= b->pressedUntil) {
      b->pressed = false;
      drawUiButton(*b, C_LED_YELLOW, C_CTRL_BLUE, C_CTRL_BLUE_HI, C_CTRL_BLUE_LO, true);
    }
  }
}

void maintainConnections() {
  // Handle WiFi with dual-network failover
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastMqttAttemptAt > 3000) {
      lastMqttAttemptAt = millis();
      connectWiFiDualNetwork();
      drawStatusBar(true);
    }
    return;
  }

  // If on secondary WiFi, periodically check if primary is back online
  if (currentWiFiMode == WIFI_SECONDARY) {
    checkPrimaryWiFi();
  }

  // Handle MQTT connection and messages
  if (!mqttClient.connected()) {
    if (millis() - lastMqttAttemptAt > 3000) {
      lastMqttAttemptAt = millis();
      attemptMqttConnect();
    }
    return;
  }

  mqttClient.loop();
  if (pendingStatusPublish) publishDeviceStatus();
}

void setup() {
  Serial.begin(115200);
  randomSeed(micros());

  // Initialize display (ST7796 parallel interface)
  tft.init();
  tft.setRotation(0);  // Vertical: 320×480
  tft.setBrightness(SCREEN_BRIGHTNESS_ON);
  tft.setTextWrap(false, false);

  // Initialize UI
  initPalette();
  pushTerminalLine("> SYS: Waiting for broker...");
  drawTerminalScreen();

  // Connect to network (with failover to secondary if available)
  connectWiFiDualNetwork();
  syncNTP();

  // Configure MQTT
  espClient.setCACert(MQTT_ROOT_CA);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);

  attemptMqttConnect();
  drawStatusBar(true);
}

void loop() {
  // Main event loop: connections, input, rendering
  maintainConnections();
  handleTouch();
  refreshButtonRelease();

  // Refresh status bar every 300ms (only if changed)
  if (currentScreen == STATE_TERMINAL && millis() - lastStatusRefreshAt > 300) {
    lastStatusRefreshAt = millis();
    drawStatusBar(false);
  }

  // Heartbeat: publish status every 45 seconds
  if (mqttClient.connected() && millis() - lastHeartbeatAt > 45000) {
    pendingStatusPublish = true;
  }

  // Clock screen animation updates
  if (currentScreen == STATE_CLOCK) {
    clockPanel.update();
    drawClockMessageIcon();
  }
}

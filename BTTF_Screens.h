#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <time.h>
#include <string.h>

class TimeCircuitPanel {
 public:
  explicit TimeCircuitPanel(lgfx::LGFX_Device& lcd) : _lcd(lcd) {}

  void begin() {
    // Initialize time circuit display: palette, layout, static elements, animation state
    initColors();
    setupLayout();
    drawStaticPanel();
    drawAllRows(true);
    drawFluxFrame();
    drawControlButtons(); // Redraw on top of flux frame
    _lastClockSecond = 255;
    _lastFluxMs = millis();
  }

  void update() {
    updatePresentTimeIfNeeded();
    animateFluxIfNeeded();
    refreshPressedTags();
  }

  void onTouch(uint16_t x, uint16_t y) {
    for (uint8_t i = 0; i < 4; i++) {
      if (hit(_tags[i].x, _tags[i].y, _tags[i].w, _tags[i].h, x, y)) {
        _tags[i].pressed = true;
        _tags[i].pressedUntil = millis() + 180;
        drawTag(i);
      }
    }
  }

 private:
  struct Row {
    // Time circuit row: layout, LED color, caption, time values, cache for optimization
    int x;
    int y;
    int w;
    int h;
    uint16_t ledColor;
    const char* caption;       // "DESTINATION TIME", "LAST TIME DEPARTED", etc.
    int month;
    int day;
    int year;
    int hour;
    int minute;
    bool dynamic;              // If true, sync with system time (PRESENT TIME only)
    char cached[32];           // Cached display string to minimize redraws
  };

  struct Tag {
    // Interactive tap target for time row captions
    int x;
    int y;
    int w;
    int h;
    bool pressed;
    uint32_t pressedUntil;
  };

  struct SmallButton {
    // Rocker button state (ON/OFF toggle)
    int x;
    int y;
    int w;
    int h;
    bool on;
  };

  lgfx::LGFX_Device& _lcd;
  Row _rows[4];
  Tag _tags[4];
  SmallButton _btnFlux;
  SmallButton _btnPanel;

  uint16_t _cSteelA;
  uint16_t _cSteelB;
  uint16_t _cPanel;
  uint16_t _cPanelDeep;
  uint16_t _cBorder;
  uint16_t _cHeader;
  uint16_t _cWhite;
  uint16_t _cRed;
  uint16_t _cRedDark;
  uint16_t _cRedEdge;
  uint16_t _cBlackTag;
  uint16_t _cBlackTagEdge;
  uint16_t _cLedOff;
  uint16_t _cBlack;
  uint16_t _cGreen;
  uint16_t _cAmber;

  uint8_t _lastClockSecond = 255;
  uint8_t _fluxPhase = 0;
  uint32_t _lastFluxMs = 0;
  bool _fluxEnabled = true;
  bool _panelLedsEnabled = true;

  static bool hit(int x, int y, int w, int h, int px, int py) {
    return px >= x && px < (x + w) && py >= y && py < (y + h);
  }

  const char* monthName(int m) {
    static const char* const kMonths[12] = {
      "JAN","FEB","MAR","APR","MAY","JUN",
      "JUL","AUG","SEP","OCT","NOV","DEC"
    };
    if (m < 0 || m > 11) return "JAN";
    return kMonths[m];
  }

  void initColors() {
    _cSteelA = _lcd.color565(108, 112, 120);
    _cSteelB = _lcd.color565(88, 92, 102);
    _cPanel = _lcd.color565(52, 56, 63);
    _cPanelDeep = _lcd.color565(28, 30, 36);
    _cBorder = _lcd.color565(184, 190, 198);
    _cHeader = _lcd.color565(230, 234, 240);
    _cWhite = 0xFFFF;
    _cRed = _lcd.color565(184, 18, 18);
    _cRedDark = _lcd.color565(110, 6, 8);
    _cRedEdge = _lcd.color565(245, 86, 86);
    _cBlackTag = _lcd.color565(10, 10, 12);
    _cBlackTagEdge = _lcd.color565(86, 88, 94);
    _cLedOff = _lcd.color565(5, 5, 8);
    _cBlack = 0x0000;
    _cGreen = _lcd.color565(72, 255, 80);
    _cAmber = _lcd.color565(255, 208, 32);
  }

  void setupLayout() {
    // Configure 4 time circuit rows and control buttons
    const int x = 10;
    const int w = 300;
    const int h = 82;
    _rows[0] = {x, 18,  w, h, 0xF800, "DESTINATION TIME",   1, 15, 1985, 6, 45, false, ""};
    _rows[1] = {x, 106, w, h, 0x07E0, "LAST TIME DEPARTED", 8, 28, 1992, 6, 14, false, ""};
    _rows[2] = {x, 194, w, h, 0xFD20, "SYNC POINT",         9, 21, 2010, 4, 29, false, ""};
    _rows[3] = {x, 282, w, h, 0x07E0, "PRESENT TIME",       0,  1, 2000, 7, 51, true,  ""};

    // Setup interactive tap targets for row captions
    for (uint8_t i = 0; i < 4; i++) {
      _tags[i].x = _rows[i].x + 8;
      _tags[i].y = _rows[i].y + 56;
      _tags[i].w = 178;
      _tags[i].h = 18;
      _tags[i].pressed = false;
      _tags[i].pressedUntil = 0;
    }

    // Side rocker buttons for flux capacitor and LED panel toggles (no text)
    _btnFlux = {62, 404, 28, 52, true};
    _btnPanel = {230, 404, 28, 52, true};
  }

  void drawStaticPanel() {
    drawBrushedSteelBackground();
    _lcd.drawRoundRect(4, 4, 312, 472, 8, _cBorder);
    _lcd.drawRoundRect(6, 6, 308, 468, 8, _cPanelDeep);

    for (uint8_t i = 0; i < 4; i++) {
      drawRowFrame(i);
      drawTag(i);
    }
    drawControlButtons();
  }

  void drawControlButtons() {
    drawRockerButton(_btnFlux);
    drawRockerButton(_btnPanel);
  }

  void drawRockerButton(const SmallButton& b) {
    // Carcaca externa preta com sombra 3D.
    _lcd.fillRoundRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4, 4, _lcd.color565(8, 8, 10));
    _lcd.drawRoundRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4, 4, _lcd.color565(36, 36, 40));
    _lcd.fillRoundRect(b.x, b.y, b.w, b.h, 3, _lcd.color565(18, 18, 20));

    // Chave interna vermelha.
    int ix = b.x + 3;
    int iy = b.y + 3;
    int iw = b.w - 6;
    int ih = b.h - 6;
    _lcd.fillRoundRect(ix, iy, iw, ih, 2, _lcd.color565(220, 16, 16));

    // Estado ON/OFF com relevo (basculante).
    if (b.on) {
      _lcd.fillRect(ix + 1, iy + 1, iw - 2, ih / 2 - 1, _lcd.color565(248, 86, 86)); // topo alto
      _lcd.fillRect(ix + 1, iy + ih / 2, iw - 2, ih / 2 - 1, _lcd.color565(180, 8, 8)); // base afundada
      _lcd.drawFastHLine(ix + 1, iy + ih / 2, iw - 2, _lcd.color565(120, 0, 0));
    } else {
      _lcd.fillRect(ix + 1, iy + 1, iw - 2, ih / 2 - 1, _lcd.color565(180, 8, 8)); // topo afundado
      _lcd.fillRect(ix + 1, iy + ih / 2, iw - 2, ih / 2 - 1, _lcd.color565(248, 86, 86)); // base alta
      _lcd.drawFastHLine(ix + 1, iy + ih / 2, iw - 2, _lcd.color565(120, 0, 0));
    }

    _lcd.setTextColor(_cWhite, _lcd.color565(220, 16, 16));
    _lcd.setTextSize(2);
    _lcd.setCursor(ix + (iw / 2) - 3, iy + 3);
    _lcd.print("I");
    _lcd.setCursor(ix + (iw / 2) - 4, iy + (ih / 2) + 4);
    _lcd.print("O");
  }

  void drawBrushedSteelBackground() {
    _lcd.fillScreen(_cSteelA);
    for (int y = 0; y < 480; y++) {
      int tone = 98 + ((y * 17) % 13) - 6;
      if (tone < 70) tone = 70;
      if (tone > 128) tone = 128;
      uint16_t c = _lcd.color565(tone, tone, tone + 8);
      _lcd.drawFastHLine(0, y, 320, c);
    }
    for (int y = 0; y < 480; y += 4) {
      _lcd.drawFastHLine(0, y, 320, _lcd.color565(64, 66, 72));
    }
  }

  void drawRowFrame(uint8_t i) {
    // Draw single time circuit row: 3D frame, brushed steel texture, LED display area, labels
    const Row& r = _rows[i];
    _lcd.fillRoundRect(r.x, r.y, r.w, r.h, 6, _cSteelB);
    _lcd.drawRoundRect(r.x, r.y, r.w, r.h, 6, _cBorder);
    _lcd.drawRoundRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, 6, _cPanel);

    // Brushed steel texture: alternating horizontal lines
    for (int yy = r.y + 4; yy < r.y + r.h - 4; yy += 3) {
      uint16_t c = ((yy / 3) % 2) ? _lcd.color565(84, 88, 96) : _lcd.color565(98, 102, 110);
      _lcd.drawFastHLine(r.x + 4, yy, r.w - 8, c);
    }

    // LED display area (empty box for digits)
    const int ledX = r.x + 8;
    const int ledY = r.y + 18;
    const int ledW = r.w - 16;
    const int ledH = 34;
    _lcd.fillRect(ledX, ledY, ledW, ledH, _cLedOff);
    _lcd.drawRect(ledX, ledY, ledW, ledH, _lcd.color565(44, 44, 50));

    // Header labels in label-maker style (red background, white text)
    drawHeaderTag(r.x + 8,   r.y + 4, 56, "MONTH");
    drawHeaderTag(r.x + 68,  r.y + 4, 42, "DAY");
    drawHeaderTag(r.x + 114, r.y + 4, 56, "YEAR");
    drawHeaderTag(r.x + 212, r.y + 4, 48, "HOUR");
    drawHeaderTag(r.x + 262, r.y + 4, 30, "MIN");
  }

  void drawHeaderTag(int x, int y, int w, const char* text) {
    _lcd.fillRoundRect(x, y, w, 12, 2, _cRed);
    _lcd.drawRoundRect(x, y, w, 12, 2, _cRedEdge);
    _lcd.drawFastHLine(x + 1, y + 1, w - 2, _cRedEdge);
    _lcd.setTextColor(_cWhite, _cRed);
    _lcd.setTextSize(1);
    _lcd.setCursor(x + 3, y + 3);
    _lcd.print(text);
  }

  void drawTag(uint8_t i) {
    Tag& t = _tags[i];
    const Row& r = _rows[i];
    _lcd.setTextSize(1);
    int tw = _lcd.textWidth(r.caption);
    t.w = tw + 12; // pouco maior que o escrito
    t.x = r.x + (r.w - t.w) / 2; // centralizado

    const uint16_t fill = t.pressed ? _lcd.color565(22, 22, 24) : _cBlackTag;
    const uint16_t edge = t.pressed ? _lcd.color565(52, 54, 60) : _cBlackTagEdge;

    _lcd.fillRoundRect(t.x, t.y, t.w, t.h, 3, fill);
    _lcd.drawRoundRect(t.x, t.y, t.w, t.h, 3, edge);
    if (!t.pressed) {
      _lcd.drawFastHLine(t.x + 2, t.y + 2, t.w - 4, _lcd.color565(70, 72, 80));
    }

    _lcd.setTextColor(_cWhite, fill);
    _lcd.setTextSize(1);
    int tx = t.x + (t.w - tw) / 2;
    _lcd.setCursor(tx, t.y + (t.pressed ? 6 : 5));
    _lcd.print(r.caption);
  }

  uint16_t dimColor565(uint16_t c, uint8_t div) {
    if (div == 0) return c;
    uint8_t r = (uint8_t)(((c >> 11) & 0x1F) / div);
    uint8_t g = (uint8_t)(((c >> 5) & 0x3F) / div);
    uint8_t b = (uint8_t)((c & 0x1F) / div);
    return (uint16_t)((r << 11) | (g << 5) | b);
  }

  uint8_t segMaskForDigit(uint8_t d) {
    static const uint8_t kMask[10] = {
      0x3F, 0x06, 0x5B, 0x4F, 0x66,
      0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    return (d < 10) ? kMask[d] : 0;
  }

  void drawSegDigit(int x, int y, uint8_t digit, uint16_t onColor, uint16_t offColor) {
    // geometria digit 7-seg
    const int w = 12;
    const int h = 22;
    const int t = 2;
    const uint8_t m = segMaskForDigit(digit);

    auto segH = [&](int sx, int sy, bool on) {
      _lcd.fillRect(sx, sy, w - 2 * t, t, on ? onColor : offColor);
    };
    auto segV = [&](int sx, int sy, bool on) {
      _lcd.fillRect(sx, sy, t, (h / 2) - 1, on ? onColor : offColor);
    };

    // a b c d e f g (0x3F = abcdef)
    segH(x + t, y,           (m & 0x01) != 0); // a
    segV(x + w - t, y + 1,   (m & 0x02) != 0); // b
    segV(x + w - t, y + h/2, (m & 0x04) != 0); // c
    segH(x + t, y + h - t,   (m & 0x08) != 0); // d
    segV(x, y + h/2,         (m & 0x10) != 0); // e
    segV(x, y + 1,           (m & 0x20) != 0); // f
    segH(x + t, y + h/2 - 1, (m & 0x40) != 0); // g
  }

  void drawSeg2(int x, int y, int value, uint16_t onColor, uint16_t offColor) {
    int a = (value / 10) % 10;
    int b = value % 10;
    drawSegDigit(x, y, (uint8_t)a, onColor, offColor);
    drawSegDigit(x + 14, y, (uint8_t)b, onColor, offColor);
  }

  void drawSeg4(int x, int y, int value, uint16_t onColor, uint16_t offColor) {
    int d1 = (value / 1000) % 10;
    int d2 = (value / 100) % 10;
    int d3 = (value / 10) % 10;
    int d4 = value % 10;
    drawSegDigit(x + 0,  y, (uint8_t)d1, onColor, offColor);
    drawSegDigit(x + 14, y, (uint8_t)d2, onColor, offColor);
    drawSegDigit(x + 28, y, (uint8_t)d3, onColor, offColor);
    drawSegDigit(x + 42, y, (uint8_t)d4, onColor, offColor);
  }

  void drawAllRows(bool force) {
    for (uint8_t i = 0; i < 4; i++) {
      if (i == 3 && _rows[i].dynamic) {
        struct tm ti;
        if (getLocalTime(&ti)) {
          drawRowData(i, ti.tm_mon, ti.tm_mday, ti.tm_year + 1900, ti.tm_hour, ti.tm_min, force);
        } else {
          drawRowData(i, _rows[i].month, _rows[i].day, _rows[i].year, _rows[i].hour, _rows[i].minute, force);
        }
      } else {
        drawRowData(i, _rows[i].month, _rows[i].day, _rows[i].year, _rows[i].hour, _rows[i].minute, force);
      }
    }
  }

  void drawRowData(uint8_t i, int mon, int day, int year, int hh, int mm, bool force) {
    // Render time values for row i: month name + 7-segment digit displays
    // Uses cache to skip redundant redraws
    Row& r = _rows[i];
    char now[32];
    snprintf(now, sizeof(now), "%02d|%02d|%04d|%02d|%02d", mon, day, year, hh, mm);
    if (!force && strcmp(now, r.cached) == 0) return;  // No change, skip redraw
    strncpy(r.cached, now, sizeof(r.cached) - 1);
    r.cached[sizeof(r.cached) - 1] = '\0';

    const int ledX = r.x + 10;
    const int ledY = r.y + 20;
    const int ledW = r.w - 20;
    const int ledH = 30;
    _lcd.fillRect(ledX, ledY, ledW, ledH, _cLedOff);

    // LED brightness: full color if enabled, dimmed if disabled
    uint16_t ledOn = _panelLedsEnabled ? r.ledColor : scaleColor565(r.ledColor, 22);
    _lcd.setTextColor(ledOn, _cLedOff);
    _lcd.setTextSize(2);
    _lcd.setCursor(r.x + 12, r.y + 28);
    _lcd.print(monthName(mon));
    // 7-segment digits with "ghost" (visible off segments for clarity)
    uint16_t ghost = dimColor565(ledOn, 5);
    drawSeg2(r.x + 84,  r.y + 24, day,  ledOn, ghost);
    drawSeg4(r.x + 124, r.y + 24, year, ledOn, ghost);
    drawSeg2(r.x + 216, r.y + 24, hh,   ledOn, ghost);
    drawSeg2(r.x + 264, r.y + 24, mm,   ledOn, ghost);
  }

  void updatePresentTimeIfNeeded() {
    if (!_panelLedsEnabled) return;
    struct tm ti;
    if (!getLocalTime(&ti)) return;
    if ((uint8_t)ti.tm_sec == _lastClockSecond) return;
    _lastClockSecond = (uint8_t)ti.tm_sec;
    drawRowData(3, ti.tm_mon, ti.tm_mday, ti.tm_year + 1900, ti.tm_hour, ti.tm_min, false);
  }

  void drawFluxFrame() {
    // Draw flux module housing at bottom of display (contains animated capacitor)
    _lcd.fillRoundRect(10, 382, 300, 90, 8, _lcd.color565(42, 44, 50));
    _lcd.drawRoundRect(10, 382, 300, 90, 8, _cBorder);
    // Single centered flux capacitor module
    const int x = 130;
    const int y = 398;
    const int w = 60;
    const int h = 62;
    _lcd.fillRoundRect(x, y, w, h, 8, _lcd.color565(118, 122, 128));     // Outer gray
    _lcd.drawRoundRect(x, y, w, h, 8, _cBlack);                           // Black outline
    _lcd.fillRoundRect(x + 4, y + 4, w - 8, h - 8, 6, _lcd.color565(90, 94, 100)); // Inner gray
    _lcd.drawRoundRect(x + 4, y + 4, w - 8, h - 8, 6, _lcd.color565(42, 44, 48));
  }

  uint16_t scaleColor565(uint16_t c, uint8_t level255) {
    uint8_t r = (c >> 11) & 0x1F;
    uint8_t g = (c >> 5) & 0x3F;
    uint8_t b = c & 0x1F;
    r = (uint8_t)((r * level255) / 255);
    g = (uint8_t)((g * level255) / 255);
    b = (uint8_t)((b * level255) / 255);
    return (uint16_t)((r << 11) | (g << 5) | b);
  }

  void drawThickLine(int x1, int y1, int x2, int y2, uint16_t color, uint8_t thick) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    if (abs(dx) > abs(dy)) {
      int half = thick / 2;
      for (int o = -half; o <= half; o++) _lcd.drawLine(x1, y1 + o, x2, y2 + o, color);
    } else if (abs(dy) > abs(dx)) {
      int half = thick / 2;
      for (int o = -half; o <= half; o++) _lcd.drawLine(x1 + o, y1, x2 + o, y2, color);
    } else {
      int half = thick / 2;
      for (int o = -half; o <= half; o++) {
        _lcd.drawLine(x1 + o, y1, x2 + o, y2, color);
        _lcd.drawLine(x1, y1 + o, x2, y2 + o, color);
      }
    }
  }

  void animateFluxIfNeeded() {
    // Update flux capacitor animation: pulsing glow effect (on/off cycle with smooth ramping)
    // Runs at ~21 Hz (48 ms per frame)
    const uint32_t now = millis();
    if (now - _lastFluxMs < 48) return;
    _lastFluxMs = now;

    // Single pulsing cycle: brightness ramps up, down, then pauses before repeating
    const uint8_t kOnFrames = 20;
    const uint8_t kPauseFrames = 12;
    const uint8_t kCycle = kOnFrames + kPauseFrames;
    _fluxPhase = (_fluxPhase + 1) % kCycle;

    bool active = (_fluxPhase < kOnFrames);
    uint8_t lum = 24;  // Base dimmed state
    if (active) {
      uint8_t p = _fluxPhase;
      if (p < 10) lum = (uint8_t)(80 + p * 17);      // Ramp up
      else lum = (uint8_t)(250 - (p - 10) * 14);     // Ramp down
    }

    bool effectiveFlux = _panelLedsEnabled && _fluxEnabled;
    if (!effectiveFlux) {
      drawFluxCell(160, 429, 16, false, _cWhite);
      return;
    }
    drawFluxCell(160, 429, lum, active, _cWhite);
  }

  void drawFluxCell(int cx, int cy, uint8_t lum, bool active, uint16_t glow) {
    // Render inverted-Y flux capacitor circuit with animated glow effect
    // Clear the cell background
    _lcd.fillRect(cx - 26, cy - 27, 52, 54, _lcd.color565(90, 94, 100));
    _lcd.drawRect(cx - 26, cy - 27, 52, 54, _lcd.color565(42, 44, 48));

    // Inverted-Y circuit geometry: two upper terminals + one lower terminal
    const int ax = cx - 15, ay = cy - 16;  // Left terminal
    const int bx = cx + 15, by = cy - 16;  // Right terminal
    const int dx = cx,      dy = cy + 18;  // Bottom terminal

    // Metallic tubes (gray, no internal black outline)
    uint16_t tubeOuter = _lcd.color565(194, 198, 206);
    drawThickLine(cx, cy, ax, ay, tubeOuter, 4);
    drawThickLine(cx, cy, bx, by, tubeOuter, 4);
    drawThickLine(cx, cy, dx, dy, tubeOuter, 4);

    // Terminal endpoints with red accent dots
    uint16_t baseGray = _lcd.color565(116, 118, 122);
    _lcd.fillCircle(ax, ay, 5, baseGray);
    _lcd.fillCircle(bx, by, 5, baseGray);
    _lcd.fillCircle(dx, dy, 5, baseGray);
    _lcd.fillCircle(ax, ay, 2, 0xF800);  // Red center
    _lcd.fillCircle(bx, by, 2, 0xF800);
    _lcd.fillCircle(dx, dy, 2, 0xF800);

    // Energy pulse: bright blue glow effect (pulsing with animation)
    uint16_t arc = scaleColor565(_lcd.color565(190, 230, 255), lum);
    uint16_t coreGlow = scaleColor565(glow, lum);
    drawThickLine(cx, cy, ax, ay, arc, 2);
    drawThickLine(cx, cy, bx, by, arc, 2);
    drawThickLine(cx, cy, dx, dy, arc, 2);

    // Spark particles when active (active phase, high luminance)
    if (active && lum > 90) {
      _lcd.fillCircle((ax + cx) / 2, (ay + cy) / 2, 1, coreGlow);
      _lcd.fillCircle((bx + cx) / 2, (by + cy) / 2, 1, coreGlow);
      _lcd.fillCircle((dx + cx) / 2, (dy + cy) / 2, 1, coreGlow);
      _lcd.fillCircle(cx - 4, cy - 2, 1, coreGlow);
      _lcd.fillCircle(cx + 5, cy + 1, 1, coreGlow);
    }

    // Core nucleus with pulsing brightness
    uint16_t core = scaleColor565(_cWhite, (uint8_t)(40 + (lum * 3 / 4)));
    _lcd.fillCircle(cx, cy, 3, core);
  }

  void handleControlButtons(uint16_t x, uint16_t y) {
    // Rocker button handlers: flux capacitor toggle and LED panel toggle
    if (hit(_btnFlux.x - 2, _btnFlux.y - 2, _btnFlux.w + 4, _btnFlux.h + 4, x, y)) {
      _fluxEnabled = !_fluxEnabled;
      _btnFlux.on = _fluxEnabled;
      drawRockerButton(_btnFlux);
      return;
    }

    if (hit(_btnPanel.x - 2, _btnPanel.y - 2, _btnPanel.w + 4, _btnPanel.h + 4, x, y)) {
      _panelLedsEnabled = !_panelLedsEnabled;
      _btnPanel.on = _panelLedsEnabled;
      drawRockerButton(_btnPanel);
      drawAllRows(true);
      // Immediately refresh flux visual based on combined state
      bool eff = _panelLedsEnabled && _fluxEnabled;
      drawFluxCell(160, 429, eff ? 120 : 16, eff, _cWhite);
    }
  }

  void refreshPressedTags() {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < 4; i++) {
      if (_tags[i].pressed && now >= _tags[i].pressedUntil) {
        _tags[i].pressed = false;
        drawTag(i);
      }
    }
  }

 public:
  void handleButtonsAndTags(uint16_t x, uint16_t y) {
    handleControlButtons(x, y);
    onTouch(x, y);
  }
};

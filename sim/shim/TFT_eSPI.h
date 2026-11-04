// Laptop stand-in for TFT_eSPI: draws into memory. Uses the library's own GLCD and free-font tables,
// so text has the same shape and size as on the Cardputer.
#pragma once
#include "Arduino.h"
#define LOAD_GFXFF
#include <Fonts/GFXFF/gfxfont.h>
#include <Fonts/glcdfont.c>

enum { TL_DATUM, TC_DATUM, TR_DATUM, ML_DATUM, MC_DATUM, MR_DATUM, BL_DATUM, BC_DATUM, BR_DATUM,
       L_BASELINE, C_BASELINE, R_BASELINE };
#define TFT_BLACK     0x0000
#define TFT_WHITE     0xFFFF
#define TFT_RED       0xF800
#define TFT_DARKGREY  0x7BEF

class Canvas {
 public:
  int w = 0, h = 0;
  uint16_t* buf = nullptr;
  uint16_t fg = TFT_WHITE, bgc = TFT_BLACK; bool fillBg = false;
  int datum = TL_DATUM, font = 1;
  const GFXfont* gfx = nullptr;

  uint16_t color565(int r, int g, int b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
  void drawPixel(int32_t x, int32_t y, uint16_t c) { if (x >= 0 && y >= 0 && x < w && y < h) buf[y * w + x] = c; }
  void fillRect(int32_t x, int32_t y, int32_t rw, int32_t rh, uint16_t c) {
    for (int j = y; j < y + rh; j++) for (int i = x; i < x + rw; i++) drawPixel(i, j, c);
  }
  void drawFastHLine(int32_t x, int32_t y, int32_t l, uint16_t c) { fillRect(x, y, l, 1, c); }
  void drawFastVLine(int32_t x, int32_t y, int32_t l, uint16_t c) { fillRect(x, y, 1, l, c); }
  void drawRect(int32_t x, int32_t y, int32_t rw, int32_t rh, uint16_t c) {
    drawFastHLine(x, y, rw, c); drawFastHLine(x, y + rh - 1, rw, c);
    drawFastVLine(x, y, rh, c); drawFastVLine(x + rw - 1, y, rh, c);
  }
  void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) { drawPixel(x0, y0, c); if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * e; if (e2 >= dy) { e += dy; x0 += sx; } if (e2 <= dx) { e += dx; y0 += sy; } }
  }
  void fillCircle(int32_t cx, int32_t cy, int32_t r, uint16_t c) {
    for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r + r) drawPixel(cx + x, cy + y, c);
  }
  void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t c) {
    int minx = std::min({x0, x1, x2}), maxx = std::max({x0, x1, x2});
    int miny = std::min({y0, y1, y2}), maxy = std::max({y0, y1, y2});
    auto e = [](int ax, int ay, int bx, int by, int px, int py) { return (long)(bx - ax) * (py - ay) - (long)(by - ay) * (px - ax); };
    for (int y = miny; y <= maxy; y++) for (int x = minx; x <= maxx; x++) {
      long a = e(x0, y0, x1, y1, x, y), b = e(x1, y1, x2, y2, x, y), d = e(x2, y2, x0, y0, x, y);
      if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) drawPixel(x, y, c);
    }
    drawLine(x0, y0, x1, y1, c); drawLine(x1, y1, x2, y2, c); drawLine(x2, y2, x0, y0, c);
  }

  void setTextColor(uint16_t c) { fg = c; fillBg = false; }
  void setTextColor(uint16_t c, uint16_t b) { fg = c; bgc = b; fillBg = (c != b); }
  void setTextDatum(int d) { datum = d; }
  void setFreeFont(const GFXfont* f) { gfx = f; }
  void setTextFont(int f) { font = f; gfx = nullptr; }

  int textWidth(const char* s) {
    int wd = 0;
    for (; *s; s++) {
      if (gfx) { if (*s >= gfx->first && *s <= gfx->last) wd += gfx->glyph[*s - gfx->first].xAdvance; }
      else wd += 6;
    }
    return wd;
  }
  void drawString(const char* s, int32_t x, int32_t y, int f) { int old = font; const GFXfont* og = gfx; setTextFont(f); drawString(s, x, y); font = old; gfx = og; }
  void drawString(const char* s, int32_t x, int32_t y) {
    int tw = textWidth(s);
    int col = datum % 3;                        // 0 left, 1 centre, 2 right (also works for the baselines)
    x -= (col == 1) ? tw / 2 : (col == 2) ? tw : 0;
    if (gfx) {                                  // free fonts: only baseline datums are used by our sketches
      for (; *s; s++) {
        if (*s < gfx->first || *s > gfx->last) continue;
        const GFXglyph& g = gfx->glyph[*s - gfx->first];
        const uint8_t* bm = gfx->bitmap + g.bitmapOffset;
        int bit = 0; uint8_t bits = 0;
        for (int yy = 0; yy < g.height; yy++) for (int xx = 0; xx < g.width; xx++) {
          if (!(bit++ & 7)) bits = *bm++;
          if (bits & 0x80) drawPixel(x + g.xOffset + xx, y + g.yOffset + yy, fg);
          bits <<= 1;
        }
        x += g.xAdvance;
      }
      return;
    }
    if (font != 1) return;                      // only font 1 is needed by the sim scenes
    int row = datum / 3;                        // 0 top, 1 middle, 2 bottom
    y -= (row == 1) ? 4 : (row == 2) ? 8 : 0;
    for (; *s; s++, x += 6) {
      if (fillBg) fillRect(x, y, 6, 8, bgc);
      for (int i = 0; i < 5; i++) {
        uint8_t line = font5x7(*s, i);
        for (int j = 0; j < 8; j++) if (line & (1 << j)) drawPixel(x + i, y + j, fg);
      }
    }
  }
  static uint8_t font5x7(char ch, int i) { return ::font[(uint8_t)ch * 5 + i]; }
};

class TFT_eSPI : public Canvas {
 public:
  void init() { w = 240; h = 135; buf = (uint16_t*)calloc(w * h, 2); }
  void setRotation(int) {}
  void fillScreen(uint16_t c) { fillRect(0, 0, w, h, c); }
};

void simScreenUpdated();                        // sim/main.cpp: the screen changed

class TFT_eSprite : public Canvas {
  TFT_eSPI* tft;
 public:
  explicit TFT_eSprite(TFT_eSPI* t) : tft(t) {}
  void setColorDepth(int) {}
  void* createSprite(int sw, int sh) { w = sw; h = sh; buf = (uint16_t*)calloc(w * h, 2); return buf; }
  void* getPointer() { return buf; }
  void fillSprite(uint16_t c) { fillRect(0, 0, w, h, c); }
  void pushSprite(int x, int y) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) tft->drawPixel(x + i, y + j, buf[j * w + i]);
    simScreenUpdated();
  }
};

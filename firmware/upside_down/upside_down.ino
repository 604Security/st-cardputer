// ============================================================================
//  THE UPSIDE DOWN SWITCH  (M5Stack Cardputer)
//  Title card -> a quiet night street -> press any key -> the Upside Down.
// ============================================================================
#include <TFT_eSPI.h>
#include <driver/i2s.h>
#if __has_include("config.h")
#include "config.h"
#endif
#ifndef KID_NAME
#define KID_NAME "friend"
#endif

// ---------------- TWEAK ME! Change a number, flash again, see what happens ----
#define ASH_COUNT        45    // how many ash flakes float in the Upside Down
#define ASH_SPEED        0.4   // how fast the ash drifts up
#define LIGHTNING_CHANCE 120   // 1 in this many frames gets a red lightning flash (smaller = more)
#define BULB_FLICKER     12    // % chance a Christmas light flickers each frame
#define GLITCH_FRAMES    14    // how long the "rip" between worlds lasts
#define VOLUME           6     // 0 = silent ... 10 = loud
#define DEMO_X           166   // where the Demogorgon stands (0 = far left, 239 = far right)
#define DEMO_EVERY       5000  // milliseconds between the Demogorgon opening its head
#define WILL_PEEK        12    // how far Will leans out from behind his tree (0 = never)
#define FRIEND_SPEED     1.0   // how fast the friends walk while they search for Will
#define CALL_EVERY       90    // frames between someone shouting "Will?" (30 frames = 1 second)
// ------------------------------------------------------------------------------

#define W 240
#define H 135
#define LED_PIN 21              // the RGB LED on the Cardputer
#define SR 16000                // sound samples per second

TFT_eSPI tft;
TFT_eSprite bg(&tft);           // the world picture (drawn once per world)
TFT_eSprite frame(&tft);        // what we show this frame (bg + moving stuff)

bool upsideDown = false;

// the search party friends (their drawing code is further down)
enum Look { ELEVEN, MIKE, DUSTIN, LUCAS };
struct Friend { Look look; float x; int dir; float speed; };

uint16_t rgb(int r, int g, int b) { return tft.color565(r, g, b); }

// ============================== KEYBOARD =====================================
// The Cardputer keyboard is a grid: pick a row with 3 address pins, read 7 columns.
const uint8_t KB_ADDR[3] = {8, 9, 11};
const uint8_t KB_IN[7]   = {13, 15, 3, 4, 5, 6, 7};

void keyboardBegin() {
  for (auto p : KB_ADDR) pinMode(p, OUTPUT);
  for (auto p : KB_IN)   pinMode(p, INPUT_PULLUP);
  pinMode(0, INPUT_PULLUP);   // the G0 button on top
}

bool anyKeyDown() {
  if (digitalRead(0) == LOW) return true;
  for (int a = 0; a < 8; a++) {
    for (int b = 0; b < 3; b++) digitalWrite(KB_ADDR[b], (a >> b) & 1);
    delayMicroseconds(3);
    for (auto p : KB_IN) if (digitalRead(p) == LOW) return true;
  }
  return false;
}

bool keyPressed() {           // true once per press (not while held)
  static bool was = false;
  bool now = anyKeyDown();
  bool pressed = now && !was;
  was = now;
  return pressed;
}

// ================================ SOUND ======================================
enum Mood { QUIET, INTRO, NIGHT, UPSIDE, WHOOSH };
volatile Mood mood = QUIET;
volatile float introLevel = 0;
volatile float growl = 0;      // 0 = quiet, 1 = full Demogorgon growl

uint32_t rng = 12345;
float noise() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (int32_t)rng / 2147483648.0f; }

void audioTask(void*) {
  static int16_t buf[256 * 2];
  float p1 = 0, p2 = 0, p3 = 0, lp = 0, lp2 = 0, env = 0, sweep = 1, g1 = 0, lp3 = 0;
  uint32_t t = 0;
  for (;;) {
    for (int i = 0; i < 256; i++, t++) {
      float s = 0, target = 0;
      Mood m = mood;
      if (m == INTRO || m == UPSIDE) {
        // a deep, wobbly drone: two buzzy notes slightly out of tune
        p1 += 55.0f / SR;  if (p1 >= 1) p1 -= 1;
        p2 += 55.4f / SR;  if (p2 >= 1) p2 -= 1;
        p3 += 110.3f / SR; if (p3 >= 1) p3 -= 1;
        float x = (2 * p1 - 1) + (2 * p2 - 1) + 0.5f * (2 * p3 - 1) + 0.6f * noise();
        lp += 0.04f * (x - lp);
        float breathe = 0.7f + 0.3f * sinf(t * 2 * PI * 0.15f / SR);
        s = lp * breathe;
        // the growl: rough rumbly noise that rattles about 38 times a second
        g1 += 38.0f / SR; if (g1 >= 1) g1 -= 1;
        lp3 += 0.12f * (noise() - lp3);
        s += growl * lp3 * (g1 < 0.5f ? 4.0f : 1.0f);
        target = (m == INTRO) ? introLevel : 1.0f;
      } else if (m == NIGHT) {
        // crickets: three little chirps every 1.6 seconds
        uint32_t c = t % 25600;
        if (c < 3 * 1400 && (c % 1400) < 650) s = sinf(t * 2 * PI * 4300.0f / SR) * 0.35f;
        target = 0.6f;
      } else if (m == WHOOSH) {
        // noise that sweeps down, like being pulled through a wall
        sweep *= 0.99985f;
        lp2 += (0.02f + 0.5f * sweep) * (noise() - lp2);
        s = lp2 * 2.5f;
        target = 1.0f;
      }
      if (m != WHOOSH) sweep = 1;
      env += 0.0008f * (target - env);
      int v = (int)(s * env * VOLUME * 700);
      if (v > 32000) v = 32000; if (v < -32000) v = -32000;
      buf[2 * i] = buf[2 * i + 1] = v;
    }
    size_t w;
    i2s_write(I2S_NUM_0, buf, sizeof(buf), &w, portMAX_DELAY);
  }
}

void soundBegin() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SR,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 6,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
  };
  i2s_pin_config_t pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE, .bck_io_num = 41, .ws_io_num = 43,
    .data_out_num = 42, .data_in_num = I2S_PIN_NO_CHANGE,
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 2, nullptr, 0);
}

// ============================== TITLE CARD ===================================
int glyphHeight(const GFXfont* f, char ch) { return f->glyph[ch - f->first].height; }

// Draws text as a hollow glowing outline: glow, then outline, then black inside.
void outlined(TFT_eSprite& s, const char* txt, int x, int y, uint16_t c, uint16_t glow) {
  for (int dx = -2; dx <= 2; dx++) for (int dy = -2; dy <= 2; dy++) {
    if (abs(dx) == 2 || abs(dy) == 2) { s.setTextColor(glow); s.drawString(txt, x + dx, y + dy); }
  }
  for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) {
    if (dx || dy) { s.setTextColor(c); s.drawString(txt, x + dx, y + dy); }
  }
  s.setTextColor(TFT_BLACK);
  s.drawString(txt, x, y);
}

void outlinedBar(TFT_eSprite& s, int x1, int x2, int y, uint16_t c, uint16_t glow) {
  if (x2 <= x1) return;
  s.drawRect(x1 - 1, y - 2, x2 - x1 + 2, 6, glow);
  s.fillRect(x1, y - 1, x2 - x1, 4, c);
  s.fillRect(x1 + 1, y, x2 - x1 - 2, 2, TFT_BLACK);
}

void drawTitle(TFT_eSprite& s, uint16_t c, uint16_t glow) {
  s.fillSprite(TFT_BLACK);
  s.setTextDatum(L_BASELINE);
  const int base1 = 66, base2 = 100;

  s.setFreeFont(&FreeSerifBold24pt7b);
  int wS = s.textWidth("S"), wR = s.textWidth("R");
  s.setFreeFont(&FreeSerifBold18pt7b);
  int wMid = s.textWidth("TRANGE");
  int hMid = glyphHeight(&FreeSerifBold18pt7b, 'T');
  int total = wS + wMid + wR, x0 = (W - total) / 2;

  s.setFreeFont(&FreeSerifBold24pt7b);
  outlined(s, "S", x0, base1, c, glow);
  outlined(s, "R", x0 + wS + wMid, base1, c, glow);
  s.setFreeFont(&FreeSerifBold18pt7b);
  outlined(s, "TRANGE", x0 + wS, base1, c, glow);
  outlinedBar(s, x0 + wS + 1, x0 + wS + wMid - 1, base1 - hMid - 6, c, glow);   // the bar on top

  s.setFreeFont(&FreeSerifBold12pt7b);
  int wT = s.textWidth("THINGS"), hT = glyphHeight(&FreeSerifBold12pt7b, 'T');
  int tx = (W - wT) / 2;
  outlined(s, "THINGS", tx, base2, c, glow);
  int barY = base2 - hT / 2 - 1;
  outlinedBar(s, x0 + 4, tx - 8, barY, c, glow);                                  // bars beside THINGS
  outlinedBar(s, tx + wT + 8, x0 + total - 4, barY, c, glow);
}

void titleCard() {
  mood = INTRO;
  frame.fillSprite(TFT_BLACK); frame.pushSprite(0, 0);
  delay(600);
  // fade in, like an old neon sign warming up
  for (int lvl = 0; lvl <= 255; lvl += 3) {
    introLevel = lvl / 255.0f;
    int l = (random(100) < 10) ? lvl / 3 : lvl;          // random flicker
    drawTitle(frame, rgb(l, l / 14, l / 14), rgb(l / 4, 0, 0));
    frame.pushSprite(0, 0);
    neopixelWrite(LED_PIN, l / 4, 0, 0);
    delay(30);
  }
  // wait for a key, with a blinking hint
  uint32_t t0 = millis();
  for (;;) {
    bool blink = ((millis() - t0) / 600) % 2;
    int l = (random(100) < 3) ? 120 : 255;
    drawTitle(frame, rgb(l, l / 14, l / 14), rgb(70, 0, 0));
    frame.setTextFont(1); frame.setTextDatum(BC_DATUM);
    frame.setTextColor(blink ? rgb(150, 150, 150) : rgb(60, 60, 60));
    frame.drawString("press any key, " KID_NAME, W / 2, H - 4);
    frame.pushSprite(0, 0);
    if (keyPressed()) break;
    delay(30);
  }
}

// =============================== THE WORLDS ==================================
const int NBULBS = 12;
int bulbX(int i) { return 10 + i * 20; }
int bulbY(int i) { return 7 + (int)(12 * sinf(PI * bulbX(i) / W)); }
const uint16_t BULB_RGB[5][3] = {{255, 40, 30}, {40, 220, 60}, {60, 120, 255}, {255, 210, 40}, {255, 120, 20}};

void drawWorld(bool upside) {
  // sky: a smooth fade from top to bottom
  for (int y = 0; y < H; y++) {
    float f = y / (float)H;
    if (upside) bg.drawFastHLine(0, y, W, rgb(15 + 50 * f, 2 + 10 * f, 4 + 10 * f));
    else        bg.drawFastHLine(0, y, W, rgb(4 + 20 * f, 6 + 30 * f, 28 + 50 * f));
  }
  randomSeed(42);
  if (!upside) for (int i = 0; i < 40; i++) bg.drawPixel(random(W), random(90), rgb(200, 200, 230));

  bg.fillCircle(198, 34, 11, upside ? rgb(120, 30, 30) : rgb(240, 230, 180));          // moon
  bg.fillRect(0, 112, W, H - 112, upside ? rgb(25, 18, 20) : rgb(10, 38, 16));          // ground

  uint16_t tree = upside ? rgb(10, 5, 6) : rgb(4, 22, 9);                               // pine trees
  int treeX[] = {14, 42, 205, 228};
  for (int x : treeX) {
    for (int k = 0; k < 3; k++) bg.fillTriangle(x - 14 + k * 3, 114 - k * 14, x + 14 - k * 3, 114 - k * 14, x, 88 - k * 14, tree);
  }

  uint16_t wall = upside ? rgb(32, 26, 28) : rgb(48, 36, 42);                           // the house
  bg.fillRect(82, 74, 68, 40, wall);
  bg.fillTriangle(76, 75, 156, 75, 116, 50, upside ? rgb(20, 14, 16) : rgb(30, 20, 26));
  bg.fillRect(134, 54, 7, 14, wall);                                                    // chimney
  bg.fillRect(92, 84, 15, 13, upside ? rgb(18, 8, 8) : rgb(255, 200, 80));             // window
  bg.drawFastVLine(99, 84, 13, wall); bg.drawFastHLine(92, 90, 15, wall);
  bg.fillRect(124, 92, 13, 22, upside ? rgb(14, 8, 8) : rgb(70, 40, 22));               // door

  for (int i = 0; i < NBULBS - 1; i++)                                                  // light wire
    bg.drawLine(bulbX(i), bulbY(i), bulbX(i + 1), bulbY(i + 1), rgb(20, 25, 20));

  if (upside) {
    // flip the whole picture upside down, row by row
    uint16_t* p = (uint16_t*)bg.getPointer();
    static uint16_t tmp[W];
    for (int y = 0; y < H / 2; y++) {
      memcpy(tmp, p + y * W, W * 2);
      memcpy(p + y * W, p + (H - 1 - y) * W, W * 2);
      memcpy(p + (H - 1 - y) * W, tmp, W * 2);
    }
    // creepy vines growing down from the (now upside down) ground
    for (int v = 0; v < 11; v++) {
      float x = random(W), y = random(10, 24), a = PI / 2 + (random(100) - 50) / 80.0f;
      for (int k = 0; k < 30; k++) {
        float nx = x + 3 * cosf(a), ny = y + 3 * sinf(a);
        bg.drawLine(x, y, nx, ny, rgb(70, 8, 14));
        bg.drawLine(x + 1, y, nx + 1, ny, rgb(35, 4, 8));
        x = nx; y = ny; a += (random(100) - 50) / 120.0f;
      }
    }
  }
  randomSeed(esp_random());
}

// ============================== THE DEMOGORGON ===============================
// open: 0 = head closed like a bud, 1 = petals wide open.  flip: draw it upside down.
void thick(TFT_eSprite& s, int x1, int y1, int x2, int y2, uint16_t c) {
  s.drawLine(x1, y1, x2, y2, c); s.drawLine(x1 + 1, y1, x2 + 1, y2, c);
}

void drawDemogorgon(TFT_eSprite& s, int x, float open, float sway, bool flip) {
  auto Y = [&](float y) { return flip ? (H - 1 - (int)y) : (int)y; };
  uint16_t skin = rgb(165, 150, 145), dark = rgb(75, 62, 62);
  uint16_t mouth = rgb(215, 35, 55), tooth = TFT_WHITE;
  int sw = (int)sway;

  // legs: hip -> knee -> foot
  thick(s, x - 3, Y(88), x - 6, Y(101), dark); thick(s, x - 6, Y(101), x - 4, Y(113), dark);
  thick(s, x + 2, Y(88), x + 5, Y(101), dark); thick(s, x + 5, Y(101), x + 3, Y(113), dark);
  s.drawLine(x - 7, Y(113), x - 2, Y(113), dark); s.drawLine(x + 2, Y(113), x + 7, Y(113), dark);
  // skinny body
  s.fillTriangle(x - 6, Y(72), x + 6, Y(72), x - 3, Y(90), skin);
  s.fillTriangle(x + 6, Y(72), x + 3, Y(90), x - 3, Y(90), skin);
  s.drawLine(x, Y(74), x, Y(88), dark);
  // long arms with claws
  for (int side = -1; side <= 1; side += 2) {
    int ex = x + side * 10 + sw, hx = x + side * 9 + sw;
    thick(s, x + side * 6, Y(73), ex, Y(86), skin);
    thick(s, ex, Y(86), hx, Y(99), skin);
    for (int c = -1; c <= 1; c++) s.drawLine(hx, Y(99), hx + c * 2, Y(104), dark);
  }
  // neck
  thick(s, x, Y(72), x, Y(66), skin);
  // the flower head: 5 petals that spread open
  for (int i = 0; i < 5; i++) {
    float a = -PI / 2 + (i - 2) * open * 0.8f;
    float tx = x + 13 * cosf(a), ty = 65 + 13 * sinf(a);
    float px = 3.5f * cosf(a + PI / 2), py = 3.5f * sinf(a + PI / 2);
    s.fillTriangle(x + px, Y(65 + py), x - px, Y(65 - py), tx, Y(ty), skin);
    s.drawLine(x, Y(65), tx, Y(ty), dark);
  }
  if (open > 0.3f) {
    int r = 2 + (int)(3 * open);
    s.fillCircle(x, Y(64), r, mouth);
    for (int k = 0; k < 8; k++) s.drawPixel(x + (r + 1) * cosf(k * PI / 4), Y(64 + (r + 1) * sinf(k * PI / 4)), tooth);
  }
}

// ================================ WILL =======================================
// Will crouches behind the pine tree at x=42. peek: 0 = hidden, 1 = leaning out.
void drawTree(TFT_eSprite& s, int x, uint16_t c, bool flip) {
  auto Y = [&](int y) { return flip ? H - 1 - y : y; };
  for (int k = 0; k < 3; k++)
    s.fillTriangle(x - 14 + k * 3, Y(114 - k * 14), x + 14 - k * 3, Y(114 - k * 14), x, Y(88 - k * 14), c);
}

void drawWill(TFT_eSprite& s, float peek, bool flip) {
  // rect in normal (not flipped) coordinates; flips it if we're in the Upside Down
  auto box = [&](int x, int y, int w, int h, uint16_t c) { s.fillRect(x, flip ? H - y - h : y, w, h, c); };
  auto Y = [&](int y) { return flip ? H - 1 - y : y; };
  int x = 50 + (int)(peek * WILL_PEEK);           // half behind the tree, leaning out to watch
  uint16_t skin = rgb(240, 200, 170), hair = rgb(150, 95, 50);
  uint16_t jacket = rgb(80, 130, 200), jeans = rgb(60, 70, 130);

  if (peek > 0.4f)                                 // flashlight beam (off when he hides!)
    s.fillTriangle(x + 10, Y(101), x + 55, Y(93), x + 55, Y(110), rgb(120, 110, 45));
  box(x - 4, 108, 9, 6, jeans);                    // crouched legs
  box(x - 4, 99, 9, 9, jacket);                    // jacket
  s.fillCircle(x, Y(94), 4, skin);                 // head
  box(x - 4, 89, 9, 4, hair);                      // bowl haircut
  box(x - 4, 93, 1, 3, hair); box(x + 4, 93, 1, 3, hair);
  s.drawPixel(x + 2, Y(94), TFT_BLACK);            // eye, looking at the Demogorgon
  box(x + 4, 101, 3, 2, skin);                     // hand
  box(x + 6, 100, 4, 3, rgb(160, 160, 160));       // flashlight
  drawTree(s, 42, rgb(10, 5, 6), flip);            // tree in front, so he's hiding behind it
}

// ============================ THE SEARCH PARTY ================================
// Four friends walk the street at night with flashlights, looking for Will.
Friend friends[4] = {
  {ELEVEN, 120, 1, 0.45f}, {MIKE, 40, 1, 0.6f}, {DUSTIN, 200, -1, 0.5f}, {LUCAS, 160, -1, 0.7f},
};

void drawFriend(TFT_eSprite& s, const Friend& f, bool talking) {
  int x = (int)f.x, d = f.dir, step = ((int)(f.x / 3)) % 2 ? 1 : -1;
  uint16_t skin = rgb(240, 200, 170), shirt, pants, hair;
  switch (f.look) {
    case ELEVEN: shirt = rgb(240, 120, 165); pants = skin;               hair = rgb(95, 70, 50);  break;
    case MIKE:   shirt = rgb(40, 140, 90);   pants = rgb(50, 60, 120);   hair = rgb(25, 18, 12);  break;
    case DUSTIN: shirt = rgb(230, 190, 60);  pants = rgb(110, 80, 50);   hair = rgb(135, 90, 50); break;
    case LUCAS:  shirt = rgb(205, 120, 50);  pants = rgb(50, 60, 95);    hair = rgb(20, 15, 10);
                 skin = rgb(125, 82, 55); break;
  }
  int hx = x + 5 * d;                                              // hand with the flashlight
  s.fillTriangle(hx + 4 * d, 114, hx + 42 * d, 106, hx + 42 * d, 123, rgb(110, 105, 50));   // beam
  thick(s, x - 2, 120, x - 2 - step, 127, pants);                  // legs (they swap = walking!)
  thick(s, x + 1, 120, x + 1 + step, 127, pants);
  if (f.look == ELEVEN) s.fillRect(x - 5, 110, 11, 12, shirt);     // Eleven's dress
  else                  s.fillRect(x - 4, 110, 9, 10, shirt);
  s.fillRect(hx, 113, 4, 3, rgb(160, 160, 160));                   // flashlight
  s.fillCircle(x, 104, 4, skin);                                   // head
  s.drawPixel(x + 2 * d, 104, TFT_BLACK);                          // eye, looking where they walk
  switch (f.look) {
    case ELEVEN: s.drawFastHLine(x - 3, 100, 7, hair); break;      // buzz cut
    case MIKE:   s.fillRect(x - 4, 99, 9, 3, hair); s.fillRect(x - 4 * d, 102, 1, 4, hair); break;
    case DUSTIN: s.fillRect(x - 4, 99, 9, 3, rgb(40, 70, 170));     // cap...
                 s.fillRect(d > 0 ? x + 3 : x - 7, 101, 5, 1, rgb(40, 70, 170));   // ...with a brim
                 s.fillRect(x - 5 * d, 102, 2, 4, hair); break;     // curls sticking out
    case LUCAS:  s.fillRect(x - 4, 99, 9, 2, hair);
                 s.fillRect(x - 4, 101, 9, 2, rgb(85, 105, 55)); break;   // bandana
  }
  if (talking) {                                                   // shout for Will
    s.setTextFont(1); s.setTextDatum(BC_DATUM);
    s.setTextColor(TFT_WHITE);
    s.drawString("Will?", x, 97);
  }
}

void drawSearchParty(TFT_eSprite& s, uint32_t n) {
  static int caller = 0;
  if (n % CALL_EVERY == 0) caller = random(4);
  bool calling = (n % CALL_EVERY) < 40;
  for (int i = 0; i < 4; i++) {
    Friend& f = friends[i];
    f.x += f.dir * f.speed * FRIEND_SPEED;
    if (f.x < 10)  f.dir = 1;                                      // turn around at the edges
    if (f.x > 230) f.dir = -1;
    drawFriend(s, f, calling && i == caller);
  }
}

// how open the head is right now (closed most of the time, then snaps open)
float demoOpen() {
  uint32_t p = millis() % DEMO_EVERY, start = DEMO_EVERY - 1500;
  if (p < start) return 0;
  p -= start;
  if (p < 300) return p / 300.0f;
  if (p < 1200) return 1;
  return 1 - (p - 1200) / 300.0f;
}

struct Ash { float x, y, wob; } ash[ASH_COUNT];

void resetAsh() {
  for (auto& a : ash) { a.x = random(W); a.y = random(H); a.wob = random(628) / 100.0f; }
}

void drawFrame(uint32_t n) {
  memcpy(frame.getPointer(), bg.getPointer(), W * H * 2);

  if (!upsideDown) {
    // twinkly Christmas lights
    for (int i = 0; i < NBULBS; i++) {
      const uint16_t* c = BULB_RGB[i % 5];
      float b = (random(100) < BULB_FLICKER) ? 0.25f : 1.0f;
      frame.fillCircle(bulbX(i), bulbY(i) + 3, 5, rgb(c[0] * b / 4, c[1] * b / 4, c[2] * b / 4));
      frame.fillCircle(bulbX(i), bulbY(i) + 3, 2, rgb(c[0] * b, c[1] * b, c[2] * b));
    }
    drawSearchParty(frame, n);
    int i = (n / 8) % NBULBS;
    neopixelWrite(LED_PIN, BULB_RGB[i % 5][0] / 8, BULB_RGB[i % 5][1] / 8, BULB_RGB[i % 5][2] / 8);
  } else {
    // the lights are dead here... except one that sometimes flickers red
    for (int i = 0; i < NBULBS; i++) {
      bool alive = (i == 5) && random(100) < 30;
      frame.fillCircle(bulbX(i), H - 1 - bulbY(i) - 3, 2, alive ? rgb(255, 30, 20) : rgb(40, 20, 20));
    }
    // the Demogorgon, standing outside the house (upside down, like everything here)
    float open = demoOpen();
    growl = open;
    // IF the Demogorgon opens its head, THEN Will hides. ELSE he peeks out, slowly.
    static float peek = 0;
    float want = (open > 0) ? 0 : 0.6f + 0.4f * sinf(n * 0.03f);
    peek += (open > 0 ? 0.4f : 0.05f) * (want - peek);
    drawWill(frame, peek, true);
    drawDemogorgon(frame, DEMO_X, open, 2 * sinf(n * 0.05f), true);
    // floating ash
    for (auto& a : ash) {
      a.y -= ASH_SPEED; a.wob += 0.05f;
      if (a.y < 0) { a.y = H; a.x = random(W); }
      frame.fillRect(a.x + 3 * sinf(a.wob), a.y, 2, 2, rgb(170, 160, 160));
    }
    // red lightning
    if (random(LIGHTNING_CHANCE) == 0) {
      int x = random(30, W - 30), y = 0;
      while (y < H) { int nx = x + random(-12, 13), ny = y + random(8, 20);
        frame.drawLine(x, y, nx, ny, rgb(255, 120, 120)); frame.drawLine(x + 1, y, nx + 1, ny, rgb(255, 40, 40));
        x = nx; y = ny; }
      neopixelWrite(LED_PIN, 255, 30, 30);
    } else if (open > 0.5f) {
      neopixelWrite(LED_PIN, 200, 0, 20);          // its head is open: LED goes bright red
    } else {
      int pulse = 20 + 18 * sinf(n * 0.08f);
      neopixelWrite(LED_PIN, pulse, 0, 0);
    }
  }
}

// rip between the worlds: rows slide sideways, static, flashes
void glitch() {
  mood = WHOOSH;
  for (int f = 0; f < GLITCH_FRAMES; f++) {
    drawFrame(f);
    uint16_t* p = (uint16_t*)frame.getPointer();
    for (int k = 0; k < 10; k++) {
      int y0 = random(H), hgt = random(3, 18), off = random(-40, 41);
      for (int y = y0; y < min(H, y0 + hgt); y++) {
        uint16_t* row = p + y * W;
        if (off > 0) memmove(row + off, row, (W - off) * 2); else memmove(row, row - off, (W + off) * 2);
      }
    }
    for (int k = 0; k < 6; k++) frame.drawFastHLine(0, random(H), W, random(2) ? TFT_WHITE : rgb(255, 0, 40));
    frame.pushSprite(0, 0);
    neopixelWrite(LED_PIN, random(2) * 255, 0, random(2) * 60);
    delay(40);
  }
  frame.fillSprite(upsideDown ? rgb(255, 60, 60) : TFT_WHITE);   // flash!
  frame.pushSprite(0, 0);
  delay(60);
}

// ================================= MAIN ======================================
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  bg.setColorDepth(16);    frame.setColorDepth(16);
  if (!bg.createSprite(W, H) || !frame.createSprite(W, H)) {
    tft.drawString("oops: out of memory", 10, 10, 2);
    for (;;) delay(1000);
  }
  keyboardBegin();
  soundBegin();
  resetAsh();

  titleCard();

  glitch();
  drawWorld(false);
  mood = NIGHT;
  Serial.println("Welcome to Hawkins. Press any key to flip.");
}

void loop() {
  static uint32_t n = 0;
  if (keyPressed()) {
    upsideDown = !upsideDown;
    glitch();
    drawWorld(upsideDown);
    mood = upsideDown ? UPSIDE : NIGHT;
    growl = 0;
    Serial.println(upsideDown ? "You are in the Upside Down." : "Back home. Phew.");
  }
  drawFrame(n++);
  frame.pushSprite(0, 0);
  delay(30);
}

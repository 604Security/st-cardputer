// Cardputer emulator: runs the real upside_down sketch on the laptop with a fake clock and a scripted
// key press, and saves the screen at chosen moments as sim/out/<name>.ppm (+ .led with the LED colour).
#define SIM
#include "../firmware/upside_down/upside_down.ino"

static uint32_t now = 0;
static int ledR, ledG, ledB;
struct Shot { uint32_t at; const char* name; bool done; };
static Shot shots[] = {
  {3900,  "1-title", false},
  {6600,  "3-night", false},
  {10600, "4-upside-down", false},
  {13900, "5-demogorgon", false},
};
// times the G0 button is held down (ms): leave the title, then flip to the Upside Down
static const uint32_t presses[][2] = {{4200, 4300}, {9000, 9100}};

uint32_t millis() { return now; }
int digitalRead(int pin) {
  if (pin != 0) return HIGH;
  for (auto& p : presses) if (now >= p[0] && now < p[1]) return LOW;
  return HIGH;
}
void neopixelWrite(int, int r, int g, int b) { ledR = r; ledG = g; ledB = b; }

static void save(const char* name) {
  char path[256];
  snprintf(path, sizeof path, "sim/out/%s.ppm", name);
  FILE* f = fopen(path, "wb");
  fprintf(f, "P6 %d %d 255\n", tft.w, tft.h);
  for (int i = 0; i < tft.w * tft.h; i++) {
    uint16_t c = tft.buf[i];
    uint8_t px[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
    fwrite(px, 1, 3, f);
  }
  fclose(f);
  snprintf(path, sizeof path, "sim/out/%s.led", name);
  f = fopen(path, "w"); fprintf(f, "%d %d %d\n", ledR, ledG, ledB); fclose(f);
  fprintf(stderr, "[sim] saved %s at %u ms\n", name, now);
}

// take a picture right after the first full frame drawn at or after each shot's time
void simScreenUpdated() {
  for (auto& s : shots) if (!s.done && now >= s.at) { save(s.name); s.done = true; }
}
void delay(uint32_t ms) {
  now += ms;
  if (now > 15000) exit(0);
}

int main() {
  setup();
  for (;;) loop();
}

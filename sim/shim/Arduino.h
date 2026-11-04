// Laptop stand-in for the Arduino core: fake clock, fake keyboard, no hardware.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
using std::min; using std::max;
#define PROGMEM
#define PI 3.14159265358979f
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1
#define portMAX_DELAY 0xffffffff
typedef uint8_t byte;

uint32_t millis();
void delay(uint32_t ms);                  // advances the fake clock (and takes pictures)
inline void delayMicroseconds(uint32_t) {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
int digitalRead(int pin);                 // pin 0 (G0 button) is "pressed" at scripted times
void neopixelWrite(int pin, int r, int g, int b);
inline uint32_t esp_random() { return (uint32_t)rand(); }
inline void randomSeed(uint32_t s) { srand(s); }
inline long random(long hi) { return hi > 0 ? rand() % hi : 0; }
inline long random(long lo, long hi) { return hi > lo ? lo + rand() % (hi - lo) : lo; }
inline void xTaskCreatePinnedToCore(void (*)(void*), const char*, int, void*, int, void*, int) {}  // no sound in the sim

struct FakeSerial {
  void begin(int) {}
  void println(const char* s) { fprintf(stderr, "[serial] %s\n", s); }
};
static FakeSerial Serial;

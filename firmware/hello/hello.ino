// Cardputer "hello" test: proves we can build + flash the Cardputer.
#include <TFT_eSPI.h>
#if __has_include("config.h")
#include "config.h"
#endif
#ifndef KID_NAME
#define KID_NAME "friend"
#endif

TFT_eSPI tft;

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);            // landscape, 240x135
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Hello " KID_NAME "!", 120, 55, 4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("the Cardputer is listening...", 120, 95, 2);
  Serial.println("Hello " KID_NAME "!");
}

void loop() {
  // a little flicker on the last line, just to show it's alive
  static bool on = true;
  tft.setTextColor(on ? TFT_RED : TFT_BLACK, TFT_BLACK);
  tft.drawString("_", 120, 118, 2);
  on = !on;
  delay(random(150, 700));
}

// M5Stack Cardputer screen: ST7789V2, 1.14" 240x135, on the StampS3 (ESP32-S3).
// Used with TFT_eSPI via -include (no editing the library's User_Setup), like firmware/cyd/screens/.
#pragma once
#define USER_SETUP_INFO "Cardputer / ST7789 240x135"
#define ST7789_DRIVER
#define TFT_WIDTH  135
#define TFT_HEIGHT 240
#define TFT_MOSI 35
#define TFT_SCLK 36
#define TFT_CS   37
#define TFT_DC   34
#define TFT_RST  33
#define TFT_BL   38
#define TFT_BACKLIGHT_ON 1
#define TFT_MISO -1
#define USE_HSPI_PORT
#define TFT_INVERSION_ON
#define TFT_RGB_ORDER TFT_BGR
#define SPI_FREQUENCY 40000000
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF

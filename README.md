# ST Cardputer

Sci-fi experiments on an M5Stack Cardputer (ESP32-S3, 240x135 screen, keyboard, speaker, mic, RGB LED).

## Setup
- arduino-cli with the esp32 core 2.0.17 and TFT_eSPI 2.5.43
- `cp firmware/hello/config.h.example firmware/hello/config.h` and put your name in it

## Build and flash
```
make flash SKETCH=firmware/hello
```

Credits: the ST crew (ideas and code) with DFIR-life (mentor).

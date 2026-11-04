# ST Cardputer build helpers (arduino-cli, shares config with Arduino IDE 2.x, esp32 core 2.0.17)
#   make flash SKETCH=firmware/hello
CLI    ?= $(HOME)/tools/arduino-cli/arduino-cli --config-file $(HOME)/.arduinoIDE/arduino-cli.yaml
FQBN   ?= esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=8M
PORT   ?= /dev/ttyACM0
SKETCH ?= firmware/hello
SETUP   = $(CURDIR)/firmware/screens/cardputer.h
FLAGS   = --build-property "compiler.cpp.extra_flags=-DUSER_SETUP_LOADED=1 -include $(SETUP)" \
          --build-property "compiler.c.extra_flags=-DUSER_SETUP_LOADED=1 -include $(SETUP)"
# esptool's stub loader can drop the native USB link; the ROM loader is reliable
UPLOAD_FLAGS ?= --upload-property upload.flags=--no-stub

.PHONY: build flash monitor
build:
	$(CLI) compile --fqbn $(FQBN) $(FLAGS) $(SKETCH)
flash: build
	$(CLI) upload --fqbn $(FQBN) -p $(PORT) $(UPLOAD_FLAGS) $(SKETCH)
monitor:
	$(CLI) monitor -p $(PORT) -c baudrate=115200

# Emulator: runs the real upside_down sketch on the laptop and saves pictures (needs g++ and Python PIL)
#   make sim   -> docs/screenshots/cardputer-*.png
SIM_FLAGS = -std=gnu++17 -O1 -w -Isim/shim -I$(HOME)/Arduino/libraries/TFT_eSPI
.PHONY: sim
sim:
	@mkdir -p sim/out
	g++ $(SIM_FLAGS) sim/main.cpp -o sim/out/sim
	./sim/out/sim
	python3 sim/to_png.py sim/out docs/screenshots

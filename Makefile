# Raylib with Wayland on Fedora
CC      = gcc
CFLAGS  = -Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces -MMD -MP -I.
BUILD   = build
TARGET  = $(BUILD)/ai-avatar

# main.c in the root plus every .c file one folder down
SRC = $(wildcard *.c) $(wildcard */*.c)
OBJ = $(SRC:%.c=$(BUILD)/%.o)
DEP = $(OBJ:.o=.d)

# Libraries needed for Wayland support
LIBS = -lraylib -lwayland-client -lwayland-cursor -lwayland-egl -lxkbcommon -lGL -lm -lpthread -ldl -lrt

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LIBS)

# Creates the matching folder inside build before compiling each file
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

# Run the desktop app alone. It finds the UNO by itself if one is plugged in
desktop: $(TARGET)
	./$(TARGET)

# Arduino sketch for the UNO + OLED
FQBN   = arduino:avr:uno
SKETCH = sketch
PORT  ?= $(firstword $(wildcard /dev/ttyACM* /dev/ttyUSB*))

compile:
	arduino-cli compile --fqbn $(FQBN) --build-path $(BUILD)/sketch $(SKETCH)

# A missing board is not an error, the desktop app works without it
upload: compile
	@if [ -z "$(PORT)" ]; then echo "No board found, skipping upload"; \
	else arduino-cli upload --fqbn $(FQBN) -p $(PORT) --input-dir $(BUILD)/sketch $(SKETCH) \
	     || echo "Upload failed, continuing without the board"; fi

# Compile the sketch, upload it, then start the desktop app
run: upload
	$(MAKE) --no-print-directory desktop

# Pull in the generated header dependency files
-include $(DEP)

.PHONY: all clean run desktop compile upload

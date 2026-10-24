# Raylib with Wayland on Fedora
CC      = gcc
CFLAGS  = -Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces -MMD -MP
BUILD   = build
TARGET  = $(BUILD)/ai-avatar

SRC = $(wildcard *.c)
OBJ = $(SRC:%.c=$(BUILD)/%.o)
DEP = $(OBJ:.o=.d)

# Libraries needed for Wayland support
LIBS = -lraylib -lwayland-client -lwayland-cursor -lwayland-egl -lxkbcommon -lGL -lm -lpthread -ldl -lrt

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LIBS)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

run: $(TARGET)
	./$(TARGET)

# Pull in the generated header dependency files
-include $(DEP)

.PHONY: all clean run

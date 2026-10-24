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

run: $(TARGET)
	./$(TARGET)

# Pull in the generated header dependency files
-include $(DEP)

.PHONY: all clean run

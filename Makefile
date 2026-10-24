# Simple Makefile for Raylib with Wayland on Fedora
CC = gcc
CFLAGS = -Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces
TARGET = ai-avatar
SRC = main.c

# Libraries needed for Wayland support
LIBS = -lraylib -lwayland-client -lwayland-cursor -lwayland-egl -lxkbcommon -lGL -lm -lpthread -ldl -lrt

# Default build rule
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LIBS)

# Clean up build files
clean:
	rm -f $(TARGET)

# Run the application
run: $(TARGET)
	./$(TARGET)

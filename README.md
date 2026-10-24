# AI Avatar

Robot-face animations written in C with [raylib](https://www.raylib.com/). The 14 animations play in a desktop window and can also be mirrored on a 0.96" SSD1306 OLED connected to an Arduino UNO. A pushbutton on the UNO steps through the animations.

## Demo

<video src="video.mp4" controls muted width="100%"></video>

If the video does not play, [open video.mp4](video.mp4).

## Animations

Idle, Listening, Thinking, Speaking, Happy, Error, Sleep, Orb Thinking, Fireworks, Radar, Rings, Aurora, ASCII, Snow.

In the desktop window, click a button in the sidebar or press keys `1`-`9`.

## Hardware (optional)

| Part | UNO pin |
|------|---------|
| OLED SDA | A4 |
| OLED SCL | A5 |
| OLED VCC / GND | 5V / GND |
| Pushbutton | D2 and GND (internal pull-up) |

The OLED I2C address is `0x3C`.

## Requirements

- gcc, make, raylib (the Makefile links the Wayland libraries)
- [arduino-cli](https://arduino.github.io/arduino-cli/) with the `arduino:avr` core
- Arduino libraries: Adafruit GFX Library, Adafruit SSD1306
- Permission to use the serial port (on most Linux systems, membership in the `dialout` group)

## Usage

```
make run       # compile the sketch, upload it, then start the desktop app
make desktop   # start the desktop app only
make compile   # compile the sketch only
make upload    # compile and upload the sketch only
make clean     # remove build/
```

- The serial port is auto-detected (`/dev/ttyACM*` or `/dev/ttyUSB*`). Override it with `make run PORT=/dev/ttyACM1`.
- `make run` stops if the sketch fails to compile. If the upload fails or no board is found, it prints a message and starts the desktop app anyway.
- Set `OLED_PORT=/dev/ttyXXX` to make the desktop app use a specific port.

## How the OLED works

The UNO has too little RAM to run the animations, so the desktop app does the drawing:

1. Each frame is rendered off screen, shrunk to 128x64 with dithering, and sent over serial (1,000,000 baud, 1024 bytes per frame).
2. The sketch writes the bytes straight into the display buffer and shows them.
3. Bytes sent back by the board: `H` hello (until the first frame), `R` ready for the next frame, `B` button pressed.

When the button is pressed, the desktop app moves to the next animation, so the window and the OLED always match. If no board answers within about 4 seconds, the app runs on its own and retries every 3 seconds. Unplugging the board while running is handled too.

Notes:
- Expect about 12 FPS on the OLED.
- The 4:3 frame is centered on the 2:1 panel with black bars at the sides.
- If the display shows garbage, lower the baud rate: `BAUD` in `sketch/sketch.ino` and `B1000000` in `oled/oled.c`.

## Project layout

```
main.c            window, sidebar, animation loop
common/           shared face type and drawing helpers
face_*/ ...       one folder per animation (each exports an Animation)
oled/             serial link and image conversion for the UNO
sketch/sketch.ino UNO firmware
```

To add an animation, create a folder with a `.c` and `.h` exporting an `Animation`, then add it to `ANIMS` in `main.c`. The Makefile picks up new `.c` files automatically.

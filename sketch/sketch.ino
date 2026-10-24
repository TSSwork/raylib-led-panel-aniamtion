#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// The desktop app draws the animations and streams each 128x64 frame over USB serial.
// This board shows the frames and reports button presses back.
//   board -> PC:  'H' hello (until the first frame), 'R' ready for a frame, 'B' button pressed
//   PC -> board:  1024 bytes, exactly the SSD1306 frame buffer

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define BUTTON_PIN 2
#define BAUD 1000000UL
#define FRAME_BYTES (SCREEN_WIDTH * SCREEN_HEIGHT / 8)

Adafruit_SSD1306 display(
  SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET
);

bool lastReading = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 40;

bool gotFrame = false;
unsigned long lastHello = 0;

void checkButton() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      // INPUT_PULLUP means LOW = button pressed
      if (buttonState == LOW) Serial.write('B');
    }
  }

  lastReading = reading;
}

// Reads one whole frame straight into the display buffer.
void receiveFrame() {
  uint8_t *buf = display.getBuffer();
  unsigned long lastByte = millis();
  int got = 0;

  while (got < FRAME_BYTES) {
    if (Serial.available()) {
      buf[got++] = Serial.read();
      lastByte = millis();
    } else if (millis() - lastByte > 200) {
      return;  // PC went quiet, drop the partial frame
    }
  }

  display.display();
  gotFrame = true;
  Serial.write('R');
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(BAUD);
  Wire.begin();
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    while (true) {}
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Waiting for PC...");
  display.display();
}

void loop() {
  checkButton();

  if (Serial.available()) {
    receiveFrame();
  } else if (!gotFrame && millis() - lastHello > 300) {
    lastHello = millis();
    Serial.write('H');
  }
}

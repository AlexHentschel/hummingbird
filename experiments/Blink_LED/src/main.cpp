/* HARDWARE: Arduino Nano ESP32
 * GOAL: blink LED
 * based on tutorial https://RandomNerdTutorials.com/vs-code-pioarduino-ide-esp32/
 * */

#include <Arduino.h>

// built-in RGB-LED DL1:
// --------------------------------------------------------
//  - GPIO0  Blue sub-LED
//  - GPIO45 Green sub-LED
//  - GPIO46 Red sub-LED
// blink the red channel of the RGB LED (active‑low)
// IMPORTANT: LOW = on, HIGH = off

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
}

void loop() {
  Serial.println("LED is red");
  digitalWrite(LED_RED, LOW); // on
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);

  delay(600);

  Serial.println("LED is green");
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, LOW); // on
  digitalWrite(LED_BLUE, HIGH);

  delay(600);

  Serial.println("LED is blue");
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, LOW); // on

  delay(600);
}

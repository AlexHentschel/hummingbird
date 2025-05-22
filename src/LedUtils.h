#pragma once
#include <Arduino.h>

class LEDToggler {

  // This class toggles a GPIO Pin (uint8_t for ESP32 - style GPIOx) on and off
  // throughout a specified interval. Each time toggleLED() is called, the internal
  // time reference `lastTriggerObservedMilli` is set to the current time milliseconds.
  // Throughout the interval `intervalMs` thereafter, the LED is allowed to be toggled
  // between on and off and when exceeding the interval, the LED is turned off.
  // To produce human-visible blinking, a `toggleIntervalMs` specifies the number of
  // milliseconds after which the output is alternated between on <-> off.
  //
  // This is intended to run on the controller loop, consuming minimla resoueces.
  // Results should be largely deterministic accross different controllers as we
  // don't rely on CPU frequency

  public:
  LEDToggler(uint8_t pin, unsigned long lifetimeMs, unsigned long toggleIntervalMs, bool highIsOn); // explicit on/off logic

  void toggleLED();
  void trigger();
  void expire();
  bool isExpired();

  static const bool HIGH_IS_ON; // Indicates that GPIO state HIGH means LED is on
  static const bool LOW_IS_ON;  // Indicates that GPIO state LOW means LED is on (modus operandi for build-in LEDs in Arduino Nano EPS32)

  private:
  void setLedOn();
  void setLedOff();

  // behavioral parameters are lifetime-constants (provided at construction)
  const uint8_t pin;
  const unsigned long lifetimeMs;
  const unsigned long toggleIntervalMs;
  const bool highIsOn;

  // dynamic state parameters
  unsigned long lastTriggerObservedMilli;
  bool expired;
};
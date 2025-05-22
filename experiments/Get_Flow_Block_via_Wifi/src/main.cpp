#include <Arduino.h>
#include <Arduino_JSON.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "WiFiCredentials.h"

/* ============================= SECTION: FUNCTION PROTOTYPES ============================= */
void connectWifi();

/* ================================== CONTROLLER SETUP ================================== */
void setup() {
  Serial.begin(115200);
  pinMode(LED_BLUE, OUTPUT);  // GPIO  0: Blue sub-LED;  LOW = on, HIGH = off
  pinMode(LED_GREEN, OUTPUT); // GPIO 45: Green sub-LED; LOW = on, HIGH = off
  pinMode(LED_RED, OUTPUT);   // GPIO 46: Red sub-LED;   LOW = on, HIGH = off

  delay(1000);
  connectWifi();
}

/* ================================== CONTROLLER LOOP =================================== */
void loop() {
  bool success = false;

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://rest-testnet.onflow.org/v1/blocks?height=final");
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();

      JSONVar myObject = JSON.parse(payload);

      // CASE A: JSON.typeof(myObject) yields "undefined", then parsing failed,
      //         then blink red LED quickly four times
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
        success = false;
        digitalWrite(LED_BLUE, HIGH); // off
        for (int i = 0; i < 4; i++) { // blink red LED quickly four times
          digitalWrite(LED_RED, LOW); // on
          delay(200);
          digitalWrite(LED_RED, HIGH); // off
          delay(100);
        }
        return;
      }

      // CASE B: parsed JSON successfully
      success = true;
      digitalWrite(LED_BLUE, LOW); // on
      JSONVar value = myObject[0]["header"]["height"];

      Serial.print("block: ");
      Serial.println(value);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      digitalWrite(LED_BLUE, HIGH); // off
      success = false;
    }
    http.end(); // Free resources

    if (success) {
      digitalWrite(LED_BLUE, LOW); // on
    } else {
      digitalWrite(LED_BLUE, HIGH); // off
    }
  } else {
    Serial.println("WiFi Disconnected");
    connectWifi();
  }
}

/* ============================== BUSINESS LOGIC FUNCTIONS ============================== */

// FUNCTION connectWifi(): connects Wifi:
//  * while connecting Wifi, Led will be on red
//  * when this function returns, all LED's are off
void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_RED, HIGH);   // on
    digitalWrite(LED_BLUE, HIGH);  // off
    digitalWrite(LED_GREEN, HIGH); // off
    return;
  }

  Serial.println("WiFi Disconnected; Connecting ...\n   ");
  digitalWrite(LED_RED, LOW);    // on
  digitalWrite(LED_BLUE, HIGH);  // off
  digitalWrite(LED_GREEN, HIGH); // off
  delay(500);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(750);
    Serial.print(".");
  }
  Serial.println("connected");
  digitalWrite(LED_RED, HIGH); // off

  for (int i = 1; i < 5; i++) {   // blink red LED quickly four times
    digitalWrite(LED_GREEN, LOW); // on
    delay(100 * i);
    digitalWrite(LED_GREEN, HIGH); // off
    delay(100);
  }
  digitalWrite(LED_GREEN, LOW); // on
  delay(1000);
  digitalWrite(LED_GREEN, HIGH); // off
}

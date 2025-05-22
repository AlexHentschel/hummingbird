#include "mbedtls/base64.h" // bundled with ESP32‑Arduino core
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// custom utils
#include "LedUtils.h"
#include "OnChainState.h"

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

// WIFI
#include "WiFiCredentials.h"

/* Websockets - NEEDS HUMAN CONFIGURATION
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// Area of future work: at the moment, we don't check the certificate of the Access Node.
// This is reflected in the code by the line `sslClient.setInsecure();`.
// The AN's root certificate authority is the ISRG Root X1, wich we can import below.
// #include "isrg_root_x1.h"

/* Websocket Server configuration
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
// const char *host = "rest-mainnet.onflow.org";              // MAINNET server, only permits SSL connections
// const char *host = "rest-testnet.onflow.org"; // TESTNET server, only permits SSL connections
const char *host = "access-001.devnet52.nodes.onflow.org"; // TESTNET server, only permits plaintext connections!
const char *path = "/v1/ws";

// Set to 1 for SSL, 0 for plain text
#define USE_SSL 0

/* Flow Events we are interested in:
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴
 * Events:
 * • Event `A.8c5303eaa26202d6.EVM.BlockExecuted` is emitted once for *every block*, including empty ones.
 * • Every transaction needs to pay fees, hence `A.912d5440f7e3769e.FlowFees.FeesDeducted` is emitted once for *every transaction*.
 * • Event `A.0d3c8d02b02ceb4c.MicrocontrollerTest.ControlValueChanged` is emitted once for *every control value change*, i.e. only when
 *   a transaction interacts with the on-chain controller.
 */

const char *const EVENT_TYPES[] = {
    // "A.8c5303eaa26202d6.EVM.BlockExecuted",
    "A.0d3c8d02b02ceb4c.MicrocontrollerTest.ControlValueChanged"
    // Add more event types here as needed
};

/* CONTROLLER SETUP
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

/* Websocket client: global variables
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
#if USE_SSL
WiFiClientSecure sslClient;
const int port = 443;
#else
WiFiClient plainClient;
const int port = 8075;
#endif
Client *client = nullptr;

/* Insternal State of the Websocket client */
String wsBuffer = "";     // holds full message as it arrives
bool wsReceiving = false; // tracks whether we’re in a multi-frame message

/* Websocket client: global variables ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
bool supressRepeatedRSVWarnings = false;

/* LED Blinking patterns to indicate current state ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
LEDToggler *blueToggler = nullptr;  // blinks 5 times turning o1 second
LEDToggler *greenToggler = nullptr; // blinks once for 0.5s
LEDToggler *redToggler = nullptr;   // blinks 5 times turning o1 second

/* Controller Output for External Load -> GPIO
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// EXT_LOAD_SWITCH defines the GPIO that is used to controll an external load attached.
// EXT_LOAD_ON and EXT_LOAD_OFF define the states that correspond to the load being provided
// power or not. Here, we use the Solid State Relay [SSR] H3MB-052D from Ingenex, which
// connects its load pins on input HIGH
#define EXT_LOAD_SWITCH D6
#define EXT_LOAD_ON HIGH
#define EXT_LOAD_OFF LOW

/* Internal representation of the state
 * We are using an 'eventually consistent' approach here.
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
OnChainState *scriptExecuter = nullptr;
bool extLoadOn = false; // state of the external load

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* FUNCTION PROTOTYPES
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
void connectWifi();
void scriptReadControllerState();
void setControllerState(int64_t newValue);
void connectAndSubscribeWebsockets();
void sendWebSocketFrame(const String &payload);
bool readWebSocketFrame();
void processWebSocketMessage();
void processControlInstruction(const char *encodedPayload);

/* FRAMEWORK FUNCTION setup(): called by Arduino framework once at startup
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
void setup() {
  Serial.begin(115200);

  /* ── LEDs' blinking patterns to indicate current state ─────────── */
  blueToggler = new LEDToggler(LED_BLUE, 1350, 150, LEDToggler::LOW_IS_ON);  // blinks 5 times turning o1 second
  greenToggler = new LEDToggler(LED_GREEN, 500, 500, LEDToggler::LOW_IS_ON); // blinks once for 0.5s
  redToggler = new LEDToggler(LED_RED, 1400, 200, LEDToggler::LOW_IS_ON);    // blinks 5 times turning o1 second

  pinMode(LED_BLUE, OUTPUT);     // GPIO  0: Blue sub-LED;  LOW = on, HIGH = off
  digitalWrite(LED_BLUE, HIGH);  // off
  pinMode(LED_GREEN, OUTPUT);    // GPIO 45: Green sub-LED; LOW = on, HIGH = off
  digitalWrite(LED_GREEN, HIGH); // off
  pinMode(LED_RED, OUTPUT);      // GPIO 46: Red sub-LED;   LOW = on, HIGH = off
  digitalWrite(LED_RED, HIGH);   // on since wifi disconnected

  /* ── GIPO ─────────────────────────────────────────────────────── */
  pinMode(EXT_LOAD_SWITCH, OUTPUT);            // GIPO controlling the external load
  digitalWrite(EXT_LOAD_SWITCH, EXT_LOAD_OFF); // off by detault for safety
  delay(1000);                                 // for debugging, wait 1 second for serial monitor to connect

  scriptExecuter = new OnChainState("http://" + String(host) + ":8070/v1/");

  connectWifi();
  scriptReadControllerState();     // initial read the on-chain state via script execution
  connectAndSubscribeWebsockets(); // keep updates via websockets stream of events
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER LOOP ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* Frequency bound for controller reconnection attempts in Milliseconds
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
const unsigned long reconnectionAttemptIntervalMS = 2000; // at most attempt to re-establish connection every 2 seconds
unsigned long latestReconnectionAttempt = 0;              // timestamp of last reconnection attempt

void loop() {
  // If not connected, try to reconnect
  if (!client || !client->connected()) {
    unsigned long currentMS = millis();
    if (currentMS - latestReconnectionAttempt < reconnectionAttemptIntervalMS) {
      return; // don't attempt to reconnect yet
    }

    Serial.println(F("⚠️ Lost connection, attempting to reconnect..."));
    latestReconnectionAttempt = currentMS;
    client->stop(); // Ensure client is stopped before reconnecting
    delay(100);

    connectWifi();         // Optional: re-check WiFi connection
    redToggler->trigger(); // blink red LED to indicate reconnection attempt
    while (true) {
      delay(20);
      redToggler->toggleLED();
      if (redToggler->isExpired()) break;
    }
    scriptReadControllerState();     // initial read the on-chain state via script execution
    connectAndSubscribeWebsockets(); // keep updates via websockets stream of events
    return;
  }

  // business logic
  if (readWebSocketFrame()) {
    processWebSocketMessage();
  }

  // work through blinking cycles of LEDs, to make sure they eventually expire
  blueToggler->toggleLED();
  greenToggler->toggleLED();
  redToggler->toggleLED();
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ BUSINESS LOGIC FUNCTIONS ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

// FUNCTION connectWifi:
// connects to the Wifi using credentials specified in `WiFiCredentials.h`
//  * while connecting Wifi, Led will be on red
//  * when this function returns, all LED's are off
// If disconnected, this function turns on the red LED while trying to reconnect to Wifi. It
// blocks until the connection is established and then turns the red LED off and returns.
void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_RED, HIGH); // off
    return;
  }

  digitalWrite(LED_RED, LOW); // on
  delay(500);
  Serial.print(F("Connecting Wi‑Fi…"));
  latestReconnectionAttempt = millis();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; true; i++) { // wait for connection
    delay(750);
    if (WiFi.status() == WL_CONNECTED) break;
    if (i == 3) {
      i = 0;
    }
    if (i == 0) {
      Serial.print(F(".looking."));
    } else if (i == 1) {
      Serial.print(F(".for."));
      delay(200);
    } else if (i == 2) {
      Serial.print(F(".Wifi.."));
      delay(500);
    }
  }
  Serial.printf("\n📡 connected to Wi‑Fi '%s' with local IP %s\n\n", WiFi.SSID(), WiFi.localIP().toString().c_str());
  digitalWrite(LED_RED, HIGH); // off
}

/* Initial state recovery via script execution
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// FUNCTION setControllerState:
//  1. reads the latest sealed block from the Flow access node via a rest call
//  2. executes the script to read the on-chain state via script execution at the latest sealed block
void scriptReadControllerState() {
  Serial.print(F("📞 Reading on-chain state via script execution from "));
  Serial.println("'" + scriptExecuter->getURL() + "'");

  // 1. get latest sealed block
  const std::tuple<unsigned long, bool> optionalLatestSealedBlock = scriptExecuter->get_latest_sealed_block();
  if (!std::get<1>(optionalLatestSealedBlock)) {
    Serial.println(F("   ❌ Failed to get latest sealed block"));
    return;
  }
  unsigned long latestSealedBlock = std::get<0>(optionalLatestSealedBlock);
  Serial.printf("   latest sealed block: %lu\n", latestSealedBlock);

  // 2. retrieve on-chain state as of the latest sealed block
  const std::tuple<int64_t, bool> scriptResult = scriptExecuter->get_led_state_at_block(latestSealedBlock);
  if (!std::get<1>(scriptResult)) {
    Serial.println(F("   ❌ Read of on-chain state failed"));
    return;
  }
  const int64_t ledState = std::get<0>(scriptResult);
  setControllerState(ledState);
}

/* Websockets Prototol Implementation
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// FUNCTION connectAndSubscribeWebsockets:
// connects to the Flow access node and subscribes to the events topic
void connectAndSubscribeWebsockets() {
  if (client && client->connected()) {
    digitalWrite(LED_RED, HIGH); // off
    return;
  }
  latestReconnectionAttempt = millis();

#if USE_SSL
  sslClient.setInsecure(); // Accept all certs (insecure, but works for dev)
  client = &sslClient;
  Serial.println(F("🔐 Using SSL connection"));
#else
  client = &plainClient;
  Serial.println(F("🔓 Using plain-text connection"));
#endif

  Serial.printf("Connecting to websockets API of s:%d\n", host, port);
  if (!client->connect(host, port)) {
    Serial.println(F("❌ Connection to server failed!"));
    return;
  }

  // handshake and WebSocket HTTP upgrade request
  String req = String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Upgrade: websocket\r\n" +
               "Connection: Upgrade\r\n" +
               "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n" +
               "Sec-WebSocket-Version: 13\r\n" +
               "Origin: https://rest-testnet.onflow.org\r\n" +
               "\r\n";
  client->print(req);
  Serial.println(F("🛰️ Sent WebSocket handshake"));

  // Wait for response
  while (client->connected() && !client->available())
    delay(10);
  Serial.println(F("📩 Handshake response:"));
  while (client->available()) {
    String line = client->readStringUntil('\n');
    Serial.print(line);
    if (line == "\r") break;
  }
  Serial.println(F("💡 End of HTTP headers"));

  /* ── send subscription  ───────────────────────────────────────── */
  StaticJsonDocument<512> doc;
  doc["subscription_id"] = "20charIDStreamEvents";
  doc["action"] = "subscribe";
  doc["topic"] = "events";
  JsonObject args = doc.createNestedObject("arguments");
  args["heartbeat_interval"] = "5";

  size_t subscribedEventTypesCount = sizeof(EVENT_TYPES) / sizeof(EVENT_TYPES[0]);
  // if no events to filter for are specified, so we take all events
  // this is achieved by setting by omitting the `event_types` in the subscription message
  if (subscribedEventTypesCount > 0) {
    JsonArray types = args.createNestedArray("event_types");
    for (size_t i = 0; i < subscribedEventTypesCount; ++i) {
      types.add(EVENT_TYPES[i]);
    }
  }

  String json;
  serializeJson(doc, json);
  Serial.println("📝 JSON payload:");
  Serial.println(json);

  sendWebSocketFrame(json);
}

// Send WebSocket frame (typically responses to PING or subscription messages)
void sendWebSocketFrame(const String &payload) {
  const uint8_t opcode = 0x1; // text frame
  uint8_t header[10];
  size_t payloadLength = payload.length();
  size_t headerSize = 2;
  uint8_t maskKey[4];

  // Construct header
  header[0] = 0x80 | opcode; // FIN + opcode
  if (payloadLength <= 125) {
    header[1] = 0x80 | payloadLength;
  } else if (payloadLength <= 65535) {
    header[1] = 0x80 | 126;
    header[2] = (payloadLength >> 8) & 0xFF;
    header[3] = payloadLength & 0xFF;
    headerSize += 2;
  } else {
    Serial.println(F("❌ Payload too large"));
    return;
  }

  // Generate random mask key
  for (int i = 0; i < 4; ++i) {
    maskKey[i] = random(0, 256);
    header[headerSize++] = maskKey[i];
  }

  // Send header + mask + masked payload
  client->write(header, headerSize);           // header + mask
  for (size_t i = 0; i < payloadLength; ++i) { // mask payload
    client->write(payload[i] ^ maskKey[i % 4]);
  }
  client->flush();

  Serial.println(F("📤 Sent WebSocket text frame"));
}

// Reads from the WebSocket stream, fills wsBuffer, returns true if a complete message is
// stored in `wsBuffer` and ready to be processed
bool readWebSocketFrame() {
  // Need at least 2 bytes for the frame header
  if (client->available() < 2) return false;

  /* ── Frame header ───────────────────────────────────────── */
  uint8_t firstByte = client->read();
  uint8_t secondByte = client->read();

  bool isFinal = firstByte & 0x80;
  uint8_t opcode = firstByte & 0x0F;
  bool isControl = opcode & 0x08;
  bool mask = secondByte & 0x80; // should be 0 for server-to-client
  uint64_t payloadLength = secondByte & 0x7F;

  // Check for reserved bits
  if (firstByte & 0x70) {
    if (!supressRepeatedRSVWarnings) {
      Serial.println(F("❌ RSV bits set, unsupported extension"));
      supressRepeatedRSVWarnings = true;
    }
    return false;
  }
  supressRepeatedRSVWarnings = false;

  // Check for mask bit (should not be set)
  if (mask) {
    Serial.println(F("❌ Server-to-client frame is masked, protocol error"));
    // Optionally, read and discard mask key to resync
    for (int i = 0; i < 4; ++i)
      client->read();
    return false;
  }

  if (payloadLength == 126) {
    while (client->available() < 2)
      delay(1);
    payloadLength = ((uint64_t)client->read() << 8) | client->read();
  } else if (payloadLength == 127) {
    Serial.println(F("❌ 64‑bit payloads not supported"));
    while (client->available() < 8)
      delay(1);
    for (int i = 0; i < 8; ++i)
      client->read(); // discard
    return false;
  }

  if (isControl && payloadLength > 125) {
    Serial.println(F("❌ Control frame payload too large"));
    return false;
  }
  // Serial.printf("📦 Expecting %llu byte(s) of payload\n", payloadLength);

  /* ── Handle control frames first ────────────────────────── */
  if (opcode == 0x9) { // PING
    // read ping payload (<=125 B by spec)
    String pingPayload;
    while (pingPayload.length() < payloadLength) {
      if (client->available())
        pingPayload += (char)client->read();
      else
        delay(1);
    }

    // Build masked PONG frame (header + mask + masked payload)
    uint8_t maskKey[4];
    for (int i = 0; i < 4; ++i)
      maskKey[i] = random(0, 256);

    uint8_t hdr[6];
    hdr[0] = 0x8A;                        // FIN=1, opcode=0xA (PONG)
    hdr[1] = 0x80 | pingPayload.length(); // MASK bit | length (≤125)
    for (int i = 0; i < 4; ++i)
      hdr[2 + i] = maskKey[i];
    client->write(hdr, 6);

    // Send masked payload
    for (size_t i = 0; i < pingPayload.length(); ++i) {
      client->write(pingPayload[i] ^ maskKey[i % 4]);
    }
    client->flush();
    Serial.print(F("📤 Pong sent, payload bytes: "));
    Serial.println(pingPayload.length());
    return false; // done with this frame
  }

  if (opcode == 0xA) { // PONG
    // Just read and discard payload
    for (uint64_t i = 0; i < payloadLength; ++i) {
      while (!client->available())
        delay(1);
      client->read();
    }
    Serial.println(F("📥 PONG received (ignored)"));
    return false;
  }

  if (opcode == 0x8) { // CLOSE frame
    for (uint64_t i = 0; i < payloadLength; ++i) {
      while (!client->available())
        delay(1);
      client->read();
    }
    client->stop();
    Serial.println(F("📴 Server closed the connection."));
    return false;
  }

  /* ── Data frames (text / continuation) ──────────────────── */
  String payload;
  while (payload.length() < payloadLength) {
    if (client->available())
      payload += (char)client->read();
    else
      delay(1);
  }
  // Serial.printf("📦 Received  %u byte(s) of payload\n", payload.length());

  if (opcode == 0x1) { // TEXT – first (or only) frame
    wsBuffer = payload;
    wsReceiving = !isFinal;
  } else if (opcode == 0x0 && wsReceiving) { // CONTINUATION
    wsBuffer += payload;
    wsReceiving = !isFinal;
  } else {
    Serial.printf("⚠️ Unsupported opcode 0x%02X\n", opcode);
    return false;
  }

  // Return true if message is complete
  return !wsReceiving;
}

// Processes the JSON message in wsBuffer
// CAUTION: should only be called if readWebSocketFrame() returned true
void processWebSocketMessage() {
  StaticJsonDocument<16384> doc;
  DeserializationError err = deserializeJson(doc, wsBuffer);
  // uncomment following two lines to print entire deserialized Json message from server for debugging:
  // Serial.println("📥 Full WebSocket message:");
  // Serial.println(wsBuffer);
  wsBuffer.clear();

  if (err) {
    Serial.print(F("❌ JSON parse failed:"));
    Serial.println(err.c_str());
    return;
  }

  const char *topic = doc["topic"];
  if (topic && strcmp(topic, "events") == 0) { // for websockets message in the `events` topic
    // pull out extra metadata:
    long blockHeight = atol(doc["payload"]["block_height"]);
    const char *ts = doc["payload"]["block_timestamp"]; // ISO‑8601
    int msgIndex = doc["payload"]["message_index"] | 0; // int fallback

    // print summary of events
    JsonArray events = doc["payload"]["events"];
    if (events.size() > 0) {
      Serial.printf("\n🔔[msg index %4d] block at height %ld, time stamp %s, has %d relevant event(s)\n", msgIndex, blockHeight, ts, events.size());
      for (JsonObject e : events) {
        Serial.printf("  • %-50s tx=%.*s…\n", (const char *)e["type"], 8, ((const char *)e["transaction_id"]));
        processControlInstruction((const char *)e["payload"]); // decode and print payload
      }
      Serial.println("");
    } else {
      Serial.printf("⏳[msg index %4d] heartbeat @ block hight %ld, time stamp %s\n", msgIndex, blockHeight, ts);
      greenToggler->trigger(); // blink green LED to indicate heartbeat
    }
  } else { // for websockets message _not_ the `events` topic
    Serial.println(F("⚙️ Non‑event message:"));
    serializeJsonPretty(doc, Serial);
    Serial.println("\n");
  }
}

/* Flow-Specific processing of websocket messages
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// FUNCTION: processControlInstruction
// processes the json-representation of a control event's PAYLOAD. The payload is base64-encoded json of the cadence representation.
// St the moment, only events of type `A.0d3c8d02b02ceb4c.MicrocontrollerTest.ControlValueChanged` are supported.
void processControlInstruction(const char *encodedPayload) {
  const size_t encpayloadLen = strlen(encodedPayload);

  // Allocate a buffer big enough for decoded data and decode Base64 into it.
  // mbedTLS provides a two-step approach (safe and recommended) for dynamic buffer allocation.
  size_t decodedLen = 0; // calling `mbedtls_base64_decode` with first input *dst = NULL or second input dlen = 0 writes the required buffer size in `decodedLen` (third input)
  mbedtls_base64_decode(nullptr, 0, &decodedLen, reinterpret_cast<const unsigned char *>(encodedPayload), encpayloadLen);
  char *decoded = static_cast<char *>(malloc(decodedLen + 1)); // +1 for null terminator
  if (!decoded) {
    Serial.println(F("❌ malloc failed"));
    return;
  }
  int rc = mbedtls_base64_decode(reinterpret_cast<unsigned char *>(decoded), decodedLen, &decodedLen,
                                 reinterpret_cast<const unsigned char *>(encodedPayload), encpayloadLen);
  if (rc != 0) { // `mbedtls_base64_decode` returning 0 indicates successful decoding
    Serial.printf("❌ Base64 decode error (code %d)\n", rc);
    free(decoded);
    return;
  }
  decoded[decodedLen] = '\0'; // Ensure null‑terminated string

  // extract fields; example payload:
  /*
  {
  "value": {
    "id": "A.0d3c8d02b02ceb4c.MicrocontrollerTest.ControlValueChanged",
    "fields": [
      {
        "value": {
          "value": "15",
          "type": "Int64"
        },
        "name": "value"
      },
      {
        "value": {
          "value": "16",
          "type": "Int64"
        },
        "name": "oldValue"
      },
      {
        "value": {
          "value": "38",
          "type": "UInt64"
        },
        "name": "eventSequence"
      }
    ]
  },
  "type": "Event"
  }
  */

  // 3. Parse JSON and extract fields
  StaticJsonDocument<4096> cadenceEvent;
  DeserializationError err = deserializeJson(cadenceEvent, decoded);
  if (err) {
    Serial.print(F("❌ JSON parse failed: "));
    Serial.println(err.c_str());
    free(decoded);
    return;
  }

  // 4. Verify type and id
  const char *type = cadenceEvent["type"];
  if (!type || strcmp(type, "Event") != 0) {
    Serial.println(F("❌ Payload type does not represent Cadence event"));
    free(decoded);
    return;
  }
  const char *id = cadenceEvent["value"]["id"]; // all cadence events should have an id - we just assume that here
  if (!id || strcmp(id, "A.0d3c8d02b02ceb4c.MicrocontrollerTest.ControlValueChanged") != 0) {
    Serial.println(F("❌ Payload does not conform with the expected event id"));
    free(decoded);
    return;
  }

  // 5. Extract fields from payload of event `A.0d3c8d02b02ceb4c.MicrocontrollerTest.ControlValueChanged`
  JsonArray fields = cadenceEvent["value"]["fields"];
  String newValueStr, oldValueStr, eventSequenceStr; // we expect _all_ of these fields to be present
  for (JsonObject field : fields) {
    const char *fieldName = field["name"];
    const char *fieldValue = field["value"]["value"]; // string
    if (!fieldName || !fieldValue) continue;
    // Use switch on first character for efficiency
    switch (fieldName[0]) {
      case 'v':
        if (strcmp(fieldName, "value") == 0)
          newValueStr = fieldValue;
        break;
      case 'o':
        if (strcmp(fieldName, "oldValue") == 0)
          oldValueStr = fieldValue;
        break;
      case 'e':
        if (strcmp(fieldName, "eventSequence") == 0)
          eventSequenceStr = fieldValue;
        break;
      default:
        break;
    }
  }
  // verify that newValueStr, oldValueStr, eventSequenceStr have been set
  if (newValueStr.isEmpty() || oldValueStr.isEmpty() || eventSequenceStr.isEmpty()) {
    Serial.println(F("❌ Payload does not contain all expected fields"));
    free(decoded);
    return;
  }

  // Convert to int64_t/uint64_t using strtoll/strtoull for large values
  int64_t newValue = strtoll(newValueStr.c_str(), nullptr, 10);
  int64_t oldValue = strtoll(oldValueStr.c_str(), nullptr, 10);
  uint64_t eventSequence = strtoull(eventSequenceStr.c_str(), nullptr, 10);

  // Print extracted values
  Serial.printf("    Event Sequence %2d; updated value: %lld  oldValue: %lld\n");

  /* ── Sate machine update - eventually consistend; information-driven approach ──────────────────── */
  setControllerState(newValue);

  // ALWAYS free the decoded buffer to prevent memory leaks
  free(decoded);
  return;
}

void setControllerState(int64_t newValue) {
  blueToggler->trigger(); // trigger blue LED blinking
  extLoadOn = (newValue < 0);
  if (extLoadOn) {
    Serial.printf(F("    ⚡ External load ON"));
    digitalWrite(EXT_LOAD_SWITCH, EXT_LOAD_ON);
  } else {
    Serial.printf(F("    🔌 External load OFF"));
    digitalWrite(EXT_LOAD_SWITCH, EXT_LOAD_OFF);
  }
  Serial.println("");
}

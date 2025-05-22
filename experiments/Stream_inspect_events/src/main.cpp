#include "mbedtls/base64.h" // bundled with ESP32‑Arduino core
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ SECTION: FUNCTION PROTOTYPES ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */
void connectWifi();
void connectAndSubscribeWebsockets();
void sendWebSocketFrame(const String &payload);
bool readWebSocketFrame();
void processWebSocketMessage();
void processControlInstruction(const char *encodedPayload);

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

// WIFI
#include "WiFiCredentials.h"

/* Websockets - NEEDS HUMAN CONFIGURATION
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

// const char *host = "rest-mainnet.onflow.org";              // MAINNET server, only permits SSL connections
// const char *host = "rest-testnet.onflow.org";              // TESTNET server, only permits SSL connections
const char *host = "access-001.devnet52.nodes.onflow.org"; // TESTNET server, only permits plaintext connections!
const char *path = "/v1/ws";

// Set to 1 for SSL, 0 for plain text
#define USE_SSL 0

// Events:
// • Event `A.8c5303eaa26202d6.EVM.BlockExecuted` is emitted once for *every block*, including empty ones.
// • Every transaction needs to pay fees, hence `A.912d5440f7e3769e.FlowFees.FeesDeducted` is emitted once for *every transaction*.
// • Event `A.0d3c8d02b02ceb4c.MicrocontrollerTest.ControlValueChanged` is emitted once for *every control value change*, i.e. only when
//   a transaction interacts with the on-chain controller.
const char *const EVENT_TYPES[] = {
    // "A.8c5303eaa26202d6.EVM.BlockExecuted",
    // "A.912d5440f7e3769e.FlowFees.FeesDeducted",
    "A.0d3c8d02b02ceb4c.MicrocontrollerTest.ControlValueChanged"
    // Add more event types here as needed
};

/* CONTROLLER SETUP
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

// Websocket client: global variables
#if USE_SSL
WiFiClientSecure sslClient;
const int port = 443;
#else
WiFiClient plainClient;
const int port = 8075;
#endif
Client *client = nullptr;

String wsBuffer = "";     // holds full message as it arrives
bool wsReceiving = false; // tracks whether we’re in a multi-frame message

bool supressRepeatedRSVWarnings = false;

// FUNCTION setup(): called by Arduino framework once at startup
void setup() {
  Serial.begin(115200);
  pinMode(LED_BLUE, OUTPUT);  // GPIO  0: Blue sub-LED;  LOW = on, HIGH = off
  pinMode(LED_GREEN, OUTPUT); // GPIO 45: Green sub-LED; LOW = on, HIGH = off
  pinMode(LED_RED, OUTPUT);   // GPIO 46: Red sub-LED;   LOW = on, HIGH = off
  delay(1000);

  connectWifi();
  connectAndSubscribeWebsockets();
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER LOOP ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

void loop() {
  // If not connected, try to reconnect
  if (!client || !client->connected()) {
    Serial.println("⚠️ Lost connection, attempting to reconnect...");
    client->stop(); // Ensure client is stopped before reconnecting
    delay(1000);
    connectWifi(); // Optional: re-check WiFi connection
    connectAndSubscribeWebsockets();
    delay(1000); // Give some time for connection to establish
    return;
  }

  if (readWebSocketFrame()) {
    processWebSocketMessage();
  }
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ BUSINESS LOGIC FUNCTIONS ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

// FUNCTION connectWifi:
// connects to the Wifi using credentials specified in `WiFiCredentials.h`
//  * while connecting Wifi, Led will be on red
//  * when this function returns, all LED's are off
void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_RED, HIGH);   // on
    digitalWrite(LED_BLUE, HIGH);  // off
    digitalWrite(LED_GREEN, HIGH); // off
    return;
  }

  Serial.print("Connecting Wi‑Fi…");
  digitalWrite(LED_RED, LOW);    // on
  digitalWrite(LED_BLUE, HIGH);  // off
  digitalWrite(LED_GREEN, HIGH); // off
  delay(500);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    for (int i = 1; i < 3; i++) { // blink red LED quickly four times
      digitalWrite(LED_RED, LOW); // on
      delay(200);
      digitalWrite(LED_RED, HIGH); // off
      delay(100);
    }
    Serial.print(".");
  }

  // now we are connected, blink green LED as confirmation
  Serial.printf("\n   connected to Wi‑Fi '%s' with local IP %s\n", WiFi.SSID(), WiFi.localIP().toString().c_str());
  digitalWrite(LED_RED, HIGH);    // off
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

// FUNCTION connectAndSubscribeWebsockets:
// connects to the Flow access node and subscribes to the events topic
void connectAndSubscribeWebsockets() {
  if (client && client->connected()) {
    Serial.println("Already connected to server");
    return;
  }
  delay(1000);

#if USE_SSL
  sslClient.setInsecure(); // Accept all certs (insecure, but works for dev)
  client = &sslClient;
  Serial.println("🔐 Using SSL connection");
#else
  client = &plainClient;
  Serial.println("🔓 Using plain-text connection");
#endif

  Serial.printf("Connecting to %s:%d\n", host, port);
  if (!client->connect(host, port)) {
    Serial.println("❌ Connection to server failed!");
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
  Serial.println("🛰️ Sent WebSocket handshake");

  // Wait for response
  while (client->connected() && !client->available())
    delay(10);
  Serial.println("📩 Handshake response:");
  while (client->available()) {
    String line = client->readStringUntil('\n');
    Serial.print(line);
    if (line == "\r") break;
  }
  Serial.println("💡 End of HTTP headers");

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
    Serial.println("❌ Payload too large");
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

  Serial.println("📤 Sent WebSocket text frame");
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
      Serial.println("❌ RSV bits set, unsupported extension");
      supressRepeatedRSVWarnings = true;
    }
    return false;
  }
  supressRepeatedRSVWarnings = false;

  // Check for mask bit (should not be set)
  if (mask) {
    Serial.println("❌ Server-to-client frame is masked, protocol error");
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
    Serial.println("❌ 64‑bit payloads not supported");
    while (client->available() < 8)
      delay(1);
    for (int i = 0; i < 8; ++i)
      client->read(); // discard
    return false;
  }

  if (isControl && payloadLength > 125) {
    Serial.println("❌ Control frame payload too large");
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
    Serial.print("📤 Pong sent, payload bytes: ");
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
    Serial.println("📥 PONG received (ignored)");
    return false;
  }

  if (opcode == 0x8) { // CLOSE frame
    for (uint64_t i = 0; i < payloadLength; ++i) {
      while (!client->available())
        delay(1);
      client->read();
    }
    client->stop();
    Serial.println("📴 Server closed the connection.");
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
    Serial.print("❌ JSON parse failed: ");
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
      Serial.printf("⏳[msg index %4d] heartbeat @ block %ld\n", msgIndex, blockHeight);
    }
  } else { // for websockets message _not_ the `events` topic
    Serial.println("⚙️ Non‑event message:");
    serializeJsonPretty(doc, Serial);
    Serial.println("\n");
  }
}

// FUNCTION: processControlInstruction
// processes the json-representation of a control event's PAYLOAD. The payload is base64-encoded json of the cadence representation.
// St the moment, only events of type `A.0d3c8d02b02ceb4c.MicrocontrollerTest.ControlValueChanged` are supported.
void processControlInstruction(const char *encodedPayload) {
  // 1. Allocate a buffer big enough for decoded data:
  //    • Base64 expands data by ~4/3, so decoded ≈ input * 3/4
  //    • this is guaranteed to always provide enough space for the decoded data
  const size_t in_len = strlen(encodedPayload);
  const size_t buf_len = (in_len * 3) / 4 + 1; // +1 for null‑terminator
  char *decoded = static_cast<char *>(malloc(buf_len));
  if (!decoded) {
    Serial.println(F("❌ malloc failed"));
    return;
  }

  // 2. Decode Base64
  size_t out_len = 0;
  int rc = mbedtls_base64_decode(reinterpret_cast<unsigned char *>(decoded), buf_len, &out_len,
                                 reinterpret_cast<const unsigned char *>(encodedPayload), in_len);
  if (rc != 0) { // return value of `mbedtls_base64_decode` should be 0 indicating successful decoding
    Serial.printf("❌ Base64 decode error (code %d)\n", rc);
    free(decoded);
    return;
  }
  decoded[out_len] = '\0'; // Ensure null‑terminated string

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
  Serial.printf("Extracted fields:\n  value: %lld\n  oldValue: %lld\n  eventSequence: %llu\n", newValue, oldValue, eventSequence);

  free(decoded);
}
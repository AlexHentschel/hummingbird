#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// WIFI
#include "WiFiCredentials.h"

const char *host = "rest-mainnet.onflow.org";
const int port = 443;
const char *path = "/v1/ws";

WiFiClientSecure client;
String wsBuffer = "";     // holds full message as it arrives
bool wsReceiving = false; // tracks whether we’re in a multi-frame message
bool reconnectRequested = false;
unsigned long reconnectTime = 0;

// Send WebSocket frame
void sendWebSocketFrame(WiFiClientSecure &client, const String &payload) {
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

  // Send header
  client.write(header, headerSize);

  // Send masked payload
  for (size_t i = 0; i < payloadLength; ++i) {
    client.write(payload[i] ^ maskKey[i % 4]);
  }

  Serial.println("📤 Sent WebSocket text frame");
}

// Prepare and send subscription message
void sendSubscribeMessage() {
  StaticJsonDocument<512> doc;
  doc["subscription_id"] = "20charIDStreamEvents";
  doc["action"] = "subscribe";
  doc["topic"] = "events";
  JsonObject args = doc.createNestedObject("arguments");
  args["heartbeat_interval"] = "5";
  JsonArray types = args.createNestedArray("event_types");
  types.add("A.f919ee77447b7497.FlowFees.FeesDeducted");
  types.add("A.e467b9dd11fa00df.EVM.BlockExecuted");

  String json;
  serializeJson(doc, json);
  Serial.println("📝 JSON payload:");
  Serial.println(json);

  sendWebSocketFrame(client, json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");

  // Accept all certs (insecure, but works for dev)
  client.setInsecure();

  Serial.printf("Connecting to %s:%d\n", host, port);
  if (!client.connect(host, port)) {
    Serial.println("❌ Connection to server failed!");
    return;
  }

  // WebSocket HTTP upgrade request
  String req = String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Upgrade: websocket\r\n" +
               "Connection: Upgrade\r\n" +
               "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n" +
               "Sec-WebSocket-Version: 13\r\n" +
               "Origin: https://rest-testnet.onflow.org\r\n" +
               "\r\n";

  client.print(req);
  Serial.println("🛰️ Sent WebSocket handshake");

  // Wait for response
  while (client.connected() && !client.available())
    delay(10);
  Serial.println("📩 Handshake response:");
  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.print(line);
    if (line == "\r") break;
  }
  Serial.println("💡 End of HTTP headers");

  // Send subscribe message
  sendSubscribeMessage();
}

void loop() {
  if (!client.connected()) {
    Serial.println("⚠️ Lost connection");
    delay(1000);
    return;
  }

  // Need at least 2 bytes for the frame header
  if (client.available() < 2) return;

  /* ── Frame header ───────────────────────────────────────── */
  uint8_t firstByte = client.read();
  uint8_t secondByte = client.read();

  bool isFinal = firstByte & 0x80;
  uint8_t opcode = firstByte & 0x0F;

  uint64_t payloadLength = secondByte & 0x7F;
  if (payloadLength == 126) {
    while (client.available() < 2)
      delay(1);
    payloadLength = ((uint64_t)client.read() << 8) | client.read();
  } else if (payloadLength == 127) {
    Serial.println("❌ 64‑bit payloads not supported");
    return;
  }

  Serial.printf("📦 Expecting %llu byte(s) of payload\n", payloadLength);

  /* ── Control frames first ───────────────────────────────── */
  if (opcode == 0x9) { // PING
    /* read ping payload (<=125 B by spec) */
    String pingPayload;
    while (pingPayload.length() < payloadLength) {
      if (client.available())
        pingPayload += (char)client.read();
      else
        delay(1);
    }

    /* build masked PONG frame */
    uint8_t maskKey[4];
    for (int i = 0; i < 4; ++i)
      maskKey[i] = random(0, 256);

    uint8_t hdr[2] = {
        0x8A,                                // FIN=1, opcode=0xA (PONG)
        uint8_t(0x80 | pingPayload.length()) // MASK bit | length (≤125)
    };
    client.write(hdr, 2);
    client.write(maskKey, 4);

    /* masked payload */
    for (size_t i = 0; i < pingPayload.length(); ++i) {
      client.write(pingPayload[i] ^ maskKey[i & 3]);
    }

    Serial.print("📤 Masked pong sent, payload bytes: ");
    Serial.println(pingPayload.length());
    return; // done with this frame
  }

  if (opcode == 0x8) { // CLOSE frame
    while (client.available() < payloadLength)
      delay(1);
    for (uint64_t i = 0; i < payloadLength; ++i)
      client.read();
    client.stop();
    Serial.println("📴 Server closed the connection.");
    return;
  }

  /* ── Data frames (text / continuation) ──────────────────── */
  String payload;
  while (payload.length() < payloadLength) {
    if (client.available())
      payload += (char)client.read();
    else
      delay(1);
  }
  Serial.printf("📦 Received  %u byte(s) of payload\n", payload.length());

  if (opcode == 0x1) { // TEXT – first (or only) frame
    wsBuffer = payload;
    wsReceiving = !isFinal;
  } else if (opcode == 0x0 && wsReceiving) { // CONTINUATION
    wsBuffer += payload;
    wsReceiving = !isFinal;
  } else {
    Serial.printf("⚠️ Unsupported opcode 0x%02X\n", opcode);
    return;
  }

  /* ── Completed message?  Parse JSON ─────────────────────── */
  if (!wsReceiving) {
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
        Serial.printf("🔔[msg index %3d] block at height %ld, time stamp %s, has %d relevant event(s)\n", msgIndex, blockHeight, ts, events.size());
        for (JsonObject e : events) {
          Serial.printf("  • %-50s tx=%.*s…\n", (const char *)e["type"], 8, ((const char *)e["transaction_id"]));
        }
      } else {
        Serial.printf("⏳[msg index %5d] heartbeat @ block %ld\n", blockHeight);
      }
      Serial.println("");
    } else { // for websockets message _not_ the `events` topic
      Serial.println("⚙️ Non‑event message:");
      serializeJsonPretty(doc, Serial);
      Serial.println("\n");
    }
  }
}
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
// #include <HTTPClient.h>
// #include <Arduino_JSON.h>

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ SECTION: FUNCTION PROTOTYPES ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */
void connectWifi();
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

// WIFI
#include "WiFiCredentials.h"

/* External Load
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
// EXT_LOAD_SWITCH defines the GPIO that is used to controll an external load attached.
// EXT_LOAD_ON and EXT_LOAD_OFF define the states that correspond to the load being provided
// power or not. Here, we use the Solid State Relay [SSR] H3MB-052D from Ingenex, which
// connects its load pins on input HIGH
#define EXT_LOAD_SWITCH D6
#define EXT_LOAD_ON HIGH
#define EXT_LOAD_OFF LOW

/* Websockets
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
// Flow devnet WebSocket endpoint
const char *websocket_host = "rest-testnet.onflow.org";
const uint16_t websocket_port = 443; // TLS
const char *websocket_path = "/v1/ws";

// ─── isrg_root_x1.pem ───
static const char root_ca[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// WebSocket client
WebSocketsClient webSocket;

/* CONTROLLER SETUP
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

void setup() {
  Serial.begin(115200);
  pinMode(LED_BLUE, OUTPUT);  // GPIO  0: Blue sub-LED;  LOW = on, HIGH = off
  pinMode(LED_GREEN, OUTPUT); // GPIO 45: Green sub-LED; LOW = on, HIGH = off
  pinMode(LED_RED, OUTPUT);   // GPIO 46: Red sub-LED;   LOW = on, HIGH = off

  pinMode(EXT_LOAD_SWITCH, OUTPUT);

  delay(2000);
  Serial.println("");
  connectWifi();

  webSocket.beginSSL(websocket_host, websocket_port, websocket_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER LOOP ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

void loop() {
  webSocket.loop(); // must be called frequently
  delay(1);         // yield time to allow other background tasks
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

void sendSubscription() {
  StaticJsonDocument<512> doc;
  doc["subscription_id"] = "20charIDStreamEvents";
  doc["action"] = "subscribe";
  doc["topic"] = "events";

  JsonObject args = doc.createNestedObject("arguments");
  // args["block_status"] = "sealed";

  JsonArray types = args.createNestedArray("event_types");
  types.add("A.912d5440f7e3769e.FlowFees.FeesDeducted");
  types.add("A.8c5303eaa26202d6.EVM.BlockExecuted");

  String out;
  serializeJson(doc, out);
  webSocket.sendTXT(out);

  Serial.println("Sent subscription:");
  Serial.println(out);
}

void handleMessage(const char *message) {
  // Dynamically allocate JSON buffer (~3–4 KB typical per block with events)
  const size_t capacity = 4096;
  DynamicJsonDocument doc(capacity);

  DeserializationError err = deserializeJson(doc, message);
  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    return;
  }

  const char *topic = doc["topic"];
  if (!topic || strcmp(topic, "events") != 0) {
    Serial.println("Non-event message:");
    serializeJsonPretty(doc, Serial);
    return;
  }

  int block_height = doc["payload"]["block_height"] | -1;
  JsonArray events = doc["payload"]["events"];

  if (!events || events.size() == 0) {
    Serial.printf("Heartbeat @ block %d\n", block_height);
  } else {
    Serial.printf("Block %d, %d event(s):\n", block_height, events.size());
    for (JsonObject evt : events) {
      const char *type = evt["type"];
      const char *tx = evt["transaction_id"];
      Serial.printf("  - %s tx=%.*s…\n", type, 8, tx ? tx : "????????");
    }
  }
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("WebSocket connected");
      sendSubscription();
      break;
    case WStype_TEXT:
      payload[length] = 0; // null-terminate
      handleMessage((const char *)payload);
      break;
    case WStype_ERROR:
      Serial.println("WebSocket error");
      break;
    default:
      break;
  }
}
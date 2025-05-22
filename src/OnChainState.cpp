#include "mbedtls/base64.h" // bundled with ESP32‑Arduino core
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <tuple>

#include "OnChainState.h"

const String OnChainState::Cadence_Script_Retrieving_Led_State = R"({"script": "aW1wb3J0IE1pY3JvY29udHJvbGxlclRlc3QgZnJvbSAweDBkM2M4ZDAyYjAyY2ViNGMKCmFjY2VzcyhhbGwpIGZ1biBtYWluKCk6IEludDY0IHsKICByZXR1cm4gTWljcm9jb250cm9sbGVyVGVzdC5Db250cm9sVmFsdWUKfQ==", "arguments": []})";

OnChainState::OnChainState(String flowRestAccess) : url(flowRestAccess), postBody(Cadence_Script_Retrieving_Led_State) {
}

std::tuple<unsigned long, bool> OnChainState::get_latest_sealed_block() {
  unsigned long latestSealedHeight = 0;
  bool success = false;

  HTTPClient http;
  http.begin(url + "blocks?height=sealed");
  const int httpResponseCode = http.GET();
  if (httpResponseCode <= 0) {
    Serial.print(F("   ❌ Get sealed block error code: "));
    Serial.println(httpResponseCode);
    http.end();
    return std::make_tuple(0, false);
  }

  String rawResponse = http.getString();
  DynamicJsonDocument response(4096); // Adjust size as needed
  DeserializationError err = deserializeJson(response, rawResponse);
  if (err) {
    Serial.print("Parsing response for script execution request failed! Error: ");
    Serial.println(err.c_str());
    http.end();
    return std::make_tuple(0, false);
  }

  // uncomment for debugging:
  // Serial.println("Result for script execution request:");
  // serializeJsonPretty(response, Serial);
  // Serial.println("\n");

  // check that response contains at least one element:
  if (response.size() == 0) {
    Serial.println(F("   ❌ Response for latest sealed block is empty"));
    http.end();
    return std::make_tuple(0, false);
  }
  JsonVariant latestStealedBlock = response[0];

  if (!latestStealedBlock.containsKey("header")) {
    Serial.println(F("   ❌ Response for latest sealed block does not contain header"));
    http.end();
    return std::make_tuple(0, false);
  }
  latestStealedBlock = latestStealedBlock["header"];

  if (!latestStealedBlock.containsKey("height")) {
    Serial.println(F("   ❌ Response for latest sealed block does not contain height"));
    http.end();
    return std::make_tuple(0, false);
  }
  latestSealedHeight = latestStealedBlock["height"];
  success = true;

  // Free resources
  http.end();
  return std::make_tuple(latestSealedHeight, success);
}

String OnChainState::getURL() {
  return url;
}

std::tuple<int64_t, bool> OnChainState::get_led_state_at_block(unsigned long blockHeight) {
  Serial.println(F("➡️ Sending script execution request to rerieve on-chain state:"));
  HTTPClient http;
  http.begin(url + "scripts?block_height=" + String(blockHeight));
  http.addHeader(F("Content-Type"), F("application/json"));
  int httpResponseCode = http.POST(postBody);

  if (httpResponseCode <= 0) {
    Serial.print(F("   ❌ Request error code: "));
    Serial.println(httpResponseCode);
    http.end();
    return std::make_tuple(0, false);
  }

  String rawResponse = http.getString();
  Serial.println(F("   ⬅️ received script execution response:"));
  // uncomment for debugging:
  // Serial.println(rawResponse);
  // Serial.println("\n");

  std::tuple<String, bool> payload = extractPayloadFromResponse(rawResponse);
  if (!std::get<1>(payload)) {
    Serial.println(rawResponse);
    http.end();
    return std::make_tuple(0, false);
  }

  std::tuple<int64_t, bool> controlValue = parsePayload(std::get<0>(payload));
  if (!std::get<1>(controlValue)) {
    Serial.println(rawResponse);
    Serial.println();
    http.end();
    return std::make_tuple(0, false);
  }

  http.end();
  return std::make_tuple(std::get<0>(controlValue), true);
}

// FUNCTION extractPayloadFromResponse:
// Takes the raw HTTP-Get response and differentiates happy path from error case. There are two expected responses cases:
//
// Case 1. In case of an error, the response is a JSON object with the following structure:
//   {
//   "code" : 404,
//   "message" : "Flow resource not found: not found: block height 0 is less than the spork root block height 218215349. Try to use a historic node: failed to retrieve block ID for height 0: could not retrieve resource: key not found"
//   }
//   Here, we want to detect the error and return false.
//
// Case 2. In case of a successful request, the response is a JSON containing soleley the base64-encoded payload:
//   "eyJ2YWx1ZSI6IjAiLCJ0eXBlIjoiSW50NjQifQo="
//   Then, we want to continue just the string without the qote signs.
std::tuple<String, bool> OnChainState::extractPayloadFromResponse(String rawResponse) {
  // Case 1. Detect error response (JSON object)
  rawResponse.trim();
  bool isError = false;
  if (rawResponse.startsWith("{")) {
    // Error case: parse JSON and check for "code"
    StaticJsonDocument<1024> errorDoc;
    DeserializationError err = deserializeJson(errorDoc, rawResponse);
    if (!err && errorDoc.containsKey("code")) {
      Serial.println(F("   ❌ Error response:"));
    } else {
      Serial.println(F("   ❌ Unexpected response structure:"));
    }
    return std::make_tuple("", false);
  }

  // Case 2. treat as base64-encoded string and attempt to decode it
  // Success case: base64 string, expected in quotation chars
  String base64Payload = rawResponse;
  if (!base64Payload.startsWith("\"") || !base64Payload.endsWith("\"")) {
    Serial.println(F("   ❌ Unexpected response structure:"));
    return std::make_tuple("", false);
  }
  base64Payload = base64Payload.substring(1, base64Payload.length() - 1);
  return std::make_tuple(base64Payload, true);
}

// FUNCTION parsePayload:
// 1. extract the controller state from a Base64 string.
std::tuple<int64_t, bool> OnChainState::parsePayload(String rawResponse) {
  // Allocate a buffer big enough for decoded data and decode Base64 into it.
  // mbedTLS provides a two-step approach (safe and recommended) for dynamic buffer allocation.
  const size_t encpayloadLen = strlen(rawResponse.c_str());
  size_t decodedLen = 0; // calling mbedtls_base64_decode first input *dst = NULL or second input dlen = 0 writes the required buffer size in `decodedLen` (third input)
  mbedtls_base64_decode(nullptr, 0, &decodedLen, reinterpret_cast<const unsigned char *>(rawResponse.c_str()), encpayloadLen);
  char *decoded = static_cast<char *>(malloc(decodedLen + 1)); // +1 for null terminator
  if (!decoded) {
    Serial.println(F("   ❌ malloc failed"));
    return std::make_tuple(0, false);
  }
  int rc = mbedtls_base64_decode(reinterpret_cast<unsigned char *>(decoded), decodedLen, &decodedLen,
                                 reinterpret_cast<const unsigned char *>(rawResponse.c_str()), encpayloadLen);
  if (rc != 0) { // `mbedtls_base64_decode` returning 0 indicates successful decoding
    Serial.printf("   ❌ Base64 decode error (code %d)\n", rc);
    free(decoded);
    return std::make_tuple(0, false);
  }
  decoded[decodedLen] = '\0'; // Ensure null‑terminated string

  // 3. Parse JSON and extract fields
  StaticJsonDocument<2048> scriptResult;
  DeserializationError err = deserializeJson(scriptResult, decoded);
  if (err) {
    Serial.print(F("   ❌ JSON parse failed:"));
    Serial.println(err.c_str());
    free(decoded);
    return std::make_tuple(0, false);
  }

  if (!scriptResult.containsKey("value")) {
    Serial.print(F("   ❌ Missing 'value' key in JSON response:"));
    serializeJsonPretty(scriptResult, Serial);
    Serial.println("\n");
    free(decoded);
    return std::make_tuple(0, false);
  }

  String valueStr = scriptResult["value"];
  int64_t value = strtoll(valueStr.c_str(), nullptr, 10);
  Serial.printf("    current led sate: %s\n", valueStr);

  // Free resources
  free(decoded);
  return std::make_tuple(value, true);
}
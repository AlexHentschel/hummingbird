#include "mbedtls/base64.h" // bundled with ESP32‑Arduino core
#include <Arduino.h>

// -----------------------------------------------------------------------------
// Hard‑coded Base64 payload from the Flow event
// -----------------------------------------------------------------------------
static const char *encoded_payload =
    "eyJ2YWx1ZSI6eyJpZCI6IkEuMGQzYzhkMDJiMDJjZWI0Yy5NaWNyb2NvbnRyb2xsZXJUZXN0LkNvbnRyb2xWYWx1ZUNoYW5nZWQiLCJmaWVsZHMiOlt7InZhbHVlIjp7InZhbHVlIjoiMTUiLCJ0eXBlIjoiSW50NjQifSwibmFtZSI6InZhbHVlIn0seyJ2YWx1ZSI6eyJ2YWx1ZSI6IjE2IiwidHlwZSI6IkludDY0In0sIm5hbWUiOiJvbGRWYWx1ZSJ9LHsidmFsdWUiOnsidmFsdWUiOiIzOCIsInR5cGUiOiJVSW50NjQifSwibmFtZSI6ImV2ZW50U2VxdWVuY2UifV19LCJ0eXBlIjoiRXZlbnQifQo=";

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  } // Wait for Serial Monitor to open

  // ---------------------------------------------------------------------
  // 1. Allocate a buffer big enough for decoded data
  //    Base64 expands data by ~4/3, so decoded ≈ input * 3/4
  // ---------------------------------------------------------------------
  const size_t in_len = strlen(encoded_payload);
  const size_t buf_len = (in_len * 3) / 4 + 1; // +1 for null‑terminator
  char *decoded = static_cast<char *>(malloc(buf_len));
  if (!decoded) {
    Serial.println(F("❌ malloc failed"));
    return;
  }

  // ---------------------------------------------------------------------
  // 2. Decode Base64
  // ---------------------------------------------------------------------
  size_t out_len = 0;
  int rc = mbedtls_base64_decode(reinterpret_cast<unsigned char *>(decoded), buf_len, &out_len,
                                 reinterpret_cast<const unsigned char *>(encoded_payload), in_len);
  if (rc != 0) { // return value of `mbedtls_base64_decode` should be 0 indicating successful decoding
    Serial.printf("❌ Base64 decode error (code %d)\n", rc);
    free(decoded);
    return;
  }

  decoded[out_len] = '\0'; // Ensure null‑terminated string

  // ---------------------------------------------------------------------
  // 3. Print result
  // ---------------------------------------------------------------------
  Serial.println(F("✅ Decoded JSON:"));
  Serial.println(decoded);

  free(decoded);
}

void loop() {
  // Nothing to do here
}

## Folder `Experiments`

contains experiments that to create individual pieces of Project Hummingbird, or validate technical aspects.
All experiments were run on the Arduino Nano ESP32 board, with Visual Stuido Code 1.100.2  and [pioarduino](https://github.com/pioarduino/platform-espressif32) version 1.0.6 on a MacBook Pro M4, macOS Sequoina 15.4.1.

**Folders:**
* `Blink_LED` was my first test with the Arduino Nano ESP32 board: making the on-borad LED light up in different colors. 
* `Get_Flow_Block_via_Wifi` connects to local Wifi, sends an HTTP Get request to `https://rest-testnet.onflow.org/v1/blocks?height=final`, parses the response and prints a log message. When Wifi connection breaks, the code tries to reconnect automatically.  

   API endpoint `https://rest-testnet.onflow.org/v1/blocks?height=final` is for the [Flow blockchain](https://flow.com/) - specifically the test network - and returns the height of the latest _finalized_ block (see [Flow Access API Specification](https://developers.flow.com/networks/access-onchain-data) for details).

* `Decode_json-cdc_poc` is a small test for decoding a base64 string into json. Flow's **websocket streaming** endpoints (see [Flow Access API Specification](https://developers.flow.com/networks/access-onchain-data)) contain such base64-encoded payloads in their replies. The relevant information that we want to access is frequently contained in those payloads. For this test, an example of such encoded payload is hard-coded into the microcontroller's code (variable `encoded_payload`). For decoding, we use Arduino's [`mbedtls`](https://github.com/Seeed-Studio/Seeed_Arduino_mbedtls) which is bundled with ESP32‑Arduino core and very performant. For decoding the resulting string into a Json, we employ the [`ArduinoJson`](https://github.com/bblanchon/ArduinoJson) library by Benoit Blanchon  for performance reasons. 
    <details>
    <summary>details on experimental setup and validation</summary>

    The variable `encoded_payload` is an example payload returned by Flow's websocket API. It is a base64-encoded representation of the following json payload:
    
    ```json
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
    ```
    I have cross-checked this result with the following tiny python script: 
    ```python
    import base64
    import json

    # Input: base64-encoded JSON-Cadence string
    encoded_payload = "eyJ2YWx1ZSI6eyJpZCI6IkEuMGQzYzhkMDJiMDJjZWI0Yy5NaWNyb2NvbnRyb2xsZXJUZXN0LkNvbnRyb2xWYWx1ZUNoYW5nZWQiLCJmaWVsZHMiOlt7InZhbHVlIjp7InZhbHVlIjoiMTUiLCJ0eXBlIjoiSW50NjQifSwibmFtZSI6InZhbHVlIn0seyJ2YWx1ZSI6eyJ2YWx1ZSI6IjE2IiwidHlwZSI6IkludDY0In0sIm5hbWUiOiJvbGRWYWx1ZSJ9LHsidmFsdWUiOnsidmFsdWUiOiIzOCIsInR5cGUiOiJVSW50NjQifSwibmFtZSI6ImV2ZW50U2VxdWVuY2UifV19LCJ0eXBlIjoiRXZlbnQifQo="

    # Step 1: Decode base64
    decoded_bytes = base64.b64decode(encoded_payload)

    # Step 2: Parse JSON
    json_data = json.loads(decoded_bytes)

    # Step 3: Print parsed object (for inspection)
    print(json.dumps(json_data, indent=2))
    ```
    </details>

* `websockets_exploring_library_Links2004-arduinoWebSockets` contains different individual attempts use the [WebSockets](https://github.com/Links2004/arduinoWebSockets) library by Markus Sattler and collaborators. My understanding is that this is by far the most broadly adopted and performant library. 
Specifically, we are attempting to subscribe to Flow events through a websockets stream provided by Flow Access Nodes [ANs]. 
Using the [WebSockets](https://github.com/Links2004/arduinoWebSockets) library to implement a client on a microcontroller Partially worked out of the box for subscribing to blocks. Nevertheless, I had consistently issues with the server hanging up after about 30 to 60 seconds. Moreover, the library didn't work at all when switching the subscription `topic` from `blocks` to `events`. In comparison, exactly the same json subscription request works in the python reference implementation (MacBook). Please see [`./websockets_exploring_library_Links2004-arduinoWebSockets/README.md`](./websockets_exploring_library_Links2004-arduinoWebSockets/README.md) for further details. 

   _Note:_ this websockets implementation was further updated and refined for Project Hummingbird to make it more stable. 

* `Stream_inspect_events` implements a first proof of concept for streaming events from Flow's Access Nodes emitted by specialized on-chain smart contracts. It is based on the learnings from the experiments in `websockets_exploring_library_Links2004-arduinoWebSockets`. 

   _Note:_ this websockets implementation was further updated and refined for Project Hummingbird to make it more stable. 
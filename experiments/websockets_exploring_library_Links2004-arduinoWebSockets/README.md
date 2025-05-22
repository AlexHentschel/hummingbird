## Goal
Implement a proof of concept for a microcontroller streaming event data from the [official flow endpoints](https://developers.flow.com/networks/access-onchain-data/websockets-stream-api). 
For efficiency reasons, we want to stream the events via a websockets subscription as this is much less resource intensive for both the server and the client as opposed to polling. 


## Setup
* We are working with the _Arduino Nano ESP32_ developer board: the board features the NORA-W106, a module with a ESP32-S3 chip inside. This module supports both Wi-Fi and Bluetooth. 
* You need a USB-C cable, the Arduino Nano ESP32, and Wifi access
* I have used [pioarduino](https://github.com/pioarduino) (a fork of [platformio](https://platformio.org/)) as Visual Studio Code [VS Code] extension ([tutorial](https://randomnerdtutorials.com/vs-code-pioarduino-ide-esp32/)). It is important to note that for our hardware (ESP32-S3) we require the ESP32 Arduino Core (version 3). The [pioarduino](https://github.com/pioarduino) fork was initially created to support the newer ESP32-S3 processors - though by now they seem to also be supported by [platformio](https://platformio.org/) (not tested).
* We are working in C / C++

_Dependencies:_
* [ArduinoJson](https://github.com/bblanchon/ArduinoJson) library by Benoit Blanchon

## Python Reference Implementation
The folder `Python-Reference-Implementation` contains reference implementations for streaming event data from Flow using the public websockets end-point (no credentials required). 

_Notes on running the reference implementations_
* I used Miniconda Python distribution 3.13.2
* to avoid interactions with your pre-existing python environments, I recommend to create a dedicated virtual environment:
  * go to the `bin` folder of your python distribution (containing the python executable) and run the following command to create a virtual environment
    ```
    ./python -m venv --symlinks <path_where_you_keep_your_python_virtual_environments>/flow-websockets
    ```
  * then activate the python virtual environment temporarily in the shell:
    ```
    source <path_where_you_keep_your_python_virtual_environments>/flow-websockets/bin/activate
    ```
  * Install dependencies
    ```
    pip install websockets msgspec
    ```

Run:
```
python websockets_ssl_client.py
```
or 
```
websockets_plaintext_client.py
```
Stop: `Ctrl‑C`

On my arm-based MacBook Pro, these reference implementations worked without issues (once I figured out the details). They are useful for confirming that the configurations work in principle and to get a sample of the expected output.
The implementations are identical, except for the configuration of the server and the subscribed events.


## Websockets client for the ESP32-S3 microcontroller

While there exist established libraries, I ran into major obstacles when attempting to use the [WebSockets](https://github.com/Links2004/arduinoWebSockets) library by Markus Sattler and collaborators (my understanding is that this is by far the most broadly adopted and performant library).  
* On my microcontroller (Arduino Nano ESP32) websockets partially worked out of the box for subscribing to blocks with the [WebSockets](https://github.com/Links2004/arduinoWebSockets) library. Nevertheless, I had consistently issues with the server hanging up after about 30 to 60 seconds. Moreover, the library didn't work at all when switching the subscription `topic` from `blocks` to `events`. In comparison, exactly the same json subscription request works in the python reference implementation (MacBook).
* I did some research with the help of ChatGPT and it seems that there is an open [issues #864](https://github.com/Links2004/arduinoWebSockets/issues/864) for [WebSockets](https://github.com/Links2004/arduinoWebSockets) with the library's heartbeat. Furthermore, when subscribing to events, [WebSockets](https://github.com/Links2004/arduinoWebSockets) didn't receive any data (and continued to have the hang-up issue). 
  * It is important to note that the payloads are much bigger for events  than for subscribing to blocks. So the server splits up the payload across multiple frames. My current working hypotheses is that [WebSockets](https://github.com/Links2004/arduinoWebSockets) has a bug / edge case / incomplete implementation preventing it from properly handling this case of very large payloads being split up across multiple websocket frames. 
  * Regarding the server-side hangups, I am pretty sure this is related to the server's heartbeat messages not being properly responded to and it therefore hanging up on the client (microcontroller) when the server perceives it as unresponsive. I think this is due to the microcontroller having problems piecing together the server's messages across multiple websocket frames, including the heartbeat control messages and therefore failing to respond according to protocol. 

With a lot of forth-and-back with ChatGPT, I managed to write my own slimmed-down processing logic for the websockets protocol, only focusing on processing the server's message frames for subscription streams and processing the minimal set of control messages including the heartbeat.
* The benefit is that we have a very small implementation now since it only contains the portion of the websockets protocol that we actually use. Though, the entire processing is _synchronous_ not utilizing multi-core microcontrollers (such as the Arduino Nano ESP32). 
* The downside is that we might not be handling some common standard functionality of websockets and our custom implementation is probably less performant than [WebSockets](https://github.com/Links2004/arduinoWebSockets)'s hand-optimized implementation. 
_Working implementation_: `src/main.cpp`

_My failed attempts to get event streaming working with the [WebSockets](https://github.com/Links2004/arduinoWebSockets) library can be found in these files:_
* `/src/main.attempt1.cpp` tries to use [WebSockets](https://github.com/Links2004/arduinoWebSockets) without modifications. Currently it attempts to subscribe to the `events` topic, without success. Try subscribing to blocks instead as described in the [flow documentation](https://developers.flow.com/networks/access-onchain-data/websockets-stream-api).
* `/src/main.attempt1.cpp` attempts to add a work-around to fix the hangup problem: I tried adding a manual heartbeat message in the `loop()` function to signal to the server that the client is still alive, but without success. Also here, no event data is received. Try subscribing to blocks instead as described in the [flow documentation](https://developers.flow.com/networks/access-onchain-data/websockets-stream-api).

Notes: 
* For compiling the microcontroller code, I would recommend renaming the version you want to compile to `main.cpp` while renaming the other two versions such that they do _not_ end in `.cpp` anymore. Then they are ignored by the compiler.
* I have tinkered a lot with the different implementations. It might be that `main.attempt1.cpp` or `main.attempt2.cpp` don't compile in the latest variant. 

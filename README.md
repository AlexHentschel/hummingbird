## Goal
Implement a proof of concept for a microcontroller streaming event data from the [official flow endpoints](https://developers.flow.com/networks/access-onchain-data/websockets-stream-api). 
For efficiency reasons, we want to stream the events via a websockets subscription as this is much less resource intensive for both the server and the client as opposed to polling. 


## Setup
* We are working with the _Arduino Nano ESP32_ developer board: the board features the NORA-W106, a module with a ESP32-S3 chip inside. This module supports both Wi-Fi and Bluetooth. 
* You need a USB-C cable, the Arduino Nano ESP32, and Wifi access
* I have used [pioarduino](https://github.com/pioarduino) (a fork of [platformio](https://platformio.org/)) as Visual Studio Code [VS Code] extension ([tutorial](https://randomnerdtutorials.com/vs-code-pioarduino-ide-esp32/)). It is important to note that for our hardware (ESP32-S3) we require the ESP32 Arduino Core (version 3). The [pioarduino](https://github.com/pioarduino) fork was initially created to support the newer ESP32-S3 processors - though by now they seem to also be supported by [platformio](https://platformio.org/) (not tested).
* We are working in C / C++

_Dependencies:_
* [ArduinoJson](https://github.com/bblanchon/ArduinoJson) library by Benoit Blanchon for parsing json


## Areas for future development

:construction: This code is a proof of concept not ready for production applications! :construction:

### Short term: using SSL connection and established websockets library

* Websockets client implementation: There exist established libraries such as the [WebSockets](https://github.com/Links2004/arduinoWebSockets) library by Markus Sattler and collaborators (my understanding is that this is by far the most broadly adopted and performant library). Unfortunately, I ran into major obstacles when receiving data from the Flow websockets endpoint (see [Readme.md](./experiments/websockets_exploring_library_Links2004-arduinoWebSockets/README.md) in the `experiments` folder). Currently, this code works, but I am suspecting that we are still rarely missing events.
* Currently, we are using Access Node `access-001.devnet52.nodes.onflow.org`, which is operated by the Flow Foundation FOR TESTNET and permits plain-text websocket connections. The different protocol versions of testnet are internally called `devnet52<Number>`. In comparison, `rest-testnet.onflow.org` references the current official testnet version. As of May 15, 2025 this is identical to `devnet52` (future version will have a higher version number). However, `rest-testnet.onflow.org` are operated by service providers and the connection (plain text as well as ssl-encrypted) tends to break after some seconds ... strangely `rest-mainnet.onflow.org` does not seem to have this problem. 
* Plain-text: we communicate in plain-text with the Access Node (currently `access-001.devnet52.nodes.onflow.org`). The code already has some support for ssl-based connections (currently working reasonably reliably with `rest-mainnet.onflow.org`), but more testing is necessary.


### Mid term: trustless data retrieval with event proofs

At the moment, the microcontroller trusts the access node to provide an honest data stream. In principle, Flow allows event proofs that are very light weight so they could be verified by the microcontroller. Fur further details:
* [Flow's Data Availability Vision → Smartphone-sized light clients](https://flow.com/data-availability-vision#smartphone-sized-light-clients)
* [Flow Architecture → State and Event Proofs](https://www.notion.so/flowfoundation/State-and-Event-Proofs-1d11aee123248096975ef55b1d05bb1e?pvs=4)


<details>
<summary>State of the art research on running pairing-based cryptography on microcontrollers</summary>

### Research Papers
- Paper: [Applications of Pairing-based Cryptography on Automotive-grade Microcontrollers](https://ats-automotive-engineering.com/wp-content/uploads/2025/01/Applications-of-Pairing-based-Cryptography-on-Automotive-grade-Microcontrollers.pdf) [2018]
- Paper: [Performance Evaluation of Elliptic Curve Libraries on Automotive-Grade Microcontrollers](https://www.aut.upt.ro/~pal-stefan.murvay/papers/performance_evaluation_elliptic_curve_libraries_automotive-grade_microcontrollers.pdf) [2019]
   
    - they use Relic and BLS-381 (the curve we are using)
        
        ![Screenshot 2025-04-29 at 12.55.17 PM.png](Smart-home%20IOT%201e41aee1232480b98790e854c04d256f/Screenshot_2025-04-29_at_12.55.17_PM.png)
        
- Paper: [Performance Comparison Of ECC Libraries For IOT Devices](https://dergipark.org.tr/tr/download/article-file/3690885) [2024] - shallow level of detail
- Paper: [Tiny keys hold big secrets: On efficiency of Pairing-Based Cryptography in IoT](https://doi.org/10.1016/j.iot.2025.101489)
    
    > [Quote] **RELIC provides broad compatibility with ARM-based architectures**. For the integration into Contiki-NG OS running on a Zolertia RE-Mote, the library was compiled as a standalone static library with some specific tweaks required by our setup, e.g., to use only static memory and disable multithreading support, as our target platform does not support them. We evaluated the performance of the IBE, ABE, and SS schemes 
    introduced in Section [3](https://www.sciencedirect.com/science/article/pii/S2542660525000022#sec3).
    > 
    
    > Perazzo et al. [[14]](https://doi.org/10.1016/j.comcom.2021.02.012) and Rasori et al. [[15]](https://doi.org/10.1109/JIOT.2022.3154039) benchmarked various ABE schemes on Espressif ESP32 and Zolertia RE-Mote devices, which have a processing power of 240 MHz and 32 MHz respectively.
    > 
    > 
    > [14] [Performance evaluation of attribute-based encryption on constrained IoT devices](https://doi.org/10.1016/j.comcom.2021.02.012)
    > [15] [A survey on attribute-based encryption schemes suitable for the internet of things](https://doi.org/10.1109/JIOT.2022.3154039)
    > 
    
       
- Market Valuation for Blockchain in Smart Home  [2022]
[https://www.globenewswire.com/en/news-release/2022/05/09/2438353/0/en/Blockchain-in-Smart-Home-Market-Valuation-USD-2-045-4-Million-by-2027-at-41-2-CAGR-Report-by-Market-Research-Future-MRFR.html](https://www.globenewswire.com/en/news-release/2022/05/09/2438353/0/en/Blockchain-in-Smart-Home-Market-Valuation-USD-2-045-4-Million-by-2027-at-41-2-CAGR-Report-by-Market-Research-Future-MRFR.html)
- Paper: [Hawk: The Blockchain Model of Cryptography and Privacy-Preserving Smart Contracts](https://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=7546538)
- Paper: [SoK: Privacy-preserving smart contract](https://doi.org/10.1016/j.hcc.2023.100183)

**Cryptography libraries**:

- **WolfSSL**
    - https://www.wolfssl.com/espressif-risc-v-hardware-accelerated-cryptographic-functions-up-to-1000-faster-than-software/
- **relic-toolkit** for arduino
    - https://code.google.com/archive/p/relic-toolkit/wikis/BuildInstructionsArduino.wiki
    - [relic-toolkit's BuildOptions.wiki](https://code.google.com/archive/p/relic-toolkit/wikis/BuildOptions.wiki)
    - This resource describers how to compile the Relic library for the `stm32f4discovery` board (32-bit Arm Cortex-M4 CPU)
[https://github.com/RIOT-OS/RIOT/wiki/Intregate-Relic-(a-library-for-cryptography)-into-RIOT](https://github.com/RIOT-OS/RIOT/wiki/Intregate-Relic-(a-library-for-cryptography)-into-RIOT)
</details>


### Long term: Data privacy 

Encrypting on-chain orchestrators via  [garbled circuit](https://en.wikipedia.org/wiki/Garbled_circuit), [monomorphic encryption](https://en.wikipedia.org/wiki/Homomorphic_encryption) or some other [secure multi-party computation [MPC]](https://en.wikipedia.org/wiki/Secure_multi-party_computation) approach. For most use-cases except the most trivial, MPC would likely require a privacy-layer needing to be added to Flow. 



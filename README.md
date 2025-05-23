# Project Hummingbird

Ever had a “smart” device suddenly stop working because the cloud service behind it disappeared? It’s frustrating — and a reminder that too many connected devices depend on centralized platforms to function.

Together with [Janez](https://www.linkedin.com/in/janez-podhostnik-40915b127) and [Jan](https://www.linkedin.com/in/janbernatik), I’ve been exploring a different model: what if your devices listened to your smart contract, not someone else’s server?

That’s the idea behind Project Hummingbird, our Flow hackathon project. It’s a small but meaningful step toward decentralized, user-owned automation for the real world. 

### Why a hummingbird feeder?

Wild hummingbirds love the mild climate of coastal British Columbia. In cities like Vancouver, they rely on sugar-water feeders during rare cold snaps. To keep the nectar from freezing, people often use heat strips or lamps — but most setups are either wasteful (always on) or unreliable (we forget to turn them on).

So we asked: what if a blockchain smart contract could manage that heating automatically, turning it on only when temperatures drop?

### We’re building

This week’s hackathon project is a complete system:

- A **smart contract** on Flow that contains the control logic for the feeder.
- An **oracle** that feeds temperature data into the blockchain.
- A physical **microcontroller** that listens for blockchain events and powers the feeder heater only when it’s actually needed.

It’s a small, tangible proof of concept of how a blockchain can manage real-world devices efficiently, securely, and independently of centralized platforms.


### Why it matters

Project Hummingbird is just the beginning. Imagine resilient blockchain infrastructure controlling the charging of electric cars, operating smart homes, and coordinating decentralized renewable energy production — optimizing energy use, saving costs, and empowering users to control their own data and devices. 

If you're curious, please check out: 
* 🎥 my [**demo video**](https://youtu.be/d3rSHN_p8u0?feature=shared) of **Project Hummingbird**: a Flow-based smart contract switching a 110V AC mainline load (E27 light bulb) via blockchain events. In this 10-minute video, you'll get an overview over hackathon project, an end-to-end demo, the broader vision and why Flow is uniquely suited for this class of applications. 
* my blog post [Blockchain for Decentralized Critical Infrastructure](https://www.linkedin.com/pulse/blockchain-decentralized-critical-infrastructure-alexander-hentschel-kfvgc/) is a concise but in-depth vision. 






# Technical Proof of Concept
Implement a proof of concept for a microcontroller streaming event data from the [official flow endpoints](https://developers.flow.com/networks/access-onchain-data/websockets-stream-api). 
For efficiency reasons, we want to stream the events via a websockets subscription as this is much less resource intensive for both the server and the client as opposed to polling. 

<p float="left">
  <img src="https://github.com/user-attachments/assets/ce2d009e-8de9-40db-b36e-20ca27fe0f77" height="270" />
  <img src="https://github.com/user-attachments/assets/da5b41cd-365c-4a99-9105-15b617c6582c" height="270" />
  <img src="https://github.com/user-attachments/assets/b6530b9a-1f66-4990-8b50-1e6fa4cfdf23" height="270" /> 
</p>


Project Hummingbird is a collaboration between [Janez](https://www.linkedin.com/in/janez-podhostnik-40915b127), [Jan](https://www.linkedin.com/in/janbernatik), and [me](https://www.linkedin.com/in/alexander-hentschel/), building on our shared interest in decentralized automation and resilient infrastructure. Please check out their GitHub repos:
* [janezpodhostnik/esp32-flow](https://github.com/janezpodhostnik/esp32-flow) for 
   - Initial concept for getting the height of the latest sealed or latest finalized block.
   - [Cadence smart contract](https://github.com/janezpodhostnik/esp32-flow/tree/main/cadence) that orchestrates Project Hummingbird.
   - Logic for preparing a cadence transaction to provide new inputs to the on-chain orchestrator; we directly use this in Project Hummingbird.
   - First working prototype for getting chain data via script execution from an Access Node; we build on Janes' work in Project Hummingbird to restore the microcontroller's initial state after rebooting.
* [j1010001/Flow-esp32-s3-touch-lcd-1.28](https://github.com/j1010001/Flow-esp32-s3-touch-lcd-1.28) 
   - Human-friendly UX: querying the chain state (specifically sealed block via Flow Access Node API), and inspecting smart contract sate details via script execution. Interacting with the blockchain - and controlling your devices via the Flow blockchain - can be as easy as just tapping your smartwatch. 




## Setup
* We are working with the [Arduino Nano ESP32](https://docs.arduino.cc/hardware/nano-esp32/) developer board: the board features the NORA-W106, a module with a ESP32-S3 chip inside. This module supports both Wi-Fi and Bluetooth.
* We are working in C++
* You need a USB-C cable, the Arduino Nano ESP32, and Wi-Fi access
* I have used [pioarduino](https://github.com/pioarduino) (a fork of [platformio](https://platformio.org/)) as Visual Studio Code [VS Code] extension ([tutorial](https://randomnerdtutorials.com/vs-code-pioarduino-ide-esp32/)). It is important to note that for our hardware (ESP32-S3) we require the ESP32 Arduino Core (version 3). The [pioarduino](https://github.com/pioarduino) fork was initially created to support the newer ESP32-S3 processors - though by now they seem to also be supported by [platformio](https://platformio.org/) (not tested).
* To power the microcontroller independently of a computer, I used an old 5V / 850mA USB power adapter. ⚠️ Always ensure your power source is stable and within rated input range (6-21 V for the Arduino Nano ESP32).
* We are switching a 110V AC 50Hz mainline: 
   * OMRON G3MB-202P Solid State Relay (rated for switching up to 240V AC @ 2A, requiring 5V input).
   * The GPIO pins of the microcontroller operate at 3.3V. Therefore, we use the GPIO pin to switch an IRL530 (N-Mosfet), which in turn switches the 5V input for the Omron Solid State Relay. 
   * 220Ω resistor for limiting the current flow on the GPIO+ 100 kΩ (edited).
   * 100 kΩ pull-down resistor
   * fuse for mainline 

    <details>
    <summary>Circuit Diagram</summary>
    <img src="https://github.com/user-attachments/assets/219f62e6-fe00-432a-80ef-15a0093297ad" height="400" />
    </details>

_Dependencies:_
* [ArduinoJson](https://github.com/bblanchon/ArduinoJson) library by Benoit Blanchon for parsing json

_Flow_:
* Our proof of concept runs on the [Flow blockchain](https://flow.com/), specifically the [Flow Testnet](https://developers.flow.com/networks/flow-networks/accessing-testnet). 



## recommendations

### `WiFiCredentials.h`

The files `WiFiCredentials.h` will need to be modified by providing Wi-Fi ssid and password. Currently, the implementation expects the following parameters: 
```
#define WIFI_SSID "..."
#define WIFI_PASS "..."
```



To avoid accidentally committing private information, this short `bash` command provides a simple solution:
```bash
for file in $(find . -type f -name "WiFiCredentials\.h"); do
 git update-index --skip-worktree  "${file}"; done 
```
following the approach described here in https://compiledsuccessfully.dev/git-skip-worktree/





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



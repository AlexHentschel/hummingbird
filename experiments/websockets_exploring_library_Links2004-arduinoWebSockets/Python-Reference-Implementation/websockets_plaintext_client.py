#!/usr/bin/env python3
import asyncio, os, random, sys
import json
import websockets
import msgspec
from typing import Any, Dict

"""
Reference implementation to stream event data from Flow using an UNENCRYPTED websockets end-point (SSL deactivated)
• Tested with Miniconda Python distribution 3.13.2
• go to the `bin` folder containing the python executable and run the following command to create a virtual environment
   > ./python -m venv --symlinks <path_where_you_keep_your_python_virtual_environments>/flow-websockets
• then activate the python virtual environment temporarily in the shell:
  > source <path_where_you_keep_your_python_virtual_environments>/flow-websockets/bin/activate
• Install dependencies
   > pip install websockets msgspec

Run:
  > python websockets_plaintext_client.py
Stop: Ctrl‑C
"""


# Plain websockets without SSL encryption
# ──────────────────────────────────────────────────────────────────────────────────────────────────────────────
# The flow foundation runs an Access Node (currently `access-001`) FOR TESTNET that permits plain-text websocket
# connections. The different protocol versions of testnet are internally called "devnet52<Number>".
# In comparison, `rest-testnet.onflow.org` references the current official testnet version. As of May 15, 2025
# this is identical to `devnet52` (future version will have a higher version number).
# --------------------------------------------------------------------------------------------------------------

# [confirmed working] Testnet Websockets API without SSL encryption
FLOW_WS_ENDPOINT = os.getenv("FLOW_WS_ENDPOINT", "ws://access-001.devnet52.nodes.onflow.org:8075/v1/ws") #

# In contrast, the official API endpoints for Flow testnet (`rest-testnet.onflow.org`) and mainnet
# (`rest-testnet.onflow.org`) deny unencrypted websocket connections.

# Filter to two ubiquitous core‑contract events:
# Event `A.8c5303eaa26202d6.EVM.BlockExecuted` is emitted once for _every_ block, including empty ones.
# Every transaction needs to pay fees, hence `A.912d5440f7e3769e.FlowFees.FeesDeducted` is emitted once
# for every transaction.
# CAUTION: the precise name of the events differs between testnet and mainnet. These only fork for testnet:
EVENT_TYPES = ["A.0d3c8d02b02ceb4c.MicrocontrollerTest.ControlValueChanged", # emitted by the controller-smart contract of project hummingbird
               # "A.912d5440f7e3769e.FlowFees.FeesDeducted",                   # emitted as part of every transaction
               # "A.8c5303eaa26202d6.EVM.BlockExecuted",                       # emitted as part of every block (incl. empty blocks)
              ]



# Implementation
# ──────────────────────────────────────────────────────────────────────────────────────────────────────────────

# Exponential back‑off limits
BACKOFF_INITIAL  = 1         # seconds
BACKOFF_MAX      = 32        # seconds
BACKOFF_JITTER   = 0.4       # fraction (+/‑) of random jitter


def jitter(delay: int) -> float:
    """±40% randomisation to prevent many clients reconnecting in lock‑step."""
    return delay * (1 + random.uniform(-BACKOFF_JITTER, BACKOFF_JITTER))


async def subscribe(ws) -> None:
    """Send a single `subscribe` request for events."""
    msg: Dict[str, Any] = { # testnet
        "subscription_id": "20charIDStreamEvents",
        "action": "subscribe",
        "topic": "events",
        "arguments": {
            "heartbeat_interval": "5",
            "event_types": EVENT_TYPES
        },
    }
    print(json.dumps(msg))
    await ws.send(msgspec.json.encode(msg))
    print("🛰️ Sent subscribe request")


async def consume(ws) -> None:
    """Read frames forever and pretty‑print."""
    async for raw in ws:
        msg = msgspec.json.decode(raw)
        # Heartbeat frames have an empty events list; normalise printout
        if msg.get("topic") == "events":
            payload = msg["payload"]
            msgIndex = payload["message_index"]   # should always be present, if this field is absent, we abort with `KeyError` exception
            block_h  = payload["block_height"]    # should always be present, if this field is absent, we abort with `KeyError` exception
            ts       = payload["block_timestamp"] # should always be present, if this field is absent, we abort with `KeyError` exception
            evts     = payload.get("events", []) # might not be present, if there are no events in the block that we subscribed to

            if evts:
                print(f"\n🔔[msg index {str(msgIndex): >5}] block at height {str(block_h): >10}, time stamp {ts}, has {len(evts)} event(s):")
                for e in evts:
                    print(f"  • {e['type']} tx={e['transaction_id'][:8]}…")
            else:
                print(f"⏳[msg index {str(msgIndex): >5}] heartbeat @ block {block_h}")
        else:
            # subscription ack / error or future topic
            print("⚙️ server response: ", json.dumps(msg, indent=2))


async def run_forever() -> None:
    """Connect → subscribe → consume. Re‑connect on any drop."""
    delay = BACKOFF_INITIAL
    while True:
        try:
            async with websockets.connect(FLOW_WS_ENDPOINT, ping_interval=20) as ws:
                print(f"✅ Connected to {FLOW_WS_ENDPOINT}")
                await subscribe(ws)
                delay = BACKOFF_INITIAL            # success ⇒ reset back‑off
                await consume(ws)                  # returns only on error/close
        except (websockets.ConnectionClosedError) as exc:
            print(f"⚠️ connection closed: {exc!s}")
        except (websockets.InvalidStatusCode) as exc:
            print(f"⚠️ connection broke with invalid status code: {exc!s}")
        except (OSError) as exc:
            print(f"⚠️ connection lost due to OS Error: {exc!s}")
        except Exception as exc:  # noqa
            print(f"💥 unexpected error: {exc!r}")
        quit()
        print(f"🔄 reconnecting in {delay}s…")
        await asyncio.sleep(jitter(delay))
        delay = min(delay * 2, BACKOFF_MAX)


if __name__ == "__main__":
    try:
        asyncio.run(run_forever())
    except KeyboardInterrupt:
        print("\n👋 bye!")
        sys.exit(0)

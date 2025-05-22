#!/usr/bin/env python3
import asyncio, os, random, sys
import json
import websockets
import msgspec
from typing import Any, Dict

"""
Reference implementation to stream event data from Flow using an SSL-ENCRYPTED websockets end-point
• Tested with Miniconda Python distribution 3.13.2
• go to the `bin` folder containing the python executable and run the following command to create a virtual environment
   > ./python -m venv --symlinks <path_where_you_keep_your_python_virtual_environments>/flow-websockets
• then activate the python virtual environment temporarily in the shell:
  > source <path_where_you_keep_your_python_virtual_environments>/flow-websockets/bin/activate
• Install dependencies
   > pip install websockets msgspec

Run:
  > python websockets_ssl_client.py
Stop: Ctrl‑C
"""

# Websocket with SSL encryption
# ──────────────────────────────────────────────────────────────────────────────────────────────────────────────
# Supported by the official API endpoints for Flow testnet (`rest-testnet.onflow.org`) and mainnet
# # (`rest-testnet.onflow.org`).
# You can switch between mainnet and testnet by uncommenting the respective section below
# --------------------------------------------------------------------------------------------------------------

# TESTNET CONFIGURATION
# ~~~~~~~~~~~~~~~~~~~~~~
# [confirmed working] Testnet WS API with SSL ency.
# FLOW_WS_ENDPOINT = os.getenv("FLOW_WS_ENDPOINT", "wss://rest-testnet.onflow.org/v1/ws")

# Filter to two ubiquitous core‑contract events:
# Event `A.8c5303eaa26202d6.EVM.BlockExecuted` is emitted once for _every_ block, including empty ones.
# Every transaction needs to pay fees, hence `A.912d5440f7e3769e.FlowFees.FeesDeducted` is emitted once
# for every transaction.
# CAUTION: the precise name of the events differs between testnet and mainnet. These only fork for testnet:
# EVENT_TYPES = ["A.912d5440f7e3769e.FlowFees.FeesDeducted", "A.8c5303eaa26202d6.EVM.BlockExecuted"]

# MAINNET CONFIGURATION
# ~~~~~~~~~~~~~~~~~~~~~~

# [confirmed working] Mainnet WS API with SSL ency.
FLOW_WS_ENDPOINT = os.getenv("FLOW_WS_ENDPOINT", "wss://rest-mainnet.onflow.org/v1/ws")

# Filter to two ubiquitous core‑contract events:
# Event `A.e467b9dd11fa00df.EVM.BlockExecuted` is emitted once for _every_ block, including empty ones.
# Every transaction needs to pay fees, hence `A.f919ee77447b7497.FlowFees.FeesDeducted` is emitted once
# for every transaction.
# CAUTION: the precise name of the events differs between testnet and mainnet. These only fork for mainnet:
EVENT_TYPES = [ "A.f919ee77447b7497.FlowFees.FeesDeducted", "A.e467b9dd11fa00df.EVM.BlockExecuted"]


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
        payload = msgspec.json.decode(raw)
        # Heartbeat frames have an empty events list; normalise printout
        if payload.get("topic") == "events":
            block_h = payload["payload"]["block_height"]
            evts    = payload["payload"]["events"]
            if evts:
                print(f"\n🔔 block {block_h}, {len(evts)}  event(s):")
                for e in evts:
                    print(f"  • {e['type']} tx={e['transaction_id'][:8]}…")
            else:
                print(f"⏳ heartbeat @ block {block_h}")
        else:
            # subscription ack / error or future topic
            print("⚙️ server response: ", json.dumps(payload, indent=2))


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

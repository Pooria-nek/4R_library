# 4R — 4-Channel Relay Subdevice Library (HDL Buspro-style)

Arduino-framework library (works with STM32duino / "STM32 Cores" board package)
for a 4-channel relay board that acts as a **subdevice** on a shared RS485 bus,
using HDL Buspro-style addressing (Subnet ID / Device ID) and operation codes.

## Depends on BusproCore

This library now depends on a separate `BusproCore` library, which holds the
shared frame codec, transport (RS485 DE/RE, address filtering), and dispatch
layer used by **every** subdevice on the bus (4R, 4Z, and future boards).
Install `BusproCore` alongside this library. See `BusproCore`'s README for
why this split exists and the full op-code registry across devices.

## ⚠️ CRITICAL: Frame layer is NOT yet verified against real HDL Buspro hardware

This applies to `BusproCore`, not this library directly -- but it governs
everything 4R sends/receives on the wire. See `BusproCore/README.md` and
`BusproCore/src/BusproFrame.cpp` for the full placeholder-format breakdown
and verification plan (`docs/CAPTURE_TEMPLATE.md`).

This library's own layers:

```
src/Relay4R.h/.cpp    <-- relay control + scene logic (the actual "4R" device)
src/SceneStore.h        <-- Area+Scene -> 4-relay-state mapping (RAM-backed for now)
```

(BusproFrame / BusproTransport / BusproDevice now live in BusproCore --
see that library if you need to touch wire-level encoding.)

**Do not deploy this on a bus with real HDL Buspro devices until
`BusproCore/src/BusproFrame.cpp` has been verified against a real capture.**
Every place that needs verification is marked `// TODO_VERIFY_HDL` in that
source file. Specifically unverified (full detail in `BusproCore/README.md`):

1. Sync/leading byte pattern (placeholder: `0xAA 0xAA`)
2. Length field meaning (placeholder: total bytes following the length byte, CRC included)
3. CRC algorithm (placeholder: simple 16-bit additive checksum — NOT real HDL CRC16)
4. Field byte order (placeholder: big-endian for multi-byte fields, matching the
   op-code byte order shown in HDL documentation excerpts)

### How to verify (recommended path)

See `docs/CAPTURE_TEMPLATE.md` in this repo for the full capture procedure.
Once confirmed, update **only** `BusproCore/src/BusproFrame.cpp` — no other
file in BusproCore, 4R, or 4Z should need to change, since everything above
that layer only depends on the decoded `BusproFrame` struct, not on
wire-level details. Fixing it once in BusproCore fixes it for every device
library that depends on it.

## Architecture

```
                 ┌─────────────────────────────────────────┐
   RS485 bus --> │ BusproTransport (byte stream, DE/RE pin) │   } from
  (shared with   │   - finds frame boundaries via Frame     │   } BusproCore
   other         │   - filters: is this frame for ME?       │   } (shared with
   subdevices)   └───────────────────┬───────────────────────┘   } 4Z, etc.)
                                      │ BusproFrame (decoded struct)
                                      v
                 ┌─────────────────────────────────────────┐
                 │ BusproDevice (op-code dispatch table)     │   }
                 │   - 0x0002 Scene Control                  │   } also from
                 │   - 0x0031/0x0032 Single Channel Control  │   } BusproCore
                 │   - 0x0033/0x0034 Read Status             │   }
                 │   - (extensible: register more handlers)  │
                 └───────────────────┬───────────────────────┘
                                      │ calls into
                                      v
                 ┌─────────────────────────────────────────┐
                 │ Relay4R (the actual device)               │   } this
                 │   - 4 relay channels (GPIO control)       │   } library
                 │   - scene table (Area+Scene -> 4 states)  │   } (depends on
                 │   - RAM-only for now; ISceneStore          │   } BusproCore)
                 │     interface ready for EEPROM/Flash later │
                 └─────────────────────────────────────────┘
```

## Supported operations (so far)

| Operation | Code | Direction | Notes |
|---|---|---|---|
| Scene Control | `0x0002` | Master -> 4R | Area (1-254) + Scene (0-254, 0=stop) |
| Single Channel Control | `0x0031` (req) / `0x0032` (resp) | Master <-> 4R | On/Off/Toggle per relay channel |
| Read Status | `0x0033` (req) / `0x0034` (resp) | Master <-> 4R | Returns current state of all 4 relays |

(Op-codes for single-channel control/status are also placeholders pending
verification — only `0x0002` was given to me as confirmed by the user; the
others follow the same numbering family pattern from public HDL documentation
excerpts and are marked accordingly.)

## Scene table

Scene -> relay-state mapping is **not** sent by the master on every Scene Control
command (the wire command only carries Area + Scene numbers). The 4R device must
already know what "Area 3, Scene 5" means in terms of its own 4 relays.

This library stores that table in RAM (`SceneTableRAM`, see `Relay4R.h`) behind
an `ISceneStore` interface, and exposes a serial **configuration sub-protocol**
to let the master (or a config tool) write/read scene entries at runtime. Since
there's no Flash/EEPROM persistence yet, the table resets on power loss — wire
up `ISceneStore` to STM32 flash emulation later without changing `Relay4R`.

## Hardware assumptions

- RS485 transceiver (e.g. MAX485) with a single GPIO controlling combined DE/RE
- 4 GPIO outputs driving relay channels (active-high by default, configurable)
- One shared HardwareSerial port also used by other subdevices on the same bus
  (this device filters by Subnet ID + Device ID and ignores frames not addressed
  to it, including broadcast handling per HDL convention placeholder)

## Quick start

```cpp
#include <Relay4R.h>

// Subnet ID, Device ID, RS485 DE/RE pin, relay pins
Relay4R relay4r(1, 12, /*dePin=*/PA8, (uint8_t[]){PB0, PB1, PB2, PB3});

void setup() {
  Serial1.begin(9600);
  relay4r.begin(&Serial1);
}

void loop() {
  relay4r.poll(); // non-blocking; call frequently
}
```

## Running the desktop logic tests

The library logic (dispatch, addressing, scene lookup, encode/decode
roundtrip) can be tested on a desktop machine without any STM32 hardware,
using a minimal Arduino API stub in `test_stubs/`. This requires BusproCore
checked out alongside this library (adjust the path below to wherever you
placed it):

```sh
g++ -std=c++17 -Wall -Wextra \
  -I test_stubs -I src -I ../BusproCore/src \
  ../BusproCore/src/BusproFrame.cpp ../BusproCore/src/BusproTransport.cpp \
  src/Relay4R.cpp test_stubs/test_main.cpp -o test_4r
./test_4r
```

This validates the C++ logic only -- it cannot validate real HDL wire
compatibility, since the frame format itself is still a placeholder
(see warning at the top of this file).


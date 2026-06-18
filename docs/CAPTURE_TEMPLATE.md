# HDL Buspro Frame Capture Template

Use this to record real captured frames from genuine HDL hardware, so the
placeholder code in `BusproFrame.cpp` can be replaced with a verified
implementation.

## Setup used

- Sniffer/analyzer model: ___________________________
- Tapped onto: A/B differential pair, between [master] and [HDL device name]
- Baud rate observed: ___________________________
- Idle line level (mark/space): ___________________________

## Capture 1

- Trigger action (what you did, e.g. "pressed Scene 5 on touch panel for Area 3"):

  _________________________________________________________________

- Raw bytes (hex, in order received):

  ```
  XX XX XX XX XX XX XX XX XX XX XX XX XX XX
  ```

- Known facts about this command (fill in what you know from the panel config):
  - Source Subnet ID: ____  Source Device ID: ____
  - Target Subnet ID: ____  Target Device ID: ____ (or broadcast?)
  - Expected Operation: Scene Control (0x0002)
  - Expected Area: ____  Expected Scene: ____

## Capture 2 (repeat with a different scene/area to vary payload, helps confirm length/CRC field positions)

- Trigger action:

  _________________________________________________________________

- Raw bytes (hex):

  ```
  XX XX XX XX XX XX XX XX XX XX XX XX XX XX
  ```

- Known facts:
  - Target Subnet ID: ____  Target Device ID: ____
  - Expected Area: ____  Expected Scene: ____

## Capture 3 (ideally a different operation entirely, e.g. single relay on/off or a status read/response pair)

- Trigger action:

  _________________________________________________________________

- Raw bytes (hex):

  ```
  XX XX XX XX XX XX XX XX XX XX XX XX XX XX
  ```

## Notes / anomalies

(Any retries, bus collisions, unexpected extra bytes, gaps between bytes, etc.)

---

### How these get used

Once you have 2-3 real captures, paste the hex here in chat (or attach a
.txt/.csv export from the analyzer) and the CRC/sync/length fields can be
derived empirically:

1. Sync pattern = the bytes that are identical at the start of every capture.
2. Length field = look for a byte whose value correlates with how many bytes
   follow it (varies between captures with different payload sizes, e.g.
   scene control vs status response).
3. CRC = take everything before the suspected CRC bytes and brute-force test
   common 16-bit CRC variants (CRC16-CCITT, CRC16-IBM, CRC16-Modbus, etc. as
   well as simple sum/XOR checksums) until one matches the trailing bytes
   across ALL captures consistently.

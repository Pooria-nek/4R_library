/*
 * Basic4R.ino
 *
 * Example sketch for the 4R relay subdevice library.
 *
 * Target: any Arduino-framework board, including STM32 via STM32duino
 * ("STM32 Cores" board package in the Arduino IDE / PlatformIO).
 *
 * Wiring assumed below (CHANGE TO MATCH YOUR BOARD):
 *   - Serial1 TX/RX wired to an RS485 transceiver (e.g. MAX485) which is
 *     shared with other Buspro-style subdevices on the same bus.
 *   - PA8 drives the transceiver's combined DE/RE pin.
 *   - PB0..PB3 drive 4 relay channels (through appropriate driver
 *     circuitry -- do NOT connect a relay coil directly to a GPIO).
 *
 * !!! REMINDER !!!
 * The wire frame format (BusproFrame.cpp) is a PLACEHOLDER pending
 * verification against real captured HDL Buspro traffic. See the
 * project README and /docs/CAPTURE_TEMPLATE.md before using this on a
 * bus shared with genuine HDL devices.
 */

#include <Relay4R.h>

// This device's own bus address.
constexpr uint8_t MY_SUBNET_ID = 1;
constexpr uint8_t MY_DEVICE_ID = 12;

// RS485 transceiver direction-control pin (set to -1 if using an
// auto-direction-sensing transceiver that needs no GPIO control).
constexpr int8_t DE_PIN = PA8;

// 4 relay output pins, in channel order [1,2,3,4].
const uint8_t kRelayPins[RELAY4R_CHANNEL_COUNT] = {PB0, PB1, PB2, PB3};

Relay4R relay4r(MY_SUBNET_ID, MY_DEVICE_ID, DE_PIN, kRelayPins);

void setup() {
    // Shared bus baud rate -- must match every other subdevice and the
    // master controller. 9600 is a common HDL Buspro default; verify
    // against your real captures.
    relay4r.begin(&Serial1, 9600);

    // Example: seed one scene in RAM at boot (Area 1, Scene 1 -> relay
    // 1 and 3 on, relay 2 and 4 off). In a real deployment you'd likely
    // configure scenes via a serial config tool instead of hardcoding.
    bool scene1[RELAY4R_CHANNEL_COUNT] = {true, false, true, false};
    relay4r.defineScene(/*area=*/1, /*scene=*/1, scene1);
}

void loop() {
    relay4r.poll(); // non-blocking; handles all incoming bus traffic
}

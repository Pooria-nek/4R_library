#pragma once
/*
 * Relay4R.h
 *
 * The "4R" device itself: 4 relay channels + scene engine, exposed as a
 * subdevice on a shared HDL-Buspro-style RS485 bus.
 *
 * This is the class you instantiate in your sketch.
 */

#include <Arduino.h>
#include "BusproFrame.h"
#include "BusproTransport.h"
#include "BusproDevice.h"
#include "SceneStore.h"

class Relay4R {
public:
    // mySubnetId/myDeviceId: this device's own bus address (1-254 each).
    // dePin: RS485 transceiver DE/RE control pin (-1 if not used).
    // relayPins: array of exactly RELAY4R_CHANNEL_COUNT GPIO pins.
    // activeHigh: true if driving the pin HIGH energizes the relay.
    Relay4R(uint8_t mySubnetId, uint8_t myDeviceId, int8_t dePin,
            const uint8_t relayPins[RELAY4R_CHANNEL_COUNT],
            bool activeHigh = true,
            ISceneStore* sceneStore = nullptr);

    void begin(HardwareSerial* serial, uint32_t baud = 9600);

    // Call frequently from loop(); non-blocking.
    void poll();

    // --- Direct relay control (also usable outside of bus commands) ---
    void setRelay(uint8_t channel /*0-3*/, bool on);
    bool getRelay(uint8_t channel) const;
    void setAllRelays(const bool states[RELAY4R_CHANNEL_COUNT]);

    // --- Scene table management (normally driven by a config tool, but
    //     exposed directly too in case you want to seed scenes in code) ---
    bool defineScene(uint8_t area, uint8_t scene, const bool states[RELAY4R_CHANNEL_COUNT]);
    bool removeScene(uint8_t area, uint8_t scene);

    // --- Internal handlers (public so the C-style dispatch table in
    //     BusproDevice can call them; not intended for direct use) ---
    void handleSceneControl(const BusproFrame& frame, BusproTransport& transport);
    void handleSingleChannelControl(const BusproFrame& frame, BusproTransport& transport);
    void handleReadStatusRequest(const BusproFrame& frame, BusproTransport& transport);

private:
    void applyRelayHardware(uint8_t channel);

    uint8_t relayPins_[RELAY4R_CHANNEL_COUNT];
    bool relayState_[RELAY4R_CHANNEL_COUNT] = {false, false, false, false};
    bool activeHigh_;

    SceneTableRAM defaultStore_; // used if caller doesn't supply one
    ISceneStore* sceneStore_;

    BusproTransport transport_;
    BusproDevice device_;
};

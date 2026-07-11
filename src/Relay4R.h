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
#include "MemoryCore.h"

class Relay4R
{
public:
    // mySubnetId/myDeviceId: this device's own bus address (1-254 each).
    // dePin: RS485 transceiver DE/RE control pin (-1 if not used).
    // relayPins: array of exactly RELAY4R_CHANNEL_COUNT GPIO pins.
    // activeHigh: true if driving the pin HIGH energizes the relay.
    Relay4R(
        BusproTransport &bus,
        MemoryCore &flash,
        uint32_t sectorAddress,
        const uint8_t relayPins[RELAY4R_CHANNEL_COUNT],
        bool activeHigh = true);

    bool begin();

    bool firstime();

    bool init();

    bool syncValues();

    void process(const BusproFrame &frame);

    // // Call frequently from loop(); non-blocking.
    // void poll();

    // --- Direct relay control (also usable outside of bus commands) ---
    bool setRelay(uint8_t channel /*0-3*/, bool on);
    bool getRelay(uint8_t channel) const;
    void setAllRelays(const bool states[RELAY4R_CHANNEL_COUNT]);

    // // --- Scene table management (normally driven by a config tool, but
    // //     exposed directly too in case you want to seed scenes in code) ---
    // bool defineScene(uint8_t area, uint8_t scene, const bool states[RELAY4R_CHANNEL_COUNT]);
    // bool removeScene(uint8_t area, uint8_t scene);

    void sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen);

private:
    void applyRelayHardware(uint8_t channel);
    uint32_t findAddress(uint32_t subaddress);
    void readMcuUID();

    uint32_t _memoryaddress; // its the refrens address of data on memoryflash

    BusproTransport &bus_;
    MemoryCore &flash_;

    uint16_t address_;
    uint8_t fuid_[12];
    uint8_t uid_[12];
    uint16_t devType_ = BusproDev::DEVICE_4R; // 4R device type per HDL spec

    uint8_t currentScene = 0;
    uint8_t relayPins_[RELAY4R_CHANNEL_COUNT];
    bool relayState_[RELAY4R_CHANNEL_COUNT] = {false, false, false, false};
    bool activeHigh_;

    bool relayEnable_[RELAY4R_CHANNEL_COUNT] = {false, false, false, false};
    uint8_t relayDelay_[RELAY4R_CHANNEL_COUNT] = {0, 0, 0, 0};
    uint8_t relayProtect_[RELAY4R_CHANNEL_COUNT] = {0, 0, 0, 0};

    void handleReadZone(const BusproFrame &frame);
    void handleModifyZone(const BusproFrame &frame);
    void handleReadZoneRemark(const BusproFrame &frame);
    void handleModifyZoneRemark(const BusproFrame &frame);
    void handleReadSceneRemark(const BusproFrame &frame);
    void handleModifySceneRemark(const BusproFrame &frame);


    // Device
    void handleSearchRequest(const BusproFrame &frame);
    void handleModifyDeviceRemark(const BusproFrame &frame);

    // Channel configuration
    void handleReadChannelRemark(const BusproFrame &frame);
    void handleModifyChannelRemark(const BusproFrame &frame);

    void handleReadChannelEnable(const BusproFrame &frame);
    void handleModifyChannelEnable(const BusproFrame &frame);

    void handleReadChannelOndelay(const BusproFrame &frame);
    void handleModifyChannelOndelay(const BusproFrame &frame);

    void handleReadChannelOnprotect(const BusproFrame &frame);
    void handleModifyChannelOnprotect(const BusproFrame &frame);

    // Control
    void handleSingleChannelControl(const BusproFrame &frame);
    void handleReversingControl(const BusproFrame &frame);
    void handleSceneControl(const BusproFrame &frame);

    // Status
    void handleReadStatusRequest(const BusproFrame &frame);
};

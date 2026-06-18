#pragma once
/*
 * BusproTransport.h
 *
 * Owns the shared HardwareSerial port. Responsibilities:
 *  - Feed incoming bytes into the BusproFrameDecoder
 *  - Filter: only surface frames addressed to this device (or broadcast)
 *  - Drive the RS485 transceiver's DE/RE pin around transmission, since the
 *    bus is half-duplex and shared with other subdevices
 *
 * This layer does NOT know about relays, scenes, or op-code meaning -- it
 * only knows "bytes in, addressed BusproFrame out" and "BusproFrame in,
 * bytes out". Op-code dispatch lives in BusproDevice.
 */

#include <Arduino.h>
#include "BusproFrame.h"

class BusproTransport {
public:
    // dePin: GPIO driving the RS485 transceiver's combined DE+RE (active
    // HIGH = transmit/driver-enabled, LOW = receive). If your transceiver
    // module ties DE and RE separately, tie RE to the inverse externally,
    // or pass -1 and manage it yourself (e.g. auto-direction transceiver).
    BusproTransport(uint8_t mySubnetId, uint8_t myDeviceId, int8_t dePin = -1);

    void begin(HardwareSerial* serial, uint32_t baud = 9600);

    // Call frequently from loop(). Reads any available bytes, decodes
    // frames, and if a complete frame addressed to this device (or
    // broadcast) arrives, returns true and fills outFrame.
    // Frames not addressed to us are silently consumed (shared bus).
    bool poll(BusproFrame& outFrame);

    // Send a frame on the shared bus (handles DE/RE direction switching
    // and waiting for transmit-complete before releasing the bus).
    void send(const BusproFrame& frame);

    uint8_t subnetId() const { return mySubnetId_; }
    uint8_t deviceId() const { return myDeviceId_; }

private:
    bool isAddressedToMe(const BusproFrame& f) const;
    void setDriverEnabled(bool enabled);

    HardwareSerial* serial_ = nullptr;
    uint8_t mySubnetId_;
    uint8_t myDeviceId_;
    int8_t  dePin_;
    BusproFrameDecoder decoder_;
};

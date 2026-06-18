#include "BusproTransport.h"

BusproTransport::BusproTransport(uint8_t mySubnetId, uint8_t myDeviceId, int8_t dePin)
    : mySubnetId_(mySubnetId), myDeviceId_(myDeviceId), dePin_(dePin) {}

void BusproTransport::begin(HardwareSerial* serial, uint32_t baud) {
    serial_ = serial;
    serial_->begin(baud);
    if (dePin_ >= 0) {
        pinMode(dePin_, OUTPUT);
        setDriverEnabled(false); // start in receive mode -- shared bus
    }
}

void BusproTransport::setDriverEnabled(bool enabled) {
    if (dePin_ >= 0) {
        digitalWrite(dePin_, enabled ? HIGH : LOW);
    }
}

bool BusproTransport::isAddressedToMe(const BusproFrame& f) const {
    const bool subnetMatch = (f.dstSubnetId == mySubnetId_) ||
                              (f.dstSubnetId == BusproAddr::BROADCAST_SUBNET);
    const bool deviceMatch = (f.dstDeviceId == myDeviceId_) ||
                              (f.dstDeviceId == BusproAddr::BROADCAST_DEVICE);
    return subnetMatch && deviceMatch;
}

bool BusproTransport::poll(BusproFrame& outFrame) {
    if (!serial_) return false;

    while (serial_->available() > 0) {
        const uint8_t b = static_cast<uint8_t>(serial_->read());
        const BusproDecodeResult result = decoder_.feed(b, outFrame);

        if (result == BusproDecodeResult::FRAME_READY) {
            // Shared bus: many frames are not for us -- consume and ignore.
            if (isAddressedToMe(outFrame)) {
                return true;
            }
            // not for us, keep draining the serial buffer this call
        } else if (result == BusproDecodeResult::FRAME_INVALID) {
            // checksum or framing error -- decoder already resynced itself
            continue;
        }
        // IN_PROGRESS / NO_FRAME -> keep reading
    }
    return false;
}

void BusproTransport::send(const BusproFrame& frame) {
    if (!serial_) return;

    uint8_t buf[BUSPRO_MAX_PAYLOAD + 8];
    const uint16_t len = busproEncodeFrame(frame, buf, sizeof(buf));
    if (len == 0) return; // encode failed (oversized payload etc.)

    setDriverEnabled(true);
    serial_->write(buf, len);
    serial_->flush(); // block until all bytes physically transmitted
    setDriverEnabled(false);
}

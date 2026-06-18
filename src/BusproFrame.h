#pragma once
/*
 * BusproFrame.h
 *
 * Decoded representation of one HDL-Buspro-style frame, plus the op-code
 * constants used by this library.
 *
 * !!! WIRE-LEVEL ENCODING IS A PLACEHOLDER, SEE BusproFrame.cpp !!!
 * This header (the decoded struct + op-code numbers) is stable and should
 * NOT need to change when the real frame format is verified -- only the
 * encode/decode functions in BusproFrame.cpp will change.
 */

#include <stdint.h>

// ---- Operation codes -------------------------------------------------
// Only 0x0002 (Scene Control) was given as confirmed by the spec provided.
// The single-channel control / status op-codes below follow the same
// family pattern documented publicly for HDL-Buspro-style devices but are
// UNCONFIRMED -- mark for verification alongside the frame format.
namespace BusproOp {
    constexpr uint16_t SCENE_CONTROL          = 0x0002; // confirmed by user
    constexpr uint16_t SINGLE_CHANNEL_CONTROL = 0x0031; // TODO_VERIFY_HDL
    constexpr uint16_t SINGLE_CHANNEL_RESPONSE= 0x0032; // TODO_VERIFY_HDL
    constexpr uint16_t READ_STATUS_REQUEST    = 0x0033; // TODO_VERIFY_HDL
    constexpr uint16_t READ_STATUS_RESPONSE   = 0x0034; // TODO_VERIFY_HDL
}

// Broadcast convention placeholder -- HDL commonly uses 255 (0xFF) as
// "all subnets" / "all devices". TODO_VERIFY_HDL against real captures.
namespace BusproAddr {
    constexpr uint8_t BROADCAST_SUBNET = 0xFF;
    constexpr uint8_t BROADCAST_DEVICE = 0xFF;
}

// Maximum payload bytes this library will buffer per frame.
// (Generous headroom; real HDL payloads for relay/scene ops are small.)
constexpr uint8_t BUSPRO_MAX_PAYLOAD = 32;

struct BusproFrame {
    uint8_t  srcSubnetId  = 0;
    uint8_t  srcDeviceId  = 0;
    uint8_t  dstSubnetId  = 0;
    uint8_t  dstDeviceId  = 0;
    uint16_t opCode       = 0;
    uint8_t  payload[BUSPRO_MAX_PAYLOAD] = {0};
    uint8_t  payloadLen   = 0;

    void reset() {
        srcSubnetId = srcDeviceId = dstSubnetId = dstDeviceId = 0;
        opCode = 0;
        payloadLen = 0;
    }
};

// Result of feeding one byte into the decoder.
enum class BusproDecodeResult : uint8_t {
    IN_PROGRESS,   // frame not complete yet, keep feeding bytes
    FRAME_READY,   // a complete, checksum-valid frame is available
    FRAME_INVALID, // a frame boundary was found but checksum/length failed
    NO_FRAME       // byte discarded, not part of a frame (resync/noise)
};

// ---- Encode / decode entry points (implemented in BusproFrame.cpp) ----
// Encodes `frame` into `outBuf`, returns number of bytes written (0 on
// failure, e.g. payload too large).
uint16_t busproEncodeFrame(const BusproFrame& frame, uint8_t* outBuf, uint16_t outBufCap);

// Streaming decoder: call once per received byte. Internally buffers state
// between calls. When it returns FRAME_READY, `outFrame` is populated.
class BusproFrameDecoder {
public:
    BusproDecodeResult feed(uint8_t byte, BusproFrame& outFrame);
    void resync(); // discard any partially-received frame and reset state

private:
    // Implementation detail kept private; see BusproFrame.cpp.
    // TODO_VERIFY_HDL: internal state machine encodes the PLACEHOLDER
    // sync/length/CRC assumptions documented in BusproFrame.cpp.
    enum class State : uint8_t { WAIT_SYNC1, WAIT_SYNC2, WAIT_LEN, READ_BODY, WAIT_CRC1, WAIT_CRC2 } state_ = State::WAIT_SYNC1;
    uint8_t  buf_[BUSPRO_MAX_PAYLOAD + 8] = {0};
    uint8_t  bufIdx_ = 0;
    uint8_t  expectedLen_ = 0;
    uint16_t crcAccum_ = 0;
};

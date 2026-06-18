/*
 * BusproFrame.cpp
 *
 * ============================================================
 *  PLACEHOLDER WIRE FORMAT -- NOT VERIFIED AGAINST REAL HDL HW
 * ============================================================
 *
 * This file is the ONLY place that should need to change once real frame
 * captures are available (see /docs/CAPTURE_TEMPLATE.md). Everything above
 * this layer (BusproTransport, BusproDevice, Relay4R) only depends on the
 * BusproFrame struct, not on these byte-level choices.
 *
 * Current placeholder frame layout on the wire:
 *
 *   [0xAA][0xAA]                  2-byte sync (TODO_VERIFY_HDL)
 *   [LEN]                          1 byte: number of bytes from SrcSubnet
 *                                   through end of payload (TODO_VERIFY_HDL:
 *                                   exact length-field convention, e.g.
 *                                   whether it includes itself or the CRC)
 *   [SrcSubnetID][SrcDeviceID]
 *   [DstSubnetID][DstDeviceID]
 *   [OpCode_Hi][OpCode_Lo]
 *   [Payload...]                   PayloadLen = LEN - 6
 *   [Checksum_Hi][Checksum_Lo]      PLACEHOLDER: 16-bit additive sum of
 *                                   every byte from LEN through end of
 *                                   payload (NOT a real CRC16, and almost
 *                                   certainly NOT what real HDL uses --
 *                                   TODO_VERIFY_HDL)
 *
 * Do not trust this checksum to interoperate with genuine HDL devices.
 */

#include "BusproFrame.h"

namespace {
    constexpr uint8_t SYNC1 = 0xAA; // TODO_VERIFY_HDL
    constexpr uint8_t SYNC2 = 0xAA; // TODO_VERIFY_HDL

    // PLACEHOLDER checksum: 16-bit additive sum. Replace with verified
    // HDL CRC16 (or whatever algorithm the captures reveal) in one place.
    uint16_t placeholderChecksum(const uint8_t* data, uint16_t len) {
        uint16_t sum = 0;
        for (uint16_t i = 0; i < len; ++i) {
            sum = static_cast<uint16_t>(sum + data[i]);
        }
        return sum;
    }
}

uint16_t busproEncodeFrame(const BusproFrame& frame, uint8_t* outBuf, uint16_t outBufCap) {
    // body = SrcSubnet,SrcDev,DstSubnet,DstDev,OpHi,OpLo,Payload...
    const uint16_t bodyLen = 6 + frame.payloadLen;
    const uint16_t totalLen = 2 /*sync*/ + 1 /*len*/ + bodyLen + 2 /*checksum*/;

    if (frame.payloadLen > BUSPRO_MAX_PAYLOAD || totalLen > outBufCap || bodyLen > 255) {
        return 0; // doesn't fit / invalid
    }

    uint16_t idx = 0;
    outBuf[idx++] = SYNC1;
    outBuf[idx++] = SYNC2;
    outBuf[idx++] = static_cast<uint8_t>(bodyLen); // TODO_VERIFY_HDL: len convention

    const uint16_t bodyStart = idx;
    outBuf[idx++] = frame.srcSubnetId;
    outBuf[idx++] = frame.srcDeviceId;
    outBuf[idx++] = frame.dstSubnetId;
    outBuf[idx++] = frame.dstDeviceId;
    outBuf[idx++] = static_cast<uint8_t>(frame.opCode >> 8);
    outBuf[idx++] = static_cast<uint8_t>(frame.opCode & 0xFF);
    for (uint8_t i = 0; i < frame.payloadLen; ++i) {
        outBuf[idx++] = frame.payload[i];
    }

    // Checksum covers LEN byte + body (placeholder choice; TODO_VERIFY_HDL
    // whether real HDL includes the length byte and/or sync bytes in CRC).
    const uint16_t checksumLen = 1 + bodyLen;
    const uint16_t checksum = placeholderChecksum(&outBuf[2], checksumLen);
    outBuf[idx++] = static_cast<uint8_t>(checksum >> 8);
    outBuf[idx++] = static_cast<uint8_t>(checksum & 0xFF);

    (void)bodyStart;
    return idx;
}

BusproDecodeResult BusproFrameDecoder::feed(uint8_t byte, BusproFrame& outFrame) {
    switch (state_) {
        case State::WAIT_SYNC1:
            if (byte == SYNC1) {
                state_ = State::WAIT_SYNC2;
            }
            return BusproDecodeResult::NO_FRAME;

        case State::WAIT_SYNC2:
            if (byte == SYNC2) {
                state_ = State::WAIT_LEN;
                bufIdx_ = 0;
            } else if (byte != SYNC1) {
                state_ = State::WAIT_SYNC1;
            }
            // if byte == SYNC1 again, stay in WAIT_SYNC2 (handles AA AA AA ...)
            return BusproDecodeResult::NO_FRAME;

        case State::WAIT_LEN:
            expectedLen_ = byte; // bodyLen, TODO_VERIFY_HDL convention
            if (expectedLen_ < 6 || expectedLen_ > (BUSPRO_MAX_PAYLOAD + 6)) {
                // implausible length, likely noise -- resync
                resync();
                return BusproDecodeResult::FRAME_INVALID;
            }
            buf_[bufIdx_++] = byte; // include LEN in checksum coverage
            state_ = State::READ_BODY;
            return BusproDecodeResult::IN_PROGRESS;

        case State::READ_BODY:
            buf_[bufIdx_++] = byte;
            if (bufIdx_ >= static_cast<uint8_t>(1 + expectedLen_)) {
                state_ = State::WAIT_CRC1;
            }
            return BusproDecodeResult::IN_PROGRESS;

        case State::WAIT_CRC1:
            crcAccum_ = static_cast<uint16_t>(byte) << 8;
            state_ = State::WAIT_CRC2;
            return BusproDecodeResult::IN_PROGRESS;

        case State::WAIT_CRC2: {
            crcAccum_ |= byte;
            const uint16_t computed = placeholderChecksum(buf_, bufIdx_);
            const uint16_t received = crcAccum_; // capture before resync() clears it
            resync(); // ready for next frame regardless of outcome

            if (computed != received) {
                return BusproDecodeResult::FRAME_INVALID;
            }

            // buf_[0] = LEN, buf_[1..6] = src/dst ids + opcode, buf_[7..] = payload
            const uint8_t bodyLen = buf_[0];
            const uint8_t payloadLen = static_cast<uint8_t>(bodyLen - 6);
            if (payloadLen > BUSPRO_MAX_PAYLOAD) {
                return BusproDecodeResult::FRAME_INVALID;
            }

            outFrame.reset();
            outFrame.srcSubnetId = buf_[1];
            outFrame.srcDeviceId = buf_[2];
            outFrame.dstSubnetId = buf_[3];
            outFrame.dstDeviceId = buf_[4];
            outFrame.opCode = static_cast<uint16_t>((buf_[5] << 8) | buf_[6]);
            outFrame.payloadLen = payloadLen;
            for (uint8_t i = 0; i < payloadLen; ++i) {
                outFrame.payload[i] = buf_[7 + i];
            }
            return BusproDecodeResult::FRAME_READY;
        }
    }
    return BusproDecodeResult::NO_FRAME;
}

void BusproFrameDecoder::resync() {
    state_ = State::WAIT_SYNC1;
    bufIdx_ = 0;
    expectedLen_ = 0;
    crcAccum_ = 0;
}

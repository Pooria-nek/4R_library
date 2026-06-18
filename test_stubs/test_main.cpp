// Desktop compile/logic test for the 4R library.
// Builds against test_stubs/Arduino.h instead of the real Arduino core.
// This validates the C++ logic (encode/decode roundtrip, dispatch,
// scene lookup) -- it does NOT validate real HDL wire compatibility,
// since the frame format itself is still a placeholder.

#include <cassert>
#include <cstdio>
#include "Relay4R.h"

static void testEncodeDecodeRoundtrip() {
    BusproFrame original;
    original.srcSubnetId = 1;
    original.srcDeviceId = 100;
    original.dstSubnetId = 1;
    original.dstDeviceId = 12;
    original.opCode = BusproOp::SCENE_CONTROL;
    original.payload[0] = 3;   // area
    original.payload[1] = 5;   // scene
    original.payloadLen = 2;

    uint8_t buf[64];
    uint16_t len = busproEncodeFrame(original, buf, sizeof(buf));
    assert(len > 0);

    BusproFrameDecoder decoder;
    BusproFrame decoded;
    BusproDecodeResult result = BusproDecodeResult::NO_FRAME;
    for (uint16_t i = 0; i < len; ++i) {
        result = decoder.feed(buf[i], decoded);
    }
    assert(result == BusproDecodeResult::FRAME_READY);
    assert(decoded.srcSubnetId == original.srcSubnetId);
    assert(decoded.srcDeviceId == original.srcDeviceId);
    assert(decoded.dstSubnetId == original.dstSubnetId);
    assert(decoded.dstDeviceId == original.dstDeviceId);
    assert(decoded.opCode == original.opCode);
    assert(decoded.payloadLen == original.payloadLen);
    assert(decoded.payload[0] == 3 && decoded.payload[1] == 5);
    std::printf("[PASS] encode/decode roundtrip\n");
}

static void testCorruptedFrameRejected() {
    BusproFrame original;
    original.srcSubnetId = 1;
    original.srcDeviceId = 100;
    original.dstSubnetId = 1;
    original.dstDeviceId = 12;
    original.opCode = BusproOp::SCENE_CONTROL;
    original.payload[0] = 3;
    original.payload[1] = 5;
    original.payloadLen = 2;

    uint8_t buf[64];
    uint16_t len = busproEncodeFrame(original, buf, sizeof(buf));
    buf[len - 1] ^= 0xFF; // corrupt last checksum byte

    BusproFrameDecoder decoder;
    BusproFrame decoded;
    BusproDecodeResult result = BusproDecodeResult::NO_FRAME;
    for (uint16_t i = 0; i < len; ++i) {
        result = decoder.feed(buf[i], decoded);
    }
    assert(result == BusproDecodeResult::FRAME_INVALID);
    std::printf("[PASS] corrupted frame correctly rejected\n");
}

static void testSceneControlAppliesRelays() {
    const uint8_t pins[RELAY4R_CHANNEL_COUNT] = {10, 11, 12, 13};
    Relay4R device(/*subnet=*/1, /*device=*/12, /*dePin=*/-1, pins);

    HardwareSerial fakeSerial;
    device.begin(&fakeSerial, 9600);

    bool sceneStates[RELAY4R_CHANNEL_COUNT] = {true, false, true, false};
    bool ok = device.defineScene(/*area=*/3, /*scene=*/5, sceneStates);
    assert(ok);

    // Build a Scene Control frame addressed to this device (subnet 1, device 12)
    BusproFrame cmd;
    cmd.srcSubnetId = 1;
    cmd.srcDeviceId = 200; // some master
    cmd.dstSubnetId = 1;
    cmd.dstDeviceId = 12;
    cmd.opCode = BusproOp::SCENE_CONTROL;
    cmd.payload[0] = 3; // area
    cmd.payload[1] = 5; // scene
    cmd.payloadLen = 2;

    uint8_t buf[64];
    uint16_t len = busproEncodeFrame(cmd, buf, sizeof(buf));
    fakeSerial.injectBytes(buf, len);

    device.poll(); // should decode, dispatch to handleSceneControl, set relays

    assert(device.getRelay(0) == true);
    assert(device.getRelay(1) == false);
    assert(device.getRelay(2) == true);
    assert(device.getRelay(3) == false);
    std::printf("[PASS] scene control sets relays per stored mapping\n");
}

static void testBroadcastAndWrongAddressFiltering() {
    const uint8_t pins[RELAY4R_CHANNEL_COUNT] = {10, 11, 12, 13};
    Relay4R device(/*subnet=*/1, /*device=*/12, /*dePin=*/-1, pins);
    HardwareSerial fakeSerial;
    device.begin(&fakeSerial, 9600);

    bool sceneStates[RELAY4R_CHANNEL_COUNT] = {true, true, true, true};
    device.defineScene(9, 9, sceneStates);

    // Frame addressed to a DIFFERENT device -- must be ignored
    BusproFrame wrongAddr;
    wrongAddr.srcSubnetId = 1;
    wrongAddr.srcDeviceId = 200;
    wrongAddr.dstSubnetId = 1;
    wrongAddr.dstDeviceId = 99; // not us
    wrongAddr.opCode = BusproOp::SCENE_CONTROL;
    wrongAddr.payload[0] = 9;
    wrongAddr.payload[1] = 9;
    wrongAddr.payloadLen = 2;

    uint8_t buf[64];
    uint16_t len = busproEncodeFrame(wrongAddr, buf, sizeof(buf));
    fakeSerial.injectBytes(buf, len);
    device.poll();
    assert(device.getRelay(0) == false); // untouched
    std::printf("[PASS] frame addressed to other device correctly ignored\n");

    // Broadcast frame -- must be accepted
    BusproFrame broadcast = wrongAddr;
    broadcast.dstSubnetId = BusproAddr::BROADCAST_SUBNET;
    broadcast.dstDeviceId = BusproAddr::BROADCAST_DEVICE;
    len = busproEncodeFrame(broadcast, buf, sizeof(buf));
    fakeSerial.injectBytes(buf, len);
    device.poll();
    assert(device.getRelay(0) == true); // applied this time
    std::printf("[PASS] broadcast frame correctly accepted\n");
}

static void testSingleChannelControlAndResponse() {
    const uint8_t pins[RELAY4R_CHANNEL_COUNT] = {10, 11, 12, 13};
    Relay4R device(/*subnet=*/1, /*device=*/12, /*dePin=*/-1, pins);
    HardwareSerial fakeSerial;
    device.begin(&fakeSerial, 9600);

    BusproFrame cmd;
    cmd.srcSubnetId = 1;
    cmd.srcDeviceId = 200;
    cmd.dstSubnetId = 1;
    cmd.dstDeviceId = 12;
    cmd.opCode = BusproOp::SINGLE_CHANNEL_CONTROL;
    cmd.payload[0] = 2;    // channel 2 (1-based)
    cmd.payload[1] = 0x01; // ON
    cmd.payloadLen = 2;

    uint8_t buf[64];
    uint16_t len = busproEncodeFrame(cmd, buf, sizeof(buf));
    fakeSerial.injectBytes(buf, len);
    device.poll();

    assert(device.getRelay(1) == true); // channel 2 -> index 1

    auto txLog = fakeSerial.takeTxLog();
    assert(!txLog.empty()); // a response should have been sent

    BusproFrameDecoder decoder;
    BusproFrame respDecoded;
    BusproDecodeResult result = BusproDecodeResult::NO_FRAME;
    for (auto b : txLog) result = decoder.feed(b, respDecoded);
    assert(result == BusproDecodeResult::FRAME_READY);
    assert(respDecoded.opCode == BusproOp::SINGLE_CHANNEL_RESPONSE);
    assert(respDecoded.payload[0] == 2);
    assert(respDecoded.payload[1] == 0x01);
    std::printf("[PASS] single channel control + response roundtrip\n");
}

int main() {
    testEncodeDecodeRoundtrip();
    testCorruptedFrameRejected();
    testSceneControlAppliesRelays();
    testBroadcastAndWrongAddressFiltering();
    testSingleChannelControlAndResponse();
    std::printf("\nAll desktop logic tests passed.\n");
    std::printf("REMINDER: this validates library LOGIC only.\n");
    std::printf("Wire format (BusproFrame.cpp) is still UNVERIFIED against real HDL hardware.\n");
    return 0;
}

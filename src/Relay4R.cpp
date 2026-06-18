#include "Relay4R.h"

namespace {
    // Static dispatch table -- op-codes this device responds to.
    // Free functions below adapt the C-style handler signature to the
    // Relay4R instance methods via the `context` pointer.
    void onSceneControl(void* ctx, const BusproFrame& f, BusproTransport& t) {
        static_cast<Relay4R*>(ctx)->handleSceneControl(f, t);
    }
    void onSingleChannelControl(void* ctx, const BusproFrame& f, BusproTransport& t) {
        static_cast<Relay4R*>(ctx)->handleSingleChannelControl(f, t);
    }
    void onReadStatusRequest(void* ctx, const BusproFrame& f, BusproTransport& t) {
        static_cast<Relay4R*>(ctx)->handleReadStatusRequest(f, t);
    }

    const BusproOpBinding kBindings[] = {
        { BusproOp::SCENE_CONTROL,       onSceneControl },
        { BusproOp::SINGLE_CHANNEL_CONTROL, onSingleChannelControl },
        { BusproOp::READ_STATUS_REQUEST, onReadStatusRequest },
    };
    constexpr uint8_t kBindingCount = sizeof(kBindings) / sizeof(kBindings[0]);
}

Relay4R::Relay4R(uint8_t mySubnetId, uint8_t myDeviceId, int8_t dePin,
                  const uint8_t relayPins[RELAY4R_CHANNEL_COUNT],
                  bool activeHigh, ISceneStore* sceneStore)
    : activeHigh_(activeHigh),
      sceneStore_(sceneStore ? sceneStore : nullptr),
      transport_(mySubnetId, myDeviceId, dePin),
      device_(transport_) {
    for (uint8_t i = 0; i < RELAY4R_CHANNEL_COUNT; ++i) relayPins_[i] = relayPins[i];
    if (!sceneStore_) sceneStore_ = &defaultStore_; // default to RAM-backed store
}

void Relay4R::begin(HardwareSerial* serial, uint32_t baud) {
    for (uint8_t i = 0; i < RELAY4R_CHANNEL_COUNT; ++i) {
        pinMode(relayPins_[i], OUTPUT);
        applyRelayHardware(i); // ensure relays start OFF and consistent with relayState_
    }
    transport_.begin(serial, baud);
    device_.configure(kBindings, kBindingCount, this);
}

void Relay4R::poll() {
    device_.poll();
}

void Relay4R::applyRelayHardware(uint8_t channel) {
    const bool on = relayState_[channel];
    const bool pinLevel = activeHigh_ ? on : !on;
    digitalWrite(relayPins_[channel], pinLevel ? HIGH : LOW);
}

void Relay4R::setRelay(uint8_t channel, bool on) {
    if (channel >= RELAY4R_CHANNEL_COUNT) return;
    relayState_[channel] = on;
    applyRelayHardware(channel);
}

bool Relay4R::getRelay(uint8_t channel) const {
    if (channel >= RELAY4R_CHANNEL_COUNT) return false;
    return relayState_[channel];
}

void Relay4R::setAllRelays(const bool states[RELAY4R_CHANNEL_COUNT]) {
    for (uint8_t i = 0; i < RELAY4R_CHANNEL_COUNT; ++i) {
        relayState_[i] = states[i];
        applyRelayHardware(i);
    }
}

bool Relay4R::defineScene(uint8_t area, uint8_t scene, const bool states[RELAY4R_CHANNEL_COUNT]) {
    return sceneStore_->set(area, scene, states);
}

bool Relay4R::removeScene(uint8_t area, uint8_t scene) {
    return sceneStore_->remove(area, scene);
}

// ---------------------------------------------------------------------
// 1.1 Scene Control (op 0x0002)
//
// Payload per spec:
//   byte 0: Area No   (1-254)
//   byte 1: Scene No  (0-254; 0 = "stop scene")
//
// "Stop scene" (Scene No == 0) has no universally defined meaning in the
// excerpt provided -- this implementation treats it as "no-op / scene
// transition stop" and does NOT change relay states, since there's no
// fade/transition engine here to halt. If your actual use case needs
// stop-scene to do something specific (e.g. revert to a "last known"
// state), that behavior should be added explicitly -- flagging this as
// TODO_VERIFY_HDL / TODO_CLARIFY rather than guessing.
// ---------------------------------------------------------------------
void Relay4R::handleSceneControl(const BusproFrame& frame, BusproTransport& /*transport*/) {
    if (frame.payloadLen < 2) return; // malformed, ignore

    const uint8_t area = frame.payload[0];
    const uint8_t scene = frame.payload[1];

    if (scene == 0) {
        // TODO_CLARIFY: "stop scene" semantics undefined for a relay-only
        // device with no transition/fade engine. Currently a no-op.
        return;
    }

    bool states[RELAY4R_CHANNEL_COUNT];
    if (sceneStore_->lookup(area, scene, states)) {
        setAllRelays(states);
    }
    // If no mapping exists for this area/scene, silently ignore (device
    // simply doesn't have that scene defined). Could optionally send a
    // failure/ack response once the real response op-codes are verified.
}

// ---------------------------------------------------------------------
// Single Channel Control (placeholder op 0x0031 / response 0x0032)
//
// Payload convention assumed (TODO_VERIFY_HDL, not from your spec excerpt):
//   byte 0: channel number (1-4)
//   byte 1: requested state (0x00 = off, 0x01 = on, 0xFF/other = toggle)
// Response payload assumed: byte0=channel, byte1=resulting state (0/1)
// ---------------------------------------------------------------------
void Relay4R::handleSingleChannelControl(const BusproFrame& frame, BusproTransport& transport) {
    if (frame.payloadLen < 2) return;

    const uint8_t channel1Based = frame.payload[0];
    const uint8_t requested = frame.payload[1];
    if (channel1Based < 1 || channel1Based > RELAY4R_CHANNEL_COUNT) return;
    const uint8_t channel = channel1Based - 1;

    bool newState;
    if (requested == 0x00) newState = false;
    else if (requested == 0x01) newState = true;
    else newState = !getRelay(channel); // toggle

    setRelay(channel, newState);

    BusproFrame resp;
    resp.srcSubnetId = transport.subnetId();
    resp.srcDeviceId = transport.deviceId();
    resp.dstSubnetId = frame.srcSubnetId;
    resp.dstDeviceId = frame.srcDeviceId;
    resp.opCode = BusproOp::SINGLE_CHANNEL_RESPONSE;
    resp.payload[0] = channel1Based;
    resp.payload[1] = newState ? 0x01 : 0x00;
    resp.payloadLen = 2;
    transport.send(resp);
}

// ---------------------------------------------------------------------
// Read Status (placeholder op 0x0033 request / 0x0034 response)
// Response payload assumed: byte[i] = state of channel i+1 (0/1), i=0..3
// ---------------------------------------------------------------------
void Relay4R::handleReadStatusRequest(const BusproFrame& frame, BusproTransport& transport) {
    BusproFrame resp;
    resp.srcSubnetId = transport.subnetId();
    resp.srcDeviceId = transport.deviceId();
    resp.dstSubnetId = frame.srcSubnetId;
    resp.dstDeviceId = frame.srcDeviceId;
    resp.opCode = BusproOp::READ_STATUS_RESPONSE;
    for (uint8_t i = 0; i < RELAY4R_CHANNEL_COUNT; ++i) {
        resp.payload[i] = getRelay(i) ? 0x01 : 0x00;
    }
    resp.payloadLen = RELAY4R_CHANNEL_COUNT;
    transport.send(resp);
}

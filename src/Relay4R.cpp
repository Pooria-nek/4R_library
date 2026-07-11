#include "Relay4R.h"

Relay4R::Relay4R(
    BusproTransport &bus,
    MemoryCore &flash,
    uint32_t sectorAddress,
    const uint8_t relayPins[RELAY4R_CHANNEL_COUNT],
    bool activeHigh
    // ISceneStore *sceneStore
    )
    : bus_(bus),
      flash_(flash),
      _memoryaddress(sectorAddress),
      activeHigh_(activeHigh)
{
    for (uint8_t i = 0; i < RELAY4R_CHANNEL_COUNT; i++)
        relayPins_[i] = relayPins[i];
}

bool Relay4R::begin()
{
    for (uint8_t i = 0; i < RELAY4R_CHANNEL_COUNT; ++i)
    {
        pinMode(relayPins_[i], OUTPUT);
        applyRelayHardware(i); // ensure relays start OFF and consistent with relayState_
    }

    if (firstime())
    {
        init();
    }

    return syncValues();
}

bool Relay4R::firstime()
{
    uint8_t fuid_[12];
    flash_.read(findAddress(MemoryAdress::DEVICE_MAC_ADDRESS), fuid_, 12);

    readMcuUID();

    bool sameUid = (memcmp(fuid_, uid_, 12) == 0);

    uint16_t fdevType;
    flash_.readObject(findAddress(MemoryAdress::DEVICE_TYPE), fdevType);

    bool sameType = (devType_ == fdevType);

    if (!sameUid || !sameType)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Relay4R::init()
{
    // flash_.eraseSector(_memoryaddress);

    flash_.writeObject(findAddress(MemoryAdress::DEVICE_ADDRESS), uint16_t{0x0102});
    flash_.writeObject(findAddress(MemoryAdress::DEVICE_TYPE), devType_);

    readMcuUID();
    flash_.writeObject(findAddress(MemoryAdress::DEVICE_MAC_ADDRESS), uid_);

    flash_.writeObject(findAddress(MemoryAdress::DEVICE_REMARK), "4Relay");
    flash_.writeObject(findAddress(MemoryAdress::DEVICE_HARDWARE_VER), "hardware");
    flash_.writeObject(findAddress(MemoryAdress::DEVICE_SOFTWARE_VER), "software");

    char remark[20];
    for (size_t i = 1; i <= RELAY4R_CHANNEL_COUNT; i++)
    {
        // uint8_t channel = i + 1;
        snprintf(remark, sizeof(remark), "Relay %u", static_cast<unsigned>(i));
        flash_.writeObject(findAddress(MemoryAdress::channelRemark(i)), remark);

        flash_.writeObject(findAddress(MemoryAdress::channelAddress(MemoryAdress::CHANNEL_ENABLE, i)), uint8_t{0x01});
        flash_.writeObject(findAddress(MemoryAdress::channelAddress(MemoryAdress::CHANNEL_ONDELAY, i)), uint8_t{0x00});
        flash_.writeObject(findAddress(MemoryAdress::channelAddress(MemoryAdress::CHANNEL_ONPROTECT, i)), uint8_t{0x00});
    }

    return true;
}

bool Relay4R::syncValues()
{
    flash_.readObject(findAddress(MemoryAdress::DEVICE_ADDRESS), address_);
    flash_.readObject(findAddress(MemoryAdress::DEVICE_MAC_ADDRESS), uid_);
    flash_.readObject(findAddress(MemoryAdress::CHANNEL_ENABLE), relayEnable_);
    flash_.readObject(findAddress(MemoryAdress::CHANNEL_ONDELAY), relayDelay_);
    flash_.readObject(findAddress(MemoryAdress::CHANNEL_ONPROTECT), relayProtect_);

    return true;
}

void Relay4R::process(const BusproFrame &frame)
{
    // if (frame.devType != devType_)
    //     return;

    if (frame.dstAddress == 0xFFFF) // universal comands
    {
        switch (frame.opCode)
        {
        case BusproOp::SEARCH_REQUEST_HDL:
            handleSearchRequest(frame);
            break;
        }
    }
    else if (frame.dstAddress == address_) // my comands
    {
        switch (frame.opCode)
        {
            // public comands
            // case BusproOp::DEVICE_REMARK.readReq():
            //     handleModifyDevicaRemark(frame);
            //     break;

        case BusproOp::DEVICE_REMARK.writeReq():
            handleModifyDeviceRemark(frame);
            break;

            // private comands
            // case BusproOp::SCENE_CONTROL.req():
            //     handleSceneControl(frame);
            //     break;

        case BusproOp::ZONE_MEMBERS.readReq():
            handleReadZone(frame);
            break;

        case BusproOp::ZONE_MEMBERS.writeReq():
            handleModifyZone(frame);
            break;

        case BusproOp::ZONE_REMARK.readReq():
            handleReadZoneRemark(frame);
            break;

        case BusproOp::ZONE_REMARK.writeReq():
            handleModifyZoneRemark(frame);
            break;

        case BusproOp::SCENE_REMARK.readReq():
            handleReadSceneRemark(frame);
            break;

        case BusproOp::SCENE_REMARK.writeReq():
            handleModifySceneRemark(frame);
            break;

        case BusproOp::SINGLE_CHANNEL.req():
            handleSingleChannelControl(frame);
            break;

        case BusproOp::READ_STATUS.req():
            handleReadStatusRequest(frame);
            break;

        case BusproOp::REVERSING_CONTROL.req():
            handleReversingControl(frame);
            break;

        case BusproOp::CHANNEL_REMARK.readReq():
            handleReadChannelRemark(frame);
            break;

        case BusproOp::CHANNEL_REMARK.writeReq():
            handleModifyChannelRemark(frame);
            break;

        case BusproOp::CHANNEL_ONDELAY.readReq():
            handleReadChannelOndelay(frame);
            break;

        case BusproOp::CHANNEL_ONDELAY.writeReq():
            handleModifyChannelOndelay(frame);
            break;

        case BusproOp::CHANNEL_ONPROTECT.readReq():
            handleReadChannelOnprotect(frame);
            break;

        case BusproOp::CHANNEL_ONPROTECT.writeReq():
            handleModifyChannelOnprotect(frame);
            break;

        case BusproOp::CHANNEL_ENABLE.readReq():
            handleReadChannelEnable(frame);
            break;

        case BusproOp::CHANNEL_ENABLE.writeReq():
            handleModifyChannelEnable(frame);
            break;
        }
    }
}

uint32_t Relay4R::findAddress(uint32_t subaddress)
{
    return ((_memoryaddress * 4096) + subaddress);
}

void Relay4R::sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen)
{
    bus_.send(address_, devType_, opcode, dst, payload, payloadLen);
}

void Relay4R::applyRelayHardware(uint8_t channel)
{
    const bool on = relayState_[channel];
    const bool pinLevel = activeHigh_ ? on : !on;
    if (relayEnable_[channel] == true)
    {
        digitalWrite(relayPins_[channel], pinLevel ? HIGH : LOW);
    }
}

bool Relay4R::setRelay(uint8_t channel, bool on)
{
    if (channel >= RELAY4R_CHANNEL_COUNT)
        return false;
    relayState_[channel] = on;
    applyRelayHardware(channel);
    return true;
}

bool Relay4R::getRelay(uint8_t channel) const
{
    if (channel >= RELAY4R_CHANNEL_COUNT)
        return false;
    return relayState_[channel];
}

void Relay4R::setAllRelays(const bool states[RELAY4R_CHANNEL_COUNT])
{
    for (uint8_t i = 0; i < RELAY4R_CHANNEL_COUNT; ++i)
    {
        relayState_[i] = states[i];
        applyRelayHardware(i);
    }
}

// helper function
constexpr uint8_t relayToBrightness(bool on)
{
    return on ? 100u : 0u;
}

void Relay4R::readMcuUID()
{
    memcpy(uid_, reinterpret_cast<const void *>(0x1FFFF7E8U), sizeof(uid_));
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// REQUEST HANDLERS ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void Relay4R::handleReadZone(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[5 + RELAY4R_CHANNEL_COUNT];

    payload[0] = 0x01; // UNKNOWN
    payload[1] = 0xD0; // UNKNOWN
    payload[2] = static_cast<uint8_t>(address_ >> 8);
    payload[3] = static_cast<uint8_t>(address_ & 0xFF);

    flash_.read(findAddress(MemoryAdress::CHANNEL_ZONE), payload + 5, RELAY4R_CHANNEL_COUNT);

    // Calculate the highest assigned zone number.
    uint8_t maxZone = 0;
    for (uint8_t i = 0; i < RELAY4R_CHANNEL_COUNT; ++i)
    {
        if (payload[5 + i] > maxZone)
            maxZone = payload[5 + i];
    }

    payload[4] = maxZone;

    sendResponse(BusproOp::ZONE_MEMBERS.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleModifyZone(const BusproFrame &frame)
{
    if (frame.payloadLen != (3 + RELAY4R_CHANNEL_COUNT))
        return;

    flash_.update(findAddress(MemoryAdress::CHANNEL_ZONE), frame.payload + 3, RELAY4R_CHANNEL_COUNT);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::ZONE_MEMBERS.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleReadZoneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[21];

    payload[0] = frame.payload[0];

    flash_.read(findAddress(MemoryAdress::zoneRemark(frame.payload[0])), payload + 1, 20);

    sendResponse(BusproOp::ZONE_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleModifyZoneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != (1 + 20))
        return;

    flash_.update(findAddress(MemoryAdress::zoneRemark(frame.payload[0])), frame.payload + 1, 20);

    uint8_t payload[1] = {frame.payload[0]};
    sendResponse(BusproOp::ZONE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleReadSceneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 2)
        return;

    uint8_t payload[22];

    payload[0] = frame.payload[0];
    payload[1] = frame.payload[1];

    // flash_.read(findAddress(MemoryAdress::sceneRemark(frame.payload[0])), payload + 1, 20);

    sendResponse(BusproOp::SCENE_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleModifySceneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != (1 + 20))
        return;

    // flash_.update(findAddress(MemoryAdress::sceneRemark(frame.payload[0])), frame.payload + 1, 20);

    uint8_t payload[1] = {frame.payload[0]};
    sendResponse(BusproOp::SCENE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleSearchRequest(const BusproFrame &frame)
{
    if (frame.payloadLen < 2)
        return;

    uint8_t payload[20];

    flash_.readObject(findAddress(MemoryAdress::DEVICE_REMARK), payload);

    sendResponse(BusproOp::DEVICE_REMARK.readResp(), 0xFFFF, payload, sizeof(payload));
}

void Relay4R::handleModifyDeviceRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 20)
        return;

    flash_.update(findAddress(MemoryAdress::DEVICE_REMARK), frame.payload, frame.payloadLen);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::DEVICE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleModifyChannelRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 21)
        return;

    flash_.update(findAddress(MemoryAdress::channelRemark(frame.payload[0])), frame.payload + 1, frame.payloadLen - 1);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::CHANNEL_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleReadChannelRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[21];

    payload[0] = frame.payload[0];

    flash_.read(findAddress(MemoryAdress::channelRemark(frame.payload[0])), payload + 1, 20);
    sendResponse(BusproOp::CHANNEL_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleModifyChannelOndelay(const BusproFrame &frame)
{
    if (frame.payloadLen != RELAY4R_CHANNEL_COUNT)
        return;

    flash_.update(findAddress(MemoryAdress::channelRemark(frame.payload[0])), frame.payload, frame.payloadLen);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::CHANNEL_ONDELAY.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleReadChannelOndelay(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[4];

    flash_.read(findAddress(MemoryAdress::CHANNEL_ONDELAY), payload, RELAY4R_CHANNEL_COUNT);
    sendResponse(BusproOp::CHANNEL_ONDELAY.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleModifyChannelOnprotect(const BusproFrame &frame)
{
    if (frame.payloadLen != RELAY4R_CHANNEL_COUNT)
        return;

    flash_.update(findAddress(MemoryAdress::channelRemark(frame.payload[0])), frame.payload, frame.payloadLen);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::CHANNEL_ONPROTECT.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleReadChannelOnprotect(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[4];

    flash_.read(findAddress(MemoryAdress::CHANNEL_ONPROTECT), payload, RELAY4R_CHANNEL_COUNT);
    sendResponse(BusproOp::CHANNEL_ONPROTECT.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleModifyChannelEnable(const BusproFrame &frame)
{
    if (frame.payloadLen != (RELAY4R_CHANNEL_COUNT + 1) &&
        frame.payload[0] == RELAY4R_CHANNEL_COUNT)
        return;

    flash_.update(findAddress(MemoryAdress::CHANNEL_ENABLE), frame.payload + 1, RELAY4R_CHANNEL_COUNT);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::CHANNEL_ENABLE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleReadChannelEnable(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[5];
    payload[0] = RELAY4R_CHANNEL_COUNT;

    flash_.read(findAddress(MemoryAdress::CHANNEL_ENABLE), payload + 1, RELAY4R_CHANNEL_COUNT);
    sendResponse(BusproOp::CHANNEL_ENABLE.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleSceneControl(const BusproFrame &frame)
{
    if (frame.payloadLen != 2)
        return; // malformed, ignore

    const uint8_t areaNo = frame.payload[0];  // Area No 1-254
    const uint8_t sceneNo = frame.payload[1]; // Scene No 0-254 -> 0 is for stopping scene

    if (areaNo != 0) // only 1 area it has
    {
        return;
    }

    // TODO
    // bool states[RELAY4R_CHANNEL_COUNT];
    // if (sceneStore_->lookup(areaNo, sceneNo, states))
    // {
    //     setAllRelays(states);
    // }
    // If no mapping exists for this area/scene, silently ignore (device
    // simply doesn't have that scene defined). Could optionally send a
    // failure/ack response once the real response op-codes are verified.

    uint8_t payload[] = {areaNo, sceneNo};
    sendResponse(BusproOp::SCENE_CONTROL.readResp(), 0xFFFF, payload, sizeof(payload));
}

void Relay4R::handleSingleChannelControl(const BusproFrame &frame)
{
    if (frame.payloadLen < 4)
        return;

    const uint8_t lightChannelNo = frame.payload[0];
    const uint8_t Brightness = frame.payload[1];  // 0x00 -> low 0x64 -> high
    const uint8_t highRuntime = frame.payload[2]; // TODO
    const uint8_t lowRuntime = frame.payload[3];  // TODO

    if (lightChannelNo < 1 || lightChannelNo > RELAY4R_CHANNEL_COUNT)
        return;

    const uint8_t channel = lightChannelNo - 1;

    bool newState;
    if (Brightness == 0x00)
        newState = false;
    else if (Brightness == 0x64)
        newState = true;
    else
        newState = false;

    setRelay(channel, newState);

    uint8_t payload[] = {lightChannelNo, relayToBrightness(getRelay(channel))};
    sendResponse(BusproOp::SINGLE_CHANNEL.resp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleReadStatusRequest(const BusproFrame &frame)
{
    if (frame.payloadLen > 0)
        return;

    uint8_t payload[] = {
        relayToBrightness(getRelay(0)),
        relayToBrightness(getRelay(1)),
        relayToBrightness(getRelay(2)),
        relayToBrightness(getRelay(3))};

    sendResponse(BusproOp::READ_STATUS.resp(), frame.srcAddress, payload, sizeof(payload));
}

void Relay4R::handleReversingControl(const BusproFrame &frame)
{
    if (frame.payloadLen < 4)
        return;

    const uint8_t lightChannelNo = frame.payload[0];
    const uint8_t Brightness = frame.payload[1];  // 0x00 -> low 0x64 -> high
    const uint8_t highRuntime = frame.payload[2]; // TODO
    const uint8_t lowRuntime = frame.payload[3];  // TODO

    if (lightChannelNo < 1 || lightChannelNo > RELAY4R_CHANNEL_COUNT)
        return;

    const uint8_t channel = lightChannelNo - 1;

    bool newState;
    if (Brightness == 0x00)
        newState = true;
    else if (Brightness == 0x64)
        newState = false;
    else
        newState = false;

    setRelay(channel, newState);

    uint8_t payload[] = {lightChannelNo, relayToBrightness(getRelay(channel))};
    sendResponse(BusproOp::REVERSING_CONTROL.resp(), frame.srcAddress, payload, sizeof(payload));
}
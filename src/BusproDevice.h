#pragma once
/*
 * BusproDevice.h
 *
 * Generic op-code dispatch on top of BusproTransport. A specific device
 * (like Relay4R) registers handlers for the op-codes it cares about; this
 * class just routes decoded frames to the right handler and ignores
 * everything else (other devices' traffic shares the same bus).
 */

#include "BusproTransport.h"

// Handler signature: given the incoming frame (already filtered to be
// addressed to us), do whatever's needed and optionally send a response
// via `transport`. Returning is enough; sending a response is the
// handler's job via transport.send(...).
using BusproOpHandler = void(*)(void* context, const BusproFrame& frame, BusproTransport& transport);

struct BusproOpBinding {
    uint16_t opCode;
    BusproOpHandler handler;
};

class BusproDevice {
public:
    BusproDevice(BusproTransport& transport) : transport_(transport) {}

    // bindings/count: a static table the concrete device sets up once.
    // context: opaque pointer passed back into handlers (e.g. the Relay4R
    // instance), so handlers can be free functions / static methods.
    void configure(const BusproOpBinding* bindings, uint8_t count, void* context) {
        bindings_ = bindings;
        bindingCount_ = count;
        context_ = context;
    }

    // Call frequently from loop().
    void poll() {
        BusproFrame frame;
        if (transport_.poll(frame)) {
            for (uint8_t i = 0; i < bindingCount_; ++i) {
                if (bindings_[i].opCode == frame.opCode) {
                    bindings_[i].handler(context_, frame, transport_);
                    return;
                }
            }
            // Unrecognized op-code addressed to us -- ignored for now.
            // (Could add an "unknown op-code" hook here later if needed.)
        }
    }

    BusproTransport& transport() { return transport_; }

private:
    BusproTransport& transport_;
    const BusproOpBinding* bindings_ = nullptr;
    uint8_t bindingCount_ = 0;
    void* context_ = nullptr;
};

#pragma once
/*
 * SceneStore.h
 *
 * Storage for the Area+Scene -> 4-relay-state mapping.
 *
 * Scene Control (op 0x0002) only carries Area + Scene numbers on the wire --
 * the device must already know what those numbers mean for its own relays.
 * This interface lets that lookup table live in RAM today and move to
 * STM32 flash emulation / EEPROM later without changing Relay4R at all.
 */

#include <stdint.h>

constexpr uint8_t RELAY4R_CHANNEL_COUNT = 4;
constexpr uint8_t MAX_SCENE_ENTRIES = 32; // tune to taste / available RAM

struct SceneEntry {
    bool used = false;
    uint8_t area = 0;   // 1-254
    uint8_t scene = 0;  // 0-254 (0 = "stop scene", handled by caller, not stored)
    bool relayState[RELAY4R_CHANNEL_COUNT] = {false, false, false, false};
};

class ISceneStore {
public:
    virtual ~ISceneStore() = default;

    // Returns true and fills outStates if a mapping exists for area/scene.
    virtual bool lookup(uint8_t area, uint8_t scene, bool outStates[RELAY4R_CHANNEL_COUNT]) const = 0;

    // Adds or overwrites the mapping for area/scene. Returns false if the
    // table is full and area/scene wasn't already present.
    virtual bool set(uint8_t area, uint8_t scene, const bool states[RELAY4R_CHANNEL_COUNT]) = 0;

    virtual bool remove(uint8_t area, uint8_t scene) = 0;
};

// RAM-only implementation. Resets to empty on power loss.
// TODO: replace with a flash/EEPROM-backed ISceneStore for persistence
// once available on the target STM32 board -- Relay4R only depends on
// ISceneStore, so no other code needs to change.
class SceneTableRAM : public ISceneStore {
public:
    bool lookup(uint8_t area, uint8_t scene, bool outStates[RELAY4R_CHANNEL_COUNT]) const override {
        for (const auto& e : entries_) {
            if (e.used && e.area == area && e.scene == scene) {
                for (uint8_t i = 0; i < RELAY4R_CHANNEL_COUNT; ++i) outStates[i] = e.relayState[i];
                return true;
            }
        }
        return false;
    }

    bool set(uint8_t area, uint8_t scene, const bool states[RELAY4R_CHANNEL_COUNT]) override {
        for (auto& e : entries_) {
            if (e.used && e.area == area && e.scene == scene) {
                for (uint8_t i = 0; i < RELAY4R_CHANNEL_COUNT; ++i) e.relayState[i] = states[i];
                return true;
            }
        }
        for (auto& e : entries_) {
            if (!e.used) {
                e.used = true;
                e.area = area;
                e.scene = scene;
                for (uint8_t i = 0; i < RELAY4R_CHANNEL_COUNT; ++i) e.relayState[i] = states[i];
                return true;
            }
        }
        return false; // table full
    }

    bool remove(uint8_t area, uint8_t scene) override {
        for (auto& e : entries_) {
            if (e.used && e.area == area && e.scene == scene) {
                e.used = false;
                return true;
            }
        }
        return false;
    }

private:
    SceneEntry entries_[MAX_SCENE_ENTRIES];
};

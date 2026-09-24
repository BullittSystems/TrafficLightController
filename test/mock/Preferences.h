/*
 * Preferences.h (native test mock)
 *
 * In-memory stand-in for the ESP32 Preferences (NVS) library. Values persist for
 * the process lifetime, keyed by "namespace/key", so Settings load/save round-trips
 * can be tested. mock_prefs_clear() resets the store between tests.
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>

inline std::map<std::string, uint32_t> mock_prefs_store;

inline void mock_prefs_clear() {
    mock_prefs_store.clear();
}

class Preferences {
  public:
    bool begin(const char* name, bool = false) {
        namespace_ = name;
        return true;
    }
    void end() {}

    bool isKey(const char* key) { return mock_prefs_store.count(fullKey(key)) > 0; }

    uint32_t getULong(const char* key, uint32_t defaultValue = 0) {
        auto it = mock_prefs_store.find(fullKey(key));
        return it == mock_prefs_store.end() ? defaultValue : it->second;
    }
    void putULong(const char* key, uint32_t value) { mock_prefs_store[fullKey(key)] = value; }

    uint8_t getUChar(const char* key, uint8_t defaultValue = 0) {
        auto it = mock_prefs_store.find(fullKey(key));
        return it == mock_prefs_store.end() ? defaultValue : static_cast<uint8_t>(it->second);
    }
    void putUChar(const char* key, uint8_t value) { mock_prefs_store[fullKey(key)] = value; }

  private:
    std::string fullKey(const char* key) const { return namespace_ + "/" + key; }
    std::string namespace_;
};

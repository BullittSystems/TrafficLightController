/*
 * Preferences.h (native test mock)
 *
 * In-memory stand-in for the ESP32 Preferences (NVS) library. Values persist for
 * the process lifetime, keyed by "namespace/key", so Settings load/save round-trips
 * can be tested. mock_prefs_clear() resets the store between tests.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

inline std::map<std::string, uint32_t> mock_prefs_store;
inline std::map<std::string, std::string> mock_prefs_strings;

inline void mock_prefs_clear() {
    mock_prefs_store.clear();
    mock_prefs_strings.clear();
}

class Preferences {
  public:
    bool begin(const char* name, bool = false) {
        namespace_ = name;
        return true;
    }
    void end() {}

    bool isKey(const char* key) {
        const std::string k = fullKey(key);
        return mock_prefs_store.count(k) + mock_prefs_strings.count(k) > 0;
    }

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

    // Mirrors the ESP32 API: returns bytes written including the terminator, or 0
    // if the key is missing or the buffer is too small.
    size_t getString(const char* key, char* value, size_t maxLen) {
        auto it = mock_prefs_strings.find(fullKey(key));
        if (it == mock_prefs_strings.end() || it->second.size() + 1 > maxLen) {
            return 0;
        }
        std::memcpy(value, it->second.c_str(), it->second.size() + 1);
        return it->second.size() + 1;
    }
    size_t putString(const char* key, const char* value) {
        mock_prefs_strings[fullKey(key)] = value;
        return std::strlen(value);
    }

    bool remove(const char* key) {
        const std::string k = fullKey(key);
        return mock_prefs_store.erase(k) + mock_prefs_strings.erase(k) > 0;
    }

  private:
    std::string fullKey(const char* key) const { return namespace_ + "/" + key; }
    std::string namespace_;
};

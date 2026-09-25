/*
 * KnownNetworks.h
 *
 * Copyright (c) 2026 BullittSystems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include <stdint.h>

// One saved Wi-Fi network. Sizes follow 802.11: SSID up to 32 bytes, WPA2
// passphrase up to 63 chars (or a 64-hex-digit PSK), each plus a terminator.
struct KnownNetwork {
    char ssid[33];
    char password[65];
};

static constexpr uint8_t MAX_KNOWN_NETWORKS = 5;

// Point-in-time view of the Wi-Fi configuration for the API. SSIDs only; passwords
// never leave the provisioner. Empty strings mean "none".
struct WifiSnapshot {
    char ssids[MAX_KNOWN_NETWORKS][33];
    uint8_t count;
    char session[33];    // portal network not in the list (RAM only)
    char connected[33];  // network currently joined
};

// Persistent, user-curated list of Wi-Fi networks in insertion order. The order is
// never changed by connecting, so the list is only written when it is edited.
// Fixed storage, no heap. Not thread-safe: callers serialize access.
class KnownNetworks {
  public:
    enum class AddResult : uint8_t { Added, Updated, Full, NotFound, Invalid };

    // Read the list from NVS, dropping any malformed entries.
    void load();

    // Append a network, or update its password in place if the SSID is already
    // known. Returns Full (changing nothing) when the list is full. Persists.
    AddResult add(const char* ssid, const char* password);

    // Overwrite the entry for oldSsid in place with a new network, keeping its
    // position. If ssid is already known, just updates that entry (like add()).
    // Returns NotFound if oldSsid is not in the list. Persists.
    AddResult replace(const char* oldSsid, const char* ssid, const char* password);

    // Remove a network. Returns false if it was not known. Persists.
    bool forget(const char* ssid);

    // Remove every network. Persists.
    void clear();

    // Index of the SSID in the list, or -1.
    int find(const char* ssid) const;

    uint8_t count() const { return count_; }
    const KnownNetwork& at(uint8_t index) const { return nets_[index]; }

    static bool validSsid(const char* ssid);
    static bool validPassword(const char* password);

  private:
    void save() const;

    KnownNetwork nets_[MAX_KNOWN_NETWORKS] = {};
    uint8_t count_                         = 0;
};

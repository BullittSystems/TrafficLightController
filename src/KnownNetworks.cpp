/*
 * KnownNetworks.cpp
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
#include "KnownNetworks.h"

#include <Preferences.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace {
// Kept in its own namespace, apart from the app settings, since it holds secrets.
constexpr char NAMESPACE[] = "tlwifi";
constexpr char KEY_COUNT[] = "count";

// NVS keys "s0".."s4" (SSID) and "p0".."p4" (password).
void keyFor(char* out, size_t len, char prefix, uint8_t index) {
    snprintf(out, len, "%c%u", prefix, static_cast<unsigned>(index));
}

void copyStr(char* dst, size_t dstLen, const char* src) {
    strncpy(dst, src, dstLen - 1);
    dst[dstLen - 1] = '\0';
}
}  // namespace

bool KnownNetworks::validSsid(const char* ssid) {
    const size_t len = strlen(ssid);
    return len >= 1 && len <= 32;
}

bool KnownNetworks::validPassword(const char* password) {
    const size_t len = strlen(password);
    if (len == 0 || (len >= 8 && len <= 63)) {
        return true;  // open network, or a WPA2 passphrase
    }
    if (len == 64) {  // raw PSK: exactly 64 hex digits
        for (size_t i = 0; i < len; i++) {
            if (!isxdigit(static_cast<unsigned char>(password[i]))) {
                return false;
            }
        }
        return true;
    }
    return false;
}

void KnownNetworks::load() {
    Preferences prefs;
    count_ = 0;
    if (!prefs.begin(NAMESPACE, /*readOnly=*/true)) {
        return;  // namespace not created yet (first boot / after erase)
    }
    uint8_t stored = prefs.getUChar(KEY_COUNT, 0);
    if (stored > MAX_KNOWN_NETWORKS) {
        stored = MAX_KNOWN_NETWORKS;
    }

    for (uint8_t i = 0; i < stored; i++) {
        char key[8];
        KnownNetwork net = {};
        keyFor(key, sizeof(key), 's', i);
        prefs.getString(key, net.ssid, sizeof(net.ssid));
        keyFor(key, sizeof(key), 'p', i);
        prefs.getString(key, net.password, sizeof(net.password));
        if (validSsid(net.ssid) && validPassword(net.password)) {
            nets_[count_++] = net;  // compacts past any malformed entries
        }
    }
    prefs.end();
}

void KnownNetworks::save() const {
    Preferences prefs;
    prefs.begin(NAMESPACE, /*readOnly=*/false);
    prefs.putUChar(KEY_COUNT, count_);
    for (uint8_t i = 0; i < MAX_KNOWN_NETWORKS; i++) {
        char sKey[8];
        char pKey[8];
        keyFor(sKey, sizeof(sKey), 's', i);
        keyFor(pKey, sizeof(pKey), 'p', i);
        if (i < count_) {
            prefs.putString(sKey, nets_[i].ssid);
            prefs.putString(pKey, nets_[i].password);
        } else {
            // Drop vacated slots so a forgotten password does not linger in flash.
            // isKey() guards against the core logging an error for absent keys.
            if (prefs.isKey(sKey)) {
                prefs.remove(sKey);
            }
            if (prefs.isKey(pKey)) {
                prefs.remove(pKey);
            }
        }
    }
    prefs.end();
}

int KnownNetworks::find(const char* ssid) const {
    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(nets_[i].ssid, ssid) == 0) {
            return i;
        }
    }
    return -1;
}

KnownNetworks::AddResult KnownNetworks::add(const char* ssid, const char* password) {
    if (!validSsid(ssid) || !validPassword(password)) {
        return AddResult::Invalid;
    }

    const int existing = find(ssid);
    if (existing >= 0) {
        KnownNetwork& net = nets_[existing];
        if (strcmp(net.password, password) != 0) {  // skip the write if unchanged
            copyStr(net.password, sizeof(net.password), password);
            save();
        }
        return AddResult::Updated;
    }

    if (count_ == MAX_KNOWN_NETWORKS) {
        return AddResult::Full;
    }

    copyStr(nets_[count_].ssid, sizeof(nets_[count_].ssid), ssid);
    copyStr(nets_[count_].password, sizeof(nets_[count_].password), password);
    count_++;
    save();
    return AddResult::Added;
}

KnownNetworks::AddResult KnownNetworks::replace(const char* oldSsid, const char* ssid,
                                                const char* password) {
    if (!validSsid(ssid) || !validPassword(password)) {
        return AddResult::Invalid;
    }
    if (find(ssid) >= 0) {
        return add(ssid, password);  // already saved; nothing to displace
    }

    const int index = find(oldSsid);
    if (index < 0) {
        return AddResult::NotFound;
    }
    copyStr(nets_[index].ssid, sizeof(nets_[index].ssid), ssid);
    copyStr(nets_[index].password, sizeof(nets_[index].password), password);
    save();
    return AddResult::Added;
}

bool KnownNetworks::forget(const char* ssid) {
    const int index = find(ssid);
    if (index < 0) {
        return false;
    }
    for (uint8_t i = static_cast<uint8_t>(index); i + 1 < count_; i++) {
        nets_[i] = nets_[i + 1];
    }
    count_--;
    nets_[count_] = {};
    save();
    return true;
}

void KnownNetworks::clear() {
    for (uint8_t i = 0; i < count_; i++) {
        nets_[i] = {};
    }
    count_ = 0;
    save();
}

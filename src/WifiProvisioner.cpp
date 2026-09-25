/*
 * WifiProvisioner.cpp
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
#include "WifiProvisioner.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_attr.h>
#include <string.h>

#include "config.h"

namespace {
// File-local so WiFiManager's headers (which pull in the core WebServer's HTTP_*
// enum) never reach code that also includes ESPAsyncWebServer.
WiFiManager wm;

// Set by requestPortalOnBoot(). RTC_NOINIT survives a software restart but holds
// garbage after power-on, hence a magic value rather than a bool.
constexpr uint32_t PORTAL_ON_BOOT_MAGIC = 0x504F5254;  // "PORT"
RTC_NOINIT_ATTR uint32_t portalOnBoot;

void copySsid(char (&dst)[33], const char* src) {
    strncpy(dst, src, sizeof(dst) - 1);
    dst[sizeof(dst) - 1] = '\0';
}
}  // namespace

WifiProvisioner::WifiProvisioner() : mutex_(xSemaphoreCreateMutex()) {}

void WifiProvisioner::begin() {
    const bool portalRequested = portalOnBoot == PORTAL_ON_BOOT_MAGIC;
    portalOnBoot               = 0;

    wm.setConfigPortalBlocking(false);
    wm.setConnectTimeout(WIFI_PORTAL_CONNECT_TIMEOUT_S);
    WiFi.mode(WIFI_STA);
#ifdef TLC_BOARD_C3
    // The C3 SuperMini's antenna layout often fails to associate at full TX power;
    // capping it is the widely reported fix. Must follow WiFi.mode().
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
#endif

    xSemaphoreTake(mutex_, portMAX_DELAY);
    networks_.load();

    // One-time migration: adopt the single network the core/WiFiManager saved before
    // multi-network support existed, so an upgraded device stays connected.
    if (networks_.count() == 0 && wm.getWiFiIsSaved()) {
        const String ssid = wm.getWiFiSSID();
        const String pass = wm.getWiFiPass();
        if (networks_.add(ssid.c_str(), pass.c_str()) == KnownNetworks::AddResult::Added) {
            Serial.printf("WiFi: migrated saved network '%s'\n", ssid.c_str());
            // Clear the core's copy so the list is the only store; otherwise forgetting
            // every network would be undone by re-migrating on the next boot.
            wm.resetSettings();
        }
    }
    xSemaphoreGive(mutex_);

    // The list is the source of truth; don't also rewrite the core's single-network
    // NVS slot every time we join a different network.
    WiFi.persistent(false);

    if (portalRequested) {
        Serial.println("WiFi: portal requested (saved networks kept)");
        startPortal();
    } else {
        startScanOrPortal();
    }
}

void WifiProvisioner::requestPortalOnBoot() {
    portalOnBoot = PORTAL_ON_BOOT_MAGIC;
}

void WifiProvisioner::process() {
    switch (state_) {
        case State::Scanning: {
            const int16_t found = WiFi.scanComplete();
            if (found == WIFI_SCAN_RUNNING) {
                if (millis() - stateStartMs_ >= WIFI_SCAN_TIMEOUT_MS) {
                    Serial.println("WiFi: scan timed out");
                    WiFi.scanDelete();
                    startPortal();
                }
            } else if (found < 0) {
                Serial.println("WiFi: scan failed");
                startPortal();
            } else {
                connectToBestScanResult(found);
                WiFi.scanDelete();
            }
            break;
        }

        case State::Connecting:
            if (WiFi.status() == WL_CONNECTED) {
                enter(State::Connected);
            } else if (millis() - stateStartMs_ >= WIFI_STA_CONNECT_TIMEOUT_MS) {
                Serial.printf("WiFi: '%s' did not connect\n", connectingSsid_);
                startPortal();
            }
            break;

        case State::Connected:
            // Brief drops are handled by the core's auto-reconnect. If we stay down,
            // assume the device moved and look for another known network.
            if (WiFi.status() == WL_CONNECTED) {
                lostSinceMs_ = 0;
            } else if (lostSinceMs_ == 0) {
                lostSinceMs_ = millis();
            } else if (millis() - lostSinceMs_ >= WIFI_STA_CONNECT_TIMEOUT_MS) {
                lostSinceMs_ = 0;
                WiFi.disconnect();
                startScanOrPortal();
            }
            break;

        case State::Portal:
            if (wm.process()) {  // true once the portal has joined a network
                captureProvisionedNetwork();
                enter(State::Connected);
            }
            break;
    }
    logStatusChange();
}

void WifiProvisioner::enter(State state) {
    state_        = state;
    stateStartMs_ = millis();
}

void WifiProvisioner::startScanOrPortal() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const unsigned candidates = networks_.count() + (hasSession_ ? 1 : 0);
    xSemaphoreGive(mutex_);
    if (candidates == 0) {
        startPortal();
        return;
    }
    WiFi.scanNetworks(/*async=*/true);
    enter(State::Scanning);
    Serial.printf("WiFi: scanning for %u known network(s)...\n", candidates);
}

const KnownNetwork* WifiProvisioner::credentialsFor(const char* ssid) const {
    const int index = networks_.find(ssid);
    if (index >= 0) {
        return &networks_.at(static_cast<uint8_t>(index));
    }
    // The session network is a candidate too, so a brief drop reconnects to it
    // instead of falling back to the portal.
    if (hasSession_ && strcmp(session_.ssid, ssid) == 0) {
        return &session_;
    }
    return nullptr;
}

void WifiProvisioner::connectToBestScanResult(int16_t found) {
    // Pick the strongest visible network that we have credentials for. Copy it out
    // under the lock, since a handler may edit the list once the lock is released.
    KnownNetwork best = {};
    int32_t bestRssi  = INT32_MIN;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    for (int16_t i = 0; i < found; i++) {
        const KnownNetwork* net = credentialsFor(WiFi.SSID(i).c_str());
        if (net != nullptr && WiFi.RSSI(i) > bestRssi) {
            best     = *net;
            bestRssi = WiFi.RSSI(i);
        }
    }
    xSemaphoreGive(mutex_);

    if (best.ssid[0] == '\0') {
        Serial.println("WiFi: no known network in range");
        startPortal();
        return;
    }

    copySsid(connectingSsid_, best.ssid);
    WiFi.begin(best.ssid, best.password);
    enter(State::Connecting);
    Serial.printf("WiFi: connecting to '%s' (%ld dBm)...\n", connectingSsid_,
                  static_cast<long>(bestRssi));
}

void WifiProvisioner::startPortal() {
    // Non-blocking (setConfigPortalBlocking(false)); process() services it.
    wm.startConfigPortal(AP_SSID);
    enter(State::Portal);
    Serial.printf("WiFi: config portal '%s' active (connect to provision)\n", AP_SSID);
}

void WifiProvisioner::captureProvisionedNetwork() {
    // Append the network just entered in the portal. If the list is full, never
    // evict silently: keep it for this session only and let the user decide what to
    // drop via the API.
    const String ssid = WiFi.SSID();
    const String pass = WiFi.psk();
    xSemaphoreTake(mutex_, portMAX_DELAY);
    switch (networks_.add(ssid.c_str(), pass.c_str())) {
        case KnownNetworks::AddResult::Added:
        case KnownNetworks::AddResult::Updated:
            hasSession_ = false;
            Serial.printf("WiFi: saved '%s' (%u known)\n", ssid.c_str(),
                          static_cast<unsigned>(networks_.count()));
            break;
        case KnownNetworks::AddResult::Full:
            strncpy(session_.ssid, ssid.c_str(), sizeof(session_.ssid) - 1);
            session_.ssid[sizeof(session_.ssid) - 1] = '\0';
            strncpy(session_.password, pass.c_str(), sizeof(session_.password) - 1);
            session_.password[sizeof(session_.password) - 1] = '\0';
            hasSession_                                      = true;
            Serial.printf("WiFi: list full, '%s' kept for this session only\n", ssid.c_str());
            break;
        default:
            Serial.printf("WiFi: connected to '%s' but could not save it\n", ssid.c_str());
            break;
    }
    xSemaphoreGive(mutex_);
}

WifiSnapshot WifiProvisioner::snapshot() const {
    WifiSnapshot snap = {};
    xSemaphoreTake(mutex_, portMAX_DELAY);
    snap.count = networks_.count();
    for (uint8_t i = 0; i < snap.count; i++) {
        copySsid(snap.ssids[i], networks_.at(i).ssid);
    }
    if (hasSession_) {
        copySsid(snap.session, session_.ssid);
    }
    xSemaphoreGive(mutex_);
    if (WiFi.status() == WL_CONNECTED) {
        copySsid(snap.connected, WiFi.SSID().c_str());
    }
    return snap;
}

KnownNetworks::AddResult WifiProvisioner::addNetwork(const char* ssid, const char* password) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const KnownNetworks::AddResult result = networks_.add(ssid, password);
    const bool saved =
        result == KnownNetworks::AddResult::Added || result == KnownNetworks::AddResult::Updated;
    if (saved && hasSession_ && strcmp(session_.ssid, ssid) == 0) {
        session_    = {};
        hasSession_ = false;
    }
    xSemaphoreGive(mutex_);
    return result;
}

bool WifiProvisioner::forgetNetwork(const char* ssid) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const bool forgotten = networks_.forget(ssid);
    xSemaphoreGive(mutex_);
    return forgotten;
}

WifiProvisioner::KeepResult WifiProvisioner::keepSessionNetwork(const char* replaceSsid) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    KeepResult keep = KeepResult::NoSession;
    if (hasSession_) {
        const KnownNetworks::AddResult result =
            (replaceSsid != nullptr)
                ? networks_.replace(replaceSsid, session_.ssid, session_.password)
                : networks_.add(session_.ssid, session_.password);
        switch (result) {
            case KnownNetworks::AddResult::Added:
            case KnownNetworks::AddResult::Updated:
                Serial.printf("WiFi: kept session network '%s'\n", session_.ssid);
                session_    = {};
                hasSession_ = false;
                keep        = KeepResult::Kept;
                break;
            case KnownNetworks::AddResult::Full:
                keep = KeepResult::Full;
                break;
            case KnownNetworks::AddResult::NotFound:
                keep = KeepResult::NotFound;
                break;
            default:
                keep = KeepResult::Invalid;
                break;
        }
    }
    xSemaphoreGive(mutex_);
    return keep;
}

bool WifiProvisioner::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

void WifiProvisioner::resetCredentials() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    networks_.clear();
    session_    = {};
    hasSession_ = false;
    xSemaphoreGive(mutex_);
    wm.resetSettings();
}

void WifiProvisioner::logStatusChange() {
    const bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected && !wasConnected_) {
        Serial.printf("WiFi: connected to '%s', IP=%s\n", WiFi.SSID().c_str(),
                      WiFi.localIP().toString().c_str());
    } else if (!connected && wasConnected_) {
        Serial.println("WiFi: connection lost");
    }
    wasConnected_ = connected;
}

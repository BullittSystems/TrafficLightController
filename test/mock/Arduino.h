/*
 * Arduino.h (native test mock)
 *
 * Minimal host-side stand-in for the Arduino core, used only by the native unit
 * tests. Records digital writes and provides a settable millis() clock so timing
 * and lamp-state logic can be asserted without hardware.
 */
#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Pin modes / levels.
#define OUTPUT 0x01
#define INPUT 0x00
#define LOW 0x00
#define HIGH 0x01

// ---- Mock state (C++17 inline variables: one shared instance across TUs) ----
inline uint32_t mock_millis_value = 0;
inline uint8_t mock_pin_state[64] = {0};
inline uint8_t mock_pin_mode[64]  = {0};

inline void mock_reset() {
    mock_millis_value = 0;
    for (int i = 0; i < 64; i++) {
        mock_pin_state[i] = 0;
        mock_pin_mode[i]  = 0;
    }
}

inline void mock_set_millis(uint32_t ms) {
    mock_millis_value = ms;
}
inline void mock_advance_millis(uint32_t ms) {
    mock_millis_value += ms;
}

// ---- Arduino API subset ----
inline uint32_t millis() {
    return mock_millis_value;
}
inline void delay(uint32_t) {}

inline void pinMode(uint8_t pin, uint8_t mode) {
    if (pin < 64)
        mock_pin_mode[pin] = mode;
}
inline void digitalWrite(uint8_t pin, uint8_t value) {
    if (pin < 64)
        mock_pin_state[pin] = value;
}
inline int digitalRead(uint8_t pin) {
    return pin < 64 ? mock_pin_state[pin] : 0;
}

// ---- Serial (no-op sink; logging is not asserted) ----
struct MockSerial {
    void begin(unsigned long) {}
    void println() {}
    void println(const char*) {}
    int printf(const char*, ...) { return 0; }
};
inline MockSerial Serial;

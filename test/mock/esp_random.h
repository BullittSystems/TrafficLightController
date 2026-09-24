/*
 * esp_random.h (native test mock)
 *
 * Deterministic stand-in for the ESP32 hardware RNG. Defaults to a simple LCG so
 * randomness rules can be checked over many iterations; tests may also queue exact
 * values to force specific outcomes (e.g. exercising retry loops).
 */
#pragma once

#include <cstdint>

inline uint32_t mock_rng_state = 0x12345678u;

// Small forced-value queue: when non-empty, esp_random() returns these in order.
inline uint32_t mock_rng_forced[16] = {0};
inline int mock_rng_forced_count    = 0;
inline int mock_rng_forced_index    = 0;

inline void mock_rng_reset(uint32_t seed = 0x12345678u) {
    mock_rng_state        = seed;
    mock_rng_forced_count = 0;
    mock_rng_forced_index = 0;
}

// Queue exact values to be returned by the next esp_random() calls.
inline void mock_rng_force(const uint32_t* values, int count) {
    mock_rng_forced_count = 0;
    mock_rng_forced_index = 0;
    for (int i = 0; i < count && i < 16; i++) {
        mock_rng_forced[mock_rng_forced_count++] = values[i];
    }
}

inline uint32_t esp_random() {
    if (mock_rng_forced_index < mock_rng_forced_count) {
        return mock_rng_forced[mock_rng_forced_index++];
    }
    // Numerical Recipes LCG.
    mock_rng_state = mock_rng_state * 1664525u + 1013904223u;
    return mock_rng_state;
}

/*
 * freertos/FreeRTOS.h (native test mock)
 *
 * Minimal FreeRTOS definitions for host-native tests.
 */
#pragma once

#include <cstdint>

typedef uint32_t TickType_t;
typedef int32_t BaseType_t;

#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xFFFFFFFF

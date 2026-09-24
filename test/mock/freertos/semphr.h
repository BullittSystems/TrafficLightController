/*
 * freertos/semphr.h (native test mock)
 *
 * No-op semaphore for host-native tests. Tests are single-threaded, so the mutex
 * only needs to exist and satisfy take/give.
 */
#pragma once

#include "FreeRTOS.h"

typedef void* SemaphoreHandle_t;

inline SemaphoreHandle_t mock_semaphore_sentinel;

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    return &mock_semaphore_sentinel;
}
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) {
    return pdTRUE;
}
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) {
    return pdTRUE;
}

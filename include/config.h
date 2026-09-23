/*
 * config.h
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

// Central hardware/firmware configuration.
// GPIO assignments resolve the "TBD" in docs/traffic-light-controller-hardware.md.
// These pins are boot-safe on the ESP32-C3 SuperMini: they avoid the BOOT/strap
// pins (GPIO2/8/9), the native USB pins (GPIO18/19), and UART0 (GPIO20/21).

// Lamp channels: active HIGH, low-side N-channel MOSFET drivers.
// GPIO HIGH -> MOSFET ON -> lamp ON.
static constexpr int RED_GPIO = 3;
static constexpr int YELLOW_GPIO = 4;
static constexpr int GREEN_GPIO = 5;

// Wi-Fi provisioning access-point name (used in later tasks).
static constexpr char AP_SSID[] = "TrafficLight-Setup";

// Serial monitor baud (matches platformio.ini monitor_speed).
static constexpr unsigned long SERIAL_BAUD = 115200;

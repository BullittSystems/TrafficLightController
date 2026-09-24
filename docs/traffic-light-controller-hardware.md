# Traffic Light Controller Hardware (Reference Build)

> **Scope:** The firmware's actual contract is hardware-agnostic: three GPIO
> outputs, one per lamp, driven on/off (active-high by default, configurable via
> `LAMP_ACTIVE_HIGH`). What those pins drive is up to the integrator. This document
> describes **one reference implementation** — a 12V bulb + N-channel MOSFET build.
> Any driver that meets the pin contract (LED drivers, relay boards, etc.) works
> without firmware changes.

## Purpose

This document describes the hardware architecture for a 3D-printed
traffic light controller. It is intended to provide hardware context to
a coding agent developing firmware for a **Teyleten Robot ESP32-C3
Development Board / ESP32 SuperMini**.

The traffic light contains all controller electronics internally. The
only external electrical connection required is **12 V DC power**.

The initial use is a Halloween Trunk or Treat display powered from a
battery. Afterward, the same controller will normally be powered from a
12 V AC-to-DC switching power supply in a workshop.

------------------------------------------------------------------------

## System Overview

The controller operates as a nominal **12 V DC system**.

Two interchangeable power sources are planned:

1.  **Portable power:** GOLDENMATE 12 V 20 Ah LiFePO4 battery
2.  **Stationary power:** regulated 12 V AC-to-DC switching power supply

The traffic light itself does not need to know which source is
connected.

Internally, the 12 V input is divided into two branches:

-   A fused 12 V branch powers the three LED lamps directly.
-   A separately fused branch feeds an LM2596 buck converter, which
    produces 5 V for the ESP32-C3 development board.

The ESP32 controls three N-channel MOSFETs. Each MOSFET independently
switches the negative side of one lamp.

------------------------------------------------------------------------

## Controller

### Teyleten Robot ESP32-C3 SuperMini

**Controller:** Teyleten Robot ESP32-C3 Development Board / ESP32
SuperMini

The firmware requires three GPIO outputs:

  Function      GPIO
  ------------- ---------------------
  Red lamp      `RED_GPIO` = GPIO3
  Yellow lamp   `YELLOW_GPIO` = GPIO4
  Green lamp    `GREEN_GPIO` = GPIO5

These pins were chosen to avoid pins that could cause undesirable
behavior during boot or interfere with programming/logging on the
ESP32-C3 SuperMini:

-   GPIO2, GPIO8, GPIO9 are boot/strapping pins (GPIO8 also drives the
    onboard LED and must be high at boot).
-   GPIO18/GPIO19 are the native USB D-/D+ lines used for
    flashing/serial.
-   GPIO20/GPIO21 are the default UART0 TX/RX.

GPIO3, GPIO4, and GPIO5 have no strapping, USB, or UART0 role, and are
physically grouped on the header for clean wiring. The assignments live
in firmware `config.h` and remain changeable if wiring dictates a
different mapping.

### Output Logic

Lamp control is **active HIGH**:

-   GPIO `HIGH` → MOSFET ON → lamp ON
-   GPIO `LOW` → MOSFET OFF → lamp OFF

Firmware should configure all three lamp GPIOs as outputs and drive them
LOW as early as practical during initialization.

Each MOSFET also has a hardware 10 kΩ gate pulldown, so the lamps
default to OFF while the ESP32 is unpowered, resetting, or its GPIOs are
high impedance.

------------------------------------------------------------------------

## Lamps

There are three independently controlled lamps:

-   Red
-   Yellow
-   Green

Each uses an E26 socket and the following bulb type:

**Bulb specifications**

-   E26 medium base
-   A15 form factor
-   LED
-   3 W
-   Approximately 270 lumens
-   4000 K
-   Rated for 12 V to 60 V low-voltage AC/DC operation
-   Approximate current at 12 V: 0.25 A per bulb

Maximum lighting load with all three bulbs illuminated is approximately:

-   9 W
-   0.75 A at 12 V

The center contact of each E26 socket receives fused +12 V. The threaded
shell/switched side connects to the drain of its corresponding MOSFET.

------------------------------------------------------------------------

## MOSFET Lamp Drivers

Three **RFP30N06LE** N-channel logic-level MOSFETs are used, one per
lamp.

Relevant characteristics:

-   N-channel power MOSFET
-   Logic-level gate
-   60 V drain-source rating
-   TO-220 package
-   Far greater current capability than required by the approximately
    250 mA lamp load

The MOSFETs are configured as **low-side switches**.

### One Lamp Channel

``` text
              Fused +12 V
                   |
             E26 center contact
                   |
                LED bulb
                   |
              E26 shell
                   |
                   |
               Drain (D)
              RFP30N06LE
               Source (S)
                   |
                   |
             COMMON GROUND


ESP32 GPIO
     |
   220 ohm
     |
     +------------ Gate (G)
     |
   10 kohm
     |
COMMON GROUND
```

This circuit is duplicated for RED, YELLOW, and GREEN.

### Gate Components

Each MOSFET uses:

-   **220 Ω series resistor** between ESP32 GPIO and MOSFET gate
-   **10 kΩ pulldown resistor** between MOSFET gate and ground

The 10 kΩ pulldown keeps the MOSFET off when the ESP32 GPIO is floating
or during controller startup.

### PWM Capability

The MOSFET architecture allows PWM control if desired later.

Possible future effects include:

-   fading
-   pulsing
-   flickering
-   simulated malfunction effects
-   variable brightness

Basic traffic-light operation only requires digital HIGH/LOW output
control.

------------------------------------------------------------------------

## Power System

### External Input

Only two power conductors need to enter the traffic-light housing:

``` text
+12 V
GND
```

All electronics are mounted inside the traffic light.

### Power Distribution

The intended architecture is:

``` text
                       12 V DC INPUT
                           +     -
                           |     |
                     [optional master
                       power switch]
                           |
              +------------+-------------+
              |                          |
           2 A fuse                    1 A fuse
              |                          |
         +12 V lamp bus                LM2596
              |                     12 V -> 5 V
       +------+------+                   |
       |      |      |                  5 V
      RED   YELLOW  GREEN                |
      bulb   bulb    bulb              ESP32-C3
       |      |      |
      MOSFET MOSFET MOSFET
       |      |      |
       +------+------+-------------------+
              |
         COMMON GROUND
              |
        12 V INPUT GND
```

### Fuses

Two inline 5 × 20 mm fuse holders are available.

Planned protection:

-   **2 A fuse:** 12 V lighting branch
-   **1 A fuse:** controller/buck-converter branch

The fuse holders have 16 AWG leads.

### Buck Converter

**Converter:** LM2596 adjustable DC-to-DC step-down module

The converter receives nominal 12 V and is adjusted to:

**5.00 V output**

The LM2596 output powers the ESP32-C3 development board through the
board's appropriate 5 V/VBUS/VIN input, depending on the exact SuperMini
board pinout.

The converter should be adjusted and verified with a multimeter before
connecting the ESP32.

### Common Ground

The following are electrically common:

-   12 V power-source negative
-   LM2596 IN-
-   LM2596 OUT-
-   ESP32 GND
-   MOSFET sources
-   10 kΩ MOSFET gate pulldowns

The ESP32 must share this ground with the MOSFET switching circuitry.

------------------------------------------------------------------------

## Wiring

### 12 V / Lamp Wiring

Use **18 AWG stranded tinned-copper silicone-insulated wire** for:

-   incoming 12 V power
-   +12 V distribution
-   E26 socket wiring
-   lamp return wiring
-   main ground distribution

Red and black wire are available.

### Control Wiring

Smaller-gauge hookup wire can be used for:

-   ESP32 GPIO to 220 Ω resistors
-   resistor to MOSFET gate
-   10 kΩ pulldowns
-   other low-current controller interconnections

------------------------------------------------------------------------

## Flyback Diodes

No flyback diodes are planned for the lamp channels.

The loads are LED light bulbs rather than solenoids, relays, motors, or
other strongly inductive loads. The flyback protection used in projects
involving inductive coils is therefore not required for these three lamp
outputs.

------------------------------------------------------------------------

## Optional Master Power Switch

A simple SPST master switch may be installed in series with the +12 V
input before it divides into the two fused branches.

Conceptually:

``` text
12 V INPUT +
     |
  SPST SWITCH
     |
     +---- 2 A fuse ---- lighting
     |
     +---- 1 A fuse ---- LM2596 / ESP32
```

This allows the entire traffic light to be switched off regardless of
whether it is connected to the battery or workshop power supply.

------------------------------------------------------------------------

## Power Sources

### Portable Operation

**GOLDENMATE 12 V 20 Ah LiFePO4 battery**

Used primarily for the annual Trunk or Treat event.

The three lamps consume only about 3 W each. Normal traffic-light
sequencing will generally have only one lamp illuminated at a time, so
the 20 Ah battery provides far more capacity than required for a single
evening.

### Workshop Operation

After the event, the same traffic light will be powered from a **12 V
AC-to-DC switching power supply**.

No controller or wiring changes should be required when changing between
battery and AC-powered 12 V operation.

------------------------------------------------------------------------

## Firmware-Facing Hardware Model

From the firmware's perspective, the physical hardware is intentionally
simple.

There are three output channels:

``` text
RED_GPIO     -> red lamp MOSFET
YELLOW_GPIO  -> yellow lamp MOSFET
GREEN_GPIO   -> green lamp MOSFET
```

Each behaves as:

``` text
GPIO LOW  = lamp OFF
GPIO HIGH = lamp ON
```

The first firmware initialization of these pins should conceptually be:

``` text
configure RED_GPIO as output
configure YELLOW_GPIO as output
configure GREEN_GPIO as output

RED_GPIO = LOW
YELLOW_GPIO = LOW
GREEN_GPIO = LOW
```

GPIO assignments are RED_GPIO = GPIO3, YELLOW_GPIO = GPIO4,
GREEN_GPIO = GPIO5 (see the Controller section for rationale), defined in
firmware `config.h`.

No relay timing, relay-contact protection, or mechanical switching
constraints exist because the lamps are MOSFET controlled.

The firmware may switch lamps independently. Electrically, all three
lamps may be on simultaneously, although normal traffic-light sequences
may choose not to do so.

------------------------------------------------------------------------

## Hardware Bill of Materials

  ------------------------------------------------------------------------
                           Qty Component             Purpose
  ---------------------------- --------------------- ---------------------
                             1 Teyleten Robot        Main controller
                               ESP32-C3 SuperMini    

                             3 RFP30N06LE N-channel  Lamp switching
                               MOSFET                

                             3 220 Ω resistor        MOSFET gate series
                                                     resistor

                             3 10 kΩ resistor        MOSFET gate pulldown

                             3 3 W, 12-60 V, E26 A15 Red/yellow/green
                               LED bulb              illumination

                             3 E26 socket            Bulb
                                                     mounting/electrical
                                                     connection

                             1 LM2596 adjustable     12 V to 5 V
                               buck converter        controller supply

                             1 2 A 5×20 mm fuse      Lighting branch
                                                     protection

                             1 1 A 5×20 mm fuse      Controller branch
                                                     protection

                             2 Inline 5×20 mm fuse   Fuse mounting
                               holder                

                     As needed 18 AWG stranded       12 V power/lamp
                               tinned-copper         wiring
                               silicone wire         

                     As needed Smaller hookup wire   GPIO/control wiring

                    1 optional SPST power switch     Master power control

                             1 12 V 20 Ah LiFePO4    Portable power
                               battery               

                             1 12 V AC-to-DC         Workshop power
                               switching supply      
  ------------------------------------------------------------------------

------------------------------------------------------------------------

## Open Hardware Decisions

The following should remain configurable or TBD in the software design:

1.  Final traffic-light timing and sequence.
2.  Whether PWM effects will be implemented.
3.  Whether Wi-Fi, Bluetooth, a web interface, Home Assistant
    integration, or other ESP32 features will be used.
4.  Behavior after boot or reset.
5.  Behavior after loss and restoration of power.

The electrical hardware does not require these software decisions to be
finalized before assembly.

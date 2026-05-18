# RDC6332G HDI Touchscreen Controller

> Custom ESP32 touchscreen replacement panel for RDC6332G laser cutters — jog axes, home, fire, frame, and monitor job progress from a clean touch UI.

---

## Overview

The RDC6332G laser controller includes a DE-15 (VGA-style) HDI port intended for an OEM front panel. This project replaces that panel with a **CYD (Cheap Yellow Display) ESP32-2432S028** board running custom firmware.

The firmware communicates with the controller over a 3-wire TTL serial connection and presents a full touchscreen interface built on **LVGL 9.5**, **TFT_eSPI**, and **XPT2046**.

### Features

| Category | Controls |
|---|---|
| Motion | X+ / X− / Y+ / Y− / Z+ / Z− jog (hold to repeat), Home All |
| Laser | Test Fire (adjustable power slider), Frame, Start, Pause, Stop |
| Status | Live X/Y/Z coordinates, machine state, laser power, job progress bar |

---

## Hardware Requirements

| Component | Notes |
|---|---|
| **ESP32-2432S028** (CYD board) | Any revision with ILI9341 display + XPT2046 touch |
| **RDC6332G** laser controller | Firmware ≥ 15.01.xx recommended |
| Short cable (≤ 1 m) | 3 wires: TX, RX, GND |
| Multimeter | To verify HDI voltage levels before connecting |

---

## Wiring

The RDC6332G HDI port is a **DE-15 connector** (looks like a VGA port). Only 3 wires are needed.

```
DE-15 (Controller)          CYD P3 Header
──────────────────          ─────────────────────
Pin 4  HDI TX  ──────────►  GPIO22  (UART2 RX)
Pin 5  HDI RX  ◄──────────  GPIO27  (UART2 TX)
Pin 1  GND     ───────────  GND
```

The CYD is powered separately via its own USB connector. Do **not** connect the DE-15 +5V pin to the ESP32.

### CYD P3 Header (rear of board)

```
[ GND | GPIO27 TX | GPIO22 RX | 3.3V ]
```

> ⚠️ **Check voltage levels first.**
> Probe DE-15 pin 4 with a multimeter while the controller is powered on.
> - ~3.3 V idle → connect directly
> - ~5 V idle → add a voltage divider on the wire going into GPIO22
> - ±5–12 V → RS-232 levels, you need a **MAX3232** level converter module

### Known DE-15 Pin Assignments

| Pin | Signal | Direction |
|-----|--------|-----------|
| 1 | GND | — |
| 2 | GND | — |
| 3 | +5V | Panel power (do not use) |
| 4 | HDI TX | Controller → CYD |
| 5 | HDI RX | CYD → Controller |
| 6–8 | Encoder A/B/SW | Not used |
| 9–10 | KEY1/KEY2 | Not used |

> Pin assignments are community reverse-engineered and may vary by firmware revision.

---

## Software Setup

### 1 — Install Arduino IDE and ESP32 support

In **File → Preferences**, add this to *Additional Boards Manager URLs*:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
Then **Tools → Boards Manager** → search `esp32` → install **esp32 by Espressif Systems** (v2.x).

### 2 — Install Libraries

Open **Sketch → Include Library → Manage Libraries** and install:

| Library | Author | Version |
|---------|--------|---------|
| **TFT_eSPI** | Bodmer | latest |
| **XPT2046_Touchscreen** | Paul Stoffregen | latest |
| **lvgl** | LVGL | **9.x** (do not install v8) |

### 3 — Copy configuration files

Two library config files must be placed in the correct locations **before compiling**.

**TFT_eSPI pin config:**
```
Copy:  User_Setup.h
  To:  Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
```

**LVGL config:**
```
Copy:  lv_conf.h
  To:  Documents\Arduino\libraries\lvgl\src\lv_conf.h
```

### 4 — Open the sketch

Open the folder `laser_controller_arduino/` — Arduino IDE will load `laser_controller_arduino.ino` automatically.

### 5 — Board settings

**Tools** menu:

| Setting | Value |
|---------|-------|
| Board | **ESP32 Dev Module** |
| **Partition Scheme** | **Huge APP (3MB No OTA / 1MB SPIFFS)** |
| CPU Frequency | 240 MHz |
| Flash Mode | QIO |
| Flash Size | 4MB |
| Upload Speed | 921600 |

### 6 — Upload

Click **Upload** (Ctrl+U). Open **Serial Monitor** at 115200 baud to see boot messages.

---

## Touch Calibration

Default calibration values may not match your specific CYD unit. If touches register in the wrong position:

1. Open Serial Monitor and note the raw X/Y values printed when you touch each corner
2. Update these lines in `config.h`:

```cpp
#define TOUCH_X_MIN   300   // raw value at left edge
#define TOUCH_X_MAX  3900   // raw value at right edge
#define TOUCH_Y_MIN   200   // raw value at top edge
#define TOUCH_Y_MAX  3900   // raw value at bottom edge
```

---

## UI Layout

```
┌──────────────────────────────────────────────────────────┐
│  IDLE       X:000.0    Y:000.0    Z:000.0                │  ← status bar
├─────────────────────────────┬────────────────────────────┤
│  [ X- ]  [ ⌂ HOME ]  [ X+ ] │  [ FRAME  ]  [ ▶  GO  ]  │
│  [ Y- ]  [        ]  [ Y+ ] │  [ ‖ PAUSE]  [ ■  STOP]  │
│  [ Z- ]  [        ]  [ Z+ ] │  [ ⚠  TEST FIRE        ]  │
│                             │  ▐▌▌▌░░  30 %             │
├─────────────────────────────┴────────────────────────────┤
│  ████████████████░░░░░░  65 %                            │  ← progress
└──────────────────────────────────────────────────────────┘
```

**Jog buttons** send a new jog command every `JOG_REPEAT_MS` milliseconds while held. Release stops motion immediately.

**Test Fire** pulses the laser at the power set by the slider for `TEST_FIRE_DURATION_MS` milliseconds.

---

## Configuration Reference (`config.h`)

| Constant | Default | Description |
|----------|---------|-------------|
| `HDI_RX_PIN` | 22 | UART2 RX GPIO |
| `HDI_TX_PIN` | 27 | UART2 TX GPIO |
| `HDI_BAUD` | 115200 | HDI baud rate |
| `JOG_SPEED_FAST` | 50000 | Jog speed (controller units) |
| `JOG_REPEAT_MS` | 100 | Re-send interval while button held (ms) |
| `TEST_FIRE_DURATION_MS` | 200 | Laser test pulse width (ms) |
| `TEST_FIRE_DEFAULT_PWR` | 30 | Default test fire power % |
| `STATUS_POLL_MS` | 100 | UI status refresh rate (ms) |
| `TOUCH_X/Y_MIN/MAX` | varies | Touch calibration raw values |

---

## Protocol Notes

The HDI protocol is **not officially documented by Ruida**. Command bytes in `ruida.h` are derived from community reverse-engineering. If buttons produce no response or unexpected motion:

1. In `ruida.cpp` find the comment `// set to 1 for raw-hex debug` and change `#if 0` → `#if 1`
2. Open Serial Monitor at 115200 baud and press each button
3. Compare the logged hex against captures from your OEM panel using a logic analyser
4. Adjust the command constants in `ruida.h`

**Community references:**
- [meerk40t](https://github.com/meerk40t/meerk40t) — Python Ruida driver
- [LightBurn forum](https://forum.lightburnsoftware.com) — Ruida serial protocol threads

---

## Project Structure

```
laser_controller_arduino/
├── laser_controller_arduino.ino   Main sketch
├── lvgl_arduino.h                 LVGL include wrapper
├── lv_conf.h                      LVGL 9.5 configuration
├── User_Setup.h                   TFT_eSPI pin configuration
├── config.h                       All pins and tunable constants
├── ruida.h / ruida.cpp            HDI protocol parser and command builder
└── ui.h / ui.cpp                  LVGL touchscreen interface
```

---

## Why TFT_eSPI and not LovyanGFX?

LovyanGFX bundles its own internal copy of LVGL headers. When combined with a standalone LVGL 9.x installation this produces hundreds of type redefinition errors. TFT_eSPI has no bundled LVGL and compiles cleanly alongside LVGL 9.5.

---

## Contributing

Pull requests are welcome. If you capture verified HDI packet traces from your controller, please open an issue — improving the protocol layer benefits everyone.

---

## Disclaimer

This project is not affiliated with or endorsed by Ruida Technology. The HDI protocol implementation is based on community reverse-engineering. Use at your own risk. Always keep your hand on the physical e-stop when testing.

---

## License

MIT License — see [LICENSE](LICENSE) for details.

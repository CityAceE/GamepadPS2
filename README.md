# NES to PS/2 Sinclair Joystick Adapter

A minimal-hardware adapter that allows a standard **NES controller** to emulate Sinclair joysticks or emulator control keys over the **PS/2 keyboard protocol**.

Designed for use with:

* Arduino Nano / Uno (ATmega328P)
* ZX Spectrum projects that accept PS/2 keyboard input
* Especially compatible with [**ESPectrum**](https://github.com/EremusOne/ESPectrum) (TTGO VGA32 ZX Spectrum emulator)

---

## 📌 Project Goal

This project allows you to use a cheap and widely available **NES gamepad** as:

* QAOPM joystick
* Left Sinclair joystick
* Right Sinclair joystick
* Emulator control pad (Cursor keys + F1 + Enter)

The adapter emulates a real PS/2 keyboard, meaning:

* No firmware modifications are required on the emulator side
* No additional interface hardware is required
* It works anywhere a PS/2 keyboard works

---

## 🧠 How It Works

The NES controller is read using its native serial protocol:

* LATCH pulse
* 8-bit shift register read via CLOCK
* Data sampled on each bit

The Arduino then translates button presses into:

* Standard PS/2 scan codes
* Extended scan codes (0xE0 prefix)
* Proper 8-byte PAUSE/Break sequence

A small internal queue ensures proper PS/2 timing and prevents signal corruption.

The device behaves exactly like a real keyboard.

---

## ✨ Features

* 4 selectable joystick layouts
* Full PS/2 compliance
* Correct PAUSE/Break implementation
* Extended arrow key support
* Safe layout switching (auto key release)
* A/B swap with EEPROM persistence
* SELECT works as:

  * Normal key
  * Configuration modifier

No additional ICs required.

---

## 🔌 Hardware Requirements

* Arduino Nano or Uno (ATmega328P)
* Standard NES controller
* 9 wires
* Soldering iron

That’s it.

No resistors.
No level shifters.
No transistors.
No external components.

Just direct wiring.

---

## 📎 Wiring

### NES Controller → Arduino

| NES Pin | Arduino Pin |
| ------- | ----------- |
| LATCH   | D8          |
| CLOCK   | A5          |
| DATA    | A4          |
| VCC     | 5V          |
| GND     | GND         |

### PS/2 Output → Target Device

| PS/2 Pin | Arduino Pin |
| -------- | ----------- |
| DATA     | D2          |
| CLOCK    | D3          |
| VCC      | 5V          |
| GND      | GND         |

All signals are 5V logic level and compatible with ATmega328P.

---

## ⚠️ Important

Make sure:

* The target system expects a **PS/2 keyboard**, not USB.
* Ground is shared between Arduino and the target device.
* Wires are kept reasonably short to avoid noise.

---

## 🛠 Flashing the Firmware

1. Install Arduino IDE.
2. Install the PS2dev library:
   [https://github.com/harbaum/ps2dev](https://github.com/harbaum/ps2dev)
3. Select board:

   * Arduino Nano (ATmega328P, Old Bootloader if required)
   * Or Arduino Uno
4. Select correct COM port.
5. Upload the sketch.

That’s it.

No additional configuration required.

---

## 🎮 Layouts

The adapter supports 4 layouts:

### 1️⃣ QAOPM

Traditional ZX Spectrum mapping:

* Q, A, O, P, M

### 2️⃣ Left Sinclair

### 3️⃣ Right Sinclair

### 4️⃣ Emulator Control

* Arrow keys
* F1 (via SELECT)
* Enter
* Pause

---

## ⚙️ Configuration Mode

Hold **SELECT** to enter configuration mode.

While holding SELECT:

| Button | Action                   |
| ------ | ------------------------ |
| UP     | Cursor / Emulator layout |
| DOWN   | QAOPM                    |
| LEFT   | Left Sinclair            |
| RIGHT  | Right Sinclair           |
| A      | SwapAB OFF               |
| B      | SwapAB ON                |

Configuration is saved to EEPROM automatically.

---

## 🔁 A/B Swap

You can invert A and B buttons.

The state is stored in EEPROM and persists after power cycle.

---

## 🧪 Tested With

* Arduino Nano (clone)
* Arduino Uno
* TTGO VGA32 running **ESPectrum**
* Real PS/2 keyboard input devices

---

## 🎯 Why This Project?

* NES controllers are cheap and reliable
* PS/2 is extremely easy to emulate
* No USB complexity
* Perfect for retro projects
* Zero additional components required

This makes it ideal for DIY ZX Spectrum builds and retro FPGA/ESP-based systems.

---

## 📄 License

Open source.
Feel free to modify and improve.

# SmartVFO – SDR VFO Control Knob, Buttons & CW Keyer

SmartVFO is a USB MIDI controller for SDR software, providing a rotary VFO knob, programmable buttons, and an integrated iambic CW keyer with local sidetone.

Built for the RP2040 (Raspberry Pi Pico) using the Arduino/earlephilhower core — produces a **UF2** firmware file.

---

## 🎛 Features

- **Rotary Encoder as MIDI Knob** — sends MIDI NoteOn/Off for left (CC 31) and right (CC 30) rotation with acceleration
- **Iambic CW Keyer** — Mode B iambic keyer with local sidetone buzzer
- **WPM Adjustment** — press encoder button (MIDI 32) to enter WPM adjust mode:
  - LED blinks red
  - "CQ CQ CQ" plays on local sidetone so you can hear the speed
  - Turn encoder to change WPM (5–50)
  - Press encoder again to confirm and exit
- **LED Indicators:**
  - 🟢 Green = normal operation
  - 🔴 Red = transmitting / WPM adjust mode
  - 🔵 Blue = startup
- **3 Buttons** — encoder push (32), top (34), bottom (33) send MIDI notes
- **Serial CW** — type text over USB serial to transmit as CW
- **Watchdog** — 2-second hardware watchdog for reliability

---

## 🔌 GPIO Pin Map

| Function       | GPIO |
|---------------|------|
| RGB LED Red    | GP3  |
| RGB LED Green  | GP4  |
| RGB LED Blue   | GP5  |
| Encoder A      | GP17 |
| Encoder B      | GP16 |
| Encoder Button | GP19 |
| Button Top     | GP18 |
| Button Bottom  | GP15 |
| Dit Paddle     | GP13 |
| Dah Paddle     | GP12 |
| Buzzer         | GP24 |

---

## 🔧 Building

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)

### Build & Flash

```bash
# Build
pio run

# Upload (with Pico in BOOTSEL mode)
pio run --target upload

# Generate UF2
# The .uf2 file will be in .pio/build/pico/
```

### Manual Flash

1. Hold BOOTSEL on the Pico and connect USB
2. A drive named `RPI-RP2` appears
3. Copy the `.uf2` file to the drive
4. The Pico reboots with new firmware

---

## 🎹 MIDI Map

| Control          | MIDI Note | Type       |
|-----------------|-----------|------------|
| Encoder Left     | 31        | NoteOn/Off |
| Encoder Right    | 30        | NoteOn/Off |
| Encoder Button   | 32        | NoteOn/Off |
| Button Bottom    | 33        | NoteOn/Off |
| Button Top       | 34        | NoteOn/Off |
| Dit Paddle*      | 35        | NoteOn/Off |
| Dah Paddle*      | 36        | NoteOn/Off |
| CW Key Down**    | 65        | NoteOn/Off |

\* Non-CW mode only  
\** CW mode only

---

## ⚙️ Configuration

Edit `include/config.h` before building:

- `CW_MODE` — `true` for iambic keyer, `false` for simple paddle/PTT
- `DEFAULT_WPM` — starting CW speed (default 15)
- `SIDETONE_FREQ` — buzzer frequency in Hz (default 880)
- `MIN_WPM` / `MAX_WPM` — WPM adjustment range

---

## 📃 License

[MIT License](LICENSE)

---

## 🧃 Credits

Developed by [ON6URE – RF.Guru](https://rf.guru)

<div align="center">
# ⚙️ PROJECT W.I.S.T.
### *Wireless Interface for Security & Telemetry*
[![Build Status](https://img.shields.io/badge/build-in_progress-yellow?style=for-the-badge&logo=github-actions&logoColor=white)](https://github.com/RabbitFored/WIST)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue?style=for-the-badge&logo=espressif&logoColor=white)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/framework-Arduino%20%2B%20LVGL-red?style=for-the-badge&logo=arduino&logoColor=white)](https://platformio.org/)
[![License](https://img.shields.io/badge/license-MIT-green?style=for-the-badge)](LICENSE)
[![Phase](https://img.shields.io/badge/phase-firmware_foundation-orange?style=for-the-badge)](https://github.com/RabbitFored/WIST)
> *"Sometimes you gotta run before you can walk."* — Tony Stark
</div>
---
## 🔭 The Vision
**W.I.S.T.** is not a Kickstarter. It's not a product. It's an **engineering obsession** — a fully custom smartwatch built from raw hardware and bare-metal firmware, engineered from the ground up with zero compromises.
The goal: a wrist-worn device with the aesthetic of Stark Industries hardware and the soul of a hacker's tool. Round display, rugged tactical enclosure, wireless everything — and an AI companion that shifts personality depending on whether you need a daily driver or a penetration tester.
This is hardware hacking at its purest. Every component chosen. Every register mapped. Every pixel owned.
---
## 🛠️ Hardware Requirements & Build Log
### Core Components
| Component | Part | Notes |
|---|---|---|
| **MCU + Display** | Waveshare ESP32-S3 1.46" Round Display Dev Board | Integrated 466×466 QSPI round display, 16 MB Flash, 8 MB PSRAM |
| **Enclosure** | 52mm+ "1251" style donor watch case | Rugged, large-format tactical housing — repurposed from an oversized aftermarket shell |
| **Connectivity** | ESP32-S3 onboard Wi-Fi 802.11 b/g/n + Bluetooth 5.0 | Dual-core Xtensa LX7 at 240 MHz |
### Flash Partition Map
The full 16 MB of flash is unlocked and divided for maximum capability:
| Partition | Type | Size | Purpose |
|---|---|---|---|
| `nvs` | data/nvs | 24 KB | Non-volatile storage (config, secrets) |
| `factory` | app | 3 MB | Main firmware image |
| `flash_test` | data/fat | 528 KB | Diagnostic scratch space |
| `model` | data/spiffs | ~5.9 MB | AI model weights & assets |
### Signal Architecture
The firmware organizes hardware signals into logical subsystems under `signals/commons/`:
```
signals/
└── commons/
    ├── audio/        # Speaker / mic signal routing
    ├── battery/      # ADC fuel gauge integration
    ├── bluetooth/    # BLE peripheral stack
    ├── display/      # QSPI display driver interface
    ├── touch/        # Capacitive touch controller
    └── wifi/         # Wi-Fi station / AP management
```
---
## 💻 Software Architecture
W.I.S.T. firmware is built on **PlatformIO + Arduino framework**, targeting the ESP32-S3, with the **LVGL v8.3** graphics library driving every pixel.
### Build Configuration (`platformio.ini`)
| Setting | Value | Purpose |
|---|---|---|
| `platform` | pioarduino ESP32 (stable) | Community-maintained ESP32 Arduino core (IDF 5.x) |
| `board` | `esp32-s3-devkitc-1` | Base board definition |
| `framework` | Arduino | Familiar, fast iteration |
| `board_build.arduino.memory_type` | `qio_opi` | OPI PSRAM mode for maximum bandwidth |
| `board_build.flash_mode` | `qio` | Quad-SPI flash for speed |
| `upload_speed` | `921600` | Fast flashing |
| `monitor_speed` | `115200` | Serial debug |
| `monitor_filters` | `esp32_exception_decoder` | Human-readable crash logs |
### LVGL Configuration Flags
```ini
-D LV_CONF_SKIP=1          ; Headerless config — all flags set here
-D LV_COLOR_DEPTH=16       ; 16-bit color (RGB565) for the QSPI display
-D LV_COLOR_16_SWAP=0      ; Color byte order
-D LV_TICK_CUSTOM=1        ; Arduino millis() drives LVGL timing
-D LV_TICK_CUSTOM_SYS_TIME_EXPR=millis()
```
### Key Library Dependencies
| Library | Version | Role |
|---|---|---|
| `moononournation/GFX Library for Arduino` | `^1.5.0` | Low-level display driver for QSPI panel |
| `lvgl/lvgl` | `~8.3.11` | Hardware-accelerated UI framework |
---
## 🚀 Installation & Flashing Guide
### Prerequisites
- [PlatformIO IDE](https://platformio.org/install) (VS Code extension recommended)
- USB-C cable with data support
- ESP32-S3 board in **USB CDC boot mode**
### Steps
**1. Clone the repository**
```bash
git clone https://github.com/RabbitFored/WIST.git
cd WIST
```
**2. Open in PlatformIO**
Open the cloned folder in VS Code. PlatformIO will auto-detect `platformio.ini` and resolve all library dependencies automatically.
**3. Build the firmware**
```bash
pio run -e WIST
```
**4. Put the board into download mode**
Hold the `BOOT` button, press `RESET`, then release `BOOT`. The board will enumerate as a USB serial device.
**5. Flash**
```bash
pio run -e WIST --target upload
```
**6. Monitor serial output**
```bash
pio device monitor -e WIST
```
> **Tip:** The `esp32_exception_decoder` monitor filter will automatically translate any panic backtraces into human-readable source file line numbers.
---
## 🗺️ Future Roadmap
### Phase 1 — Firmware Foundation *(Active)*
- [x] PlatformIO environment configured
- [x] 16 MB flash partition map unlocked
- [x] LVGL v8.3 integrated with custom tick source
- [x] Signal subsystem scaffold (`audio`, `battery`, `bluetooth`, `display`, `touch`, `wifi`)
- [ ] QSPI display driver fully initialized
- [ ] Capacitive touch calibrated
- [ ] Battery ADC fuel gauge operational
### Phase 2 — UI/UX Framework
- [ ] Base watchface rendered on round LVGL canvas
- [ ] Gesture-based navigation system
- [ ] Always-on display mode
- [ ] OTA (Over-The-Air) firmware update pipeline
### Phase 3 — Dual-Personality AI Companion 🤖
This is where W.I.S.T. becomes something different from every other smartwatch on the market.
The onboard AI companion — loaded from the `model` partition — operates in two distinct **personality modes**, switchable from the wrist:
---
#### 🐱 Cat Mode — *Daily Driver*
> *Casual. Charming. Surprisingly capable.*
Cat Mode is the everyday persona of W.I.S.T. Optimized for ambient awareness, personal productivity, and a friendly UX. Think less terminal, more co-pilot.
**Planned capabilities:**
- Natural language reminders & calendar nudges
- Contextual notifications with smart filtering
- Fitness & biometric tracking with AI-driven insights
- Conversational interface for quick queries
- Personalized watch-face theming
---
#### 🦊 Fox Mode — *Hacker Interface*
> *Tactical. Silent. Ruthlessly efficient.*
Fox Mode is the advanced, operator-centric interface. Raw telemetry. Deep system access. Built for the field.
**Planned capabilities:**
- Real-time Wi-Fi & Bluetooth environment scanning
- Network telemetry dashboard (RSSI, channel, device fingerprinting)
- BLE advertisement sniffer & logger
- Serial terminal access over BLE SPP
- System diagnostics: heap, PSRAM, CPU load, flash health
- Scriptable macros triggered from hardware inputs
---
### Phase 4 — Hardware Expansion
- [ ] External sensor bus (I²C/SPI) breakout — heart rate, environmental sensors
- [ ] Vibration motor haptic feedback
- [ ] Custom PCB revision to replace dev board
---
## 🤝 Contributing & License
W.I.S.T. is an open build log as much as it is a project. Contributions, ideas, hardware suggestions, and firmware PRs are all welcome.
### Contributing
1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Commit your changes: `git commit -m 'Add some feature'`
4. Push to the branch: `git push origin feature/your-feature`
5. Open a Pull Request
Please keep PRs focused and well-described. This is a hardware project — document your changes with context about why the hardware requires it.
### License
This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.
---
<div align="center">
*Built with stubbornness, solder fumes, and an unhealthy obsession with round displays.*
**⚙️ W.I.S.T. — Engineered on the wrist.**
</div>

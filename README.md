# Project W.I.S.T. 
**W**ireless **I**nterface for **S**ecurity & **T**elemetry

![Build Status](https://img.shields.io/badge/build-in%20progress-orange)
![Hardware](https://img.shields.io/badge/hardware-ESP32--S3-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## 🚀 The Vision
Project W.I.S.T. is a custom-engineered, from-scratch smartwatch project. Moving away from locked-down commercial ecosystems, this project aims to build a highly capable, modular wrist-worn terminal. 

Inspired by high-tech, functional engineering aesthetics, W.I.S.T. blends everyday utility with advanced telemetry and hardware hacking capabilities. The current development phase is focused strictly on establishing a rock-solid hardware foundation and core firmware architecture.

---

## 🛠️ Hardware Stack

The hardware is currently being prototyped and fitted into a repurposed, ruggedized chassis to ensure durability and a tactical form factor.

| Component | Specification | Notes |
| :--- | :--- | :--- |
| **Microcontroller / Display** | Waveshare ESP32-S3 | 1.46" round IPS display, integrated touch |
| **Enclosure** | Repurposed "1251" style donor case | 52mm+ diameter, chosen for a rugged, tactical housing |
| **Power** | *TBD* | Currently evaluating high-density LiPo options |

### Hardware Build Notes & Quirks
* **Touchscreen FPC Ribbon:** During prototyping, we've noted that the touch interface on the 1.46" Waveshare board may require slight, sustained physical pressure on the side of the FPC ribbon to register touch inputs consistently upon booting. Chassis design will need to account for this to maintain steady pressure once sealed.

---

## 💻 Software Architecture (Current Phase)

Development is currently focused on the foundational layer:
* **Framework:** ESP-IDF / Arduino core for ESP32-S3.
* **UI/UX:** Establishing the base display drivers and a lightweight graphical interface optimized for the 1.46" round screen.
* **Core Functions:** Power management, sleep states, and basic I/O testing.

---

## 🗺️ Roadmap & Future Vision

While the current focus is entirely on hardware and base firmware, Project W.I.S.T. is being built to support a complex software ecosystem in the future. 

**Planned Feature: Dual-Personality AI Companion**
The ultimate goal for the W.I.S.T. operating system is an integrated, context-aware AI interface split into two distinct modes:
* **"Cat Mode":** The default state. Designed for casual, everyday smartwatch usage, standard notifications, and battery optimization.
* **"Fox Mode":** The advanced state. A hacker-centric, elevated-privilege interface designed for telemetry monitoring, security tools, and deep system access.

---

## ⚙️ Getting Started / Installation

*(Detailed flashing instructions and environment setup for the Waveshare ESP32-S3 will be added here once the base repository is stable.)*

1. Clone the repository.
2. Set up the ESP32-S3 build environment.
3. Flash the base firmware.

---

## 🤝 Contributing
Contributions, suggestions, and hardware mods are welcome. As the project stabilizes, formal contribution guidelines will be established.

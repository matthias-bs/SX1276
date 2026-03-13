# Copilot Instructions for SX1276_Radio_Lite

Purpose
- Low-level SX1276 radio driver and supporting examples used by related projects in this workspace. This file gives Copilot quick orientation for safe, consistent suggestions when editing this repository.

Quick overview
- Core files: `SX1276.cpp`, `SX1276.h`, `README.md`, `library.properties`, `keywords.txt`, `LICENSE`.
- Documentation: `docs/` contains the SX1276 data sheet and errata note, design notes, root-cause analyses, and proposed fixes — read these before changing protocol or timing logic.
- Examples: `examples/` contains minimal usage and board-specific wiring notes.

Build & test
- This is an Arduino-style library. Build or compile examples using Arduino IDE, PlatformIO, or `arduino-cli`.
- **Board configuration is in `.vscode/arduino.json`** — always read this file first for the authoritative FQBN, sketch path, port, and baud rate. Current settings:
  - `fqbn`: `esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new`
  - `sketch`: `examples/BresserRxExample/BresserRxExample.ino`
  - `baudRate`: `115200`
- Common build/upload/monitor tasks are defined in `.vscode/tasks.json`; use those as the reference for CLI arguments.

Conventions & constraints
- Preserve public API in `SX1276.h` unless the change is necessary and clearly documented.
- Keep changes minimal and non-breaking: prefer adding helpers or flags to enable behavior rather than removing features.
- Follow existing style and header conventions (MIT license header, Doxygen comments where present).

When you edit
- Read `docs/` files that discuss deviations, fixes, or FIFO behavior before changing radio state machines.
- Run or suggest local compile steps; do not assume CI runs for new branches.
- For firmware changes that affect timing or radio state, include suggested test steps and hardware required.

Example prompts
- "Update `SX1276.cpp` to add a non-blocking FIFO-empty retry with configurable timeout; preserve current API and add unit test if possible."
- "Document the `begin()` call sequence in `README.md` showing required pin wiring for TTGO LoRa32 v2.1."
- "Refactor debug output to use `log_d()/log_i()` macros where available and keep verbose logs behind `#ifdef DEBUG_RADIO`."

Suggested next customizations
- Add `AGENTS.md` if you want specialized agents (e.g., a hardware-testing agent that suggests test procedures or serial monitor commands).

If anything is unclear, tell me which file or behavior you want changed and which board/variant to target.

# BresserWeatherSensorTransmitter Example - Session Summary

**Date:** 13 March 2026  
**Model:** GitHub Copilot (Claude Sonnet 4.6)  
**Project:** [SX1276_Radio_Lite](https://github.com/matthias-bs/SX1276_Radio_Lite)

---

## 1. Add BresserWeatherSensorTransmitter example

**User:**
> Add an example "BresserWeatherSensorTransmitter" which uses the Bresser radio and message
> parameters which uses the 6-in-1 encoding scheme and mimics a temperature/hygrometer sensor.

**Copilot:**

Created three new files:

- `examples/BresserWeatherSensorTransmitter/BresserWeatherSensorTransmitter.ino`
- `examples/BresserWeatherSensorTransmitter/Bresser6in1Encoder.h`
- `examples/BresserWeatherSensorTransmitter/Bresser6in1Encoder.cpp`

The sketch emulates a Bresser 6-in-1 thermo-hygro sensor (type `0x02`,
`SENSOR_TYPE_THERMO_HYGRO`). It encodes temperature and humidity into the Bresser 6-in-1
over-the-air format and transmits on 868.3 MHz using identical radio parameters to a physical
Bresser sensor:

| Parameter           | Value                    |
|---------------------|--------------------------|
| Frequency           | 868.3 MHz                |
| Bit rate            | 8.21 kbps                |
| Frequency deviation | 57.136 kHz               |
| Preamble            | 32 bits (4 × 0xAA)       |
| Sync word           | `0xAA 0x2D`              |
| First payload byte  | `0xD4` (3rd Bresser sync) |
| Packet length       | 27 bytes, CRC disabled   |

The encoder (`Bresser6in1Encoder`) fills an 18-byte block (indices 0–17) with:

| Bytes | Field |
|-------|-------|
| 0–1   | LFSR-16 digest (gen `0x8810`, key `0x5412`) over bytes 2–16 |
| 2–5   | Sensor ID (32-bit big-endian) |
| 6     | `[7:4]` type \| `[3]` startup \| `[2:0]` channel |
| 7–9   | Wind speed (set `0x00` → invalid) |
| 10–11 | Wind direction (`0x00`, unused) |
| 12    | Temperature hundreds + tens digit (BCD) |
| 13    | `[7:4]` ones digit \| `[3]` sign \| `[1]` battery OK |
| 14    | Humidity (BCD) |
| 15    | UV (`0xFF` → invalid) |
| 16    | `[3:0]` flags = `0x0` (temp/humidity message) |
| 17    | Add-with-carry checksum so that `sum(bytes 2–17) & 0xFF == 0xFF` |

The 27-byte RF payload is structured as:

```
buffer[0]      = 0xD4  (Bresser third sync byte)
buffer[1..18]  = 18-byte 6-in-1 encoded data
buffer[19..26] = 0x00  (padding, ignored by decoder)
```

---

## 2. Compile using the predefined workflow

**User:**
> Use the predefined workflow for compiling and upload!

**Copilot:**

To use the predefined VS Code workflow, create or update your local `.vscode/arduino.json`
to point to the transmitter sketch, for example:

```json
{
  "sketch": "examples/BresserWeatherSensorTransmitter",
  "fqbn": "esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new"
}
```

Then ran the **"Compile Arduino Sketch"** VS Code task
(`bash .vscode/arduino-compile-runner.sh compile`). Compilation succeeded.

---

## 3. Upload to the board

**User:**
> Compiling succeeded, now upload!

**Copilot:**
> Which port is the board connected to?

**User:**
> ttyACM0

**Copilot:**

Ran:

```bash
bash .vscode/arduino-cli-wrapper.sh upload \
  --fqbn esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new \
  --port /dev/ttyACM0 \
  examples/BresserWeatherSensorTransmitter
```

Output (abbreviated):

```
esptool v5.1.0
Connected to ESP32 on /dev/ttyACM0:
Chip type: ESP32-PICO-D4 (revision v1.0)
...
Wrote 293968 bytes (162435 compressed) at 0x00010000 in 3.5 seconds
Hash of data verified.
Hard resetting via RTS pin...
```

Upload successful.

## 4. Improve the warning comment

**User:**
> Wouldn't it make sense to leverage this warning a little?

**Copilot:**

Replaced the generic warning block in `BresserWeatherSensorTransmitter.ino` with one that
includes concrete numbers:

```cpp
// !!! WARNING !!!
//   Transmitting on 868 MHz (or any ISM band) is subject to local radio
//   regulations (e.g. ETSI EN 300 220 in Europe, FCC Part 15 in the US).
//   Duty-cycle limits and output-power limits apply.
//
//   This sketch transmits at 10 dBm (PA_BOOST pin, SX1276 default) via the
//   TTGO LoRa32 on-board antenna connector.  Each 27-byte packet takes roughly
//   32 ms on air at 8.21 kbps; at the default TX_INTERVAL_MS of 60 s this
//   gives a duty cycle of ≈ 0.05 %, well within the ETSI 1 % hourly limit.
//   However, even this low power level can interfere with nearby real Bresser
//   base stations if a real antenna is attached.
//
//   For close-range bench testing, reduce the power to the minimum (+2 dBm)
//   by changing the 5th argument of beginFSK() and use a 50 Ω dummy load
//   or a shielded enclosure instead of an antenna.
```

---

## Summary of all changes

| File | Change |
|------|--------|
| `examples/BresserWeatherSensorTransmitter/BresserWeatherSensorTransmitter.ino` | **New** — main sketch, Bresser 6-in-1 thermo-hygro emulator |
| `examples/BresserWeatherSensorTransmitter/Bresser6in1Encoder.h` | **New** — encoder API + packet layout documentation |
| `examples/BresserWeatherSensorTransmitter/Bresser6in1Encoder.cpp` | **New** — encoder implementation |

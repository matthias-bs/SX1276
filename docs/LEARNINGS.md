# LEARNINGS.md

Lessons learned during development of `SX1276_Radio_Lite`, collected for future agents.
Each entry has a short title, the symptom that revealed the problem, the root cause, and
the fix applied.

---

## L-001 — Integer width and sign extension in Bresser sensor-ID decoding

**Date:** 2026-03-13 / 2026-03-14  
**Files:** `examples/BresserWeatherSensorBasic/BresserDecoders.cpp`,
           `examples/BresserWeatherSensorBasic/BresserWeatherSensorBasic.ino`

### Symptom

Two separate observations triggered this investigation:

1. **6-in-1 transmitter test**: the receiver printed sensor ID `FFFFBEEF` instead of
   the expected `DEADBEEF` (`SENSOR_ID = 0xDEADBEEFUL`).  The lower 16 bits `BEEF`
   were correct; the upper 16 bits `DEAD` were replaced by `FFFF`.

2. **Real lightning sensor packet**: the receiver printed `FFFFEEFB` with an incorrect
   sensor type, instead of a 16-bit ID `EEFB` with type Lightning.  This was caused by
   the 6-in-1 decoder incorrectly claiming the packet (false positive on the integrity
   checks) before the lightning decoder could run — a separate but related issue
   documented in the *Secondary effect* section below.

### Root cause — three contributing bugs

#### 1. Missing `uint32_t` casts in the 6-in-1 ID reconstruction

```cpp
// WRONG – see below for platform-specific consequences
uint32_t id_tmp = (msg[2] << 24) | (msg[3] << 16)
                 | (msg[4] << 8) | msg[5];

// CORRECT – all shift operands must be cast to uint32_t before shifting
uint32_t id_tmp = ((uint32_t)msg[2] << 24) | ((uint32_t)msg[3] << 16)
                 | ((uint32_t)msg[4] << 8) | (uint32_t)msg[5];
```

All four bytes must be cast to `uint32_t` **before** shifting.  Omitting the casts
causes different problems depending on the platform `int` width:

**32-bit `int` (ESP32, ARM Cortex-M):**
- `msg[2] << 24`: if `msg[2] >= 0x80` (e.g. `0xDE`), the shift produces a value
  that overflows signed `int` — this is **undefined behaviour** in C/C++.  GCC on ARM
  typically yields the expected bit pattern in practice, but the compiler is free to
  optimise the expression in unexpected ways.
- `msg[3] << 16`, `msg[4] << 8`, `msg[5]`: with 32-bit `int` the intermediate results
  are positive even when the byte value ≥ 0x80 (e.g. `0xBE << 8 = 48640`), so there
  is **no sign-extension** issue for these three bytes on a 32-bit platform.

**16-bit `int` (AVR, MSP430):**
- `msg[2] << 24` and `msg[3] << 16`: shifting ≥ 16 positions on a 16-bit `int` is
  **undefined behaviour**.
- `msg[4] << 8`: when `msg[4] >= 0x80` the result is a negative `int16_t`
  (e.g. `(int16_t)0xBE << 8 = −16640`); OR-ing with `msg[5]` gives e.g.
  `int16_t(0xBEEF) = −16657`; widening to `uint32_t` sign-extends to `0xFFFFBEEF`.

The `(uint32_t)` casts eliminate both the UB and the sign-extension on all platforms.

#### 2. The same pattern in the 7-in-1 and lightning decoders

Both decoders use a 16-bit sensor ID:

```cpp
// WRONG – identical sign-extension risk
int id_tmp = (msgw[2] << 8) | msgw[3];

// CORRECT – explicit 16-bit type, unsigned intermediate arithmetic
uint16_t id_tmp = (uint16_t)(((unsigned)msgw[2] << 8) | msgw[3]);
```

The `int` type disguises the fact that the sensor ID is only 16 bits wide (as confirmed
in the reference implementation `BresserWeatherSensorReceiver/src/WeatherSensorDecoders.cpp`,
lines 746 and 920).  Using `uint16_t` makes the width explicit.  The `(unsigned)` cast on
`msgw[2]` prevents a potential sign extension during the shift on platforms where `int` is
16 bits.

The same pattern applies to `unknown1` and `unknown2` in the lightning decoder:

```cpp
// WRONG
uint16_t unknown1 = ((msgw[5] & 0x0F) << 8) | msgw[6];
uint16_t unknown2 = (msgw[8] << 8) | msgw[9];

// CORRECT
uint16_t unknown1 = (uint16_t)(((unsigned)(msgw[5] & 0x0F) << 8) | msgw[6]);
uint16_t unknown2 = (uint16_t)(((unsigned)msgw[8] << 8) | msgw[9]);
```

#### 3. `Serial.print(uint32_t, HEX)` resolving through a signed overload

On some toolchains `Serial.print(value, HEX)` where `value` is `uint32_t` may be
dispatched via a signed `long` overload, causing sign extension in the print path:

```cpp
// Can sign-extend on some toolchains
Serial.print(sensor.sensor_id, HEX);

// Safe on all toolchains – forces the unsigned long overload
Serial.print((unsigned long)sensor.sensor_id, HEX);
```

---

## L-002 — Bresser sensor ID widths differ per protocol

**Date:** 2026-03-14  
**Files:** `examples/BresserWeatherSensorBasic/BresserDecoders.cpp`

### Rule

| Protocol / decoder | ID width | Source bytes (de-whitened) |
|--------------------|----------|---------------------------|
| 5-in-1             | 8-bit    | `msg[14]` (raw, not de-whitened) |
| 6-in-1             | 32-bit   | `msg[2..5]` (raw, big-endian) |
| 7-in-1             | 16-bit   | `msgw[2..3]` (de-whitened, big-endian) |
| Lightning          | 16-bit   | `msgw[2..3]` (de-whitened, big-endian) |
| Leakage            | 32-bit   | `msg[2..5]` (raw, big-endian) |

All IDs are stored in `SensorData::sensor_id` as `uint32_t`.  The upper bytes are zero
for 8-bit and 16-bit IDs.

### Implication for `Serial.print`

Always cast to `unsigned long` before printing with `HEX`:

```cpp
Serial.print((unsigned long)sensor.sensor_id, HEX);
```

---

## L-003 — Compile and upload workflow for SX1276_Radio_Lite

**Date:** 2026-03-13

The active sketch for compilation and upload is controlled by `.vscode/arduino.json`.
The `"sketch"` field must point to the directory containing the target `.ino` file
(relative to the workspace root), **not** to the `.ino` file itself.

Compile task: **"Compile Arduino Sketch"** → calls
`.vscode/arduino-compile-runner.sh compile` (reads `fqbn` from `.vscode/arduino.json`).

Upload task: **"Upload Arduino Sketch"** → calls `.vscode/arduino-cli-wrapper.sh upload`
with `--fqbn` and `--port` from task inputs.

Current target board: `esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new`

---

## L-004 — TX power and duty-cycle for BresserWeatherSensorTransmitter

**Date:** 2026-03-13  
**File:** `examples/BresserWeatherSensorTransmitter/BresserWeatherSensorTransmitter.ino`

The SX1276 driver (`SX1276.cpp`) defaults to `_useBoost = true` (PA_BOOST pin).
The `beginFSK()` power argument maps to `RegPaConfig = SX1276_PA_BOOST | (power - 2)`.

| Power arg | PA path   | Actual output |
|-----------|-----------|---------------|
| 2         | PA_BOOST  | +2 dBm (minimum) |
| 10        | PA_BOOST  | +10 dBm (default used by this sketch) |
| 17        | PA_BOOST  | +17 dBm (nominal maximum) |
| 18–20     | PA_BOOST + high-power mode | +18…+20 dBm |

A 27-byte packet at 8.21 kbps (including preamble + sync) takes **≈ 32 ms** on air.
At the default `TX_INTERVAL_MS` of 60 000 ms the duty cycle is **≈ 0.05 %**, well
within the ETSI EN 300 220 limit of 1 % per hour for the 868 MHz SRD band.

For close-range bench testing, reduce the power to **+2 dBm** (change the 5th argument
of `beginFSK()`) and use a 50 Ω dummy load instead of an antenna.

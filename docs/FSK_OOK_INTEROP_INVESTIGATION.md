# FSK / OOK Interoperability Investigation — Session Summary

**Date:** 16 March 2026  
**Model:** GitHub Copilot (Claude Sonnet 4.6)  
**Project:** [SX1276_Radio_Lite](https://github.com/matthias-bs/SX1276_Radio_Lite)

---

## Background

The interoperability test sketches (`SX127x_FSK_Modem`, `SX127x_OOK_Modem` running RadioLib
on one Lilygo T3 LoRa32, and `FSKExample` / `OOKExample` running SX1276_Radio_Lite on another
board) reported that **both nodes always get a timeout** — no packets were exchanged in either
direction.  Bresser-mode examples (fixed-length, CRC disabled) worked correctly; only
variable-length + CRC-on mode failed.

---

## Investigation — Exhaustive register comparison

All FSK/OOK configuration registers written by both libraries were compared byte-by-byte
against the SX1276 datasheet.

| Register | SX1276_Radio_Lite | RadioLib | Match? |
|---|---|---|---|
| `PKT_CONFIG_1` (0x30) | `0x90` | `0x90` | ✅ |
| `PKT_CONFIG_2` (0x31) | `0x40` | `0x40` | ✅ |
| `SYNC_CONFIG` (0x27) | `0x91` (2-byte sync) | `0x91` | ✅ |
| `PREAMBLE_DETECT` (0x1F) | `0xAA` | `0xAA` | ✅ |
| `FIFO_THRESH` (0x35) bit 7 | `1` = FIFO\_NOT\_EMPTY | `1` = FIFO\_NOT\_EMPTY | ✅ |
| `RX_CONFIG` (0x0D) | `0x09` | `0x09` | ✅ |
| Preamble conversion | bits ÷ 8 → bytes | bits ÷ 8 → bytes | ✅ |
| Sync word bytes | `{0x2D, 0xD4}` | `{0x2D, 0xD4}` | ✅ |
| CRC type (bit 0) | `0` = CCITT | `0` = CCITT | ✅ |
| AFC auto | disabled | disabled | ✅ |

FIFO write strategy (TX) and FIFO read strategy (RX) were also verified equivalent:
both write length byte first then payload; both read length byte from FIFO then payload.

---

## Root Cause #1 — FSKExample: Hardcoded Feather 32u4 pins

### Symptom

`FSKExample.ino` had the radio pin numbers hard-coded for the Adafruit Feather 32u4 RFM95:

```cpp
// Before — wrong for any non-Feather board
#define RADIO_CS    8
#define RADIO_RST   4
#define RADIO_DIO0  7
```

If compiled for a Lilygo T3 ESP32 board (the natural second test node), pins 8 / 4 / 7
do not connect to the SX1276's CS / RST / DIO0 lines.  The radio could not be initialised,
causing both sides to time out immediately.

### Fix

Added the same board-detection guard already present in `OOKExample.ino`:

```cpp
// After — board-aware
#if defined(ARDUINO_AVR_FEATHER32U4)
#define RADIO_CS    8
#define RADIO_RST   4
#define RADIO_DIO0  7
#else
// ESP32-based boards (Lilygo T3 LoRa32, TTGO LoRa32, etc.)
#define RADIO_CS    LORA_CS
#define RADIO_RST   LORA_RST
#define RADIO_DIO0  LORA_IRQ
#endif
```

**File changed:** `examples/FSKExample/FSKExample.ino`

---

## Root Cause #2 — OOKExample: Missing OOK peak-threshold decay setting

### Symptom

OOK reception failed even when the radio was correctly initialised.  The 80-bit preamble
was transmitted but the receiver never locked onto the sync word.

### Root cause

The SX1276 OOK mode uses a "peak" envelope detector.  During "0" bit periods in the
preamble the threshold level decays toward zero.  The decay rate is controlled by
`REG_OOK_AVG` (0x16) bits [7:5] (`OokPeakThreshDec`):

| Field value | Decay rate | Named constant |
|---|---|---|
| `000` | 1 per chip | `DEC_1_1_CHIP` — **chip default** |
| `011` | 1 per 8 chips | `DEC_1_8_CHIP` |

At the chip default (fast decay) the decision boundary oscillates wildly during "0" periods
of the 80-bit preamble, preventing the correlator from locking before the sync word arrives.

`SX127x_OOK_Modem` (the RadioLib reference sketch) calls:

```cpp
radio.setOokPeakThresholdDecrement(RADIOLIB_SX127X_OOK_PEAK_THRESH_DEC_1_8_CHIP);
```

which writes `0x60` into bits [7:5] of `REG_OOK_AVG`.  `OOKExample.ino` never made
this call, leaving the register at its chip reset value.

### Fix

Added a read-modify-write of `REG_OOK_AVG` immediately after `setPacketConfig()`:

```cpp
// Slow OOK peak threshold decay to once per 8 chips (matches SX127x_OOK_Modem).
// REG_OOK_AVG (0x16) bits[7:5]: OokPeakThreshDec
//   000 = 1/1 chip  (chip default — too fast for stable detection during preamble)
//   011 = 1/8 chip  (DEC_1_8_CHIP — matches RadioLib's SX127x_OOK_Modem)
{
    uint8_t reg = radio.readRegister(SX1276_REG_OOK_AVG);
    radio.writeRegister(SX1276_REG_OOK_AVG, (reg & 0x1F) | 0x60);  // DEC_1_8_CHIP
}
```

**File changed:** `examples/OOKExample/OOKExample.ino`

---

## Root Cause #3 — SX127x_OOK_Modem: `getRSSI()` stops receiver during RX window

### Symptom

OOK_Modem (RadioLib) never received OOKExample packets despite a 30 s receive window
that overlapped multiple OOKExample transmissions.  RSSI stayed at noise floor
(-119 to -126 dBm) for the entire window.  Meanwhile OOKExample could receive
OOK_Modem's packets (proving frequency, modulation, and antennas were correct).

### Root cause

`receiveWithDiagnostics()` calls `radio.getRSSI()` every 2 s to monitor the signal level.
In FSK/OOK mode, RadioLib's `SX1278::getRSSI()` calls:

```
SX127x::getRSSI(packet=true, skipReceive=false, offset)
```

When `skipReceive` is `false` (the default), the FSK/OOK branch of `SX127x::getRSSI()`:
1. Calls `startReceive()` (re-enters RX mode)
2. Reads `REG_RSSI_VALUE_FSK` (0x11)
3. Calls **`standby()`** — **stops the receiver**

After each RSSI sample the radio was left in **standby** for the next ~2 s.  The receiver
was only active for microseconds per RSSI read, making packet reception impossible.

### Fix

Changed `radio.getRSSI()` to `radio.getRSSI(false, true)` in both RSSI read sites inside
`receiveWithDiagnostics()`:

```cpp
// skipReceive=true: read RSSI register without calling startReceive()/
// standby().  The default (skipReceive=false) would stop the receiver
// after every RSSI sample, leaving the radio in standby for ~2 s and
// preventing any packet from being received.
float rssi = radio.getRSSI(false, true);
```

With `skipReceive=true` the register read is performed in-place without mode changes,
so the ongoing reception is not interrupted.

**File changed:** `extras/interop_tests/SX127x_OOK_Modem/SX127x_OOK_Modem.ino`

---

## Root Cause #4 — SX127x_OOK_Modem: TX/RX phase-lock prevents OOKExample reception

### Symptom

After fixing Root Cause #3, OOK_Modem reliably received every OOKExample packet.
However, OOKExample **never** received OOK_Modem's replies — every 10 s receive window
timed out.

### Root cause

Half-duplex TX/RX **phase-locking**.  The two nodes' TX/RX cycles synchronised in a
pathological pattern:

1. OOKExample transmits at time **T**.
2. OOK_Modem receives the packet and immediately responds (~5 ms after **T**).
3. But OOKExample is still in its **post-TX delay** (2–4 s) — **not yet listening**.
4. By the time OOKExample enters its 10 s RX window (**T + 2–4 s**), OOK_Modem has
   already finished TX and entered its own RX window.
5. Both nodes sit in RX waiting for each other until OOKExample times out and
   transmits again — repeating the same locked pattern.

The OOKExample `randomSeed(micros())` + `random(0, 2000)` jitter on the post-TX delay
does not help because the jitter only varies the *start* of the RX window, while
OOK_Modem always responds within a fixed ~5 ms.

### Fix

Added a random delay (4–7 s) at the end of OOK_Modem's `loop()`, between the RX phase
and the next TX.  This ensures the response arrives **during** OOKExample's 10 s RX
window (which starts 2–4 s after OOKExample's TX):

```cpp
// Random delay before next TX to prevent phase-locking with OOKExample.
// OOKExample enters its RX window 2-4 s after transmitting.  Without this
// delay, OOK_Modem responds within ~5 ms of receiving — long before
// OOKExample has switched from TX to RX — so every response is missed.
// A 4-7 s pause ensures the next TX falls inside OOKExample's 10 s RX
// window regardless of its random post-TX delay (2-4 s).
delay(4000 + random(0, 3000));
```

Also added `randomSeed(micros())` in `setup()` (matching OOKExample) so two
identically-flashed boards do not share the same PRNG sequence.

**File changed:** `extras/interop_tests/SX127x_OOK_Modem/SX127x_OOK_Modem.ino`

---

## Files Changed

| File | Change |
|---|---|
| `examples/FSKExample/FSKExample.ino` | Added `#if defined(ARDUINO_AVR_FEATHER32U4)` / `#else` board pin guard |
| `examples/OOKExample/OOKExample.ino` | Added `REG_OOK_AVG` write to set `DEC_1_8_CHIP` (0x60 into bits [7:5]) |
| `extras/interop_tests/SX127x_OOK_Modem/SX127x_OOK_Modem.ino` | Changed `getRSSI()` → `getRSSI(false, true)` to avoid stopping receiver |
| `extras/interop_tests/SX127x_OOK_Modem/SX127x_OOK_Modem.ino` | Added 4–7 s random delay before TX + `randomSeed(micros())` in setup |

---

## Predefined Compile Tasks (SX1276_Radio_Lite)

The following VS Code tasks are defined in `.vscode/tasks.json` for compilation:

| Task label | Target sketch | Board FQBN |
|---|---|---|
| `Compile Arduino Sketch` | Active sketch (from `.vscode/arduino.json`) | from `arduino.json` |
| `Compile FSK Modem (ESP32)` | `extras/interop_tests/SX127x_FSK_Modem` | `esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new` |
| `Compile OOK Modem (ESP32)` | `extras/interop_tests/SX127x_OOK_Modem` | same |
| `Compile FSKExample (ESP32)` | `examples/FSKExample` | same |
| `Compile OOKExample (ESP32)` | `examples/OOKExample` | same |

# SX1276_Radio_Lite — Code Review Findings

**Review Date:** 2026-03-13  
**Files Reviewed:** `SX1276.cpp`, `SX1276.h`, `examples/BresserRxExample/BresserRxExample.ino`  
**Reference Sources:**
- `OUTLINE.md` (requirements)
- `docs/sx1276/DS_SX1276-7-8-9_W_APP_V7.pdf` (datasheet)
- `docs/sx1276/SX1276_77_8_ErrataNote_1.1_STD.pdf` (errata)
- RadioLib `SX127x.cpp` / `SX127x.h` (gold-standard implementation at https://github.com/jgromes/RadioLib)

---
## Prompts

*Check #file:OUTLINE.md and review the existing code in #SX1276.cpp and SX1276.h. Indicated any general bugs or flaws or implementations which do not meet the requirements in #file:OUTLINE.md. Also check the implementation against the specifications in #file:DS_SX1276-7-8-9_W_APP_V7.pdf and the errata note.*
*Outline any suggestions for changing the code before actually modifying the code.*

Attachments:
* OUTLINE.md
* SX1276.cpp
* SX1276.cpp
* docs/sx1276/DS_SX1276-7-8-9_W_APP_V7.pdf
* docs/sx1276/SX1276_77_8_ErrataNote_1.1_STD.pdf

Added:
*If this is still too much information for the context, exclude the LoRa modulation specific parts.*
*If this is still too much information for the context, check all general initialization and reception related code first, i.e. skip the transmission related parts.*

Added:
*Also review against the SX1276 implementation in https://github.com/jgromes/RadioLib, which shall be considered as gold standard. It works for the intended use cases.*

After the reply was completed:
*Summarize your findings in a file which can be used by other agents in a new context.*

For each issue to be fixed:
*See issue ID <x>. Cross check the two implementations and the data sheet. If the finding can be confirmed, implement a fix in SX1276_Radio_Lite.*

## Priority 1 — Critical Bugs (will cause incorrect behaviour)

### A. ~~`setPacketConfig()` — fixed/variable length logic is INVERTED~~ ✅ FIXED

**File:** `SX1276.cpp`, function `setPacketConfig(bool fixedLength, bool crcOn)`

Per SX1276 datasheet RegPacketConfig1 bit 7:
- `0` = fixed length
- `1` = variable length

Current code:
```cpp
if (fixedLength) {
    config1 |= 0x80;  // Sets bit 7 = 1 = VARIABLE, not fixed!
}
```

**Effect:** When `BresserRxExample` calls `radio.setPacketConfig(true, false)` requesting fixed-length mode, the chip is placed in variable-length mode. The first received byte is consumed as a length byte, misframing the entire payload by one byte. **Reception cannot work correctly.**

**Fix:**
```cpp
// Bit 7: 0 = Fixed, 1 = Variable (per datasheet)
if (!fixedLength) {
    config1 |= 0x80;  // Variable length
}
// fixedLength == true → leave bit 7 = 0 (fixed)
```

---

### B. ~~`setMode()` — OOK modulation bit (bit 5) is clobbered on every call~~ ✅ FIXED

**File:** `SX1276.cpp`, function `setMode(uint8_t mode)`

`setMode()` reads OP_MODE and masks only bit 7 (LoRa/FSK selector bit) when building the new value. Bit 5 (OOK vs FSK selector) is lost.

```cpp
// Bug: modulationMask only covers bit 7
const uint8_t modulationMask = SX1276_LORA_MODE | SX1276_FSK_OOK_MODE; // = 0x80
```

In `configFSK()`, after OOK bit 5 is set:
1. `opMode |= 0x20` — OOK bit set
2. `standby()` → `setMode(0x01)` is called
3. `setMode()` computes `(0x01 & ~0x80) | (currentOpMode & 0x80)` — bit 5 erased
4. Chip silently reverts to FSK

RadioLib avoids this by modifying **only bits [2:0]** of OP_MODE:
```cpp
// RadioLib: SPIsetRegValue(reg, mode, 2, 0, 5) — only bits 2:0 are modified
```

**Fix:** Change `setMode()` to only write bits [2:0], preserving bits [7:3]:
```cpp
int16_t SX1276::setMode(uint8_t mode) {
    uint8_t currentOpMode = readRegister(SX1276_REG_OP_MODE);
    uint8_t newOpMode = (currentOpMode & 0xF8) | (mode & 0x07);
    writeRegister(SX1276_REG_OP_MODE, newOpMode);
    waitForModeReady();
    return SX1276_ERR_NONE;
}
```
Mode-switching across LoRa/FSK boundary must be done explicitly by the caller (as `configFSK()` already does), not implicitly by `setMode()`.

---

### 1. ~~Coding Rate Off-by-One in `begin(float ...)`~~ ✅ FIXED

**File:** `SX1276.cpp`, inside `#ifdef LORA_ENABLED` `begin(float freq, ...)`:
```cpp
_cr = ((cr - 5) << 1);  // cr denominator 5 → result = 0x00, not SX1276_CR_4_5 = 0x02
```

Mapping table vs what the formula produces:

| cr arg | formula result | constant required |
|--------|---------------|-------------------|
| 5 | 0x00 | `SX1276_CR_4_5` = 0x02 |
| 6 | 0x02 | `SX1276_CR_4_6` = 0x04 |
| 7 | 0x04 | `SX1276_CR_4_7` = 0x06 |
| 8 | 0x06 | `SX1276_CR_4_8` = 0x08 |

For `cr=5` the result (0x00) fails the validity check in `setCodingRate()` and returns an error. For `cr=6`–`8` the wrong coding rate is silently applied.

**Fix:** `_cr = ((cr - 4) << 1);`

---

## Priority 2 — Bugs (incorrect results, degraded reception)

### G. ~~LoRa RSSI Calculation Missing SNR Correction~~ ✅ FIXED

**File:** `SX1276.cpp`, `getRSSI()`

For weak signals (SNR < 0 dB) the correct formula per the datasheet and confirmed by RadioLib is:
```
RSSI_dBm = -157 + PacketRSSI + SNR/4   [HF band, SNR < 0]
```
Current code never applies SNR correction. Error can be up to 30 dB for weak signals.

**Fix:**
```cpp
int16_t SX1276::getRSSI() {
    uint8_t rawRSSI = readRegister(SX1276_REG_PKT_RSSI_VALUE);
    int16_t rssi = (_freq < 862000000L) ? -164 + rawRSSI : -157 + rawRSSI;
    int8_t snr = getSNR();
    if (snr < 0) {
        rssi += snr / 4;  // getSNR() returns value already in 0.25 dB units
    }
    return rssi;
}
```

---

### 4. ~~`getFrequencyError()` — int32 Overflow~~ ✅ FIXED

**File:** `SX1276.cpp`, `getFrequencyError()`

```cpp
int32_t freqError = ((int32_t)rawError * bwHz) / 524288L;
```

`rawError` can be up to ~524 K; `bwHz` up to 500 000. Product can exceed 2³¹ — **undefined behaviour** on overflow.

**Fix:** Cast before multiplying:
```cpp
int32_t freqError = (int32_t)(((int64_t)rawError * bwHz) / 524288LL);
```

---

### C. ~~Sequencer Register Writes are Dead Code~~ ✅ FIXED

**File:** `SX1276.cpp`, `configFSK()`

```cpp
writeRegister(SX1276_REG_SEQ_CONFIG_1, 0x00);
writeRegister(SX1276_REG_SEQ_CONFIG_2, 0x24);
```

The hardware sequencer is never started (bit 7 of SEQ_CONFIG_1 must be set to 1 to start it). RadioLib does not configure these registers in `configFSK()`. These writes have no useful effect and should be removed.

---

### D. ~~`setRxBandwidth()` Overwrites Reserved Bits~~ ✅ FIXED

**File:** `SX1276.cpp`, `setRxBandwidth()`

```cpp
writeRegister(SX1276_REG_RX_BW, rxBw);  // overwrites all 8 bits
```

RadioLib writes only bits [4:0]. Bits [7:5] should be preserved per good practice.

**Fix:** Use read-modify-write on bits [4:0] only:
```cpp
uint8_t reg = readRegister(SX1276_REG_RX_BW);
writeRegister(SX1276_REG_RX_BW, (reg & 0xE0) | (rxBw & 0x1F));
```

---

### E. Frequency Deviation Validation Missing Carson's Rule Constraint

**File:** `SX1276.cpp`, `setFrequencyDeviation()`

Only validates absolute range. RadioLib also validates that `freqDev + bitRate/2 <= 250 kHz` (Carson's rule). Invalid combinations can cause transmitter spectral distortion.

---

### 10. SF6 Does Not Enable Implicit Header Mode

**File:** `SX1276.cpp`, `setSpreadingFactor()`

SF6 in LoRa **requires** `ImplicitHeaderModeOn = 1` (bit 0 of RegModemConfig1), per the datasheet. The detection registers are correctly set, but the header mode is not switched. Transmitting or receiving at SF6 in explicit header mode will silently fail.

**Fix:** When `sf == SX1276_SF_6`, also set bit 0 of RegModemConfig1.

---

### 11. FSK FIFO Overflow Not Validated

**File:** `SX1276.cpp`, `transmit()` (FSK path)

The SX1276 FSK packet-mode FIFO is **64 bytes** (confirmed by RadioLib header: `RADIOLIB_SX127X_MAX_PACKET_LENGTH_FSK = 64`). The code allows up to `SX1276_MAX_PACKET_LENGTH` (255) bytes in a single burst write, which will overflow the FIFO for any packet > 63 bytes in variable-length mode (> 64 bytes fixed-length).

**Fix:** Add validation or chunked FIFO writes (see RadioLib `fifoAdd()`/`fifoGet()` pattern).

---

## Priority 3 — Missing Required Feature

### 5. ~~`setPacketReceivedAction()` Not Implemented~~ ✅ FIXED

**File:** `SX1276.h` / `SX1276.cpp`

OUTLINE.md explicitly requires:
> *"Allow interrupt-based transmission and reception using `radio.setPacketReceivedAction()`"*

Neither the declaration nor any implementation exists. All `receive()` and `transmit()` calls are fully blocking (polling `digitalRead()` or register flags in a loop).

RadioLib implements this as:
```cpp
void SX127x::setPacketReceivedAction(void (*func)(void)) {
    // attachInterrupt on DIO0 pin, rising edge
    this->setDio0Action(func, RISING);
}
```

**Fix required:** Add `setPacketReceivedAction(void (*func)(void))` and `clearPacketReceivedAction()` to the class, plus a companion non-blocking `startReceive()`.

---

## Priority 4 — Datasheet / Errata Deviations

### 8. Reset Pin Driven HIGH Instead of Released

**File:** `SX1276.cpp`, `reset()`

```cpp
digitalWrite(_rstPin, LOW);
delay(10);
digitalWrite(_rstPin, HIGH);  // should be: pinMode(_rstPin, INPUT) or open-drain
```

DS §7.2.2 and the errata require RST to be *released* (tri-stated or driven through ≥10 kΩ) after the reset pulse, not driven actively HIGH. Driving it HIGH is harmless on many boards (which have an external pull-up) but violates the datasheet spec and can damage ICs without external resistors.

---

### 9. ~~LoRa/FSK Mode-Switch Must Transit Through Sleep (Errata)~~ ✅ FIXED

The errata note requires all transitions across the LoRa/FSK boundary to pass through sleep mode. `configFSK()` does this correctly. However, `setModulation()` calls `config()` without explicit sleep — when called at runtime while the chip is in RX or TX mode, the boundary crossing is incorrect. Callers should ensure the chip is in sleep or standby before switching modulation.

---

### 12. ~~`waitForModeReady()` Uses Fixed Delay~~ ✅ FIXED

**File:** `SX1276.cpp`

```cpp
void SX1276::waitForModeReady() {
    delay(2);
}
```

In FSK mode, IRQ_FLAGS_1 bit 7 (`ModeReady`) can be polled to know when the mode switch is complete, making the fixed delay both wasteful and potentially insufficient for slow crystal start-up. RadioLib uses `delay(1)` after sleep only; for mode changes it relies on `SPIsetRegValue()`'s built-in read-back verification.

---

## Priority 5 — Minor / Cleanup

### F. ~~FSK Preamble Unit Inconsistency vs RadioLib~~ ✅ FIXED

`setPreambleLength()` in FSK mode accepts **bytes**. RadioLib accepts **bits** and divides by 8 internally. This is internally consistent but diverges from RadioLib's API. Users migrating from RadioLib will get 8× too many preamble bytes.

### 6. Float Types Used in Interfaces (OUTLINE.md violation)

OUTLINE.md states: *"Avoid floating point types if possible."* Float parameters appear in:
- `begin(float freq, float bw, ...)` — the RadioLib-compatible LoRa init
- `beginFSK(float freq, float br, float freqDev, float rxBw, ...)`
- `setFrequency(float freq)`
- `SX1276_FSTEP` macro (float constant, never used in code — should be removed)

The integer-Hz `begin(long freq, ...)` path is the correct approach for AVR.

### 7. ~~`gain` Parameter Silently Ignored~~ ✅ FIXED

`begin(float ..., uint8_t gain)` immediately discards `gain` with `(void)gain`. The parameter should be removed from the signature until implemented.

### 13. ~~`SX1276_FSTEP` Macro is Unused~~ ✅ FIXED

**File:** `SX1276.h`
```cpp
#define SX1276_FSTEP  (SX1276_FXOSC / 524288.0)  // float constant, never referenced
```
On AVR this risks pulling in the software float library. Should be removed.

### 14. ~~Wrong Bit-Number in `setSyncWord()` Comment~~ ✅ FIXED

**File:** `SX1276.cpp`, `setSyncWord(const uint8_t*, uint8_t)`
```cpp
// Bit 7: Sync On   ← wrong; Bit 7 is AutoRestartRxMode MSB
```
Per the datasheet RegSyncConfig bit 4 is `SyncOn`. The register value written is correct; only the comment is wrong.

### 15. ~~Misleading Error Code for Unconfigured Pins~~ ✅ FIXED

When pins are not set in the constructor and `begin(float ...)` is called, the function returns `SX1276_ERR_CHIP_NOT_FOUND`. A more informative error code (e.g., `SX1276_ERR_INVALID_PARAM`) would reduce debugging time.

---

## Summary Table

| ID | Severity | Category | Description |
|----|----------|----------|-------------|
| A | **Critical** | Bug | ~~`setPacketConfig()`: fixed/variable logic inverted (bit 7)~~ ✅ FIXED |
| B | **Critical** | Bug | ~~`setMode()` clobbers OOK modulation bit (bit 5) on every call~~ ✅ FIXED |
| 1 | **Critical** | Bug | ~~LoRa coding rate formula `((cr-5)<<1)` off-by-one; should be `((cr-4)<<1)`~~ ✅ FIXED |
| 5 | **Missing** | Feature | ~~`setPacketReceivedAction()` not implemented (required by OUTLINE.md)~~ ✅ FIXED |
| G | High | Bug | ~~LoRa `getRSSI()` missing SNR correction for SNR < 0 dB~~ ✅ FIXED |
| 4 | High | Bug | ~~`getFrequencyError()` int32 overflow; needs int64 intermediate~~ ✅ FIXED |
| C | Medium | Bug | ~~SEQ_CONFIG writes in `configFSK()` are dead code (sequencer never started)~~ ✅ FIXED |
| D | Medium | Bug | ~~`setRxBandwidth()` overwrites reserved bits [7:5]~~ ✅ FIXED |
| E | Medium | Bug | FreqDev validation missing Carson's rule: `freqDev + bitRate/2 ≤ 250 kHz` |
| 10 | Medium | Bug/DS | SF6 does not enable implicit header mode (required by datasheet) |
| 11 | Medium | Bug | FSK FIFO overflow for packets > 63 bytes not validated or handled |
| 8 | Low | DS | RST pin driven HIGH instead of released/tri-stated after reset |
| 9 | Low | Errata | ~~Runtime modulation switch via `setModulation()` may skip required sleep transit~~ ✅ FIXED |
| 12 | Low | Robustness | ~~`waitForModeReady()` uses fixed `delay(2)` instead of polling ModeReady flag~~ ✅ FIXED |
| F | Low | Inconsistency | ~~FSK preamble unit is bytes here, bits in RadioLib API~~ ✅ FIXED |
| 6 | Low | Req. | Float types in `begin(float)`, `beginFSK()` violate OUTLINE.md "avoid float" |
| 7 | Low | Req. | ~~`gain` parameter in `begin(float)` silently ignored~~ ✅ FIXED |
| 13 | Trivial | Cleanup | ~~Unused `SX1276_FSTEP` float macro in `SX1276.h`~~ ✅ FIXED |
| 14 | Trivial | Comment | ~~`setSyncWord()` comment says "Bit 7: Sync On", should be "Bit 4"~~ ✅ FIXED |
| 15 | Trivial | UX | ~~Returns `SX1276_ERR_CHIP_NOT_FOUND` when pins not configured~~ ✅ FIXED |

---

## Recommended Fix Order

1. **A** — Invert the fixed/variable logic in `setPacketConfig()` (one-liner, restores FSK reception)
2. **B** — Rewrite `setMode()` to modify only bits [2:0] (restores OOK functionality)
3. **1** — Fix `((cr-5)<<1)` → `((cr-4)<<1)` in `begin(float)` (restores LoRa coding rate)
4. **G + 4** — Fix RSSI SNR correction and int64 overflow in frequency error
5. **C, D, E** — Clean up dead code, bit-mask fix, validation improvement
6. **10, 11** — SF6 implicit header, FSK FIFO guard
7. **5** — Implement `setPacketReceivedAction()` / non-blocking `startReceive()`
8. **8, 9, 12** — Datasheet/errata compliance
9. **F, 6, 7, 13, 14, 15** — Minor cleanup and documentation

---

## Context for Implementing Agent

- **Target platform:** Adafruit Feather 32u4 with SX1276 (AVR ATmega32u4, 2.5 KB RAM). All fixes must not introduce dynamic allocation or excessive stack usage.
- **Primary use case in examples:** `BresserRxExample` — FSK, fixed-length 27-byte packets, 868.3 MHz, 8.21 kbps, 57.136 kHz deviation, 250 kHz RX BW, sync word {0xAA, 0x2D}.
- **LoRa modulation** is secondary (compile-time optional via `#define LORA_ENABLED`).
- **Flat class hierarchy:** no inheritance, no virtual functions, no dynamic allocation — keep this constraint when adding `setPacketReceivedAction()`.
- **RadioLib compatibility:** the public API should remain compatible with RadioLib where possible (constructor, `begin()`, `beginFSK()`, error codes).
- **The `sleep()` function** passes mode bits that include `SX1276_LORA_MODE` (0x80) directly to `setMode()`. After fix B this will no longer work — `sleep()` must write OP_MODE directly or call a separate internal helper that sets all 8 bits atomically.

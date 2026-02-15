# Deviations from RadioLib

This document lists all intentional deviations from the RadioLib SX1276 API in the SX1276_Radio_Lite library.

## Architecture Deviations

### 1. No Module Class ❌
**RadioLib:** Uses `Module` class for pin/SPI abstraction
```cpp
Module mod(cs, irq, rst);
SX1276 radio(&mod);
```

**This Library:** Direct pin specification
```cpp
SX1276 radio(cs, irq, rst);  // RadioLib-compatible style
// OR
SX1276 radio;  // Simplified style - pins in begin()
```

**Reason:** Simplification - eliminates pointer indirection and reduces memory overhead in this lightweight variant.

---

### 2. No Class Inheritance ❌
**RadioLib:** `SX1276` → `SX127x` → `PhysicalLayer` → `Module`

**This Library (SX1276_Radio_Lite):** Flat `SX1276` class only

**Reason:** Memory optimization - reduces vtable overhead and code size on AVR.

---

### 3. Static Memory Only ❌
**RadioLib:** Uses dynamic allocation in some places

**This Library (SX1276_Radio_Lite):** All static allocation, no `new`/`malloc`

**Reason:** Critical for AVR ATmega32u4 with limited RAM (2.5KB).

---

## API Deviations

### 4. Constructor Difference
**RadioLib:** Takes `Module*` pointer
```cpp
SX1276 radio(&mod);
```

**This Library (SX1276_Radio_Lite):** Takes pins directly (optional)
```cpp
SX1276 radio(cs, irq, rst);  // RadioLib-compatible
SX1276 radio();              // Simplified - pins in begin()
```

**Compatibility:** RadioLib-compatible constructor added for migration.

---

### 5. begin() - Two Signatures
**RadioLib:** Single `begin()` taking MHz and parameters
```cpp
int state = radio.begin(freq, bw, sf, cr, syncWord, power, preambleLength, gain);
```

**This Library (SX1276_Radio_Lite):** Two versions
```cpp
// Simplified API - pins in begin(), Hz
int16_t state = radio.begin(freqHz, cs, rst, dio0);

// RadioLib-compatible - pins in constructor, MHz
int16_t state = radio.begin(freq, bw, sf, cr, syncWord, power, preambleLength, gain);
```

**Compatibility:** RadioLib-compatible version available when using pin-configuring constructor.

---

### 6. Frequency Units
**RadioLib:** Always MHz (float)
```cpp
radio.setFrequency(915.0);  // MHz
```

**This Library (SX1276_Radio_Lite):** Both Hz (long) and MHz (float) supported
```cpp
radio.setFrequency(915000000L);  // Hz - simplified
radio.setFrequency(915.0);       // MHz - RadioLib-compatible
```

**Compatibility:** Overloaded method supports both.

---

### 7. Transmit API
**RadioLib:** Supports `String` and byte arrays
```cpp
radio.transmit("Hello");
radio.transmit(data, len);
```

**This Library (SX1276_Radio_Lite):** Byte arrays only
```cpp
radio.transmit((uint8_t*)"Hello", 5);
radio.transmit(data, len);
```

**Reason:** Avoids String class overhead on memory-constrained devices.

---

### 8. Compile-Time Feature Selection ⚠️
**RadioLib:** All features always available

**This Library (SX1276_Radio_Lite):** Conditional compilation
```cpp
#define LORA_ENABLED      // Enable LoRa
#define FSK_OOK_ENABLED   // Enable FSK/OOK
```

**Reason:** Reduces code size - only include what you need.

---

## Feature Deviations

### 9. No Interrupt-Driven Mode ❌
**RadioLib:** Supports interrupt callbacks for TX/RX

**This Library (SX1276_Radio_Lite):** Blocking operations only

**Reason:** Simplification - reduces complexity and memory usage.

---

### 10. No Protocol Implementations ❌
**RadioLib:** Includes LoRaWAN, RTTY, Morse, etc.

**This Library (SX1276_Radio_Lite):** Raw radio only

**Reason:** Memory constraints - protocol stacks are large.

---

### 11. No CAD (Channel Activity Detection) ❌
**RadioLib:** Has `scanChannel()`, `startChannelScan()`

**This Library (SX1276_Radio_Lite):** Not implemented

**Reason:** Advanced feature, not critical for basic operation.

---

### 12. No FHSS (Frequency Hopping) ❌
**RadioLib:** Supports frequency hopping

**This Library (SX1276_Radio_Lite):** Not implemented

**Reason:** Complex feature, limited use cases.

---

### 13. No Direct Mode ❌
**RadioLib:** Supports direct modulation

**This Library (SX1276_Radio_Lite):** Not implemented

**Reason:** Rarely used, adds complexity.

---

### 14. Limited Error Codes
**RadioLib:** ~30+ specific error codes

**This Library (SX1276_Radio_Lite):** ~14 focused error codes

**Reason:** Reduces code size while covering common cases.

---

### 15. No SPI Settings Configuration
**RadioLib:** Allows custom SPI settings

**This Library (SX1276_Radio_Lite):** Fixed at 2 MHz, MSB first

**Reason:** Conservative defaults work for all supported platforms.

---

## Method Name Differences

### 16. setModem() Added as Alias
**RadioLib:** `setModem(RADIOLIB_SX127X_LORA)`

**This Library (SX1276_Radio_Lite):** Both supported
```cpp
radio.setModulation(SX1276_MODULATION_LORA);  // Primary
radio.setModem(SX1276_MODULATION_LORA);       // Alias for RadioLib
```

**Compatibility:** Alias added for familiarity.

---

### 17. getRSSI() Location
**RadioLib:** Returns last packet RSSI after receive

**This Library (SX1276_Radio_Lite):** 
- LoRa: `getRSSI()` - after receive
- FSK/OOK: `getRSSI_FSK()` - current RSSI

**Reason:** Different register addresses in FSK vs LoRa modes.

---

## Constant Name Differences

### 18. Error Code Prefix
**RadioLib:** `RADIOLIB_ERR_*`

**This Library (SX1276_Radio_Lite):** `SX1276_ERR_*`

**Reason:** Library-specific naming.

---

### 19. Modulation Type Constants
**RadioLib:** `RADIOLIB_SX127X_LORA`, `RADIOLIB_SX127X_FSK_OOK`

**This Library (SX1276_Radio_Lite):** `SX1276_MODULATION_LORA`, `SX1276_MODULATION_FSK`, `SX1276_MODULATION_OOK`

**Reason:** More explicit modulation types.

---

### 20. Sync Word Constants
**RadioLib:** `RADIOLIB_SX127X_SYNC_WORD` (0x12)

**This Library (SX1276_Radio_Lite):** Use `0x12` directly (no constant defined)

**Reason:** Simple values don't need named constants.

---

## Summary

**Total Deviations:** 20 documented

**Critical (Breaking):** 8
- No Module class
- No inheritance
- Static memory only
- Compile-time features
- No interrupt mode
- No protocols
- No CAD/FHSS
- Limited error codes

**Compatible (Mitigated):** 12
- Constructor has compatible version
- begin() has compatible version
- setFrequency() accepts both units
- setModem() alias added
- Method names mostly same
- Constants can be adapted

**Migration Impact:** Low to Medium
- Simple applications: Easy migration using RadioLib-compatible API
- Complex applications: May require refactoring or staying with RadioLib

**Recommendation:** 
- ✅ Use this library (SX1276_Radio_Lite) for: Simple LoRa/FSK/OOK communication on memory-constrained devices
- ❌ Use RadioLib for: Complex protocols, LoRaWAN, multiple radios, advanced features

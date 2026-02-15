# RadioLib Compatibility Guide

This document describes the compatibility between the **SX1276_Radio_Lite** library and [RadioLib](https://github.com/jgromes/RadioLib).

## Overview

The **SX1276_Radio_Lite** library is a **simplified port** of RadioLib's SX1276 implementation, specifically optimized for memory-constrained devices (AVR ATmega32u4) while maintaining compatibility with other architectures. It provides **two API styles**:

1. **Simplified API** - Direct pin specification, optimized for simplicity
2. **RadioLib-Compatible API** - Familiar to RadioLib users (requires pin configuration in constructor)

## Key Architectural Differences

### RadioLib Architecture
- Uses `Module` class for pin/SPI abstraction
- Class inheritance hierarchy (SX127x → SX1276)
- Dynamic pin configuration through Module object
- Comprehensive error handling and status codes
- Full protocol stack support

### This Library (SX1276_Radio_Lite)
- **No Module class** - pins specified directly
- **Flat class hierarchy** - no inheritance
- **Static memory allocation** - no dynamic allocation
- **Compile-time feature selection** - `LORA_ENABLED`, `FSK_OOK_ENABLED`
- **Raw radio only** - no protocol implementations
- **Smaller footprint** - optimized for AVR

## API Comparison

### Constructor

**RadioLib:**
```cpp
Module mod(cs, irq, rst, gpio);
SX1276 radio(&mod);
```

**This Library - Simplified API:**
```cpp
SX1276 radio;  // Pins specified in begin()
```

**This Library - RadioLib-Compatible API:**
```cpp
SX1276 radio(cs, irq, rst);  // Pin configuration in constructor
```

### Initialization - LoRa Mode

**RadioLib:**
```cpp
// Module already created
int state = radio.begin(freq, bw, sf, cr, syncWord, power, preambleLength, gain);
// freq in MHz, e.g., 915.0
```

**This Library - Simplified API:**
```cpp
int16_t state = radio.begin(freq, cs, rst, dio0);
// freq in Hz, e.g., 915000000L
// Then configure parameters individually:
radio.setSpreadingFactor(9);
radio.setBandwidth(SX1276_BW_125_KHZ);
```

**This Library - RadioLib-Compatible API:**
```cpp
SX1276 radio(cs, irq, rst);  // Configure pins first
int16_t state = radio.begin(freq, bw, sf, cr, syncWord, power, preambleLength, gain);
// freq in MHz, e.g., 915.0 (same as RadioLib)
```

### Initialization - FSK Mode

**RadioLib:**
```cpp
int state = radio.beginFSK(freq, br, freqDev, rxBw, power, preambleLength, enableOOK);
```

**This Library - Simplified API:**
```cpp
radio.begin(freq, cs, rst, dio0);  // freq in Hz
radio.setModulation(SX1276_MODULATION_FSK);
radio.setBitrate(4800);
radio.setFrequencyDeviation(5000);
```

**This Library - RadioLib-Compatible API:**
```cpp
SX1276 radio(cs, irq, rst);  // Configure pins first
int16_t state = radio.beginFSK(freq, br, freqDev, rxBw, power, preambleLength, enableOOK);
// Same signature as RadioLib
```

### Set Frequency

**RadioLib:**
```cpp
radio.setFrequency(915.0);  // MHz
```

**This Library - Both APIs:**
```cpp
radio.setFrequency(915.0);       // MHz (RadioLib-compatible)
radio.setFrequency(915000000L);  // Hz (simplified)
```

### Change Modulation

**RadioLib:**
```cpp
radio.setModem(RADIOLIB_SX127X_LORA);  // or RADIOLIB_SX127X_FSK_OOK
```

**This Library:**
```cpp
radio.setModulation(SX1276_MODULATION_LORA);  // or FSK, OOK
radio.setModem(SX1276_MODULATION_LORA);       // Alias for RadioLib compatibility
```

## Migration from RadioLib

### Step 1: Update Includes

**RadioLib:**
```cpp
#include <RadioLib.h>
```

**This Library:**
```cpp
#define LORA_ENABLED      // Enable LoRa support
// #define FSK_OOK_ENABLED  // Enable FSK/OOK if needed
#include <SX1276.h>
```

### Step 2: Remove Module Object

**RadioLib:**
```cpp
Module mod(8, 7, 4);  // cs, irq, rst
SX1276 radio(&mod);
```

**This Library - RadioLib-Compatible:**
```cpp
SX1276 radio(8, 7, 4);  // cs, irq, rst (directly in constructor)
```

### Step 3: Initialization

Keep your existing `begin()` or `beginFSK()` calls - they should work with the RadioLib-compatible API!

### Step 4: Update Constants (if needed)

**RadioLib Constants → This Library:**
- `RADIOLIB_SX127X_SYNC_WORD` (0x12) → `0x12` (use directly)
- `RADIOLIB_SX127X_LORA` → `SX1276_MODULATION_LORA`
- `RADIOLIB_SX127X_FSK_OOK` → `SX1276_MODULATION_FSK` or `SX1276_MODULATION_OOK`

## Complete Migration Example

### RadioLib Code

```cpp
#include <RadioLib.h>

Module mod(8, 7, 4);
SX1276 radio(&mod);

void setup() {
  Serial.begin(115200);
  
  int state = radio.begin(915.0, 125.0, 9, 7, 0x12, 10, 8, 0);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Success!");
  }
}

void loop() {
  String message = "Hello";
  radio.transmit(message);
  delay(1000);
}
```

### Migrated Code (RadioLib-Compatible API)

```cpp
#define LORA_ENABLED
#include <SX1276.h>

SX1276 radio(8, 7, 4);  // cs, irq, rst

void setup() {
  Serial.begin(115200);
  
  int16_t state = radio.begin(915.0, 125.0, 9, 7, 0x12, 10, 8, 0);
  if (state == SX1276_ERR_NONE) {
    Serial.println("Success!");
  }
}

void loop() {
  const char* message = "Hello";
  radio.transmit((uint8_t*)message, strlen(message));
  delay(1000);
}
```

**Key Differences:**
1. No `Module` object needed
2. Include changed from `RadioLib.h` to `SX1276.h`
3. Return type `int` → `int16_t` (compatible)
4. String → byte array for transmit (more memory efficient)

## Feature Compatibility Matrix

| Feature | RadioLib | This Library | Notes |
|---------|----------|--------------|-------|
| **LoRa Mode** | ✅ | ✅ | Full support with `LORA_ENABLED` |
| **FSK Mode** | ✅ | ✅ | Full support with `FSK_OOK_ENABLED` |
| **OOK Mode** | ✅ | ✅ | Full support with `FSK_OOK_ENABLED` |
| **Transmit** | ✅ | ✅ | Blocking mode only |
| **Receive** | ✅ | ✅ | Blocking mode only |
| **Interrupt-driven TX/RX** | ✅ | ❌ | Not implemented (use polling) |
| **RSSI/SNR** | ✅ | ✅ | Available in LoRa mode |
| **Frequency Error** | ✅ | ✅ | Available in LoRa mode |
| **CAD** | ✅ | ❌ | Not implemented |
| **LoRaWAN** | ✅ | ❌ | Not implemented (raw radio only) |
| **RTTY/Morse/etc** | ✅ | ❌ | Not implemented (raw radio only) |
| **Multiple modules** | ✅ | ✅ | Create multiple instances |

## Differences & Limitations

### What's Different

1. **No Module Abstraction**: Pins are specified directly, not through Module object
2. **No Inheritance**: Flat class hierarchy for smaller code size
3. **Compile-time Features**: Use `#define` to enable LoRa or FSK/OOK
4. **Blocking Only**: No interrupt-driven callbacks (simpler, less RAM)
5. **No Protocol Stack**: Raw radio only, no LoRaWAN/RTTY/etc.
6. **Simpler Error Codes**: Fewer error codes, focused on common cases

### What's Compatible

1. **Method Names**: Most methods have the same or similar names
2. **Return Types**: `int16_t` for status codes (compatible with `int`)
3. **Parameter Types**: Same types for most parameters
4. **Frequency Range**: Same 137-1020 MHz range
5. **Power Range**: Same 2-17 dBm range
6. **LoRa Parameters**: Same SF, BW, CR ranges

## Recommended Approach

For **new projects**: Use the **simplified API** - it's more direct and efficient.

```cpp
SX1276 radio;
radio.begin(915000000L, 8, 4, 7);  // Hz, cs, rst, dio0
```

For **migrating from RadioLib**: Use the **RadioLib-compatible API** - minimal changes needed.

```cpp
SX1276 radio(8, 7, 4);          // cs, irq, rst
radio.begin(915.0);              // MHz with defaults
```

## Support and Issues

If you encounter compatibility issues when migrating from RadioLib:

1. Check that you've enabled the required features (`LORA_ENABLED` or `FSK_OOK_ENABLED`)
2. Verify pin numbers are correct (cs, irq/dio0, rst)
3. Check frequency units (MHz for RadioLib-compatible API, Hz for simplified)
4. Review the error codes - they may differ slightly
5. Consult this guide for equivalent methods

For features not available in this simplified library, consider:
- Using RadioLib for complex requirements (LoRaWAN, multiple protocols)
- Using this library for simple, memory-constrained applications (AVR)

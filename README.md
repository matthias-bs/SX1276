# SX1276

Small SX1276 library for Arduino supporting LoRa, FSK, and OOK modulations, optimized for memory-constrained devices.

## Description

This is a simplified port of [RadioLib](https://github.com/jgromes/RadioLib)'s SX1276 implementation, specifically optimized for the Adafruit Feather 32u4 with RFM95 LoRa Radio, while maintaining compatibility with other Arduino architectures.

**📖 Migrating from RadioLib?** See the [RadioLib Compatibility Guide](docs/RADIOLIB_COMPATIBILITY.md) for detailed migration instructions.

### Features

- **Memory Efficient**: Optimized for AVR ATmega32u4 with limited RAM
- **No Dynamic Allocation**: All memory is statically allocated
- **Flat Class Hierarchy**: No inheritance for reduced overhead
- **Multiple Modulations**: Support for LoRa, FSK, and OOK
- **Optional Modes**: Enable LoRa with `#define LORA_ENABLED` and/or FSK/OOK with `#define FSK_OOK_ENABLED`
- **RadioLib-Compatible API**: Provides RadioLib-compatible methods for easy migration
- **Multi-Architecture**: Compatible with AVR, ESP32, ESP8266, and RP2040
- **Simple API**: Easy-to-use interface for all modulation types
- **Debug Support**: Optional debug output with `#define SX1276_DEBUG`

## Installation

### Arduino IDE

1. Download this repository as a ZIP file
2. In Arduino IDE: Sketch → Include Library → Add .ZIP Library
3. Select the downloaded ZIP file

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps = 
    https://github.com/matthias-bs/SX1276.git
```

## Usage

**Note:** LoRa and FSK/OOK modulations are **enabled by default**. To disable them for minimal memory usage, edit `src/SX1276.h` and comment out `#define LORA_ENABLED` and/or `#define FSK_OOK_ENABLED`.

This library provides **two API styles** to suit different needs:

### 1. Simplified API (Recommended for New Projects)

Direct and memory-efficient - pins specified in `begin()`:

```cpp
#include <SX1276.h>

SX1276 radio;

void setup() {
    Serial.begin(115200);
    
    // Initialize: freq in Hz, then pins
    int16_t state = radio.begin(915000000L, 8, 4, 7);  // freq, cs, rst, dio0
    
    if (state == SX1276_ERR_NONE) {
        Serial.println("Radio initialized!");
    }
    
    // Configure parameters
    radio.setSpreadingFactor(SX1276_SF_7);
    radio.setBandwidth(SX1276_BW_125_KHZ);
}

void loop() {
    const char* message = "Hello LoRa!";
    radio.transmit((uint8_t*)message, strlen(message));
    delay(5000);
}
```

### 2. RadioLib-Compatible API (For Migration)

Familiar to RadioLib users - pins in constructor, frequency in MHz:

```cpp
#include <SX1276.h>

SX1276 radio(8, 7, 4);  // cs, irq, rst (like RadioLib's Module)

void setup() {
    Serial.begin(115200);
    
    // RadioLib-style begin with MHz and optional parameters
    int16_t state = radio.begin(915.0);  // Frequency in MHz
    // Or with full parameters:
    // state = radio.begin(915.0, 125.0, 9, 7, 0x12, 10, 8, 0);
    
    if (state == SX1276_ERR_NONE) {
        Serial.println("Radio initialized!");
    }
}

void loop() {
    const char* message = "Hello LoRa!";
    radio.transmit((uint8_t*)message, strlen(message));
    delay(5000);
}
```

See the [BasicExample](examples/BasicExample/BasicExample.ino) for simplified API or [RadioLibCompatible](examples/RadioLibCompatible/RadioLibCompatible.ino) for RadioLib-compatible API.

### Complete Example (Simplified API)

```cpp
#define LORA_ENABLED  // Enable LoRa modulation
#include <SX1276.h>

// Pin definitions for Adafruit Feather 32u4 RFM95
#define LORA_CS    8
#define LORA_RST   4
#define LORA_DIO0  7

SX1276 radio;

void setup() {
    Serial.begin(115200);
    
    // Initialize radio at 915 MHz
    int16_t state = radio.begin(915000000L, LORA_CS, LORA_RST, LORA_DIO0);
    
    if (state == SX1276_ERR_NONE) {
        Serial.println("Radio initialized!");
    } else {
        Serial.println("Radio initialization failed!");
    }
    
    // Optional: Configure LoRa parameters
    radio.setSpreadingFactor(SX1276_SF_7);
    radio.setBandwidth(SX1276_BW_125_KHZ);
    radio.setCodingRate(SX1276_CR_4_5);
    radio.setPower(17, true);  // 17 dBm with PA_BOOST
}

void loop() {
    // Transmit a message
    const char* message = "Hello LoRa!";
    int16_t state = radio.transmit((uint8_t*)message, strlen(message));
    
    if (state == SX1276_ERR_NONE) {
        Serial.println("Transmission successful!");
    }
    
    delay(5000);
}
```

See the [BasicExample](examples/BasicExample/BasicExample.ino) for a complete LoRa example.

### FSK Example

```cpp
#define FSK_OOK_ENABLED  // Enable FSK/OOK modulation
#include <SX1276.h>

SX1276 radio;

void setup() {
    Serial.begin(115200);
    
    // Initialize radio at 915 MHz
    radio.begin(915000000L, 8, 4, 7);
    
    // Set modulation to FSK
    radio.setModulation(SX1276_MODULATION_FSK);
    
    // Configure FSK parameters
    radio.setBitrate(4800);                          // 4.8 kbps
    radio.setFrequencyDeviation(5000);               // 5 kHz
    radio.setRxBandwidth(SX1276_RX_BW_10_4_KHZ_FSK);
    
    uint8_t syncWord[] = {0x2D, 0xD4};
    radio.setSyncWord(syncWord, 2);
}

void loop() {
    const char* message = "Hello FSK!";
    radio.transmit((uint8_t*)message, strlen(message));
    delay(5000);
}
```

See the [FSKExample](examples/FSKExample/FSKExample.ino) for a complete FSK example.

### OOK Example

```cpp
#define FSK_OOK_ENABLED  // Enable FSK/OOK modulation
#include <SX1276.h>

SX1276 radio;

void setup() {
    Serial.begin(115200);
    
    // Initialize radio at 433 MHz
    radio.begin(433000000L, 8, 4, 7);
    
    // Set modulation to OOK
    radio.setModulation(SX1276_MODULATION_OOK);
    
    // Configure OOK parameters
    radio.setBitrate(4800);                          // 4.8 kbps
    radio.setFrequencyDeviation(0);                  // 0 Hz (OOK)
    radio.setRxBandwidth(SX1276_RX_BW_10_4_KHZ_FSK);
    
    uint8_t syncWord[] = {0x69, 0x81};
    radio.setSyncWord(syncWord, 2);
}

void loop() {
    const char* message = "Hello OOK!";
    radio.transmit((uint8_t*)message, strlen(message));
    delay(5000);
}
```

See the [OOKExample](examples/OOKExample/OOKExample.ino) for a complete OOK example.

## API Reference

### Initialization

```cpp
int16_t begin(long freq, int cs, int rst, int dio0);
```

Initialize the radio module.
- `freq`: Frequency in Hz (e.g., 915000000 for 915 MHz)
- `cs`: Chip select pin
- `rst`: Reset pin
- `dio0`: DIO0 interrupt pin
- Returns: `SX1276_ERR_NONE` on success, error code otherwise

```cpp
int16_t setModulation(uint8_t modulation);
```

Set modulation type.
- `modulation`: `SX1276_MODULATION_LORA`, `SX1276_MODULATION_FSK`, or `SX1276_MODULATION_OOK`
- Returns: `SX1276_ERR_NONE` on success, error code otherwise

### Transmission and Reception

```cpp
int16_t transmit(const uint8_t* data, size_t len);
```

Transmit data packet (blocking).
- Returns: `SX1276_ERR_NONE` on success, error code otherwise

```cpp
int16_t receive(uint8_t* data, size_t maxLen);
```

Receive data packet (blocking, 10 second timeout).
- Returns: Number of bytes received, or error code (< 0)

### Configuration (LoRa Mode)

When `LORA_ENABLED` is defined:

```cpp
int16_t setSpreadingFactor(uint8_t sf);    // SF6-SF12
int16_t setBandwidth(uint8_t bw);          // Use SX1276_BW_* constants
int16_t setCodingRate(uint8_t cr);         // Use SX1276_CR_* constants
int16_t setPreambleLength(uint16_t len);   // In symbols
int16_t setSyncWord(uint8_t sw);           // 0x12 private, 0x34 LoRaWAN
int16_t setCRC(bool enable);               // Enable/disable CRC
```

### Configuration (FSK/OOK Mode)

When `FSK_OOK_ENABLED` is defined:

```cpp
int16_t setBitrate(uint32_t bitrate);                      // 1200-300000 bps
int16_t setFrequencyDeviation(uint32_t freqDev);           // 600-200000 Hz (0 for OOK)
int16_t setRxBandwidth(uint8_t rxBw);                      // Use SX1276_RX_BW_* constants
int16_t setSyncWord(const uint8_t* syncWord, uint8_t len); // 1-8 bytes
int16_t setPreambleLength(uint16_t len);                   // In bits
int16_t setPacketConfig(bool fixedLength, bool crcOn);     // Packet format
```

### Signal Quality

LoRa mode:
```cpp
int16_t getRSSI();              // Get RSSI in dBm
int8_t getSNR();                // Get SNR (divide by 4 for actual dB)
int32_t getFrequencyError();    // Get frequency error in Hz
```

FSK/OOK mode:
```cpp
int16_t getRSSI_FSK();          // Get RSSI in dBm
```

### Power Management

```cpp
int16_t setPower(int8_t power, bool useBoost);  // Set TX power
int16_t standby();              // Enter standby mode
int16_t sleep();                // Enter sleep mode
```

## Modulation Types

The library supports three modulation types:

- **LoRa**: Long Range, requires `LORA_ENABLED` define
- **FSK**: Frequency Shift Keying, requires `FSK_OOK_ENABLED` define
- **OOK**: On-Off Keying, requires `FSK_OOK_ENABLED` define

You can enable both LoRa and FSK/OOK in the same project and switch between them using `setModulation()`.

## Pin Configuration

### Adafruit Feather 32u4 RFM95

| Signal | Pin |
|--------|-----|
| CS     | 8   |
| RST    | 4   |
| DIO0   | 7   |
| MOSI   | 16  |
| MISO   | 14  |
| SCK    | 15  |

### ESP32

Configure pins according to your hardware setup. The library uses the default SPI pins.

## Memory Optimization

This library is specifically designed for memory-constrained devices:

- **No `malloc` or `new`**: All allocations are static
- **Minimal RAM usage**: ~50-100 bytes of instance data (depending on enabled modes)
- **No floating point**: All calculations use integers (except one unused constant)
- **Compile-time options**: Enable only the modes you need
  - Define `LORA_ENABLED` to enable LoRa modulation
  - Define `FSK_OOK_ENABLED` to enable FSK/OOK modulation
  - Define both to enable all modes with runtime switching
- **Debug macros**: Debug output compiled out when not needed

## Compatibility

Tested and compatible with:

- ✅ AVR ATmega32u4 (Adafruit Feather 32u4 RFM95)
- ✅ ESP32
- ✅ ESP8266 (limited testing)
- ✅ RP2040 (limited testing)

## License

This library is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Credits

Based on [RadioLib](https://github.com/jgromes/RadioLib) by Jan Gromeš.

## Contributing

Contributions are welcome! Please open an issue or pull request on GitHub.


# SX1276

Small SX1276 LoRa library for Arduino, optimized for memory-constrained devices.

## Description

This is a simplified port of [RadioLib](https://github.com/jgromes/RadioLib)'s SX1276 implementation, specifically optimized for the Adafruit Feather 32u4 with RFM95 LoRa Radio, while maintaining compatibility with other Arduino architectures.

### Features

- **Memory Efficient**: Optimized for AVR ATmega32u4 with limited RAM
- **No Dynamic Allocation**: All memory is statically allocated
- **Flat Class Hierarchy**: No inheritance for reduced overhead
- **Optional LoRa Support**: Enable with `#define LORA_ENABLED`
- **Multi-Architecture**: Compatible with AVR, ESP32, ESP8266, and RP2040
- **Simple API**: Easy-to-use interface for basic LoRa operations
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

### Basic Example

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

See the [BasicExample](examples/BasicExample/BasicExample.ino) for a complete example.

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

### Signal Quality

```cpp
int16_t getRSSI();              // Get RSSI in dBm
int8_t getSNR();                // Get SNR (divide by 4 for actual dB)
int32_t getFrequencyError();    // Get frequency error in Hz
```

### Power Management

```cpp
int16_t setPower(int8_t power, bool useBoost);  // Set TX power
int16_t standby();              // Enter standby mode
int16_t sleep();                // Enter sleep mode
```

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
- **Minimal RAM usage**: ~50 bytes of instance data
- **No floating point**: All calculations use integers
- **Compile-time options**: Disable unused features with `#undef LORA_ENABLED`
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


# BRESSER_RX_EXAMPLE

Implement an example sketch BresserRxExample with the following parameters:

* carrier frequency:                   868.3 MHz
* bit rate:                            8.21 kbps
* frequency deviation:                 57.136417 kHz
* Rx bandwidth:                        250 kHz
* output power:                        10 dBm
* preamble length:                     32 bits (4 bytes)
* packet mode:                         fixed length, 27 bytes
* CRC filtering:                       disabled
* preamble:                            0xAA, 0xAA, 0xAA, 0xAA
* sync word:                           0xAA, 0x2D

**Note:** The actual Bresser protocol uses a 40-bit preamble (AA AA AA AA AA) followed by sync word 2D D4. Since the SX1276 preamble length must be specified in bytes, we use a 32-bit preamble (4 bytes of 0xAA) and set the sync word to AA 2D. This configuration causes the last sync byte (0xD4) to be received as the first byte of the payload, which matches the Bresser protocol expectation.

In the receive loop, only display valid Bresser frames (packets starting with 0xD4). Non-Bresser packets are silently ignored to reduce noise in the output.

## Implementation notes

The sketch initialization mirrors `WeatherSensor::begin()` in [BresserWeatherSensorReceiver](https://github.com/matthias-bs/BresserWeatherSensorReceiver) for the SX1276:

1. A `SX1276` instance is created with the pin-constructor (`cs`, `irq/DIO0`, `rst`).
2. `beginFSK(868.3, 8.21, 57.136417, 250.0, 10, 32)` configures all FSK parameters in one call (mirrors RadioLib's `beginFSK()`).
3. `setPacketConfig(true, false)` enables fixed-length mode with CRC disabled (mirrors `fixedPacketLengthMode()` + `setCrcFiltering(false)`).
4. `setSyncWord({0xAA, 0x2D}, 2)` sets the two-byte sync word.
5. `setPacketReceivedAction(setFlag)` + `startReceive()` starts interrupt-driven reception (mirrors the pattern in `WeatherSensor::begin()`).

The receive loop mirrors `WeatherSensor::getMessage()`:
- On interrupt: clear flag → `readData()` → `startReceive()` → check `buffer[0] == 0xD4`.

# OUTLINE

Create a port of RadioLib which only supports the SX1276 radio and is targeted to the Adafruit Feather 32u4 with SX1276 (https://learn.adafruit.com/adafruit-feather-32u4-radio-with-lora-radio-module/)

This lightweight radio library (SX1276_Radio_Lite) is designed with the following goals:
- With the AVR Atmega 32u4, memory usage is critical.
- Do not use dynamic memory allocation.
- Avoid floating point types if possible.
- Use macros for logging where appropriate.
- Take the SX1276 class from https://github.com/jgromes/RadioLib and simplify it into a flat class hierarchy without inheritance.
- Keep compatibility to other architectures (ESP32, ESP8266 and RP2040)
- Do not provide any protocol implementations
- The new SX1276 class shall be compatible with RadioLib.
- Include the LoRa modulation as an option (enabled with a define)
- Implement a simple interface to the Arduino SPI and pin functions.

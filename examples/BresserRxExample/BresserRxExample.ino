/*
 * BresserRxExample.ino
 * 
 * Receive-only example for SX1276 library configured for Bresser weather sensors.
 * Listens for FSK packets from Bresser weather station sensors and displays them.
 * Initialization mirrors WeatherSensor.cpp (BresserWeatherSensorReceiver) for SX1276.
 * Reception is interrupt-driven (setPacketReceivedAction + startReceive + readData),
 * matching the pattern in WeatherSensor::begin() / WeatherSensor::getMessage().
 * 
 * Radio Configuration:
 * - Carrier frequency:      868.3 MHz
 * - Bit rate:               8.21 kbps
 * - Frequency deviation:    57.136417 kHz
 * - Rx bandwidth:           250 kHz
 * - Output power:           10 dBm
 * - Preamble:               AA AA AA AA (32 bits; last preamble byte absorbed into sync)
 * - Sync word:              0xAA, 0x2D (last physical sync byte 0xD4 becomes first payload byte)
 * - Packet mode:            Fixed length, 27 bytes (MSG_BUF_SIZE)
 * - CRC filtering:          Disabled
 * 
 * Pin defaults (Adafruit Feather 32u4 RFM95):
 * - CS:   8   (RADIO_CS)
 * - RST:  4   (RADIO_RST)
 * - DIO0: 7   (RADIO_DIO0)
 * Other boards use LORA_CS / LORA_RST / LORA_IRQ from their BSP.
 */

#include <Arduino.h>

// Enable SX1276 debug prints for deeper runtime diagnostics
// (keeps output verbose; remove or comment out when no longer needed)
#define SX1276_DEBUG
// Note: FSK/OOK mode is enabled by default in the library
#include "SX1276.h"

#if defined(ARDUINO_AVR_FEATHER32U4)
// Pin definitions for Adafruit Feather 32u4 RFM95
#define RADIO_CS    8
#define RADIO_RST   4
#define RADIO_DIO0  7
#else
#define RADIO_CS    LORA_CS
#define RADIO_RST   LORA_RST
#define RADIO_DIO0  LORA_IRQ
#endif

// Fixed packet length for Bresser sensors (matches MSG_BUF_SIZE in WeatherSensor.h)
#define PACKET_LENGTH   27

// SX1276 instance — use pin constructor so beginFSK() can be called directly
// Parameter order: (cs, irq/DIO0, rst)
SX1276 radio(RADIO_CS, RADIO_DIO0, RADIO_RST);

// Flag set by the DIO0 interrupt when a complete packet has been written to the FIFO
static volatile bool receivedFlag = false;

// Interrupt service routine — must reside in IRAM on ESP32/ESP8266
#if defined(ESP8266) || defined(ESP32)
IRAM_ATTR
#endif
void setFlag(void) {
    receivedFlag = true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for Serial to be ready (or 5 seconds timeout)
  }
  // Give the serial monitor a moment to attach after reset
  delay(10000);
  
  Serial.println(F("Bresser Weather Sensor Receiver"));
  Serial.println(F("Initializing..."));
  
  // -----------------------------------------------------------------------
  // Initialize radio in FSK mode — mirrors WeatherSensor.cpp for SX1276:
  //   radio.beginFSK(frequency, 8.21, 57.136417, 250, 10, 32)
  // Parameters: freq[MHz], bitrate[kbps], freqDev[kHz], rxBw[kHz], power[dBm], preamble[bits]
  // -----------------------------------------------------------------------
  int16_t state = radio.beginFSK(868.3, 8.21, 57.136417, 250.0, 10, 32);
  if (state == SX1276_ERR_NONE) {
    Serial.println(F("Radio initialized (FSK) successfully!"));
  } else {
    Serial.print(F("beginFSK() failed, error: "));
    Serial.println(state);
    while (true) { delay(1000); }
  }

  // Fixed packet length, CRC disabled — mirrors fixedPacketLengthMode(27) + setCrcFiltering(false)
  state = radio.setPacketConfig(true, false);
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("setPacketConfig() failed, error: "));
    Serial.println(state);
    while (true) { delay(1000); }
  }
  radio.writeRegister(SX1276_REG_PAYLOAD_LENGTH_FSK, PACKET_LENGTH);

  // Sync word: 0xAA 0x2D
  // Physical frame: AA AA AA AA AA | 2D D4 | <26 payload bytes>
  // Preamble (32 bits = 4 bytes of 0xAA) + sync {0xAA, 0x2D} consumes the 5th preamble byte;
  // the last physical sync byte 0xD4 therefore arrives as the first byte of the payload.
  uint8_t syncWord[] = {0xAA, 0x2D};
  state = radio.setSyncWord(syncWord, 2);
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("setSyncWord() failed, error: "));
    Serial.println(state);
    while (true) { delay(1000); }
  }

  Serial.println(F("FSK configuration complete"));
  
  // Dump key registers for debugging
  Serial.println(F("Register dump (selected):"));
  uint8_t regs[] = {
    SX1276_REG_OP_MODE,
    SX1276_REG_BITRATE_MSB, SX1276_REG_BITRATE_LSB,
    SX1276_REG_FDEV_MSB, SX1276_REG_FDEV_LSB,
    SX1276_REG_RX_BW, SX1276_REG_AFC_BW,
    SX1276_REG_PREAMBLE_MSB_FSK, SX1276_REG_PREAMBLE_LSB_FSK,
    SX1276_REG_SYNC_CONFIG, SX1276_REG_SYNC_VALUE_1, SX1276_REG_SYNC_VALUE_2,
    SX1276_REG_PACKET_CONFIG_1, SX1276_REG_PAYLOAD_LENGTH_FSK,
    SX1276_REG_IRQ_FLAGS_1, SX1276_REG_IRQ_FLAGS_2,
    SX1276_REG_RSSI_VALUE_FSK
  };
  for (size_t i = 0; i < sizeof(regs); i++) {
    uint8_t v = radio.readRegister(regs[i]);
    Serial.print(F("0x"));
    if (regs[i] < 0x10) Serial.print('0');
    Serial.print(regs[i], HEX);
    Serial.print(F(": 0x"));
    if (v < 0x10) Serial.print('0');
    Serial.println(v, HEX);
  }
  Serial.println();
  
  // Verify we're in FSK mode by reading OP_MODE register
  uint8_t opMode = radio.readRegister(SX1276_REG_OP_MODE);
  Serial.print(F("OP_MODE register: 0x"));
  Serial.println(opMode, HEX);
  if (opMode & 0x80) {
    Serial.println(F("WARNING: Radio is in LoRa mode (bit 7 = 1)!"));
  } else {
    Serial.println(F("Radio is in FSK/OOK mode (bit 7 = 0)"));
  }
  
  // Verify packet configuration
  uint8_t pktConfig1 = radio.readRegister(SX1276_REG_PACKET_CONFIG_1);
  uint8_t payloadLen = radio.readRegister(SX1276_REG_PAYLOAD_LENGTH_FSK);
  Serial.print(F("PACKET_CONFIG_1: 0x"));
  Serial.print(pktConfig1, HEX);
  Serial.print(F(" (Fixed="));
  // PacketFormat bit 7: 0 = fixed length, 1 = variable length
  Serial.print((pktConfig1 & 0x80) ? F("No") : F("Yes"));
  Serial.print(F(", CRC="));
  Serial.print((pktConfig1 & 0x10) ? F("On") : F("Off"));
  Serial.println(F(")"));
  Serial.print(F("PAYLOAD_LENGTH: "));
  Serial.print(payloadLen);
  Serial.println(F(" bytes"));
  
  Serial.println(F("Radio Parameters:"));
  Serial.println(F("  Frequency:        868.3 MHz"));
  Serial.println(F("  Bit rate:         8.21 kbps"));
  Serial.println(F("  Freq deviation:   57.136 kHz"));
  Serial.println(F("  Rx bandwidth:     250 kHz"));
  Serial.println(F("  Packet length:    27 bytes (fixed)"));
  Serial.println(F("  Sync word:        0xAA 0x2D"));
  Serial.println();

  // Start interrupt-driven reception — mirrors WeatherSensor::begin() pattern:
  //   radio.setPacketReceivedAction(setFlag);
  //   radio.startReceive();
  radio.setPacketReceivedAction(setFlag);
  state = radio.startReceive();
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("startReceive() failed, error: "));
    Serial.println(state);
    while (true) { delay(1000); }
  }

  Serial.println(F("Listening for Bresser sensor packets (interrupt-driven)..."));
  Serial.println(F("(Only frames with first byte 0xD4 are displayed)"));
  Serial.println(F("(Heartbeat '.' printed every 10 s while waiting)"));
  Serial.println();
  Serial.flush();
}

void loop() {
  static uint32_t lastDot = 0;

  if (!receivedFlag) {
    // No packet yet — print a heartbeat dot every 10 s
    if (millis() - lastDot >= 10000) {
      Serial.print('.');
      Serial.flush();
      lastDot = millis();
    }
    return;
  }

  // -----------------------------------------------------------------------
  // Packet received — mirrors WeatherSensor::getMessage() for SX1276:
  //   receivedFlag = false;
  //   radio.readData(recvData, MSG_BUF_SIZE);
  //   rssi = radio.getRSSI();
  //   radio.startReceive();
  //   if (recvData[0] == 0xD4) { ... }
  // -----------------------------------------------------------------------
  receivedFlag = false;

  uint8_t buffer[PACKET_LENGTH];
  int16_t state = radio.readData(buffer, sizeof(buffer));
  int16_t rssi  = radio.getRSSI_FSK();

  // Restart reception immediately so we don't miss the next packet
  radio.startReceive();

  if (state > 0) {
    // Verify first byte is 0xD4 — the last physical sync byte that arrives
    // as the first payload byte (see setSyncWord comment above).
    // Mirrors: if (recvData[0] == 0xD4) in WeatherSensor::getMessage()
    if (buffer[0] == 0xD4) {
      Serial.println(F("========================================"));
      Serial.print(F("Received BRESSER packet ("));
      Serial.print(state);
      Serial.println(F(" bytes):"));

      Serial.print(F("Data: "));
      for (int i = 0; i < state; i++) {
        if (buffer[i] < 0x10) Serial.print('0');
        Serial.print(buffer[i], HEX);
        Serial.print(' ');
      }
      Serial.println();

      Serial.print(F("RSSI: "));
      Serial.print(rssi);
      Serial.println(F(" dBm"));

      Serial.println(F("========================================"));
      Serial.println();
    }
    // else: start byte != 0xD4 — silently discard

  } else if (state == SX1276_ERR_RX_TIMEOUT) {
    // Should not occur with interrupt-driven RX, but guard just in case
    Serial.print('.');
    Serial.flush();
  } else {
    // Reception error
    Serial.println();
    Serial.print(F("Reception error: "));
    Serial.println(state);
  }
}

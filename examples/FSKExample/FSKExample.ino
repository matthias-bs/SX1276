/*
 * FSKExample.ino
 *
 * FSK (Frequency Shift Keying) example for SX1276_Radio_Lite library
 * Demonstrates FSK transmission and reception
 *
 * Interop partner: extras/interop_tests/SX127x_FSK_Modem/SX127x_FSK_Modem.ino (RadioLib)
 *
 * Configuration (must match SX127x_FSK_Modem):
 *   Frequency     : 868 MHz
 *   Bit rate      : 4.8 kbps
 *   Freq deviation: 5 kHz
 *   RX bandwidth  : 41.7 kHz (crystal tolerance)
 *   Sync word     : 0x2D 0xD4
 *   Preamble      : 128 bits (16 bytes)
 *   Packet format : variable length, CRC on (CCITT, autoclear on)
 *   DC-free       : none
 */

#include <Arduino.h>

// FSK/OOK mode is enabled by default in the library.
#include <SX1276.h>

// Board-specific pin definitions
#if defined(ARDUINO_AVR_FEATHER32U4)
// Adafruit Feather 32u4 RFM95 LoRa 868/915 MHz
#define RADIO_CS    8
#define RADIO_RST   4
#define RADIO_DIO0  7
#pragma message "Using pinout for Adafruit Feather 32u4 RFM95 (CS=8, RST=4, DIO0=7)"
#elif defined(ARDUINO_DFROBOT_FIREBEETLE_ESP32)
// https://wiki.dfrobot.com/FireBeetle_ESP32_IOT_Microcontroller(V3.0)__Supports_Wi-Fi_&_Bluetooth__SKU__DFR0478
// https://wiki.dfrobot.com/FireBeetle_Covers_LoRa_Radio_868MHz_SKU_TEL0125
#define RADIO_CS      27 // D4
#define RADIO_RST     25 // D2
#define RADIO_DIO0    26 // D3
#define RADIO_GPIO     9 // D5
#elif defined(ARDUINO_TTGO_LoRa32_v21new)
// ESP32-based boards (Lilygo T3 LoRa32, TTGO LoRa32, etc.)
// The board package defines LORA_CS / LORA_RST / LORA_IRQ.
#define RADIO_CS    LORA_CS
#define RADIO_RST   LORA_RST
#define RADIO_DIO0  LORA_IRQ
#else
#if !defined(LORA_CS) || !defined(LORA_RST) || !defined(LORA_IRQ)
#error "Unsupported board: define RADIO_CS/RADIO_RST/RADIO_DIO0 for this target."
#endif
#endif

// Radio frequency (868 MHz for EU)
#define RADIO_FREQ 868000000L

// Low-power preset for close-range interoperability testing.
static const int8_t TX_POWER_DBM = 5;
static const uint16_t FSK_PREAMBLE_BITS = 128;

// Create SX1276 instance
SX1276 radio;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for Serial to be ready (or 5 seconds timeout)
  }
  
  Serial.println(F("SX1276_Radio_Lite - FSK Example"));
  Serial.println(F("Initializing..."));

  // Initialize radio with correct board pin mapping
  int16_t state = radio.begin(RADIO_FREQ, RADIO_CS, RADIO_RST, RADIO_DIO0);
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("Failed to initialize radio, error code: "));
    Serial.println(state);
    while (true) delay(1000);
  }

  // Set modulation to FSK
  state = radio.setModulation(SX1276_MODULATION_FSK);
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("Failed to set FSK modulation, error code: "));
    Serial.println(state);
    while (true) delay(1000);
  }

  // Configure FSK parameters for interop
  radio.setBitrate(4800);                          // 4.8 kbps
  radio.setFrequencyDeviation(5000);               // 5 kHz deviation
  radio.setRxBandwidth(SX1276_RX_BW_41_7_KHZ_FSK); // 41.7 kHz bandwidth
  // Align AFC bandwidth with RX bandwidth
  {
    uint8_t afc = radio.readRegister(SX1276_REG_AFC_BW);
    radio.writeRegister(SX1276_REG_AFC_BW, (afc & 0xE0) | (SX1276_RX_BW_41_7_KHZ_FSK & 0x1F));
  }
  radio.setPower(TX_POWER_DBM, true);              // low TX power for near-field tests

  // Set sync word to match interop partner
  uint8_t syncWord[] = {0x2D, 0xD4};
  radio.setSyncWord(syncWord, 2);

  // Use a longer preamble for robust lock
  radio.setPreambleLength(FSK_PREAMBLE_BITS);

  // Set packet format (variable length, CRC enabled)
  radio.setPacketConfig(false, true);

  // Seed PRNG to decorrelate peer timing
  randomSeed(micros());

  Serial.print(F("FSK configuration complete (TX power "));
  Serial.print(TX_POWER_DBM);
  Serial.print(F(" dBm, preamble "));
  Serial.print(FSK_PREAMBLE_BITS);
  Serial.println(F(" bits)"));

  Serial.println(F("Ready for interop test."));
}

// One-time diagnostic: monitor RSSI and IRQ flags during a 10-second RX window.
// Reveals whether the receiver detects any RF energy (RSSI spike) or preamble
// when the peer board transmits.  Run FSK_Modem first so it is transmitting
// during this window.
static void diagnosticRxScan() {
  // Standby → clear IRQ → RX continuous (direct register writes)
  uint8_t op = radio.readRegister(SX1276_REG_OP_MODE);
  radio.writeRegister(SX1276_REG_OP_MODE, (op & 0xF8) | 0x01);  // Standby
  delay(1);
  radio.writeRegister(SX1276_REG_IRQ_FLAGS_1, 0xFF);
  radio.writeRegister(SX1276_REG_IRQ_FLAGS_2, 0xFF);
  op = radio.readRegister(SX1276_REG_OP_MODE);
  radio.writeRegister(SX1276_REG_OP_MODE, (op & 0xF8) | 0x05);  // RX continuous
  delay(1);

  Serial.println(F("--- RSSI/IRQ scan (10 s) ---"));
  Serial.println(F("time_ms OP_MODE RSSI_dBm IRQ1 IRQ2"));
  uint32_t start = millis();
  while (millis() - start < 10000) {
    uint8_t opMode = radio.readRegister(SX1276_REG_OP_MODE);
    uint8_t rssiRaw = radio.readRegister(0x11);            // RegRssiValue (FSK)
    uint8_t irq1 = radio.readRegister(SX1276_REG_IRQ_FLAGS_1);
    uint8_t irq2 = radio.readRegister(SX1276_REG_IRQ_FLAGS_2);

    Serial.print(millis() - start);
    Serial.print(F("\t0x")); Serial.print(opMode, HEX);
    Serial.print(F("\t-"));  Serial.print(rssiRaw / 2);
    Serial.print(F("\t0x")); Serial.print(irq1, HEX);
    Serial.print(F("\t0x")); Serial.println(irq2, HEX);

    if (irq1 & 0x02) Serial.println(F("  *** PREAMBLE DETECTED ***"));
    if (irq1 & 0x01) Serial.println(F("  *** SYNC ADDRESS MATCH ***"));

    delay(200);
  }

  // Back to standby
  op = radio.readRegister(SX1276_REG_OP_MODE);
  radio.writeRegister(SX1276_REG_OP_MODE, (op & 0xF8) | 0x01);
  Serial.println(F("--- end RSSI/IRQ scan ---"));
}

void loop() {
  // RX-first symmetric loop: listen, then transmit with probability
  Serial.println(F("Listening for packet..."));
  uint8_t buffer[64];
  int16_t state = radio.receive(buffer, sizeof(buffer), 4000);

  bool shouldTransmit = false;
  if (state > 0) {
    Serial.print(F("Received packet: "));
    for (int i = 0; i < state; i++) Serial.write(buffer[i]);
    Serial.println();
    Serial.print(F("RSSI: "));
    Serial.print(radio.getRSSI_FSK());
    Serial.println(F(" dBm"));
    shouldTransmit = true;
    delay(40 + random(0, 120)); // short reply delay
  } else if (state == SX1276_ERR_RX_TIMEOUT) {
    Serial.println(F("No packet received (timeout)"));
    // Beacon with 60% probability to reduce collision risk
    shouldTransmit = (random(0, 100) < 60);
  } else {
    Serial.print(F("Reception failed, error code: "));
    Serial.println(state);
    // Always beacon on error so partner can confirm node is alive
    shouldTransmit = true;
  }

  if (shouldTransmit) {
    static uint32_t counter = 0;
    char message[50];
    snprintf(message, sizeof(message), "FSK msg #%lu", (unsigned long)counter);
    counter++;
    Serial.print(F("Transmitting: "));
    Serial.println(message);
    state = radio.transmit((uint8_t*)message, strlen(message));
    if (state == SX1276_ERR_NONE) {
      Serial.println(F("Transmission successful!"));
    } else {
      Serial.print(F("Transmission failed, error code: "));
      Serial.println(state);
    }
  }

  // Jittered cycle timing to avoid phase-lock
  delay(120 + random(0, 600));
  Serial.println();
}

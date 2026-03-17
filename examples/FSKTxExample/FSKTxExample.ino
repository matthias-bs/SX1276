/*
  FSKTxExample.ino — FSK transmit-only example for SX1276_Radio_Lite

  Target board : Adafruit Feather 32u4 RFM95 LoRa 868/915 MHz (AVR ATmega32U4)
  Interop peer : extras/interop_tests/SX127x_FSK_Modem/SX127x_FSK_Modem.ino
                 (RadioLib on an SX1276/SX1278 board)

  This sketch configures the on-board RFM95 (SX1276) as a pure FSK transmitter
  using the same parameters as SX127x_FSK_Modem.ino so that the peer can decode
  every packet this board sends. No reception is ever attempted.

  FSK parameters (must be identical on both sides):
    Frequency     : 868 MHz
    Bit rate      : 4.8 kbps
    Freq deviation: 5 kHz
    RX bandwidth  : 41.7 kHz  (set for completeness; not used in TX-only mode)
    Sync word     : 0x2D 0xD4
    Preamble      : 128 bits  (16 bytes)
    Packet format : variable length, CRC on (CCITT, autoclear on)
    DC-free       : none

  Pinout (Adafruit Feather 32u4 RFM95):
    CS  → pin 8
    RST → pin 4
    DIO0→ pin 7

  Prerequisites:
    - Install SX1276_Radio_Lite library (this library).
    - Board package: Adafruit AVR Boards (Feather 32u4).
    - FQBN: adafruit:avr:feather32u4
*/

#include <Arduino.h>
#include <SX1276.h>

// ---------------------------------------------------------------------------
// Pin definitions — Adafruit Feather 32u4 RFM95
// ---------------------------------------------------------------------------
#if defined(ARDUINO_AVR_FEATHER32U4)
#define RADIO_CS    8
#define RADIO_RST   4
#define RADIO_DIO0  7
#pragma message "FSKTxExample: using Adafruit Feather 32u4 RFM95 pins (CS=8, RST=4, DIO0=7)"
#else
#error "FSKTxExample is targeted at the Adafruit Feather 32u4 RFM95. " \
       "Define RADIO_CS / RADIO_RST / RADIO_DIO0 for other boards and remove this error."
#endif

// ---------------------------------------------------------------------------
// Constants — must match SX127x_FSK_Modem.ino
// ---------------------------------------------------------------------------
#define RADIO_FREQ        868000000L  // 868 MHz

// Low-power preset for close-range interoperability testing.
// Increase only when larger separation requires it.
static const int8_t TX_POWER_DBM     = 5;
static const uint16_t FSK_PREAMBLE_BITS = 128;

// Interval between successive transmissions (milliseconds).
// A small random jitter is added each cycle to avoid collisions when multiple
// identical boards are active simultaneously.
//
// EU duty-cycle compliance (ETSI EN 300 220-1, 868.0–868.6 MHz sub-band: 1%):
//   Worst-case airtime at 4.8 kbps: preamble(128 b) + sync(16 b) + len(8 b)
//     + payload(~152 b) + CRC(16 b) ≈ 67 ms
//   Required quiet time: 67 ms × 99 ≈ 6633 ms → use 7000 ms minimum.
static const uint16_t TX_INTERVAL_MS = 7000;

// ---------------------------------------------------------------------------
// Radio object
// ---------------------------------------------------------------------------
SX1276 radio;

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait up to 5 s for USB-CDC on 32u4
  }

  Serial.println(F("SX1276_Radio_Lite - FSK TX Example"));
  Serial.println(F("Peer: SX127x_FSK_Modem (RadioLib)"));
  Serial.println(F("Initializing radio..."));

  int16_t state = radio.begin(RADIO_FREQ, RADIO_CS, RADIO_RST, RADIO_DIO0);
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("Radio init failed, code: "));
    Serial.println(state);
    while (true) { delay(1000); }
  }
  Serial.println(F("Radio initialized."));

  // Select FSK modulation
  state = radio.setModulation(SX1276_MODULATION_FSK);
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("setModulation failed, code: "));
    Serial.println(state);
    while (true) { delay(1000); }
  }

  // Bit rate: 4.8 kbps
  radio.setBitrate(4800);

  // Frequency deviation: 5 kHz
  radio.setFrequencyDeviation(5000);

  // RX bandwidth: 41.7 kHz (not used for TX, set for register consistency)
  radio.setRxBandwidth(SX1276_RX_BW_41_7_KHZ_FSK);

  // TX output power
  radio.setPower(TX_POWER_DBM, true);

  // Sync word: 0x2D 0xD4
  uint8_t syncWord[] = {0x2D, 0xD4};
  state = radio.setSyncWord(syncWord, 2);
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("setSyncWord failed, code: "));
    Serial.println(state);
    while (true) { delay(1000); }
  }

  // Preamble length: 128 bits
  radio.setPreambleLength(FSK_PREAMBLE_BITS);

  // Packet format: variable length, CRC on (CCITT, auto-clear on error)
  radio.setPacketConfig(false, true);

  // Seed PRNG for TX interval jitter
  randomSeed(micros());

  Serial.print(F("FSK TX ready ("));
  Serial.print(TX_POWER_DBM);
  Serial.print(F(" dBm, preamble "));
  Serial.print(FSK_PREAMBLE_BITS);
  Serial.println(F(" bits, 868 MHz, 4.8 kbps)"));

  // Optional: dump key FSK registers for debugging/interop
  #define FSKTX_REG_DUMP 0
    if (FSKTX_REG_DUMP) {
      Serial.println(F("--- FSK register dump ---"));
      const uint8_t regs[] = {
        0x01,              // OP_MODE
        0x02, 0x03,        // BITRATE
        0x04, 0x05,        // FDEV
        0x06, 0x07, 0x08,  // FRF
        0x09,              // PA_CONFIG
        0x0A,              // PA_RAMP (data shaping)
        0x12,              // RX_BW
        0x25, 0x26,        // PREAMBLE_MSB/LSB (FSK)
        0x27,              // SYNC_CONFIG
        0x28, 0x29,        // SYNC_VALUE 1-2
        0x30,              // PACKET_CONFIG_1
        0x31,              // PACKET_CONFIG_2
        0x35,              // FIFO_THRESH
      };
      for (uint8_t i = 0; i < sizeof(regs); i++) {
        Serial.print(F("  [0x"));
        if (regs[i] < 0x10) Serial.print('0');
        Serial.print(regs[i], HEX);
        Serial.print(F("] = 0x"));
        uint8_t v = radio.readRegister(regs[i]);
        if (v < 0x10) Serial.print('0');
        Serial.println(v, HEX);
      }
      Serial.println(F("--- end register dump ---"));
    }

  Serial.println(F("Starting transmissions..."));
  Serial.println();
}

// ---------------------------------------------------------------------------
// loop — transmit a numbered beacon, never receive
// ---------------------------------------------------------------------------
void loop() {
  static uint32_t counter = 0;

  char message[50];
  snprintf(message, sizeof(message), "FSK TX #%lu", counter++);

  Serial.print(F("[TX] Transmitting: "));
  Serial.println(message);

  int16_t state = radio.transmit((uint8_t*)message, strlen(message));
  if (state == SX1276_ERR_NONE) {
    Serial.println(F("[TX] OK"));
  } else if (state == SX1276_ERR_PACKET_TOO_LONG) {
    Serial.println(F("[TX] Packet too long!"));
  } else if (state == SX1276_ERR_TX_TIMEOUT) {
    Serial.println(F("[TX] TX timeout!"));
  } else {
    Serial.print(F("[TX] Failed, code: "));
    Serial.println(state);
  }

  Serial.println();

  // Wait TX_INTERVAL_MS plus a small random jitter before the next packet.
  delay(TX_INTERVAL_MS + random(0, 500));
}

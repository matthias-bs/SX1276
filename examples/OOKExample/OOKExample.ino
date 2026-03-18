/*
 * OOKExample.ino
 * 
 * OOK (On-Off Keying) example for SX1276_Radio_Lite library.
 * Compatible with the RadioLib counterpart in extras/SX127x_OOK_Modem/.
 * 
 * Parameters:
 *   - OOK modulation, 868 MHz (EU 868 MHz SRD band), 4.8 kbps
 *   - Sync word: 0x12, 0xAD
 *   - Preamble: 80 bits (10 bytes) — extended to allow the OOK peak threshold
 *     detector on the receiver side to converge before the sync word arrives
 *   - Variable-length packets, CRC enabled
 * 
 * EU compliance note: 868 MHz SRD band (868.0–868.6 MHz) permits OOK at
 * max 25 mW ERP with a 1% duty cycle (ETSI EN 300 220).  This sketch
 * enforces a minimum TX interval (TX_MIN_INTERVAL_MS) to stay below 1%.
 * Parameters that affect the duty cycle: bitrate (setBitrate), preamble
 * length (setPreambleLength), payload size, and TX_MIN_INTERVAL_MS.
 * 
 * RadioLib counterpart: extras/SX127x_OOK_Modem/SX127x_OOK_Modem.ino
 * 
 * This example is configured for Adafruit Feather 32u4 RFM95
 * Pins:
 * - CS:  8
 * - RST: 4
 * - DIO0: 7
 */

#include <Arduino.h>

// Note: FSK/OOK mode is enabled by default in the library
// (No need to define FSK_OOK_ENABLED unless you've disabled it in SX1276.h)
#include <SX1276.h>

#if defined(ARDUINO_AVR_FEATHER32U4)
// Pin definitions for Adafruit Feather 32u4 RFM95
#define RADIO_CS    8
#define RADIO_RST   4
#define RADIO_DIO0  7
#elif defined(ARDUINO_TTGO_LoRa32_v21new)
#define RADIO_CS    LORA_CS
#define RADIO_RST   LORA_RST
#define RADIO_DIO0  LORA_IRQ
#else
#if defined(LORA_CS) && defined(LORA_RST) && defined(LORA_IRQ)
#define RADIO_CS    LORA_CS
#define RADIO_RST   LORA_RST
#define RADIO_DIO0  LORA_IRQ
#else
#error "Unsupported board: define RADIO_CS/RADIO_RST/RADIO_DIO0 for this target."
#endif
#endif

// Radio frequency (433 MHz is common for OOK applications in EU)
//#define RADIO_FREQ  433000000L

// Radio frequency — 868 MHz SRD band (EU)
#define RADIO_FREQ  868000000L

// Create SX1276 instance
SX1276 radio;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for Serial to be ready (or 5 seconds timeout)
  }
  
  Serial.print(F("SX1276_Radio_Lite - OOK Example ("));
  Serial.print(RADIO_FREQ / 1e6, 3);
  Serial.println(F(" MHz)"));
  Serial.println(F("Initializing..."));
  
  // Initialize the radio
  int16_t state = radio.begin(RADIO_FREQ, RADIO_CS, RADIO_RST, RADIO_DIO0);
  
  if (state == SX1276_ERR_NONE) {
    Serial.println(F("Radio initialized successfully!"));
  } else {
    Serial.print(F("Failed to initialize radio, error code: "));
    Serial.println(state);
    while (true) {
      delay(1000);
    }
  }
  
  // Set OOK modulation — RadioLib counterpart uses setOOK(true)
  state = radio.setModulation(SX1276_MODULATION_OOK);
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("Failed to set OOK modulation, error code: "));
    Serial.println(state);
    while (true) {
      delay(1000);
    }
  }
  
  // Configure OOK parameters to match extras/SX127x_OOK_Modem/SX127x_OOK_Modem.ino:
  //   beginFSK(868.0, 4.8, 5.0, 10.4) + setOOK(true) on the RadioLib side
  radio.setBitrate(4800);                              // 4.8 kbps
  int16_t fdState = radio.setFrequencyDeviation(0);    // 0 Hz deviation (OOK)
  if (fdState != SX1276_ERR_NONE) {
    Serial.print(F("Failed to set frequency deviation, error code: "));
    Serial.println(fdState);
    while (true) {
      delay(1000);
    }
  }
  radio.setRxBandwidth(SX1276_RX_BW_10_4_KHZ_FSK);   // 10.4 kHz bandwidth
  // Set transmit power: 10 dBm is safe for both 433 MHz (max 10 mW ERP) and 868 MHz (max 25 mW ERP)
  radio.setPower(10, true);                        // 10 dBm with PA_BOOST
  
  // Sync word — must match RadioLib counterpart:
  uint8_t syncWord[] = {0x12, 0xAD};
  radio.setSyncWord(syncWord, 2);
  
  // Set preamble length: 80 bits (10 bytes).
  // Doubling from the minimum (40 bits) ensures the OOK envelope peak detector
  // on the receiver side has enough on/off cycles to converge before the sync word.
  radio.setPreambleLength(80);
  
  // Set packet format (variable length, CRC enabled)
  radio.setPacketConfig(false, true);

  // Slow OOK peak threshold decay to once per 8 chips (matches SX127x_OOK_Modem).
  // The SX1276 OOK peak detector tracks the signal envelope.  At the chip default
  // (DEC_1_1_CHIP = once per chip) the threshold decays as fast as it rises, so it
  // oscillates during the "0" half of preamble pulses and the detector loses lock
  // before the sync word arrives.  Slowing the decay (DEC_1_8_CHIP) keeps the
  // threshold near the true signal peak during "0" intervals, giving the detector a
  // stable decision boundary throughout the 80-bit preamble.
  //
  // REG_OOK_AVG (0x16) bits[7:5]: OokPeakThreshDec
  //   000 = 1/1 chip  (default — too fast for stable detection)
  //   011 = 1/8 chip  (matches RadioLib's RADIOLIB_SX127X_OOK_PEAK_THRESH_DEC_1_8_CHIP)
  {
    uint8_t reg = radio.readRegister(SX1276_REG_OOK_AVG);
    radio.writeRegister(SX1276_REG_OOK_AVG, (reg & 0x1F) | 0x60);  // DEC_1_8_CHIP
  }

  // Seed the PRNG from the free-running timer so two identically-flashed boards
  // start with different random offsets and do not phase-lock their loops.
  randomSeed(micros());

  Serial.println(F("OOK configuration complete (868 MHz)"));
  Serial.println(F("Starting transmission..."));
}

void loop() {
  // Enforce 1% duty cycle (EU 868 MHz sub-band g1, ETSI EN 300 220).
  // TX_MIN_INTERVAL_MS >= airtime / 0.01.  At 4.8 kbps with 80-bit preamble,
  // a ~30-byte frame (preamble+sync+len+payload+CRC) takes ~50 ms on air;
  // 6 s interval -> 0.83% duty cycle, well within the 1% limit.
  static const uint32_t TX_MIN_INTERVAL_MS = 6000;
  static uint32_t lastTxEnd = 0;
  if (lastTxEnd != 0) {
    uint32_t elapsed = millis() - lastTxEnd;
    if (elapsed < TX_MIN_INTERVAL_MS) {
      delay(TX_MIN_INTERVAL_MS - elapsed);
    }
  }

  // Prepare message
  static uint32_t counter = 0;
  char message[50];
  snprintf(message, sizeof(message), "OOK msg #%lu", counter++);
  
  Serial.print(F("Transmitting: "));
  Serial.println(message);
  
  // Transmit the message
  int16_t state = radio.transmit((uint8_t*)message, strlen(message));
  
  if (state == SX1276_ERR_NONE) {
    Serial.println(F("Transmission successful!"));
  } else {
    Serial.print(F("Transmission failed, error code: "));
    Serial.println(state);
  }
  lastTxEnd = millis();

  // Wait before entering the receive window.  A random jitter (0–2000 ms) on
  // top of the fixed 2 s gap prevents two identically-configured boards from
  // phase-locking their TX/RX cycles and permanently missing each other.
  delay(2000 + random(0, 2000));

  // Try to receive for a short period
  Serial.println(F("Listening for packets..."));
  
  uint8_t buffer[255];
  state = radio.receive(buffer, sizeof(buffer));
  
  if (state > 0) {
    // Received a packet
    Serial.print(F("Received packet: "));
    for (int i = 0; i < state; i++) {
      Serial.write(buffer[i]);
    }
    Serial.println();
    
    // Get RSSI
    int16_t rssi = radio.getRSSI_FSK();
    Serial.print(F("RSSI: "));
    Serial.print(rssi);
    Serial.println(F(" dBm"));
  } else if (state == SX1276_ERR_RX_TIMEOUT) {
    Serial.println(F("No packet received (timeout)"));
  } else {
    Serial.print(F("Reception failed, error code: "));
    Serial.println(state);
  }
  
  Serial.println();
}

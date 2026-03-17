/*
  FSKRxExample.ino — FSK receive-only example for SX1276_Radio_Lite

  Target board : Adafruit Feather 32u4 RFM95 LoRa 868/915 MHz (AVR ATmega32U4)
  Interop peer : extras/interop_tests/SX127x_FSK_Modem/SX127x_FSK_Modem.ino
                 (RadioLib on an SX1276/SX1278 board)

  This sketch configures the on-board RFM95 (SX1276) as a pure FSK receiver
  using interrupt-driven (non-blocking) reception:
    - DIO0 is wired to pin 7; the chip asserts it HIGH when PayloadReady fires.
    - setPacketReceivedAction() attaches an ISR to the rising edge of DIO0.
    - startReceive() places the radio in RX_CONTINUOUS and returns immediately.
    - The main loop polls a volatile flag set by the ISR; when set it calls
      readData() to pull the packet from the FIFO, then re-arms startReceive().
  No transmissions are ever made.

  FSK parameters (must be identical on both sides):
    Frequency     : 868 MHz
    Bit rate      : 4.8 kbps
    Freq deviation: 5 kHz
    RX bandwidth  : 41.7 kHz  (robust against ±10 ppm crystal offset)
    AFC bandwidth : 41.7 kHz  (aligned with RX BW)
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
#pragma message "FSKRxExample: using Adafruit Feather 32u4 RFM95 pins (CS=8, RST=4, DIO0=7)"
#else
#error "FSKRxExample is targeted at the Adafruit Feather 32u4 RFM95. " \
       "Define RADIO_CS / RADIO_RST / RADIO_DIO0 for other boards and remove this error."
#endif

// ---------------------------------------------------------------------------
// Constants — must match SX127x_FSK_Modem.ino
// ---------------------------------------------------------------------------
#define RADIO_FREQ        868000000L  // 868 MHz
static const uint16_t FSK_PREAMBLE_BITS = 128;

// Maximum FSK payload size: SX127x FIFO is 64 bytes (datasheet, page 66).
static const size_t RX_BUF_LEN = 64;

// Heartbeat interval: print a status line if no packet arrives within this
// many milliseconds, so the user knows the receiver is still alive.
static const uint32_t HEARTBEAT_MS = 5000;

// ---------------------------------------------------------------------------
// Radio object and ISR state
// ---------------------------------------------------------------------------
SX1276 radio;

// Set by the DIO0 ISR; read and cleared in loop().
volatile bool packetReceived = false;

// ISR — called on DIO0 rising edge (PayloadReady).
// Keep it minimal: just set the flag and return.
void onPacketReceived() {
    packetReceived = true;
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait up to 5 s for USB-CDC on 32u4
  }

  Serial.println(F("SX1276_Radio_Lite - FSK RX Example"));
  Serial.println(F("Peer: SX127x_FSK_Modem (RadioLib)"));
  Serial.println(F("Initializing radio..."));

  // Initialize the radio (FSK/OOK mode is enabled by default in the library)
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

  // RX bandwidth: 41.7 kHz
  // Sizing (Semtech AN1200.29): RxBW >= BR/2 + FreqDev + freq_error
  //   Minimum: 4.8/2 + 5.0 = 7.4 kHz
  //   41.7 kHz provides 34.3 kHz margin for crystal offset between boards,
  //   making interop reliable even with ±10 ppm oscillators at 868 MHz.
  radio.setRxBandwidth(SX1276_RX_BW_41_7_KHZ_FSK);

  // Align AFC bandwidth with RX bandwidth for stable frequency tracking.
  // The library has no public setter for AFC BW; write the register directly.
  {
    uint8_t afc = radio.readRegister(SX1276_REG_AFC_BW);
    radio.writeRegister(SX1276_REG_AFC_BW,
                        (afc & 0xE0) | (SX1276_RX_BW_41_7_KHZ_FSK & 0x1F));
  }

  // Sync word: 0x2D 0xD4  (RadioLib default; SX1276_Radio_Lite library default)
  uint8_t syncWord[] = {0x2D, 0xD4};
  state = radio.setSyncWord(syncWord, 2);
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("setSyncWord failed, code: "));
    Serial.println(state);
    while (true) { delay(1000); }
  }

  // Preamble length: 128 bits (matching SX127x_FSK_Modem.ino)
  radio.setPreambleLength(FSK_PREAMBLE_BITS);

  // Packet format: variable length, CRC on (CCITT, auto-clear on error)
  radio.setPacketConfig(false, true);

  Serial.print(F("FSK RX ready (preamble "));
  Serial.print(FSK_PREAMBLE_BITS);
  Serial.println(F(" bits, 868 MHz, 4.8 kbps, 41.7 kHz BW)"));

  // Attach ISR to DIO0 (pin 7) rising edge, then enter RX_CONTINUOUS.
  // The radio will assert DIO0 HIGH when PayloadReady fires.
  radio.setPacketReceivedAction(onPacketReceived);
  int16_t rxState = radio.startReceive();
  if (rxState != SX1276_ERR_NONE) {
    Serial.print(F("startReceive failed, code: "));
    Serial.println(rxState);
    while (true) { delay(1000); }
  }

  Serial.println(F("Listening (IRQ mode)... waiting for packets from SX127x_FSK_Modem."));
  Serial.println();
}


// ---------------------------------------------------------------------------
// loop — interrupt-driven receive, never transmit
// ---------------------------------------------------------------------------
void loop() {
  // --- Packet received via DIO0 interrupt ---
  if (packetReceived) {
    packetReceived = false;  // Clear flag before readData() so a back-to-back
                             // packet arriving during processing is not lost.

    uint8_t buffer[RX_BUF_LEN];
    int16_t rxLen = radio.readData(buffer, sizeof(buffer));

    if (rxLen > 0) {
      Serial.print(F("[RX] Received ("));
      Serial.print(rxLen);
      Serial.print(F(" bytes): "));
      for (int16_t i = 0; i < rxLen; i++) {
        Serial.write(isprint(buffer[i]) ? buffer[i] : '.');
      }
      Serial.println();

      int16_t rssi = radio.getRSSI_FSK();
      Serial.print(F("[RX] RSSI: "));
      Serial.print(rssi);
      Serial.println(F(" dBm"));
      Serial.println();

    } else if (rxLen == SX1276_ERR_CRC_MISMATCH) {
      Serial.println(F("[RX] CRC error — packet discarded"));
      Serial.println();
    } else {
      Serial.print(F("[RX] readData failed, code: "));
      Serial.println(rxLen);
      Serial.println();
    }

    // Re-arm receiver for the next packet.
    radio.startReceive();
  }

  // --- Periodic heartbeat so the user knows the sketch is alive ---
  static uint32_t lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = millis();
    Serial.println(F("[RX] Listening..."));
  }
}

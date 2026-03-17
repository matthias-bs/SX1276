/*
 * SX127x_FSK_Modem.ino — RadioLib interoperability test for SX1276_Radio_Lite
 *
 * Interop partner: examples/FSKExample/FSKExample.ino (SX1276_Radio_Lite library)
 *
 * Both sides use identical FSK parameters for reliable bidirectional communication.
 *
 * Configuration (must match FSKExample.ino):
 *   Frequency     : 868 MHz
 *   Bit rate      : 4.8 kbps
 *   Freq deviation: 5 kHz
 *   RX bandwidth  : 41.7 kHz (crystal tolerance)
 *   Sync word     : 0x2D 0xD4
 *   Preamble      : 128 bits (16 bytes)
 *   Packet format : variable length, CRC on (CCITT, autoclear on)
 *   DC-free       : none
 *
 * Loop logic:
 *   1. Listen for a packet from the FSKExample board (8 s timeout).
 *   2. If received, print payload and RSSI, then transmit a reply.
 *   3. If nothing received within the timeout, transmit a beacon anyway.
 *   4. Repeat.
 */

// include the library
#include <RadioLib.h>

// Board-specific pin definitions
#if defined(ARDUINO_DFROBOT_FIREBEETLE_ESP32)
// https://wiki.dfrobot.com/FireBeetle_ESP32_IOT_Microcontroller(V3.0)__Supports_Wi-Fi_&_Bluetooth__SKU__DFR0478
// https://wiki.dfrobot.com/FireBeetle_Covers_LoRa_Radio_868MHz_SKU_TEL0125
#define RADIO_CS      27 // D4
#define RADIO_RST     25 // D2
#define RADIO_DIO0    26 // D3
#define RADIO_DIO1     9 // D5
#elif defined(ARDUINO_TTGO_LoRa32_v21new)
// ESP32-based boards (Lilygo T3 LoRa32, TTGO LoRa32, etc.)
// The board package defines LORA_CS / LORA_RST / LORA_IRQ.
#define RADIO_CS    LORA_CS
#define RADIO_RST   LORA_RST
#define RADIO_DIO0  LORA_IRQ
#define RADIO_DIO1  LORA_D1
#else
#if !defined(LORA_CS) || !defined(LORA_RST) || !defined(LORA_IRQ)
#error "Unsupported board: define RADIO_CS/RADIO_RST/RADIO_DIO0 for this target."
#endif
#endif


Module mod(RADIO_CS, RADIO_DIO0, RADIO_RST, RADIO_DIO1);
SX1276 radio(&mod);

// Low-power preset for close-range interoperability testing.
static const int8_t TX_POWER_DBM = 5;
static const uint16_t FSK_PREAMBLE_BITS = 128;

// or detect the pinout automatically using RadioBoards
// https://github.com/radiolib-org/RadioBoards
/*
#define RADIO_BOARD_AUTO
#include <RadioBoards.h>
Radio radio = new RadioModule();
*/

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) { ; }

  // Initialize SX1276 FSK modem — parameters chosen to match FSKExample.ino:
  //   freq=868 MHz, bitrate=4.8 kbps, freqDev=5 kHz, rxBw=41.7 kHz,
  //   pwr=TX_POWER_DBM, preamble=FSK_PREAMBLE_BITS
  //   RX bandwidth 41.7 kHz ensures robust interop with crystal tolerance.
  Serial.print(F("[SX1276] Initializing FSK modem ... "));
  int state = radio.beginFSK(868.0F, 4.8, 5.0, 41.7, TX_POWER_DBM, FSK_PREAMBLE_BITS);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // Set sync word to match FSKExample.ino
  uint8_t syncWord[] = {0x2D, 0xD4};
  state = radio.setSyncWord(syncWord, 2);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("Unable to set sync word, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // Preamble polarity: RadioLib and SX1276_Radio_Lite both use 0x55 (invertPreamble(false)).
  state = radio.invertPreamble(false);  // false = 0x55, matching SX1276_Radio_Lite
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("Unable to set preamble polarity, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // Align AFC bandwidth with RX bandwidth for interop.
  state = radio.setAFCBandwidth(41.7);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("Unable to set AFC bandwidth, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // Enable AFC auto and set trigger to PreambleDetect for robust sync detection.
  mod.SPIwriteRegister(0x0D, 0x08 | 0x10 | 0x06);

  // Force LowFrequencyModeOn=0 at 868 MHz (HF path).
  {
    const uint8_t opModeReg = 0x01;
    uint8_t op = mod.SPIreadRegister(opModeReg);
    op &= (uint8_t)~0x08;  // clear bit 3: LowFrequencyModeOn
    mod.SPIwriteRegister(opModeReg, op);
  }

  // Seed PRNG to decorrelate peer timing.
  randomSeed(micros());

  Serial.print(F("FSK configuration complete (TX power "));
  Serial.print(TX_POWER_DBM);
  Serial.print(F(" dBm, preamble "));
  Serial.print(FSK_PREAMBLE_BITS);
  Serial.println(F(" bits)."));

  // Optional: dump key FSK registers for debugging/interop
#define FSKMODEM_REG_DUMP 0
  if (FSKMODEM_REG_DUMP) {
    Serial.println(F("--- FSK register dump ---"));
    const uint8_t regs[] = {
      0x01,              // OP_MODE
      0x02, 0x03,        // BITRATE
      0x04, 0x05,        // FDEV
      0x06, 0x07, 0x08,  // FRF
      0x09,              // PA_CONFIG
      0x0A,              // PA_RAMP (data shaping)
      0x0D,              // RX_CONFIG
      0x0E,              // RSSI_CONFIG
      0x10,              // RSSI_THRESH
      0x12,              // RX_BW
      0x13,              // AFC_BW
      0x1F,              // PREAMBLE_DETECT
      0x25, 0x26,        // PREAMBLE_MSB/LSB (FSK)
      0x27,              // SYNC_CONFIG
      0x28, 0x29,        // SYNC_VALUE 1-2
      0x30,              // PACKET_CONFIG_1
      0x31,              // PACKET_CONFIG_2
      0x32,              // PAYLOAD_LENGTH
      0x35,              // FIFO_THRESH
      0x40,              // DIO_MAPPING_1
    };
    for (uint8_t i = 0; i < sizeof(regs); i++) {
      Serial.print(F("  [0x"));
      if (regs[i] < 0x10) Serial.print('0');
      Serial.print(regs[i], HEX);
      Serial.print(F("] = 0x"));
      uint8_t v = mod.SPIreadRegister(regs[i]);
      if (v < 0x10) Serial.print('0');
      Serial.println(v, HEX);
    }
    Serial.println(F("--- end register dump ---"));
  }

  Serial.println(F("Starting interop loop..."));
}

// Maximum FSK payload size: SX127x FIFO is 64 bytes (see datasheet, page 66).
// FSKExample sends messages up to 50 bytes, so this buffer is sufficient.
static const size_t RX_BUF_LEN = 64;
static uint32_t txCounter = 0;

void loop() {
  // Listen for a packet (8 s timeout) — ensures overlap with peer TX interval.
  Serial.println(F("[SX1276] Listening for packet..."));
  byte rxBuf[RX_BUF_LEN];
  memset(rxBuf, 0, sizeof(rxBuf));
  int rxState = radio.receive(rxBuf, RX_BUF_LEN, /* timeout ms */ 8000);

  bool shouldTransmit = true;
  if (rxState == RADIOLIB_ERR_NONE) {
    // Use RadioLib's reported payload length for clean output.
    size_t rxLen = radio.getPacketLength(false);
    if (rxLen > RX_BUF_LEN) {
      rxLen = RX_BUF_LEN;
    }

    Serial.print(F("[SX1276] Received ("));
    Serial.print(rxLen);
    Serial.print(F(" bytes): "));
    for (size_t i = 0; i < rxLen; i++) {
      Serial.write(rxBuf[i]);
    }
    Serial.println();

    // RSSI is measured during reception.
    Serial.print(F("[SX1276] RSSI: "));
    Serial.print(radio.getRSSI());
    Serial.println(F(" dBm"));

    // Short delay to avoid turnaround races.
    delay(80 + random(0, 120));

  } else if (rxState == RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.println(F("[SX1276] No packet received (timeout)"));

    // Always send a beacon after timeout to maximize overlap probability.
    shouldTransmit = true;
  } else {
    Serial.print(F("[SX1276] Receive failed, code "));
    Serial.println(rxState);
    // For unknown RX errors, skip TX and retry reception quickly.
    shouldTransmit = false;
  }

  // Transmit a reply or beacon.
  if (shouldTransmit) {
    char txMsg[50];
    snprintf(txMsg, sizeof(txMsg), "RadioLib reply #%lu", txCounter++);
    Serial.print(F("[SX1276] Transmitting: "));
    Serial.println(txMsg);

    int txState = radio.transmit((uint8_t*)txMsg, strlen(txMsg));
    if (txState == RADIOLIB_ERR_NONE) {
      Serial.println(F("[SX1276] Transmission successful!"));
    } else if (txState == RADIOLIB_ERR_PACKET_TOO_LONG) {
      Serial.println(F("[SX1276] Packet too long!"));
    } else if (txState == RADIOLIB_ERR_TX_TIMEOUT) {
      Serial.println(F("[SX1276] TX timeout!"));
    } else {
      Serial.print(F("[SX1276] TX failed, code "));
      Serial.println(txState);
    }
  }

  // Keep non-RX gap short so this node is listening most of the time.
  delay(80 + random(0, 220));
  Serial.println();
}

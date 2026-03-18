/*
 * BasicExample.ino
 * 
 * Basic example for SX1276_Radio_Lite library
 * Demonstrates LoRa transmission and reception (bidirectional)
 * 
 * Interop partners:
 *   - extras/interop_tests/SX127x_Receive_Interrupt/SX127x_Receive_Interrupt.ino (RadioLib, RX)
 *   - extras/interop_tests/SX127x_Transmit_Interrupt/SX127x_Transmit_Interrupt.ino (RadioLib, TX)
 *
 * Configuration (must match RadioLib interop partners):
 *   Frequency     : 868 MHz
 *   Spreading fac.: SF7
 *   Bandwidth     : 125 kHz
 *   Coding rate   : 4/5
 *   Sync word     : 0x12
 *   CRC           : on
 *   Preamble      : 8 symbols
 */

#include <Arduino.h>

// Note: LoRa mode is enabled by default in the library
// (No need to define LORA_ENABLED unless you've disabled it in SX1276.h)
#include <SX1276.h>

// Board-specific pin definitions
#if defined(ARDUINO_AVR_FEATHER32U4)
// Adafruit Feather 32u4 RFM95 LoRa 868/915 MHz
#define RADIO_CS    8
#define RADIO_RST   4
#define RADIO_DIO0  7
#elif defined(ARDUINO_DFROBOT_FIREBEETLE_ESP32)
#define RADIO_CS      27 // D4
#define RADIO_RST     25 // D2
#define RADIO_DIO0    26 // D3
#elif defined(ARDUINO_TTGO_LoRa32_v21new)
// ESP32-based boards (Lilygo T3 LoRa32, TTGO LoRa32, etc.)
#define RADIO_CS    LORA_CS
#define RADIO_RST   LORA_RST
#define RADIO_DIO0  LORA_IRQ
#else
#if !defined(LORA_CS) || !defined(LORA_RST) || !defined(LORA_IRQ)
#error "Unsupported board: define RADIO_CS/RADIO_RST/RADIO_DIO0 for this target."
#endif
#endif

// LoRa frequency — 868 MHz SRD band (EU)
#define LORA_FREQ  868000000L

// Create SX1276 instance
SX1276 radio;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for Serial to be ready (or 5 seconds timeout)
  }
  
  Serial.println(F("SX1276_Radio_Lite - Basic Example"));
  Serial.println(F("Initializing..."));
  
  // Initialize the radio
  int16_t state = radio.begin(LORA_FREQ, RADIO_CS, RADIO_RST, RADIO_DIO0);
  
  if (state == SX1276_ERR_NONE) {
    Serial.println(F("Radio initialized successfully!"));
  } else {
    Serial.print(F("Failed to initialize radio, error code: "));
    Serial.println(state);
    while (true) {
      delay(1000);
    }
  }
  
  // Configure LoRa parameters (optional, these are the defaults)
  radio.setSpreadingFactor(SX1276_SF_7);      // SF7 - fastest
  radio.setBandwidth(SX1276_BW_125_KHZ);      // 125 kHz
  radio.setCodingRate(SX1276_CR_4_5);         // CR 4/5
  radio.setPower(17, true);                   // 17 dBm with PA_BOOST
  radio.setPreambleLength(8);                 // 8 symbols
  radio.setSyncWord(0x12);                    // Private network
  radio.setCRC(true);                         // Enable CRC
  
  Serial.println(F("Configuration complete"));
  Serial.println(F("Starting transmission..."));
}

void loop() {
  // Prepare message
  static uint32_t counter = 0;
  char message[50];
  snprintf(message, sizeof(message), "Hello LoRa! #%lu", counter++);
  
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
  
  // Wait a bit before next transmission
  delay(5000);
  
  // Optionally, try to receive for a short period
  Serial.println(F("Listening for packets..."));
  
  uint8_t buffer[255];
  // Note: receive is blocking with a 10 second timeout
  // For non-blocking operation, you would check DIO0 pin instead
  state = radio.receive(buffer, sizeof(buffer));
  
  if (state > 0) {
    // Received a packet
    Serial.print(F("Received packet: "));
    for (int i = 0; i < state; i++) {
      Serial.write(buffer[i]);
    }
    Serial.println();
    
    // Get RSSI and SNR
    int16_t rssi = radio.getRSSI();
    int8_t snr = radio.getSNR();
    
    Serial.print(F("RSSI: "));
    Serial.print(rssi);
    Serial.print(F(" dBm, SNR: "));
    Serial.print(snr / 4.0);
    Serial.println(F(" dB"));
  } else if (state == SX1276_ERR_RX_TIMEOUT) {
    Serial.println(F("No packet received (timeout)"));
  } else {
    Serial.print(F("Reception failed, error code: "));
    Serial.println(state);
  }
  
  Serial.println();
}

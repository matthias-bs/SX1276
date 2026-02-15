/*
 * OOKExample.ino
 * 
 * OOK (On-Off Keying) example for SX1276 library
 * Demonstrates OOK transmission and reception
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

// Pin definitions for Adafruit Feather 32u4 RFM95
#define RADIO_CS    8
#define RADIO_RST   4
#define RADIO_DIO0  7

// Radio frequency (433 MHz is common for OOK applications)
#define RADIO_FREQ  433000000L

// Create SX1276 instance
SX1276 radio;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for Serial to be ready (or 5 seconds timeout)
  }
  
  Serial.println(F("SX1276 OOK Example"));
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
  
  // Set modulation to OOK
  state = radio.setModulation(SX1276_MODULATION_OOK);
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("Failed to set OOK modulation, error code: "));
    Serial.println(state);
    while (true) {
      delay(1000);
    }
  }
  
  // Configure OOK parameters
  radio.setBitrate(4800);                          // 4.8 kbps
  int16_t fdState = radio.setFrequencyDeviation(0); // 0 Hz deviation (OOK)
  if (fdState != SX1276_ERR_NONE) {
    Serial.print(F("Failed to set frequency deviation, error code: "));
    Serial.println(fdState);
    while (true) {
      delay(1000);
    }
  }
  radio.setRxBandwidth(SX1276_RX_BW_10_4_KHZ_FSK); // 10.4 kHz bandwidth
  radio.setPower(17, true);                        // 17 dBm with PA_BOOST
  
  // Set sync word (2 bytes)
  uint8_t syncWord[] = {0x69, 0x81};
  radio.setSyncWord(syncWord, 2);
  
  // Set preamble length to 2 bytes (16 bits)
  radio.setPreambleLength(2);
  
  // Set packet format (variable length, CRC enabled)
  radio.setPacketConfig(false, true);
  
  Serial.println(F("OOK configuration complete"));
  Serial.println(F("Starting transmission..."));
}

void loop() {
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
  
  // Wait a bit before next transmission
  delay(3000);
  
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

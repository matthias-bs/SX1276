/*
 * BasicExample.ino
 * 
 * Basic example for SX1276_Radio_Lite library
 * Demonstrates LoRa transmission and reception
 * 
 * This example is configured for Adafruit Feather 32u4 RFM95
 * Pins:
 * - CS:  8
 * - RST: 4
 * - DIO0: 7
 */

#include <Arduino.h>

// Note: LoRa mode is enabled by default in the library
// (No need to define LORA_ENABLED unless you've disabled it in SX1276.h)
#include "SX1276.h"

// Pin definitions for Adafruit Feather 32u4 RFM95
#define LORA_CS    8
#define LORA_RST   4
#define LORA_DIO0  7

// LoRa frequency (915 MHz for US, 868 MHz for EU)
#define LORA_FREQ  915000000L

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
  int16_t state = radio.begin(LORA_FREQ, LORA_CS, LORA_RST, LORA_DIO0);
  
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

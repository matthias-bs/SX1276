/*
 * TransmitExample.ino
 * 
 * Transmit-only example for SX1276_Radio_Lite library
 * Periodically sends LoRa packets
 * 
 * This example is configured for Adafruit Feather 32u4 RFM95
 * Pins:
 * - CS:  8
 * - RST: 4
 * - DIO0: 7
 */

#include <Arduino.h>

// Enable LoRa mode
#define LORA_ENABLED
#include <SX1276.h>

// Pin definitions for Adafruit Feather 32u4 RFM95
#define LORA_CS    8
#define LORA_RST   4
#define LORA_DIO0  7

// LoRa frequency (915 MHz for US, 868 MHz for EU)
#define LORA_FREQ  915000000L

// Transmission interval in milliseconds
#define TX_INTERVAL 5000

// Create SX1276 instance
SX1276 radio;

// Packet counter
uint32_t packetCounter = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for Serial to be ready (or 5 seconds timeout)
  }
  
  Serial.println(F("SX1276_Radio_Lite - Transmit Example"));
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
  
  // Configure LoRa parameters
  radio.setSpreadingFactor(SX1276_SF_7);      // SF7 - fastest, shortest range
  radio.setBandwidth(SX1276_BW_125_KHZ);      // 125 kHz bandwidth
  radio.setCodingRate(SX1276_CR_4_5);         // CR 4/5 - least overhead
  radio.setPower(17, true);                   // 17 dBm with PA_BOOST
  radio.setPreambleLength(8);                 // 8 symbol preamble
  radio.setSyncWord(0x12);                    // Private network sync word
  radio.setCRC(true);                         // Enable CRC checking
  
  Serial.println(F("Configuration complete"));
  Serial.print(F("Frequency: "));
  Serial.print(LORA_FREQ / 1000000.0, 1);
  Serial.println(F(" MHz"));
  Serial.print(F("TX Power: 17 dBm"));
  Serial.println();
  Serial.println(F("Starting transmission..."));
  Serial.println();
}

void loop() {
  // Prepare message with packet counter
  char message[100];
  snprintf(message, sizeof(message), "Hello LoRa! Packet #%lu", packetCounter);
  
  Serial.print(F("["));
  Serial.print(millis() / 1000);
  Serial.print(F("s] Transmitting packet #"));
  Serial.print(packetCounter);
  Serial.print(F(": \""));
  Serial.print(message);
  Serial.print(F("\" ("));
  Serial.print(strlen(message));
  Serial.print(F(" bytes)... "));
  
  // Transmit the message
  uint32_t txStart = millis();
  int16_t state = radio.transmit((uint8_t*)message, strlen(message));
  uint32_t txTime = millis() - txStart;
  
  if (state == SX1276_ERR_NONE) {
    Serial.print(F("OK ("));
    Serial.print(txTime);
    Serial.println(F(" ms)"));
    packetCounter++;
  } else {
    Serial.print(F("FAILED (error "));
    Serial.print(state);
    Serial.println(F(")"));
  }
  
  // Wait before next transmission
  delay(TX_INTERVAL);
}

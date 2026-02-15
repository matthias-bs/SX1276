/*
 * ReceiveExample.ino
 * 
 * Receive-only example for SX1276 library
 * Continuously listens for LoRa packets and displays them
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

// Create SX1276 instance
SX1276 radio;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for Serial to be ready (or 5 seconds timeout)
  }
  
  Serial.println(F("SX1276 Receive Example"));
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
  
  // Configure LoRa parameters to match transmitter
  radio.setSpreadingFactor(SX1276_SF_7);      // SF7 - fastest
  radio.setBandwidth(SX1276_BW_125_KHZ);      // 125 kHz
  radio.setCodingRate(SX1276_CR_4_5);         // CR 4/5
  radio.setPreambleLength(8);                 // 8 symbols
  radio.setSyncWord(0x12);                    // Private network
  radio.setCRC(true);                         // Enable CRC
  
  Serial.println(F("Configuration complete"));
  Serial.println(F("Listening for packets..."));
  Serial.println();
}

void loop() {
  uint8_t buffer[255];
  
  // Receive with 10 second timeout
  int16_t state = radio.receive(buffer, sizeof(buffer));
  
  if (state > 0) {
    // Successfully received a packet
    Serial.println(F("========================================"));
    Serial.print(F("Received packet ("));
    Serial.print(state);
    Serial.println(F(" bytes):"));
    Serial.print(F("Data: "));
    
    // Print as string (if printable)
    bool isPrintable = true;
    for (int i = 0; i < state; i++) {
      if (buffer[i] < 32 || buffer[i] > 126) {
        isPrintable = false;
        break;
      }
    }
    
    if (isPrintable) {
      for (int i = 0; i < state; i++) {
        Serial.write(buffer[i]);
      }
      Serial.println();
    } else {
      // Print as hex
      Serial.println();
      Serial.print(F("Hex: "));
      for (int i = 0; i < state; i++) {
        if (buffer[i] < 16) Serial.print('0');
        Serial.print(buffer[i], HEX);
        Serial.print(' ');
      }
      Serial.println();
    }
    
    // Get and display signal quality metrics
    int16_t rssi = radio.getRSSI();
    int8_t snr = radio.getSNR();
    int32_t freqError = radio.getFrequencyError();
    
    Serial.print(F("RSSI: "));
    Serial.print(rssi);
    Serial.println(F(" dBm"));
    
    Serial.print(F("SNR: "));
    // SNR is in 1/4 dB units, so divide by 4
    Serial.print(snr / 4.0, 2);
    Serial.println(F(" dB"));
    
    Serial.print(F("Frequency Error: "));
    Serial.print(freqError);
    Serial.println(F(" Hz"));
    
    Serial.println(F("========================================"));
    Serial.println();
    
  } else if (state == SX1276_ERR_RX_TIMEOUT) {
    // No packet received within timeout
    Serial.print('.');
    
  } else if (state == SX1276_ERR_CRC_MISMATCH) {
    // CRC error
    Serial.println();
    Serial.println(F("CRC error - corrupted packet"));
    
  } else {
    // Other error
    Serial.println();
    Serial.print(F("Reception error: "));
    Serial.println(state);
  }
}

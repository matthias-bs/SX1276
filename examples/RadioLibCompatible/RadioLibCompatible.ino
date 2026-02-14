/*
 * RadioLibCompatible.ino
 * 
 * Example demonstrating RadioLib-compatible API usage
 * This shows how to migrate from RadioLib with minimal code changes
 * 
 * RadioLib equivalent:
 * Module mod(8, 7, 4);
 * SX1276 radio(&mod);
 * radio.begin(915.0, 125.0, 9, 7, 0x12, 10, 8, 0);
 */

#include <Arduino.h>

// Enable LoRa mode
#define LORA_ENABLED
#include <SX1276.h>

// Pin definitions for Adafruit Feather 32u4 RFM95
// These match RadioLib's Module(cs, irq, rst) parameters
#define LORA_CS    8   // Chip select
#define LORA_IRQ   7   // DIO0 interrupt
#define LORA_RST   4   // Reset

// Create SX1276 instance with RadioLib-compatible constructor
SX1276 radio(LORA_CS, LORA_IRQ, LORA_RST);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for Serial to be ready (or 5 seconds timeout)
  }
  
  Serial.println(F("SX1276 RadioLib-Compatible Example"));
  Serial.println(F("Initializing with RadioLib-style API..."));
  
  // RadioLib-compatible begin() with defaults
  // begin(freq, bw, sf, cr, syncWord, power, preambleLength, gain)
  // All parameters after freq are optional with sensible defaults
  int16_t state = radio.begin(915.0);  // Just frequency in MHz
  
  // Or with full parameters like RadioLib:
  // int16_t state = radio.begin(915.0, 125.0, 9, 7, 0x12, 10, 8, 0);
  // Parameters:
  //   freq = 915.0 MHz
  //   bw = 125.0 kHz bandwidth
  //   sf = 9 spreading factor
  //   cr = 7 coding rate (4/7)
  //   syncWord = 0x12 (private network)
  //   power = 10 dBm
  //   preambleLength = 8 symbols
  //   gain = 0 (auto gain control)
  
  if (state == SX1276_ERR_NONE) {
    Serial.println(F("Radio initialized successfully!"));
    Serial.println(F("Configuration:"));
    Serial.println(F("  Frequency: 915.0 MHz"));
    Serial.println(F("  Bandwidth: 125.0 kHz"));
    Serial.println(F("  Spreading Factor: 9"));
    Serial.println(F("  Coding Rate: 4/7"));
    Serial.println(F("  Power: 10 dBm"));
  } else {
    Serial.print(F("Failed to initialize radio, error code: "));
    Serial.println(state);
    while (true) {
      delay(1000);
    }
  }
  
  // You can also change frequency using MHz (RadioLib-compatible)
  // radio.setFrequency(868.0);  // Change to 868 MHz
  
  Serial.println(F("Starting transmission..."));
}

void loop() {
  // Prepare message
  static uint32_t counter = 0;
  char message[50];
  snprintf(message, sizeof(message), "RadioLib-style #%lu", counter++);
  
  Serial.print(F("Transmitting: "));
  Serial.println(message);
  
  // Transmit the message
  // Note: RadioLib uses String, we use byte array for efficiency
  int16_t state = radio.transmit((uint8_t*)message, strlen(message));
  
  if (state == SX1276_ERR_NONE) {
    Serial.println(F("Transmission successful!"));
  } else {
    Serial.print(F("Transmission failed, error code: "));
    Serial.println(state);
  }
  
  delay(5000);
  
  // Optional: Try to receive
  Serial.println(F("Listening for packets..."));
  
  uint8_t buffer[255];
  state = radio.receive(buffer, sizeof(buffer));
  
  if (state > 0) {
    // Received a packet
    Serial.print(F("Received: "));
    for (int i = 0; i < state; i++) {
      Serial.write(buffer[i]);
    }
    Serial.println();
    
    // Get signal quality (RadioLib-compatible methods)
    int16_t rssi = radio.getRSSI();
    int8_t snr = radio.getSNR();
    
    Serial.print(F("RSSI: "));
    Serial.print(rssi);
    Serial.print(F(" dBm, SNR: "));
    Serial.print(snr / 4.0);
    Serial.println(F(" dB"));
  } else if (state == SX1276_ERR_RX_TIMEOUT) {
    Serial.println(F("No packet received"));
  }
  
  Serial.println();
}

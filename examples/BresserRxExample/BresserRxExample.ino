/*
 * BresserRxExample.ino
 * 
 * Receive-only example for SX1276 library configured for Bresser weather sensors
 * Listens for FSK packets from Bresser weather station sensors and displays them
 * 
 * Radio Configuration:
 * - Carrier frequency:      868.3 MHz
 * - Bit rate:               8.22 kbps
 * - Frequency deviation:    57.136417 kHz
 * - Rx bandwidth:           250 kHz
 * - Output power:           10 dBm
 * - Preamble length:        40 bits (5 bytes)
 * - Packet mode:            Fixed length, 27 bytes
 * - CRC filtering:          Disabled
 * - Preamble:               0xAA, 0xAA, 0xAA, 0xAA, 0xAA
 * - Sync word:              0xAA, 0x2D
 * 
 * This example is configured for Adafruit Feather 32u4 RFM95
 * Pins:
 * - CS:  8
 * - RST: 4
 * - DIO0: 7
 */

#include <Arduino.h>

// Note: FSK/OOK mode is enabled by default in the library
#include <SX1276.h>

// Pin definitions for Adafruit Feather 32u4 RFM95
#define RADIO_CS    8
#define RADIO_RST   4
#define RADIO_DIO0  7

// Radio frequency (868.3 MHz for Bresser sensors)
#define RADIO_FREQ  868300000L

// Fixed packet length for Bresser sensors
#define PACKET_LENGTH 27

// Create SX1276 instance
SX1276 radio;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for Serial to be ready (or 5 seconds timeout)
  }
  
  Serial.println(F("Bresser Weather Sensor Receiver"));
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
  
  // Set modulation to FSK
  state = radio.setModulation(SX1276_MODULATION_FSK);
  if (state != SX1276_ERR_NONE) {
    Serial.print(F("Failed to set FSK modulation, error code: "));
    Serial.println(state);
    while (true) {
      delay(1000);
    }
  }
  
  // Configure FSK parameters for Bresser sensors
  radio.setBitrate(8220);                          // 8.22 kbps
  radio.setFrequencyDeviation(57136);              // 57.136 kHz (closest to 57.136417 kHz)
  radio.setRxBandwidth(SX1276_RX_BW_250_0_KHZ_FSK); // 250 kHz bandwidth
  radio.writeRegister(SX1276_REG_AFC_BW, SX1276_RX_BW_250_0_KHZ_FSK); // keep AFC BW in sync with RX BW
  radio.setPower(10, true);                        // 10 dBm with PA_BOOST
  
  // Set sync word (2 bytes: 0xAA, 0x2D)
  uint8_t syncWord[] = {0xAA, 0x2D};
  radio.setSyncWord(syncWord, 2);
  
  // Set preamble length (5 bytes = 40 bits)
  radio.setPreambleLength(5);
  
  // Set packet format (fixed length, CRC disabled)
  radio.setPacketConfig(true, false);
  
  // Set fixed payload length to 27 bytes
  radio.writeRegister(SX1276_REG_PAYLOAD_LENGTH_FSK, PACKET_LENGTH);
  
  Serial.println(F("FSK configuration complete"));
  Serial.println(F("Radio Parameters:"));
  Serial.println(F("  Frequency:        868.3 MHz"));
  Serial.println(F("  Bit rate:         8.22 kbps"));
  Serial.println(F("  Freq deviation:   57.136 kHz"));
  Serial.println(F("  Rx bandwidth:     250 kHz"));
  Serial.println(F("  Packet length:    27 bytes (fixed)"));
  Serial.println(F("  Sync word:        0xAA 0x2D"));
  Serial.println();
  Serial.println(F("Listening for Bresser sensor packets..."));
  Serial.println(F("(Displaying only frames starting with 0xD4)"));
  Serial.println();
}

void loop() {
  uint8_t buffer[PACKET_LENGTH];
  
  // Receive packet (blocks with 10 second timeout)
  int16_t state = radio.receive(buffer, sizeof(buffer));
  
  if (state > 0) {
    // Successfully received a packet
    
    // Only display frames starting with 0xD4
    if (buffer[0] == 0xD4) {
      Serial.println(F("========================================"));
      Serial.print(F("Received Bresser packet ("));
      Serial.print(state);
      Serial.println(F(" bytes):"));
      
      // Print packet data in hex format
      Serial.print(F("Data: "));
      for (int i = 0; i < state; i++) {
        if (buffer[i] < 16) Serial.print('0');
        Serial.print(buffer[i], HEX);
        Serial.print(' ');
      }
      Serial.println();
      
      // Get and display signal quality metrics
      int16_t rssi = radio.getRSSI_FSK();
      
      Serial.print(F("RSSI: "));
      Serial.print(rssi);
      Serial.println(F(" dBm"));
      
      Serial.println(F("========================================"));
      Serial.println();
    }
    
  } else if (state == SX1276_ERR_RX_TIMEOUT) {
    // No packet received within timeout - print dot for heartbeat
    Serial.print('.');
    
  } else {
    // Other error
    Serial.println();
    Serial.print(F("Reception error: "));
    Serial.println(state);
  }
}

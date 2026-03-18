/*
 * RadioLibCompatible.ino
 * 
 * Example demonstrating RadioLib-compatible API usage.
 * This shows how to migrate from RadioLib with minimal code changes.
 * 
 * Interop partner: extras/interop_tests/SX127x_PingPong/SX127x_PingPong.ino (RadioLib)
 * 
 * Configuration (RadioLib defaults):
 *   Frequency     : 868 MHz
 *   Spreading fac.: SF9
 *   Bandwidth     : 125 kHz
 *   Coding rate   : 4/7
 *   Sync word     : 0x12
 *   CRC           : on
 *   Preamble      : 8 symbols
 * 
 * EU compliance note: 868 MHz g1 sub-band (868.0–868.6 MHz) permits
 * max 25 mW ERP with a 1% duty cycle (ETSI EN 300 220).  This sketch
 * enforces a minimum TX interval (TX_MIN_INTERVAL_MS) to stay below 1%.
 */

#include <Arduino.h>

// Note: LoRa mode is enabled by default in the library
// (No need to define LORA_ENABLED unless you've disabled it in SX1276.h)
#include <SX1276.h>

// Pin definitions for Adafruit Feather 32u4 RFM95
// These match RadioLib's Module(cs, irq, rst) parameters
#define LORA_CS    8   // Chip select
#define LORA_IRQ   7   // DIO0 interrupt
#define LORA_RST   4   // Reset

#define LORA_FREQ 868.0F

// Create SX1276 instance with RadioLib-compatible constructor
SX1276 radio(LORA_CS, LORA_IRQ, LORA_RST);

// EU 868 MHz g1 sub-band duty cycle enforcement.
// At SF9 / BW 125 kHz a ~20-byte LoRa packet takes ~226 ms airtime.
// 1% duty cycle  =>  min interval >= airtime / 0.01 ≈ 22.6 s.
// We use 25 s to leave headroom.
static const unsigned long TX_MIN_INTERVAL_MS = 25000;
static unsigned long lastTxMs = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for Serial to be ready (or 5 seconds timeout)
  }
  
  Serial.println(F("SX1276_Radio_Lite - RadioLib-Compatible Example"));
  Serial.println(F("Initializing with RadioLib-style API..."));
  
  // RadioLib-compatible begin() with defaults
  // begin(freq, bw, sf, cr, syncWord, power, preambleLength, gain)
  // All parameters after freq are optional with sensible defaults
  int16_t state = radio.begin(LORA_FREQ);  // Just frequency in MHz
  // Or with full parameters like RadioLib:
  // int16_t state = radio.begin(LORA_FREQ, 125.0, 9, 7, 0x12, 10, 8, 0);
  // Parameters:
  //   freq = 868.0/915.0 MHz
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
    Serial.print(F("  Frequency: ")); Serial.print(LORA_FREQ);Serial.println(" MHz");
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
  // enforce EU 868 MHz 1% duty cycle before next TX
  unsigned long elapsed = millis() - lastTxMs;
  if (elapsed < TX_MIN_INTERVAL_MS) {
    delay(TX_MIN_INTERVAL_MS - elapsed);
  }

  // Prepare message
  static uint32_t counter = 0;
  char message[50];
  snprintf(message, sizeof(message), "RadioLib-style #%lu", counter++);

  Serial.print(F("Transmitting: "));
  Serial.println(message);

  // Transmit the message
  lastTxMs = millis();
  int16_t state = radio.transmit((uint8_t*)message, strlen(message));

  if (state == SX1276_ERR_NONE) {
    Serial.println(F("Transmission successful!"));
  } else {
    Serial.print(F("Transmission failed, error code: "));
    Serial.println(state);
  }

  // Listen for the PingPong peer's response.
  // The peer enforces its own duty cycle, so the response may be delayed
  // up to TX_MIN_INTERVAL_MS; use a matching timeout.
  Serial.println(F("Listening for packets..."));

  uint8_t buffer[255];
  state = radio.receive(buffer, sizeof(buffer), TX_MIN_INTERVAL_MS);

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

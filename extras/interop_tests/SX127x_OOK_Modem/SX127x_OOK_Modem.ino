/*
 * SX127x_OOK_Modem.ino
 *
 * RadioLib SX127x OOK Modem Example
 *
 * Compatible with SX1276_Radio_Lite OOKExample:
 *   - OOK modulation, 868 MHz, 4.8 kbps
 *   - Sync word: 0x12, 0xAD (2 bytes)
 *   - Preamble: 80 bits (10 bytes)
 *   - Variable-length packets, CRC enabled
 *
 * EU compliance note: 868 MHz SRD band (868.0–868.6 MHz) permits OOK at
 * max 25 mW ERP with a 1% duty cycle (ETSI EN 300 220).  This sketch
 * enforces a minimum TX interval (TX_MIN_INTERVAL_MS) to stay below 1%.
 * Parameters that affect the duty cycle: bitrate (beginFSK), preamble
 * length (beginFSK), payload size, and TX_MIN_INTERVAL_MS.
 *
 * Receive notes:
 *   The receive loop uses startReceive() rather than the blocking receive() so
 *   that the sketch can print the instantaneous RSSI every RX_RSSI_INTERVAL_MS
 *   to confirm the OOKExample signal is physically arriving at this receiver.
 *
 * Receive troubleshooting notes:
 *   - OOK peak threshold detection needs sufficient preamble time to converge.
 *     80 bits (10 bytes) at 4.8 kbps = 16.7 ms, which is enough for the
 *     SX1276 OOK envelope detector to stabilize before the sync word arrives.
 *   - OokPeakThreshDec is slowed to 1/8 chip rate so the threshold decays
 *     more slowly during "0" bit periods, keeping it close to the true peak
 *     and providing a stable decision boundary.
 *   - Receive timeout is set to 30 s (covers 2+ OOKExample TX cycles of
 *     ~13 s each), so phase misalignment cannot prevent a packet from being
 *     captured regardless of the independent loop timing.
 *
 * Tested with Lilygo T3 LoRa32 V1.6.1 (SX1276).
 * Pin macros LORA_CS, LORA_IRQ, LORA_RST, LORA_D1 are provided by the
 * ESP32 Arduino board package for this board variant.
 *
 * For full RadioLib API reference, see the GitHub Pages
 * https://jgromes.github.io/RadioLib/
 */

#include <RadioLib.h>

#if defined(ARDUINO_TTGO_LoRa32_v21new)
// Pin definitions — resolved by the ESP32 Arduino board package
// for the Lilygo T3 LoRa32 V1.6.1 board variant.
#define RADIO_CS   LORA_CS
#define RADIO_RST  LORA_RST
#define RADIO_DIO0 LORA_IRQ
#define RADIO_DIO1 LORA_D1
#else
#if !defined(LORA_CS) || !defined(LORA_RST) || !defined(LORA_IRQ)
#error "Unsupported board: define RADIO_CS/RADIO_RST/RADIO_DIO0 for this target."
#endif
#endif

SX1276 radio = new Module(RADIO_CS, RADIO_DIO0, RADIO_RST, RADIO_DIO1);

// Transmit/receive counter
static uint32_t counter = 0;

// How often to print RSSI during the receive window (milliseconds).
static const uint32_t RX_RSSI_INTERVAL_MS = 2000;

// Receive window (CRC ON).  Must cover at least 2 full OOKExample TX cycles so
// that the window always overlaps at least one transmission regardless of phase.
// OOKExample cycle ≈ 13 s → 30 s covers 2+ cycles comfortably.
static const uint32_t RX_TIMEOUT_MS = 30000;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000);

  Serial.println(F("[SX1276] Initializing OOK modem ..."));

  // Start in FSK mode.  OOK is enabled below via setOOK().
  // Parameters: freq=868 MHz, bitrate=4.8 kbps, freqDev=5.0 kHz (unused in OOK),
  //             rxBw=10.4 kHz, power=17 dBm, preambleLength=80 bits.
  // Preamble is 80 bits (10 bytes) — double the minimum — to give the OOK
  // envelope peak detector enough on/off cycles to converge before the sync word.
  int state = radio.beginFSK(868.0, 4.8, 5.0, 10.4, 17, 80);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[SX1276] beginFSK() failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // Switch to OOK modulation.
  // NOTE: Maximum OOK bit rate is 32.768 kbps.
  state = radio.setOOK(true);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[SX1276] setOOK() failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // Explicitly request peak OOK threshold detection (this is the chip default,
  // but being explicit documents intent and guards against library version drift).
  state = radio.setOokThresholdType(RADIOLIB_SX127X_OOK_THRESH_PEAK);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[SX1276] setOokThresholdType() failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // Slow the OOK peak threshold decrement to once every 8 chips (default: every chip).
  // With the default rate the threshold decays as fast as it increments, so it
  // oscillates without locking.  Slowing the decay (DEC_1_8_CHIP) keeps the
  // threshold close to the true signal peak during the "0" half of symmetric
  // preamble pulses, giving the detector a stable decision boundary.
  state = radio.setOokPeakThresholdDecrement(RADIOLIB_SX127X_OOK_PEAK_THRESH_DEC_1_8_CHIP);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[SX1276] setOokPeakThresholdDecrement() failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // Disable data shaping (no filter), matching the SX1276_Radio_Lite default.
  state = radio.setDataShapingOOK(0);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[SX1276] setDataShapingOOK() failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // Set sync word to match OOKExample: {0x12, 0xAD}.
  uint8_t syncWord[] = {0x12, 0xAD};
  state = radio.setSyncWord(syncWord, 2);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[SX1276] setSyncWord() failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // Seed PRNG from free-running timer so two identically-flashed boards
  // start with different random offsets.
  randomSeed(micros());

  Serial.println(F("[SX1276] OOK modem ready (868 MHz, 4.8 kbps, 80-bit preamble, sync 0x12AD)"));
}

// ---------------------------------------------------------------------------
// receiveWithDiagnostics() — non-blocking receive with RSSI monitoring.
//
// @param timeoutMs   Receive window length in milliseconds.
// @param received    Output: string filled with received payload on success.
// @param peakRssi    Output: highest RSSI sample seen during the window
//                    (dBm, negative).  Initialise to a very low value before
//                    calling so callers can detect whether a signal was ever
//                    present even on timeout.
// @return RADIOLIB_ERR_NONE        — packet received
//         RADIOLIB_ERR_RX_TIMEOUT — no PayloadReady within timeoutMs
//         other negative codes    — RadioLib error from readData()
// ---------------------------------------------------------------------------
static int16_t receiveWithDiagnostics(uint32_t timeoutMs,
                                       String& received, float& peakRssi) {
  int16_t state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }

  uint32_t start    = millis();
  uint32_t nextRssi = start + RX_RSSI_INTERVAL_MS;
  bool payloadReady = false;

  while (millis() - start < timeoutMs) {
    if (digitalRead(RADIO_DIO0)) {
      payloadReady = true;
      break;
    }

    if (millis() >= nextRssi) {
      // skipReceive=true: read RSSI register without calling startReceive()/
      // standby().  The default (skipReceive=false) would stop the receiver
      // after every RSSI sample, leaving the radio in standby for ~2 s and
      // preventing any packet from being received.
      float rssi = radio.getRSSI(false, true);
      if (rssi > peakRssi) peakRssi = rssi;
      Serial.print(F("  [RSSI] "));
      Serial.print(rssi);
      Serial.println(F(" dBm"));
      nextRssi += RX_RSSI_INTERVAL_MS;
    }

    delay(1);
  }

  if (!payloadReady) {
    radio.standby();
    return RADIOLIB_ERR_RX_TIMEOUT;
  }

  // Read one final RSSI sample right after PayloadReady to capture the
  // in-packet peak (the value is latched by the SX127x until next RX start).
  float rssiNow = radio.getRSSI(false, true);
  if (rssiNow > peakRssi) peakRssi = rssiNow;

  state = radio.readData(received);
  return state;
}

void loop() {
  // Enforce 1% duty cycle (EU 868 MHz sub-band g1, ETSI EN 300 220).
  // TX_MIN_INTERVAL_MS >= airtime / 0.01.  At 4.8 kbps with 80-bit preamble,
  // a ~30-byte frame (preamble+sync+len+payload+CRC) takes ~50 ms on air;
  // 6 s interval -> 0.83% duty cycle, well within the 1% limit.
  static const uint32_t TX_MIN_INTERVAL_MS = 6000;
  static uint32_t lastTxEnd = 0;
  if (lastTxEnd != 0) {
    uint32_t elapsed = millis() - lastTxEnd;
    if (elapsed < TX_MIN_INTERVAL_MS) {
      delay(TX_MIN_INTERVAL_MS - elapsed);
    }
  }

  // --- Transmit ---
  char message[50];
  snprintf(message, sizeof(message), "OOK msg #%lu", counter++);

  Serial.print(F("[SX1276] Transmitting: "));
  Serial.println(message);

  int state = radio.transmit(message);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("[SX1276] Transmission successful!"));
  } else {
    Serial.print(F("[SX1276] Transmission failed, code "));
    Serial.println(state);
  }
  lastTxEnd = millis();

  // Enter RX immediately — OOKExample's response arrives within ~10 ms.

  // --- Receive ---
  Serial.print(F("[SX1276] Listening ("));
  Serial.print(RX_TIMEOUT_MS / 1000);
  Serial.println(F(" s) ..."));

  String received;
  float peakRssi = -999.0f;
  state = receiveWithDiagnostics(RX_TIMEOUT_MS, received, peakRssi);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.print(F("[SX1276] Received: "));
    Serial.println(received);
    Serial.print(F("[SX1276] RSSI: "));
    Serial.print(peakRssi);
    Serial.println(F(" dBm"));

  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.print(F("[SX1276] Timeout. Peak RSSI: "));
    Serial.print(peakRssi);
    Serial.println(F(" dBm"));

  } else {
    Serial.print(F("[SX1276] Receive error, code "));
    Serial.println(state);
  }

  Serial.println();

  // Random delay before next TX to prevent phase-locking with OOKExample.
  // OOKExample enters its RX window 2-4 s after transmitting.  Without this
  // delay, OOK_Modem responds within ~5 ms of receiving — long before
  // OOKExample has switched from TX to RX — so every response is missed.
  // A 4-7 s pause ensures the next TX falls inside OOKExample's 10 s RX
  // window regardless of its random post-TX delay (2-4 s).
  delay(4000 + random(0, 3000));
}

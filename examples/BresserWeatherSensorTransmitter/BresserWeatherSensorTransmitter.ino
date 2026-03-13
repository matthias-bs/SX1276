///////////////////////////////////////////////////////////////////////////////////////////////////
// BresserWeatherSensorTransmitter.ino
//
// Example for SX1276_Radio_Lite – Bresser 6-in-1 thermo-hygro sensor emulator.
//
// Encodes temperature and humidity readings into the Bresser 6-in-1 over-the-air
// protocol and transmits them on 868.3 MHz using the same radio configuration as a
// real Bresser sensor.  The transmitted packet is accepted by any receiver that
// implements the 6-in-1 decoder, including BresserWeatherSensorBasic and rtl_433.
//
// Sensor emulated:
//   Bresser 6-in-1 thermo-hygro (type 0x02, SENSOR_TYPE_THERMO_HYGRO)
//   Protocol flags == 0  →  temperature + humidity; no wind, no UV, no rain
//
// Radio configuration (identical to a physical Bresser 6-in-1 sensor):
//   Frequency:          868.3 MHz
//   Bit rate:             8.21 kbps
//   Frequency deviation: 57.136 kHz
//   Preamble:            32 bits  (4 × 0xAA)
//   Sync word:           0xAA 0x2D
//   First payload byte:  0xD4  (third Bresser sync byte, received as payload[0])
//   Packet length:       27 bytes (fixed), hardware CRC disabled
//
// Physical Bresser RF frame layout:
//   [AA AA AA AA]  [AA 2D]  [D4  <18 bytes 6-in-1 data>  <8 bytes padding>]
//    ──preamble──  ─sync─   ───────────────27 byte payload──────────────────
//
// Pin defaults:
//   Adafruit Feather 32u4 RFM95 : CS = 8,  RST = 4,  DIO0 = 7
//   All other boards             : LORA_CS / LORA_RST / LORA_IRQ from BSP
//
// !!! WARNING !!!
//   Transmitting on 868 MHz (or any ISM band) is subject to local radio
//   regulations (e.g. ETSI EN 300 220 in Europe, FCC Part 15 in the US).
//   Duty-cycle limits and output-power limits apply.
//
//   This sketch transmits at 10 dBm (PA_BOOST pin, SX1276 default) via the
//   TTGO LoRa32 on-board antenna connector.  Each 27-byte packet takes roughly
//   32 ms on air at 8.21 kbps; at the default TX_INTERVAL_MS of 60 s this
//   gives a duty cycle of ≈ 0.05 %, well within the ETSI 1 % hourly limit.
//   However, even this low power level can interfere with nearby real Bresser
//   base stations if a real antenna is attached.
//
//   For close-range bench testing, reduce the power to the minimum (+2 dBm)
//   by changing the 5th argument of beginFSK() and use a 50 Ω dummy load
//   or a shielded enclosure instead of an antenna.
//
// https://github.com/matthias-bs/SX1276_Radio_Lite
//
// NOTE: This sketch was created as a proof of concept with the assistance of
// GitHub Copilot (Claude Sonnet 4.6). It is NOT intended as a production-ready
// solution.
//
//
// created: 03/2026
//
//
// MIT License
//
// Copyright (c) 2026 Matthias Prinke
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include "SX1276.h"
#include "Bresser6in1Encoder.h"

// ---------------------------------------------------------------------------
// User-configurable sensor parameters
// ---------------------------------------------------------------------------

/// Unique 32-bit sensor ID.  Any value is valid; choose one not used by a
/// real sensor in the vicinity to avoid collisions.
#define SENSOR_ID       0xDEADBEEFUL

/// Sensor channel (0-7).
#define SENSOR_CHANNEL  1

/// Initial temperature transmitted at startup (degrees Celsius).
/// Subsequent packets will add a small synthetic drift for demonstration.
#define INITIAL_TEMP_C  21.5f

/// Initial relative humidity transmitted at startup (percent, 0-99).
#define INITIAL_HUM_PCT 55

/// Transmission interval in milliseconds.
/// Keep this value large enough to respect local duty-cycle regulations
/// (e.g. ≤ 1 % per hour on 868 MHz sub-bands ≈ at least  36 s between packets).
#define TX_INTERVAL_MS  60000UL

// ---------------------------------------------------------------------------
// Pin definitions
// ---------------------------------------------------------------------------
#if defined(ARDUINO_AVR_FEATHER32U4)
#  define RADIO_CS   8
#  define RADIO_RST  4
#  define RADIO_DIO0 7
#else
#  define RADIO_CS   LORA_CS
#  define RADIO_RST  LORA_RST
#  define RADIO_DIO0 LORA_IRQ
#endif

// ---------------------------------------------------------------------------
// Bresser RF packet length (matches MSG_BUF_SIZE in WeatherSensor.h and
// BresserDecoders.h).  The 27 bytes are:
//   buffer[0]    = 0xD4          (Bresser's third sync byte / decoder marker)
//   buffer[1..18] = 18-byte 6-in-1 encoded payload
//   buffer[19..26] = 0x00        (padding; decoder does not use these bytes)
// ---------------------------------------------------------------------------
#define PACKET_LENGTH 27

// ---------------------------------------------------------------------------
// Radio instance  (constructor order: cs, irq/DIO0, rst)
// ---------------------------------------------------------------------------
SX1276 radio(RADIO_CS, RADIO_DIO0, RADIO_RST);

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void printPacketHex(const uint8_t *buf, uint8_t len);

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
#if defined(ESP32) || defined(ESP8266)
    Serial.setDebugOutput(false);
#endif
    while (!Serial && millis() < 5000) {
        ; // brief wait for USB-CDC boards
    }

    Serial.println(F("\n=== BresserWeatherSensorTransmitter ==="));
    Serial.println(F("Bresser 6-in-1 thermo-hygro emulator"));
    Serial.println();

    // ------------------------------------------------------------------
    // Initialise radio in FSK mode with Bresser 6-in-1 parameters.
    // beginFSK(freq[MHz], bitrate[kbps], freqDev[kHz], rxBw[kHz], power[dBm], preamble[bits])
    // ------------------------------------------------------------------
    int16_t state = radio.beginFSK(868.3f, 8.21f, 57.136417f, 250.0f, 10, 32);
    if (state != SX1276_ERR_NONE) {
        Serial.print(F("beginFSK() failed, error: "));
        Serial.println(state);
        while (true) delay(1000);
    }

    // ------------------------------------------------------------------
    // Fixed-length packet, hardware CRC disabled – mirrors the Bresser
    // sensor (fixedPacketLengthMode() + setCrcFiltering(false) in RadioLib).
    // ------------------------------------------------------------------
    state = radio.setPacketConfig(true, false);
    if (state != SX1276_ERR_NONE) {
        Serial.print(F("setPacketConfig() failed, error: "));
        Serial.println(state);
        while (true) delay(1000);
    }
    radio.writeRegister(SX1276_REG_PAYLOAD_LENGTH_FSK, PACKET_LENGTH);

    // ------------------------------------------------------------------
    // Sync word 0xAA 0x2D.
    // Physical Bresser frame: AA AA AA AA | AA 2D | D4 <payload>
    //   - SX1276 preamble (32 bits = 4 × 0xAA) contributes the first four
    //     bytes of 0xAA before the sync word bytes.
    //   - After the sync word (0xAA 0x2D), the radio transmits the payload,
    //     whose first byte is 0xD4 (the third Bresser sync byte).
    // ------------------------------------------------------------------
    uint8_t syncWord[] = {0xAA, 0x2D};
    state = radio.setSyncWord(syncWord, sizeof(syncWord));
    if (state != SX1276_ERR_NONE) {
        Serial.print(F("setSyncWord() failed, error: "));
        Serial.println(state);
        while (true) delay(1000);
    }

    Serial.println(F("Radio initialised OK."));
    Serial.print(F("  Sensor ID  : 0x"));
    Serial.println(SENSOR_ID, HEX);
    Serial.print(F("  Channel    : "));
    Serial.println(SENSOR_CHANNEL);
    Serial.print(F("  TX interval: "));
    Serial.print(TX_INTERVAL_MS / 1000UL);
    Serial.println(F(" s"));
    Serial.println();
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop()
{
    static float   temp_c   = INITIAL_TEMP_C;
    static uint8_t hum_pct  = INITIAL_HUM_PCT;
    static bool    startup   = true;   // first transmission flag
    static uint32_t lastTx  = 0xFFFFFFFFUL - TX_INTERVAL_MS; // transmit immediately

    if ((uint32_t)(millis() - lastTx) < TX_INTERVAL_MS)
        return;
    lastTx = millis();

    // ---------------------------------------------------------------
    // Build the 27-byte RF payload.
    //   buffer[0]         = 0xD4  (Bresser third sync byte)
    //   buffer[1..18]     = 6-in-1 encoded data (BRESSER_6IN1_MSG_SIZE bytes)
    //   buffer[19..26]    = 0x00  (padding; ignored by the decoder)
    // ---------------------------------------------------------------
    uint8_t buffer[PACKET_LENGTH];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 0xD4;

    encode6In1ThermoHygro(&buffer[1],           // msg[0..17]
                           SENSOR_ID,
                           SENSOR_CHANNEL,
                           temp_c,
                           hum_pct,
                           /*battery_ok=*/true,
                           startup);

    // ---------------------------------------------------------------
    // Print what we are about to send.
    // ---------------------------------------------------------------
    Serial.print(F("["));
    Serial.print(millis() / 1000UL);
    Serial.print(F("s] Transmitting  T="));
    Serial.print(temp_c, 1);
    Serial.print(F(" °C  H="));
    Serial.print(hum_pct);
    Serial.print(F(" %  startup="));
    Serial.println(startup ? F("yes") : F("no"));

    Serial.print(F("  Payload hex: "));
    printPacketHex(buffer, PACKET_LENGTH);

    // ---------------------------------------------------------------
    // Transmit.
    // ---------------------------------------------------------------
    int16_t state = radio.transmit(buffer, PACKET_LENGTH);
    if (state == SX1276_ERR_NONE) {
        Serial.println(F("  >> TX OK"));
    } else {
        Serial.print(F("  >> TX failed, error: "));
        Serial.println(state);
    }
    Serial.println();

    // ---------------------------------------------------------------
    // Advance synthetic sensor data for the next packet.
    // In a real application, replace this with actual sensor readings.
    // ---------------------------------------------------------------
    startup = false;
    temp_c += 0.3f;                 // drift up 0.3 °C per cycle
    if (temp_c > 40.0f)
        temp_c = INITIAL_TEMP_C;   // wrap back for demonstration
    if (hum_pct < 95)
        hum_pct++;
    else
        hum_pct = INITIAL_HUM_PCT;
}

// ---------------------------------------------------------------------------
// Utility: print a byte buffer as hex to Serial.
// ---------------------------------------------------------------------------
static void printPacketHex(const uint8_t *buf, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        if (buf[i] < 0x10) Serial.print('0');
        Serial.print(buf[i], HEX);
        if (i < len - 1) Serial.print(' ');
    }
    Serial.println();
}

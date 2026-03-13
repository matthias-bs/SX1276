///////////////////////////////////////////////////////////////////////////////////////////////////
// BresserWeatherSensorBasic.ino
//
// Example for SX1276_Radio_Lite – Bresser weather sensor receiver with full decoding.
//
// Uses the same interrupt-driven FSK receive mechanism as BresserRxExample.ino combined
// with decoder functions adapted from BresserWeatherSensorReceiver (BresserDecoders.*).
// No RadioLib or WeatherSensor class required.
//
// The serial output is equivalent to:
// https://github.com/matthias-bs/BresserWeatherSensorReceiver/blob/main/examples/
//                              BresserWeatherSensorBasic/BresserWeatherSensorBasic.ino
//
// Radio configuration (mirrors WeatherSensor.cpp for SX1276):
//   Frequency:       868.3 MHz
//   Bit rate:         8.21 kbps
//   Freq deviation:  57.136 kHz
//   Rx bandwidth:   250 kHz
//   Preamble:        32 bits (0xAA x4)
//   Sync word:       0xAA 0x2D  (last physical sync byte 0xD4 = first payload byte)
//   Packet length:   27 bytes (fixed), CRC disabled
//
// Pin defaults:
//   Adafruit Feather 32u4 RFM95: CS=8, RST=4, DIO0=7
//   All other boards: LORA_CS / LORA_RST / LORA_IRQ from BSP
//
// https://github.com/matthias-bs/SX1276_Radio_Lite
//
// NOTE: This sketch was created as a proof of concept with the assistance of
// GitHub Copilot (Claude Sonnet). It demonstrates that SX1276_Radio_Lite can
// receive and decode Bresser weather sensor packets without RadioLib or the
// WeatherSensor class. The decoder functions are adapted (duplicated and
// simplified) from BresserWeatherSensorReceiver – slot management, Preferences
// storage, and logging macros have been removed to keep the sketch self-contained.
// It is NOT intended as a production-ready solution.
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
#include "BresserDecoders.h"

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

// Fixed packet length for Bresser sensors (matches MSG_BUF_SIZE in WeatherSensor.h)
#define PACKET_LENGTH MSG_BUF_SIZE

// ---------------------------------------------------------------------------
// Radio instance
// Constructor order: (cs, irq/DIO0, rst)
// ---------------------------------------------------------------------------
SX1276 radio(RADIO_CS, RADIO_DIO0, RADIO_RST);

// Flag set by the DIO0 ISR when a complete packet is available
static volatile bool receivedFlag = false;

// ISR – must reside in IRAM on ESP32/ESP8266
#if defined(ESP8266) || defined(ESP32)
IRAM_ATTR
#endif
void setFlag(void) {
    receivedFlag = true;
}

// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
#if defined(ESP32) || defined(ESP8266)
    Serial.setDebugOutput(true);
#endif
    Serial.println(F("Starting execution..."));

    // Initialise radio in FSK mode – mirrors WeatherSensor::begin() for SX1276
    // beginFSK(freq[MHz], bitrate[kbps], freqDev[kHz], rxBw[kHz], power[dBm], preamble[bits])
    int16_t state = radio.beginFSK(868.3, 8.21, 57.136417, 250.0, 10, 32);
    if (state != SX1276_ERR_NONE) {
        Serial.print(F("beginFSK() failed, error: ")); Serial.println(state);
        while (true) delay(1000);
    }

    // Fixed-length packet, CRC disabled – mirrors fixedPacketLengthMode() + setCrcFiltering(false)
    state = radio.setPacketConfig(true, false);
    if (state != SX1276_ERR_NONE) {
        Serial.print(F("setPacketConfig() failed, error: ")); Serial.println(state);
        while (true) delay(1000);
    }
    radio.writeRegister(SX1276_REG_PAYLOAD_LENGTH_FSK, PACKET_LENGTH);

    // Sync word 0xAA 0x2D:
    // Physical frame AA AA AA AA | 2D D4 | <26 payload bytes>
    // The preamble (32 bits = 4 × 0xAA) eats the 5th preamble byte; the last
    // physical sync byte 0xD4 therefore arrives as the first byte of the payload.
    uint8_t syncWord[] = {0xAA, 0x2D};
    state = radio.setSyncWord(syncWord, 2);
    if (state != SX1276_ERR_NONE) {
        Serial.print(F("setSyncWord() failed, error: ")); Serial.println(state);
        while (true) delay(1000);
    }

    // Start interrupt-driven reception
    radio.setPacketReceivedAction(setFlag);
    state = radio.startReceive();
    if (state != SX1276_ERR_NONE) {
        Serial.print(F("startReceive() failed, error: ")); Serial.println(state);
        while (true) delay(1000);
    }
}

// ---------------------------------------------------------------------------
void loop()
{
    if (!receivedFlag) {
        delay(100);
        return;
    }
    receivedFlag = false;

    // Read raw packet
    uint8_t buffer[PACKET_LENGTH];
    int16_t state = radio.readData(buffer, sizeof(buffer));
    float   rssi  = radio.getRSSI_FSK();

    // Restart reception immediately so we don't miss the next packet
    radio.startReceive();

    // Verify the leading 0xD4 sync byte and that readData() succeeded
    if (state <= 0 || buffer[0] != 0xD4)
        return;

    // Strip the 0xD4 byte; pass the remaining 26 bytes to the decoder
    SensorData sensor;
    memset(&sensor, 0, sizeof(sensor));
    DecodeStatus decode_status = decodeMessage(&buffer[1], sizeof(buffer) - 1,
                                               &sensor, rssi);

    if (decode_status != DECODE_OK)
        return;

    // -------------------------------------------------------------------------
    // Print decoded sensor data
    // -------------------------------------------------------------------------
    const __FlashStringHelper *batt;
    if ((sensor.s_type == SENSOR_TYPE_WEATHER1) && !sensor.temp_ok)
        batt = F("---");
    else if (sensor.battery_ok)
        batt = F("OK");
    else
        batt = F("Low");

    Serial.print(F("Id: [")); Serial.print((unsigned long)sensor.sensor_id, HEX);
    Serial.print(F("] Typ: [")); Serial.print(sensor.s_type, HEX);
    Serial.print(F("] Ch: [")); Serial.print(sensor.chan);
    Serial.print(F("] St: [")); Serial.print(sensor.startup);
    Serial.print(F("] Bat: [")); Serial.print(batt);
    Serial.print(F("] RSSI: [")); Serial.print(sensor.rssi, 1);
    Serial.print(F("dBm] "));

    if (sensor.s_type == SENSOR_TYPE_LIGHTNING) {
        // Lightning sensor
        Serial.print(F("Lightning Counter: [")); Serial.print(sensor.lgt_strike_count);
        Serial.print(F("] Distance: ["));
        if (sensor.lgt_distance_km != 0) { Serial.print(sensor.lgt_distance_km); Serial.print(F("km")); }
        else Serial.print(F("----"));
        Serial.print(F("] unknown1: [0x")); Serial.print(sensor.lgt_unknown1, HEX);
        Serial.print(F("] unknown2: [0x")); Serial.print(sensor.lgt_unknown2, HEX);
        Serial.println(F("]"));
    }
    else if (sensor.s_type == SENSOR_TYPE_LEAKAGE) {
        // Water leakage sensor
        Serial.print(F("Leakage: ["));
        Serial.println(sensor.leak_alarm ? F("ALARM]") : F("OK]"));
    }
    else if (sensor.s_type == SENSOR_TYPE_AIR_PM) {
        // Air quality – particulate matter
        Serial.print(F("PM1.0: ["));
        if (sensor.pm_1_0_init) Serial.print(F("init"));
        else { Serial.print(sensor.pm_1_0); Serial.print(F("\xc2\xb5g/m\xc2\xb3")); }
        Serial.print(F("] PM2.5: ["));
        if (sensor.pm_2_5_init) Serial.print(F("init"));
        else { Serial.print(sensor.pm_2_5); Serial.print(F("\xc2\xb5g/m\xc2\xb3")); }
        Serial.print(F("] PM10: ["));
        if (sensor.pm_10_init) Serial.print(F("init"));
        else { Serial.print(sensor.pm_10); Serial.print(F("\xc2\xb5g/m\xc2\xb3")); }
        Serial.println(F("]"));
    }
    else if (sensor.s_type == SENSOR_TYPE_CO2) {
        // CO2 sensor
        Serial.print(F("CO2: ["));
        if (sensor.co2_init) Serial.println(F("init]"));
        else { Serial.print(sensor.co2_ppm); Serial.println(F("ppm]")); }
    }
    else if (sensor.s_type == SENSOR_TYPE_HCHO_VOC) {
        // HCHO / VOC sensor
        Serial.print(F("HCHO: ["));
        if (sensor.hcho_init) Serial.print(F("init"));
        else { Serial.print(sensor.hcho_ppb); Serial.print(F("ppb")); }
        Serial.print(F("] VOC: ["));
        if (sensor.voc_init) Serial.println(F("init]"));
        else { Serial.print(sensor.voc_level); Serial.println(F("]")); }
    }
    else if (sensor.s_type == SENSOR_TYPE_SOIL) {
        // Soil moisture sensor
        Serial.print(F("Temp: [")); Serial.print(sensor.soil_temp_c, 1);
        Serial.print(F("C] Moisture: [")); Serial.print(sensor.soil_moisture);
        Serial.println(F("%]"));
    }
    else {
        // All weather-type sensors (WEATHER0, WEATHER1, WEATHER3, WEATHER8,
        //                           THERMO_HYGRO, POOL_THERMO, RAIN)
        Serial.print(F("Temp: ["));
        if (sensor.temp_ok) { Serial.print(sensor.temp_c, 1); Serial.print(F("C] ")); }
        else Serial.print(F("---.-C] "));

        Serial.print(F("Hum: ["));
        if (sensor.humidity_ok) { Serial.print(sensor.humidity); Serial.print(F("%] ")); }
        else Serial.print(F("---%] "));

        Serial.print(F("Wmax: ["));
        if (sensor.wind_ok) {
            Serial.print(sensor.wind_gust_meter_sec, 1); Serial.print(F("m/s] Wavg: ["));
            Serial.print(sensor.wind_avg_meter_sec, 1); Serial.print(F("m/s] Wdir: ["));
            Serial.print(sensor.wind_direction_deg, 1); Serial.print(F("deg] "));
        } else {
            Serial.print(F("--.-m/s] Wavg: [--.-m/s] Wdir: [---.-deg] "));
        }

        Serial.print(F("Rain: ["));
        if (sensor.rain_ok) { Serial.print(sensor.rain_mm, 1); Serial.print(F("mm] ")); }
        else Serial.print(F("-----.-mm] "));

        // UV (6-in-1 and 7-in-1 sensors)
        Serial.print(F("UVidx: ["));
        if (sensor.uv_ok) { Serial.print(sensor.uv, 1); Serial.print(F("] ")); }
        else Serial.print(F("--.-] "));

        // Light (7-in-1 sensors only)
        Serial.print(F("Light: ["));
        if (sensor.light_ok) { Serial.print(sensor.light_klx, 1); Serial.print(F("klx] ")); }
        else Serial.print(F("--.-klx] "));

        // Globe thermometer (8-in-1 only)
        if (sensor.s_type == SENSOR_TYPE_WEATHER8) {
            Serial.print(F("T_globe: ["));
            if (sensor.tglobe_ok) { Serial.print(sensor.tglobe_c, 1); Serial.print(F("C] ")); }
            else Serial.print(F("--.-C] "));
        }
        Serial.println();
    }
}

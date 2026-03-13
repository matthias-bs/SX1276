///////////////////////////////////////////////////////////////////////////////////////////////////
// Bresser6in1Encoder.h
//
// Bresser 6-in-1 protocol encoder for use with SX1276_Radio_Lite.
//
// Encodes temperature and humidity data into the Bresser 6-in-1 over-the-air packet format.
// The encoded payload is compatible with BresserWeatherSensorBasic (and any receiver based on
// BresserWeatherSensorReceiver) that has the 6-in-1 decoder enabled.
//
// Protocol reference:
//   rtl_433 by Benjamin Larsson:
//   https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_6in1.c
//
// Packet layout produced by this encoder (flags == 0, temp/humidity message):
//
//   Byte  Field
//   ----  -----
//    0-1  LFSR-16 digest (gen 0x8810, key 0x5412) over bytes 2-16
//    2-5  Sensor ID (32-bit, big-endian)
//    6    [7:4] Sensor type | [3] Startup flag* | [2:0] Channel
//    7-9  Wind (set to 0x00 → invalid; XOR with 0xFF gives 0xFF > 0x99)
//   10-11 Wind direction (set to 0x00, not used for thermo-hygro)
//   12    Temperature hundreds + tens digit (BCD)
//   13    [7:4] Temp ones digit  |  [3] Sign  |  [1] Battery OK
//   14    Humidity (BCD, e.g. 0x65 for 65 %)
//   15    UV (set to 0xFF → complement 0x00 ≤ 0x99; type override forces invalid)
//   16    [3:0] Flags = 0x0 (temp/humidity message; no rain)
//   17    Add-with-carry checksum so that sum(bytes 2-17) & 0xFF == 0xFF
//
//   * Startup flag: bit 3 = 0 means first transmission after power-on.
//
// Sensor type used: SENSOR_TYPE_THERMO_HYGRO (0x02).
//   The 6-in-1 decoder forces wind_ok and uv_ok to false for this type.
//
// Total 6-in-1 payload: 18 bytes (indices 0-17 above).
// The caller prepends 0xD4 and appends zero-padding to produce the full
// 27-byte Bresser RF payload (see BresserWeatherSensorTransmitter.ino).
//
// https://github.com/matthias-bs/SX1276_Radio_Lite
//
// MIT License
// Copyright (c) 2026 Matthias Prinke
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef BRESSER_6IN1_ENCODER_H
#define BRESSER_6IN1_ENCODER_H

#include <Arduino.h>

// Size of the 18-byte 6-in-1 message (bytes 0-17 of the 26 bytes after the 0xD4 marker)
#define BRESSER_6IN1_MSG_SIZE 18

/**
 * @brief Encode a thermo-hygro sensor reading into the Bresser 6-in-1 format.
 *
 * Fills msg[0..17] with a valid, ready-to-transmit 6-in-1 packet for a
 * temperature/humidity sensor (sensor type SENSOR_TYPE_THERMO_HYGRO = 0x02,
 * flags == 0).  Wind and UV fields are set to "invalid" values; the checksum
 * and LFSR digest are computed automatically.
 *
 * @param msg        Output buffer – must be at least BRESSER_6IN1_MSG_SIZE bytes.
 * @param id         32-bit sensor ID (arbitrary; choose a unique value).
 * @param channel    Channel number (0-7).
 * @param temp_c     Temperature in degrees Celsius (range -49.9 … +99.9 °C).
 * @param humidity   Relative humidity in percent (0-99).
 * @param battery_ok true if the (simulated) battery is OK; false for low battery.
 * @param startup    true for the first transmission after power-on.
 */
void encode6In1ThermoHygro(uint8_t *msg,
                            uint32_t id,
                            uint8_t  channel,
                            float    temp_c,
                            uint8_t  humidity,
                            bool     battery_ok,
                            bool     startup);

#endif // BRESSER_6IN1_ENCODER_H

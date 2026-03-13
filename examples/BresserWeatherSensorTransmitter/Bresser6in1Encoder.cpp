///////////////////////////////////////////////////////////////////////////////////////////////////
// Bresser6in1Encoder.cpp
//
// Bresser 6-in-1 protocol encoder for use with SX1276_Radio_Lite.
//
// See Bresser6in1Encoder.h for the full description.
//
// https://github.com/matthias-bs/SX1276_Radio_Lite
//
// MIT License
// Copyright (c) 2026 Matthias Prinke
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Bresser6in1Encoder.h"

// ---------------------------------------------------------------------------
// Sensor type constant (matches BresserDecoders.h / WeatherSensor.h)
// ---------------------------------------------------------------------------
#define SENSOR_TYPE_THERMO_HYGRO 2

// ---------------------------------------------------------------------------
// Internal helper functions (mirror the utility routines in BresserDecoders.cpp)
// ---------------------------------------------------------------------------

/**
 * @brief LFSR-16 digest used by the Bresser 6-in-1 protocol.
 *
 * Generator polynomial: 0x8810
 * Initial key:          supplied by caller (0x5412 for 6-in-1 encoding)
 *
 * @param message  Pointer to the first byte to process.
 * @param bytes    Number of bytes to process.
 * @param gen      LFSR generator polynomial.
 * @param key      Starting key value.
 * @return         16-bit digest.
 */
static uint16_t lfsr_digest16(const uint8_t *message, unsigned bytes,
                               uint16_t gen, uint16_t key)
{
    uint16_t sum = 0;
    for (unsigned k = 0; k < bytes; ++k) {
        uint8_t data = message[k];
        for (int i = 7; i >= 0; --i) {
            if ((data >> i) & 1)
                sum ^= key;
            if (key & 1)
                key = (key >> 1) ^ gen;
            else
                key = (key >> 1);
        }
    }
    return sum;
}

/**
 * @brief Byte-addition (add-with-carry) checksum helper.
 *
 * Returns the unsigned sum of all bytes (no modular reduction here;
 * the caller masks with 0xFF as needed).
 */
static int add_bytes(const uint8_t *message, unsigned num_bytes)
{
    int result = 0;
    for (unsigned i = 0; i < num_bytes; ++i)
        result += (int)message[i];
    return result;
}

// ---------------------------------------------------------------------------
// Public encoder
// ---------------------------------------------------------------------------

void encode6In1ThermoHygro(uint8_t *msg,
                            uint32_t id,
                            uint8_t  channel,
                            float    temp_c,
                            uint8_t  humidity,
                            bool     battery_ok,
                            bool     startup)
{
    // -----------------------------------------------------------------
    // Bytes 2-5: Sensor ID (big-endian, 32-bit)
    // -----------------------------------------------------------------
    msg[2] = (uint8_t)((id >> 24) & 0xFF);
    msg[3] = (uint8_t)((id >> 16) & 0xFF);
    msg[4] = (uint8_t)((id >>  8) & 0xFF);
    msg[5] = (uint8_t)( id        & 0xFF);

    // -----------------------------------------------------------------
    // Byte 6: [7:4] type | [3] startup* | [2:0] channel
    //   * startup flag: bit 3 = 0 means "just powered on" (startup=true),
    //                   bit 3 = 1 means normal operation  (startup=false)
    // -----------------------------------------------------------------
    msg[6] = (uint8_t)((SENSOR_TYPE_THERMO_HYGRO << 4) |
                       (startup ? 0x00 : 0x08)         |
                       (channel & 0x07));

    // -----------------------------------------------------------------
    // Bytes 7-9: Wind speed (all 0x00 → XOR with 0xFF gives 0xFF > 0x99
    //            → decoder treats wind as invalid)
    // Bytes 10-11: Wind direction (0x00, unused for thermo-hygro)
    // -----------------------------------------------------------------
    msg[7]  = 0x00;
    msg[8]  = 0x00;
    msg[9]  = 0x00;
    msg[10] = 0x00;
    msg[11] = 0x00;

    // -----------------------------------------------------------------
    // Bytes 12-13: Temperature (BCD, with sign)
    //
    // Decoded as:
    //   sign     = (msg[13] >> 3) & 1
    //   temp_raw = (msg[12] >> 4) * 100
    //            + (msg[12] & 0x0F) * 10
    //            + (msg[13] >> 4)
    //   temp_c   = (sign ? (temp_raw - 1000) : temp_raw) * 0.1
    //
    // Valid range produced here: -49.9 … +99.9 °C
    // (Temperatures ≤ -50 °C trigger a special Bresser 3-in-1 anemometer
    // correction in the decoder and shall not be used for thermo-hygro sensors.)
    // -----------------------------------------------------------------
    bool sign;
    int  temp_raw;

    if (temp_c >= 0.0f) {
        sign     = false;
        temp_raw = (int)(temp_c * 10.0f + 0.5f);   // round to nearest 0.1 °C
    } else {
        sign     = true;
        temp_raw = 1000 - (int)(-temp_c * 10.0f + 0.5f);
    }

    // Clamp to representable BCD range (0-999)
    if (temp_raw < 0)   temp_raw = 0;
    if (temp_raw > 999) temp_raw = 999;

    uint8_t d2 = (uint8_t)(temp_raw / 100);         // hundreds digit
    uint8_t d1 = (uint8_t)((temp_raw / 10) % 10);   // tens digit
    uint8_t d0 = (uint8_t)(temp_raw % 10);           // ones digit

    msg[12] = (uint8_t)((d2 << 4) | d1);
    msg[13] = (uint8_t)((d0 << 4) |
                        (sign       ? 0x08 : 0x00) |
                        (battery_ok ? 0x02 : 0x00));

    // -----------------------------------------------------------------
    // Byte 14: Humidity (BCD, e.g. 65 % → 0x65)
    // -----------------------------------------------------------------
    humidity = (uint8_t)(humidity > 99 ? 99 : humidity);
    msg[14]  = (uint8_t)(((humidity / 10) << 4) | (humidity % 10));

    // -----------------------------------------------------------------
    // Byte 15: UV index (inverted BCD).
    //   Set to 0xFF so that ~0xFF = 0x00 ≤ 0x99 (part 1 of uv_ok test),
    //   but the high nibble of byte 16 (0x0, see below) will cause
    //   ~msg[16] & 0xF0 = 0xF0 > 0x90 (part 2 fails) → uv_ok = false.
    //   Additionally, SENSOR_TYPE_THERMO_HYGRO forces uv_ok = false in
    //   the decoder regardless of the raw bytes.
    // -----------------------------------------------------------------
    msg[15] = 0xFF;

    // -----------------------------------------------------------------
    // Byte 16: [3:0] flags = 0x0 (temp/humidity message)
    //   High nibble 0x0 → ~msg[16] & 0xF0 = 0xF0 > 0x90 → UV invalid.
    // -----------------------------------------------------------------
    msg[16] = 0x00;

    // -----------------------------------------------------------------
    // Byte 17: Add-with-carry checksum
    //   Constraint: (sum of bytes 2..17) & 0xFF == 0xFF
    //   → msg[17] = (0xFF - sum(bytes 2..16)) & 0xFF
    // -----------------------------------------------------------------
    int partial_sum = add_bytes(&msg[2], 15);   // bytes 2..16
    msg[17] = (uint8_t)((0xFF - (partial_sum & 0xFF)) & 0xFF);

    // -----------------------------------------------------------------
    // Bytes 0-1: LFSR-16 digest over bytes 2..16 (15 bytes)
    //   Generator: 0x8810   Key: 0x5412
    // -----------------------------------------------------------------
    uint16_t digest = lfsr_digest16(&msg[2], 15, 0x8810, 0x5412);
    msg[0] = (uint8_t)(digest >> 8);
    msg[1] = (uint8_t)(digest & 0xFF);
}

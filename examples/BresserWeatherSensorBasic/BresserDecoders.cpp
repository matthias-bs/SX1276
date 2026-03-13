///////////////////////////////////////////////////////////////////////////////////////////////////
// BresserDecoders.cpp
//
// Standalone Bresser weather sensor decoder functions for use with SX1276_Radio_Lite.
//
// Adapted from BresserWeatherSensorReceiver by Matthias Prinke:
// https://github.com/matthias-bs/BresserWeatherSensorReceiver
//
// Changes vs. the original WeatherSensor / WeatherSensorDecoders:
//  - Free functions instead of class member functions.
//  - Single SensorData* parameter replaces sensor[] slot array + findSlot().
//  - log_d / log_v / log_e removed (no ESP-IDF logging dependency).
//  - WIND_DATA_FIXEDPOINT removed; only floating-point wind data is provided.
//  - LIGHTNING_TEST_DATA support removed.
//
// Based on:
//   rtl_433 by Benjamin Larsson (https://github.com/merbanan/rtl_433)
//   Bresser5in1-CC1101 by Sean Siford (https://github.com/seaniefs/Bresser5in1-CC1101)
//   RadioLib by Jan Gromeš (https://github.com/jgromes/RadioLib)
//
// MIT License
// Copyright (c) 2022-2026 Matthias Prinke
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "BresserDecoders.h"

// ===========================================================================
// Internal utility functions
// (from rtl_433 / WeatherSensor.cpp – copied verbatim)
// ===========================================================================

static uint16_t lfsr_digest16(uint8_t const message[], unsigned bytes,
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

static int add_bytes(uint8_t const message[], unsigned num_bytes)
{
    int result = 0;
    for (unsigned i = 0; i < num_bytes; ++i)
        result += message[i];
    return result;
}

static uint16_t crc16(uint8_t const message[], unsigned nBytes,
                      uint16_t polynomial, uint16_t init)
{
    uint16_t remainder = init;
    for (unsigned byte = 0; byte < nBytes; ++byte) {
        remainder ^= (uint16_t)message[byte] << 8;
        for (unsigned bit = 0; bit < 8; ++bit) {
            if (remainder & 0x8000)
                remainder = (remainder << 1) ^ polynomial;
            else
                remainder = (remainder << 1);
        }
    }
    return remainder;
}

// ===========================================================================
// 5-in-1 decoder
// From rtl_433: https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_5in1.c
//
// Packet layout (after 0xD4 sync byte, 26 bytes):
//   CC CC CC CC CC CC CC CC CC CC CC CC CC uu II sS GG DG WW W TT T HH RR RR Bt
//   [0..12] = inverted [13..25], [13] = popcount of [14..25]
// ===========================================================================
#ifdef BRESSER_5_IN_1
DecodeStatus decode5In1(const uint8_t *msg, uint8_t msgSize,
                        SensorData *out, float rssi)
{
    // First 13 bytes must be the bitwise inverse of the last 13 bytes
    for (unsigned col = 0; col < (unsigned)(msgSize / 2); ++col) {
        if ((msg[col] ^ msg[col + 13]) != 0xFF)
            return DECODE_PAR_ERR;
    }

    // Checksum: popcount of bytes [14..25] must equal byte [13]
    uint8_t bitsSet = 0;
    uint8_t expectedBitsSet = msg[13];
    for (uint8_t p = 14; p < msgSize; p++) {
        uint8_t b = msg[p];
        while (b) { bitsSet += (b & 1); b >>= 1; }
    }
    if (bitsSet != expectedBitsSet)
        return DECODE_CHK_ERR;

    uint8_t id_tmp   = msg[14];
    uint8_t type_tmp = msg[15] & 0x7F;

    out->sensor_id   = id_tmp;
    out->chan        = 0; // no channel in 5-in-1
    out->startup     = ((msg[15] & 0x80) == 0);
    out->battery_ok  = !(msg[25] & 0x80);
    out->valid       = true;
    out->complete    = true;
    out->rssi        = rssi;

    int temp_raw  = (msg[20] & 0x0F) + ((msg[20] & 0xF0) >> 4) * 10 + (msg[21] & 0x0F) * 100;
    if (msg[25] & 0x0F)
        temp_raw = -temp_raw;
    out->temp_c   = temp_raw * 0.1f;
    out->humidity = (msg[22] & 0x0F) + ((msg[22] & 0xF0) >> 4) * 10;

    int wind_dir_raw = ((msg[17] & 0xF0) >> 4) * 225;
    int gust_raw     = ((msg[17] & 0x0F) << 8) + msg[16];
    int wind_raw     = (msg[18] & 0x0F) + ((msg[18] & 0xF0) >> 4) * 10 + (msg[19] & 0x0F) * 100;

    out->wind_direction_deg  = wind_dir_raw * 0.1f;
    out->wind_gust_meter_sec = gust_raw * 0.1f;
    out->wind_avg_meter_sec  = wind_raw * 0.1f;

    int rain_raw = (msg[23] & 0x0F) + ((msg[23] & 0xF0) >> 4) * 10
                 + (msg[24] & 0x0F) * 100 + ((msg[24] & 0xF0) >> 4) * 1000;
    out->rain_mm = rain_raw * 0.1f;

    // Professional Rain Gauge (types 0x39..0x3B) – rescale rain, no wind/humidity
    if (type_tmp >= 0x39 && type_tmp <= 0x3B) {
        out->rain_mm    *= 2.5f;
        type_tmp         = SENSOR_TYPE_WEATHER0;
        out->humidity_ok = false;
        out->wind_ok     = false;
    } else {
        out->wind_ok     = true;
        out->humidity_ok = (msg[22] & 0x0F) <= 9; // BCD, 0x0F = invalid
    }

    out->s_type   = type_tmp;
    out->temp_ok  = (msg[20] & 0x0F) <= 9;
    out->light_ok = false;
    out->uv_ok    = false;
    out->rain_ok  = true;
    return DECODE_OK;
}
#endif // BRESSER_5_IN_1

// ===========================================================================
// 6-in-1 decoder
// From rtl_433: https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_6in1.c
//
// Data is split across two alternating messages:
//   flags == 0: temperature / humidity / UV
//   flags == 1: rain counter (weather station type 1 only)
// Wind is present in every message.
// ===========================================================================
#ifdef BRESSER_6_IN_1
DecodeStatus decode6In1(const uint8_t *msg, uint8_t msgSize,
                        SensorData *out, float rssi)
{
    (void)msgSize;
    static const int moisture_map[] = {0, 7, 13, 20, 27, 33, 40, 47, 53, 60,
                                       67, 73, 80, 87, 93, 99};

    // LFSR-16 digest, generator 0x8810, key 0x5412
    int chkdgst = (msg[0] << 8) | msg[1];
    int digest  = lfsr_digest16(&msg[2], 15, 0x8810, 0x5412);
    if (chkdgst != digest)
        return DECODE_DIG_ERR;

    // Add-with-carry checksum over bytes [2..17] must equal 0xFF
    if ((add_bytes(&msg[2], 16) & 0xFF) != 0xFF)
        return DECODE_CHK_ERR;

    uint32_t id_tmp   = ((uint32_t)msg[2] << 24) | ((uint32_t)msg[3] << 16)
                       | ((uint32_t)msg[4] << 8) | (uint32_t)msg[5];
    uint8_t  type_tmp = msg[6] >> 4;
    uint8_t  chan_tmp = msg[6] & 0x07;
    uint8_t  flags    = msg[16] & 0x0F;

    // If this is the first packet for this slot, reset per-field flags
    if (!out->valid) {
        out->temp_ok     = false;
        out->humidity_ok = false;
        out->uv_ok       = false;
        out->wind_ok     = false;
        out->rain_ok     = false;
    }

    out->sensor_id  = id_tmp;
    out->s_type     = type_tmp;
    out->chan       = chan_tmp;
    out->startup    = (msg[6] & 0x08) == 0;
    out->battery_ok = (msg[13] >> 1) & 1;

    // --- Temperature / humidity / UV (flags == 0) ---
    bool temp_ok = false, humidity_ok = false, uv_ok = false;
    bool wind_ok = false, rain_ok = false;

    temp_ok = humidity_ok = (flags == 0);
    float temp = 0.0f;
    if (temp_ok) {
        bool sign    = (msg[13] >> 3) & 1;
        int temp_raw = (msg[12] >> 4) * 100 + (msg[12] & 0x0F) * 10 + (msg[13] >> 4);
        temp = (sign ? (temp_raw - 1000) : temp_raw) * 0.1f;
        // Correction for Bresser 3-in-1 Anemometer (negative values encoded differently)
        if (temp < -50.0f)
            temp = -temp_raw * 0.1f;
        out->temp_c   = temp;
        out->humidity = (msg[14] >> 4) * 10 + (msg[14] & 0x0F);

        // UV: inverted BCD, 0xFF01 / 0x0000 if unavailable
        uv_ok = ((~msg[15] & 0xFF) <= 0x99) && ((~msg[16] & 0xF0) <= 0x90);
        if (uv_ok) {
            int uv_raw = ((~msg[15] & 0xF0) >> 4) * 100
                       + (~msg[15] & 0x0F) * 10
                       + ((~msg[16] & 0xF0) >> 4);
            out->uv = uv_raw * 0.1f;
        }
    }

    // --- Wind speed / direction (present in every message) ---
    uint8_t im7 = msg[7] ^ 0xFF;
    uint8_t im8 = msg[8] ^ 0xFF;
    uint8_t im9 = msg[9] ^ 0xFF;
    wind_ok = (im7 <= 0x99) && (im8 <= 0x99) && (im9 <= 0x99);
    if (wind_ok) {
        int gust_raw     = (im7 >> 4) * 100 + (im7 & 0x0F) * 10 + (im8 >> 4);
        int wavg_raw     = (im9 >> 4) * 100 + (im9 & 0x0F) * 10 + (im8 & 0x0F);
        int wind_dir_raw = ((msg[10] & 0xF0) >> 4) * 100 + (msg[10] & 0x0F) * 10
                         + ((msg[11] & 0xF0) >> 4);
        out->wind_gust_meter_sec = gust_raw * 0.1f;
        out->wind_avg_meter_sec  = wavg_raw * 0.1f;
        out->wind_direction_deg  = (float)wind_dir_raw;
    }

    // --- Rain counter (flags == 1, type 1 only) ---
    rain_ok = (flags == 1) && (type_tmp == 1);
    if (rain_ok) {
        uint8_t im12 = msg[12] ^ 0xFF;
        uint8_t im13 = msg[13] ^ 0xFF;
        uint8_t im14 = msg[14] ^ 0xFF;
        int rain_raw = (im12 >> 4) * 100000 + (im12 & 0x0F) * 10000
                     + (im13 >> 4) * 1000   + (im13 & 0x0F) * 100
                     + (im14 >> 4) * 10     + (im14 & 0x0F);
        out->rain_mm = rain_raw * 0.1f;
    }

    // Pool thermometer has no meaningful humidity
    if (out->s_type == SENSOR_TYPE_POOL_THERMO)
        humidity_ok = false;

    // Soil and thermo-hygro sensors have no wind / UV hardware
    if (out->s_type == SENSOR_TYPE_SOIL || out->s_type == SENSOR_TYPE_THERMO_HYGRO) {
        wind_ok = false;
        uv_ok   = false;
    }

    // Soil sensor: humidity field carries moisture index (1-16)
    if (out->s_type == SENSOR_TYPE_SOIL && temp_ok
            && out->humidity >= 1 && out->humidity <= 16) {
        humidity_ok        = false;
        out->soil_moisture = moisture_map[out->humidity - 1];
        out->soil_temp_c   = temp;
    }

    // Accumulate per-field flags across the two alternating messages
    out->temp_ok     |= temp_ok;
    out->humidity_ok |= humidity_ok;
    out->uv_ok       |= uv_ok;
    out->wind_ok     |= wind_ok;
    out->rain_ok     |= rain_ok;
    out->valid        = true;
    out->rssi         = rssi;

    // Weather station type 1 sends data in two separate messages
    if (out->s_type == SENSOR_TYPE_WEATHER1)
        out->complete = out->temp_ok && out->rain_ok;
    else
        out->complete = true;

    return DECODE_OK;
}
#endif // BRESSER_6_IN_1

// ===========================================================================
// 7-in-1 decoder
// From rtl_433: https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_7in1.c
//
// Also covers: Air Quality PM (type 8), CO2 (type 10), HCHO/VOC (type 11),
//              3-in-1 (type 12), 8-in-1 (type 13).
// Data is de-whitened by XOR with 0xAA before processing.
// ===========================================================================
#ifdef BRESSER_7_IN_1
DecodeStatus decode7In1(const uint8_t *msg, uint8_t msgSize,
                        SensorData *out, float rssi)
{
    uint8_t msgw[MSG_BUF_SIZE];
    for (unsigned i = 0; i < msgSize && i < MSG_BUF_SIZE; ++i)
        msgw[i] = msg[i] ^ 0xAA;

    // LFSR-16 digest, generator 0x8810, key 0xba95, final XOR 0x6df1
    int chkdgst = (msgw[0] << 8) | msgw[1];
    int digest  = lfsr_digest16(&msgw[2], 23, 0x8810, 0xba95);
    if ((chkdgst ^ digest) != 0x6DF1)
        return DECODE_DIG_ERR;

    // 7-in-1 sensor ID is 16-bit (matches reference WeatherSensorDecoders.cpp).
    uint16_t id_tmp    = (uint16_t)(((unsigned)msgw[2] << 8) | msgw[3]);
    int      s_type    = msg[6] >> 4; // raw (no de-whitening), matches original
    int      flags     = msgw[15] & 0x0F;
    bool     batt_low  = (flags & 0x06) == 0x06;

    out->sensor_id  = (uint32_t)id_tmp;
    out->s_type     = (uint8_t)s_type;
    out->startup    = (msg[6] & 0x08) == 0x00;
    out->chan       = msg[6] & 0x07;
    out->battery_ok = !batt_low;
    out->valid      = true;
    out->complete   = true;
    out->rssi       = rssi;

    if (s_type == SENSOR_TYPE_WEATHER1 || s_type == SENSOR_TYPE_WEATHER3
            || s_type == SENSOR_TYPE_WEATHER8) {

        bool wind_light_ok = (s_type != SENSOR_TYPE_WEATHER3);

        int wdir     = (msgw[4] >> 4) * 100 + (msgw[4] & 0x0F) * 10 + (msgw[5] >> 4);
        int wgst_raw = (msgw[7] >> 4) * 100 + (msgw[7] & 0x0F) * 10 + (msgw[8] >> 4);
        int wavg_raw = (msgw[8] & 0x0F) * 100 + (msgw[9] >> 4) * 10 + (msgw[9] & 0x0F);
        int rain_raw = (msgw[10] >> 4) * 100000 + (msgw[10] & 0x0F) * 10000
                     + (msgw[11] >> 4) * 1000   + (msgw[11] & 0x0F) * 100
                     + (msgw[12] >> 4) * 10     + (msgw[12] & 0x0F);
        int temp_raw = (msgw[14] >> 4) * 100 + (msgw[14] & 0x0F) * 10 + (msgw[15] >> 4);
        float temp_c = temp_raw * 0.1f;
        if (temp_raw > 600)
            temp_c = (temp_raw - 1000) * 0.1f;
        int humidity  = (msgw[16] >> 4) * 10 + (msgw[16] & 0x0F);
        int lght_raw  = (msgw[17] >> 4) * 100000 + (msgw[17] & 0x0F) * 10000
                      + (msgw[18] >> 4) * 1000   + (msgw[18] & 0x0F) * 100
                      + (msgw[19] >> 4) * 10     + (msgw[19] & 0x0F);
        int uv_raw    = (msgw[20] >> 4) * 100 + (msgw[20] & 0x0F) * 10 + (msgw[21] >> 4);

        out->temp_ok             = true;
        out->humidity_ok         = true;
        out->wind_ok             = wind_light_ok;
        out->rain_ok             = true;
        out->light_ok            = wind_light_ok;
        out->uv_ok               = wind_light_ok;
        out->tglobe_ok           = false;
        out->temp_c              = temp_c;
        out->humidity            = (uint8_t)humidity;
        out->wind_gust_meter_sec = wgst_raw * 0.1f;
        out->wind_avg_meter_sec  = wavg_raw * 0.1f;
        out->wind_direction_deg  = (float)wdir;
        out->rain_mm             = rain_raw * 0.1f;
        out->light_klx           = lght_raw * 0.001f;
        out->uv                  = uv_raw * 0.1f;

        if (s_type == SENSOR_TYPE_WEATHER8) {
            out->tglobe_ok = (msgw[23] >> 4) < 10;
            out->tglobe_c  = (msgw[22] >> 4) * 10.0f + (msgw[22] & 0x0F)
                           + (msgw[23] >> 4) * 0.1f;
        }
    }
    else if (s_type == SENSOR_TYPE_AIR_PM) {
        out->pm_1_0      = (msgw[8]  & 0x0F) * 1000 + (msgw[9]  >> 4) * 100
                         + (msgw[9]  & 0x0F) * 10   + (msgw[10] >> 4);
        out->pm_2_5      = (msgw[10] & 0x0F) * 1000 + (msgw[11] >> 4) * 100
                         + (msgw[11] & 0x0F) * 10   + (msgw[12] >> 4);
        out->pm_10       = (msgw[12] & 0x0F) * 1000 + (msgw[13] >> 4) * 100
                         + (msgw[13] & 0x0F) * 10   + (msgw[14] >> 4);
        out->pm_1_0_init = ((msgw[10] >> 4) & 0x0F) == 0x0F;
        out->pm_2_5_init = ((msgw[12] >> 4) & 0x0F) == 0x0F;
        out->pm_10_init  = ((msgw[14] >> 4) & 0x0F) == 0x0F;
    }
    else if (s_type == SENSOR_TYPE_CO2) {
        out->co2_ppm  = ((msgw[4] & 0xF0) >> 4) * 1000 + (msgw[4] & 0x0F) * 100
                      + ((msgw[5] & 0xF0) >> 4) * 10   + (msgw[5] & 0x0F);
        out->co2_init = (msgw[5] & 0x0F) == 0x0F;
    }
    else if (s_type == SENSOR_TYPE_HCHO_VOC) {
        out->hcho_ppb  = ((msgw[4] & 0xF0) >> 4) * 1000 + (msgw[4] & 0x0F) * 100
                       + ((msgw[5] & 0xF0) >> 4) * 10   + (msgw[5] & 0x0F);
        out->voc_level = msgw[22] & 0x0F;
        out->hcho_init = (msgw[5] & 0x0F) == 0x0F;
        out->voc_init  = (msgw[22] == 0x0F);
    }

    return DECODE_OK;
}
#endif // BRESSER_7_IN_1

// ===========================================================================
// Lightning sensor decoder
// https://github.com/merbanan/rtl_433/issues/2140
// Data de-whitened by XOR with 0xAA.
// Digest: LFSR-16 gen 0x8810 key 0xabf9 final XOR 0x899e
// ===========================================================================
#ifdef BRESSER_LIGHTNING
DecodeStatus decodeLightning(const uint8_t *msg, uint8_t msgSize,
                             SensorData *out, float rssi)
{
    (void)msgSize;

    uint8_t msgw[MSG_BUF_SIZE];
    for (unsigned i = 0; i < msgSize && i < MSG_BUF_SIZE; ++i)
        msgw[i] = msg[i] ^ 0xAA;

    // LFSR-16 digest, generator 0x8810, key 0xabf9, final XOR 0x899e
    int chk    = (msgw[0] << 8) | msgw[1];
    int digest = lfsr_digest16(&msgw[2], 8, 0x8810, 0xabf9);
    if ((chk ^ digest) != 0x899E)
        return DECODE_DIG_ERR;

    // Lightning sensor ID is 16-bit (matches reference WeatherSensorDecoders.cpp).
    uint16_t id_tmp    = (uint16_t)(((unsigned)msgw[2] << 8) | msgw[3]);
    int      s_type    = msg[6] >> 4;
    bool     startup   = (msg[6] & 0x08) == 0x00;
    uint16_t ctr       = (msgw[4] >> 4) * 100 + (msgw[4] & 0x0F) * 10 + (msgw[5] >> 4);
    bool     batt_low  = (msgw[5] & 0x08) == 0x00;
    uint16_t unknown1  = (uint16_t)(((unsigned)(msgw[5] & 0x0F) << 8) | msgw[6]);
    uint8_t  dist_km   = msgw[7];
    uint16_t unknown2  = (uint16_t)(((unsigned)msgw[8] << 8) | msgw[9]);

    out->sensor_id       = (uint32_t)id_tmp;
    out->s_type          = (uint8_t)s_type;
    out->startup         = startup;
    out->chan            = 0;
    out->battery_ok      = !batt_low;
    out->rssi            = rssi;
    out->valid           = true;
    out->complete        = true;
    out->lgt_strike_count = ctr;
    out->lgt_distance_km  = dist_km;
    out->lgt_unknown1     = unknown1;
    out->lgt_unknown2     = unknown2;
    return DECODE_OK;
}
#endif // BRESSER_LIGHTNING

// ===========================================================================
// Water leakage sensor decoder
// https://github.com/matthias-bs/BresserWeatherSensorReceiver/issues/77
// First two bytes are CRC16/XMODEM over bytes [2..6].
// ===========================================================================
#ifdef BRESSER_LEAKAGE
DecodeStatus decodeLeakage(const uint8_t *msg, uint8_t msgSize,
                           SensorData *out, float rssi)
{
    (void)msgSize;

    uint16_t crc_act = crc16(&msg[2], 5, 0x1021, 0x0000);
    uint16_t crc_exp = ((uint16_t)msg[0] << 8) | msg[1];
    if (crc_act != crc_exp)
        return DECODE_CHK_ERR;

    uint32_t id_tmp   = ((uint32_t)msg[2] << 24) | ((uint32_t)msg[3] << 16)
                       | ((uint32_t)msg[4] << 8) | msg[5];
    uint8_t  type_tmp = msg[6] >> 4;
    uint8_t  chan_tmp = msg[6] & 0x07;
    bool     alarm    = (msg[7] & 0x80) == 0x80;
    bool     no_alarm = (msg[7] & 0x40) == 0x40;

    // Sanity: must be leakage type, alarm and no_alarm must differ, channel != 0
    if (type_tmp != SENSOR_TYPE_LEAKAGE || alarm == no_alarm || chan_tmp == 0)
        return DECODE_INVALID;

    out->sensor_id  = id_tmp;
    out->s_type     = type_tmp;
    out->chan       = chan_tmp;
    out->startup    = (msg[6] & 0x08) == 0x00;
    out->battery_ok = (msg[7] & 0x30) != 0x00;
    out->rssi       = rssi;
    out->valid      = true;
    out->complete   = true;
    out->leak_alarm = alarm && !no_alarm;
    return DECODE_OK;
}
#endif // BRESSER_LEAKAGE

// ===========================================================================
// decodeMessage – try all enabled decoders in priority order
// (matches WeatherSensor::decodeMessage() order)
// ===========================================================================
DecodeStatus decodeMessage(const uint8_t *msg, uint8_t msgSize,
                           SensorData *out, float rssi)
{
    DecodeStatus res = DECODE_INVALID;

#ifdef BRESSER_7_IN_1
    res = decode7In1(msg, msgSize, out, rssi);
    if (res == DECODE_OK || res == DECODE_FULL || res == DECODE_SKIP)
        return res;
#endif
#ifdef BRESSER_6_IN_1
    res = decode6In1(msg, msgSize, out, rssi);
    if (res == DECODE_OK || res == DECODE_FULL || res == DECODE_SKIP)
        return res;
#endif
#ifdef BRESSER_5_IN_1
    res = decode5In1(msg, msgSize, out, rssi);
    if (res == DECODE_OK || res == DECODE_FULL || res == DECODE_SKIP)
        return res;
#endif
#ifdef BRESSER_LIGHTNING
    res = decodeLightning(msg, msgSize, out, rssi);
    if (res == DECODE_OK || res == DECODE_FULL || res == DECODE_SKIP)
        return res;
#endif
#ifdef BRESSER_LEAKAGE
    res = decodeLeakage(msg, msgSize, out, rssi);
#endif
    return res;
}

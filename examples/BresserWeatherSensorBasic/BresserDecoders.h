///////////////////////////////////////////////////////////////////////////////////////////////////
// BresserDecoders.h
//
// Standalone Bresser weather sensor decoder functions for use with SX1276_Radio_Lite.
//
// Adapted from BresserWeatherSensorReceiver by Matthias Prinke:
// https://github.com/matthias-bs/BresserWeatherSensorReceiver
//
// The original decoders are class members of WeatherSensor and depend on RadioLib /
// ESP-IDF logging. Here they are plain C++ functions operating on a single SensorData
// struct – no slot management, no dynamic allocation, no RadioLib required.
//
// MIT License
// Copyright (c) 2022-2026 Matthias Prinke
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef BRESSER_DECODERS_H
#define BRESSER_DECODERS_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Enable / disable individual decoders (comment out what you don't need)
// ---------------------------------------------------------------------------
#define BRESSER_5_IN_1
#define BRESSER_6_IN_1
#define BRESSER_7_IN_1
#define BRESSER_LIGHTNING
#define BRESSER_LEAKAGE

// Message buffer size (matches MSG_BUF_SIZE in WeatherSensor.h)
#define MSG_BUF_SIZE 27

// ---------------------------------------------------------------------------
// Sensor type identifiers
// ---------------------------------------------------------------------------
#define SENSOR_TYPE_WEATHER0     0   // 5-in-1 weather station
#define SENSOR_TYPE_WEATHER1     1   // 6-in-1 weather station / 7-in-1
#define SENSOR_TYPE_THERMO_HYGRO 2   // Thermo-/Hygro sensor
#define SENSOR_TYPE_POOL_THERMO  3   // Pool / Spa thermometer
#define SENSOR_TYPE_SOIL         4   // Soil temperature + moisture
#define SENSOR_TYPE_LEAKAGE      5   // Water leakage sensor
#define SENSOR_TYPE_AIR_PM       8   // Air quality PM2.5/PM10
#define SENSOR_TYPE_RAIN         9   // Professional rain gauge (5-in-1 decoder)
#define SENSOR_TYPE_LIGHTNING    9   // Lightning sensor
#define SENSOR_TYPE_CO2          10  // CO2 sensor
#define SENSOR_TYPE_HCHO_VOC     11  // HCHO / VOC sensor
#define SENSOR_TYPE_WEATHER3     12  // 3-in-1 professional wind gauge
#define SENSOR_TYPE_WEATHER8     13  // 8-in-1 weather station

// ---------------------------------------------------------------------------
// Decode status
// ---------------------------------------------------------------------------
typedef enum DecodeStatus {
    DECODE_INVALID,   // No decoder matched
    DECODE_OK,        // Successfully decoded
    DECODE_PAR_ERR,   // Parity error (5-in-1)
    DECODE_CHK_ERR,   // Checksum / CRC error
    DECODE_DIG_ERR,   // Digest error (6-in-1, 7-in-1, lightning)
    DECODE_SKIP,      // Sensor in exclude list / not in include list
    DECODE_FULL       // No free slot (not used here, kept for completeness)
} DecodeStatus;

// ---------------------------------------------------------------------------
// Sensor data – flat struct, no union, no dynamic allocation
// ---------------------------------------------------------------------------
struct SensorData {
    // Common fields
    uint32_t sensor_id;
    float    rssi;
    uint8_t  s_type;
    uint8_t  chan;
    bool     startup;
    bool     battery_ok;
    bool     valid;
    bool     complete;

    // Weather / climate
    bool    temp_ok;
    bool    tglobe_ok;
    bool    humidity_ok;
    bool    light_ok;
    bool    uv_ok;
    bool    wind_ok;
    bool    rain_ok;
    float   temp_c;
    float   tglobe_c;
    float   light_klx;
    float   uv;
    float   rain_mm;
    float   wind_direction_deg;
    float   wind_gust_meter_sec;
    float   wind_avg_meter_sec;
    uint8_t humidity;

    // Soil
    float   soil_temp_c;
    uint8_t soil_moisture;

    // Lightning
    uint8_t  lgt_distance_km;
    uint16_t lgt_strike_count;
    uint16_t lgt_unknown1;
    uint16_t lgt_unknown2;

    // Water leakage
    bool leak_alarm;

    // Air quality – particulate matter
    uint16_t pm_1_0;
    uint16_t pm_2_5;
    uint16_t pm_10;
    bool     pm_1_0_init;
    bool     pm_2_5_init;
    bool     pm_10_init;

    // CO2
    uint16_t co2_ppm;
    bool     co2_init;

    // HCHO / VOC
    uint16_t hcho_ppb;
    uint8_t  voc_level;
    bool     hcho_init;
    bool     voc_init;
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * Try all enabled decoders on a received payload.
 *
 * @param msg     Payload bytes *after* the leading 0xD4 sync byte
 *                (i.e. buffer[1..26], length MSG_BUF_SIZE-1 = 26)
 * @param msgSize Length of msg (pass sizeof(buffer)-1)
 * @param out     SensorData struct to fill; must be zero-initialised by caller
 * @param rssi    RSSI value in dBm to store in out->rssi
 * @return DECODE_OK on success, otherwise the error from the last attempted decoder
 */
DecodeStatus decodeMessage(const uint8_t *msg, uint8_t msgSize,
                           SensorData *out, float rssi);

// Individual decoders (exposed for direct use if needed)
#ifdef BRESSER_5_IN_1
DecodeStatus decode5In1(const uint8_t *msg, uint8_t msgSize,
                        SensorData *out, float rssi);
#endif
#ifdef BRESSER_6_IN_1
DecodeStatus decode6In1(const uint8_t *msg, uint8_t msgSize,
                        SensorData *out, float rssi);
#endif
#ifdef BRESSER_7_IN_1
DecodeStatus decode7In1(const uint8_t *msg, uint8_t msgSize,
                        SensorData *out, float rssi);
#endif
#ifdef BRESSER_LIGHTNING
DecodeStatus decodeLightning(const uint8_t *msg, uint8_t msgSize,
                             SensorData *out, float rssi);
#endif
#ifdef BRESSER_LEAKAGE
DecodeStatus decodeLeakage(const uint8_t *msg, uint8_t msgSize,
                           SensorData *out, float rssi);
#endif

#endif // BRESSER_DECODERS_H

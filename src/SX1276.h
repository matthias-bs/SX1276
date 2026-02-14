/**
 * SX1276.h
 * 
 * Small SX1276 LoRa library for Arduino
 * Simplified port of RadioLib optimized for memory-constrained devices
 * 
 * Copyright (c) 2024 Matthias Prinke
 * Licensed under MIT License
 */

#ifndef SX1276_H
#define SX1276_H

#include <Arduino.h>
#include <SPI.h>

// Optional LoRa support - define this to enable LoRa modulation
// #define LORA_ENABLED

// Debugging support - define to enable debug output
// #define SX1276_DEBUG

// Debug macro for memory-efficient logging
#ifdef SX1276_DEBUG
  #define SX1276_DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
  #define SX1276_DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
  #define SX1276_DEBUG_PRINT(...)
  #define SX1276_DEBUG_PRINTLN(...)
#endif

// SX1276 Register Map
#define SX1276_REG_FIFO                         0x00
#define SX1276_REG_OP_MODE                      0x01
#define SX1276_REG_FRF_MSB                      0x06
#define SX1276_REG_FRF_MID                      0x07
#define SX1276_REG_FRF_LSB                      0x08
#define SX1276_REG_PA_CONFIG                    0x09
#define SX1276_REG_PA_RAMP                      0x0A
#define SX1276_REG_OCP                          0x0B
#define SX1276_REG_LNA                          0x0C
#define SX1276_REG_FIFO_ADDR_PTR                0x0D
#define SX1276_REG_FIFO_TX_BASE_ADDR            0x0E
#define SX1276_REG_FIFO_RX_BASE_ADDR            0x0F
#define SX1276_REG_FIFO_RX_CURRENT_ADDR         0x10
#define SX1276_REG_IRQ_FLAGS_MASK               0x11
#define SX1276_REG_IRQ_FLAGS                    0x12
#define SX1276_REG_RX_NB_BYTES                  0x13
#define SX1276_REG_PKT_SNR_VALUE                0x19
#define SX1276_REG_PKT_RSSI_VALUE               0x1A
#define SX1276_REG_RSSI_VALUE                   0x1B
#define SX1276_REG_MODEM_CONFIG_1               0x1D
#define SX1276_REG_MODEM_CONFIG_2               0x1E
#define SX1276_REG_PREAMBLE_MSB                 0x20
#define SX1276_REG_PREAMBLE_LSB                 0x21
#define SX1276_REG_PAYLOAD_LENGTH               0x22
#define SX1276_REG_MODEM_CONFIG_3               0x26
#define SX1276_REG_FREQ_ERROR_MSB               0x28
#define SX1276_REG_FREQ_ERROR_MID               0x29
#define SX1276_REG_FREQ_ERROR_LSB               0x2A
#define SX1276_REG_RSSI_WIDEBAND                0x2C
#define SX1276_REG_DETECTION_OPTIMIZE           0x31
#define SX1276_REG_INVERT_IQ                    0x33
#define SX1276_REG_DETECTION_THRESHOLD          0x37
#define SX1276_REG_SYNC_WORD                    0x39
#define SX1276_REG_DIO_MAPPING_1                0x40
#define SX1276_REG_VERSION                      0x42
#define SX1276_REG_TCXO                         0x4B
#define SX1276_REG_PA_DAC                       0x4D

// Operating Modes
#define SX1276_MODE_SLEEP                       0x00
#define SX1276_MODE_STDBY                       0x01
#define SX1276_MODE_FSTX                        0x02
#define SX1276_MODE_TX                          0x03
#define SX1276_MODE_FSRX                        0x04
#define SX1276_MODE_RX_CONTINUOUS               0x05
#define SX1276_MODE_RX_SINGLE                   0x06
#define SX1276_MODE_CAD                         0x07

// Long Range Mode (LoRa)
#define SX1276_LORA_MODE                        0x80
#define SX1276_FSK_OOK_MODE                     0x00

// PA Configuration
#define SX1276_PA_BOOST                         0x80
#define SX1276_PA_OUTPUT_RFO_PIN                0x00
#define SX1276_MAX_POWER                        0x70
#define SX1276_OUTPUT_POWER                     0x0F

// IRQ Flags
#define SX1276_IRQ_CAD_DETECTED                 0x01
#define SX1276_IRQ_FHSS_CHANGE_CHANNEL          0x02
#define SX1276_IRQ_CAD_DONE                     0x04
#define SX1276_IRQ_TX_DONE                      0x08
#define SX1276_IRQ_VALID_HEADER                 0x10
#define SX1276_IRQ_PAYLOAD_CRC_ERROR            0x20
#define SX1276_IRQ_RX_DONE                      0x40
#define SX1276_IRQ_RX_TIMEOUT                   0x80

// LoRa Bandwidth
#define SX1276_BW_7_8_KHZ                       0x00
#define SX1276_BW_10_4_KHZ                      0x10
#define SX1276_BW_15_6_KHZ                      0x20
#define SX1276_BW_20_8_KHZ                      0x30
#define SX1276_BW_31_25_KHZ                     0x40
#define SX1276_BW_41_7_KHZ                      0x50
#define SX1276_BW_62_5_KHZ                      0x60
#define SX1276_BW_125_KHZ                       0x70
#define SX1276_BW_250_KHZ                       0x80
#define SX1276_BW_500_KHZ                       0x90

// LoRa Coding Rate
#define SX1276_CR_4_5                           0x02
#define SX1276_CR_4_6                           0x04
#define SX1276_CR_4_7                           0x06
#define SX1276_CR_4_8                           0x08

// LoRa Spreading Factor
#define SX1276_SF_6                             0x06
#define SX1276_SF_7                             0x07
#define SX1276_SF_8                             0x08
#define SX1276_SF_9                             0x09
#define SX1276_SF_10                            0x0A
#define SX1276_SF_11                            0x0B
#define SX1276_SF_12                            0x0C

// Error codes
#define SX1276_ERR_NONE                         0
#define SX1276_ERR_CHIP_NOT_FOUND               -1
#define SX1276_ERR_PACKET_TOO_LONG              -2
#define SX1276_ERR_TX_TIMEOUT                   -3
#define SX1276_ERR_RX_TIMEOUT                   -4
#define SX1276_ERR_CRC_MISMATCH                 -5
#define SX1276_ERR_INVALID_BANDWIDTH            -6
#define SX1276_ERR_INVALID_SPREADING_FACTOR     -7
#define SX1276_ERR_INVALID_CODING_RATE          -8
#define SX1276_ERR_INVALID_FREQUENCY            -9
#define SX1276_ERR_INVALID_OUTPUT_POWER         -10

// Constants
#define SX1276_MAX_PACKET_LENGTH                255
#define SX1276_FIFO_SIZE                        256
#define SX1276_FXOSC                            32000000L  // 32 MHz crystal
#define SX1276_FSTEP                            (SX1276_FXOSC / 524288.0)  // FXOSC / 2^19

/**
 * SX1276 class - flat hierarchy, no inheritance
 */
class SX1276 {
public:
    /**
     * Constructor
     */
    SX1276();
    
    /**
     * Initialize the SX1276 module
     * @param freq Frequency in Hz (e.g., 915000000 for 915 MHz)
     * @param cs Chip select pin
     * @param rst Reset pin
     * @param dio0 DIO0 pin (used for TX/RX done interrupts)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t begin(long freq, int cs, int rst, int dio0);
    
    /**
     * Shutdown the module
     */
    void end();
    
    /**
     * Transmit data
     * @param data Pointer to data buffer
     * @param len Length of data (max SX1276_MAX_PACKET_LENGTH)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t transmit(const uint8_t* data, size_t len);
    
    /**
     * Receive data (blocking)
     * @param data Pointer to buffer to store received data
     * @param maxLen Maximum length of buffer
     * @return Number of bytes received, or error code (< 0)
     */
    int16_t receive(uint8_t* data, size_t maxLen);
    
    /**
     * Set carrier frequency
     * @param freq Frequency in Hz
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setFrequency(long freq);
    
    /**
     * Set output power
     * @param power Output power in dBm (2-17 for PA_BOOST, -1-14 for RFO)
     * @param useBoost Use PA_BOOST output (true) or RFO output (false)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setPower(int8_t power, bool useBoost = true);
    
#ifdef LORA_ENABLED
    /**
     * Set LoRa bandwidth
     * @param bw Bandwidth (use SX1276_BW_* constants)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setBandwidth(uint8_t bw);
    
    /**
     * Set LoRa spreading factor
     * @param sf Spreading factor (6-12)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setSpreadingFactor(uint8_t sf);
    
    /**
     * Set LoRa coding rate
     * @param cr Coding rate (use SX1276_CR_* constants)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setCodingRate(uint8_t cr);
    
    /**
     * Set preamble length
     * @param len Preamble length in symbols
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setPreambleLength(uint16_t len);
    
    /**
     * Set sync word
     * @param sw Sync word (0x12 for private networks, 0x34 for LoRaWAN)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setSyncWord(uint8_t sw);
    
    /**
     * Enable or disable CRC
     * @param enable Enable CRC if true
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setCRC(bool enable);
    
    /**
     * Get RSSI of last received packet
     * @return RSSI in dBm
     */
    int16_t getRSSI();
    
    /**
     * Get SNR of last received packet
     * @return SNR in dB (scaled by 4, divide by 4.0 for actual SNR)
     */
    int8_t getSNR();
    
    /**
     * Get frequency error of last received packet
     * @return Frequency error in Hz
     */
    int32_t getFrequencyError();
#endif
    
    /**
     * Set module to standby mode
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t standby();
    
    /**
     * Set module to sleep mode
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t sleep();
    
    /**
     * Read a register
     * @param addr Register address
     * @return Register value
     */
    uint8_t readRegister(uint8_t addr);
    
    /**
     * Write to a register
     * @param addr Register address
     * @param value Value to write
     */
    void writeRegister(uint8_t addr, uint8_t value);

private:
    // Pin assignments
    int _csPin;
    int _rstPin;
    int _dio0Pin;
    
    // Current configuration
    uint32_t _freq;
    int8_t _power;
    bool _useBoost;
    
    // LoRa configuration (if enabled)
#ifdef LORA_ENABLED
    uint8_t _bw;
    uint8_t _sf;
    uint8_t _cr;
    uint16_t _preambleLength;
    uint8_t _syncWord;
    bool _crcEnabled;
#endif
    
    // SPI communication helpers
    void spiBegin();
    void spiEnd();
    uint8_t spiTransfer(uint8_t data);
    
    // Module control
    int16_t reset();
    int16_t setMode(uint8_t mode);
    int16_t config();
    
    // Wait for mode ready
    void waitForModeReady();
};

#endif // SX1276_H

/**
 * SX1276.h
 * 
 * SX1276_Radio_Lite - Lightweight SX1276 radio library for Arduino
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
#define LORA_ENABLED

// Optional FSK/OOK support - define this to enable FSK/OOK modulation  
#define FSK_OOK_ENABLED

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

// SX1276 Register Map (Common and LoRa-specific)
#define SX1276_REG_FIFO                         0x00
#define SX1276_REG_OP_MODE                      0x01
#define SX1276_REG_FRF_MSB                      0x06
#define SX1276_REG_FRF_MID                      0x07
#define SX1276_REG_FRF_LSB                      0x08
#define SX1276_REG_PA_CONFIG                    0x09
#define SX1276_REG_PA_RAMP                      0x0A
#define SX1276_REG_OCP                          0x0B
#define SX1276_REG_LNA                          0x0C
#define SX1276_REG_FIFO_ADDR_PTR                0x0D  // LoRa mode only
#define SX1276_REG_FIFO_TX_BASE_ADDR            0x0E  // LoRa mode only
#define SX1276_REG_FIFO_RX_BASE_ADDR            0x0F  // LoRa mode only
#define SX1276_REG_FIFO_RX_CURRENT_ADDR         0x10  // LoRa mode only
#define SX1276_REG_IRQ_FLAGS_MASK               0x11
#define SX1276_REG_IRQ_FLAGS                    0x12
#define SX1276_REG_RX_NB_BYTES                  0x13  // LoRa mode only

// FSK/OOK Specific Registers
#define SX1276_REG_BITRATE_MSB                  0x02
#define SX1276_REG_BITRATE_LSB                  0x03
#define SX1276_REG_FDEV_MSB                     0x04
#define SX1276_REG_FDEV_LSB                     0x05
#define SX1276_REG_RX_CONFIG                    0x0D  // FSK/OOK mode only
#define SX1276_REG_RSSI_CONFIG                  0x0E
#define SX1276_REG_RSSI_COLLISION               0x0F
#define SX1276_REG_RSSI_THRESH                  0x10
#define SX1276_REG_RSSI_VALUE_FSK               0x11  // FSK/OOK mode only
#define SX1276_REG_RX_BW                        0x12
#define SX1276_REG_AFC_BW                       0x13
#define SX1276_REG_OOK_PEAK                     0x14
#define SX1276_REG_OOK_FIX                      0x15
#define SX1276_REG_OOK_AVG                      0x16
#define SX1276_REG_AFC_FEI                      0x1A
#define SX1276_REG_AFC_MSB                      0x1B
#define SX1276_REG_AFC_LSB                      0x1C
#define SX1276_REG_FEI_MSB                      0x1D
#define SX1276_REG_FEI_LSB                      0x1E
#define SX1276_REG_PREAMBLE_DETECT              0x1F
#define SX1276_REG_RX_TIMEOUT_1                 0x20
#define SX1276_REG_RX_TIMEOUT_2                 0x21
#define SX1276_REG_RX_TIMEOUT_3                 0x22
#define SX1276_REG_RX_DELAY                     0x23
#define SX1276_REG_OSC                          0x24
#define SX1276_REG_PREAMBLE_MSB_FSK             0x25
#define SX1276_REG_PREAMBLE_LSB_FSK             0x26
#define SX1276_REG_SYNC_CONFIG                  0x27
#define SX1276_REG_SYNC_VALUE_1                 0x28
#define SX1276_REG_SYNC_VALUE_2                 0x29
#define SX1276_REG_SYNC_VALUE_3                 0x2A
#define SX1276_REG_SYNC_VALUE_4                 0x2B
#define SX1276_REG_SYNC_VALUE_5                 0x2C
#define SX1276_REG_SYNC_VALUE_6                 0x2D
#define SX1276_REG_SYNC_VALUE_7                 0x2E
#define SX1276_REG_SYNC_VALUE_8                 0x2F
#define SX1276_REG_PACKET_CONFIG_1              0x30
#define SX1276_REG_PACKET_CONFIG_2              0x31
#define SX1276_REG_PAYLOAD_LENGTH_FSK           0x32
#define SX1276_REG_NODE_ADRS                    0x33
#define SX1276_REG_BROADCAST_ADRS               0x34
#define SX1276_REG_FIFO_THRESH                  0x35
#define SX1276_REG_SEQ_CONFIG_1                 0x36
#define SX1276_REG_SEQ_CONFIG_2                 0x37
#define SX1276_REG_TIMER_RESOL                  0x38
#define SX1276_REG_TIMER1_COEF                  0x39
#define SX1276_REG_TIMER2_COEF                  0x3A
#define SX1276_REG_IMAGE_CAL                    0x3B
#define SX1276_REG_TEMP                         0x3C
#define SX1276_REG_LOW_BAT                      0x3D
#define SX1276_REG_IRQ_FLAGS_1                  0x3E
#define SX1276_REG_IRQ_FLAGS_2                  0x3F
// LoRa Specific Registers (when in LoRa mode)
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

// Common Registers
#define SX1276_REG_DIO_MAPPING_1                0x40
#define SX1276_REG_DIO_MAPPING_2                0x41
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

// Modulation Type
#define SX1276_LORA_MODE                        0x80
#define SX1276_FSK_OOK_MODE                     0x00

// Modulation Type Selection
#define SX1276_MODULATION_FSK                   0x00
#define SX1276_MODULATION_OOK                   0x01
#define SX1276_MODULATION_LORA                  0x02

// FSK/OOK IRQ Flags (registers 0x3E and 0x3F)
#define SX1276_IRQ1_MODE_READY                  0x80
#define SX1276_IRQ1_RX_READY                    0x40
#define SX1276_IRQ1_TX_READY                    0x20
#define SX1276_IRQ1_PLL_LOCK                    0x10
#define SX1276_IRQ1_RSSI                        0x08
#define SX1276_IRQ1_TIMEOUT                     0x04
#define SX1276_IRQ1_PREAMBLE_DETECT             0x02
#define SX1276_IRQ1_SYNC_ADDRESS_MATCH          0x01

#define SX1276_IRQ2_FIFO_FULL                   0x80
#define SX1276_IRQ2_FIFO_EMPTY                  0x40
#define SX1276_IRQ2_FIFO_LEVEL                  0x20
#define SX1276_IRQ2_FIFO_OVERRUN                0x10
#define SX1276_IRQ2_PACKET_SENT                 0x08
#define SX1276_IRQ2_PAYLOAD_READY               0x04
#define SX1276_IRQ2_CRC_OK                      0x02
#define SX1276_IRQ2_LOW_BAT                     0x01

// PA Configuration
#define SX1276_PA_BOOST                         0x80
#define SX1276_PA_OUTPUT_RFO_PIN                0x00
#define SX1276_MAX_POWER                        0x70
#define SX1276_OUTPUT_POWER                     0x0F

// LoRa IRQ Flags
#define SX1276_IRQ_CAD_DETECTED                 0x01
#define SX1276_IRQ_FHSS_CHANGE_CHANNEL          0x02
#define SX1276_IRQ_CAD_DONE                     0x04
#define SX1276_IRQ_TX_DONE                      0x08
#define SX1276_IRQ_VALID_HEADER                 0x10
#define SX1276_IRQ_PAYLOAD_CRC_ERROR            0x20
#define SX1276_IRQ_RX_DONE                      0x40
#define SX1276_IRQ_RX_TIMEOUT                   0x80

// FSK/OOK RX Bandwidth values
#define SX1276_RX_BW_2_6_KHZ                    0x17
#define SX1276_RX_BW_3_1_KHZ                    0x0F
#define SX1276_RX_BW_3_9_KHZ                    0x07
#define SX1276_RX_BW_5_2_KHZ                    0x16
#define SX1276_RX_BW_6_3_KHZ                    0x0E
#define SX1276_RX_BW_7_8_KHZ_FSK                0x06
#define SX1276_RX_BW_10_4_KHZ_FSK               0x15
#define SX1276_RX_BW_12_5_KHZ                   0x0D
#define SX1276_RX_BW_15_6_KHZ_FSK               0x05
#define SX1276_RX_BW_20_8_KHZ_FSK               0x14
#define SX1276_RX_BW_25_0_KHZ                   0x0C
#define SX1276_RX_BW_31_3_KHZ                   0x04
#define SX1276_RX_BW_41_7_KHZ_FSK               0x13
#define SX1276_RX_BW_50_0_KHZ                   0x0B
#define SX1276_RX_BW_62_5_KHZ_FSK               0x03
#define SX1276_RX_BW_83_3_KHZ                   0x12
#define SX1276_RX_BW_100_0_KHZ                  0x0A
#define SX1276_RX_BW_125_0_KHZ_FSK              0x02
#define SX1276_RX_BW_166_7_KHZ                  0x11
#define SX1276_RX_BW_200_0_KHZ                  0x09
#define SX1276_RX_BW_250_0_KHZ_FSK              0x01

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
#define SX1276_ERR_INVALID_BITRATE              -11
#define SX1276_ERR_INVALID_FREQUENCY_DEVIATION  -12
#define SX1276_ERR_INVALID_SYNC_WORD            -13
#define SX1276_ERR_WRONG_MODEM                  -14

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
     * Constructor with pin configuration (RadioLib-compatible)
     * @param cs Chip select pin
     * @param irq DIO0 pin (interrupt/GPIO)
     * @param rst Reset pin
     * @param gpio Additional GPIO pin (optional, defaults to -1 for unused)
     */
    SX1276(int cs, int irq, int rst, int gpio = -1);
    
    /**
     * Initialize the SX1276 module (simplified API)
     * @param freq Frequency in Hz (e.g., 915000000 for 915 MHz)
     * @param cs Chip select pin
     * @param rst Reset pin
     * @param dio0 DIO0 pin (used for TX/RX done interrupts)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t begin(long freq, int cs, int rst, int dio0);
    
#ifdef LORA_ENABLED
    /**
     * Initialize the SX1276 module in LoRa mode (RadioLib-compatible)
     * @param freq Carrier frequency in MHz (e.g., 915.0 for 915 MHz)
     * @param bw LoRa bandwidth in kHz (default: 125.0 kHz)
     * @param sf LoRa spreading factor (default: 9, range: 6-12)
     * @param cr LoRa coding rate denominator (default: 7, range: 5-8)
     * @param syncWord LoRa sync word (default: 0x12 for private networks, 0x34 for LoRaWAN)
     * @param power Transmission output power in dBm (default: 10, range: 2-17)
     * @param preambleLength Length of LoRa preamble in symbols (default: 8)
     * @param gain Receiver LNA gain (default: 0 for automatic gain control)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t begin(float freq = 434.0, float bw = 125.0, uint8_t sf = 9, uint8_t cr = 7, 
                  uint8_t syncWord = 0x12, int8_t power = 10, uint16_t preambleLength = 8, uint8_t gain = 0);
#endif
    
#ifdef FSK_OOK_ENABLED
    /**
     * Initialize the SX1276 module in FSK/OOK mode (RadioLib-compatible)
     * @param freq Carrier frequency in MHz (e.g., 434.0 for 434 MHz)
     * @param br Bit rate in kbps (default: 4.8 kbps)
     * @param freqDev Frequency deviation in kHz (default: 5.0 kHz, set to 0 for OOK)
     * @param rxBw Receiver bandwidth in kHz (default: 125.0 kHz)
     * @param power Transmission output power in dBm (default: 10, range: 2-17)
     * @param preambleLength Length of FSK/OOK preamble in bytes (default: 5 bytes, min: 3)
     * @param enableOOK Use OOK modulation instead of FSK (default: false)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t beginFSK(float freq = 434.0, float br = 4.8, float freqDev = 5.0, float rxBw = 125.0, 
                     int8_t power = 10, uint16_t preambleLength = 5, bool enableOOK = false);
#endif
    
    /**
     * Set modulation type
     * @param modulation Modulation type (SX1276_MODULATION_FSK, SX1276_MODULATION_OOK, or SX1276_MODULATION_LORA)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setModulation(uint8_t modulation);
    
    /**
     * Set modem type (RadioLib-compatible alias for setModulation)
     * @param modem Modem type (SX1276_MODULATION_FSK, SX1276_MODULATION_OOK, or SX1276_MODULATION_LORA)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setModem(uint8_t modem);
    
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
     * Set carrier frequency (simplified API with Hz)
     * @param freq Frequency in Hz
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setFrequency(long freq);
    
    /**
     * Set carrier frequency (RadioLib-compatible with MHz)
     * @param freq Frequency in MHz (e.g., 915.0 for 915 MHz)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setFrequency(float freq);
    
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
#endif
    
#if defined(LORA_ENABLED) || defined(FSK_OOK_ENABLED)
    /**
     * Set preamble length (works for both LoRa and FSK/OOK modes)
     * @param len Preamble length in symbols (LoRa) or bytes (FSK/OOK)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setPreambleLength(uint16_t len);
#endif
    
#ifdef LORA_ENABLED
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
    
#ifdef FSK_OOK_ENABLED
    /**
     * Set FSK/OOK bit rate
     * @param bitrate Bit rate in bps (1200-300000)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setBitrate(uint32_t bitrate);
    
    /**
     * Set FSK frequency deviation (FSK only, set to 0 for OOK)
     * @param freqDev Frequency deviation in Hz (600-200000)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setFrequencyDeviation(uint32_t freqDev);
    
    /**
     * Set FSK/OOK RX bandwidth
     * @param rxBw RX bandwidth (use SX1276_RX_BW_* constants)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setRxBandwidth(uint8_t rxBw);
    
    /**
     * Set FSK/OOK sync word
     * @param syncWord Pointer to sync word bytes
     * @param len Length of sync word (1-8 bytes)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setSyncWord(const uint8_t* syncWord, uint8_t len);
    
    /**
     * Set packet format
     * @param fixedLength Use fixed length packets (true) or variable length (false)
     * @param crcOn Enable CRC (true) or disable (false)
     * @return Error code (SX1276_ERR_NONE on success)
     */
    int16_t setPacketConfig(bool fixedLength, bool crcOn);
    
    /**
     * Get RSSI value in FSK/OOK mode
     * @return RSSI in dBm
     */
    int16_t getRSSI_FSK();
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
    uint8_t _modulation;  // Current modulation type
    
    // LoRa configuration (if enabled)
#ifdef LORA_ENABLED
    uint8_t _bw;
    uint8_t _sf;
    uint8_t _cr;
    uint16_t _preambleLength;
    uint8_t _syncWord;
    bool _crcEnabled;
#endif
    
    // FSK/OOK configuration (if enabled)
#ifdef FSK_OOK_ENABLED
    uint32_t _bitrate;
    uint32_t _freqDev;
    uint8_t _rxBw;
    uint8_t _syncWordFSK[8];
    uint8_t _syncWordLen;
    uint16_t _preambleLengthFSK;
    bool _fixedLength;
    bool _crcOnFSK;
#endif
    
    // SPI communication helpers
    void spiBegin();
    void spiEnd();
    uint8_t spiTransfer(uint8_t data);
    
    // Module control
    int16_t reset();
    int16_t setMode(uint8_t mode);
    int16_t config();
    
#ifdef FSK_OOK_ENABLED
    int16_t configFSK();
#endif
    
    // Wait for mode ready
    void waitForModeReady();
};

#endif // SX1276_H

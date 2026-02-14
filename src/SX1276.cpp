/**
 * SX1276.cpp
 * 
 * Small SX1276 LoRa library for Arduino
 * Simplified port of RadioLib optimized for memory-constrained devices
 * 
 * Copyright (c) 2024 Matthias Prinke
 * Licensed under MIT License
 */

#include "SX1276.h"

/**
 * Constructor
 */
SX1276::SX1276() {
    _csPin = -1;
    _rstPin = -1;
    _dio0Pin = -1;
    _freq = 0;
    _power = 17;
    _useBoost = true;
    
#ifdef LORA_ENABLED
    _bw = SX1276_BW_125_KHZ;
    _sf = SX1276_SF_7;
    _cr = SX1276_CR_4_5;
    _preambleLength = 8;
    _syncWord = 0x12;  // Private network
    _crcEnabled = true;
#endif
}

/**
 * Initialize the SX1276 module
 */
int16_t SX1276::begin(long freq, int cs, int rst, int dio0) {
    // Store pin assignments
    _csPin = cs;
    _rstPin = rst;
    _dio0Pin = dio0;
    _freq = freq;
    
    // Initialize pins
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    
    pinMode(_rstPin, OUTPUT);
    pinMode(_dio0Pin, INPUT);
    
    // Initialize SPI
    SPI.begin();
    
    // Reset the module
    int16_t state = reset();
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Check version register
    uint8_t version = readRegister(SX1276_REG_VERSION);
    if (version != 0x12) {
        SX1276_DEBUG_PRINT(F("SX1276: Chip version mismatch, expected 0x12, got 0x"));
        SX1276_DEBUG_PRINTLN(version, HEX);
        return SX1276_ERR_CHIP_NOT_FOUND;
    }
    
    SX1276_DEBUG_PRINTLN(F("SX1276: Chip found"));
    
    // Configure the module
    state = config();
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    return SX1276_ERR_NONE;
}

/**
 * Shutdown the module
 */
void SX1276::end() {
    sleep();
    SPI.end();
}

/**
 * Reset the module
 */
int16_t SX1276::reset() {
    // Perform reset sequence
    digitalWrite(_rstPin, LOW);
    delay(10);
    digitalWrite(_rstPin, HIGH);
    delay(10);
    
    return SX1276_ERR_NONE;
}

/**
 * Configure the module
 */
int16_t SX1276::config() {
    int16_t state = SX1276_ERR_NONE;
    
#ifdef LORA_ENABLED
    // Set to sleep mode for configuration (LoRa mode)
    state = sleep();
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set LoRa mode
    writeRegister(SX1276_REG_OP_MODE, SX1276_MODE_SLEEP | SX1276_LORA_MODE);
    delay(10);
    
    // Set to standby mode
    state = standby();
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set frequency
    state = setFrequency(_freq);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set FIFO base addresses
    writeRegister(SX1276_REG_FIFO_TX_BASE_ADDR, 0x00);
    writeRegister(SX1276_REG_FIFO_RX_BASE_ADDR, 0x00);
    
    // Set LNA boost
    writeRegister(SX1276_REG_LNA, readRegister(SX1276_REG_LNA) | 0x03);
    
    // Set auto AGC
    writeRegister(SX1276_REG_MODEM_CONFIG_3, 0x04);
    
    // Set output power
    state = setPower(_power, _useBoost);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set OCP to 240mA
    writeRegister(SX1276_REG_OCP, 0x20 | 0x1B);
    
    // Set LoRa parameters
    state = setBandwidth(_bw);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    state = setSpreadingFactor(_sf);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    state = setCodingRate(_cr);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    state = setPreambleLength(_preambleLength);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    state = setSyncWord(_syncWord);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    state = setCRC(_crcEnabled);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set DIO0 to TxDone/RxDone
    writeRegister(SX1276_REG_DIO_MAPPING_1, 0x00);
    
#else
    // FSK/OOK mode configuration would go here
    // For now, just set to sleep mode
    state = sleep();
#endif
    
    return state;
}

/**
 * Set carrier frequency
 */
int16_t SX1276::setFrequency(long freq) {
    // Check frequency range (137-1020 MHz)
    if (freq < 137000000L || freq > 1020000000L) {
        return SX1276_ERR_INVALID_FREQUENCY;
    }
    
    _freq = freq;
    
    // Calculate frequency register value
    // FRF = (Freq × 2^19) / FXOSC
    uint32_t frf = ((uint64_t)freq << 19) / SX1276_FXOSC;
    
    // Write frequency registers
    writeRegister(SX1276_REG_FRF_MSB, (frf >> 16) & 0xFF);
    writeRegister(SX1276_REG_FRF_MID, (frf >> 8) & 0xFF);
    writeRegister(SX1276_REG_FRF_LSB, frf & 0xFF);
    
    return SX1276_ERR_NONE;
}

/**
 * Set output power
 */
int16_t SX1276::setPower(int8_t power, bool useBoost) {
    _power = power;
    _useBoost = useBoost;
    
    uint8_t paConfig = 0;
    uint8_t paDac = 0x84;  // Default +17dBm
    
    if (useBoost) {
        // PA_BOOST pin
        if (power > 17) {
            // Enable high power mode (+20dBm)
            if (power > 20) {
                power = 20;
            }
            paDac = 0x87;
            power -= 3;
        } else if (power < 2) {
            power = 2;
        }
        paConfig = SX1276_PA_BOOST | (power - 2);
    } else {
        // RFO pin
        if (power > 14) {
            power = 14;
        } else if (power < -1) {
            power = -1;
        }
        paConfig = SX1276_MAX_POWER | (power + 1);
    }
    
    writeRegister(SX1276_REG_PA_CONFIG, paConfig);
    writeRegister(SX1276_REG_PA_DAC, paDac);
    
    return SX1276_ERR_NONE;
}

/**
 * Transmit data
 */
int16_t SX1276::transmit(const uint8_t* data, size_t len) {
    if (len > SX1276_MAX_PACKET_LENGTH) {
        return SX1276_ERR_PACKET_TOO_LONG;
    }
    
    // Set to standby mode
    int16_t state = standby();
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
#ifdef LORA_ENABLED
    // Set DIO0 to TxDone
    writeRegister(SX1276_REG_DIO_MAPPING_1, 0x40);
    
    // Clear IRQ flags
    writeRegister(SX1276_REG_IRQ_FLAGS, 0xFF);
    
    // Set FIFO pointer to TX base
    writeRegister(SX1276_REG_FIFO_ADDR_PTR, 0x00);
    
    // Write data to FIFO
    spiBegin();
    spiTransfer(SX1276_REG_FIFO | 0x80);
    for (size_t i = 0; i < len; i++) {
        spiTransfer(data[i]);
    }
    spiEnd();
    
    // Set payload length
    writeRegister(SX1276_REG_PAYLOAD_LENGTH, len);
    
    // Start transmission
    state = setMode(SX1276_MODE_TX);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Wait for TX done (with timeout)
    uint32_t start = millis();
    while (digitalRead(_dio0Pin) == LOW) {
        if (millis() - start > 5000) {
            standby();
            return SX1276_ERR_TX_TIMEOUT;
        }
        yield();
    }
    
    // Clear IRQ flags
    writeRegister(SX1276_REG_IRQ_FLAGS, 0xFF);
    
    // Set back to standby
    state = standby();
    
#endif
    
    return state;
}

/**
 * Receive data (blocking)
 */
int16_t SX1276::receive(uint8_t* data, size_t maxLen) {
#ifdef LORA_ENABLED
    // Set to standby mode
    int16_t state = standby();
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set DIO0 to RxDone
    writeRegister(SX1276_REG_DIO_MAPPING_1, 0x00);
    
    // Clear IRQ flags
    writeRegister(SX1276_REG_IRQ_FLAGS, 0xFF);
    
    // Set FIFO pointer to RX base
    writeRegister(SX1276_REG_FIFO_ADDR_PTR, 0x00);
    
    // Start reception
    state = setMode(SX1276_MODE_RX_CONTINUOUS);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Wait for RX done (with timeout)
    uint32_t start = millis();
    while (digitalRead(_dio0Pin) == LOW) {
        if (millis() - start > 10000) {
            standby();
            return SX1276_ERR_RX_TIMEOUT;
        }
        yield();
    }
    
    // Check for CRC error
    uint8_t irqFlags = readRegister(SX1276_REG_IRQ_FLAGS);
    if (irqFlags & SX1276_IRQ_PAYLOAD_CRC_ERROR) {
        writeRegister(SX1276_REG_IRQ_FLAGS, 0xFF);
        standby();
        return SX1276_ERR_CRC_MISMATCH;
    }
    
    // Get packet length
    uint8_t len = readRegister(SX1276_REG_RX_NB_BYTES);
    if (len > maxLen) {
        len = maxLen;
    }
    
    // Set FIFO pointer to last packet
    uint8_t fifoAddr = readRegister(SX1276_REG_FIFO_RX_CURRENT_ADDR);
    writeRegister(SX1276_REG_FIFO_ADDR_PTR, fifoAddr);
    
    // Read data from FIFO
    spiBegin();
    spiTransfer(SX1276_REG_FIFO);
    for (size_t i = 0; i < len; i++) {
        data[i] = spiTransfer(0x00);
    }
    spiEnd();
    
    // Clear IRQ flags
    writeRegister(SX1276_REG_IRQ_FLAGS, 0xFF);
    
    // Set back to standby
    standby();
    
    return len;
#else
    return SX1276_ERR_NONE;
#endif
}

#ifdef LORA_ENABLED
/**
 * Set LoRa bandwidth
 */
int16_t SX1276::setBandwidth(uint8_t bw) {
    // Validate bandwidth
    if (bw > SX1276_BW_500_KHZ) {
        return SX1276_ERR_INVALID_BANDWIDTH;
    }
    
    _bw = bw;
    
    // Read current config
    uint8_t config1 = readRegister(SX1276_REG_MODEM_CONFIG_1);
    
    // Clear BW bits and set new value
    config1 = (config1 & 0x0F) | bw;
    
    writeRegister(SX1276_REG_MODEM_CONFIG_1, config1);
    
    return SX1276_ERR_NONE;
}

/**
 * Set LoRa spreading factor
 */
int16_t SX1276::setSpreadingFactor(uint8_t sf) {
    // Validate spreading factor
    if (sf < SX1276_SF_6 || sf > SX1276_SF_12) {
        return SX1276_ERR_INVALID_SPREADING_FACTOR;
    }
    
    _sf = sf;
    
    // Read current config
    uint8_t config2 = readRegister(SX1276_REG_MODEM_CONFIG_2);
    
    // Clear SF bits and set new value
    config2 = (config2 & 0x0F) | (sf << 4);
    
    writeRegister(SX1276_REG_MODEM_CONFIG_2, config2);
    
    // Set detection optimize and detection threshold for SF6
    if (sf == SX1276_SF_6) {
        writeRegister(SX1276_REG_DETECTION_OPTIMIZE, 0x05);
        writeRegister(SX1276_REG_DETECTION_THRESHOLD, 0x0C);
    } else {
        writeRegister(SX1276_REG_DETECTION_OPTIMIZE, 0x03);
        writeRegister(SX1276_REG_DETECTION_THRESHOLD, 0x0A);
    }
    
    return SX1276_ERR_NONE;
}

/**
 * Set LoRa coding rate
 */
int16_t SX1276::setCodingRate(uint8_t cr) {
    // Validate coding rate
    if (cr < SX1276_CR_4_5 || cr > SX1276_CR_4_8) {
        return SX1276_ERR_INVALID_CODING_RATE;
    }
    
    _cr = cr;
    
    // Read current config
    uint8_t config1 = readRegister(SX1276_REG_MODEM_CONFIG_1);
    
    // Clear CR bits and set new value
    config1 = (config1 & 0xF1) | cr;
    
    writeRegister(SX1276_REG_MODEM_CONFIG_1, config1);
    
    return SX1276_ERR_NONE;
}

/**
 * Set preamble length
 */
int16_t SX1276::setPreambleLength(uint16_t len) {
    _preambleLength = len;
    
    writeRegister(SX1276_REG_PREAMBLE_MSB, (len >> 8) & 0xFF);
    writeRegister(SX1276_REG_PREAMBLE_LSB, len & 0xFF);
    
    return SX1276_ERR_NONE;
}

/**
 * Set sync word
 */
int16_t SX1276::setSyncWord(uint8_t sw) {
    _syncWord = sw;
    
    writeRegister(SX1276_REG_SYNC_WORD, sw);
    
    return SX1276_ERR_NONE;
}

/**
 * Enable or disable CRC
 */
int16_t SX1276::setCRC(bool enable) {
    _crcEnabled = enable;
    
    // Read current config
    uint8_t config2 = readRegister(SX1276_REG_MODEM_CONFIG_2);
    
    if (enable) {
        config2 |= 0x04;
    } else {
        config2 &= 0xFB;
    }
    
    writeRegister(SX1276_REG_MODEM_CONFIG_2, config2);
    
    return SX1276_ERR_NONE;
}

/**
 * Get RSSI of last received packet
 */
int16_t SX1276::getRSSI() {
    uint8_t rawRSSI = readRegister(SX1276_REG_PKT_RSSI_VALUE);
    
    // Adjust based on frequency band
    int16_t rssi;
    if (_freq < 862000000L) {
        // LF band
        rssi = -164 + rawRSSI;
    } else {
        // HF band
        rssi = -157 + rawRSSI;
    }
    
    return rssi;
}

/**
 * Get SNR of last received packet
 */
int8_t SX1276::getSNR() {
    return (int8_t)readRegister(SX1276_REG_PKT_SNR_VALUE);
}

/**
 * Get frequency error of last received packet
 */
int32_t SX1276::getFrequencyError() {
    uint8_t msb = readRegister(SX1276_REG_FREQ_ERROR_MSB);
    uint8_t mid = readRegister(SX1276_REG_FREQ_ERROR_MID);
    uint8_t lsb = readRegister(SX1276_REG_FREQ_ERROR_LSB);
    
    // Combine into 20-bit value
    uint32_t rawError = ((uint32_t)(msb & 0x0F) << 16) | ((uint32_t)mid << 8) | lsb;
    
    // Handle sign bit
    if (rawError & 0x80000) {
        rawError |= 0xFFF00000;
    }
    
    // Calculate frequency error
    // FreqError = (FreqErrorReg × 2^24) / (FXOSC × BW_Hz)
    // Approximate calculation to avoid floating point
    int32_t bwHz;
    switch (_bw) {
        case SX1276_BW_7_8_KHZ: bwHz = 7800; break;
        case SX1276_BW_10_4_KHZ: bwHz = 10400; break;
        case SX1276_BW_15_6_KHZ: bwHz = 15600; break;
        case SX1276_BW_20_8_KHZ: bwHz = 20800; break;
        case SX1276_BW_31_25_KHZ: bwHz = 31250; break;
        case SX1276_BW_41_7_KHZ: bwHz = 41700; break;
        case SX1276_BW_62_5_KHZ: bwHz = 62500; break;
        case SX1276_BW_125_KHZ: bwHz = 125000; break;
        case SX1276_BW_250_KHZ: bwHz = 250000; break;
        case SX1276_BW_500_KHZ: bwHz = 500000; break;
        default: bwHz = 125000; break;
    }
    
    int32_t freqError = ((int32_t)rawError * bwHz) / 524288L;  // 2^19
    
    return freqError;
}
#endif

/**
 * Set module to standby mode
 */
int16_t SX1276::standby() {
    return setMode(SX1276_MODE_STDBY);
}

/**
 * Set module to sleep mode
 */
int16_t SX1276::sleep() {
#ifdef LORA_ENABLED
    return setMode(SX1276_MODE_SLEEP | SX1276_LORA_MODE);
#else
    return setMode(SX1276_MODE_SLEEP);
#endif
}

/**
 * Set operating mode
 */
int16_t SX1276::setMode(uint8_t mode) {
    writeRegister(SX1276_REG_OP_MODE, mode);
    waitForModeReady();
    return SX1276_ERR_NONE;
}

/**
 * Wait for mode to be ready
 */
void SX1276::waitForModeReady() {
    // Small delay for mode switching
    delay(2);
}

/**
 * Read a register
 */
uint8_t SX1276::readRegister(uint8_t addr) {
    spiBegin();
    spiTransfer(addr & 0x7F);  // Read: MSB = 0
    uint8_t value = spiTransfer(0x00);
    spiEnd();
    return value;
}

/**
 * Write to a register
 */
void SX1276::writeRegister(uint8_t addr, uint8_t value) {
    spiBegin();
    spiTransfer(addr | 0x80);  // Write: MSB = 1
    spiTransfer(value);
    spiEnd();
}

/**
 * Begin SPI transaction
 */
void SX1276::spiBegin() {
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);
}

/**
 * End SPI transaction
 */
void SX1276::spiEnd() {
    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();
}

/**
 * Transfer a byte via SPI
 */
uint8_t SX1276::spiTransfer(uint8_t data) {
    return SPI.transfer(data);
}

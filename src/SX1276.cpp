/**
 * SX1276.cpp
 * 
 * SX1276_Radio_Lite - Lightweight SX1276 radio library for Arduino
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
    
    // Set default modulation based on what's compiled in
#if defined(LORA_ENABLED)
    _modulation = SX1276_MODULATION_LORA;
#elif defined(FSK_OOK_ENABLED)
    _modulation = SX1276_MODULATION_FSK;
#else
    _modulation = SX1276_MODULATION_LORA;  // Fallback
#endif
    
#ifdef LORA_ENABLED
    _bw = SX1276_BW_125_KHZ;
    _sf = SX1276_SF_7;
    _cr = SX1276_CR_4_5;
    _preambleLength = 8;
    _syncWord = 0x12;  // Private network
    _crcEnabled = true;
#endif

#ifdef FSK_OOK_ENABLED
    _bitrate = 4800;  // Default 4.8 kbps
    _freqDev = 5000;  // Default 5 kHz
    _rxBw = SX1276_RX_BW_10_4_KHZ_FSK;
    _syncWordFSK[0] = 0x12;
    _syncWordFSK[1] = 0xAD;
    _syncWordLen = 2;
    _preambleLengthFSK = 5;  // 5 bytes (40 bits) - typical for FSK
    _fixedLength = false;  // Variable length
    _crcOnFSK = true;
    _lastRSSI = 0;
#endif
}

/**
 * Constructor with pin configuration (RadioLib-compatible)
 */
SX1276::SX1276(int cs, int irq, int rst, int gpio) {
    (void)gpio;  // Unused parameter - reserved for future use
    _csPin = cs;
    _rstPin = rst;
    _dio0Pin = irq;  // DIO0 is the primary interrupt pin
    _freq = 0;
    _power = 17;
    _useBoost = true;
    
    // Set default modulation based on what's compiled in
#if defined(LORA_ENABLED)
    _modulation = SX1276_MODULATION_LORA;
#elif defined(FSK_OOK_ENABLED)
    _modulation = SX1276_MODULATION_FSK;
#else
    _modulation = SX1276_MODULATION_LORA;  // Fallback
#endif
    
#ifdef LORA_ENABLED
    _bw = SX1276_BW_125_KHZ;
    _sf = SX1276_SF_7;
    _cr = SX1276_CR_4_5;
    _preambleLength = 8;
    _syncWord = 0x12;  // Private network
    _crcEnabled = true;
#endif

#ifdef FSK_OOK_ENABLED
    _bitrate = 4800;  // Default 4.8 kbps
    _freqDev = 5000;  // Default 5 kHz
    _rxBw = SX1276_RX_BW_10_4_KHZ_FSK;
    _syncWordFSK[0] = 0x12;
    _syncWordFSK[1] = 0xAD;
    _syncWordLen = 2;
    _preambleLengthFSK = 5;  // 5 bytes (40 bits) - typical for FSK
    _fixedLength = false;  // Variable length
    _crcOnFSK = true;
    _lastRSSI = 0;
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
 * Set modulation type
 */
int16_t SX1276::setModulation(uint8_t modulation) {
    if (modulation > SX1276_MODULATION_LORA) {
        return SX1276_ERR_WRONG_MODEM;
    }
    
    _modulation = modulation;
    
    // Reconfigure the module with new modulation
    return config();
}

/**
 * Set modem type (RadioLib-compatible alias)
 */
int16_t SX1276::setModem(uint8_t modem) {
    return setModulation(modem);
}

#ifdef LORA_ENABLED
/**
 * Initialize in LoRa mode (RadioLib-compatible)
 */
int16_t SX1276::begin(float freq, float bw, uint8_t sf, uint8_t cr, 
                      uint8_t syncWord, int8_t power, uint16_t preambleLength, uint8_t gain) {
    (void)gain;  // Gain setting not yet implemented
    
    // Check if pins were configured via constructor
    if (_csPin < 0 || _rstPin < 0 || _dio0Pin < 0) {
        return SX1276_ERR_CHIP_NOT_FOUND;  // Pins not configured
    }
    
    // Convert frequency from MHz to Hz
    long freqHz = (long)(freq * 1000000.0);
    
    // Initialize hardware
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    pinMode(_rstPin, OUTPUT);
    pinMode(_dio0Pin, INPUT);
    
    SPI.begin();
    
    // Reset the module
    int16_t state = reset();
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Check version register
    uint8_t version = readRegister(SX1276_REG_VERSION);
    if (version != 0x12) {
        return SX1276_ERR_CHIP_NOT_FOUND;
    }
    
    // Set LoRa mode
    _modulation = SX1276_MODULATION_LORA;
    _freq = freqHz;
    _power = power;
    
    // Configure LoRa parameters from arguments
    // Convert bandwidth from kHz to register value
    if (bw == 7.8) _bw = SX1276_BW_7_8_KHZ;
    else if (bw == 10.4) _bw = SX1276_BW_10_4_KHZ;
    else if (bw == 15.6) _bw = SX1276_BW_15_6_KHZ;
    else if (bw == 20.8) _bw = SX1276_BW_20_8_KHZ;
    else if (bw == 31.25) _bw = SX1276_BW_31_25_KHZ;
    else if (bw == 41.7) _bw = SX1276_BW_41_7_KHZ;
    else if (bw == 62.5) _bw = SX1276_BW_62_5_KHZ;
    else if (bw == 125.0) _bw = SX1276_BW_125_KHZ;
    else if (bw == 250.0) _bw = SX1276_BW_250_KHZ;
    else if (bw == 500.0) _bw = SX1276_BW_500_KHZ;
    else _bw = SX1276_BW_125_KHZ;  // Default
    
    _sf = sf;
    _cr = ((cr - 5) << 1);  // Convert denominator (5-8) to register value (0x02-0x08)
    _preambleLength = preambleLength;
    _syncWord = syncWord;
    _crcEnabled = true;
    
    // Configure the module
    state = config();
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    return SX1276_ERR_NONE;
}
#endif

#ifdef FSK_OOK_ENABLED
/**
 * Initialize in FSK/OOK mode (RadioLib-compatible)
 */
int16_t SX1276::beginFSK(float freq, float br, float freqDev, float rxBw, 
                         int8_t power, uint16_t preambleLength, bool enableOOK) {
    // Check if pins were configured via constructor
    if (_csPin < 0 || _rstPin < 0 || _dio0Pin < 0) {
        return SX1276_ERR_CHIP_NOT_FOUND;  // Pins not configured
    }
    
    // Convert frequency from MHz to Hz
    long freqHz = (long)(freq * 1000000.0);
    
    // Initialize hardware
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    pinMode(_rstPin, OUTPUT);
    pinMode(_dio0Pin, INPUT);
    
    SPI.begin();
    
    // Reset the module
    int16_t state = reset();
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Check version register
    uint8_t version = readRegister(SX1276_REG_VERSION);
    if (version != 0x12) {
        return SX1276_ERR_CHIP_NOT_FOUND;
    }
    
    // Set FSK or OOK mode
    _modulation = enableOOK ? SX1276_MODULATION_OOK : SX1276_MODULATION_FSK;
    _freq = freqHz;
    _power = power;
    
    // Configure FSK/OOK parameters from arguments
    _bitrate = (uint32_t)(br * 1000.0);  // Convert kbps to bps
    _freqDev = (uint32_t)(freqDev * 1000.0);  // Convert kHz to Hz
    _preambleLengthFSK = preambleLength;
    
    // Convert RX bandwidth from kHz to register value
    // Simplified mapping - use closest value
    if (rxBw <= 2.6) _rxBw = SX1276_RX_BW_2_6_KHZ;
    else if (rxBw <= 3.9) _rxBw = SX1276_RX_BW_3_9_KHZ;
    else if (rxBw <= 5.2) _rxBw = SX1276_RX_BW_5_2_KHZ;
    else if (rxBw <= 7.8) _rxBw = SX1276_RX_BW_7_8_KHZ_FSK;
    else if (rxBw <= 10.4) _rxBw = SX1276_RX_BW_10_4_KHZ_FSK;
    else if (rxBw <= 15.6) _rxBw = SX1276_RX_BW_15_6_KHZ_FSK;
    else if (rxBw <= 20.8) _rxBw = SX1276_RX_BW_20_8_KHZ_FSK;
    else if (rxBw <= 31.3) _rxBw = SX1276_RX_BW_31_3_KHZ;
    else if (rxBw <= 41.7) _rxBw = SX1276_RX_BW_41_7_KHZ_FSK;
    else if (rxBw <= 62.5) _rxBw = SX1276_RX_BW_62_5_KHZ_FSK;
    else if (rxBw <= 125.0) _rxBw = SX1276_RX_BW_125_0_KHZ_FSK;
    else _rxBw = SX1276_RX_BW_250_0_KHZ_FSK;
    
    // Configure the module
    state = config();
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    return SX1276_ERR_NONE;
}
#endif

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
    
#if defined(LORA_ENABLED) && defined(FSK_OOK_ENABLED)
    // Both modes available - check which one is selected
    if (_modulation == SX1276_MODULATION_LORA) {
#endif

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
        
        // Set frequency (cast to long to avoid ambiguity with float overload)
        state = setFrequency((long)_freq);
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
#endif

#if defined(LORA_ENABLED) && defined(FSK_OOK_ENABLED)
    } else {
        // FSK/OOK mode
        return configFSK();
    }
#elif defined(FSK_OOK_ENABLED)
    // Only FSK/OOK mode available
    return configFSK();
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
 * Set carrier frequency (RadioLib-compatible with MHz)
 */
int16_t SX1276::setFrequency(float freq) {
    // Convert MHz to Hz and call the Hz version
    long freqHz = (long)(freq * 1000000.0);
    return setFrequency(freqHz);
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
    if (_modulation == SX1276_MODULATION_LORA) {
        // LoRa mode transmit
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
    }
#endif

#ifdef FSK_OOK_ENABLED
    if (_modulation == SX1276_MODULATION_FSK || _modulation == SX1276_MODULATION_OOK) {
        // FSK/OOK mode transmit
        // Set payload length register (used for both fixed and variable modes)
        writeRegister(SX1276_REG_PAYLOAD_LENGTH_FSK, len);
        
        // Write data to FIFO
        spiBegin();
        spiTransfer(SX1276_REG_FIFO | 0x80);
        
        // For variable length mode, write length byte first
        if (!_fixedLength) {
            spiTransfer(len);
        }
        
        // Write payload data
        for (size_t i = 0; i < len; i++) {
            spiTransfer(data[i]);
        }
        spiEnd();
        
        // Start transmission
        state = setMode(SX1276_MODE_TX);
        if (state != SX1276_ERR_NONE) {
            return state;
        }
        
        // Wait for TX done (PacketSent flag in IRQ_FLAGS_2)
        uint32_t start = millis();
        while (!(readRegister(SX1276_REG_IRQ_FLAGS_2) & SX1276_IRQ2_PACKET_SENT)) {
            if (millis() - start > 5000) {
                standby();
                return SX1276_ERR_TX_TIMEOUT;
            }
            yield();
        }
        
        // Set back to standby
        state = standby();
    }
#endif

#if !defined(LORA_ENABLED) && !defined(FSK_OOK_ENABLED)
    // Suppress unused parameter warnings when no modulation is enabled
    (void)data;
    (void)len;
#endif
    
    return state;
}

/**
 * Receive data (blocking)
 */
int16_t SX1276::receive(uint8_t* data, size_t maxLen) {
#ifdef LORA_ENABLED
    if (_modulation == SX1276_MODULATION_LORA) {
        // LoRa mode receive
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
    }
#endif

#ifdef FSK_OOK_ENABLED
    if (_modulation == SX1276_MODULATION_FSK || _modulation == SX1276_MODULATION_OOK) {
        // FSK/OOK mode receive
        // Set to standby mode
        int16_t state = standby();
        if (state != SX1276_ERR_NONE) {
            return state;
        }
        
        // Clear IRQ flags before starting reception
        writeRegister(SX1276_REG_IRQ_FLAGS_1, 0xFF);
        writeRegister(SX1276_REG_IRQ_FLAGS_2, 0xFF);
        
        // Start reception
        state = setMode(SX1276_MODE_RX_CONTINUOUS);
        if (state != SX1276_ERR_NONE) {
            return state;
        }
        
        // Wait for PayloadReady flag (with timeout)
        uint32_t start = millis();
        while (!(readRegister(SX1276_REG_IRQ_FLAGS_2) & SX1276_IRQ2_PAYLOAD_READY)) {
            if (millis() - start > 10000) {
                standby();
                return SX1276_ERR_RX_TIMEOUT;
            }
            yield();
        }
        
        // Check for CRC error (if enabled)
        if (_crcOnFSK) {
            uint8_t irqFlags2 = readRegister(SX1276_REG_IRQ_FLAGS_2);
            if (!(irqFlags2 & SX1276_IRQ2_CRC_OK)) {
                standby();
                return SX1276_ERR_CRC_MISMATCH;
            }
        }
        
        // Read RSSI while still in RX mode (must be done before standby)
        uint8_t rawRSSI = readRegister(SX1276_REG_RSSI_VALUE_FSK);
        _lastRSSI = -(rawRSSI / 2);  // Cache RSSI in dBm
        
        // Get packet length and read data
        uint8_t len;
        if (_fixedLength) {
            // Fixed length mode - length is from register
            len = readRegister(SX1276_REG_PAYLOAD_LENGTH_FSK);
            if (len > maxLen) {
                len = maxLen;
            }
            
            // Read data from FIFO
            spiBegin();
            spiTransfer(SX1276_REG_FIFO);
            for (size_t i = 0; i < len; i++) {
                data[i] = spiTransfer(0x00);
            }
            spiEnd();
        } else {
            // Variable length mode - first byte in FIFO is length
            // Read length and data in one transaction
            spiBegin();
            spiTransfer(SX1276_REG_FIFO);
            len = spiTransfer(0x00);  // First byte is length
            
            if (len > maxLen) {
                len = maxLen;
            }
            
            // Read payload data
            for (size_t i = 0; i < len; i++) {
                data[i] = spiTransfer(0x00);
            }
            spiEnd();
        }
        
        // Set back to standby
        standby();
        
        return len;
    }
#endif

#if !defined(LORA_ENABLED) && !defined(FSK_OOK_ENABLED)
    // Suppress unused parameter warnings when no modulation is enabled
    (void)data;
    (void)maxLen;
#endif
    
    return SX1276_ERR_WRONG_MODEM;
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
 * Set preamble length (works for both LoRa and FSK/OOK modes)
 */
int16_t SX1276::setPreambleLength(uint16_t len) {
#ifdef LORA_ENABLED
    if (_modulation == SX1276_MODULATION_LORA) {
        _preambleLength = len;
        
        writeRegister(SX1276_REG_PREAMBLE_MSB, (len >> 8) & 0xFF);
        writeRegister(SX1276_REG_PREAMBLE_LSB, len & 0xFF);
        
        return SX1276_ERR_NONE;
    }
#endif

#ifdef FSK_OOK_ENABLED
    if (_modulation == SX1276_MODULATION_FSK || _modulation == SX1276_MODULATION_OOK) {
        _preambleLengthFSK = len;
        
        writeRegister(SX1276_REG_PREAMBLE_MSB_FSK, (len >> 8) & 0xFF);
        writeRegister(SX1276_REG_PREAMBLE_LSB_FSK, len & 0xFF);
        
        return SX1276_ERR_NONE;
    }
#endif

    return SX1276_ERR_WRONG_MODEM;
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
#if defined(LORA_ENABLED) && defined(FSK_OOK_ENABLED)
    // Both modes available - check which one
    if (_modulation == SX1276_MODULATION_LORA) {
        return setMode(SX1276_MODE_SLEEP | SX1276_LORA_MODE);
    } else {
        return setMode(SX1276_MODE_SLEEP | SX1276_FSK_OOK_MODE);
    }
#elif defined(LORA_ENABLED)
    return setMode(SX1276_MODE_SLEEP | SX1276_LORA_MODE);
#elif defined(FSK_OOK_ENABLED)
    return setMode(SX1276_MODE_SLEEP | SX1276_FSK_OOK_MODE);
#else
    return setMode(SX1276_MODE_SLEEP);
#endif
}

/**
 * Set operating mode
 */
int16_t SX1276::setMode(uint8_t mode) {
    // Preserve modulation-select bits (LoRa / FSK-OOK) unless explicitly overridden.
    // This prevents unintended modulation changes when callers pass only SX1276_MODE_*.
    const uint8_t modulationMask = SX1276_LORA_MODE | SX1276_FSK_OOK_MODE;

    // Modulation bits requested by the caller (if any).
    uint8_t requestedModulation = mode & modulationMask;

    if (requestedModulation == 0) {
        // Caller did not specify modulation bits: preserve current modulation.
        uint8_t currentOpMode = readRegister(SX1276_REG_OP_MODE);
        requestedModulation = currentOpMode & modulationMask;
    }

    // Clear modulation bits from the passed mode and OR in the desired modulation.
    uint8_t newOpMode = (mode & ~modulationMask) | requestedModulation;

    writeRegister(SX1276_REG_OP_MODE, newOpMode);
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

#ifdef FSK_OOK_ENABLED
/**
 * Configure FSK/OOK mode
 */
int16_t SX1276::configFSK() {
    int16_t state = SX1276_ERR_NONE;
    
    // Set to sleep mode for configuration
    writeRegister(SX1276_REG_OP_MODE, SX1276_MODE_SLEEP | SX1276_FSK_OOK_MODE);
    delay(10);
    
    // Set modulation type (FSK or OOK)
    uint8_t opMode = readRegister(SX1276_REG_OP_MODE);
    if (_modulation == SX1276_MODULATION_OOK) {
        opMode |= 0x20;  // Set OOK bit
    } else {
        opMode &= ~0x20;  // Clear OOK bit for FSK
    }
    writeRegister(SX1276_REG_OP_MODE, opMode);
    
    // Set to standby mode
    state = standby();
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set frequency (cast to long to avoid ambiguity with float overload)
    state = setFrequency((long)_freq);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set bitrate
    state = setBitrate(_bitrate);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set frequency deviation (FSK only)
    if (_modulation == SX1276_MODULATION_FSK) {
        state = setFrequencyDeviation(_freqDev);
        if (state != SX1276_ERR_NONE) {
            return state;
        }
    }
    
    // Set RX bandwidth
    state = setRxBandwidth(_rxBw);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set AFC bandwidth (same as RX bandwidth)
    writeRegister(SX1276_REG_AFC_BW, _rxBw);
    
    // Set output power
    state = setPower(_power, _useBoost);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set OCP to 120mA (safer for FSK/OOK)
    writeRegister(SX1276_REG_OCP, 0x20 | 0x0F);
    
    // Set RSSI threshold to -127.5 dBm (0xFF) - essentially no threshold
    // This allows reception of weak signals
    writeRegister(SX1276_REG_RSSI_THRESH, 0xFF);
    
    // Configure RX_CONFIG: AGC auto on, AFC/AGC trigger on RSSI interrupt
    // Bit 7: RestartRxOnCollision = 0 (off)
    // Bit 6: RestartRxWithoutPLLLock = 0
    // Bit 5: RestartRxWithPLLLock = 0
    // Bit 4: AfcAutoOn = 0 (off initially, can be enabled if needed)
    // Bit 3: AgcAutoOn = 1 (AGC auto on)
    // Bits 2-0: AfcAgcTrigger = 001 (RSSI interrupt)
    writeRegister(SX1276_REG_RX_CONFIG, 0x08 | 0x01);  // AGC auto + RSSI trigger
    
    // Reset FIFO overrun flag
    writeRegister(SX1276_REG_IRQ_FLAGS_2, SX1276_IRQ2_FIFO_OVERRUN);
    
    // Disable Rx timeouts to prevent premature timeout errors
    // These must be disabled for reliable packet reception
    writeRegister(SX1276_REG_RX_TIMEOUT_1, 0x00);  // Disable RSSI timeout
    writeRegister(SX1276_REG_RX_TIMEOUT_2, 0x00);  // Disable preamble timeout
    writeRegister(SX1276_REG_RX_TIMEOUT_3, 0x00);  // Disable sync timeout
    
    // Set preamble detector (3 bytes minimum)
    writeRegister(SX1276_REG_PREAMBLE_DETECT, 0xAA);
    
    // Set preamble length
    state = setPreambleLength(_preambleLengthFSK);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set sync word
    state = setSyncWord(_syncWordFSK, _syncWordLen);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set packet configuration
    state = setPacketConfig(_fixedLength, _crcOnFSK);
    if (state != SX1276_ERR_NONE) {
        return state;
    }
    
    // Set payload length (max for variable length mode)
    writeRegister(SX1276_REG_PAYLOAD_LENGTH_FSK, SX1276_MAX_PACKET_LENGTH);
    
    // Set FIFO threshold (half FIFO)
    writeRegister(SX1276_REG_FIFO_THRESH, 0x80 | 0x20);
    
    // Enable sequence mode
    writeRegister(SX1276_REG_SEQ_CONFIG_1, 0x40);
    
    // Set DIO0 to PacketSent/PayloadReady
    writeRegister(SX1276_REG_DIO_MAPPING_1, 0x00);
    
    return state;
}

/**
 * Set FSK/OOK bit rate
 */
int16_t SX1276::setBitrate(uint32_t bitrate) {
    // Validate bitrate (1200 - 300000 bps)
    if (bitrate < 1200 || bitrate > 300000) {
        return SX1276_ERR_INVALID_BITRATE;
    }
    
    _bitrate = bitrate;
    
    // Calculate bitrate register value
    // Bitrate = FXOSC / BitrateReg
    uint32_t bitrateReg = SX1276_FXOSC / bitrate;
    
    writeRegister(SX1276_REG_BITRATE_MSB, (bitrateReg >> 8) & 0xFF);
    writeRegister(SX1276_REG_BITRATE_LSB, bitrateReg & 0xFF);
    
    return SX1276_ERR_NONE;
}

/**
 * Set FSK frequency deviation
 */
int16_t SX1276::setFrequencyDeviation(uint32_t freqDev) {
    // Validate frequency deviation: 0 (for OOK) or 600 Hz - 200 kHz
    if (freqDev != 0 && (freqDev < 600 || freqDev > 200000)) {
        return SX1276_ERR_INVALID_FREQUENCY_DEVIATION;
    }
    
    _freqDev = freqDev;
    
    // Calculate frequency deviation register value
    // Fdev = FSTEP × FreqDevReg
    uint32_t fdevReg = ((uint64_t)freqDev << 19) / SX1276_FXOSC;
    
    writeRegister(SX1276_REG_FDEV_MSB, (fdevReg >> 8) & 0x3F);
    writeRegister(SX1276_REG_FDEV_LSB, fdevReg & 0xFF);
    
    return SX1276_ERR_NONE;
}

/**
 * Set FSK/OOK RX bandwidth
 */
int16_t SX1276::setRxBandwidth(uint8_t rxBw) {
    _rxBw = rxBw;
    writeRegister(SX1276_REG_RX_BW, rxBw);
    return SX1276_ERR_NONE;
}

/**
 * Set FSK/OOK sync word
 */
int16_t SX1276::setSyncWord(const uint8_t* syncWord, uint8_t len) {
    // Validate sync word length (1-8 bytes)
    if (len < 1 || len > 8) {
        return SX1276_ERR_INVALID_SYNC_WORD;
    }
    
    _syncWordLen = len;
    for (uint8_t i = 0; i < len; i++) {
        _syncWordFSK[i] = syncWord[i];
    }
    
    // Configure sync word
    // Bit 7: Sync On
    // Bits 5-3: FIFO fill condition
    // Bits 2-0: Sync word size - 1
    writeRegister(SX1276_REG_SYNC_CONFIG, 0x90 | ((len - 1) & 0x07));
    
    // Write sync word bytes
    for (uint8_t i = 0; i < len; i++) {
        writeRegister(SX1276_REG_SYNC_VALUE_1 + i, syncWord[i]);
    }
    
    return SX1276_ERR_NONE;
}

/**
 * Set FSK/OOK packet configuration
 */
int16_t SX1276::setPacketConfig(bool fixedLength, bool crcOn) {
    _fixedLength = fixedLength;
    _crcOnFSK = crcOn;
    
    // PacketConfig1
    // Bit 7: Fixed (1) or Variable (0) length
    // Bit 6-5: DC-free encoding (00 = none)
    // Bit 4: CRC on (1) or off (0)
    // Bit 3: CRC auto clear off
    // Bit 2-0: Address filtering (000 = off)
    uint8_t config1 = 0x00;
    if (fixedLength) {
        config1 |= 0x80;
    }
    if (crcOn) {
        config1 |= 0x10;
    }
    writeRegister(SX1276_REG_PACKET_CONFIG_1, config1);
    
    // PacketConfig2
    // Bit 6: Data mode (1 = packet)
    // Bits 5-3: I/O home control
    // Bit 2: Beacon on (0)
    // Bits 1-0: Payload length MSB
    writeRegister(SX1276_REG_PACKET_CONFIG_2, 0x40);
    
    return SX1276_ERR_NONE;
}

/**
 * Get RSSI in FSK/OOK mode
 * Returns the cached RSSI value from the last received packet
 */
int16_t SX1276::getRSSI_FSK() {
    return _lastRSSI;
}
#endif

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

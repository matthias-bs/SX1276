# SX1276_Radio_Lite ↔ RadioLib Interoperability Test Plan

## Objective
Verify interoperability between SX1276_Radio_Lite examples and RadioLib SX127x examples, ensuring compliance with European regulations (frequency, power, modulation).

---

## Test Matrix

| SX1276_Radio_Lite Example      | RadioLib Example(s)                | Modulation | Frequency | Notes |
|-------------------------------|------------------------------------|------------|----------|-------|
| TransmitExample                | Receive_Blocking, Receive_Interrupt| LoRa       | 868 MHz  | SF7, BW 125kHz, CR 4/5, Sync 0x12 |
| ReceiveExample                 | Transmit_Blocking, Transmit_Interrupt| LoRa     | 868 MHz  | SF7, BW 125kHz, CR 4/5, Sync 0x12 |
| RadioLibCompatible             | PingPong                            | LoRa       | 868 MHz  | SF9, BW 125kHz, CR 4/7, Sync 0x12 |
| FSKExample                     | FSK_Modem                          | FSK        | 868 MHz  | 4.8kbps, 5kHz dev, Sync 0x2D,0xD4 |
| OOKExample                     | SX127x_OOK_Modem (extras)           | OOK        | 868 MHz  | 4.8kbps, 0Hz dev, Sync 0x12,0xAD. Note: OOK is typically used at 433 MHz in EU; 868 MHz was used because suitable 433 MHz test HW was not available. |
| FSKRxExample                   | SX127x_FSK_Modem                    | FSK        | 868 MHz  | RX-only, same params as FSKExample |
| FSKTxExample                   | SX127x_FSK_Modem                    | FSK        | 868 MHz  | TX-only, same params as FSKExample |
| BasicExample                   | Receive_Blocking, Transmit_Blocking | LoRa       | 868 MHz  | Bidirectional, basic LoRa test |
| BresserRxExample               | SX127x_FSK_Modem                    | FSK        | 868.3 MHz| Bresser sensor RX, custom sync/preamble |
| BresserWeatherSensorBasic      | SX127x_FSK_Modem                    | FSK        | 868.3 MHz| Bresser sensor RX + decode |
| BresserWeatherSensorTransmitter| SX127x_FSK_Modem                    | FSK        | 868.3 MHz| Bresser sensor TX emulation |

---

## Test Steps

### 1. Frequency & Modulation Alignment

> **Note:**
> - The **433 MHz ISM band** (433.05–434.79 MHz) is typically used for OOK in Europe. It usually allows up to **10% duty cycle** and **10 mW ERP** (but always check local regulations).
> - The **868 MHz SRD band** (868.0–868.6 MHz) is used for FSK, LoRa, and sometimes OOK. It is limited to **1% duty cycle** and **25 mW ERP** for all modulation types.

- Connect antennas suitable for 868 MHz (LoRa/FSK/OOK).

### 3. Test Execution

#### LoRa Tests
- Flash TransmitExample to SX1276_Radio_Lite board, Receive_Blocking to RadioLib board.
- Confirm packets are received, payload matches, RSSI/SNR reported.
- Swap roles: ReceiveExample ↔ Transmit_Blocking.
- Test RadioLibCompatible ↔ PingPong for bidirectional communication.

#### FSK Tests
- Flash FSKExample to SX1276_Radio_Lite board, FSK_Modem to RadioLib board.
- Confirm message exchange, check sync word and CRC.

#### OOK Tests
- Flash OOKExample to SX1276_Radio_Lite board, `extras/SX127x_OOK_Modem/SX127x_OOK_Modem.ino` to RadioLib board.
- Confirm message exchange, check sync word and CRC.

### 4. Compliance Checks
- Measure output power (should not exceed 17 dBm for LoRa/FSK, check OOK limits).
- Verify duty cycle restrictions (especially for OOK).

### 5. Documentation
- Record test results: success/failure, packet loss, RSSI/SNR, compliance notes.
- Note any required changes for full interoperability.

---

## Example Test Log

SX1276_Radio_Lite test board: [Adafruit Feather 32u4 RFM95 LoRa Radio 868MHz](https://www.adafruit.com/product/3078)
RadioLib test board: [Lilygo T3 LoRa32 V1.6.1](https://lilygo.cc/en-us/products/lora3?variant=51248229154997)

| SX1276_Radio_Lite Example | Actual RadioLib Sketch Used | Result | Notes |
|---------------------------|-----------------------------|--------|-------|
| TransmitExample           | [SX127x_Receive_Interrupt.ino](../extras/interop_tests/SX127x_Receive_Interrupt/SX127x_Receive_Interrupt.ino)    | ✅     | LoRa, TX-only      |
| ReceiveExample            | [SX127x_Transmit_Interrupt.ino](../extras/interop_tests/SX127x_Transmit_Interrupt/SX127x_Transmit_Interrupt.ino)   | ✅     | LoRa, RX-only      |
| BasicExample              | [SX127x_Receive_Interrupt.ino](../extras/interop_tests/SX127x_Receive_Interrupt/SX127x_Receive_Interrupt.ino) / [SX127x_Transmit_Interrupt.ino](../extras/interop_tests/SX127x_Transmit_Interrupt/SX127x_Transmit_Interrupt.ino)   | ✅     | LoRa, bidirectional |
| RadioLibCompatible        | [SX127x_PingPong.ino](../extras/interop_tests/SX127x_PingPong/SX127x_PingPong.ino)                                      | ✅     | LoRa, bidirectional, uses RadioLib compatible constructor |
| FSKExample                | [SX127x_FSK_Modem](../extras/SX127x_FSK_Modem/SX127x_FSK_Modem.ino)                                                      | ✅     | FSK, bidirectional |
| FSKRxExample              | [SX127x_FSK_Modem](../extras/SX127x_FSK_Modem/SX127x_FSK_Modem.ino)                                                      | ✅     | FSK, RX-only, interrupt-driven |
| FSKTxExample              | [SX127x_FSK_Modem](../extras/SX127x_FSK_Modem/SX127x_FSK_Modem.ino)                                                      | ✅     | FSK, TX-only                 |
| BresserRxExample          | [SensorTransmitter](https://github.com/matthias-bs/SensorTransmitter/blob/main/SensorTransmitter.ino)                                                      | ✅     | FSK, Bresser sensor RX          |
| BresserWeatherSensorBasic | [SensorTransmitter](https://github.com/matthias-bs/SensorTransmitter/blob/main/SensorTransmitter.ino)                                                      | ✅     | FSK, Bresser sensor RX + decode |
| BresserWeatherSensorTransmitter | [BresserWeatherSensorBasic](https://github.com/matthias-bs/BresserWeatherSensorReceiver/tree/main/examples/BresserWeatherSensorBasic)                                | ✅     | FSK, Bresser sensor TX emulation |
| OOKExample                | [SX127x_OOK_Modem.ino](../extras/SX127x_OOK_Modem/SX127x_OOK_Modem.ino)                                                      | ✅     | OOK, bidirectional. 868 MHz used (no 433 MHz HW available). |

<!-- Fill in the Actual RadioLib Sketch Used, Result, and Notes columns during testing -->

---

## Recommendations
- Always match modulation settings exactly.
- Use 868 MHz for LoRa/FSK/OOK in EU.
- Observe the 1% duty cycle limit for all modulation types in the EU SRD band 868.0–868.6 MHz.
- Document any deviations or issues.

---

## References
- [RadioLib SX127x Examples](https://github.com/jgromes/RadioLib/tree/master/examples/SX127x)
- [EU Frequency Regulations](https://www.cept.org/)

---

_Last updated: 2026-03-15_

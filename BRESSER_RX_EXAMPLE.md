# BRESSER_RX_EXAMPLE

Implement an example sketch BresserRxExample with the following parameters:

* carrier frequency:                   868.3 MHz
* bit rate:                            8.21 kbps
* frequency deviation:                 57.136417 kHz
* Rx bandwidth:                        250 kHz
* output power:                        10 dBm
* preamble length:                     32 bits (4 bytes)
* packet mode:                         fixed length, 27 bytes
* CRC filtering:                       disabled
* preamble:                            0xAA, 0xAA, 0xAA, 0xAA
* sync word:                           0xAA, 0x2D

**Note:** The actual Bresser protocol uses a 40-bit preamble (AA AA AA AA AA) followed by sync word 2D D4. Since the SX1276 preamble length must be specified in bytes, we use a 32-bit preamble (4 bytes of 0xAA) and set the sync word to AA 2D. This configuration causes the last sync byte (0xD4) to be received as the first byte of the payload, which matches the Bresser protocol expectation.

In the receive loop, print all received frames for debugging. Bresser frames start with 0xD4.

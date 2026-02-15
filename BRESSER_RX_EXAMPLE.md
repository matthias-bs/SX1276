# BRESSER_RX_EXAMPLE

Implement an example sketch BresserRxExample with the following parameters:

* carrier frequency:                   868.3 MHz
* bit rate:                            8.22 kbps
* frequency deviation:                 57.136417 kHz
* Rx bandwidth:                        250 kHz
* output power:                        10 dBm
* preamble length:                     40 bits
* packed mode:                         fixed length, 27 bytes
* CRC filtering:                       disabled
* preamble:                            0xAA, 0xAA, 0xAA, 0xAA, 0xAA
* sync word:                           0xAA, 0x2D

In the receive loop, print all frames starting with 0xD4.

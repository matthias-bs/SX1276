/*
  RadioLib SX127x Transmit with Interrupts Example

  This example transmits LoRa packets using interrupt-driven TX.

  Interop partner: examples/ReceiveExample/ReceiveExample.ino (SX1276_Radio_Lite)

  Configuration (must match interop partner):
    Frequency     : 868 MHz
    Spreading fac.: SF7
    Bandwidth     : 125 kHz
    Coding rate   : 4/5
    Sync word     : 0x12
    CRC           : on
    Preamble      : 8 symbols

  EU compliance note: 868 MHz g1 sub-band (868.0–868.6 MHz) permits
  max 25 mW ERP with a 1% duty cycle (ETSI EN 300 220).  This sketch
  enforces a minimum TX interval (TX_MIN_INTERVAL_MS) to stay below 1%.

  For default module settings, see the wiki page
  https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx127xrfm9x---lora-modem

  For full API reference, see the GitHub Pages
  https://jgromes.github.io/RadioLib/
*/

// include the library
#include <RadioLib.h>

#define RADIO_CS   LORA_CS
#define RADIO_RST  LORA_RST
#define RADIO_DIO0 LORA_IRQ
#define RADIO_DIO1 LORA_D1

SX1276 radio = new Module(RADIO_CS, RADIO_DIO0, RADIO_RST, RADIO_DIO1);

// or detect the pinout automatically using RadioBoards
// https://github.com/radiolib-org/RadioBoards
/*
#define RADIO_BOARD_AUTO
#include <RadioBoards.h>
Radio radio = new RadioModule();
*/

// save transmission state between loops
int transmissionState = RADIOLIB_ERR_NONE;

// flag to indicate that a packet was sent
volatile bool transmittedFlag = false;

// this function is called when a complete packet
// is transmitted by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  // we sent a packet, set the flag
  transmittedFlag = true;
}

// EU 868 MHz g1 sub-band duty cycle enforcement.
// At SF7 / BW 125 kHz a ~20-byte LoRa packet takes ~57 ms airtime.
// 1% duty cycle  =>  min interval >= airtime / 0.01 ≈ 5.7 s.
// We use 10 s to leave headroom.
static const unsigned long TX_MIN_INTERVAL_MS = 10000;

// Timestamp of the last transmission start
static unsigned long lastTxMs = 0;

void setup() {
  Serial.begin(115200);

  // initialize SX1276 with default settings
  Serial.print(F("[SX1276] Initializing ... "));
  int state = radio.begin(868.0F, 125.0F, 7, 5);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // set the function that will be called
  // when packet transmission is finished
  radio.setPacketSentAction(setFlag);

  // start transmitting the first packet
  Serial.print(F("[SX1276] Sending first packet ... "));
  lastTxMs = millis();
  transmissionState = radio.startTransmit("Hello World!");
}

// counter to keep track of transmitted packets
int count = 0;

void loop() {
  // check if the previous transmission finished
  if(transmittedFlag) {
    // reset flag
    transmittedFlag = false;

    if (transmissionState == RADIOLIB_ERR_NONE) {
      // packet was successfully sent
      Serial.println(F("transmission finished!"));

    } else {
      Serial.print(F("failed, code "));
      Serial.println(transmissionState);

    }

    // clean up after transmission is finished
    // this will ensure transmitter is disabled,
    // RF switch is powered down etc.
    radio.finishTransmit();

    // enforce EU 868 MHz 1% duty cycle
    unsigned long elapsed = millis() - lastTxMs;
    if (elapsed < TX_MIN_INTERVAL_MS) {
      delay(TX_MIN_INTERVAL_MS - elapsed);
    }

    // send another one
    Serial.print(F("[SX1276] Sending another packet ... "));

    String str = "Hello World! #" + String(count++);
    lastTxMs = millis();
    transmissionState = radio.startTransmit(str);
  }
}

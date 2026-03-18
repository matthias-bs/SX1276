/*
  RadioLib SX127x Ping-Pong Example

  Interop partner: examples/RadioLibCompatible/RadioLibCompatible.ino (SX1276_Radio_Lite)

  Configuration (RadioLib defaults, must match interop partner):
    Frequency     : 868 MHz
    Spreading fac.: SF9
    Bandwidth     : 125 kHz
    Coding rate   : 4/7
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

// uncomment the following only on one
// of the nodes to initiate the pings
//#define INITIATING_NODE

#define RADIO_CS    LORA_CS
#define RADIO_RST   LORA_RST
#define RADIO_DIO0  LORA_IRQ
#define RADIO_DIO1  LORA_D1

SX1276 radio = new Module(RADIO_CS, RADIO_DIO0, RADIO_RST, RADIO_DIO1);

// or detect the pinout automatically using RadioBoards
// https://github.com/radiolib-org/RadioBoards
/*
#define RADIO_BOARD_AUTO
#include <RadioBoards.h>
Radio radio = new RadioModule();
*/

// save transmission states between loops
int transmissionState = RADIOLIB_ERR_NONE;

// flag to indicate transmission or reception state
bool transmitFlag = false;

// flag to indicate that a packet was sent or received
volatile bool operationDone = false;

// this function is called when a complete packet
// is transmitted or received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  // we sent or received  packet, set the flag
  operationDone = true;
}

// EU 868 MHz g1 sub-band duty cycle enforcement.
// At SF9 / BW 125 kHz a ~12-byte LoRa packet takes ~169 ms airtime.
// 1% duty cycle  =>  min interval >= airtime / 0.01 ≈ 16.9 s.
// We use 25 s to leave headroom and match the peer.
static const unsigned long TX_MIN_INTERVAL_MS = 25000;

// Timestamp of the last transmission start
static unsigned long lastTxMs = 0;

void setup() {
  Serial.begin(115200);

  // initialize SX1276 with default settings
  Serial.print(F("[SX1276] Initializing ... "));
  int state = radio.begin(868.0);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // set the function that will be called
  // when new packet is received
  radio.setDio0Action(setFlag, RISING);

  #if defined(INITIATING_NODE)
    // send the first packet on this node
    Serial.print(F("[SX1276] Sending first packet ... "));
    lastTxMs = millis();
    transmissionState = radio.startTransmit("Hello World!");
    transmitFlag = true;
  #else
    // start listening for LoRa packets on this node
    Serial.print(F("[SX1276] Starting to listen ... "));
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      while (true) { delay(10); }
    }
  #endif
}

void loop() {
  // check if the previous operation finished
  if(operationDone) {
    // reset flag
    operationDone = false;

    if(transmitFlag) {
      // the previous operation was transmission, listen for response
      // print the result
      if (transmissionState == RADIOLIB_ERR_NONE) {
        // packet was successfully sent
        Serial.println(F("transmission finished!"));

      } else {
        Serial.print(F("failed, code "));
        Serial.println(transmissionState);

      }

      // listen for response
      radio.startReceive();
      transmitFlag = false;

    } else {
      // the previous operation was reception
      // print data and send another packet
      String str;
      int state = radio.readData(str);

      if (state == RADIOLIB_ERR_NONE) {
        // packet was successfully received
        Serial.println(F("[SX1276] Received packet!"));

        // print data of the packet
        Serial.print(F("[SX1276] Data:\t\t"));
        Serial.println(str);

        // print RSSI (Received Signal Strength Indicator)
        Serial.print(F("[SX1276] RSSI:\t\t"));
        Serial.print(radio.getRSSI());
        Serial.println(F(" dBm"));

        // print SNR (Signal-to-Noise Ratio)
        Serial.print(F("[SX1276] SNR:\t\t"));
        Serial.print(radio.getSNR());
        Serial.println(F(" dB"));

      }

      // enforce EU 868 MHz 1% duty cycle
      unsigned long elapsed = millis() - lastTxMs;
      if (elapsed < TX_MIN_INTERVAL_MS) {
        delay(TX_MIN_INTERVAL_MS - elapsed);
      }

      // send another one
      Serial.print(F("[SX1276] Sending another packet ... "));
      lastTxMs = millis();
      transmissionState = radio.startTransmit("Hello World!");
      transmitFlag = true;
    }
  }
}

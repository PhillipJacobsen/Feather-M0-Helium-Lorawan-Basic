/*******************************************************************************
   Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
   Copyright (c) 2018 Terry Moore, MCCI

   Permission is hereby granted, free of charge, to anyone
   obtaining a copy of this document and accompanying files,
   to do whatever they want with them without any restriction,
   including, but not limited to, copying, modification and redistribution.
   NO WARRANTY OF ANY KIND IS PROVIDED.

   This example sends a valid LoRaWAN packet with payload "Hello,
   world!", using frequency and encryption settings matching those of
   the The Things Network. It's pre-configured for the Adafruit
   Feather M0 LoRa.

   This uses OTAA (Over-the-air activation), where where a DevEUI and
   application key is configured, which are used in an over-the-air
   activation procedure where a DevAddr and session keys are
   assigned/generated for use with all further communication.

   Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in
   g1, 0.1% in g2), but not the TTN fair usage policy (which is probably
   violated by this sketch when left running for longer)!

   To use this sketch, first register your application and device with
   the things network, to set or generate an AppEUI, DevEUI and AppKey.
   Multiple devices can use the same AppEUI, but each device has its own
   DevEUI and AppKey.

   Do not forget to define the radio type correctly in
   arduino-lmic/project_config/lmic_project_config.h or from your BOARDS.txt.

 *******************************************************************************/

//Must be used with this LMIC library.  https://github.com/mcci-catena/arduino-lmic
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// include the FeatherWing OLED library
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display = Adafruit_SSD1306();

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
static const u1_t PROGMEM APPEUI[8] = {0xD2, 0x24, 0xC2, 0x76, 0x1D, 0xFA, 0x10, 0x1D};
void os_getArtEui (u1_t* buf) {
  memcpy_P(buf, APPEUI, 8);
}

// This should also be in little endian format, see above.
static const u1_t PROGMEM DEVEUI[8] = {0xD8, 0xBC, 0x5A, 0x19, 0x07, 0x87, 0x03, 0x46};
void os_getDevEui (u1_t* buf) {
  memcpy_P(buf, DEVEUI, 8);
}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from the TTN console can be copied as-is.

static const u1_t PROGMEM APPKEY[16] = { 0xDB, 0x0A, 0x4C, 0x15, 0xF0, 0xD4, 0x14, 0xCC, 0xF8, 0xFE, 0x0F, 0xC3, 0x0E, 0x99, 0x6C, 0x53 };
void os_getDevKey (u1_t* buf) {
  memcpy_P(buf, APPKEY, 16);
}

static uint8_t mydata[] = "Hello, world!";
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 20;

// Pin mapping
//
// Adafruit BSPs are not consistent -- m0 express defs ARDUINO_SAMD_FEATHER_M0,
// m0 defs ADAFRUIT_FEATHER_M0
//

// Pin mapping for Adafruit Feather M0 LoRa, etc.
const lmic_pinmap lmic_pins = {
  .nss = 8,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 4,
  .dio = {3, 6, LMIC_UNUSED_PIN},
  .rxtx_rx_active = 0,
//  .rssi_cal = 8,              // Listen Before Talke (LBT) algorithm for European market requirement cal for the Adafruit Feather M0 LoRa, in dB
  .rssi_cal = 0,              // disable LBT
  .spi_freq = 8000000,
};


void printHex2(unsigned v) {
  v &= 0xff;
  if (v < 16)
    Serial.print('0');
  Serial.print(v, HEX);
}

void onEvent (ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
  switch (ev) {
    case EV_SCAN_TIMEOUT:
      Serial.println(F("EV_SCAN_TIMEOUT"));
      break;
    case EV_BEACON_FOUND:
      Serial.println(F("EV_BEACON_FOUND"));
      break;
    case EV_BEACON_MISSED:
      Serial.println(F("EV_BEACON_MISSED"));
      break;
    case EV_BEACON_TRACKED:
      Serial.println(F("EV_BEACON_TRACKED"));
      break;
    case EV_JOINING:
      display.print("Helium: Joining...");
      display.display();
      Serial.println(F("EV_JOINING"));
      break;
    case EV_JOINED:
      display.clearDisplay();
      display.display();
      display.setCursor(0, 0);
      display.println("Helium: Connected");
      display.display();
      Serial.println(F("EV_JOINED"));
      {
        u4_t netid = 0;
        devaddr_t devaddr = 0;
        u1_t nwkKey[16];
        u1_t artKey[16];
        LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
        Serial.print("netid: ");
        Serial.println(netid, DEC);
        Serial.print("devaddr: ");
        Serial.println(devaddr, HEX);
        Serial.print("AppSKey: ");
        for (size_t i = 0; i < sizeof(artKey); ++i) {
          if (i != 0)
            Serial.print("-");
          printHex2(artKey[i]);
        }
        Serial.println("");
        Serial.print("NwkSKey: ");
        for (size_t i = 0; i < sizeof(nwkKey); ++i) {
          if (i != 0)
            Serial.print("-");
          printHex2(nwkKey[i]);
        }
        Serial.println();
      }
      // Disable link check validation (automatically enabled
      // during join, but because slow data rates change max TX
      // size, we don't use it in this example.
      LMIC_setLinkCheckMode(0);
      break;
    /*
      || This event is defined but not used in the code. No
      || point in wasting codespace on it.
      ||
      || case EV_RFU1:
      ||     Serial.println(F("EV_RFU1"));
      ||     break;
    */
    case EV_JOIN_FAILED:
      Serial.println(F("EV_JOIN_FAILED"));
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("EV_REJOIN_FAILED"));
      break;
      break;
    case EV_TXCOMPLETE:
      display.clearDisplay();
      display.display();
      display.setCursor(0, 0);
      display.println("Helium: Connected");
      display.display();
      display.setCursor(0, 10);
      display.println("* Sent!");
      display.display();

      Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
      if (LMIC.txrxFlags & TXRX_ACK)
        Serial.println(F("Received ack"));
      if (LMIC.dataLen) {

        display.clearDisplay();
        display.display();
        display.setCursor(0, 0);
        display.println("Helium: Connected");
        display.display();
        display.setCursor(0, 10);
        display.println("* Received!");

        Serial.print(F("Received "));
        Serial.print(LMIC.dataLen);
        Serial.println(F(" bytes of payload"));
        //   Serial.write(LMIC.frame+LMIC.dataBeg,strlen(LMIC.frame+LMIC.dataBeg));
        //        for (int i = 0; i < LMIC.dataLen; ++i) {
        //          Serial.print((LMIC.frame+LMIC.dataBeg)[i], HEX);
        //        }
        //        Serial.println("");
        for (int i = 0; i < LMIC.dataLen; ++i) {
          Serial.print((char)(LMIC.frame + LMIC.dataBeg)[i]);
          display.print((char)(LMIC.frame + LMIC.dataBeg)[i]);
        }
        Serial.println("");
        display.display();    //update display with received data

      }
      // Schedule next transmission
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
      break;
    case EV_LOST_TSYNC:
      Serial.println(F("EV_LOST_TSYNC"));
      break;
    case EV_RESET:
      Serial.println(F("EV_RESET"));
      break;
    case EV_RXCOMPLETE:
      // data received in ping slot
      Serial.println(F("EV_RXCOMPLETE"));
      break;
    case EV_LINK_DEAD:
      Serial.println(F("EV_LINK_DEAD"));
      break;
    case EV_LINK_ALIVE:
      Serial.println(F("EV_LINK_ALIVE"));
      break;
    /*
      || This event is defined but not used in the code. No
      || point in wasting codespace on it.
      ||
      || case EV_SCAN_FOUND:
      ||    Serial.println(F("EV_SCAN_FOUND"));
      ||    break;
    */
    case EV_TXSTART:
      display.clearDisplay();
      display.display();
      display.setCursor(0, 0);
      display.println("Helium: Connected");
      display.setCursor(0, 10);
      display.println("* Sending");
      display.setCursor(0, 25);
      //display.print(mydata);
      display.display();
      Serial.println(F("EV_TXSTART"));
      break;
    case EV_TXCANCELED:
      Serial.println(F("EV_TXCANCELED"));
      break;
    case EV_RXSTART:
      /* do not print anything -- it wrecks timing */
      break;
    case EV_JOIN_TXCOMPLETE:
      Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
      break;

    default:
      Serial.print(F("Unknown event: "));
      Serial.println((unsigned) ev);
      break;
  }
}

void do_send(osjob_t* j) {
  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("OP_TXRXPEND, not sending"));
  } else {
    // Prepare upstream data transmission at the next possible time.
    LMIC_setTxData2(1, mydata, sizeof(mydata) - 1, 0);      //no acks
    Serial.println(F("Packet queued"));
  }
  // Next TX is scheduled after TX_COMPLETE event.
}

void setup() {
  delay(5000);
//  while (! Serial)
//    ;
  Serial.begin(115200);
  Serial.println(F("Starting"));


  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  Serial.println("OLED init'd");

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(1000);
  // Clear the buffer.
  display.clearDisplay();
  display.display();

  // set text display size/location
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // LMIC init
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

  // allow much more clock error than the X/1000 default. See:
  // https://github.com/mcci-catena/arduino-lorawan/issues/74#issuecomment-462171974
  // https://github.com/mcci-catena/arduino-lmic/commit/42da75b56#diff-16d75524a9920f5d043fe731a27cf85aL633
  // the X/1000 means an error rate of 0.1%; the above issue discusses using values up to 10%.
  // so, values from 10 (10% error, the most lax) to 1000 (0.1% error, the most strict) can be used.
  LMIC_setClockError(1 * MAX_CLOCK_ERROR / 40);

  LMIC_setLinkCheckMode(0);
  // according to the following link It seems like Helium is hardcoded to SF10BW125
  //  https://developer.helium.com/devices/migrating-devices-to-helium/device-migration-the-things-network
  // in the helium console the device packets seem to be using SF9BW125
  // https://docs.exploratory.engineering/lora/dr_sf/
  
  // Set the data rate to Spreading Factor 7.  This is the fastest supported rate for 125 kHz channels, and it
  // minimizes air time and battery power. Set the transmission power to 14 dBi (25 mW).
  //LMIC_setDrTxpow(DR_SF7, 14); 
   LMIC_setDrTxpow(DR_SF10, 14); 
  LMIC_selectSubBand(1);

  // Start job (sending automatically starts OTAA too)
  do_send(&sendjob);
}

// https://www.url-encode-decode.com/base64-encode-decode/

//join issues
// https://www.thethingsnetwork.org/forum/t/strange-problem-rfm95-lmic-arduino-long-join-delays-and-missing-frames/15352/19

void loop() {
  // we call the LMIC's runloop processor. This will cause things to happen based on events and time. One
  // of the things that will happen is callbacks for transmission complete or received messages. We also
  // use this loop to queue periodic data transmissions.  You can put other things here in the `loop()` routine,
  // but beware that LoRaWAN timing is pretty tight, so if you do more than a few milliseconds of work, you
  // will want to call `os_runloop_once()` every so often, to keep the radio running.
  os_runloop_once();
}

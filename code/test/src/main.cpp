#include "can.h"
#include "esp32-hal.h"
#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

struct can_frame flow = {0x7DF, 8,    0x30, 0x00, 0x00,
                         0x00,  0x00, 0x00, 0x00, 0x00};
struct can_frame getdtc = {0x7DF, 8,    0x02, 0x03, 0x00,
                           0x00,  0x00, 0x00, 0x00, 0x00};
struct can_frame msg;

#define SCK 18
#define MISO 19
#define MOSI 23
#define CS 15

SPIClass hspi(HSPI);
MCP2515 mcp2515(CS, 10000000, &hspi);

//              n of bytes
// id 7DF     [ 0x02, 0x03 , ... 0x00]
//                    mode 3
//
//
// single frame exa
// id ???  [0x04, 0x41, 0xFB, 0x23, 0x11, 0x33, 0x00, 0x00]
//
// FF id ??? [0x12, 0xFF, data ...]
//
//
//
//
// control frame id ????
//                        high 4 bits of byte 0 = 0x3
//                        and rest is 0x00

//

void setup() {

  Serial.begin(115200);
  delay(5000);

  hspi.begin(SCK, MISO, MOSI, CS);

  mcp2515.reset();
  Serial.println("✅ MCP2515 Ready");
  if (mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ) != MCP2515::ERROR_OK) {
    Serial.println("❌ Bitrate setup failed");
  }
  if (mcp2515.setNormalMode() != MCP2515::ERROR_OK) {
    Serial.println("❌ Failed to enter Normal Mode");
  }

  Serial.println("done with setup");
}

void loop() {
  MCP2515::ERROR error;

  if ((error = mcp2515.readMessage(&msg)) == MCP2515::ERROR_OK) {

    String string =
        "[" + String(msg.can_id, HEX) + "]" + " " + "{" +
        String(msg.data[0], HEX) + ", " + String(msg.data[1], HEX) + ", " +
        String(msg.data[2], HEX) + ", " + String(msg.data[3], HEX) + ", " +
        String(msg.data[4], HEX) + ", " + String(msg.data[5], HEX) + ", " +
        String(msg.data[6], HEX) + ", " + String(msg.data[7], HEX) + "}";
    Serial.println(string);

    if ((msg.data[0] >> 4) == 0x1) {
      mcp2515.sendMessage(&flow);
      Serial.println(
          "-- -- -- -- -- -- -- Sent Flow Frame -- -- -- -- -- -- --");
    }
  }
  static int delta = 0;
  if (millis() - delta > 10000) {
    delta = millis();
    mcp2515.sendMessage(&getdtc);
    Serial.println(" -- -- -- -- -- -- Sent Get DTCs -- -- -- -- -- -- ");
  }
}

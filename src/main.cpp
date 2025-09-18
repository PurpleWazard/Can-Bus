#include "can.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <SPI.h>
#include <WiFi.h>
#include <cstdint>
#include <mcp2515.h>

AsyncWebServer server(80);
AsyncEventSource events("/events");
int rpmMin = 9999, rpmMax = 0, rpmSum = 0, rpmCount = 0;

struct can_frame rpm_frame;
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
String dtcCode(uint8_t high, uint8_t low) {

  char codecat;
  uint8_t Bcodecat;
  uint16_t codenum;

  Bcodecat = high >> 4;

  switch (Bcodecat) {
  case 0x0: // P
    codecat = 'P';
    break;
  case 0x4: // C
    codecat = 'C';
    break;
  case 0x8: // B
    codecat = 'B';
    break;
  case 0xC: // U
    codecat = 'U';
    break;
  }

  codenum = ((high & 0b00001111) << 8) | low;

  String string = (String)codecat + (String)codenum;
  Serial.println(string);
  return string;
}

void hahah(struct can_frame B) {

  uint8_t hb = (B.data[0] & 0xF0) >> 4;
  uint8_t lb = B.data[0] & 0x0F;

  if (hb == 0) {
    uint8_t Ndtcs = lb / 2;
    uint8_t Nbyte;
    for (int i = 0; i < Ndtcs; i++) {
      uint8_t high = B.data[2 + (i * 2)];
      uint8_t low = B.data[3 + (i * 2)];
      dtcCode(high, low);
    }

  } else if (hb == 1) {
    uint16_t length = (lb << 8) | B.data[1];
    Serial.print(length / 2);
    Serial.println(" DTCs ");
    uint16_t lengthTracker = length;
    dtcCode(B.data[2], B.data[3]);
    length -= 2;
    dtcCode(B.data[4], B.data[5]);
    length -= 2;
    dtcCode(B.data[6], B.data[7]);
    length -= 2;
    if (lengthTracker > 0) {
      uint8_t index = 1;
      struct can_frame msg;
      MCP2515::ERROR error = MCP2515::ERROR_OK;
      while (lengthTracker > 0) {
        if (mcp2515.readMessage(&msg) == MCP2515::ERROR_OK) {
          if ((msg.data[0] >> 4) == 0x2 && (msg.data[0] & 0x0F) == index){
            index++;




          }
        }
      }
    }

  } else if (hb == 2) {
  }
}

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

  rpm_frame.can_id = 0x7DF;
  rpm_frame.can_dlc = 8;
  rpm_frame.data[0] = 0x02;
  rpm_frame.data[1] = 0x01;
  rpm_frame.data[2] = 0x0C;
  rpm_frame.data[3] = 0x00;
  rpm_frame.data[4] = 0x00;
  rpm_frame.data[5] = 0x00;
  rpm_frame.data[6] = 0x00;
  rpm_frame.data[7] = 0x00;

  WiFi.softAP("esp", NULL);
  LittleFS.begin(true);

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  events.onConnect([](AsyncEventSourceClient *client) {});
  server.addHandler(&events);

  server.begin();
  mcp2515.sendMessage(&rpm_frame);
  Serial.println("done with setup");
}

void loop() {
  MCP2515::ERROR error;
  int rpm = 0;

  while ((error = mcp2515.readMessage(&msg)) == MCP2515::ERROR_OK) {
    if (msg.data[1] == 0x41 && msg.data[2] == 0x0C) {

      rpm = ((256 * msg.data[3]) + msg.data[4]) / 4;

      rpmMin = min(rpmMin, rpm);
      rpmMax = max(rpmMax, rpm);
      rpmSum += rpm;
      rpmCount++;

      float avg = (float)rpmSum / rpmCount;

      // JSON payload to send to frontend
      String payload =
          "{\"rpm\":" + String(rpm) + ",\"min\":" + String(rpmMin) +
          ",\"max\":" + String(rpmMax) + ",\"avg\":" + String(avg, 0) + "}";

      events.send(payload.c_str(), "message", millis());

      break;
    }
  }
  // Send new RPM request every 500ms
  static unsigned long lastSend = 0;
  if (millis() - lastSend >= 100) {
    mcp2515.sendMessage(&rpm_frame);
    lastSend = millis();
  }
}

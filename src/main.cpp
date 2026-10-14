
#include <Arduino.h>
#include <Arduino_Nesso_N1.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <SNMPClient.h>
#include <SNMPTable.h>
#include <SNMPMIB.h>
#include "secrets.h"

// --- Network Settings ---
const char* ssid = SSID;
const char* password = WIFI_PWD;

// --- SNMP Target Settings ---
const char* community = "public";    // Standard SNMP community string

// --- SNMP Variables ---
WiFiUDP udp;
SNMPClient client(udp);
// Printer-MIB rows have a compound device/supply index; keep the full suffix.
SNMPDevice printer(client, TARGET_IP, community);
SNMPTableRead<64, 32, 32> supplies(printer);
bool ready = false;


String modName = MOD_NAME;
NessoBattery battery;
NessoDisplay display;
LGFX_Sprite statusSprite(&display);

const int DISPLAY_WIDTH = 240;
const int DISPLAY_HEIGHT = 135;
const uint16_t COLOR_TEAL = 0x0410;
const uint16_t COLOR_BLACK = 0x0000;
const uint16_t COLOR_GREEN = 0x1e85;
const uint16_t COLOR_ORANGE = 0xed03;
const uint16_t COLOR_RED = 0xe841;
const uint16_t COLOR_BLUE = 0x001F;

// Standard MIB-2 OID for System Description
const char* sysDescrOID = ".1.3.6.1.2.1.1.1.0"; 
char responseBuffer[256];


void setup() {
  Serial.begin(115200);
  display.begin();

  battery.begin();
  battery.enableCharge();
  display.setRotation(1);
  display.setEpdMode(epd_mode_t::epd_fastest);

  display.fillScreen(TFT_WHITE);
  display.setTextColor(COLOR_TEAL);
  display.setTextSize(5);
  display.drawString(modName, 6, 11);    
  delay(3000);
  Serial.println("Starting SNMP Manager...");

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("\nConnected to network!");
  Serial.print("ESP32-C6 IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize SNMP
    auto status = client.begin();
    if (status.ok())
        status = supplies.addColumn(".1.3.6.1.2.1.43.11.1.1.6.1", STRING); // description
    if (status.ok())
        status = supplies.addColumn(".1.3.6.1.2.1.43.11.1.1.8.1", INTEGER); // capacity
    if (status.ok())
        status = supplies.addColumn(".1.3.6.1.2.1.43.11.1.1.9.1", INTEGER); // level
    if (status.ok())
        status = supplies.start();
    ready = status.ok();
    if (!ready)
        Serial.println(status.message());
}

void loop() {
  if (!ready)
    return;
  client.loop();
  if (!supplies.takeCompleted())
    return;

  Serial.println(supplies.status().message());
  for (const auto &row : supplies)
  {
    Serial.print(row.index);
    Serial.print(" ");
    if (row[0].ok() && row[0].value.isText())
      Serial.print(row[0].value.text());
    Serial.print(": ");
    if (!row[2].ok())
    {
      Serial.println(row[2].status.message());
      continue;
    }
    double percent;
    if (row[1].ok() && SNMPMIB::supplyPercent(row[2].value, row[1].value, percent))
    {
      // Same-row capacity and level use the same units. For waste receptacles,
      // level describes remaining space, not the amount of waste accumulated.
      Serial.print(percent, 1);
      Serial.println("% remaining");
      continue;
    }
    switch (SNMPMIB::supplyState(row[2].value))
    {
    case SNMPMIB::SupplyState::Other:
      Serial.println("other");
      break;
    case SNMPMIB::SupplyState::Unknown:
      Serial.println("unknown");
      break;
    case SNMPMIB::SupplyState::SomeRemaining:
      Serial.println("some remaining");
      break;
    case SNMPMIB::SupplyState::Known:
      Serial.println("known level; percentage unavailable");
      break;
    default:
      Serial.print("invalid level: ");
      break;
    }
  }
}

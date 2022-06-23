#include <ESP8266WiFi.h>        //Use ESP8266 functions                                              
#include <ESP8266HTTPClient.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <WakeOnLan.h>
#include <SimpleTimer.h>
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h" // Include the UI lib
#include "ClickButton.h"
SSD1306Wire  display(0x3c, D2, D1); //  D2 -> SDA and D1 -> SCL
OLEDDisplayUi ui     ( &display );
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

const char *mqtt_broker = "broker.emqx.io"; // your_mqtt_url
const char *topic = "D1024181028";
const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;


#define MAGIC_PACKET_LENGTH 102
#define PORT_WAKEONLAN 9
WiFiUDP udp;
byte magicPacket[MAGIC_PACKET_LENGTH];
unsigned int localPort = 9;
SimpleTimer timer;

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer server(80);

void MQTT_connect();

const int ledPin = D3;
int ledState = 0;


// the Button
const int buttonPin1 = D4;
ClickButton button1(buttonPin1, LOW, CLICKBTN_PULLUP);

// Arbitrary LED function
int LEDfunction = 0;

void reset() {

  // clear WiFi creds.
  WiFiManager wifiManager;
  wifiManager.resetSettings();

  Serial.println("Restarting...");
  ESP.restart();
}

void setup()
{

  pinMode(ledPin, OUTPUT);
  // Setup button timers (all in milliseconds / ms)
  // (These are default if not set, but changeable for convenience)
  button1.debounceTime   = 20;   // Debounce timer in ms
  button1.multiclickTime = 250;  // Time limit for multi clicks
  button1.longClickTime  = 1000; // time until "held-down clicks" register


  Serial.begin(115200);
  WiFiManager wifiManager;

  // initialize dispaly
  display.init();
  display.clear();
  display.display();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.flipScreenVertically();
  display.setContrast(255);
  if (WiFi.status() != WL_CONNECTED) {
    display.clear();
    display.drawString(64, 1, "WiFi Connecting ....");
    display.drawString(64, 15, "If Connect timeout");
    display.drawString(64, 30, "Please Connect to ");
    display.drawString(64, 45, "SSID : WIFI_FLASH");
    display.display();
  }

  if (!wifiManager.autoConnect("WIFI_FLASH")) {
    display.clear();
    display.drawString(64, 1, "Failed to connect...");
    display.drawString(64, 15, "Syetem resetting....");
    delay(3000);
    ESP.restart();
  }


}



void callback(char *topic, byte *payload, unsigned int length) {

  String message;
  for (int i = 0; i < length; i++) {
    message = message + (char) payload[i];  // convert *byte to string
  }

  String html_mac = message;
  byte mac_length = html_mac.length();
  char mac_array[mac_length + 1];  //字元陣列儲存空間
  html_mac.toUpperCase();
  html_mac.toCharArray(mac_array, mac_length + 1);
  unsigned char ascii_to_decimal[mac_length];                 //字元轉Decimal陣列
  unsigned char macAddr[6];                          //最後MAC整合後的
  for (int i = 0; i < mac_length; i++) {

    if (isDigit(mac_array[i])) {

      ascii_to_decimal[i] = byte(mac_array[i] - 48);

    } else if (isAlpha(mac_array[i])) {

      switch (mac_array[i]) {
        case 'A':
          ascii_to_decimal[i] = 0x0A;
          break;
        case 'B':
          ascii_to_decimal[i] = 0x0B;
          break;
        case 'C':
          ascii_to_decimal[i] = 0x0C;
          break;
        case 'D':
          ascii_to_decimal[i] = 0x0D;
          break;
        case 'E':
          ascii_to_decimal[i] = 0x0E;
          break;
        case 'F':
          ascii_to_decimal[i] = 0x0F;
          break;
      }

    } else {
      ascii_to_decimal[i] = 0;
    }

    if (i % 2 == 1) {
      macAddr[(i - 1) / 2] = ascii_to_decimal[i - 1] << 4 | ascii_to_decimal[i];

    }
  }

  IPAddress bcastAddr1(192, 168, 0, 255);
  //IPAddress bcastAddr1(10, 14, 1, 255);
  memset(magicPacket, 0xFF, 6);
  for (int i = 0; i < 16; i++) {
    int ofs = i * sizeof(macAddr) + 6;
    memcpy(&magicPacket[ofs], macAddr, sizeof(macAddr));
  }
  udp.beginPacket(bcastAddr1, PORT_WAKEONLAN);
  udp.write(magicPacket, MAGIC_PACKET_LENGTH);
  udp.endPacket();
  Serial.print(message);
  display.clear();
  display.drawString(64, 1, "Received :");
  display.drawString(64, 30, message);
  display.display();
  delay(3000);
  String client_id = "esp8266-client-";
  client_id += String(WiFi.macAddress());
  if (client.connect((char*) client_id.c_str(), mqtt_username, mqtt_password)) {
    Serial.println("mqtt broker connected");
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 1, "MQTT Status :");
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 30, "Connected !!! ");
    display.display();
    digitalWrite(ledPin, HIGH);
  } else {
    Serial.print("failed with state ");
    Serial.print(client.state());
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 1, "MQTT Status :");
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 30, "Not Connect... ");
    delay(2000);
  }
}



void loop()
{

  // Update button state
  button1.Update();

  // Save click codes in LEDfunction, as click codes are reset at next Update()
  if (button1.clicks != 0) LEDfunction = button1.clicks;



  // button單按一下執行狀態
  if (button1.clicks == 1) {
    MQTT_connect();
  }

  // button快按兩下執行狀態
  if (LEDfunction == 2) {
  }

  // button快按三下執行狀態
  if (LEDfunction == 3) {
  }

  // button長按一下執行狀態
  if (LEDfunction == -1) {
  }

  // button快按兩下第二下Hold住執行狀態
  if (LEDfunction == -2) {
  }

  // button快按兩下第三下Hold住執行狀態
  if (LEDfunction == -3) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 30, "Wifi Resetting....");
    display.display();
    reset();
  }

  client.loop();
  MQTT_connect();




}


void MQTT_connect() {
  //connecting to a mqtt broker
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp8266-client-";
    client_id += String(WiFi.macAddress());
    Serial.println("Connecting to mqtt broker.....");
    if (client.connect((char*) client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("mqtt broker connected");
      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.drawString(64, 1, "MQTT Status :");
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 30, "Connected !!! ");
      display.display();
      digitalWrite(ledPin, HIGH);
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.drawString(64, 1, "MQTT Status :");
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 30, "Not Connect... ");
      delay(2000);
    }
  }
  // publish and subscribe
  //    client.publish(topic, "hello emqx");
  client.subscribe(topic);
}

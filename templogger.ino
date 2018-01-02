#include <SPI.h>
#include <WiFi101.h>
#include <TimeLib.h> 
#include <DHT.h>
#include <Ticker.h>
#include "templogger.h"  // ssid[], pass[], syslogServer and ntpServer

#define WIFI_OK 0
#define WIFI_RECONNECTED 1
#define SYSLOG_OK 0
#define SYSLOG_RECONNECTED 1

int dht_status = 0;
int led = 13;
bool ledState;
int dht_counter;
unsigned int localPort = 8888;
float h, t, f, hic, hif;
long rssi;  // WiFi signal strength
float bat; 
byte mac[6];

int syslog_status = SYSLOG_OK;
int wifi_status = WIFI_OK;

#define DHTPIN 6
#define VBATPIN A7
#define DHTTYPE DHT22

WiFiClient client;
DHT dht(DHTPIN, DHTTYPE);

void readDht();
void readBat();
void toggleLed();
void sendToSyslog();
void checkWifiConnectivity();

Ticker readDhtTicker(readDht, 5000);                                // 5s
Ticker readBatTicker(readBat, 5000);                                // 5s
Ticker sendToSyslogTicker(sendToSyslog, 60000);                     // 1m
Ticker toggleLedTicker(toggleLed, 1000);                            // .5s

void setup() {
  //Configure pins for Adafruit ATWINC1500 Feather
  WiFi.setPins(8,7,4,2);
  pinMode(led, OUTPUT);

  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  delay(5000);
  
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);

  connectWiFi();
  
  Serial.println("Connected to wifi");
  printWiFiStatus();

  connectSyslog();
  
  readDhtTicker.start();
  readBatTicker.start();
  sendToSyslogTicker.start();
  toggleLedTicker.start();
}

void connectSyslog() {
  if (! client.connected()) {
    Serial.print("\nConnecting to syslog ");
    client.connect(syslogServer,514);
    if (client.connected()) {
      Serial.println("done");
      syslog_status = SYSLOG_RECONNECTED;
    }
  }  
}

void connectWiFi() {
  Serial.print("\nWiFi status: ");
  Serial.println(WiFi.status());
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("\nConnecting to WiFi ");
    WiFi.begin(ssid, pass);
    Serial.print("\nWiFi status: ");
    Serial.println(WiFi.status());
    wifi_status = WIFI_RECONNECTED;
    delay(1000);
  }  
  rssi = WiFi.RSSI();
  WiFi.macAddress(mac);
}

void loop() {
  readDhtTicker.update();
  readBatTicker.update();
  sendToSyslogTicker.update();
  toggleLedTicker.update();
}

void toggleLed() { 
  // Activity indicator
  digitalWrite(led, ledState);
  ledState = !ledState;
}

void readDht() {
  Serial.print("+");
  h = dht.readHumidity();
  t = dht.readTemperature();
  f = dht.readTemperature(true); // Farenheit

  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    dht_status = 1;
    return;
  }

  hif = dht.computeHeatIndex(f, h);
  hic = dht.computeHeatIndex(t, h, false);
  dht_status = 0;
}

void readBat() {
  bat = analogRead(VBATPIN);
  bat *= 2;
  bat *= 3.3;
  bat /= 1024;
}

void sendToSyslog() {
  char syslog[256];

  connectWiFi();
  connectSyslog();
    
  if (!dht_status) {
    sprintf(syslog, "<14>templogger: mac=%02x:%02x:%02x:%02x:%02x:%02x signal_dbm=%d bat_v=%.2f wifi_status=%d syslog_status=%d humidity=%.2f temperature_c=%.2f temperature_f=%.2f heatindex_c=%.2f heatindex_f=%.2f", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0], rssi, bat, wifi_status, syslog_status, h, t, f, hic, hif);
    wifi_status = WIFI_OK;
    syslog_status = SYSLOG_OK;
  } else {
    sprintf(syslog, "<14>templogger: Failed to read DHT sensor data");
  }

  Serial.println("");
  Serial.println(syslog);
  client.println(syslog);
}

void printWiFiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI): ");
  Serial.print(rssi);
  Serial.println(" dBm");
}


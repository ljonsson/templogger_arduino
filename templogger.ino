#include <SPI.h>
#include <WiFi101.h>
#include <TimeLib.h> 
#include <WiFiUdp.h>
#include <DHT.h>
#include <TimeLib.h>
#include "templogger.h"  // ssid[], pass[], syslogServer and ntpServer

int status = WL_IDLE_STATUS;
int led = 13;
int toggle = 0;
int i = 0;
const int timeZone = -4;
unsigned int localPort = 8888;
float h, t, f, hic, hif;

#define DHTPIN 6
#define DHTTYPE DHT22

WiFiClient client;
WiFiUDP Udp;

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  //Configure pins for Adafruit ATWINC1500 Feather
  WiFi.setPins(8,7,4,2);
  pinMode(led, OUTPUT);

  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  delay(5000);
  
  if (Serial) { 
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
  }

  status = WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  if (Serial) {
    Serial.println("Connected to wifi");
    printWiFiStatus();
    Serial.print("\nConnecting to syslog server ... ");
  }

  client.connect(syslogServer,514);
  
  if (client.connected()) {
    if (Serial) {
      Serial.println("done");
    }
  }

  Udp.begin(localPort);
  if (Serial) {
    Serial.println("waiting for time sync");
  }
  setSyncProvider(getNtpTime);
}

void loop() {
  toggleLed();

  if (second()%5 == 0) {
    // Poll the DHT every 5 seconds so that it doesn't go to sleep
    status = readDHT();
  }

  if (second() == 0) {
    // Save data only once a minute
    sendToSyslog(status);
  }
  delay(1000);
}

int readDHT() {
  h = dht.readHumidity();
  t = dht.readTemperature();
  f = dht.readTemperature(true); // Farenheit

  if (isnan(h) || isnan(t) || isnan(f)) {
    if (Serial) {
      Serial.println("Failed to read from DHT sensor!");
    }
    return 1;
  }

  hif = dht.computeHeatIndex(f, h);
  hic = dht.computeHeatIndex(t, h, false);
  return 0;
}

void toggleLed() { 
  // Activity indicator
  if (toggle == 0) {
    digitalWrite(led, HIGH);
    toggle = 1;
  } else {
    digitalWrite(led, LOW);
    toggle = 0;     
  }
}

void sendToSyslog(int status) {
    if (Serial) {
      Serial.println("Sending syslog data!");
    }
    if (!status) {
      client.print("<14>templogger:");
      client.print(" humidity=");
      client.print(h);
      client.print(" temperature_c=");
      client.print(t);
      client.print(" temperature_f=");
      client.print(f);
      client.print(" heatindex_c=");
      client.print(hic);
      client.print(" heatindex_f=");
      client.print(hif);
      client.println("");
    } else {
      client.println("<14>templogger: Failed to read DHT sensor data");
    }
}

void printWiFiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

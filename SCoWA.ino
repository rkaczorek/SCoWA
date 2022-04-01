/*
  Name: Servo Control over Web API
  Version: 1.0
  By: Radek Kaczorek, March 2022
  License: GNU General Public License v3.0
  This code controls servo motor over web api
  
  Hardware:
  - NodeMCU / ESP8266
  - Servo motor connected by default to D4

  Set these variables in SCoWA.h:
    SECRET_SSID "ssid"
    SECRET_PASS "password"
    HOSTNAME "scowa"
    SERVOPIN 2
    SERVOMIN 500
    SERVOMAX 2500
    SERVOSPEED 20
*/

#define VERSION 1.0

#include "SCoWA.h"
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "LittleFS.h"
#include <ArduinoJson.h>

Servo servo;
ESP8266WebServer server(80);

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;

uint8_t park = 0; // minimum servo position (not angle!)
uint8_t unpark = 180; // maximum servo position (not angle!)

void handleRoot() {
  // return version
  String message = "-= SCoWA v";
  message += VERSION;
  message += " =-\n";
  message += "Usage:\n";
  message += "http://";
  message += HOSTNAME;
  message += ".local";
  message += "/[goto?pos=int|park|unpark|set|reset|status] OR\n";
  message += "http://";
  message += WiFi.localIP().toString();
  message += "/[goto?pos=int|park|unpark|set|reset|status]";
  message += "\r\n";
  server.send(200, "text/plain", message);
}

void handleNotFound() {
  // return 404 and request
  String message = "Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void handleGoto() {
  uint8_t targetpos = servo.read();

  // parse http get arguments
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "pos") {
      targetpos = atoi(server.arg(i).c_str ());
    }
  }

  // goto
  if (targetpos != servo.read()) {
    gotoPos(targetpos);
  }

  server.send(200, "text/plain", "OK\r\n");
}

void handlePark() {
  gotoPos(park);
  server.send(200, "text/plain", "OK\r\n");
}

void handleUnpark() {
  gotoPos(unpark);
  server.send(200, "text/plain", "OK\r\n");
}

void handleStatus() {
  String message;
  uint8_t pos = servo.read();
  if (pos == park) {
    message = "parked";
  } else if (pos == unpark) {
    message = "unparked";
  } else {
    message = "custom";
  }

  message += " (";
  message += pos;
  message += ")";
  message += "\r\n";
  server.send(200, "text/plain", message);
}

void gotoPos(uint8_t targetpos) {
  // goto
  if (targetpos > servo.read()) {
    for (int pos = servo.read(); pos <= targetpos; pos += 1) {
      servo.write(pos);
      delay(SERVOSPEED);
    }
  } else if (targetpos < servo.read()) {
    for (int pos = servo.read(); pos >= targetpos; pos -= 1) {
      servo.write(pos);
      delay(SERVOSPEED);
    }    
  }
}

void handleSetParkPosition() {
  unpark = servo.read();

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial1.println("Failed to open config file for writing");
  } else {
    StaticJsonDocument<32> json;
    json["unpark"] = unpark;
    serializeJson(json, configFile);
  }
  configFile.close();
  server.send(200, "text/plain", "OK\r\n");
}

void handleResetParkPosition() {
  unpark = 180;
  LittleFS.format();
  server.send(200, "text/plain", "OK\r\n");
}

void setup() {
  Serial.begin(115200);

  Serial.print("-= SCoWA v");
  Serial.print(VERSION);
  Serial.println(" =-");

  if (LittleFS.begin()) {
    if (LittleFS.exists("/config.json")) {
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        StaticJsonDocument<32> json;
        deserializeJson(json, configFile);
        unpark = json["unpark"];
      }
      configFile.close();
    }
  } else {
    Serial.println("Failed to mount FS");
  }

  servo.attach(SERVOPIN, SERVOMIN, SERVOMAX); //D4
  servo.write(park);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (!MDNS.begin(HOSTNAME)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("mDNS responder started");

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.on("/goto", handleGoto);
  server.on("/park", handlePark);
  server.on("/unpark", handleUnpark);
  server.on("/set", handleSetParkPosition);
  server.on("/reset", handleResetParkPosition);
  server.on("/status", handleStatus);

  server.begin();
  Serial.println("HTTP server started");

  Serial.println("");
  Serial.println("Usage:");
  Serial.print("http://");
  Serial.print(HOSTNAME);
  Serial.print(".local");
  Serial.println("/[goto?pos=int|park|unpark|set|reset|status] OR");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/[goto?pos=int|park|unpark|set|reset|status]");
}

void loop() {
  MDNS.update();
  server.handleClient();
}

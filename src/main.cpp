#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiUdp.h>
#include "credentials.h"
#include "ESP8266HTTPClient.h"
#include <ArduinoJson.h>

#define ONE_WIRE_BUS 2
#define RELAY_BUS 0

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempSensor1;
boolean goingUp = false;
int deviceCount = 0;
String statusStr = "";
uint8_t sensor1[8];

//Settings from webserver
float minTemp;
float maxTemp;
const char* currentTime;
boolean shouldSleep;

DeviceAddress Thermometer;

ESP8266WebServer server(80);

WiFiUDP ntpUDP;

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    Serial.print("0x");
    if (deviceAddress[i] < 0x10)
      Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    if (i < 7)
      Serial.print(", ");
  }
  Serial.println("");
}

void handle_NotFound()
{
  server.send(404, "text/plain", "Not found");
}

void handle_OnConnect()
{
  //server.send(200, "text/html", SendHTML());
}

String SendHTML()
{
  String ptr = "<!DOCTYPE html>";
  ptr += "<html>";
  ptr += "<head>";
  ptr += "<title>ESP8266 with DS18B20 Temperature Monitor and Relay Switch</title>";
  ptr += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  ptr += "<link href='https://fonts.googleapis.com/css?family=Open+Sans:300,400,600' rel='stylesheet'>";

  ptr += "<style>";
  ptr += "html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #444444;}";
  ptr += "body{margin-top: 50px;} ";
  ptr += "h1 {margin: 50px auto 30px;} ";
  ptr += ".side-by-side{display: table-cell;vertical-align: middle;position: relative;}";
  ptr += ".text{font-weight: 600;font-size: 19px;width: 200px;}";
  ptr += ".temperature{font-weight: 300;font-size: 50px;padding-right: 15px;}";
  ptr += ".Sensor1 .temperature{color: #3B97D3;}";
  ptr += ".superscript{font-size: 17px;font-weight: 600;position: absolute;right: -5px;top: 15px;}";
  ptr += ".data{padding: 10px;}";
  ptr += ".container{display: table;margin: 0 auto;}";
  ptr += ".icon{width:82px}";
  ptr += "</style>";

  //AJAX to auto refresh the body
  ptr += "<script>\n";
  ptr += "setInterval(loadDoc,1000);\n";
  ptr += "function loadDoc() {\n";
  ptr += "var xhttp = new XMLHttpRequest();\n";
  ptr += "xhttp.onreadystatechange = function() {\n";
  ptr += "if (this.readyState == 4 && this.status == 200) {\n";
  ptr += "document.body.innerHTML =this.responseText}\n";
  ptr += "};\n";
  ptr += "xhttp.open(\"GET\", \"/\", true);\n";
  ptr += "xhttp.send();\n";
  ptr += "}\n";
  ptr += "</script>\n";

  ptr += "</head>";

  ptr += "<body>";
  ptr += "<h1>ESP8266 with DS18B20 Temperature Monitor and Relay Switch</h1>";
  ptr += "<div class='container'>";

  ptr += "<div class='data Sensor1'>";
  ptr += "<div class='side-by-side text'>Sensor 1</div>";
  ptr += "<div class='side-by-side temperature'>";
  ptr += (int)tempSensor1;
  ptr += "<span class='superscript'>&deg;C</span></div>";
  ptr += "</div>";

  ptr += "<div class='data Sensor1'>";
  ptr += "<div class='side-by-side text'>Status</div>";
  ptr += "<div class='side-by-side text'>" + statusStr + "</div>";
  ptr += "</div>";
  ptr += "</div>";

  ptr += "</body>";
  ptr += "</html>";
  return ptr;
}

void charToStringL(const char S[], String &D)
{
    byte at = 0;
    const char *p = S;
    D = "";

    while (*p++) {
      D.concat(S[at++]);
      }
}

void charToString(char S[], String &D)
{
 String rc(S);
 D = rc;
}

boolean GetProperties(String currentTemp)
{
  HTTPClient http;

  http.begin("http://maiden.pagekite.me/MelektroApi/getsettings/?statusStr="+currentTemp);
  http.addHeader("Content-Type", "text/plain;charset=UTF-8");
  int httpResponseCode = http.POST("");

  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    if (error)
    {
      Serial.print("deserializeJson error ");
      Serial.println(error.c_str());
      return false;
    }

    const char* tempPtr; 
    tempPtr = doc["minValue"];
    String tempStr;
    charToStringL(tempPtr, tempStr);
    minTemp = tempStr.toFloat();
    tempPtr = doc["maxValue"];
    charToStringL(tempPtr,tempStr);
    maxTemp = tempStr.toFloat();

    const char* shouldSleepPtr;
    String shouldSleepStr;
    shouldSleepPtr = doc["shouldSleep"];
    currentTime = doc["currentTime"];

    charToStringL(shouldSleepPtr, shouldSleepStr);
    shouldSleepStr.equals("true")  ? shouldSleep = true : shouldSleep = false;

    return true;
  }
  else
  {
    Serial.print("Error on sending PUT Request: ");
    Serial.println(httpResponseCode);
    return false;
  }

  http.end();
}

void setup()
{
  Serial.begin(115200);
  pinMode(RELAY_BUS, OUTPUT);
  digitalWrite(RELAY_BUS, HIGH);

  delay(100);

  sensors.begin();

  deviceCount = sensors.getDeviceCount();
  Serial.print(deviceCount, DEC);
  Serial.println(" devices.");
  Serial.println("");

  Serial.println("Printing addresses...");
  for (int i = 0; i < deviceCount; i++)
  {
    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.print(" : ");
    sensors.getAddress(Thermometer, i);
    printAddress(Thermometer);
  }

  if (sensors.getAddress(Thermometer, 0))
  {
    //Take the first sensor as the measuring sensor
    for (uint8_t i = 0; i < 8; i++)
    {
      sensor1[i] = Thermometer[i];
    }
  }

  Serial.println("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(5000);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  sensors.requestTemperatures();
  tempSensor1 = sensors.getTempC(sensor1); // Gets the values of the temperature

  statusStr = "";
  if (!GetProperties((String)tempSensor1))
  {
    statusStr = "Could not load Properties";
    //swith warming off
    digitalWrite(RELAY_BUS, HIGH);
  }
  else
  {
    String currentTimeStr;
    charToStringL(currentTime, currentTimeStr);
    if (shouldSleep)
    {
      statusStr = currentTimeStr + " - Sleeping";
      digitalWrite(RELAY_BUS, HIGH);
    }
    else
    {
      //handle temperature histeresis
      if (goingUp)
      {
        if (tempSensor1 >= maxTemp)
        {
          goingUp = false;
          statusStr = "Phase change from going up to going down";
        }
        else
        {
          //swith warming on
          digitalWrite(RELAY_BUS, LOW);
          statusStr = "going up - " + String(tempSensor1) + " < (Upper) " + String(maxTemp) + " Pad is ON";
        }
      }
      else
      {
        if (tempSensor1 <= minTemp)
        {
          statusStr = "Phase change from going down to going up";
          goingUp = true;
        }
        else
        {
          //swith warming off
          digitalWrite(RELAY_BUS, HIGH);
          statusStr = "going down - " + String(tempSensor1) + " > (Lower)" + String(minTemp) + " Pad is OFF";
        }
      }
    }

    statusStr = currentTimeStr + " - " + statusStr;
    Serial.println(statusStr);

    server.send(200, "text/html", SendHTML());
  }
  server.handleClient();
  delay(1000);
}
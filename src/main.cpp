#include <stdio.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include "credentials.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiUdp.h>

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50

#define DEBUG

IPAddress staticIP(192, 168, 63, 126);
IPAddress gateway(192, 168, 63, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 63, 21);
IPAddress dnsGoogle(8, 8, 8, 8);
String hostName = "serre01";
const char *propertyHost = "pastei05.local";

ESP8266WebServer httpRestServer(HTTP_REST_PORT);

WiFiUDP ntpUDP;

#define ONE_WIRE_BUS 2
#define RELAY_BUS 0
#define PROCESSING_DELAY 60000

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempSensor1 = 0;
boolean goingUp = false;
int deviceCount = 0;
String statusStr = " nothing ";
String settingStr = " empty ";
uint8_t sensor1[8];

//Settings from webserver
float minTemp;
float maxTemp;
const char *currentTime;
boolean shouldSleep;

DeviceAddress Thermometer;

void SerialPrintLine(String line)
{
#ifdef DEBUG
  Serial.println(line);
#endif
}

void SerialPrintLine(const Printable &line)
{
#ifdef DEBUG
  Serial.println(line);
#endif
}

void SerialPrint(String line)
{
#ifdef DEBUG
  Serial.print(line);
#endif
}

void SerialPrint(int i, int j)
{
#ifdef DEBUG
  Serial.print(i, j);
#endif
}

void SerialPrint(int i, unsigned char j)
{
#ifdef DEBUG
  Serial.print(i, j);
#endif
}

void SerialPrint(int i)
{
#ifdef DEBUG
  Serial.print(i);
#endif
}

int init_wifi()
{
  int retries = 0;

  SerialPrintLine("Connecting to WiFi");

  WiFi.config(staticIP, gateway, subnet, dns, dnsGoogle);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostName);
  WiFi.begin(ssid, password);

  while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY))
  {
    retries++;
    delay(WIFI_RETRY_DELAY);
    SerialPrint("#");
  }
  SerialPrintLine("");
  return WiFi.status();
}

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    SerialPrint("0x");
    if (deviceAddress[i] < 0x10)
      SerialPrint("0");
    Serial.print(deviceAddress[i], HEX);
    if (i < 7)
      SerialPrint(", ");
  }
  SerialPrintLine("");
}

void handle_NotFound()
{
  httpRestServer.send(404, "text/plain", "Not found");
}

void handle_OnConnect()
{
  //httpRestServer.send(200, "text/html", SendHTML());
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
  ptr += ".Sensor1 .Settings1 .temperature{color: #3B97D3;}";
  ptr += ".superscript{font-size: 17px;font-weight: 600;position: absolute;right: -5px;top: 15px;}";
  ptr += ".data{padding: 10px;}";
  ptr += ".container{display: table;margin: 0 auto;}";
  ptr += ".icon{width:82px}";
  ptr += "</style>";

  // //AJAX to auto refresh the body
  // ptr += "<script>\n";
  // ptr += "setInterval(loadDoc,10000);\n";
  // ptr += "function loadDoc() {\n";
  // ptr += "var xhttp = new XMLHttpRequest();\n";
  // ptr += "xhttp.onreadystatechange = function() {\n";
  // ptr += "if (this.readyState == 4 && this.status == 200) {\n";
  // ptr += "document.body.innerHTML =this.responseText}\n";
  // ptr += "};\n";
  // ptr += "xhttp.open(\"GET\", \"/\", true);\n";
  // ptr += "xhttp.send();\n";
  // ptr += "}\n";
  // ptr += "</script>\n";

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
  ptr += "<div class='side-by-side text'>" + statusStr + "<br /><br />" + settingStr + "</div>";
  ptr += "</div>";
  ptr += "</div>";

  // ptr += "<div class='data Settings1'>";
  // ptr += "<div class='side-by-side text'>Settings1</div>";
  // ptr += "<div class='side-by-side text'>" + settingStr + "</div>";
  // ptr += "</div>";
  // ptr += "</div>";

  ptr += "</body>";
  ptr += "</html>";
  return ptr;
}

void charToStringL(const char S[], String &D)
{
  byte at = 0;
  const char *p = S;
  D = "";

  while (*p++)
  {
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
  String url = "/getsettings/?statusStr=" + currentTemp;
  WiFiClient client;
  String response;
  if (client.connect(propertyHost, 8082))
  {
    client.print(String("GET " + url) + " HTTP/1.1\r\n" +
                 "Host: " + propertyHost + "\r\n" +
                 "Connection: close\r\n" +
                 "\r\n");

    while (client.connected() || client.available())
    {
      if (client.available())
      {
        String line = client.readStringUntil('\n');
        SerialPrintLine("line=" + line);
        if (line.indexOf("minValue") > 0)
        {
          response = line;
        }
      }
    }
    client.stop();
  }
  else
  {
    SerialPrintLine("connection failed!");
    client.stop();
    return false;
  }
  DynamicJsonDocument doc(1024);
  SerialPrintLine("response=" + response);
  DeserializationError error = deserializeJson(doc, response);
  if (error)
  {
    SerialPrint("deserializeJson error ");
    SerialPrintLine(error.c_str());
    return false;
  }

  const char *tempPtr;
  tempPtr = doc["minValue"];
  String tempStr;
  charToStringL(tempPtr, tempStr);
  minTemp = tempStr.toFloat();
  settingStr = "Minimum temperature=" + tempStr;

  tempPtr = doc["maxValue"];
  charToStringL(tempPtr, tempStr);
  maxTemp = tempStr.toFloat();
  settingStr += ", Maximum temperature=" + tempStr;

  SerialPrintLine("Response=" + response);

  tempPtr = doc["wakeUpTime"];
  charToStringL(tempPtr, tempStr);
  settingStr += ", Wakeup time=" + tempStr;

  tempPtr = doc["sleepTime"];
  charToStringL(tempPtr, tempStr);
  settingStr += ", Sleep time=" + tempStr;

  const char *shouldSleepPtr;
  String shouldSleepStr;
  shouldSleepPtr = doc["shouldSleep"];

  charToStringL(shouldSleepPtr, shouldSleepStr);
  shouldSleepStr.equals("true") ? shouldSleep = true : shouldSleep = false;

  currentTime = doc["currentTime"];

  return true;
}

void ConfigRestServerRouting()
{
  httpRestServer.on("/", HTTP_GET, []() {
    httpRestServer.send(200, "text/html", SendHTML());
  });
}

void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
  delay(5000);
#endif

  pinMode(RELAY_BUS, OUTPUT);
  digitalWrite(RELAY_BUS, HIGH);

  delay(100);

  SerialPrintLine("Connecting to ");
  SerialPrintLine(ssid);

  if (init_wifi() == WL_CONNECTED)
  {
    SerialPrint("Connected to ");
    SerialPrint(ssid);
    SerialPrint("--- IP: ");
    SerialPrintLine(WiFi.localIP());
  }
  else
  {
    SerialPrint("Error connecting to: ");
    SerialPrintLine(ssid);
  }

  SerialPrintLine("");
  SerialPrintLine("WiFi connected..!");
  SerialPrint("Got IP: ");
  SerialPrintLine(WiFi.localIP());

  sensors.begin();
  deviceCount = sensors.getDeviceCount();
  //SerialPrint(deviceCount, DEC);
  SerialPrintLine(deviceCount + " devices.");
  SerialPrintLine("");

  if (deviceCount > 0)
  {
#ifdef DEBUG
    SerialPrintLine("Printing addresses...");
#endif
    for (int i = 0; i < deviceCount; i++)
    {
      SerialPrint("Sensor ");
      SerialPrint(i + 1);
      SerialPrint(" : ");
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
  }

  ConfigRestServerRouting();

  httpRestServer.begin();
  SerialPrintLine("HTTP httpRestServer started");
}

void loop()
{
  if (deviceCount > 0)
  {
    sensors.requestTemperatures();
    tempSensor1 = sensors.getTempC(sensor1); // Gets the values of the temperature
  }
  else
  {
    tempSensor1 = 1000;
  }

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
    SerialPrintLine(statusStr);
  }

  httpRestServer.handleClient();
  delay(PROCESSING_DELAY);
}

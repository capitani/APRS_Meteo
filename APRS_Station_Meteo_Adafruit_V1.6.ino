/*
   Mini Aprs Weather Station

   Date 2019-Sept-17

   esp8266 core 2.4.2
   ide 1.8.5

**** Thank-You ****
  https://lastminuteengineers.com/bme280-esp8266-weather-station/
  https://www.arduinolibraries.info/libraries/adafruit-bme280-library
  https://www.freemaptools.com/elevation-finder.htm
*******************

  NodeMCU and BME280 PIN ASSIGMENT
  3v --------- 3v
  Gnd -------- Gnd
  D5 --------- SCL
  D6 --------- SDA

  D4 --------- Push Button
  Gnd -------- Push Button

  Note: Read Sensor and Update WebPage Every 60sec.
        Broadcast to Aprs Server Every 10min.

  ///////////////////////////////////////////////////////////////////////////////////////////
*/

const String Ver = "Ver 1.6";

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 bme;

float tFar, temperature, humidity, pressure, altitude;
float tMin = 50.00;
float tMax = -50.00;
int Hum, Bar;

//////////////////
// Station Info //
//////////////////////////////////////////////////////////////////////////////
String ssid = "";
String password = "";

// System Uptime
byte hh = 0; byte mi = 0; byte ss = 0; // hours, minutes, seconds
unsigned int dddd = 0; // days (for if you want a calendar)
unsigned long lastTick = 0; // time that the clock last "ticked"
char timestring[25]; // for output
////////////////////////////////////////////

/////////// APRS STATION SETUP //////////////
String CallSign = "";
String Lat = "";
String Lon = "";
String Comment = "";
String aprsString = "";

/////////// APRS SERVER /////////////////////
String AprsServer = "";
String Password = "" ;
int AprsPort = 14580;
String BroadcastStatus = "Aprs Station Rebooted! Broadcast every 10min";

//////////// Altitude Setting ///////////////
int ALTITUDE = 90; //  https://www.freemaptools.com/elevation-finder.htm

bool Mode = 1;   // default mode, 0 = Imperial, 1 = Metric

//////////////////////////////////////////////////////////////////////////////
/////////////////////////////// END CONFIG ///////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

///////// Button CSS /////////
const String Button = ".button {background-color: white;border: 2px solid #555555;color: black;padding: 16px 32px;text-align: center;text-decoration: none;display: inline-block;font-size: 16px;margin: 4px 2px;-webkit-transition-duration: 0.4s;transition-duration: 0.4s;cursor: pointer;}.button:hover {background-color: #555555;color: white;}.disabled {opacity: 0.6;pointer-events: none;}";
/////////////////////////////

byte percentQ = 0; //wifi signal strength %

// Push Button
const int buttonPin = 2; // D4

/////////////
// Millis //
////////////////////////////////////////////////
const unsigned long Broadcast = 600000; // 10min broadcast
unsigned long previousBroadcastMillis = 0;
/////////// End Millis //////////

ESP8266WebServer server(80);
WiFiClient client;
String readString;

// OTA UPDATE
ESP8266HTTPUpdateServer httpUpdater; //ota

void setup() {
  // Wire.begin(int sda, int scl);
  Wire.begin(12, 14); // NodeMcu sda=D6, scl=D5
  delay(100);
  bme.begin(0x76);
  delay(100);
  /////////////////////////////////////

  pinMode(buttonPin, INPUT_PULLUP);

  //////////////////////////////////////
  // START EEPROM
  EEPROM.begin(512);
  delay(200);

  //////////////////////////////////////
  // Init EEPROM
  byte Init = EEPROM.read(451);

  if (Init != 111) {
    for (int i = 0; i < 512; ++i)
    {
      EEPROM.write(i, 0);
    }
    delay(100);
    EEPROM.commit();
    delay(200);

    EEPROM.write(451, 111);
    delay(100);
    EEPROM.commit();
    delay(200);
  }

  //////////////////////////////////////
  // READ EEPROM SSID & PASSWORD
  String esid;
  for (int i = 34; i < 67; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  delay(200);
  ssid = esid;

  String epass = "";
  for (int i = 67; i < 100; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  delay(200);
  password = epass;

  /////// APRSSERVER ////////
  int AprsServerLength = EEPROM.read(100) + 101;
  String eaprs = "";
  for (int i = 101; i < AprsServerLength; ++i)
  {
    eaprs += char(EEPROM.read(i));
  }
  AprsServer = eaprs;

  ////////// CALL SIGN //////////
  int CallSignlength = EEPROM.read(150) + 151;
  delay(200);
  String eCallSign = "";
  for (int i = 151; i < CallSignlength; ++i)
  {
    eCallSign += char(EEPROM.read(i));
  }
  CallSign = eCallSign;

  ////////// Comment ////////
  int eCommentlength = EEPROM.read(300) + 301;
  delay(200);
  String eComment = "";
  for ( int i = 301; i < eCommentlength; ++i)
  {
    eComment += char(EEPROM.read(i));
  }
  Comment = eComment;

  ////// APRS PASSWORD ///////
  int Passwordlength = EEPROM.read(330) + 331;
  delay(200);
  String ePassword = "";
  for (int i = 331; i < Passwordlength; ++i)
  {
    ePassword += char(EEPROM.read(i));
  }
  Password = ePassword;

  ///////// APRSPORT ////////
  int AprsPortLength = EEPROM.read(340) + 341;
  String eaprsport = "";
  for (int i = 341; i < AprsPortLength; ++i)
  {
    eaprsport += char(EEPROM.read(i));
  }
  AprsPort = (eaprsport.toInt());

  ////////// LATITUDE ///////
  int Latlength = EEPROM.read(350) + 351;
  delay(200);
  String eLat = "";
  for (int i = 351; i < Latlength; ++i)
  {
    eLat += char(EEPROM.read(i));
  }
  Lat = eLat;

  ///////// LONGITUDE ////////
  int Lonlength = EEPROM.read(360) + 361;
  delay(200);
  String eLon = "";
  for (int i = 361; i < Lonlength; ++i)
  {
    eLon += char(EEPROM.read(i));
  }
  Lon = eLon;

  ///////// ALTITUDE ////////
  int Altlength = EEPROM.read(370) + 371;
  delay(200);
  String eAlt = "";
  for (int i = 371; i < Altlength; ++i)
  {
    eAlt += char(EEPROM.read(i));
  }
  ALTITUDE = eAlt.toInt();

  EEPROM.end();

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, // temperature
                  Adafruit_BME280::SAMPLING_X1, // pressure
                  Adafruit_BME280::SAMPLING_X1, // humidity
                  Adafruit_BME280::FILTER_OFF   );

  // suggested rate is 1/60Hz (1m)

  ////////
  // API //
  ///////////////////////////////////////////////////////////////
  server.on("/api", []() {

    String JsonResponse = "{\n\"station\": [";
    JsonResponse += "{\n\"call\":\"" + String (CallSign) + "\", \n";
    JsonResponse += "\"lat\":\"" + String (Lat) + "\", \n";
    JsonResponse += "\"lon\":\"" + String (Lon) + "\", \n";

    if (Mode == 1) {
      JsonResponse += "\"temp\":\"" + String (temperature) + "\", \n";
    } else {
      JsonResponse += "\"temp\":\"" + String (tFar) + "\", \n";
    }

    JsonResponse += "\"hum\":\"" + String (Hum) + "\", \n";
    JsonResponse += "\"pres\":\"" + String (pressure) + "\", \n";
    JsonResponse += "\"alt\":\"" + String (altitude) + "\", \n";
    JsonResponse += "\"wifi_signal\":\"" + String (percentQ) + "\", \n";

    if (Mode == 1) {
      JsonResponse += "\"unity\":\"metric\", \n";

    } else {
      JsonResponse += "\"unity\":\"imperial\", \n";
    }

    JsonResponse += "\"server\":\"" + String (AprsServer) + "\", \n";

    JsonResponse += "\"status\":\"" + String (BroadcastStatus) + "\"";

    JsonResponse += "\n}\n]\n}";

    server.send(200, "application/json",  JsonResponse);
  });

  ///////////////////
  // SSID PASSWORD //
  ///////////////////////////////////////////////////////////////
  server.on("/wifi", []() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!server.authenticate("admin", password.c_str()))
        return server.requestAuthentication();
    }

    String WifiSsid = server.arg("ssid");
    String WifiPassword = server.arg("pass");

    if (WifiPassword.length() > 9 and WifiPassword.length() < 33) {
      // START EEPROM
      EEPROM.begin(512);
      delay(200);

      for (int i = 34; i < 100; ++i)
      {
        EEPROM.write(i, 0);
      }
      delay(100);
      EEPROM.commit();
      delay(200);

      for (int i = 0; i < WifiSsid.length(); ++i)
      {
        EEPROM.write(34 + i, WifiSsid[i]);
      }

      for (int i = 0; i < WifiPassword.length(); ++i)
      {
        EEPROM.write(67 + i, WifiPassword[i]);
      }
      delay(100);
      EEPROM.commit();
      delay(200);
      EEPROM.end();

      handleREBOOT();
    }
    else if (WifiPassword.length() <= 9 or WifiPassword.length() > 32) {
      server.send(200, "text/html", "<header><h1>Error!, Please enter valid PASS! min10 max32 character <a href=/admin >Back!</a></h1></header>");
    }
    else {
      server.send(200, "text/html", "<header><h1>Error!, Please enter PASS! <a href=/admin >Back!</a></h1></header>");
    }
  });

  ///////////////////
  // Station Info //
  ///////////////////////////////////////////////////////////////
  server.on("/station", []() {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();

    String ReadCallSign = server.arg("callsign");
    String ReadLat = server.arg("lat");
    String ReadLon = server.arg("lon");
    String ReadAlt = server.arg("alt");
    String ReadComment = server.arg("comment");

    // START EEPROM
    EEPROM.begin(512);
    delay(200);

    //////////////////////////////////////////////////////
    if (ReadCallSign.length() > 0) {
      EEPROM.write(150, ReadCallSign.length());

      for (int i = 0; i < ReadCallSign.length(); ++i)
      {
        EEPROM.write(151 + i, ReadCallSign[i]);
      }
      CallSign = ReadCallSign;
    }
    //////////////////////////////////////////////////////
    if (ReadLat.length() > 0) {
      EEPROM.write(350, ReadLat.length());

      for (int i = 0; i < ReadLat.length(); ++i)
      {
        EEPROM.write(351 + i, ReadLat[i]);
      }
      Lat = ReadLat;
    }
    //////////////////////////////////////////////////////
    if (ReadLon.length() > 0) {
      EEPROM.write(360, ReadLon.length());

      for (int i = 0; i < ReadLon.length(); ++i)
      {
        EEPROM.write(361 + i, ReadLon[i]);
      }
      Lon = ReadLon;
    }
    //////////////////////////////////////////////////////
    if (ReadAlt.length() > 0) {
      EEPROM.write(370, ReadAlt.length());

      for (int i = 0; i < ReadAlt.length(); ++i)
      {
        EEPROM.write(371 + i, ReadAlt[i]);
      }
      ALTITUDE = ReadAlt.toInt();
    }
    //////////////////////////////////////////////////////
    if (ReadComment.length() > 0) {
      EEPROM.write(300, ReadComment.length());

      for (int i = 0; i < ReadComment.length(); ++i)
      {
        EEPROM.write(301 + i, ReadComment[i]);
      }
      Comment = ReadComment;
    }
    //////////////////////////////////////////////////////

    delay(100);
    EEPROM.commit();
    delay(200);
    EEPROM.end();

    handleADMIN();
  });

  ///////////////////
  // Station Info //
  ///////////////////////////////////////////////////////////////
  server.on("/server", []() {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();

    String ReadAddress = server.arg("address");
    String ReadPort = server.arg("port");
    String ReadPassword = server.arg("password");

    // START EEPROM
    EEPROM.begin(512);
    delay(200);

    //////////////////////////////////////////////////////
    if (ReadAddress.length() > 0) {
      EEPROM.write(100, ReadAddress.length());

      for (int i = 0; i < ReadAddress.length(); ++i)
      {
        EEPROM.write(101 + i, ReadAddress[i]);
      }
      AprsServer = ReadAddress;
    }
    //////////////////////////////////////////////////////
    if (ReadPort.length() > 0) {
      EEPROM.write(340, ReadPort.length());

      for (int i = 0; i < ReadPort.length(); ++i)
      {
        EEPROM.write(341 + i, ReadPort[i]);
      }
      AprsPort = ReadPort.toInt();
    }
    //////////////////////////////////////////////////////
    if (ReadPassword.length() > 0) {
      EEPROM.write(330, ReadPassword.length());

      for (int i = 0; i < ReadPassword.length(); ++i)
      {
        EEPROM.write(331 + i, ReadPassword[i]);
      }
      Password = ReadPassword;
    }
    //////////////////////////////////////////////////////

    delay(100);
    EEPROM.commit();
    delay(200);
    EEPROM.end();

    handleADMIN();
  });

  //////////////////////
  // WiFi Connection //
  ////////////////////////////////////////////////////////////
  if (Init != 111 or ssid == "") {
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAP("APRS_Weather_Station", password.c_str());
  }
  else {
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    WiFi.softAP("APRS_Weather_Station", password.c_str());
    delay(100);
    WiFi.hostname("APRS-WX");
    delay(100);
    WiFi.begin(ssid.c_str(), password.c_str());
    delay(5000);
  }

  httpUpdater.setup(&server, "/firmware", "admin", password.c_str());

  server.onNotFound(handle_NotFound);

  server.on("/", handle_OnConnect);
  server.on("/admin", handleADMIN);
  server.on("/sw", handleSW);
  server.on("/reboot", handleREBOOT);

  ////////////
  // Server //
  ////////////////////////////////////////////////////////////
  server.begin();
  delay(100);

  if (!MDNS.begin("aprs-wx")) { // Start the mDNS responder for aprs.local
  }

  BME280Read();
}

void loop() {
  server.handleClient();
  unsigned long currentMillis = millis();

  ////////////////////
  // RESET PASSWORD //
  ////////////////////////////////////////////////////////////
  bool PushButton = digitalRead(buttonPin);
  int PushButtonCount = 0;
  if (PushButton == LOW)
  {
    while (PushButton == LOW) {
      PushButton = digitalRead(buttonPin);

      delay(1000);
      PushButtonCount++;

      if (PushButtonCount >= 10) {

        // START EEPROM
        EEPROM.begin(512);
        delay(200);

        // Erase SSID and PASSWORD
        for (int i = 34; i < 100; ++i)
        {
          EEPROM.write(i, 0);
        }
        delay(100);
        EEPROM.commit();
        delay(200);

        String WifiPassword = "weatherstation";

        for (int i = 0; i < WifiPassword.length(); ++i)
        {
          EEPROM.write(67 + i, WifiPassword[i]);
        }

        delay(100);
        EEPROM.commit();
        delay(200);
        EEPROM.end();

        delay(1000);
        WiFi.disconnect();
        delay(3000);
        ESP.restart();
      }
    }
  }

  /////////////
  // MILLIS //
  ////////////////////////////////////////////////////////////
  if (currentMillis - previousBroadcastMillis >= Broadcast) {
    APRS();

    previousBroadcastMillis = currentMillis;
  }

  // System Uptime
  if ((micros() - lastTick) >= 1000000UL) {
    lastTick += 1000000UL;
    ss++;
    if (ss >= 60) {
      ss -= 60;
      mi++;
    }
    if (mi >= 60) {
      mi -= 60;
      hh++;
    }
    if (hh >= 24) {
      hh -= 24;
      dddd++;
    }
  }
  ///////////////////////////////////////////////////////////
  yield();
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(temperature, humidity, pressure, altitude));
}

void handleSW() {
  if (server.arg("sw") == "Metric") {
    Mode = 1;
  } else {
    Mode = 0;
  }
  BME280Read();
  server.send(200, "text/html", SendHTML(temperature, humidity, pressure, altitude));
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float temperature, float humidity, float pressure, float altitude) {
  temperature = round(temperature);

  String ptr = (F("<!DOCTYPE html>"));
  ptr += (F("<html>"));
  ptr += (F("<head>"));
  ptr += (F("<title>APRS Weather Station</title>"));
  ptr += (F("<meta name='viewport' content='width=device-width, initial-scale=1.0'>"));
  ptr += (F("<link href='https://fonts.googleapis.com/css?family=Open+Sans:300,400,600' rel='stylesheet'>"));
  ptr += (F("<style>"));
  ptr += (F("html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #444444;}"));
  ptr += (F("body{margin: 0px;} "));
  ptr += (F("h1 {margin: 50px auto 30px;} "));
  ptr += (F(".side-by-side{display: table-cell;vertical-align: middle;position: relative;}"));
  ptr += (F(".text{font-weight: 600;font-size: 19px;width: 200px;}"));
  ptr += (F(".reading{font-weight: 300;font-size: 50px;padding-right: 25px;}"));
  ptr += (F(".temperature .reading{color: #F29C1F;}"));
  ptr += (F(".humidity .reading{color: #3B97D3;}"));
  ptr += (F(".pressure .reading{color: #26B99A;}"));
  ptr += (F(".altitude .reading{color: #955BA5;}"));
  ptr += (F(".superscript{font-size: 17px;font-weight: 600;position: absolute;top: 10px;}"));
  ptr += (F(".data{padding: 10px;}"));
  ptr += (F(".container{display: table;margin: 0 auto;}"));
  ptr += (F(".icon{width:65px}"));
  ptr += (Button);
  ptr += (F("</style>"));

  // AJAX script
  ptr += (F("<script>\n"));
  ptr += (F("setInterval(loadDoc,1000);\n")); // Update WebPage Every 1sec
  ptr += (F("function loadDoc() {\n"));
  ptr += (F("var xhttp = new XMLHttpRequest();\n"));
  ptr += (F("xhttp.onreadystatechange = function() {\n"));
  ptr += (F("if (this.readyState == 4 && this.status == 200) {\n"));
  ptr += (F("document.body.innerHTML =this.responseText}\n"));
  ptr += (F("};\n"));
  ptr += (F("xhttp.open(\"GET\", \"/\", true);\n"));
  ptr += (F("xhttp.send();\n"));
  ptr += (F("}\n"));
  ptr += (F("</script>\n"));
  ///////////////////////

  ptr += (F("</head>"));
  ptr += (F("<body>"));
  ptr += (F("<h1>"));
  ptr += (String)CallSign;
  ptr += (F("</h1>"));
  ptr += (F("<small>"));
  ptr += (F("Lat: "));
  ptr += (Lat);
  ptr += (F("<br>Lon: "));
  ptr += (Lon);
  ptr += (F("<br>Comment: "));
  ptr += (Comment);
  ptr += (F("<br><br>APRS Server: "));
  ptr += (AprsServer);
  ptr += (F("<br>APRS Port: "));
  ptr += (AprsPort);

  ptr += (F("</small><br><h2>APRS Weather Station</h2><br>"));
  ptr += (F("<div class='container'>"));
  //////////////////////////////////////////////
  ptr += (F("<div class='data temperature'>"));
  ptr += (F("<div class='side-by-side icon'>"));
  ptr += (F("<svg enable-background='new 0 0 19.438 54.003'height=54.003px id=Layer_1 version=1.1 viewBox='0 0 19.438 54.003'width=19.438px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path d='M11.976,8.82v-2h4.084V6.063C16.06,2.715,13.345,0,9.996,0H9.313C5.965,0,3.252,2.715,3.252,6.063v30.982"));
  ptr += (F("C1.261,38.825,0,41.403,0,44.286c0,5.367,4.351,9.718,9.719,9.718c5.368,0,9.719-4.351,9.719-9.718"));
  ptr += (F("c0-2.943-1.312-5.574-3.378-7.355V18.436h-3.914v-2h3.914v-2.808h-4.084v-2h4.084V8.82H11.976z M15.302,44.833"));
  ptr += (F("c0,3.083-2.5,5.583-5.583,5.583s-5.583-2.5-5.583-5.583c0-2.279,1.368-4.236,3.326-5.104V24.257C7.462,23.01,8.472,22,9.719,22"));
  ptr += (F("s2.257,1.01,2.257,2.257V39.73C13.934,40.597,15.302,42.554,15.302,44.833z'fill=#F29C21 /></g></svg>"));
  ptr += (F("</div>"));
  ptr += (F("<div class='side-by-side text'>Temperature</div>"));
  ptr += (F("<div class='side-by-side reading'>"));

  if (Mode == 0) {
    int Far = round(tFar);
    ptr += (int)Far;
    ptr += (F("<span class='superscript'>&deg;F</span></div>"));
  }
  else {
    ptr += (int)temperature;
    ptr += (F("<span class='superscript'>&deg;C</span></div>"));
  }

  ptr += (F("</div>"));
  //////////////////////////////////////////////
  ptr += (F("<div class='data humidity'>"));
  ptr += (F("<div class='side-by-side icon'>"));
  ptr += (F("<svg enable-background='new 0 0 29.235 40.64'height=40.64px id=Layer_1 version=1.1 viewBox='0 0 29.235 40.64'width=29.235px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><path d='M14.618,0C14.618,0,0,17.95,0,26.022C0,34.096,6.544,40.64,14.618,40.64s14.617-6.544,14.617-14.617"));
  ptr += (F("C29.235,17.95,14.618,0,14.618,0z M13.667,37.135c-5.604,0-10.162-4.56-10.162-10.162c0-0.787,0.638-1.426,1.426-1.426"));
  ptr += (F("c0.787,0,1.425,0.639,1.425,1.426c0,4.031,3.28,7.312,7.311,7.312c0.787,0,1.425,0.638,1.425,1.425"));
  ptr += (F("C15.093,36.497,14.455,37.135,13.667,37.135z'fill=#3C97D3 /></svg>"));
  ptr += (F("</div>"));
  ptr += (F("<div class='side-by-side text'>Humidity</div>"));
  ptr += (F("<div class='side-by-side reading'>"));
  ptr += (int)humidity;
  ptr += (F("<span class='superscript'>%</span></div>"));
  ptr += (F("</div>"));
  //////////////////////////////////////////////
  ptr += (F("<div class='data pressure'>"));
  ptr += (F("<div class='side-by-side icon'>"));
  ptr += (F("<svg enable-background='new 0 0 40.542 40.541'height=40.541px id=Layer_1 version=1.1 viewBox='0 0 40.542 40.541'width=40.542px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path d='M34.313,20.271c0-0.552,0.447-1,1-1h5.178c-0.236-4.841-2.163-9.228-5.214-12.593l-3.425,3.424"));
  ptr += (F("c-0.195,0.195-0.451,0.293-0.707,0.293s-0.512-0.098-0.707-0.293c-0.391-0.391-0.391-1.023,0-1.414l3.425-3.424"));
  ptr += (F("c-3.375-3.059-7.776-4.987-12.634-5.215c0.015,0.067,0.041,0.13,0.041,0.202v4.687c0,0.552-0.447,1-1,1s-1-0.448-1-1V0.25"));
  ptr += (F("c0-0.071,0.026-0.134,0.041-0.202C14.39,0.279,9.936,2.256,6.544,5.385l3.576,3.577c0.391,0.391,0.391,1.024,0,1.414"));
  ptr += (F("c-0.195,0.195-0.451,0.293-0.707,0.293s-0.512-0.098-0.707-0.293L5.142,6.812c-2.98,3.348-4.858,7.682-5.092,12.459h4.804"));
  ptr += (F("c0.552,0,1,0.448,1,1s-0.448,1-1,1H0.05c0.525,10.728,9.362,19.271,20.22,19.271c10.857,0,19.696-8.543,20.22-19.271h-5.178"));
  ptr += (F("C34.76,21.271,34.313,20.823,34.313,20.271z M23.084,22.037c-0.559,1.561-2.274,2.372-3.833,1.814"));
  ptr += (F("c-1.561-0.557-2.373-2.272-1.815-3.833c0.372-1.041,1.263-1.737,2.277-1.928L25.2,7.202L22.497,19.05"));
  ptr += (F("C23.196,19.843,23.464,20.973,23.084,22.037z'fill=#26B999 /></g></svg>"));
  ptr += (F("</div>"));
  ptr += (F("<div class='side-by-side text'>Pressure</div>"));
  ptr += (F("<div class='side-by-side reading'>"));

  ptr += (float)pressure;
  ptr += (F("<span class='superscript'>mbar</span></div>"));

  ptr += (F("</div>"));
  //////////////////////////////////////////////
  ptr += (F("<div class='data altitude'>"));
  ptr += (F("<div class='side-by-side icon'>"));
  ptr += (F("<svg enable-background='new 0 0 58.422 40.639'height=40.639px id=Layer_1 version=1.1 viewBox='0 0 58.422 40.639'width=58.422px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path d='M58.203,37.754l0.007-0.004L42.09,9.935l-0.001,0.001c-0.356-0.543-0.969-0.902-1.667-0.902"));
  ptr += (F("c-0.655,0-1.231,0.32-1.595,0.808l-0.011-0.007l-0.039,0.067c-0.021,0.03-0.035,0.063-0.054,0.094L22.78,37.692l0.008,0.004"));
  ptr += (F("c-0.149,0.28-0.242,0.594-0.242,0.934c0,1.102,0.894,1.995,1.994,1.995v0.015h31.888c1.101,0,1.994-0.893,1.994-1.994"));
  ptr += (F("C58.422,38.323,58.339,38.024,58.203,37.754z'fill=#955BA5 /><path d='M19.704,38.674l-0.013-0.004l13.544-23.522L25.13,1.156l-0.002,0.001C24.671,0.459,23.885,0,22.985,0"));
  ptr += (F("c-0.84,0-1.582,0.41-2.051,1.038l-0.016-0.01L20.87,1.114c-0.025,0.039-0.046,0.082-0.068,0.124L0.299,36.851l0.013,0.004"));
  ptr += (F("C0.117,37.215,0,37.62,0,38.059c0,1.412,1.147,2.565,2.565,2.565v0.015h16.989c-0.091-0.256-0.149-0.526-0.149-0.813"));
  ptr += (F("C19.405,39.407,19.518,39.019,19.704,38.674z'fill=#955BA5 /></g></svg>"));
  ptr += (F("</div>"));
  ptr += (F("<div class='side-by-side text'>Altitude</div>"));
  ptr += (F("<div class='side-by-side reading'>"));

  if (Mode == 0) {
    ptr += (int)altitude;
    ptr += (F("<span class='superscript'>f</span></div>"));
  }
  else {
    ptr += (int)altitude;
    ptr += (F("<span class='superscript'>m</span></div>"));
  }

  ptr += (F("</div>"));
  ptr += (F("</div>"));
  ptr += (F("</body>"));

  if (Mode == 1) {
    ptr += (F("<br><b>min:</b> "));
    ptr += (tMin);
    ptr += (F("&deg;C &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"));
    ptr += (F(" <b> max:</b> "));
    ptr += (tMax);
    ptr += (F("&deg;C "));
  } else {
    float tFmin = (tMin * 9 / 5) + 32;
    float tFmax = (tMax * 9 / 5) + 32;
    ptr += (F("<br><b>min:</b> "));
    ptr += (tFmin);
    ptr += (F("&deg;F &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"));
    ptr += (F(" <b> max:</b> "));
    ptr += (tFmax);
    ptr += (F("&deg;F "));
  }

  ptr += (F("<hr><br>"));

  if (Mode == 0) {
    ptr += (F("<p><a href='sw?sw=Metric' class='button'>Metric</a></p>"));
  } else {
    ptr += (F("<p><a href='sw?sw=Imperial' class='button'>Imperial</a></p>"));
  }

  ptr += (F("<br><hr>"));
  ptr += "<p><h3><a href='https://www.aprs.fi/" + String(CallSign) + "' target='_blank'>https://www.aprs.fi/" + String(CallSign) + "</a></h3></p>";

  ptr += (F("<br><p><h2>Bosh BME280 Sensor</h2></p>"));
  ptr += (F("<p><a href ='/admin' class='button'>Admin</a></p>"));
  ptr += (F("<p>http://aprs-wx.local</p>"));
  ptr += (F("<p><font color = 'blue'><i>Signal Strength: </i></font> "));
  ptr += String(percentQ);
  ptr += (F(" %</p>"));

  // System Uptime
  sprintf(timestring, "%d days %02d:%02d:%02d", dddd, hh, mi, ss);
  ptr += (F("<p>System Uptime : "));
  ptr += String(timestring);
  ptr += (F("</p>"));
  ////////////////////////////////////////////////

  ptr += (F("<p><small>Mini APRS Weather Station "));
  ptr += (Ver);
  ptr += (F("</p><p><small>Made by VE2CUZ</small></p>"));
  ptr += (F("</html>\n"));
  return ptr;
}

///////////////////
// HANDLE REBOOT //
////////////////////////////////////////////////////////////
void handleREBOOT() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();
  }

  String Spinner = (F("<html>"));
  Spinner += (F("<head><center><meta http-equiv=\"refresh\" content=\"15;URL='/'\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><style>"));

  Spinner += (F(".loader {border: 16px solid #f3f3f3;border-radius: 50%;border-top: 16px solid #3498db;"));
  Spinner += (F("width: 120px;height: 120px;-webkit-animation: spin 2s linear infinite;animation: spin 2s linear infinite;}"));

  Spinner += (F("@-webkit-keyframes spin {0% { -webkit-transform: rotate(0deg); }100% { -webkit-transform: rotate(360deg); }}"));

  Spinner += (F("@keyframes spin {0% { transform: rotate(0deg); }100% { transform: rotate(360deg); }}"));

  Spinner += (F("</style></head>"));

  Spinner += (F("<body>"));
  Spinner += (F("<p><h2>Rebooting Please Wait...</h2></p>"));
  Spinner += (F("<div class=\"loader\"></div>"));
  Spinner += (F("</body></center></html>"));

  server.send(200, "text/html",  Spinner);
  delay(1000);
  WiFi.disconnect();
  delay(3000);
  ESP.restart();
}

//////////////////
// HANDLE ADMIN //
////////////////////////////////////////////////////////////
void handleADMIN() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();
  }

  String webSite = (F("<!DOCTYPE html><html><head>"));
  webSite += (F("<meta charset='UTF-8'>"));
  webSite += (F("<meta name='viewport' content='width=device-width, initial-scale=1'>"));
  webSite += (F("<link rel='stylesheet' href='https://www.w3schools.com/w3css/4/w3.css'>"));
  webSite += (F("<body class='w3-content' style='max-width:1300px'>"));

  webSite += (F("<style>"));

  webSite += (Button);
  webSite += (F("</style></head><body>"));

  webSite += (F("<title>APRS Weather Station Admin</title>"));

  //// First Grid: Logo & About
  webSite += (F("<body class='w3-content' style='max-width:1300px'>"));

  // Left Page
  webSite += (F("<div class='w3-half w3-blue-grey w3-container' style='height:1150px'>"));

  if (WiFi.status() == WL_CONNECTED) {
    String IP = (WiFi.localIP().toString());
    webSite += (F("<h2><p>Connected to <mark>"));
    webSite += WiFi.SSID();
    webSite += (F("</mark><p>Ip : "));
    webSite += IP;
    webSite += (F("</p></h2>"));
  }
  else {
    webSite += (F("<h2><p>WiFi Disconnect!</p></h2>"));
  }

  webSite += (F("<hr><p><font color=black>Wifi Available.</font></p>"));

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks(false, true);

  // sort by RSSI
  int indices[n];
  for (int i = 0; i < n; i++) {
    indices[i] = i;
  }
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
        std::swap(indices[i], indices[j]);
      }
    }
  }

  String st = "";
  if (n > 7) n = 7;
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<small><li>";
    st += WiFi.RSSI(indices[i]);
    st += " dBm, ";
    st += WiFi.SSID(indices[i]);
    st += "</small></li>";
  }

  webSite += (F("<p>"));
  webSite += st;
  webSite += (F("</p>"));
  ////////////////////////////////////////////////////////////
  //// Station EX
  webSite += (F("<hr><h1 style='font-family:verdana;color:blue;'><u>Station Sample</u></h1>\n"));
  webSite += (F("<p>Call Sign: VE2CUZ-WX</p>"));
  webSite += (F("<p>Lat: 4545.99N</p>"));
  webSite += (F("<p>Lon: 07400.79W</p>"));
  webSite += (F("<p><small><font color=black>Valid format ddmm.hh {degrees, minutes and hundreths of minutes}</small></p></font>"));
  webSite += (F("<p>Alt in Meter: <font color=black><a href='https://www.freemaptools.com/elevation-finder.htm' target='_blank'>Elevation-Finder</a></font></p>"));
  webSite += (F("<p>Comment: http://qsl.net/ve2cuz</p>"));

  //// Server EX
  webSite += (F("<hr><h1 style='font-family:verdana;color:blue;'><u>Server Sample</u></h1>\n"));
  webSite += (F("<p>Address: cwop.aprs.net</p>"));
  webSite += (F("<p>Port: 14580</p>"));
  webSite += (F("<p>Password: APRS Server Password</p>"));

  webSite += (F("<hr><b><mark>Note:</mark></b><font color=black> APRS_Weather_Station_Setup."));
  webSite += (F("<small><li>Push on button for 10sec to reset at default password, 'weatherstation'.</small></li>"));
  webSite += (F("<small><li>Access Point accessible at 192.168.4.1</small></li>"));
  webSite += (F("<small><li>API available at http://aprs-wx.local/api</small></li>"));
  webSite += (F("<small><li>Broadcast to APRS server every 10min.</small></li>"));
  webSite += (F("<small><li><mark>Admin Password = WiFi Password.</mark></small></li>"));
  webSite += (F("</div></font>"));

  //// Right Page
  webSite += (F("<div class='w3-half w3-black w3-container w3-center' style='height:1150px'>"));

  //// WiFi SSID & PASSWORD
  webSite += (F("<br><h1 style='font-family:verdana;color:blue;'><u>Wifi</u></h1>\n"));
  webSite += (F("<form method='get' action='wifi'><label>SSID: </label><input name='ssid' type='text' maxlength=32><br><br><label>PASS: </label><input name='pass' type='text' length=32><br><br><input type='submit'></form>"));
  webSite += (F("<hr>"));

  //// Station Config
  webSite += (F("<h1 style='font-family:verdana;color:red;'><b>WEATHER STATION</b></h1>\n"));
  webSite += (F("<hr><h1 style='font-family:verdana;color:blue;'><u>Station Info</u></h1>\n"));
  webSite += (F("<form method='get' action='station'><label>Call Sign: </label><input name='callsign' type='text' maxlength='9' value='"));
  webSite += String(CallSign);
  webSite += (F("'><br><br>"));
  webSite += (F("<label>Lat: </label><input name='lat' type='text' maxlength='9' value='"));
  webSite += String(Lat);
  webSite += (F("'><br><br>"));
  webSite += (F("<label>Lon: </label><input name='lon' type='text' maxlength='9' value='"));
  webSite += String(Lon);
  webSite += (F("'><br><br>"));
  webSite += (F("<label>Alt in Meter: </label><input name='alt' type='number' min='0' maxlength='9' value='"));
  webSite += int(ALTITUDE);
  webSite += (F("'><br><br>"));
  webSite += (F("<label>Comment: </label><input name='comment' type='text' maxlength='29' value='"));
  webSite += String(Comment);
  webSite += (F("'><br><br>"));
  webSite += (F("<input type='submit'></form>"));
  webSite += (F("<hr>"));

  //// Server Config
  webSite += (F("<h1 style='font-family:verdana;color:blue;'><u>Server Info</u></h1>\n"));
  webSite += (F("<form method='get' action='server'><label>Address: </label><input name='address' type='text' maxlength='32' value='"));
  webSite += String(AprsServer);
  webSite += (F("'><br><br>"));
  webSite += (F("<label>Port: </label><input name='port' type='number' min='0' max='99999' value='"));
  webSite += int(AprsPort);
  webSite += (F("'><br><br>"));
  webSite += (F("<label>Password: </label><input name='password' type='password' maxlength='9' value='"));
  webSite += String(Password);
  webSite += (F("'><br><br>"));
  webSite += (F("<input type='submit'></form>"));
  webSite += (F("<hr></div>"));

  webSite += (F("<footer class=w3-container w3-black w3-padding-16>"));
  webSite += (F("<p><a href ='/' class='button'>Home</a> "));
  webSite += (F("<a href=/reboot class='button' onclick=\"return confirm('Are you sure?');\">Reboot!</a>"));
  webSite += (F("<a href ='/firmware' class='button'>Update Firmware</a></p>"));
  webSite += (F("<h5 style='font-family:verdana;'>Broadcast Status: <font color=blue>"));
  webSite += String(BroadcastStatus);
  webSite += (F("</font></h5>"));
  webSite += String(aprsString);
  webSite += (F("</footer>"));

  webSite += (F("</body></html>"));

  server.send(200, "text/html",  webSite);
};

//////////////////////////////////////////
// Broadcast to APRS Server Every 10min //
/////////////////////////////////////////////////
void APRS() {

  BME280Read();

  int Far = round(tFar);

  String response = "";
  readString = "";

  //////// Temperature Process ///////
  String temp;
  temp = "";

  if (Far <= 9 && Far >= 1)
  {
    temp = String("00");
    temp += (Far);
  }

  else if (Far >= 10)
  {
    temp = String("0");
    temp += (Far);
  }

  else if (Far == 0)
  {
    temp = String("000");
  }

  else if (Far >= -9 && Far <= -1)
  {
    int pos = (Far * (-1));
    temp = String("-0");
    temp += (pos);
  }

  else if (Far <= -10)
  {
    int pos = (Far * (-1));
    temp = String("-");
    temp += (pos);
  }
  ///////// Humidity Process //////////
  String hum;
  hum = "";

  if (Hum <= 9 && Hum >= 1)
  {
    hum = String("0");
    hum += (Hum);
  }

  if (Hum >= 10 && Hum <= 99)
  {
    hum = (Hum);
  }

  if (Hum >= 100)
  {
    hum = String("00");
  }

  ////////// Bar Process /////////
  String bar;
  bar = "";

  if (Bar >= 10000)
  {
    bar = (Bar);
  }
  else {
    bar = String("0");
    bar += (Bar);
  }
  ////////// Make String //////////
  String Login = ("user " + (CallSign) + " pass " + (Password.c_str()) + " vers ESP8266Weather 1");
  String Station = ((CallSign) + ">APRSWX,TCPIP*,qAC,WIDE1-1:> Mini Weather Station to APRS " + (Ver.c_str()));

  aprsString = "";

  aprsString += (CallSign);
  aprsString += ">APRSWX,TCPIP*,qAC,WIDE1-1:=";
  aprsString += (Lat.c_str());
  aprsString += "/";
  aprsString += (Lon.c_str());
  aprsString += "_.../...g...t";
  aprsString += temp;
  aprsString += "r...p...P...h";
  aprsString += hum;
  aprsString += "b";
  aprsString += bar;
  aprsString += (Comment);

  ///////// Broadcast to APRS ////////////
  if (CallSign.length() > 0 && AprsServer.length() > 0 && client.connect(AprsServer.c_str() , AprsPort)) {

    String response = "";

    int timeout = millis() + 5000;
    while (client.available() == 0) {
      if (timeout - millis() < 0) {
        client.flush();
        client.stop();
        BroadcastStatus = (F("FAIL! TimeOut to Connect to APRS Server!"));
        return;
      }
    }

    while (client.available()) {
      response = client.readStringUntil('\r');

      if (response.indexOf("APRS") > 0)
      {
        client.println(Login);
        delay(100);
      }

      else if (response.indexOf("unverified") > 0)
      {
        client.flush();
        client.stop();
        BroadcastStatus = (F("FAIL! Please Check Password!"));
        return;
      }

      if (response.indexOf("verified") > 0)
      {
        client.println(aprsString);
        delay(100);
        client.println(Station);

        BroadcastStatus = (F("APRS Station Updated! :-)"));
      }
    }
  }
  else {
    BroadcastStatus = (F("FAIL! Check Setting!"));
  }
  delay(100);
  client.flush();
  client.stop();
  delay(1000);
}

//////////////////////
// Bosh BME280 Read //
////////////////////////////////////////////////
void BME280Read() {

  percentQ = 0;

  if (WiFi.RSSI() <= -100) {
    percentQ = 0;
  } else if (WiFi.RSSI() >= -50) {
    percentQ = 100;
  } else {
    percentQ = 2 * (WiFi.RSSI() + 100);
  }

  //////// APRS VARIABLE /////////////
  bme.takeForcedMeasurement(); // has no effect in normal mode

  temperature = bme.readTemperature(); // in Celcius
  tFar = (temperature * 9 / 5) + 32; //Convert Celcius to Fahrenheit

  // Min,Max Temp Value
  if (temperature > tMax) {
    tMax = temperature;
  }

  if (temperature < tMin) {
    tMin = temperature;
  }

  humidity = bme.readHumidity();
  Hum = humidity;

  pressure = bme.readPressure();
  pressure = bme.seaLevelForAltitude(ALTITUDE, pressure);
  pressure = pressure / 100.0F;
  Bar = pressure * 10;

  ////////////////////////////////////

  // Variable Display on WebPage

  if (Mode == 0) {
    altitude = ALTITUDE * 3.28084; //Convert Meter to Feet
  } else {
    altitude = ALTITUDE;
  }
}

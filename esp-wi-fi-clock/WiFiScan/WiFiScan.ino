/*
    This sketch demonstrates how to scan WiFi networks.
    The API is almost the same as with the WiFi Shield library,
    the most obvious difference being the different file you need to include:
*/
#include "ESP8266WiFi.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <ESP8266WebServer.h>

#define MAX_CHARS 30

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000); //7200 seconds of utc offset = UTC+2 (this has to be changed according to summer/winter time)

ESP8266WebServer server(80);

void handleRoot();
void handleLed();
void handleTemp();
void handleNotFound();

char outputString[MAX_CHARS] = "";

time_t rawtime;
struct tm * ti;
uint8_t month;
uint8_t day;
uint8_t week_day;

bool blink = true;
uint8_t mode = 0;
uint8_t color_preset_f = 0xFF;
uint32_t raw_foreground = 0xFFFFFF;
uint8_t color_preset_b = 1;
uint32_t raw_background = 0;
uint8_t brightness = 255;

unsigned long milli_time;
unsigned long previous_trigger;
bool difference_minute;

bool config_posted = true;

void setup() {
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n;
  do {
    n = WiFi.scanNetworks();
  } while(n < 1);

  for(int i = 0; i < n; i++) {
    if(WiFi.SSID(i) == "Wi-Fi SSID") {
      WiFi.begin("Wi-Fi SSID", "Wi-Fi Password");
      break;
    } 
  }

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
  }
  
  Serial.println(WiFi.localIP());

  timeClient.begin();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/change", HTTP_POST, handlePost);
  server.onNotFound(handleNotFound);

  server.begin();

  previous_trigger = millis();
  milli_time = 0; //trigger instantly
}

void loop() {
  server.handleClient();

  difference_minute = (millis() - previous_trigger) > milli_time;
  
  if(Serial.available()) {
    char inChar;
    while(inChar != '\n')
      if(Serial.available())
        inChar = (char)Serial.read();
    if(WiFi.status() != WL_CONNECTED) {
      while(WiFi.status() != WL_CONNECTED) {
        delay(50);
      }
    }
    if(difference_minute) {
      while(!timeClient.update()) ;

      previous_trigger = millis();
      milli_time = (60 - timeClient.getSeconds()) * 1000;
      difference_minute = false;
    }
  
    rawtime = timeClient.getEpochTime();
    ti = localtime (&rawtime);
  
    month = ti->tm_mon + 1;
    day = ti->tm_mday;
    week_day = timeClient.getDay();
  
    if((month == 3)  && ((day-week_day) >= 25) ||
       (month > 3)   && (month < 10) ||
       (month == 10) && ((day-week_day) <= 24))
      timeClient.setTimeOffset(7200);
    else
      timeClient.setTimeOffset(3600);

    if(strlen(outputString)!=0) {
      Serial.print(outputString);
      memset(outputString, '\0', strlen(outputString));
    }

    Serial.print("t");
    Serial.print(timeClient.getHours());
    Serial.print(" ");
    Serial.println(timeClient.getMinutes());
    config_posted = true;
  }
}

bool posted = false;

const String first = "<form action=\"change\" method=\"POST\"><div><table width=\"100%\">";
const String postedString = "<tr><th colspan=\"3\" style=\"color: red\">Command sent to server!</th></tr>";
const String blinkString = "<tr><th>Blinking : </th><th colspan=\"2\"><select name=\"blink\"><option value=\"on\">On</option><option value=\"off\""; //selected="selected"
const String modeString = ">Off</option></select></th></tr><tr><th>Mode : </th><th colspan=\"2\"><select name=\"mode\"><option value=\"clock\">Clock</option><option value=\"temperature\""; //selected="selected"
const String modeFireString = ">Temperature</option><option value=\"fire\""; //selected = selected
const String color_fString = ">Fire</option></select></th></tr><tr><th colspan=\"3\">Choose the colors: </th></tr><tr></tr><tr><th>Foreground</th><th></th><th>Background</th></tr><tr><th><input type=\"color\" name=\"color_f\" value=\"#";
const String color_bString = "\"></th><th></th><th><input type=\"color\" name=\"color_b\" value=\"#";
const String preset_fOffString = "\"></th><tr><th colspan=\"3\">OR</th></tr><tr><th colspan=\"3\">Choose presets</th></tr><tr><th><select name=\"preset_f\"><option value=\"colors\">Chosen color</option><option value=\"off\""; //selected = "selected"
const String preset_fRainbowString = ">Off</option><option value=\"rainbow\""; //selected = "selected"
const String preset_fOceanString = ">Rainbow</option><option value=\"ocean\""; //selected = "selected"
const String preset_fCloudsString = ">Ocean</option><option value=\"cloud\""; //selected = "selected"
const String preset_fLavaString = ">Clouds</option><option value=\"lava\""; //selected = "selected"
const String preset_fForestString = ">Lava</option><option value=\"forest\""; //selected = "selected"
const String preset_fPartyString = ">Forest</option><option value=\"party\""; //selected = "selected"
const String preset_bOffString = ">Party</option></select></th><th></th><th><select name=\"preset_b\"><option value=\"colors\">Chosen color</option><option value=\"off\""; //selected = "selected"
const String preset_bRainbowString = ">Off</option><option value=\"rainbow\""; //selected = "selected"
const String preset_bOceanString = ">Rainbow</option><option value=\"ocean\""; //selected = "selected"
const String preset_bCloudsString = ">Ocean</option><option value=\"cloud\""; //selected = "selected"
const String preset_bLavaString = ">Clouds</option><option value=\"lava\""; //selected = "selected"
const String preset_bForestString = ">Lava</option><option value=\"forest\""; //selected = "selected"
const String preset_bPartyString = ">Forest</option><option value=\"party\""; //selected = "selected"
const String brightnessString = ">Party</option></select></th></tr><tr></tr><tr><th colspan=\"3\">Brightness</th></tr><tr><th colspan=\"3\"><input type=\"range\" min=\"0\" max=\"255\" value=\"";
const String endString = "\" name=\"brightness\"></th></tr><tr></tr><tr><th colspan=\"3\"><input type=\"submit\" value=\"submit\"></th></tr></table></div></form>";
const String selected = " selected = \"selected\"";

void handleRoot(){
  String htmlPage = first;
  if(posted)
    htmlPage += postedString;
  htmlPage += blinkString;
  if(!blink)
    htmlPage += selected;
  htmlPage += modeString;
  if(mode == 1)
    htmlPage += selected;
  htmlPage += modeFireString;
  if(mode == 2)
    htmlPage += selected;
  htmlPage += color_fString + String(raw_foreground, HEX) + color_bString + String(raw_background, HEX) + preset_fOffString;
  if(color_preset_f == 0)
    htmlPage += selected;
  htmlPage += preset_fRainbowString;
  if(color_preset_f == 1)
    htmlPage += selected;
  htmlPage += preset_fOceanString;
  if(color_preset_f == 2)
    htmlPage += selected;
  htmlPage += preset_fCloudsString;
  if(color_preset_f == 3)
    htmlPage += selected;
  htmlPage += preset_fLavaString;
  if(color_preset_f == 4)
    htmlPage += selected;
  htmlPage += preset_fForestString;
  if(color_preset_f == 5)
    htmlPage += selected;
  htmlPage += preset_fPartyString;
  if(color_preset_f == 6)
    htmlPage += selected;
  htmlPage += preset_bOffString;
  if(color_preset_b == 0)
    htmlPage += selected;
  htmlPage += preset_bRainbowString;
  if(color_preset_b == 1)
    htmlPage += selected;
  htmlPage += preset_bOceanString;
  if(color_preset_b == 2)
    htmlPage += selected;
  htmlPage += preset_bCloudsString;
  if(color_preset_b == 3)
    htmlPage += selected;
  htmlPage += preset_bLavaString;
  if(color_preset_b == 4)
    htmlPage += selected;
  htmlPage += preset_bForestString;
  if(color_preset_b == 5)
    htmlPage += selected;
  htmlPage += preset_bPartyString;
  if(color_preset_b == 6)
    htmlPage += selected;
  htmlPage += brightnessString + brightness + endString;

  server.send(200, "text/html", htmlPage);
  
  posted = false;
}

void handlePost(){
  if(!config_posted) {
    handleRoot();
    return;
  }
  outputString[strlen(outputString)] = 'L';
  if(server.hasArg("blink")) {
    if(server.arg("blink") == "on") {
      blink = true;
      outputString[strlen(outputString)] = 'B';
    } else if(server.arg("blink") == "off") {
      blink = false;
      outputString[strlen(outputString)] = 'b';
    }
  }
  if(server.hasArg("mode")){
    if(server.arg("mode") == "clock") {
      mode = 0;
      outputString[strlen(outputString)] = 'C';
    } else if(server.arg("mode") == "temperature") {
      mode = 1;
      outputString[strlen(outputString)] = 'T';
    } else if(server.arg("mode") == "fire") {
      mode = 2;
      outputString[strlen(outputString)] = 'f';
    }
  }
  if(server.hasArg("preset_f")) {
    outputString[strlen(outputString)] = 'f';
    if(server.arg("preset_f") == "colors") {
      color_preset_f = 0xFF;
      outputString[strlen(outputString)] = 'c';
      if(server.hasArg("color_f")) {
        uint32_t color = strtoul(&server.arg("color_f").c_str()[1], NULL, 16);
        raw_foreground = color;
        sprintf(&outputString[strlen(outputString)], "%d", color);
      }
    } else if(server.arg("preset_f") == "off") {
      color_preset_f = 0;
      outputString[strlen(outputString)] = '0';
    } else if(server.arg("preset_f") == "rainbow") {
      color_preset_f = 1;
      outputString[strlen(outputString)] = '1';
    } else if(server.arg("preset_f") == "ocean") {
      color_preset_f = 2;
      outputString[strlen(outputString)] = '2';
    } else if(server.arg("preset_f") == "cloud") {
      color_preset_f = 3;
      outputString[strlen(outputString)] = '3';
    } else if(server.arg("preset_f") == "lava") {
      color_preset_f = 4;
      outputString[strlen(outputString)] = '4';
    } else if(server.arg("preset_f") == "forest") {
      color_preset_f = 5;
      outputString[strlen(outputString)] = '5';
    } else if(server.arg("preset_f") == "party") {
      color_preset_f = 6;
      outputString[strlen(outputString)] = '6';
    }
  }
  if(server.hasArg("preset_b")) {
    outputString[strlen(outputString)] = 'b';
    if(server.arg("preset_b") == "colors") {
      color_preset_b = 0xFF;
      outputString[strlen(outputString)] = 'c';
      if(server.hasArg("color_b")) {
        uint32_t color = strtoul(&server.arg("color_b").c_str()[1], NULL, 16);
        raw_background = color;
        sprintf(&outputString[strlen(outputString)], "%d", color);
      }
    } else if(server.arg("preset_b") == "off") {
      color_preset_b = 0;
      outputString[strlen(outputString)] = '0';
    } else if(server.arg("preset_b") == "rainbow") {
      color_preset_b = 1;
      outputString[strlen(outputString)] = '1';
    } else if(server.arg("preset_b") == "ocean") {
      color_preset_b = 2;
      outputString[strlen(outputString)] = '2';
    } else if(server.arg("preset_b") == "cloud") {
      color_preset_b = 3;
      outputString[strlen(outputString)] = '3';
    } else if(server.arg("preset_b") == "lava") {
      color_preset_b = 4;
      outputString[strlen(outputString)] = '4';
    } else if(server.arg("preset_b") == "forest") {
      color_preset_b = 5;
      outputString[strlen(outputString)] = '5';
    } else if(server.arg("preset_b") == "party") {
      color_preset_b = 6;
      outputString[strlen(outputString)] = '6';
    }
  }
  if(server.hasArg("brightness")) {
    outputString[strlen(outputString)] = 'B';
    brightness = strtoul(server.arg("brightness").c_str(), NULL, 10);
    sprintf(&outputString[strlen(outputString)], "%d", brightness);
  }
  posted = true;
  config_posted = false;
  handleRoot();
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

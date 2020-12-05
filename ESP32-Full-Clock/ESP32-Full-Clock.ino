/*
 *  This sketch demonstrates how to scan WiFi networks.
 *  The API is almost the same as with the WiFi Shield library,
 *  the most obvious difference being the different file you need to include:
 */
#include "WiFi.h"
#include <time.h>
#include <Wire.h>
#include <ds3231.h>
#include <NTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <EEPROM.h>

#define WIFI_TIMEOUT 20000
#define WIFI_DELAY_RETRY 500
#define EEPROM_SIZE 14

#include <FastLED.h>
#define LED_PIN 19
#define NUM_LEDS    128
#define LED_TYPE    WS2813
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];
bool led_FG[NUM_LEDS];

#define UPDATES_PER_SECOND 30
#define MILLIS_PER_SECOND 1000

#include "DHT.h"
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define MAX_NR_WATER_STATES 10

#define FILESYSTEM_VERSION 0x03

/* time variables*/
bool first_connect = true;
struct ts t;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000); //7200 seconds of utc offset = UTC+2 (this has to be changed according to summer/winter time)

/*server settings variables*/
WebServer server(80);

bool blink_on = true;

bool blink = true;
uint8_t color_preset_f = 0xFF;
uint32_t raw_foreground = 0xFFFFFF;
CRGB foreground = CRGB::White;
uint8_t color_preset_b = 1;
uint32_t raw_background = 0;
CRGB background = CRGB::White;
uint8_t brightness = 255;

static uint8_t nr_of_drops = 2;
static uint8_t water_speed = 6;

CRGBPalette16 currentPalette = PartyColors_p;
TBlendType    currentBlending = LINEARBLEND;

CRGBPalette16 timePalette = LavaColors_p;
TBlendType    timeBlending = LINEARBLEND;

static uint8_t startIndex = 0;

static uint8_t fire_height[20];

/* show current temperature */
float original_temp = 0;
int8_t temp = 0;

bool firstFire;
bool firstWater = true;

const uint8_t startNumber[] = {0, 8, 9, 21, 22, 35, 36, 49, 50, 63, 64, 77, 78, 91, 92, 105, 106, 118, 119, 127};

enum water_type {
  TYPE_STATE,
  TYPE_X,
  TYPE_Y
};

int8_t water_states[MAX_NR_WATER_STATES][3]; //max 3 drips at the same time

enum mode {
  mode_time = 0,
  mode_temp = 1,
  mode_fire = 2,
  mode_water_drips = 3
};
mode current_mode;
mode previous_mode;

static bool first_send = true;

void handleRoot();
void handlePost();
void handleNotFound();

static void set_number(uint8_t number, uint8_t position, bool start_offset);
void FillLEDsFromPaletteColors( uint8_t colorIndex);
void set_fire();
void set_water_animation();

void setup() {
  current_mode = mode_time;
  previous_mode = current_mode;
  memset(fire_height, 4, 20);

  dht.begin();

  EEPROM.begin(EEPROM_SIZE);

  random16_set_seed(analogRead(0));
  
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Wire.begin();
  DS3231_init(DS3231_INTCN);

  read_eeprom();

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );

  server.on("/", HTTP_GET, handleRoot);
  server.on("/change", HTTP_POST, handlePost);
  server.onNotFound(handleNotFound);
}

void read_eeprom() {
  if(EEPROM.read(0) == FILESYSTEM_VERSION) { //first byte is version byte to see if there's something already writen
    blink = EEPROM.read(1);
    current_mode = (mode)EEPROM.read(2);
    color_preset_f = EEPROM.read(3);
    foreground.red = EEPROM.read(4);
    foreground.green = EEPROM.read(5);
    foreground.blue = EEPROM.read(6);
    raw_foreground = (uint32_t)foreground.blue + ((uint32_t)foreground.green << 8) + ((uint32_t)foreground.red << 16);
    color_preset_b = EEPROM.read(7);
    background.red = EEPROM.read(8);
    background.green = EEPROM.read(9);
    background.blue = EEPROM.read(10);
    raw_background = (uint32_t)background.blue + ((uint32_t)background.green << 8) + ((uint32_t)background.red << 16);
    brightness = EEPROM.read(11);
    nr_of_drops = EEPROM.read(12);
    water_speed = EEPROM.read(13);
  } else
    write_eeprom();
}

void write_eeprom() {
  EEPROM.write(0, FILESYSTEM_VERSION); //version
  EEPROM.write(1, blink);
  EEPROM.write(2, current_mode);
  EEPROM.write(3, color_preset_f);
  EEPROM.write(4, foreground.red);
  EEPROM.write(5, foreground.green);
  EEPROM.write(6, foreground.blue);
  EEPROM.write(7, color_preset_b);
  EEPROM.write(8, background.red);
  EEPROM.write(9, background.green);
  EEPROM.write(10, background.blue);
  EEPROM.write(11, brightness);
  EEPROM.write(12, nr_of_drops);
  EEPROM.write(13, water_speed);

  EEPROM.commit();
}

void set_time_server() {
  while(!timeClient.update()) ;

  struct tm * ti;
  time_t rawtime = timeClient.getEpochTime();
  ti = localtime (&rawtime);

  t.mon = ti->tm_mon + 1;
  t.mday = ti->tm_mday;
  uint8_t week_day = timeClient.getDay();

  if((t.mon == 3)  && ((t.mday-week_day) >= 25) ||
     (t.mon > 3)   && (t.mon < 10) ||
     (t.mon == 10) && ((t.mday-week_day) <= 24))
    timeClient.setTimeOffset(7200);
  else
    timeClient.setTimeOffset(3600);

  t.hour = timeClient.getHours();
  t.min = timeClient.getMinutes();
  t.sec = timeClient.getSeconds();

  DS3231_set(t);
}

bool check_connection() {
  if(WiFi.status() != WL_CONNECTED) {
    int n = WiFi.scanNetworks();
    if(n < 1)
      return false;
    bool found = false;
    for(int i = 0; i < n; i++) {
      if(WiFi.SSID(i) == "WiFi-SSID") {
        WiFi.begin("WiFi-SSID", "WiFi-Password");
        found = true;
        break;
      }
    }
    if(!found)
      return false;

    int total_delay = 0;
    while((WiFi.status() != WL_CONNECTED) && (total_delay < WIFI_TIMEOUT)) {
      delay(WIFI_DELAY_RETRY);
      total_delay += WIFI_DELAY_RETRY;
    }
    
    return WiFi.status() == WL_CONNECTED;
  } else
    return true;
}

void update_temp() {
  original_temp = dht.readTemperature();
  if(isnan(original_temp)) {
    return;
  }
  temp = (int)round(original_temp);
  
  set_temp(temp);
}

void set_temp(int8_t temperature) {
  /* Minus symbol */
  led_FG[116] = (temp < 0); 
  led_FG[120] = (temp < 0);
  
  if(temp < 0)
    temp = - temp;
   
  set_number((temp - (temp % 10))/10, 0, true);
  set_number(temp % 10, 1, true);

  /* Degrees symbol */
  memset(&led_FG[39], true, 2);
  led_FG[44] = true;
  led_FG[46] = true;
  memset(&led_FG[54], true, 2);

  /* Celcius symbol */
  led_FG[7] = true; 
  led_FG[10] = true;
  led_FG[16] = true;
  led_FG[19] = true;
  memset(&led_FG[25], true, 3);
}

void blink_seconds(bool show) {
  led_FG[61] = show || !blink;
  led_FG[68] = show || !blink;
}

void set_time(uint8_t hours, uint8_t minutes) {
  set_number((hours - (hours % 10))/10, 0, false);
  set_number(hours % 10, 1, false);
  set_number((minutes - (minutes % 10))/10, 2, false);
  set_number(minutes % 10, 3, false);
}

void show_all() {
  startIndex = startIndex + 1;

  FillLEDsFromPaletteColors(startIndex);

  firstFire = false;
    
  FastLED.show();
}

unsigned long last_update_time       = 0;
unsigned long last_blink_time        = 0;
unsigned long last_temperature_time  = 0;
unsigned long last_fire_set_time     = 0;
unsigned long last_water_set_time    = 0;

void loop() {  
  if(millis() < (last_update_time + (MILLIS_PER_SECOND / UPDATES_PER_SECOND)))
    return;
  last_update_time = millis();
  if(!first_send && check_connection()) {
    if(first_connect) {
      set_time_server();
      MDNS.begin("clock");
      server.begin();
      first_connect = false;
    }
    server.handleClient();
  }

  first_send = false;

  DS3231_get(&t);

  if(current_mode != previous_mode) {
    firstFire = true;
    firstWater = true;
    memset(led_FG, false, NUM_LEDS);
    previous_mode = current_mode;
  }

  switch(current_mode) {
    case mode_time:
      if(millis() > (last_blink_time + MILLIS_PER_SECOND / 2)) { //update every half second
        blink_on = !blink_on;
        last_blink_time = millis();
      }
      blink_seconds(blink_on);
  
      set_time(t.hour, t.min);
      break;
    case mode_temp:
      if(millis() > (last_temperature_time + 2 * MILLIS_PER_SECOND)) { //update every 2 seconds
        update_temp();
        last_temperature_time = millis();
      }
      break;
    case mode_fire:
      if(millis() > (last_fire_set_time + MILLIS_PER_SECOND / 10)) { //update every 1/10 second
        set_fire();
        last_fire_set_time = millis();
      }
      break;
    case mode_water_drips:
      if(millis() > (last_water_set_time + MILLIS_PER_SECOND / water_speed)) { //update every 1/water_speed second
        set_water_animation();
        last_water_set_time = millis();
      }
      break;
  }

  show_all();
}

bool posted = false;

const String first = "<form action=\"change\" method=\"POST\"><div><table width=\"100%\">";
const String postedString = "<tr><th colspan=\"3\" style=\"color: red\">Command sent to server!</th></tr>";
const String blinkString = "<tr><th>Blinking : </th><th colspan=\"2\"><select name=\"blink\"><option value=\"on\">On</option><option value=\"off\""; //selected="selected"
const String modeString = ">Off</option></select></th></tr><tr><th>Mode : </th><th colspan=\"2\"><select name=\"mode\"><option value=\"clock\">Clock</option><option value=\"temperature\""; //selected="selected"
const String modeFireString = ">Temperature</option><option value=\"fire\""; //selected = selected
const String modeWaterDrips = ">Fire</option><option value=\"water\""; //selected = selected
const String color_fString = ">Water Drips</option></select></th></tr><tr><th colspan=\"3\">Choose the colors: </th></tr><tr></tr><tr><th>Foreground</th><th></th><th>Background</th></tr><tr><th><input type=\"color\" name=\"color_f\" value=\"#";
const String color_bString = "\"></th><th></th><th><input type=\"color\" name=\"color_b\" value=\"#";
const String preset_fOffString = "\"></th><tr><th colspan=\"3\">OR</th></tr><tr><th colspan=\"3\">Choose presets</th></tr><tr><th><select name=\"preset_f\"><option value=\"colors\">Chosen color</option><option value=\"off\""; //selected = "selected"
const String preset_fRainbowString = ">Off</option><option value=\"rainbow\""; //selected = "selected"
const String preset_fOceanString = ">Rainbow</option><option value=\"ocean\""; //selected = "selected"
const String preset_fCloudsString = ">Ocean</option><option value=\"cloud\""; //selected = "selected"
const String preset_fLavaString = ">Clouds</option><option value=\"lava\""; //selected = "selected"
const String preset_fForestString = ">Lava</option><option value=\"forest\""; //selected = "selected"
const String preset_fPartyString = ">Forest</option><option value=\"party\""; //selected = "selected"
const String preset_fFireString = ">Party</option><option value=\"fire\""; //selected = "selected"
const String preset_bOffString = ">Fire</option></select></th><th></th><th><select name=\"preset_b\"><option value=\"colors\">Chosen color</option><option value=\"off\""; //selected = "selected"
const String preset_bRainbowString = ">Off</option><option value=\"rainbow\""; //selected = "selected"
const String preset_bOceanString = ">Rainbow</option><option value=\"ocean\""; //selected = "selected"
const String preset_bCloudsString = ">Ocean</option><option value=\"cloud\""; //selected = "selected"
const String preset_bLavaString = ">Clouds</option><option value=\"lava\""; //selected = "selected"
const String preset_bForestString = ">Lava</option><option value=\"forest\""; //selected = "selected"
const String preset_bPartyString = ">Forest</option><option value=\"party\""; //selected = "selected"
const String preset_bFireString = ">Party</option><option value=\"fire\""; //selected = "selected"
const String brightnessString = ">Fire</option></select></th></tr><tr></tr><tr><th colspan=\"3\">Brightness</th></tr><tr><th colspan=\"3\"><input type=\"range\" min=\"0\" max=\"255\" value=\"";
const String waterDropString = "\" name=\"brightness\"></th></tr><tr></tr><tr><th></th><th>Drop count</th><th>Drop speed</th></tr><tr><th>Water</th><th><input type=\"range\" min=\"1\" max=\"10\" value=\"";
const String waterSpeedString = "\" name=\"waterCount\"></th><th><input type=\"range\" min=\"1\" max=\"10\" value=\"";
const String endString = "\" name=\"waterSpeed\"></th></tr><tr></tr><tr><th colspan=\"3\"><input type=\"submit\" value=\"submit\"></th></tr></table></div></form>";
const String selected = " selected = \"selected\"";

void handleRoot(){
  String htmlPage = first;
  if(posted)
    htmlPage += postedString;
  htmlPage += blinkString;
  if(!blink)
    htmlPage += selected;
  htmlPage += modeString;
  if(current_mode == mode_temp)
    htmlPage += selected;
  htmlPage += modeFireString;
  if(current_mode == mode_fire)
    htmlPage += selected;
  htmlPage += modeWaterDrips;
  if(current_mode == mode_water_drips)
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
  htmlPage += preset_fFireString;
  if(color_preset_f == 7)
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
  htmlPage += preset_bFireString;
  if(color_preset_b == 7)
    htmlPage += selected;
  htmlPage += brightnessString + brightness + waterDropString + nr_of_drops + waterSpeedString + water_speed + endString;

  server.send(200, "text/html", htmlPage);
  
  posted = false;
}

void handlePost(){  
  if(server.hasArg("blink")) {
    if(server.arg("blink") == "on") {
      blink = true;
    } else if(server.arg("blink") == "off") {
      blink = false;
    }
  }
  if(server.hasArg("mode")){
    if(server.arg("mode") == "clock") {
      current_mode = mode_time;
    } else if(server.arg("mode") == "temperature") {
      current_mode = mode_temp;
    } else if(server.arg("mode") == "fire") {
      current_mode = mode_fire;
    } else if(server.arg("mode") == "water") {
      current_mode = mode_water_drips;
    }
  }
  if(server.hasArg("preset_f")) {
    if(server.hasArg("color_f")) {
      uint32_t color = strtoul(&server.arg("color_f").c_str()[1], NULL, 16);
      raw_foreground = color;
      foreground.blue = raw_foreground & 0xFF;
      foreground.green = (raw_foreground >> 8) & 0xFF;
      foreground.red = (raw_foreground >> 16) & 0xFF;
    }
    if(server.arg("preset_f") == "colors") {
      color_preset_f = 0xFF;
    } else if(server.arg("preset_f") == "off") {
      color_preset_f = 0;
    } else if(server.arg("preset_f") == "rainbow") {
      color_preset_f = 1;
    } else if(server.arg("preset_f") == "ocean") {
      color_preset_f = 2;
    } else if(server.arg("preset_f") == "cloud") {
      color_preset_f = 3;
    } else if(server.arg("preset_f") == "lava") {
      color_preset_f = 4;
    } else if(server.arg("preset_f") == "forest") {
      color_preset_f = 5;
    } else if(server.arg("preset_f") == "party") {
      color_preset_f = 6;
    } else if(server.arg("preset_f") == "fire") {
      color_preset_f = 7;
    }
  }
  if(server.hasArg("preset_b")) {
    if(server.hasArg("color_b")) {
      uint32_t color = strtoul(&server.arg("color_b").c_str()[1], NULL, 16);
      raw_background = color;
      background.blue = raw_background & 0xFF;
      background.green = (raw_background >> 8) & 0xFF;
      background.red = (raw_background >> 16) & 0xFF; 
    }
    if(server.arg("preset_b") == "colors") {
      color_preset_b = 0xFF;
    } else if(server.arg("preset_b") == "off") {
      color_preset_b = 0;
    } else if(server.arg("preset_b") == "rainbow") {
      color_preset_b = 1;
    } else if(server.arg("preset_b") == "ocean") {
      color_preset_b = 2;
    } else if(server.arg("preset_b") == "cloud") {
      color_preset_b = 3;
    } else if(server.arg("preset_b") == "lava") {
      color_preset_b = 4;
    } else if(server.arg("preset_b") == "forest") {
      color_preset_b = 5;
    } else if(server.arg("preset_b") == "party") {
      color_preset_b = 6;
    } else if(server.arg("preset_b") == "fire") {
      color_preset_b = 7;
    }
  }
  if(server.hasArg("brightness")) {
    brightness = strtoul(server.arg("brightness").c_str(), NULL, 10);
  }

  if(server.hasArg("waterCount")) {
    uint8_t new_nr_of_drops = strtoul(server.arg("waterCount").c_str(), NULL, 10);
    if(new_nr_of_drops != nr_of_drops)
      firstWater = true;
    nr_of_drops = new_nr_of_drops;
  }

  if(server.hasArg("waterSpeed")) {
    water_speed = strtoul(server.arg("waterSpeed").c_str(), NULL, 10);
  }

  write_eeprom();
  
  posted = true;
  handleRoot();
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

void FillLEDsFromPaletteColors( uint8_t colorIndex) {
  for( int i = 0; i < NUM_LEDS; i++) {
    if(led_FG[i]) {
      if(color_preset_f == 0xFF) {
        leds[i].blue = foreground.blue * brightness / 255;
        leds[i].red = foreground.red * brightness / 255;
        leds[i].green = foreground.green * brightness / 255;
      } else if(color_preset_f == 0)
        leds[i] = CRGB::Black;
      else if(color_preset_f == 1)
        leds[i] = ColorFromPalette( RainbowColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_f == 2)
        leds[i] = ColorFromPalette( OceanColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_f == 3)
        leds[i] = ColorFromPalette( CloudColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_f == 4)
        leds[i] = ColorFromPalette( LavaColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_f == 5)
        leds[i] = ColorFromPalette( ForestColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_f == 6)
        leds[i] = ColorFromPalette( PartyColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_f == 7) {
        CHSV tempHSV = rgb2hsv_approximate(leds[i]);
        if(firstFire || (tempHSV.val == 0))
          leds[i] = CHSV( random8(90)%60 , 255, random8(153, 255) );
        else {
          uint8_t randomH = random8(16);
          if(randomH > 14 && tempHSV.hue < 53)
            tempHSV.hue += 7;
          else if(randomH > 12 && tempHSV.hue < 55)
            tempHSV.hue += 5;
          else if(randomH > 9 && tempHSV.hue < 57)
            tempHSV.hue += 3;
          else if(randomH < 2 && tempHSV.hue > 7)
            tempHSV.hue -= 7;
          else if(randomH < 3 && tempHSV.hue > 5)
            tempHSV.hue -= 5;
          else if(randomH < 6 && tempHSV.hue > 3)
            tempHSV.hue -= 3;
          tempHSV.sat = 255;
          leds[i] = tempHSV;
        }
      }
    } else {
      if(color_preset_b == 0xFF) {
        leds[i].blue = background.blue *  brightness / 255;
        leds[i].red = background.red * brightness / 255;
        leds[i].green = background.green * brightness / 255;
      } else if(color_preset_b == 0)
        leds[i] = CRGB::Black;
      else if(color_preset_b == 1)
        leds[i] = ColorFromPalette( RainbowColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_b == 2)
        leds[i] = ColorFromPalette( OceanColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_b == 3)
        leds[i] = ColorFromPalette( CloudColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_b == 4)
        leds[i] = ColorFromPalette( LavaColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_b == 5)
        leds[i] = ColorFromPalette( ForestColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_b == 6)
        leds[i] = ColorFromPalette( PartyColors_p, colorIndex, brightness, currentBlending);
      else if(color_preset_b == 7) {
        CHSV tempHSV = rgb2hsv_approximate(leds[i]);
        if(firstFire || (tempHSV.val == 0))
          leds[i] = CHSV( random8(90)%60 , 255, random8(153, 255) );
        else {
          uint8_t randomH = random8(16);
          if(randomH > 14 && tempHSV.hue < 53)
            tempHSV.hue += 7;
          else if(randomH > 12 && tempHSV.hue < 55)
            tempHSV.hue += 5;
          else if(randomH > 9 && tempHSV.hue < 57)
            tempHSV.hue += 3;
          else if(randomH < 2 && tempHSV.hue > 7)
            tempHSV.hue -= 7;
          else if(randomH < 3 && tempHSV.hue > 5)
            tempHSV.hue -= 5;
          else if(randomH < 6 && tempHSV.hue > 3)
            tempHSV.hue -= 3;
          tempHSV.sat = 255;
          leds[i] = tempHSV;
        }
      }
    }
    colorIndex += 1;
  }
}

static void set_number(uint8_t number, uint8_t position, bool start_offset) {
  uint8_t offset = 0;
  switch(position){
    case 0:
      position = 103 - 14 * start_offset;
      offset = 2 * (!start_offset);
      break;
    case 1:
      position = 75 - 14 * start_offset;
      break;
    case 2:
      position = 33;
      break;
    case 3:
      position = 6;
      offset = 1;
      break;
  }

  switch(number){
    case 0:
      led_FG[position   ] = true;
      led_FG[position+1 ] = true;
      led_FG[position+4 ] = true;
      led_FG[position+5 ] = false;
      led_FG[position+6 ] = false;
      led_FG[position+7 ] = true;
      position -= (offset & 0x01);
      led_FG[position+11] = true;
      led_FG[position+12] = false;
      led_FG[position+13] = false;
      led_FG[position+14] = true;
      position -= ((offset & 0x02) >> 1) * 3;
      led_FG[position+21] = true;
      led_FG[position+22] = true;
      break;
    case 1:
      led_FG[position   ] = false;
      led_FG[position+1 ] = true;
      led_FG[position+4] = false;
      led_FG[position+5 ] = true;
      led_FG[position+6 ] = true;
      led_FG[position+7 ] = false;
      position -= (offset & 0x01);
      led_FG[position+11] = true;
      led_FG[position+12] = true;
      led_FG[position+13] = false;
      led_FG[position+14] = false;
      position -= ((offset & 0x02) >> 1) * 3;
      led_FG[position+21] = false;
      led_FG[position+22] = false;
      break;
    case 2:
      led_FG[position   ] = false;
      led_FG[position+1 ] = true;
      led_FG[position+4 ] = true;
      led_FG[position+5 ] = false;
      led_FG[position+6 ] = true;
      led_FG[position+7 ] = true;
      position -= (offset & 0x01);
      led_FG[position+11] = true;
      led_FG[position+12] = false;
      led_FG[position+13] = true;
      led_FG[position+14] = true;
      position -= ((offset & 0x02) >> 1) * 3;
      led_FG[position+21] = false;
      led_FG[position+22] = true;
      break;
    case 3:
      led_FG[position   ] = true;
      led_FG[position+1 ] = true;
      led_FG[position+4 ] = true;
      led_FG[position+5 ] = false;
      led_FG[position+6 ] = true;
      led_FG[position+7 ] = true;
      position -= (offset & 0x01);
      led_FG[position+11] = true;
      led_FG[position+12] = false;
      led_FG[position+13] = true;
      led_FG[position+14] = false;
      position -= ((offset & 0x02) >> 1) * 3;
      led_FG[position+21] = false;
      led_FG[position+22] = true;
      break;
    case 4:
      led_FG[position   ] = true;
      led_FG[position+1 ] = true;
      led_FG[position+4 ] = false;
      led_FG[position+5 ] = false;
      led_FG[position+6 ] = true;
      led_FG[position+7 ] = true;
      position -= (offset & 0x01);
      led_FG[position+11] = false;
      led_FG[position+12] = false;
      led_FG[position+13] = true;
      led_FG[position+14] = false;
      position -= ((offset & 0x02) >> 1) * 3;
      led_FG[position+21] = true;
      led_FG[position+22] = true;
      break;
    case 5:
      led_FG[position   ] = true;
      led_FG[position+1 ] = true;
      led_FG[position+4 ] = true;
      led_FG[position+5 ] = false;
      led_FG[position+6 ] = true;
      led_FG[position+7 ] = false;
      position -= (offset & 0x01);
      led_FG[position+11] = true;
      led_FG[position+12] = false;
      led_FG[position+13] = true;
      led_FG[position+14] = false;
      position -= ((offset & 0x02) >> 1) * 3;
      led_FG[position+21] = true;
      led_FG[position+22] = true;
      break;
    case 6:
      led_FG[position   ] = true;
      led_FG[position+1 ] = true;
      led_FG[position+4 ] = true;
      led_FG[position+5 ] = false;
      led_FG[position+6 ] = true;
      led_FG[position+7 ] = false;
      position -= (offset & 0x01);
      led_FG[position+11] = true;
      led_FG[position+12] = true;
      led_FG[position+13] = true;
      led_FG[position+14] = true;
      position -= ((offset & 0x02) >> 1) * 3;
      led_FG[position+21] = false;
      led_FG[position+22] = false;
      break;
    case 7:
      led_FG[position   ] = false;
      led_FG[position+1 ] = false;
      led_FG[position+4 ] = true;
      led_FG[position+5 ] = true;
      led_FG[position+6 ] = true;
      led_FG[position+7 ] = true;
      position -= (offset & 0x01);
      led_FG[position+11] = true;
      led_FG[position+12] = false;
      led_FG[position+13] = false;
      led_FG[position+14] = false;
      position -= ((offset & 0x02) >> 1) * 3;
      led_FG[position+21] = false;
      led_FG[position+22] = true;
      break;
    case 8:
      led_FG[position   ] = true;
      led_FG[position+1 ] = true;
      led_FG[position+4 ] = true;
      led_FG[position+5 ] = false;
      led_FG[position+6 ] = true;
      led_FG[position+7 ] = true;
      position -= (offset & 0x01);
      led_FG[position+11] = true;
      led_FG[position+12] = false;
      led_FG[position+13] = true;
      led_FG[position+14] = true;
      position -= ((offset & 0x02) >> 1) * 3;
      led_FG[position+21] = true;
      led_FG[position+22] = true;
      break;
    case 9:
      led_FG[position   ] = false;
      led_FG[position+1 ] = false;
      led_FG[position+4 ] = true;
      led_FG[position+5 ] = true;
      led_FG[position+6 ] = true;
      led_FG[position+7 ] = true;
      position -= (offset & 0x01);
      led_FG[position+11] = true;
      led_FG[position+12] = false;
      led_FG[position+13] = true;
      led_FG[position+14] = false;
      position -= ((offset & 0x02) >> 1) * 3;
      led_FG[position+21] = true;
      led_FG[position+22] = true;
      break;
  }
}

void enable_led(int8_t x, int8_t y) {
  uint8_t max_height = (x < 3) ? 3 + x : 6;
  uint8_t min_height = (x > 16) ? x - 16 : 0;
  if((y < min_height) || (y > max_height) || (x < 0) || (x > 19))
    return;
  if(x % 2)
    led_FG[startNumber[x] - (y - min_height)] = true;
  else
    led_FG[startNumber[x] + (y - min_height)] = true;
}

void show_water_state(int8_t water_state[3]) {
  if(water_state[TYPE_STATE] < 1) { //drop effect
    int8_t height = water_state[TYPE_Y] - water_state[TYPE_STATE];
    enable_led(water_state[TYPE_X], height);
  } else { //wave effect
    for(uint8_t height = 0; height < water_state[TYPE_STATE]; height++) {
      enable_led(water_state[TYPE_X] - water_state[TYPE_STATE], water_state[TYPE_Y] - height);
      enable_led(water_state[TYPE_X] + water_state[TYPE_STATE], water_state[TYPE_Y] + height);
      enable_led(water_state[TYPE_X] - water_state[TYPE_STATE] + height, water_state[TYPE_Y] + height);
      enable_led(water_state[TYPE_X] + water_state[TYPE_STATE] - height, water_state[TYPE_Y] - height);
    }
    for(uint8_t width = 0; width <= water_state[TYPE_STATE]; width++) {
      enable_led(water_state[TYPE_X] - width, water_state[TYPE_Y] - water_state[TYPE_STATE]);
      enable_led(water_state[TYPE_X] + width, water_state[TYPE_Y] + water_state[TYPE_STATE]);
    }
  }
}

void set_water_animation() {
  memset(led_FG, false, NUM_LEDS);

  if(firstWater) {
    for(uint8_t states = 0; states < MAX_NR_WATER_STATES; states++) {
      water_states[states][TYPE_Y] = 127;
    }
    firstWater = false;
  }

  for(uint8_t states = 0; states < nr_of_drops; states ++) {
    if(water_states[states][TYPE_Y] == 127) { //free to use
      randomSeed(analogRead(0));
      water_states[states][TYPE_X] = random(20);
      uint8_t max_height = (water_states[states][TYPE_X] < 3) ? 3 + water_states[states][TYPE_X] : 6;
      water_states[states][TYPE_Y] = random(max_height);
      water_states[states][TYPE_STATE] = - (max_height - water_states[states][TYPE_Y]);
    } else {
      if(water_states[states][TYPE_STATE] < ((water_states[states][TYPE_X] < (19 - water_states[states][TYPE_X])) ? 19 - water_states[states][TYPE_X] : water_states[states][TYPE_X])) {
        water_states[states][TYPE_STATE]++;
      } else {
        water_states[states][TYPE_Y] = 127;
        continue;
      }
    }
  
    show_water_state(water_states[states]);
  }
}

void set_fire() {
  randomSeed(analogRead(0));
  memset(led_FG, false, NUM_LEDS);
  for(uint8_t i = 0; i < sizeof(fire_height); i++) {
    uint8_t offset = (i > 16) ? i - 16 : 0;
    
    int8_t rand_ = random(11);

    /*the chance that it's going down decreases when it's already quite down*/
    if(rand_ < fire_height[i])
      fire_height[i] -= 1;
    else if(rand_ > fire_height[i] + 5)
      fire_height[i] += 1;
    
    
    if(fire_height[i] > offset) {
      for(uint8_t randInc = 1; randInc <= (fire_height[i] - offset); randInc++) {
        if(i % 2) 
          led_FG[startNumber[i] - (randInc - 1)] = true;
        else
          led_FG[startNumber[i] + (randInc - 1)] = true;
      }
    }
  }
}

#include <FastLED.h>
#include "DHT.h"

#define TIME_CHECK_PERIOD 2

#define DHTPIN 2 //Digital pin connected to the DHT sensor
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
#define LED_PIN     5
#define NUM_LEDS    128
#define LED_TYPE    WS2813
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];
bool led_FG[NUM_LEDS];

#define UPDATES_PER_SECOND 30
// This example shows several ways to set up and use 'palettes' of colors
// with FastLED.
//
// These compact palettes provide an easy way to re-colorize your
// animation on the fly, quickly, easily, and with low overhead.
//
// USING palettes is MUCH simpler in practice than in theory, so first just
// run this sketch, and watch the pretty lights as you then read through
// the code.  Although this sketch has eight (or more) different color schemes,
// the entire sketch compiles down to about 6.5K on AVR.
//
// FastLED provides a few pre-configured color palettes, and makes it
// extremely easy to make up your own color schemes with palettes.
//
// Some notes on the more abstract 'theory and practice' of
// FastLED compact palettes are at the bottom of this file.

// FastLED provides several 'preset' palettes: RainbowColors_p, RainbowStripeColors_p,
// OceanColors_p, CloudColors_p, LavaColors_p, ForestColors_p, and PartyColors_p.

// Blending: NOBLEND, LINEARBLEND

CRGBPalette16 currentPalette = PartyColors_p;
TBlendType    currentBlending = LINEARBLEND;

CRGBPalette16 timePalette = LavaColors_p;
TBlendType    timeBlending = LINEARBLEND;

static uint8_t startIndex = 0;
static uint8_t time_count = 0;
static uint8_t second_counter = 0;
//static uint8_t second_limit = 0;

char inputString[30] = "";         // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

uint8_t fire_height[20];

uint8_t preset_color_f = 0xFF;
uint8_t preset_color_b = 1;
CRGB foreground = CRGB::White;
CRGB background = CRGB::White;
uint8_t brightness = 255;

enum mode {
  mode_time,
  mode_temp,
  mode_fire
} current_mode;
mode previous_mode;

/* show current temperature */
float original_temp = 0;
int8_t temp = 0;

/* show current time */
uint8_t hours = 0;
uint8_t minutes = 0;
bool blinking_seconds = true;

bool updated = false;

bool firstFire;

void setup() {
  current_mode = mode_time;
  previous_mode = current_mode;
  memset(fire_height, 4, 20);
  Serial.begin(115200);

  dht.begin();

  firstFire = true;

  random16_set_seed(analogRead(0));
 
  delay( 10000 ); // power-up safety delay + delay for start-up Wi-Fi module
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(  brightness );

  second_counter = TIME_CHECK_PERIOD;
}


void loop()
{
  if(current_mode != previous_mode) {
    firstFire = true;
    memset(led_FG, false, NUM_LEDS);
    previous_mode = current_mode;
  }

  if(second_counter >= TIME_CHECK_PERIOD)
    update_serial();

  switch(current_mode) {
    case mode_time:
      blink_seconds(time_count < (UPDATES_PER_SECOND / 2));
  
      set_time(hours, minutes);
      break;
    case mode_temp:
      if(second_counter % 2 == 1){
        if(!updated) {
          update_temp();
          updated = true;
        }
      } else
        updated = false;
      break;
    case mode_fire:
      preset_color_f = 7;
      preset_color_b = 0;
      if(time_count % (UPDATES_PER_SECOND/10) == 0){
        if(!updated) {
          set_fire();
          updated = true;
        }
      } else
        updated = false;
      break;
  }
  
  show_all();

  delay(1000/UPDATES_PER_SECOND);
  time_count++;
  if(time_count == UPDATES_PER_SECOND) {
    time_count = 0;
    second_counter++;
  }
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

void update_serial() {
  while(Serial.available())
    Serial.read();
  Serial.println("T"); //send request for data
  char inChar = 0;
  uint32_t counter = 0;
  do{
    if(Serial.available()) {
      inChar = (char)Serial.read();
      inputString[strlen(inputString)] = inChar;
    }
    counter++;
    if(counter >= 1000000) { //if no answer, try again in ~3 seconds
      counter = 0;
      Serial.println(" ");
    }
  } while(inChar != '\n');
  
  char* end;
  uint32_t raw_foreground;
  uint32_t raw_background;
  if(inputString[0] == 'L') {
    blinking_seconds = (inputString[1] == 'B');
    if(inputString[2] == 'C')
      current_mode = mode_time;
    else if(inputString[2] == 'T')
      current_mode = mode_temp;
    else if(inputString[2] == 'f')
      current_mode = mode_fire;
    if(inputString[3] == 'f') {
      if(inputString[4] == 'c') {
        preset_color_f = 0xFF;
        raw_foreground = strtol(&inputString[5], &end, 10);
        foreground.blue = raw_foreground & 0xFF;
        foreground.green = (raw_foreground >> 8) & 0xFF;
        foreground.red = (raw_foreground >> 16) & 0xFF;
      } else {
        preset_color_f = strtol(&inputString[4], &end, 10);
      }
    }
    if(end[0] == 'b') {
      if(end[1] == 'c') {
        preset_color_b = 0xFF;
        raw_background = strtol(&end[2], &end, 10);
        background.blue = raw_background & 0xFF;
        background.green = (raw_background >> 8) & 0xFF;
        background.red = (raw_background >> 16) & 0xFF; 
      } else
        preset_color_b = strtol(&end[1], &end, 10);
    }
    if(end[0] == 'B') {
      brightness = strtol(&end[1], &end, 10);
      end++;
    }
  } else {
    end = &inputString[1];
  }
  hours = strtol(end, &end, 10);
  minutes = strtol(end, &end, 10);
  //second_limit = 60 - strtol(end, &end, 10);
  second_counter = 0;  
  
  Serial.flush();
  memset(inputString, '\0', strlen(inputString));
}

void blink_seconds(bool show) {
  led_FG[61] = show || !blinking_seconds;
  led_FG[68] = show || !blinking_seconds;
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

void FillLEDsFromPaletteColors( uint8_t colorIndex) {
  for( int i = 0; i < NUM_LEDS; i++) {
    if(led_FG[i]) {
      if(preset_color_f == 0xFF) {
        leds[i].blue = foreground.blue * brightness / 255;
        leds[i].red = foreground.red * brightness / 255;
        leds[i].green = foreground.green * brightness / 255;
      } else if(preset_color_f == 0)
        leds[i] = CRGB::Black;
      else if(preset_color_f == 1)
        leds[i] = ColorFromPalette( RainbowColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_f == 2)
        leds[i] = ColorFromPalette( OceanColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_f == 3)
        leds[i] = ColorFromPalette( CloudColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_f == 4)
        leds[i] = ColorFromPalette( LavaColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_f == 5)
        leds[i] = ColorFromPalette( ForestColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_f == 6)
        leds[i] = ColorFromPalette( PartyColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_f == 7) {
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
      if(preset_color_b == 0xFF) {
        leds[i].blue = background.blue *  brightness / 255;
        leds[i].red = background.red * brightness / 255;
        leds[i].green = background.green * brightness / 255;
      } else if(preset_color_b == 0)
        leds[i] = CRGB::Black;
      else if(preset_color_b == 1)
        leds[i] = ColorFromPalette( RainbowColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_b == 2)
        leds[i] = ColorFromPalette( OceanColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_b == 3)
        leds[i] = ColorFromPalette( CloudColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_b == 4)
        leds[i] = ColorFromPalette( LavaColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_b == 5)
        leds[i] = ColorFromPalette( ForestColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_b == 6)
        leds[i] = ColorFromPalette( PartyColors_p, colorIndex, brightness, currentBlending);
      else if(preset_color_b == 7)
        leds[i] = CHSV( random8(60) , 255, random8(153, 255) );
    }
    colorIndex += 1;
  }
}

void set_number(uint8_t number, uint8_t position, bool start_offset) {
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

void set_fire() {
  randomSeed(analogRead(0));
  memset(led_FG, false, NUM_LEDS);
  uint8_t startNumber[] = {0, 8, 9, 21, 22, 35, 36, 49, 50, 63, 64, 77, 78, 91, 92, 105, 106, 118, 119, 127};
  for(uint8_t i = 0; i < 20; i++) {
    int8_t rand_ = random(8);
    uint8_t offset = 0;
    if(rand_ < (fire_height[i]-2)) {
      if(rand_ < (fire_height[i]-4))
        fire_height[i] -= 2;
      else
        fire_height[i] -= 1;
    } else if(rand_ >= (fire_height[i]+2)) {
      if(rand_ >= (fire_height[i] + 4))
        fire_height[i] += 2;
      else
        fire_height[i] += 1;
    }
    if(i > 16){
      offset = i - 16;
    }
    if(fire_height[i] > offset) {
      for(uint8_t randInc = 1; randInc < (fire_height[i] - offset); randInc++) {
        if(i % 2) 
          led_FG[startNumber[i] - (randInc - 1)] = true;
        else
          led_FG[startNumber[i] + (randInc - 1)] = true;
          
      }
    }
  }
  
  /*for(uint8_t i = 0; i < 20; i++) {
    uint8_t randNumber = random(30);
    uint8_t change_decr = 1;
    int8_t stop_number;
    int8_t start_number;
    if(i == 0 || i == 19) 
      stop_number = 20;
    else if(i == 1)
      stop_number = 15;
    else if(i == 2)
      stop_number = 9;
    else if( i == 19)
      start_number = 20;
    else if( i == 18)
      start_number = 24;
    else if( i == 17)
      start_number = 27;
    else {
      stop_number = 2;
      start_number = 29;
    }
    for(int8_t randInc = start_number; randInc >= stop_number; randInc-= change_decr){
      if(randNumber < randInc){
        if(i % 2) {
          led_FG[startNumber[i] - (change_decr - 1)] = true;
        } else {
          led_FG[startNumber[i] + (change_decr - 1)] = true;
        }
      } else {
        break;
      }
      change_decr++;
    }
  }*/
}

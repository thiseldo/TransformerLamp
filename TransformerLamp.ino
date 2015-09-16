/*
  Arduino Yún Bridge Transformer Lamp
  Uses either NeoPixel strip or WS2801 RGB LED strip.

   http://lamp.local/arduino/TransformerLamp/rgb/off – Turn off the lamp
   http://lamp.local/arduino/TransformerLamp/rgb/ff0000 – Set lamp colour to red
   http://lamp.local/arduino/TransformerLamp/rgb/random – Set random colour mode
   http://lamp.local/arduino/TransformerLamp/rgb/rainbow – set rainbow mode 

  Created by Andrew D. Lindsay, September 2015.
  Details at http://blog.thiseldo.co.uk/?p=1436

  License http://creativecommons.org/licenses/by-sa/3.0/
 
 */

#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>
#include <EEPROM.h>

// Depending on which 
#ifdef NEOPIXEL
#include <Adafruit_NeoPixel.h>
#else
#include "Adafruit_WS2801.h"
#include "SPI.h"
#endif

// A few defines to keep track of what mode the RGB is in
#define RGB_STEADY 0
#define RGB_RANDOM 1
#define RGB_RAINBOW 2

// Keep track of RGB Mode
int rgbMode = RGB_STEADY;
int currentRed = 0;
int currentGreen = 0;
int currentBlue = 0;

int rgbOuterCounter = 0;

// Listen to the default port 5555, the Yún webserver
// will forward there all the HTTP requests you send
YunServer server;


// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      14

#ifdef NEOPIXEL
// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
#define PIN            6

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#else
// Choose which 2 pins you will use for output.
// Can be any valid output pins.
// The colors of the wires may be totally different so
// BE SURE TO CHECK YOUR PIXELS TO SEE WHICH WIRES TO USE!
uint8_t dataPin  = 3;
uint8_t clockPin = 2;

// Don't forget to connect the ground wire to Arduino ground,
// and the +5V wire to a +5V supply

// Set the first variable to the NUMBER of pixels. 25 = 25 pixels in a row
Adafruit_WS2801 strip = Adafruit_WS2801(NUMPIXELS, dataPin, clockPin);
#endif

// Setup EEPROM storage, use magic number to determine if
// the data has already been written.
#define MAGIC_NUMBER 0xAD2305AD
#define EEPROM_START 0x00

// Structure for the EEPROM data
typedef struct eepromData {
  long magic;
  int rgbMode;
  uint32_t colour;
  int red;
  int green;
  int blue;
};

eepromData settings;

/** setup - perform all the setup for the lamp
 */
void setup() {
  // Bridge startup
  strip.begin(); // This initializes the NeoPixel library.
  colorWipe(Color(255, 0, 0), 1000); // Red
  colorWipe(Color(0, 0, 0), 1000); // off
  colorWipe(Color(255, 0, 0), 1000); // Red

  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  Bridge.begin();
  digitalWrite(13, HIGH);
  colorWipe(Color(0, 0, 255), 100); // Blue

  // Listen for incoming connection only from localhost
  // (no one from the external network could connect)
  server.listenOnLocalhost();
  server.begin();
  colorWipe(Color(0, 255, 0), 100); // Green

  // Read EEPROM to determine last colour and mode.
  readSettings();

  if ( rgbMode == RGB_STEADY ) {
    colorWipe(Color(currentRed, currentGreen, currentBlue), 50);
  }
}


/** loop - Main code loop, where the magic happens!
 *  Listen for client connections and process the commands
 */
void loop() {
  // Get clients coming from server
  YunClient client = server.accept();

  // There is a new client?
  if (client) {
    // Process request
    process(client);

    // Close connection and free resources.
    client.stop();
  }

  /* Random and Rainbow modes need the sequence to be moved on
     continuously, this is done by calling the selected function
     to perform a single colour change each time. RGB_STEADY is not
     included as there is no change needed here
   */
  switch ( rgbMode ) {
    case RGB_RANDOM:    // Random mode
      rgbRandom();
      break;
    case RGB_RAINBOW:   // Rainbow mode
      rgbRainbow();
      break;
  }

  delay(50); // Poll every 50ms
}

/** readSettings - Read block of data from EEPROM and map to the
 *  settings structure. A magic number is used to determine if the
 *  settings have previously been saved.
 */
void readSettings() {
  eeprom_read_block((void*)&settings, (void*)0, sizeof(settings));
  if ( settings.magic != MAGIC_NUMBER ) {
    // Not saved previously so create new set
    settings.magic = MAGIC_NUMBER;
    // Steady and off
    settings.rgbMode = RGB_STEADY;
    settings.red = 0;
    settings.green = 0;
    settings.blue = 0;
  }
  // Set current colour to last saved colour
  currentRed = settings.red;
  currentGreen = settings.green;
  currentBlue = settings.blue;
  rgbMode = settings.rgbMode;
}

/** writeSettings - Write block of data to EEPROM if anything has changed.
 */
void writeSettings() {
  if ( currentRed != settings.red || currentGreen != settings.green ||
       currentBlue != settings.blue || rgbMode != settings.rgbMode ) {
    settings.magic = MAGIC_NUMBER;
    settings.rgbMode = rgbMode;
    settings.red = currentRed;
    settings.green = currentGreen;
    settings.blue = currentBlue;
    eeprom_write_block((const void*)&settings, (void*)0, sizeof(settings));
  }
}

/** process - Process the received command. Original example sketch
 *  commands have been removed and replaced with rgb command.
 *  It is possible to restore the original digital and analog i/o commands.
 *  @param client - YunClient object
 */
void process(YunClient client) {
  // read the command
  String command = client.readStringUntil('/');

  // is "rgb" command?
  if (command == "rgb") {
    rgbCommand(client);
  } else
    client.print(F("{\"error\":\"No command\"}"));
}

/** rgbCommand - 
 *  @param client - YunClient object
 */
void rgbCommand(YunClient client) {
  String rgb = client.readStringUntil('\r');

  // Check if commands sent instead of colours
  if ( rgb == "off" ) {
    rgbMode = RGB_STEADY;
    currentRed = 0;
    currentGreen = 0;
    currentBlue = 0;
    colorWipe(Color(currentRed, currentGreen, currentBlue), 0);
    writeSettings();
  } else if ( rgb == "random" ) {
    // Create a random colour
    rgbMode = RGB_RANDOM;
    currentRed = 0;
    currentGreen = 0;
    currentBlue = 0;
    colorWipe(Color(currentRed, currentGreen, currentBlue), 0);
    writeSettings();
  } else if ( rgb == "rainbow" ) {
    // Create a rainbow colour
    rgbMode = RGB_RAINBOW;
    rgbOuterCounter = 0;
    currentRed = 0;
    currentGreen = 0;
    currentBlue = 0;
    colorWipe(Color(currentRed, currentGreen, currentBlue), 0);
    writeSettings();
  } else {
    // Steady, single colour
    char rgbBuf[8];
    rgbMode = RGB_STEADY;

    rgb.toCharArray( rgbBuf, 8);
    currentRed = getColour( &rgbBuf[0] );
    currentGreen = getColour( &rgbBuf[2] );
    currentBlue = getColour( &rgbBuf[4] );
    colorWipe(Color(currentRed, currentGreen, currentBlue), 50);
    writeSettings();
  }
  // Send feedback to client
  client.print(F("{\"rgb\":\""));
  client.print(rgb);
  client.print(F("\"}"));

  // Update datastore key with the current rgb value
  Bridge.put("RGB", rgb);
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

/** Colour - Create a 24 bit color value from R,G,B components
 * @param r - Red component
 * @param g - Green component
 * @param b - Blue component
 * @return 24 (32) bit encoded RGB value
 */
uint32_t Color(byte r, byte g, byte b)
{
  uint32_t c;
  c = r;
  c <<= 8;
  c |= g;
  c <<= 8;
  c |= b;
  return c;
}

/** getColour - Get the integer colour value from a 2 hex digit characters
 * @param val - The character pointer to hex values
 * @return integer representation, 0-255 of the hex value
 */
int getColour( char *val ) {
  int col = 0;
  for ( int i = 0; i < 2; i++ ) {
    col *= 16;
    if ( *val >= 'a' && *val <= 'f')
      col += (*val - 'a' + 10);
    else if ( *val >= 'A' && *val <= 'F' )
      col += (*val - 'A' + 10);
    else
      col += (*val - '0');

    val++;
  }
  return col;
}

/** Wheel - Input a value 0 to 255 to get a color value.
 * The colours are a transition r - g -b - back to r
 * @param WheelPos - Input value in range of 0-255
 * @return 24 (32) bit encoded RGB value
 */
uint32_t Wheel(byte WheelPos)
{
  if (WheelPos < 85) {
    return Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
  else if (WheelPos < 170) {
    WheelPos -= 85;
    return Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  else {
    WheelPos -= 170;
    return Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

/** rgbRandom - Random mode, set new random colour for a single random pixel
 */
void rgbRandom() {
  strip.setPixelColor(random(0, strip.numPixels()), Wheel( random(255)));
  strip.show();
}

/** rgbRainbow - Produce a rainbow effect along the strip. Based on 
 *  Adafruit NeoPixel example sketch functions.
 *  This function only performs 1 cycle each time it is called. A counter
 *  is used to track the outer loop value.
 */
void rgbRainbow() {
  for (int i = 0; i < strip.numPixels(); i++) {
    // tricky math! we use each pixel as a fraction of the full 96-color wheel
    // (thats the i / strip.numPixels() part)
    // Then add in j which makes the colors go around per pixel
    // the % 96 is to make the wheel cycle around
    strip.setPixelColor(i, Wheel( ((i * 256 / strip.numPixels()) + rgbOuterCounter) % 256) );
  }
  rgbOuterCounter++;
  if ( rgbOuterCounter >= 256 ) rgbOuterCounter = 0;

  strip.show();
}

// That's All!

/*
 * Project trellis
 * Description:  control an Adafruit NeoTrellis array of 16x16 LEDs and keys with MQTT
 * Author: Kirk Carlson
 * Date: 8 Apr 2019
 */

/*
expand on the basic MultiTrellis.

/add copyright block from similar code.
/add optional Particle.io communications
test optional Particle.io communications
add local only mode for Conway's life.
test local only mode for Conway's life.
/fix problem with MQTT disconnecting when it shouldn't

/add mode to send individual switch down and up
/add mode to send individual switch down
/add mode to use switches to control local bit array
/add module for switches
/add module for rotary encoders
add module for lattice

add command to get local bit array
/add command to set local bit array
add command to control individual light (color)
add command to control light array (color, bitmap)
add command to control lights array (array of colors )
add command to control brightness ( brightness)
/add command to generate next generation of life
add command to request ping
/add command to request key on and off
/add command to request key on only
/add command to request key use for local matrix

add message to request local bit array
add message to send individual switch down
add message to send individual switch up
add message to send brightness
/add heartbeat message (ping response)

add message for external switch down (2,3,4,5,6,7)
add message for external switch up
*/


// **** LIBRARIES ****
#include "Adafruit_NeoTrellis.h"
#define PARTICLE_NO_ARDUINO_COMPATIBILITY 1
#include <MQTT.h>
#include "addresses.h"
#include "button.h"
#include "encoder.h"
#include "pixeltypes.h"
//                      from FastLED for definition of CRGB and others


//**** HARDWARE DEFINES ****
#define BUTTON_A	A0
#define BUTTON_B	A1
#define BUTTON_C	A2
#define BUTTON_D	A0
#define BUTTON_E	A1
#define BUTTON_F	A2
#define LED_A		A4
#define LED_B		A5
#define LED_C		A6
#define LED_D		A4
#define LED_E		A5
#define LED_F		A6

// SDA			D0
// SCL			D1
#define ROTARY1		D2 // select for encoder1
#define ROTARY2		D3 // select for encoder2)
#define ENCODER1A	D4
#define ENCODER1B	D5
#define ENCODER1SW	D6
#define ENCODER2A	D4
#define ENCODER2B	D5
#define ENCODER2SW	D6
//#define WIFI_MANUAL	D6
#define BUTTON_MANUAL	D7



//**** DEFINES ****

#define X_DIM 16
//                  number of columns of keys
#define Y_DIM 16
//                  number of rows of keys


#define KEEP_ALIVE 120
//                  number of seconds without MQTT message
#define HEARTBEAT KEEP_ALIVE/4
//                  number of seconds without MQTT message
#define BROADCAST 5*60
//                  number of seconds between broadcast update
#define TIME unsigned long
//                  TIME datatype, cause I forget


// **** DEFINE MATRIX OF KEYS AND LEDS ****


//create a matrix of trellis panels
Adafruit_NeoTrellis t_array[ Y_DIM/4][ X_DIM/4] = {
  { // row 0
    Adafruit_NeoTrellis(0x2E),
    Adafruit_NeoTrellis(0x2F),
    Adafruit_NeoTrellis(0x30),
    Adafruit_NeoTrellis(0x31)
  },
  { // row 1
    Adafruit_NeoTrellis(0x32),
    Adafruit_NeoTrellis(0x33),
    Adafruit_NeoTrellis(0x34),
    Adafruit_NeoTrellis(0x35)
  },
  { // row 2
    Adafruit_NeoTrellis(0x36),
    Adafruit_NeoTrellis(0x37),
    Adafruit_NeoTrellis(0x38),
    Adafruit_NeoTrellis(0x39)
  },
  { // row 3
    Adafruit_NeoTrellis(0x3A),
    Adafruit_NeoTrellis(0x3B),
    Adafruit_NeoTrellis(0x3C),
    Adafruit_NeoTrellis(0x3D)
  }
};

//pass this matrix to the multitrellis object
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM/4, X_DIM/4);


//define the logger
SerialLogHandler logHandler(LOG_LEVEL_TRACE);

// **** CONSTANTS ****
#define ON 1
#define OFF 0
#define MAX_TOPIC_LENGTH 30


// **** DATA DEFINITIONS ****

enum ledModes {
    offMode,
    demo1,
    demo2,
    NUM_LED_MODES
};


String ledModeNames []= {
    "offMode",
    "demo1",
    "demo2",
};


enum keyModes {
    keyModeOnOff,
    keyModeOnOnly,
    keyModeLocalMatrix,
    NUM_KEY_MODES
};


String keyModeNames []= {
    "onOff",
    "onOnly",
    "localMatrix",
    "invalid"
};


const int RED = (255<<16) + (  0<<8) +   0;
const int YELLOW = (255<<16) + (255<<8) +   0;
const int WHITE = (255<<16) + (255<<8) + 255;
const int BLUE = (  0<<16) + (  0<<8) + 255;
const int GREEN = (  0<<16) + (128<<8) +   0;
const int ORANGE = (255<<16) + (165<<8) +   0;
const int VIOLET = (238<<16) + (130<<8) + 238;
const int BLACK = (  0<<16) + (  0<<8) +   0;


const int colors [] = {
  RED,
  YELLOW,
  WHITE,
  BLUE,
  GREEN,
  ORANGE,
  VIOLET,
  BLACK
};



// **** GLOBALS ****

ledModes ledMode = offMode; // LED display mode, see above
keyModes keyMode = keyModeOnOff; // key mode, see above
int ledDirection = 0; // 0..359 LED direction in degrees for selected modes
//CRGB ledColor = CRGB::White; // default color of individual LEDs
uint32_t ledColor = (255<<16) | (255<<8) | 255; // default color of individual LEDs
int ledBrightness; // level of over all brightness desired 0..255
int ledKeypressBrightness; // level of the brightness for key presses 0..255
int selectedGroup = 0; // currently selected group
char loopTopic[ MAX_TOPIC_LENGTH + 1];
bool matrixB [ X_DIM * Y_DIM];
uint16_t matrixI [ Y_DIM];
//CRGB ledColors [X_DIM * Y_DIM];  //commanded colors are stored here
uint32_t ledColors [X_DIM * Y_DIM];  //commanded colors are stored here
                                 // the LED is given commanded color modified by ledBrightness

MQTT mqttClient(mqttServer, 1883, KEEP_ALIVE, receiveMqttMessage);
//bool mqttClient.publish( String topic, String payload);


// **** PROTOTYPES ****
keyModes stringToKeyMode( String str);



// **** OBJECTS ****

Encoder encoder1 ( ENCODER1A, ENCODER1B, ENCODER1SW, "encoder1");
Encoder encoder2 ( ENCODER2A, ENCODER2B, ENCODER2SW, "encoder2");

Button buttonA (BUTTON_A, LED_A, "buttonA");
Button buttonB (BUTTON_B, LED_B, "buttonB");
Button buttonC (BUTTON_C, LED_C, "buttonC");
Button buttonD (BUTTON_D, LED_D, "buttonD");


// **** MQTT FUNCTIONS ****

#define MAX_PAYLOAD_SIZE 100
// for QoS2 MQTTPUBREL message.
// this messageid maybe have store list or array structure.
uint16_t qos2messageid = 0;
char lastPayload [MAX_PAYLOAD_SIZE+1];
unsigned long lastPayloadTime; // epoch of last payload

// **** GENERAL FUNCTIONS ****

// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    if(WheelPos < 85) {
        return seesaw_NeoPixel::Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    } else if(WheelPos < 170) {
        WheelPos -= 85;
        return seesaw_NeoPixel::Color(255 - WheelPos * 3, 0, WheelPos * 3);
    } else {
        WheelPos -= 170;
        return seesaw_NeoPixel::Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    return 0;
}


void setBrightness( uint8_t bright) {
    for (int y=0; y < Y_DIM; y++) {
        for (int x=0; x < X_DIM; x++) {
            //t_array[x][y]-> ... non pointer type
            //seesaw_NeoPixel::setBrightness( bright); ... cant do without object
            //t_array[x][y].seesaw_NeoPixel::setBrightness( bright); ...seesaw not a base of t_
            //t_array[x][y].seesaw_NeoPixel::setBrightness( bright); .. some error
            //t_array[x][y].pixels.setBrightness(NEO_TRELLIS_XY(x, y), ledBrightness); .. 2 params
            //t_array[x][y].pixels.setBrightness( ledBrightness); .. compiles but no go
        }
    }
}


void showLeds () {
    // show all LEDs using commanded color modified by ledBrightness
    //CRGB temp;
    uint32_t temp;

    for (int i=0; i < X_DIM * Y_DIM; i++) {
        temp = ledColors[ i];
/*
        if (ledBrightness) {
            temp.r = (temp.r * ledBrightness) >> 8;
            temp.g = (temp.g * ledBrightness) >> 8;
            temp.b = (temp.b * ledBrightness) >> 8;
        }
*/
        if (ledBrightness) {
            temp = ( ((((((temp >> 16) & 0xFF) * ledBrightness) >> 8) & 0xFF) << 16) |
                     ((((((temp >>  8) & 0xFF) * ledBrightness) >> 8) & 0xFF) <<  8) |
                      (((((temp      ) & 0xFF) * ledBrightness) >> 8) & 0xFF) );
        }

        trellis.setPixelColor(i, temp);
    }
    trellis.show();
}


void loadMatrix ( ) {
/*
    CRGB temp = ledColor;
    if (ledBrightness) {
        temp.r = (temp.r * ledBrightness) >> 8;
        temp.g = (temp.g * ledBrightness) >> 8;
        temp.b = (temp.b * ledBrightness) >> 8;
    }
*/
    uint32_t temp = ledColor;
    if (ledBrightness) {
         temp = ( ((((((temp >> 16) & 0xFF) * ledBrightness) >> 8) & 0xFF) << 16) |
                  ((((((temp >>  8) & 0xFF) * ledBrightness) >> 8) & 0xFF) <<  8) |
                   (((((temp      ) & 0xFF) * ledBrightness) >> 8) & 0xFF) );
    }

    for (int i=0; i < X_DIM * Y_DIM; i++) {
        if (matrixB[i]) {
            ledColors[i] = ledColor;
            trellis.setPixelColor(i, temp);
        } else {
            ledColors[i] = 0;
            trellis.setPixelColor(i, 0);
        }
        //Log.trace( " " + String (i) + " " + String( matrixB[i] ? 1 : 0));
        //delay(20);
    }
        //delay(20);
    //Log.trace( "showing loadMatrix");
    trellis.show();
}


void arrBoolToArrInt ( bool arrB[], uint16_t arrI[]) {
    // these are numbered to reflect the array, so 0 is highest bit of first member
    // and 255 is lowest bit of last member
    for (int i=0; i < Y_DIM; i++) {
        int row = 0;
        for (int j=0; j < X_DIM; j++) {
            //Log.trace ("aB2aI j" + String(j) + String (" ") + String( arrB[ i * X_DIM + j]));
            if (arrB[i * X_DIM + j]) {
                row = (row << 1) | 1;
            } else {
                row = (row << 1);
            }
        }
        //Log.trace ("aB2aI row i " + String(i) + String (" ") + String(row, HEX));
        arrI[i] = row;
    }
}


void arrBoolToLed ( bool arrB[]) {
    // these are numbered to reflect the array, so 0 is highest bit of first member
    // and 255 is lowest bit of last member
    int ledNumber = 0;
    for (int i = 0; i < Y_DIM; i++) {
        for (int j = 0; j < X_DIM; j++) {
            if (arrB[ ledNumber]) {
                ledColors[ ledNumber] = ledColor;
            } else {
                ledColors[ ledNumber] = 0;
            }
            ledNumber++;
        }
    }
}


void arrIntToArrBool ( uint16_t arrI[], bool arrB[]) {
    // these are numbered to reflect the array, so 0 is highest bit of first member
    // and 255 is lowest bit of last member
    int ledNumber = 0;
    for (int i = 0; i < Y_DIM; i++) {
        for (int j = 0; j < X_DIM; j++) {
            if (arrI[i] & 1 << (X_DIM-j-1)) { // 15, 14, 13..1, 0
               arrB[ ledNumber] = true;
            } else {
               arrB[ ledNumber] = false;
            }
            ledNumber++;
        }
    }
}


void allLedsOff ( bool arrB[]) {
    // these are numbered to reflect the array, so 0 is highest bit of first member
    // and 255 is lowest bit of last member
    int ledNumber = 0;
    for (int i = 0; i < Y_DIM; i++) {
        for (int j = 0; j < X_DIM; j++) {
            ledColors[ ledNumber] = 0;
            ledNumber++;
        }
    }
}


void allLedsOn ( bool arrB[]) {
    // these are numbered to reflect the array, so 0 is highest bit of first member
    // and 255 is lowest bit of last member
    int ledNumber = 0;
    for (int i = 0; i < Y_DIM; i++) {
        for (int j = 0; j < X_DIM; j++) {
            ledColors[ ledNumber] = ledColor;
            ledNumber++;
        }
    }
}


String arrIntToJsonString( uint16_t arr[] ) {
    String output = "[";
    String prefix = "";
    for (int i=0; i < Y_DIM; i++) {
        output = output + prefix + String (arr[i]);
        prefix = ", ";
    }
    output = output + ']';
    return output;
}

bool jsonToIntArray( String json, uint16_t arr[] ) { // a limiting length would be good
/*
  the array is assumed to be 16 16-bit unsigned integers
  this is not quite right... it is true for the key and led binary arrays
  but false for led color arrays ... a 1x256 array of 24-bit color values.
  the context will know how many members are expected and the maximum value allowed.
*/
    int pos = 0;
    int arrPos = 0;
    int arrPosLimit = 16;
    uint32_t number = 0;
    uint32_t numberLimit = 0xFFFF;
    char jchar = json.charAt(pos);
    while ( jchar == ' ') { // skip whitespace
        pos++;
        jchar = json.charAt(pos);
    }
    //Log.trace( "jsonToInt char: " + String( jchar));
    if ( jchar == '[') { // good start
        do { // while not end of list of numbers
            pos++;
            jchar = json.charAt(pos);
            //Log.trace( "jsonToInt char: " + String( jchar));
            while ( jchar == ' ') { // skip whitespace
                pos++;
                jchar = json.charAt(pos);
            };
            if ( jchar >= '0' && jchar <= '9') { // start of number
                //Log.trace( "jsonToInt char: " + String( jchar));
                number = jchar - '0';
                //Log.trace( "jsonToInt number: " + String( number));
                pos++;
                jchar = json.charAt(pos);
                while ( jchar >= '0' && jchar <= '9') { // still in number
                    //Log.trace( "jsonToInt char: " + String( jchar));
                    number = 10 * number + jchar - '0';
                    //Log.trace( "jsonToInt number: " + String( number));
                    pos++;
                    jchar = json.charAt(pos);
                    if (number >  numberLimit) {
                        return false; // number too big
                    }
                };
                // end of number, character is not a digit
                //Log.trace( "jsonToInt EoN char: " + String( jchar));
                arr[arrPos] = number;
                arrPos++;
                if (arrPos >= arrPosLimit) {
                    return true; // too many numbers in array
                }
                while ( jchar == ' ') { // skip whitespace
                    pos++;
                    jchar = json.charAt(pos);
                }
                if ( jchar == ']') {
                    return true; // properly formatted integer array
                }; // other chars fall through including ','
            } else {
                return false; // poorly formatted
            };
        } while ( jchar == ',');
        if ( jchar == ']' ) {
            return true;
        } else {
            return false; // poorly formatted
        };
    } else {
        return false; // poorly formatted
    }
}


String  arrToString ( uint16_t arr[] ) { // a limiting length would be good
    int arrPos = 0;
    int arrPosLimit = 16;
    String arrString = "";
    String prefix = "";

    for (int i = 0; i < arrPosLimit; i++) {
        //Log.trace( "arrToString number: " + String( arr[arrPos]));
        arrString = arrString + prefix + String( arr[ arrPos], HEX);
        prefix = ", ";
        arrPos++;
    }
    return ( arrString);
}


void updateLedMatrix ( ) {
    // turn LEDs on or off according to the bit map
    // use the global ledColor
    arrBoolToLed ( matrixB);
    showLeds();
}


int neighborCount (bool grid[], int cell ) {
    // grid is a 1xN grid
    // cell is the number of the cell of interest
    // This algorithm treats a boundary as dead

    int r = cell / X_DIM; // row of interest. 0 is top,, Y_DIM-1 is bottom
    int c = cell % X_DIM; // column of interest. 0 is left most, X_DIM-1 is rightmost

// base
int above = (r - 1) * X_DIM;
int same = r * X_DIM;
int below = (r + 1) * X_DIM;
int left = c - 1;
int center = c;
int right = c + 1;
bool notLeftCol = (c > 0);
bool notRightCol = (c < X_DIM-1);
bool notTopRow = (r > 0);
bool notBottomRow = (r < Y_DIM-1);

    int count = 0;

    if ( notTopRow) {
        if ( notLeftCol && grid[above + left]) {
            count = count + 1;
            //console.log("NW " + String(r) + "," + String(c));
        }
        if ( grid[above + center]) {
            count = count + 1;
            //console.log("N " + String(r) + "," + String(c));
        }
        if ( notRightCol && grid[above + right]) {
            count = count + 1;
            //console.log("NE " + String(r) + "," + String(c));
        }
    }

    //same row
    if ( notLeftCol && grid[same + left]) {
        count = count + 1;
        //console.log("W " + String(r) + "," + String(c));
    }
    if ( notRightCol && grid[same + right]) {
        count = count + 1;
        //console.log("E " + String(r) + "," + String(c));
    }

    if (notBottomRow) {
        if ( notLeftCol && grid[below + left]) {
            count = count + 1;
            //console.log("SW " + String(r) + "," + String(c));
        }
        if ( grid[below + center]) {
            count = count + 1;
            //console.log("S " + String(r) + "," + String(c));
        }
        if ( notRightCol && grid[below + right]) {
            count = count + 1;
            //console.log("SE " + String(r) + "," + String(c));
        }
    }
    return count;
}


void generation (bool currentGrid[]) {
  bool nextGrid [Y_DIM * X_DIM];

  for (int r=0; r < Y_DIM; r++) {
    for (int c=0; c < X_DIM; c++) {
      int cell = r * X_DIM + c;
      int count = neighborCount( currentGrid, cell);
      //Log.trace ("row:" + String(r) + " col:" + String(c) + " count:" + String(count));
      if (currentGrid[ cell]) { //alive
        if (count == 2 || count == 3) {
           nextGrid[ cell] = true;
        } else {
           nextGrid[ cell] = false;
        }
      } else { // vacant
        if ( count == 3) {
           nextGrid[ cell] = true;
        } else {
           nextGrid[ cell] = false;
        }
      }
    }
  }
  for (int r=0; r < Y_DIM; r++) {
    for (int c=0; c < X_DIM; c++) {
      int cell = r * X_DIM + c;
      currentGrid [ cell] = nextGrid[ cell];
    }
  }
} 


void enableEncoder ( uint8_t pin) {
    digitalWrite( pin, LOW);
}


void disableEncoder ( uint8_t pin) {
    digitalWrite( pin, HIGH);
}


void setKeyMode( keyModes mode) {
    if (mode < NUM_KEY_MODES) {
        keyMode = mode;
    }
    if (keyMode == keyModeLocalMatrix) {
        Log.trace( "Setting keyMode to keyModeLocalMatrix");
        arrIntToArrBool( matrixI, matrixB);
        loadMatrix();
    } else {
        allLedsOff( matrixB);
        showLeds();
    }
}


keyModes stringToKeyMode( String cmdString) {
    for (int i=0; i < NUM_KEY_MODES; i++) {
        if (cmdString.compareTo( keyModeNames[i]) == 0 ) { 
            return (keyModes) i;
        };
    };
    Log.error("stringToKeyMode: " + cmdString);
    return (keyModes) NUM_KEY_MODES;
};


void handleKeyDown ( int keyNumber) {
    Log.trace("Key " + String( keyNumber) + String( " down"));
    switch (keyMode) {
    case keyModeOnOff:
        trellis.setPixelColor(keyNumber, ledColor);
        trellis.show();
        //ledColors[ keyNumber] = ledColor;
        publish ( "key/" + String( keyNumber),  String("1"));
        break;

    case keyModeOnOnly:
        //ledColors[ keyNumber] = ledColor;
        trellis.setPixelColor(keyNumber, ledColor);
        trellis.show();
        publish ( String( "key/") + String( keyNumber), String("1"));
        break;

    case keyModeLocalMatrix:
        trellis.setPixelColor(keyNumber, ledColor);
        trellis.show();
        matrixB [ keyNumber] = !matrixB[ keyNumber]; // toggle the bit in matrix
        if (matrixB [ keyNumber]) {
            ledColors[ keyNumber] = ledColor; // LED on
        } else {
            ledColors[ keyNumber] = 0; // LED off
        }
        break;

    default:
        break;
    }
};

void handleKeyUp ( int keyNumber) {
    switch (keyMode) {
    case keyModeOnOff:
        trellis.setPixelColor(keyNumber, 0);
        trellis.show();
        Log.trace("Key " + String( keyNumber) + String( " up"));
        //ledColors[ keyNumber] = 0;
        publish ( String( "/key/") + String( keyNumber),  String("0"));
        break;

    case keyModeOnOnly:
        trellis.setPixelColor(keyNumber, 0);
        trellis.show();
        //ledColors[ keyNumber] = 0;
        break;

    case keyModeLocalMatrix:
        if (matrixB [ keyNumber]) {
            trellis.setPixelColor(keyNumber, ledColor);
            trellis.show();
        } else {
            trellis.setPixelColor(keyNumber, 0);
            trellis.show();
        }
        break;

    default:
        break;
    }
};

//define a callback for key presses
TrellisCallback keypressEventCallback(keyEvent evt){

    if(evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        handleKeyDown( evt.bit.NUM);
    } else if(evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
        handleKeyUp( evt.bit.NUM);
    }

    showLeds();
    return 0;
}


// receive MQTT message
void receiveMqttMessage(char* topic, byte* payload, unsigned int length) {
    int temp;
    bool overridingCommand = false;

    if (length > MAX_PAYLOAD_SIZE) {
        length = MAX_PAYLOAD_SIZE;
    }
    memcpy(lastPayload, payload, length);
    lastPayload[length] = '\0';
    lastPayloadTime = Time.now();
    //String topicS = String( (const char*) topic);
    //String payloadS = String( (const char*) lastPayload);
    String topicS = String( (char*) topic);
    String payloadS = String( (char*) lastPayload);

    Log.trace( "Received message topic:" + topicS + " payload:" + payloadS);

    int index = topicS.indexOf( String( NODE_NAME) + String("/command/"));
    if (index < 0) {
        Log.error( "Bad message received, topic: " + topicS + " payload: " + payloadS);
    } else {
        index = topicS.indexOf( "/command/") + 9;
        String command = topicS.substring(index);
        if (command.compareTo( "keyMode") == 0) {
            setKeyMode( stringToKeyMode( payloadS));
        } else if (command.compareTo( "getMatrix") == 0) {
            //String bmatrix = arrIntToJsonString( matrixI );
            //Log.trace( "publishing boolmatrix: " +  bmatrix);
            arrBoolToArrInt( matrixB, matrixI );
            String jmatrix = arrIntToJsonString( matrixI );
            //Log.trace( "publishing matrix: " +  jmatrix);
            publish ("matrix", jmatrix);
        } else if (command.compareTo( "setMatrix") == 0) {
            //Log.trace( "extracting matrix: " +  payloadS);
            jsonToIntArray( payloadS, matrixI);
            arrIntToArrBool( matrixI, matrixB);
            if (keyMode == keyModeLocalMatrix) {
                loadMatrix();
            }
            //Log.trace( "extracted matrix: " + arrToString( matrixI));
        } else if (command.compareTo( "setBrightness") == 0) {
            temp = payloadS.toInt();
            if (temp <= 255) {
                ledBrightness = temp;
                setBrightness( ledBrightness);
                //Log.trace( "brightness set to " + String( ledBrightness));
                //change brightness immediately
                updateLedMatrix();
            } else {
                Log.warn( "bad brightness command: " + payloadS);
            }
        } else if (command.compareTo( "setColor") == 0) {
            uint32_t tempColor = payloadS.toInt();
            //Log.trace( "color payload is #" + String( tempColor, HEX));
            if (tempColor <= 0xffffff) {
                //Log.trace( "color payload is #" + String( tempColor, HEX));
                //Log.trace( "color payload is #" + String( (CRGB) tempColor, HEX));
                ledColor = tempColor;
                //Log.trace( "ledColor set to " + String( ledColor));
                //Log.trace( "ledColor set to #" + String( ledColor, HEX));
                // change color immediately
                updateLedMatrix();
            } else {
                Log.warn( "bad color command: " + payloadS);
            }
        } else if (command.compareTo( "allOn") == 0) {
            allLedsOn( matrixB);
            showLeds();
            overridingCommand = true;
            Log.trace( "all on");
        } else if (command.compareTo( "allOff") == 0) {
            allLedsOff( matrixB);
            showLeds();
            overridingCommand = true;
            Log.trace( "all off");
        } else if (command.compareTo( "ping") == 0) {
            Log.trace( "ping");
            sendHeartbeat();
        } else if (command.compareTo( "lifeGen") == 0) {
            // maybe should only do this in the matrix mode...
            Log.trace( "lifeGen");
            generation( matrixB); // updates bool matrix
            loadMatrix();
        } else if (command.compareTo( "LED/") == 0) {
            // need to look for trailing LED number and separately payload = color
            Log.trace( "LED");
            uint16_t ledNumber = payloadS.toInt();
            if (ledNumber <  Y_DIM * X_DIM){
                ledColors[ ledNumber] = ledColor;
                trellis.setPixelColor( ledNumber, (uint32_t) ledColor);
                Log.trace( "LED " + String( ledNumber) + "  color set to #" + String( (int)ledColor, HEX));
            } else {
                Log.warn( "bad LED command: " + payloadS);
            }
        } else {
            Log.warn( "bad command: " + command);
        }

        if (!overridingCommand) {
            if (keyMode == keyModeLocalMatrix) {
                updateLedMatrix();
            } else {
                allLedsOff( matrixB);
            }
            showLeds();
        }
    }

/*
    if (topicS.compareTo( String( NODE_NAME) + String ("/command/mode")) == 0) {
        // payload expected to be 0..n (mode index)
        passedMode = payloadS.toInt();
        if (passedMode >= 0 && passedMode < NUM_LED_MODES) {
            Log.trace( "Set LED mode " + String( passedMode, DEC) + " " + ledModeNames [ passedMode]);
            setLedMode( selectedGroup, passedMode);
        } else {
            Log.warn( "Mode payload was bad: " + payloadS);
        }
    } else if (topicS.compareTo( String( NODE_NAME) + String ("/command/direction")) == 0) {
        // payload expected to be 0..359 (degrees)
        passedDirection = payloadS.toInt();
        if (passedDirection > 0 && passedDirection <= 360) {
            ledDirection = passedDirection;
            *Log.trace( "Direction set to: " + String( passedDirection, DEC) + String("%"));
        } else {
            Log.warn( "Direction payload was bad: " + payloadS);
        }
    }
*/
}


void publish (String topic, String payload) {
// this prepends the node name to the topic
#define BUFFER_LEN 100
//                    buffer need to hold array of 16 3-digit numbers
//                    [000, 000, 000 ... 000]
    char topicBuffer[BUFFER_LEN];
    char payloadBuffer[BUFFER_LEN];

    if (!mqttClient.isConnected()) {
       Log.trace( "MQTT disconnected to publish " + topic + String( ": ") + payload);
    }
    topic = String( NODE_NAME) + String( "/") + topic;
    topic.toCharArray(topicBuffer, BUFFER_LEN);
    payload.toCharArray(payloadBuffer, BUFFER_LEN);
    mqttClient.publish( topicBuffer, payloadBuffer);
    Log.trace(String( "MQTT: ") + String( topicBuffer) + String( ": ") + String( payloadBuffer));
}


void sendHeartbeat() {
    publish ( "heartbeat",  "");
}


void setLedMode( int group, int mode)
/*
 * this function changes the current mode of the LED display and sets up
 * the effect(s) to be applied over time.
 * This is normally called initially and by a web page via a particle.function call


it sure looks like most of this function can be replaced with a table. index by mode or effect number and provide a pointer to an array. Question is how to extract the length of the array. Maybe that is just another column in the table...

a better simplification is to add a known marker to end of a command string
when you hit it, you either stop, LED_STOP, GROUP_STOP, or loop, LED_LOOP, GROUP_LOOP.
 */
{
    //Serial.println(ledModeNames[mode]);
    //Serial.println(ledModeNames[mode].length());
    char modeName [ledModeNames[mode].length() + 1]; //include terminating null
    ledModeNames[ mode].toCharArray(modeName, ledModeNames[mode].length()+1);\
    Log.trace("attempting LED mode change " + String( mode) + " " + String( modeName));

    //stopAllLedFSMs();
    //stopAllLedGroupFSMs(); // this isn't right, want to control each group independently
    //ledGroupStates[group].timeToWait = 0; //cancel timer
    /*
       the long term stopping mechanism should have a scope of control.
       you get a scope of control to control either the group as a whole or individuals within
       that scope.

       not sure how the scope is defined and allocated, but maybe it is with a group mechanism.
     */
    //TIME now = millis();

    ledMode = (ledModes) mode; // assume this works
    switch(  mode)
    {
        case offMode:
            //Log.trace("LED mode changed to offMode");
            //interpretGroupCommand ( &ledGroupStates[ group], offGC, now);
            break;

        case demo1:
            Log.trace("LED mode changed to demo1");
            //interpretGroupCommand ( &ledGroupStates[ 0], demo1GC, now);
            break;

        case demo2:
            //Log.trace("LED mode changed to demo2");
            //interpretGroupCommand ( &ledGroupStates[ 0], demo2GC, now);
            break;

        default:
            ledMode = offMode; // not quite right, should save entry value
            Log.error("bad attempt to change LED mode");
            break;
    }
}

// **** Main system routines

TIME lastBroadcast;
TIME lastHeartbeat;
SYSTEM_MODE(MANUAL);

void setup() {
    pinMode (BUTTON_MANUAL, INPUT_PULLUP);
    //pinMode (WIFI_MANUAL, INPUT_PULLUP);

    pinMode( ROTARY1, OUTPUT);
    pinMode( ROTARY2, OUTPUT);
    disableEncoder( ROTARY1);
    disableEncoder( ROTARY2);

    //CRGB tempColor;
    uint32_t tempColor;

    setBrightness( ledBrightness);

    Serial.begin(9600);
    //while(!Serial);

    if(!trellis.begin()){
        Log.error("failed to begin trellis");
        while(1);
    }

    /* the array can be addressed as x,y or with the key number */
    for(int i = 0; i < Y_DIM * X_DIM; i++){
        tempColor = Wheel(map(i, 0, X_DIM * Y_DIM, 0, 255));
        ledColors[ i] = tempColor;
        trellis.setPixelColor(i, tempColor);
        trellis.show();
        delay(1);
    }

    for(int y=0; y < Y_DIM; y++){
        for(int x=0; x < X_DIM; x++){
            //activate rising and falling edges on all keys
            trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_RISING, true);
            trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_FALLING, true);
            trellis.registerCallback(x, y, keypressEventCallback);
            ledColors[ y * X_DIM + x] = 0;
            trellis.setPixelColor( y * X_DIM + x, 0);
            trellis.show();
            delay(1);
        }
    }

    lastBroadcast = millis();
}


void loop() {
//    if (digitalRead (WIFI_MANUAL) == HIGH) {
        if (!WiFi.ready()) {
            WiFi.on();
            //WiFi.useDynamicIP();
            WiFi.connect();
            sendHeartbeat();
            lastHeartbeat = millis();
        }
        if (digitalRead (BUTTON_MANUAL) == HIGH) {
            Particle.connect();
        }
        if (Particle.connected()) {
            Particle.process();
        }
//    }

    TIME now = millis();

    // check encoders and buttons
    enableEncoder( ROTARY1);
    encoder1.check( now);
    disableEncoder( ROTARY1);

    enableEncoder( ROTARY2);
    encoder2.check( now);
    disableEncoder( ROTARY2);

    buttonA.check( now);
    buttonB.check( now);
    buttonC.check( now);
    buttonD.check( now);

    trellis.read(); // scan the keyboard
    // do LED effects if any

//    if (digitalRead (WIFI_MANUAL) == HIGH) {
        if (mqttClient.isConnected()) {
            mqttClient.loop(); // look for messages received, etc.

            if (now - lastHeartbeat > HEARTBEAT * 1000) { // heartbeat due
                sendHeartbeat();
                lastHeartbeat = now;
            }
            if (now - lastBroadcast > BROADCAST * 1000) { // broadcast due
                Log.trace(String( NODE_NAME) + String( "broadcast now"));
                // or when something changes
                publish ( String( "mode"),        String( ledMode, DEC));
                publish ( String( "brightness"),  String( ledBrightness, DEC));
                publish ( String( "direction"),   String( ledDirection, DEC));
                //publish ( String( "color/r"),     String( ledColor.r, DEC));
                //publish ( String( "color/g"),     String( ledColor.g, DEC));
                //publish ( String( "color/b"),     String( ledColor.b, DEC));
                lastBroadcast = now;
            }
        } else {
            // connect to the server
            Log.trace("Attempting to connect to MQTT broker again");
            mqttClient.connect(NODE_NAME);
            delay(1000);

            // subscribe to all commands at once with wild card
            String topic = String( NODE_NAME) + String( "/command/#");
            #define TOPIC_LEN 30
            char subscribeTopic[TOPIC_LEN];

            topic.toCharArray(subscribeTopic, TOPIC_LEN);
            mqttClient.subscribe( subscribeTopic);
        }
//    }
    delay(20);
}

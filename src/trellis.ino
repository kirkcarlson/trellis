/*
 * Project trellis
 * Description:  control an Adafruit NeoTrellis array of 16x16 LEDs and keys with MQTT
 * Author: Kirk Carlson
 * Date: 8 Apr 2019
 */

/*
expand on the basic MultiTrellis.

/add copyright block from similar code.
add optional Particle.io communications
add local only mode for Conway's life.

/add mode to send individual switch down and up
/add mode to send individual switch down
add mode to use switches to control local bit array

add command to request local bit array
add command to control individual light (color)
add command to control light array (color, bitmap)
add command to control lights array (array of colors )
add command to control brightness ( brightness)
add command to request ping
/add command to request key on and off
/add command to request key on only
/add command to request key use for local matrix

add message to send local bit array
add message to send individual switch down
add message to send individual switch up
add message to send brightness
add heartbeat message (ping response)

add message for external switch down (2,3,4,5,6,7)
add message for external switch up

*/
/* This example shows basic usage of the
MultiTrellis object controlling an array of
NeoTrellis boards

As is this example shows use of two NeoTrellis boards
connected together with the leftmost board having the
default I2C address of 0x2E, and the rightmost board
having the address of 0x2F (the A0 jumper is soldered)
*/

// **** LIBRARIES ****
#include "Adafruit_NeoTrellis.h"
#define PARTICLE_NO_ARDUINO_COMPATIBILITY 1
#include <MQTT.h>
#include "addresses.h"
// from FastLED for definition of CRGB
#include "pixeltypes.h"


//**** HARDWARE DEFINES ****
// SDA			D0
// SCL			D1
#define BUTTON_A	D2
#define BUTTON_B	D3
#define BUTTON_C	D4
#define BUTTON_D	D5
#define WIFI_MANUAL	D6
#define BUTTON_MANUAL	D7


//**** DEFINES ****

// KEEP_ALIVE is number of seconds without MQTT message
#define KEEP_ALIVE 60
#define TIME unsigned long


// **** DEFINE MATRIX OF KEYS AND LEDS ****

#define Y_DIM 16 //number of rows of key
#define X_DIM 16 //number of columns of keys

//create a matrix of trellis panels
Adafruit_NeoTrellis t_array[Y_DIM/4][X_DIM/4] = {

  {
    Adafruit_NeoTrellis(0x2E),
    Adafruit_NeoTrellis(0x2F),
    Adafruit_NeoTrellis(0x30),
    Adafruit_NeoTrellis(0x31)
  },
  {
    Adafruit_NeoTrellis(0x32),
    Adafruit_NeoTrellis(0x33),
    Adafruit_NeoTrellis(0x34),
    Adafruit_NeoTrellis(0x35)
  },
  {
    Adafruit_NeoTrellis(0x36),
    Adafruit_NeoTrellis(0x37),
    Adafruit_NeoTrellis(0x38),
    Adafruit_NeoTrellis(0x39)
  },
  {
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
CRGB ledColor = CRGB::White; // default color of individual LEDs
int ledBrightness; // level of over all brightness desired 0..100 percent
int selectedGroup = 0; // currently selected group
char loopTopic[ MAX_TOPIC_LENGTH + 1];

MQTT mqttClient(mqttServer, 1883, KEEP_ALIVE, receiveMqttMessage);
//bool mqttClient.publish( String topic, String payload);


// **** PROTOTYPES ****
keyModes stringToKeyMode( String str);


//  **** CLASSES ****

class Button {
    public:
        int8_t state = OFF;
        int8_t pin = BUTTON_A;
        String name = "A";

        Button ( int8_t buttonPin, String buttonName) {
          pin = buttonPin;
          name = buttonName;
          pinMode (pin, INPUT_PULLUP);
        }

        void checkButton() {
            String topic;

            if (digitalRead( pin) == LOW) {
                if (state == OFF) {
                    delay(20); // wait to debounce button
                    if (digitalRead (pin) == LOW) {
                        topic = String( NODE_NAME) + String( "/") + String( name);
                        Log.trace( topic + String( " pressed"));
                        topic.toCharArray(loopTopic, MAX_TOPIC_LENGTH);
                        mqttClient.publish( loopTopic, "pressed");
                        state = ON;
                    }
                }
            } else {
                if (state == ON) {
                    delay(20); // wait to debounce button
                    if (digitalRead (pin) == HIGH) {
                        topic = String( NODE_NAME) + String( "/") + String( name);
                        Log.trace( topic + String( " released"));
                        topic.toCharArray(loopTopic, MAX_TOPIC_LENGTH);
                        mqttClient.publish( loopTopic, "released");
                        state = OFF;
                    }
                }
            }
        }
};


// **** Local Objects ****

Button buttonA (BUTTON_A, "buttonA");
Button buttonB (BUTTON_B, "buttonB");
Button buttonC (BUTTON_C, "buttonC");
Button buttonD (BUTTON_D, "buttonD");


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


void setKeyMode( keyModes mode) {
    // probably has case specific things to do, but not for now.
    // should need to turn off all LEDs almost always.
    if (mode < NUM_KEY_MODES) {
        keyMode = mode;
    }
}


keyModes stringToKeyMode( String cmdString) {
    for (int i=0; i < NUM_KEY_MODES; i++) {
        if (cmdString.compareTo( keyModeNames[i]) == 0 ) { 
            return (keyModes) i;
        };
    };
    Log.error("stringToKeyMode: " + cmdstring);
    return (keyModes) NUM_KEY_MODES;
};


void handleKeyDown ( int keyNumber) {
    Log.trace("Key " + String( keyNumber) + String( "down"));
    switch (keyMode) {
    case keyModeOnOff:
        trellis.setPixelColor( keyNumber, Wheel(map( keyNumber, 0, X_DIM * Y_DIM, 0, 255))); //on rising
        publish ( String( NODE_NAME) + String( "/key/") + String( keyNumber),  String( "On"));
        break;

    case keyModeOnOnly:
        trellis.setPixelColor( keyNumber, Wheel(map( keyNumber, 0, X_DIM * Y_DIM, 0, 255))); //on rising
        publish ( String( NODE_NAME) + String( "/key/") + String( keyNumber),  String( "On"));
        break;

    case keyModeLocalMatrix:
        trellis.setPixelColor( keyNumber, Wheel(map( keyNumber, 0, X_DIM * Y_DIM, 0, 255))); //on rising
        break;

    default:
        break;
    }
};

void handleKeyUp ( int keyNumber) {
    switch (keyMode) {
    case keyModeOnOff:
        Log.trace("Key " + String( keyNumber) + String( "up"));
        trellis.setPixelColor( keyNumber, 0); //off falling
        publish ( String( NODE_NAME) + String( "/key/") + String( keyNumber),  String( "Off"));
        break;

    case keyModeOnOnly:
        trellis.setPixelColor( keyNumber, 0); //off falling
        break;

    case keyModeLocalMatrix:
        trellis.setPixelColor( keyNumber, 0); //off falling
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

    trellis.show();
    return 0;
}


// receive MQTT message
void receiveMqttMessage(char* topic, byte* payload, unsigned int length) {
/*
    int passedMode;
    int passedBrightness;
    int passedDirection;
    int passedColor;
*/
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
    } else if (topicS.compareTo( String( NODE_NAME) + String ("/command/brightness")) == 0) {
        // payload expected to be 0..100 (percent)
        passedBrightness = payloadS.toInt();
        if (passedBrightness > 0 && passedBrightness <= 100) {
            //FastLED.setBrightness( (int) (passedBrightness*255/100));
            Log.trace( "Brightness set to: " + String( passedBrightness, DEC) + String("%"));
        } else {
            Log.warn( "Brightness payload was bad: " + payloadS);
        }
    } else if (topicS.compareTo( String( NODE_NAME) + String ("/command/direction")) == 0) {
        // payload expected to be 0..359 (degrees)
        passedDirection = payloadS.toInt();
        if (passedDirection > 0 && passedDirection <= 360) {
            ledDirection = passedDirection;
            Log.trace( "Direction set to: " + String( passedDirection, DEC) + String("%"));
        } else {
            Log.warn( "Direction payload was bad: " + payloadS);
        }
    } else if (topicS.compareTo( String( NODE_NAME) + String ("/command/color")) == 0) {
        // payload expected to be some sort of HTML color in the form:
        // index into a box of crayons
        // 0xFFFFFF, 0xFFF, names, hue angles, lists, etc., can come later
        // want a multi color, but that may be the same thing as a list
        passedColor = payloadS.toInt();
        if (passedColor > 0 && passedColor <= (int)(sizeof( colors)/sizeof(int))) {
            ledColor = passedColor;
            Log.trace( "Set color " + String( passedColor, DEC) + " " + colors [ passedColor]);
        } else {
            Log.warn( "Color payload was bad: " + payloadS);
        }
    }
*/
}


void publish (String topic, String payload) {
#define BUFFER_LEN 30
    char topicBuffer[BUFFER_LEN];
    char payloadBuffer[BUFFER_LEN];
    topic.toCharArray(topicBuffer, BUFFER_LEN);
    payload.toCharArray(payloadBuffer, BUFFER_LEN);
    mqttClient.publish( topicBuffer, payloadBuffer);
    Log.trace(String( "MQTT: ") + String( topicBuffer) + String( ": ") + String( payloadBuffer));
}


void sendHeartbeat() {
    Log.trace( "Heart beat");
    publish ( String( NODE_NAME) + String( "/heartbeat"),  String( ""));
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
    TIME now = millis();

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

TIME lastPublish;
SYSTEM_MODE(MANUAL);

void setup() {
    pinMode (BUTTON_MANUAL, INPUT_PULLUP);
    pinMode (WIFI_MANUAL, INPUT_PULLUP);
    pinMode (BUTTON_A, INPUT_PULLUP);
    pinMode (BUTTON_B, INPUT_PULLUP);
    pinMode (BUTTON_C, INPUT_PULLUP);
    pinMode (BUTTON_D, INPUT_PULLUP);

    Serial.begin(9600);
    //while(!Serial);

    if(!trellis.begin()){
        Log.error("failed to begin trellis");
        while(1);
    }

    /* the array can be addressed as x,y or with the key number */
    for(int i = 0; i < Y_DIM * X_DIM; i++){
        trellis.setPixelColor(i, Wheel(map(i, 0, X_DIM * Y_DIM, 0, 255)));
        //addressed with keynum
        trellis.show();
        delay(1);
    }

    for(int y=0; y < Y_DIM; y++){
        for(int x=0; x < X_DIM; x++){
            //activate rising and falling edges on all keys
            trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_RISING, true);
            trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_FALLING, true);
            trellis.registerCallback(x, y, keypressEventCallback);
            trellis.setPixelColor(x, y, 0x000000); //addressed with x,y
            trellis.show(); //show all LEDs
            delay(1);
        }
    }

    lastPublish = millis();
}


void loop() {
    if (digitalRead (WIFI_MANUAL) == HIGH) {
        if (!WiFi.ready()) {
            WiFi.on();
            //WiFi.useDynamicIP();
            WiFi.connect();
            sendHeartbeat();
        }
        if (digitalRead (BUTTON_MANUAL) == HIGH) {
            Particle.connect();
        }
        if (Particle.connected()) {
            Particle.process();
        }
    }

    TIME now = millis();

    // check buttons
    buttonA.checkButton();
    buttonB.checkButton();
    buttonC.checkButton();
    buttonD.checkButton();

    trellis.read(); // scan the keyboard
    // do LED effects if any

    if (digitalRead (WIFI_MANUAL) == HIGH) {
        if (mqttClient.isConnected()) {
            mqttClient.loop();
            if (now - lastPublish > 30 * 1000) { // only want to do this every thirty seconds
                Log.trace(String( NODE_NAME) + String( "publishing now"));
                // or when something changes
                publish ( String( NODE_NAME) + String( "/mode"),        String( ledMode, DEC));
                publish ( String( NODE_NAME) + String( "/brightness"),  String( ledBrightness, DEC));
                publish ( String( NODE_NAME) + String( "/direction"),   String( ledDirection, DEC));
                publish ( String( NODE_NAME) + String( "/color/r"),     String( ledColor.r, DEC));
                publish ( String( NODE_NAME) + String( "/color/g"),     String( ledColor.g, DEC));
                publish ( String( NODE_NAME) + String( "/color/b"),     String( ledColor.b, DEC));
                lastPublish = now;
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
    }
    delay(20);
}

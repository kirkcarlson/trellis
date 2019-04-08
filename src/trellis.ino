/*
expand on the basic MultiTrellis.

add copyright block from similar code.
add optional Particle.io communications
add local only mode for Conway's life.

add mode to send individual switch down and up
add mode to send individual switch down
add mode to use switches to control local bit array

add command to request local bit array
add command to control individual light (color)
add command to control light array (color, bitmap)
add command to control lights array (array of colors )
add command to control brightness ( brightness)
add command to request ping

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

#include "Adafruit_NeoTrellis.h"

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

/*
If you were using a 2x2 array of NeoTrellis boards, the above lines would be:

#define Y_DIM 8 //number of rows of key
#define X_DIM 8 //number of columns of keys

//create a matrix of trellis panels
Adafruit_NeoTrellis t_array[Y_DIM/4][X_DIM/4] = {
  
  { Adafruit_NeoTrellis(0x2E), Adafruit_NeoTrellis(0x2F) },

  { Adafruit_NeoTrellis(LOWER_LEFT_I2C_ADDR), Adafruit_NeoTrellis(LOWER_RIGHT_I2C_ADDR) }
  
};
*/

//pass this matrix to the multitrellis object
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM/4, X_DIM/4);

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

//define a callback for key presses
TrellisCallback blink(keyEvent evt){
  Serial.print("keypress");

  if(evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING)
    trellis.setPixelColor(evt.bit.NUM, Wheel(map(evt.bit.NUM, 0, X_DIM*Y_DIM, 0, 255))); //on rising
  else if(evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING)
    trellis.setPixelColor(evt.bit.NUM, 0); //off falling

  trellis.show();
  return 0;
}

void setup() {
  Serial.begin(9600);
  //while(!Serial);

  if(!trellis.begin()){
    Serial.println("failed to begin trellis");
    while(1);
  }

  /* the array can be addressed as x,y or with the key number */
  for(int i=0; i<Y_DIM*X_DIM; i++){
      Serial.print("k");
      trellis.setPixelColor(i, Wheel(map(i, 0, X_DIM*Y_DIM, 0, 255))); //addressed with keynum
      trellis.show();
      delay(5);
  }

  for(int y=0; y<Y_DIM; y++){
    for(int x=0; x<X_DIM; x++){
      Serial.print("a");
      //activate rising and falling edges on all keys
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_RISING, true);
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_FALLING, true);
      trellis.registerCallback(x, y, blink);
      trellis.setPixelColor(x, y, 0x000000); //addressed with x,y
      trellis.show(); //show all LEDs
      delay(5);
    }
  }
}

void loop() {
  Serial.print(".");
  trellis.read();
  delay(20);

}

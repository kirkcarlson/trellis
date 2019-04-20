/**************************************************************************
 * encoder -- class for KY-040 rotary encoder
 *
 * The KY-040 has 5 pins:
 *   CLK encoder B
 *   DT  encoder A
 *   SW  switch input
 *   +   connect to +3.3
 *   GND connect to ground
 *
 * board has pull ups, so cannot use the pullups in the processor.
 * Rotary encoder has detents.  Both encoder pins at the detent are 0.
 * Values change between detents using a Gray code.
 *
 * EncoderB-EncoderA sequences:
 *   00-01-11-10-00 clockwise (CW) rotation
 *   00-10-11-01-00 counter clockwise (CCW) rotation
 **************************************************************************/


// **** INCLUDES ****

#include "button.h"


// **** PROTOTYPE ****

void publish (String topic, String payload); //MQTT helper function


// **** CLASSSES ****

class Encoder {
    public:
        uint16_t state = 0; // encoder readings in bit pairs at various times
                            // rightmost pair is most recent
        uint8_t pinA = D2;
        uint8_t pinB = D3;
        uint8_t pinSwitch = D4;
        uint8_t flags = 0;
        unsigned long lastTime = 0;
        String topic = "A";
        Button button = Button( pinSwitch, pinSwitch, "dummy");

        Encoder ( uint8_t encoderPinA, uint8_t encoderPinB, uint8_t switchPin, String encoderName) {
            pinA = encoderPinA;
            pinB = encoderPinB;
            pinSwitch = switchPin;
            topic = encoderName;
            pinMode( pinA, INPUT); // do not use pullups with KY-040
            pinMode( pinB, INPUT);
            button.pin = pinSwitch; // almost should change the pin mode at this point also
            button.pinLed = pinSwitch;
            button.topic =  encoderName + String("Button");
        }

        void check( unsigned long epoch) { // check for encoder input
            // read the encoder state
            uint8_t enc_t0 = 0; // encoder reading at time 0
            if (digitalRead(pinA) == LOW) {
              enc_t0 |= (1 << 0);
            }
            if (digitalRead(pinB) == LOW) {
              enc_t0 |= (1 << 1);
            }

            /*  this code block is retained to aid in understand the code following it

old variables included only for understanding code
uint8_t enc_t0 = 0; // encoder reading at time 0
uint8_t enc_t1 = 0; // encoder reading at time -1
uint8_t enc_t2 = 0; // encoder reading at time -2
uint8_t enc_t3 = 0; // encoder reading at time -3
uint8_t enc_t4 = 0; // encoder reading at time -4

            // if there is any rotation from last measurement
            if (enc_t1 != enc_t0) {
              // if four or five measurements show CW rotation
              if ( (enc_t4 == 0b00 && enc_t3 == 0b01 && enc_t2 == 0b11 && enc_t1 == 0b10 && enc_t0 == 0b00) ||
                   (enc_t3 == 0b00 && enc_t2 == 0b01 && enc_t1 == 0b11 &&                   enc_t0 == 0b00) ||
                   (enc_t3 == 0b00 && enc_t2 == 0b01 &&                   enc_t1 == 0b10 && enc_t0 == 0b00) ||
                   (enc_t3 == 0b00 &&                   enc_t2 == 0b11 && enc_t1 == 0b10 && enc_t0 == 0b00) ) {
                 Serial.print("+");
             }
              // if four or five measurements show CCW rotation
              if ( (enc_t4 == 0b00 && enc_t3 == 0b10 && enc_t2 == 0b11 && enc_t1 == 0b01 && enc_t0 == 0b00) ||
                   (enc_t3 == 0b00 && enc_t2 == 0b10 && enc_t1 == 0b11 &&                   enc_t0 == 0b00) ||
                   (enc_t3 == 0b00 && enc_t2 == 0b10 &&                   enc_t1 == 0b01 && enc_t0 == 0b00) ||
                   (enc_t3 == 0b00 &&                   enc_t2 == 0b11 && enc_t1 == 0b01 && enc_t0 == 0b00) ) {
                 Serial.print("-");
              }

              // age measurements for the next round
              enc_t4 = enc_t3;
              enc_t3 = enc_t2;
              enc_t2 = enc_t1;
              enc_t1 = enc_t0;

            }
          End of historic code block */

            // if there is any rotation from last measurement
            if ((state & 0b11) != enc_t0 ) {
                state = (state << 2) | enc_t0;  //age the measurements and add new
                // if four or five measurements show CW rotation
                if ( ((state & 0b1111111111) == 0b0001111000) ||
                     ((state & 0b0011111111) ==   0b00011100) ||
                     ((state & 0b0011111111) ==   0b00011000) ||
                     ((state & 0b0011111111) ==   0b00111000) ) {
                    if (button.stillOn()) {
                        publish (topic, "R");
                    } else {
                        publish (topic, "r");
                    }

                }

                // if four or five measurements show CCW rotation
                if ( ((state & 0b1111111111) == 0b0010110100) ||
                     ((state & 0b0011111111) ==   0b00101100) ||
                     ((state & 0b0011111111) ==   0b00100100) ||
                     ((state & 0b0011111111) ==   0b00110100) ) {
                    if (button.stillOn()) {
                        publish (topic, "L");
                    } else {
                        publish (topic, "l");
                    }
                }

            }
            button.check ( epoch);
        } // end of check
}; // end class Encoder

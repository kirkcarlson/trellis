/*
 * Project trellis
 * Description:  control an Adafruit NeoTrellis array of 16x16 LEDs and switches with MQTT
 * Author: Kirk Carlson
 * Date: 8 Apr 2019
 */

// This file contains the addresses used by trellis

#define NODE_NAME "trellis1"

/**
 * if want to use IP address:
 *   byte server[] = { XXX,XXX,XXX,XXX };
 *   MQTT client(server, 1882, callback);
 * want to use domain name:
 *   MQTT client("iot.eclipse.org", 1882, callback);
 **/
byte mqttServer[] = { 192,168,4,1 };

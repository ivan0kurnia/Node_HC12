#include <SoftwareSerial.h>

#include <Node_HC12.h>

const unsigned int SOFT_RX = 8;
const unsigned int SOFT_TX = 9;
const unsigned int SET_PIN = 7;

SoftwareSerial HC12_Serial(SOFT_RX, SOFT_TX);

Node_HC12 HC12(&HC12_Serial, SET_PIN);

void setup()
{
    Serial.begin(115200);
    HC12.begin(2400, 1); // Baudrate and channel
}

void loop()
{
}
#include <Arduino.h>
#include <SoftwareSerial.h>

#include "Node_HC12.h"

// Notice: this library might fail to detect responses in high baudrates due to the nature of SoftwareSerial
const uint32_t Node_HC12::BAUDRATES[]{1200UL, 2400UL, 4800UL, 9600UL, 19200UL, 38400UL, 57600UL, 115200UL};

uint32_t Node_HC12::responseTimeout = 40UL;

Node_HC12::Node_HC12(SoftwareSerial *const serial, const uint8_t setPin) : serial{serial}, SET_PIN{setPin}
{
}

const bool Node_HC12::begin(const uint32_t br, const uint8_t ch)
{
    if (!isBaudrateAllowed(br))
        return false;

    pinMode(SET_PIN, OUTPUT);
    setToATCommandMode();

    baudrate = checkDeviceBaudrate();
    if (!baudrate)
        return false;

    serial->begin(baudrate);

    if (!changeBaudrate(br))
        return false;

    channel = checkDeviceChannel();
    if (!changeChannel(ch))
        return false;

    setToTransmissionMode();
    return true;
}

void Node_HC12::setToATCommandMode()
{
    if (mode == AT_COMMAND_MODE)
    {
        setToTransmissionMode();
        setToATCommandMode();
    }
    else
    {
        // Serial.println(F("[M] Changing mode to AT command mode"));

        mode = AT_COMMAND_MODE;
        digitalWrite(SET_PIN, AT_COMMAND_MODE);
        delay(40);
    }
}

void Node_HC12::setToTransmissionMode()
{
    if (mode == TRANSMISSION_MODE)
    {
        setToATCommandMode();
        setToTransmissionMode();
    }
    else
    {
        // Serial.println(F("[M] Changing mode to transmission mode"));

        mode = TRANSMISSION_MODE;
        digitalWrite(SET_PIN, TRANSMISSION_MODE);
        delay(80);
    }
}

void Node_HC12::setResponseTimeout(const uint32_t timeout)
{
    responseTimeout = timeout;
}

const String Node_HC12::getResponse(const uint32_t timeout) const
{
    String response = "";

    const unsigned long previousMillis = millis();
    while (millis() - previousMillis < timeout)
    {
        if (serial->available())
        {
            do
            {
                response += static_cast<char>(serial->read());
                delay(1);
            } while (serial->available());
            response.trim();

            // Serial.print(F("[R] "));
            // Serial.print(response);
            // Serial.println();

            break;
        }
    }

    return response;
}

const bool Node_HC12::testAT() const
{
    if (mode == AT_COMMAND_MODE)
    {
        serial->print(F("AT"));

        if (getResponse() == "OK")
        {
            Serial.println(F("[M] OK response received"));
            return true;
        }

        return false;
    }
    else
    {
        Serial.println(F("[M][E] Set device mode to AT command mode first!"));
        return false;
    }
}

const bool Node_HC12::changeBaudrate(const uint32_t br)
{
    if (mode == AT_COMMAND_MODE)
    {
        if (baudrate == br)
        {
            Serial.println(F("[M] Baudrate not changed. Already the same"));
            return true;
        }

        if (!isBaudrateAllowed(br))
            return false;

        const String expectedResponse = String(F("OK+B")) + String(br);

        serial->print(F("AT+B"));
        serial->print(br);
        delay(40);

        if (getResponse() == expectedResponse)
        {
            baudrate = br;

            serial->end();
            serial->begin(br);

            Serial.print(F("[M] Changing baudrate to "));
            Serial.print(br);
            Serial.print(F(" was successful"));
            Serial.println();

            setToATCommandMode();

            return true;
        }

        Serial.println(F("[M][E] Baudrate change failed"));
        return false;
    }
    else
    {
        Serial.println(F("[M][E] Set device mode to AT command mode first!"));
        return false;
    }
}

const uint32_t Node_HC12::checkDeviceBaudrate()
{
    if (mode == AT_COMMAND_MODE)
    {
        serial->end();

        for (size_t baudrateIndex = 0; baudrateIndex < sizeof(BAUDRATES) / sizeof(const uint32_t); ++baudrateIndex)
        {
            serial->begin(BAUDRATES[baudrateIndex]);

            if (testAT())
            {
                Serial.print(F("[M] Current baudrate found at "));
                Serial.print(BAUDRATES[baudrateIndex]);
                Serial.println();

                serial->end();

                return BAUDRATES[baudrateIndex];
            }

            serial->end();
        }

        Serial.println(F("[M][E] Baudrate not found. Could not get a response"));
        return false;
    }
    else
    {
        Serial.println(F("[M][E] Set device mode to AT command mode first!"));
        return false;
    }
}

const bool Node_HC12::changeChannel(const uint8_t ch)
{
    if (mode == AT_COMMAND_MODE)
    {
        if (channel == ch)
        {
            Serial.println(F("[M] Channel not changed. Already the same"));
            return true;
        }

        if (!ch || ch > 127)
        {
            Serial.println(F("[M][E] Channel out of bounds!"));
            return false;
        }

        String chInString;

        if (ch > 99) // Three digits
            chInString = String(ch);
        else if (ch > 9) // Two digits
            chInString = String(F("0")) + String(ch);
        else // One digits
            chInString = String(F("00")) + String(ch);

        const String expectedResponse = String(F("OK+C")) + chInString;

        serial->print(F("AT+C"));
        serial->print(chInString);

        if (getResponse() == expectedResponse)
        {
            channel = ch;

            Serial.print(F("[M] Changing channel to "));
            Serial.print(ch);
            Serial.print(F(" was successful"));
            Serial.println();

            setToATCommandMode();

            return true;
        }

        Serial.println(F("[M][E] Channel change failed"));
        return false;
    }
    else
    {
        Serial.println(F("[M][E] Set device mode to AT command mode first!"));
        return false;
    }
}

const uint8_t Node_HC12::checkDeviceChannel() const
{
    if (mode == AT_COMMAND_MODE)
    {
        serial->print(F("AT+RC"));

        const String response = getResponse();
        if (response.startsWith(F("OK+RC")))
        {
            const uint8_t ch = response.substring(5).toInt();

            Serial.print(F("[M] Channel detected at channel "));
            Serial.print(ch);
            Serial.println();

            return ch;
        }

        Serial.println(F("[M][E] Failed checking device channel"));
        return false;
    }
    else
    {
        Serial.println(F("[M][E] Set device mode to AT command mode first!"));
        return false;
    }
}

const bool Node_HC12::isBaudrateAllowed(const uint32_t br)
{
    for (size_t baudrateIndex = 0; baudrateIndex < sizeof(BAUDRATES) / sizeof(const uint32_t); ++baudrateIndex)
    {
        if (br == BAUDRATES[baudrateIndex])
        {
            // Serial.println(F("[M] Baudrate is fine"));
            return true;
        }
    }

    Serial.println(F("[M][E] Baudrate cannot be used"));
    return false;
}

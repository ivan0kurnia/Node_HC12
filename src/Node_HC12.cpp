#include <Arduino.h>
#include <SoftwareSerial.h>

#include "Node_HC12.h"

// #define SLOW_CPU defined(ARDUINO_ARCH_AVR)
#define FAST_CPU defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)

// Notice: this library might fail to detect responses in high baudrates due to the nature of SoftwareSerial
const uint32_t Node_HC12::BAUDRATES[]{1200UL, 2400UL, 4800UL, 9600UL, 19200UL, 38400UL, 57600UL, 115200UL};

#if FAST_CPU
uint32_t Node_HC12::responseTimeout = 80UL;
#else
uint32_t Node_HC12::responseTimeout = 40UL;
#endif

Node_HC12::Node_HC12(SoftwareSerial *const serial, const uint8_t setPin) : serial{serial}, SET_PIN{setPin}
{
}

const bool Node_HC12::begin(const uint32_t br, const uint8_t ch)
{
    if (!isBaudrateAllowed(br) || !isChannelAllowed(ch))
        return false;

    pinMode(SET_PIN, OUTPUT);
    setToATCommandMode();

    baudrate = checkDeviceBaudrate();
    if (!baudrate)
    {
        end();
        return false;
    }

    serial->begin(baudrate);

    if (!changeBaudrate(br))
    {
        end();
        return false;
    }

    channel = checkDeviceChannel();
    if (!changeChannel(ch))
    {
        end();
        return false;
    }

    setToTransmissionMode();

    Serial.println(F("[M] Begin sequence successful"));
    return true;
}

void Node_HC12::end()
{
    serial->end();

    if (isAsleep())
        wake();

    setToATCommandMode();
    pinMode(SET_PIN, INPUT);

    baudrate = 0U;
    channel = 0U;

    Serial.println(F("[M] End sequence successful"));
}

void Node_HC12::setToATCommandMode()
{
    if (getMode() == AT_COMMAND_MODE)
    {
        setToTransmissionMode();
        setToATCommandMode();
    }
    else
    {
#if DEBUG_MODE
        Serial.println(F("[M] Changing mode to AT command mode"));
#endif

        mode = AT_COMMAND_MODE;
        digitalWrite(SET_PIN, AT_COMMAND_MODE);
        delay(40UL);
    }
}

void Node_HC12::setToTransmissionMode()
{
    if (getMode() == TRANSMISSION_MODE)
    {
        setToATCommandMode();
        setToTransmissionMode();
    }
    else
    {
#if DEBUG_MODE
        Serial.println(F("[M] Changing mode to transmission mode"));
#endif

        mode = TRANSMISSION_MODE;
        digitalWrite(SET_PIN, TRANSMISSION_MODE);
        delay(80UL);
    }
}

void Node_HC12::clearSerialBuffer() const
{
    while (serial->available())
    {
        serial->read();

#if FAST_CPU
        const uint32_t previousMicros = micros();
        while (micros() - previousMicros < 5120UL)
            if (serial->available())
                break;
#else
        delay(1U);
#endif
    }
}

const String Node_HC12::getResponse(const uint32_t timeout) const
{
    String response = "";

    const uint32_t previousMillis = millis();
    while (millis() - previousMillis < timeout)
    {
        if (serial->available())
        {
            do
            {
                response += static_cast<char>(serial->read());

#if FAST_CPU
                const uint32_t previousMicros = micros();
                while (micros() - previousMicros < 5120UL)
                    if (serial->available())
                        break;
#else
                delay(1U);
#endif
            } while (serial->available());
            response.trim();

#if DEBUG_MODE
            Serial.print(F("[R] "));
            Serial.print(response);
            Serial.println();
#endif

            break;
        }
    }

    return response;
}

const bool Node_HC12::testAT() const
{
    if (getMode() == AT_COMMAND_MODE)
    {
        clearSerialBuffer();
        serial->print(F("AT"));

        if (getResponse() == F("OK"))
        {
#if DEBUG_MODE
            Serial.println(F("[M] OK response received"));
#endif

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
    if (getMode() == AT_COMMAND_MODE)
    {
        if (getBaudrate() == br)
        {
#if DEBUG_MODE
            Serial.println(F("[M] Baudrate not changed. Already the same"));
#endif

            return true;
        }

        if (!isBaudrateAllowed(br))
            return false;

        const String expectedResponse = String(F("OK+B")) + String(br);

        clearSerialBuffer();
        serial->print(F("AT+B"));
        serial->print(br);
        delay(40UL);

        if (getResponse() == expectedResponse)
        {
            baudrate = br;

            serial->end();
            serial->begin(br);

#if DEBUG_MODE
            Serial.print(F("[M] Changing baudrate to "));
            Serial.print(br);
            Serial.print(F(" was successful"));
            Serial.println();
#endif

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

const uint32_t Node_HC12::checkDeviceBaudrate() const
{
    if (getMode() == AT_COMMAND_MODE)
    {
        serial->end();
        delay(40U);

        for (size_t baudrateIndex = 0U; baudrateIndex < sizeof(BAUDRATES) / sizeof(const uint32_t); ++baudrateIndex)
        {
            serial->begin(BAUDRATES[baudrateIndex]);

#if DEBUG_MODE
            Serial.print(F("[M] Testing response at baudrate "));
            Serial.print(BAUDRATES[baudrateIndex]);
            Serial.println();
#endif

            if (testAT())
            {
#if DEBUG_MODE
                Serial.print(F("[M] Current baudrate found at "));
                Serial.print(BAUDRATES[baudrateIndex]);
                Serial.println();
#endif

                serial->end();

                return BAUDRATES[baudrateIndex];
            }

            serial->end();
            delay(40U);
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
    if (getMode() == AT_COMMAND_MODE)
    {
        if (getChannel() == ch)
        {
#if DEBUG_MODE
            Serial.println(F("[M] Channel not changed. Already the same"));
#endif

            return true;
        }

        if (!isChannelAllowed(ch))
            return false;

        String chInString;

        if (ch > 99U) // Three digits
            chInString = String(ch);
        else if (ch > 9U) // Two digits
            chInString = String(F("0")) + String(ch);
        else // One digits
            chInString = String(F("00")) + String(ch);

        const String expectedResponse = String(F("OK+C")) + chInString;

        clearSerialBuffer();
        serial->print(F("AT+C"));
        serial->print(chInString);

        if (getResponse() == expectedResponse)
        {
            channel = ch;

#if DEBUG_MODE
            Serial.print(F("[M] Changing channel to "));
            Serial.print(ch);
            Serial.print(F(" was successful"));
            Serial.println();
#endif

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
    if (getMode() == AT_COMMAND_MODE)
    {
        clearSerialBuffer();
        serial->print(F("AT+RC"));

        const String response = getResponse();
        if (response.startsWith(F("OK+RC")))
        {
            const uint8_t ch = response.substring(5U).toInt();

#if DEBUG_MODE
            Serial.print(F("[M] Channel detected at channel "));
            Serial.print(ch);
            Serial.println();
#endif

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

const String Node_HC12::checkFirmwareVersion() const
{
    if (getMode() == AT_COMMAND_MODE)
    {
        clearSerialBuffer();
        serial->print(F("AT+V"));

        return getResponse();
    }
    else
    {
        Serial.println(F("[M][E] Set device mode to AT command mode first!"));
        return "";
    }
}

const bool Node_HC12::sleep()
{
    if (getMode() != AT_COMMAND_MODE)
    {
        setToATCommandMode();
    }

    clearSerialBuffer();
    serial->print(F("AT+SLEEP"));

    if (getResponse() == F("OK+SLEEP"))
    {
        sleeping = true;
        setToTransmissionMode();

        Serial.println(F("[M] Device is now asleep"));
        return true;
    }
    else
    {
        setToTransmissionMode();

        Serial.println(F("[M][E] Failed to sleep device"));
        return false;
    }
}

void Node_HC12::wake()
{
    sleeping = false;
    setToTransmissionMode();

    Serial.println(F("[M] Device is now awake"));
}

const bool Node_HC12::isBaudrateAllowed(const uint32_t br)
{
    for (size_t baudrateIndex = 0U; baudrateIndex < sizeof(BAUDRATES) / sizeof(const uint32_t); ++baudrateIndex)
    {
        if (br == BAUDRATES[baudrateIndex])
        {
#if DEBUG_MODE
            Serial.println(F("[M] Baudrate is allowed"));
#endif

            return true;
        }
    }

    Serial.println(F("[M][E] Baudrate cannot be used"));
    return false;
}

const bool Node_HC12::isChannelAllowed(const uint8_t ch)
{
    if (ch >= 1U && ch <= 127U)
    {
#if DEBUG_MODE
        Serial.println(F("[M] Channel is allowed"));
#endif

        return true;
    }
    else
    {
        Serial.println(F("[M][E] Channel out of bounds"));
        return false;
    }
}
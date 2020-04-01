#ifndef NODE_HC12_h
#define NODE_HC12_h

#include <Arduino.h>
#include <SoftwareSerial.h>

#define DEBUG_MODE false

class Node_HC12
{
public:
    Node_HC12(SoftwareSerial *const serial, const uint8_t setPin);

    const bool begin(const uint32_t br, const uint8_t ch);
    void end();

    const bool getMode() const { return mode; }
    void setToATCommandMode();
    void setToTransmissionMode();

    void clearSerialBuffer() const;

    // For AT command use only
    const String getResponse(const uint32_t timeout = responseTimeout) const;

    const bool testAT() const;
    const uint32_t checkDeviceBaudrate() const;
    const uint8_t checkDeviceChannel() const;
    const String checkFirmwareVersion() const;

    const uint32_t getBaudrate() const { return baudrate; }
    const bool changeBaudrate(const uint32_t br);

    const uint8_t getChannel() const { return channel; }
    const bool changeChannel(const uint8_t ch);

    const bool sleep();
    void wake();

    const bool isAsleep() const { return sleeping; }

    static const bool isBaudrateAllowed(const uint32_t br);
    static const bool isChannelAllowed(const uint8_t ch);

    static const uint32_t getResponseTimeout() { return responseTimeout; }
    static void setResponseTimeout(const uint32_t timeout) { responseTimeout = timeout; }

    static const bool AT_COMMAND_MODE = LOW;
    static const bool TRANSMISSION_MODE = HIGH;

    static const uint32_t BAUDRATES[];

private:
    SoftwareSerial *const serial;
    const uint8_t SET_PIN;

    bool mode = AT_COMMAND_MODE;
    bool sleeping = false;

    uint32_t baudrate{};
    uint8_t channel{};

    static uint32_t responseTimeout;
};

#endif
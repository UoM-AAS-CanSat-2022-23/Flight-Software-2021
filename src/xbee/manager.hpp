#pragma once

#include <cstdint>
#include <string>
#include <XBee.h>

class XBeeManager {
    XBeeWithCallbacks _xbee;
    std::uint16_t _panid;

public:
    XBeeManager();
    void loop();
    void setup(HardwareSerial&);
    std::uint8_t set_panid(std::uint16_t, std::uint16_t timeout=100);
    std::uint16_t get_panid() const;
    void send(std::string&);
};

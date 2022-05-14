#pragma once

#include <cstdint>
#include <Arduino.h>
// prevent colliding symbol errors
#undef B1

// stringification macros from https://gcc.gnu.org/onlinedocs/gcc-3.4.3/cpp/Stringification.html
#define XSTR(s) STR(s)
#define STR(s) #s

// list of compile time constants used in various parts of the code
#define TEAM_ID 1057
static constexpr std::uint16_t GCS_LINK_PANID = TEAM_ID;
static constexpr std::uint16_t PAYLOAD_LINK_PANID = TEAM_ID + 5000;

// BMP sea level pressure constant
static constexpr double SEALEVEL_PRESSURE_HPA = 1013.25;
static constexpr double SEALEVEL_PRESSURE_PA  = SEALEVEL_PRESSURE_HPA * 100;

// xbee addresses
static constexpr std::uint16_t CONTAINER_XBEE_ADDRESS = 0;
static constexpr std::uint16_t PAYLOAD_XBEE_ADDRESS = 1;
static constexpr std::uint16_t GCS_XBEE_ADDRESS = 2;

// voltage divider values
static constexpr std::uint32_t ANALOG_READ_BITS = 12;
static constexpr std::uint32_t ANALOG_READ_MAX = (1 << ANALOG_READ_BITS) - 1;
static constexpr std::uint8_t VD_PIN = 23;
static constexpr double ADC_MAX_INPUT_V = 3.3;
static constexpr double VD_INPUT_V = 5.0;
static constexpr double VD_R1 = 5'000.0; // 120'000.0
static constexpr double VD_R2 = 10'000.0; // 820'000.0

// Serial interfaces and baud rates
static auto& XBEE_SERIAL = Serial2;
static auto& GPS_SERIAL = Serial5;
static constexpr long DEBUG_SERIAL_BAUD = 230400; 
static constexpr long XBEE_SERIAL_BAUD = 230400;
static constexpr long GPS_SERIAL_BAUD = 9600;

// other pins
static constexpr std::uint8_t BUZZER_PIN = 6;
#pragma once
#include <array>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <cctype>

class MacAddress {
private:
    std::array<uint8_t, 6> address;

public:
    MacAddress(const uint8_t* addr) {
        // Serial.printf("MacAddress(const uint8_t* addr) %02X:%02X:%02X:%02X:%02X:%02X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        std::copy(addr, addr + 6, address.begin());
    }

    const uint8_t* getBytes() const {
        return address.data();
    }

    std::string toString() const {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 address[0], address[1], address[2], address[3], address[4], address[5]);
        return std::string(buf);
    }

    bool operator==(const MacAddress& other) const {
        return address == other.address;
    }

    bool operator!=(const MacAddress& other) const {
        return !(*this == other);
    }

    bool isLocallyAdministered() const {
        return address[0] & 0x02;
    }
};

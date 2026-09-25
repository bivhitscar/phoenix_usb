#pragma once

#include "report.h"

#include <string>

namespace phoenix {

class VjoyDevice {
public:
    VjoyDevice() = default;
    explicit VjoyDevice(unsigned int deviceId) : deviceId_(deviceId) {}
    VjoyDevice(const VjoyDevice&) = delete;
    VjoyDevice& operator=(const VjoyDevice&) = delete;
    ~VjoyDevice();

    bool open(std::wstring& error);
    bool update(const Report& report, std::uint8_t aux2, std::uint8_t aux3,
                std::wstring& error);
    bool update(std::uint8_t x, std::uint8_t y, std::uint8_t rx, std::uint8_t ry,
                std::uint8_t rudder, std::uint8_t throttle, std::uint8_t aux2,
                std::uint8_t aux3, bool buttonA, std::wstring& error);
    void close();

private:
    unsigned int deviceId_ = 1;
    bool acquired_ = false;
};

} // namespace phoenix
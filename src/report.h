// SPDX-License-Identifier: GPL-2.0-only
// The provisional byte mapping follows pxrc.c by Marcus Folkesson.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace phoenix {

constexpr std::uint16_t kVendorId = 0x1781;
constexpr std::uint16_t kProductId = 0x0898;
constexpr std::size_t kReportSize = 8;

struct Report {
    std::array<std::uint8_t, kReportSize> raw{};

    std::uint8_t x() const { return raw[0]; }
    std::uint8_t buttonA() const { return raw[1]; }
    std::uint8_t y() const { return raw[2]; }
    std::uint8_t rx() const { return raw[3]; }
    std::uint8_t ry() const { return raw[4]; }
    std::uint8_t rudder() const { return raw[5]; }
    std::uint8_t throttle() const { return raw[6]; }
    std::uint8_t misc() const { return raw[7]; }

    bool controllerDisconnected() const
    {
        return raw == std::array<std::uint8_t, kReportSize>{0xff, 0x00, 0xff, 0xff,
                                                            0xff, 0x00, 0x00, 0x00};
    }
};

bool parseReport(const std::uint8_t* data, std::size_t length, Report& report);

} // namespace phoenix
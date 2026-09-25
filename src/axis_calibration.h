#pragma once

#include <cstdint>
#include <string>

namespace phoenix {

struct AxisRange {
    std::uint8_t min = 255;
    std::uint8_t max = 0;

    void update(std::uint8_t sample);
    std::uint8_t normalize(std::uint8_t raw) const;
    bool isDegenerate() const;
    bool hasUsableRange() const;
};

struct CalibrationSet {
    static constexpr int kVersion = 1;

    AxisRange x{};
    AxisRange y{};
    AxisRange rx{};
    AxisRange ry{};
    AxisRange rudder{};
    AxisRange throttle{};
    AxisRange aux2{};
    AxisRange aux3{};

    bool isValid() const;
    bool hasAnyUsableAxis() const;
    std::wstring invalidAxisSummary() const;
    void applyIdentityFallback();
    bool load(const std::wstring& path, std::wstring& error);
    bool save(const std::wstring& path, std::wstring& error) const;
};

std::wstring defaultCalibrationFilePath();

} // namespace phoenix

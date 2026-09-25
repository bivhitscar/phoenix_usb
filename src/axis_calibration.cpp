#include "axis_calibration.h"

#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#undef min
#undef max

namespace phoenix {
namespace {

std::wstring trim(const std::wstring& value)
{
    const auto begin = value.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos)
        return L"";

    const auto end = value.find_last_not_of(L" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::wstring toUpper(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towupper(static_cast<wint_t>(ch)));
    });
    return value;
}

std::wstring toLower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(ch)));
    });
    return value;
}

std::optional<std::uint8_t> parseByte(const std::wstring& text)
{
    if (text.empty())
        return std::nullopt;

    wchar_t* end = nullptr;
    const unsigned long value = std::wcstoul(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != L'\0' || value > 255)
        return std::nullopt;
    return static_cast<std::uint8_t>(value);
}

AxisRange* axisRangeForName(CalibrationSet& set, const std::wstring& name)
{
    const std::wstring key = toUpper(trim(name));
    if (key == L"X")
        return &set.x;
    if (key == L"Y")
        return &set.y;
    if (key == L"RX")
        return &set.rx;
    if (key == L"RY")
        return &set.ry;
    if (key == L"RUDDER")
        return &set.rudder;
    if (key == L"THROTTLE")
        return &set.throttle;
    if (key == L"AUX2")
        return &set.aux2;
    if (key == L"AUX3")
        return &set.aux3;
    return nullptr;
}

const AxisRange* axisRangeForName(const CalibrationSet& set, const std::wstring& name)
{
    const std::wstring key = toUpper(trim(name));
    if (key == L"X")
        return &set.x;
    if (key == L"Y")
        return &set.y;
    if (key == L"RX")
        return &set.rx;
    if (key == L"RY")
        return &set.ry;
    if (key == L"RUDDER")
        return &set.rudder;
    if (key == L"THROTTLE")
        return &set.throttle;
    if (key == L"AUX2")
        return &set.aux2;
    if (key == L"AUX3")
        return &set.aux3;
    return nullptr;
}

} // namespace

void AxisRange::update(std::uint8_t sample)
{
    this->min = std::min(this->min, sample);
    this->max = std::max(this->max, sample);
}

std::uint8_t AxisRange::normalize(std::uint8_t raw) const
{
    if (this->max <= this->min)
        return raw;

    const std::uint32_t minimum = static_cast<std::uint32_t>(this->min);
    const std::uint32_t maximum = static_cast<std::uint32_t>(this->max);
    const std::uint32_t value = static_cast<std::uint32_t>(raw);
    const std::uint32_t clamped = std::max(minimum, std::min(value, maximum));
    const std::uint32_t span = maximum - minimum;
    return static_cast<std::uint8_t>((255U * (clamped - minimum)) / span);
}

bool AxisRange::isDegenerate() const
{
    return this->max <= this->min;
}

bool AxisRange::hasUsableRange() const
{
    return this->max > this->min && (static_cast<int>(this->max) - static_cast<int>(this->min)) > 10;
}

bool CalibrationSet::isValid() const
{
    return invalidAxisSummary().empty();
}

bool CalibrationSet::hasAnyUsableAxis() const
{
    return x.hasUsableRange() || y.hasUsableRange() || rx.hasUsableRange() || ry.hasUsableRange() ||
           rudder.hasUsableRange() || throttle.hasUsableRange() || aux2.hasUsableRange() ||
           aux3.hasUsableRange();
}

std::wstring CalibrationSet::invalidAxisSummary() const
{
    std::wstring invalidAxes;
    const auto appendAxis = [&](const wchar_t* name, const AxisRange& axis) {
        if (!axis.hasUsableRange()) {
            if (!invalidAxes.empty())
                invalidAxes += L", ";
            invalidAxes += name;
        }
    };

    appendAxis(L"X", x);
    appendAxis(L"Y", y);
    appendAxis(L"RX", rx);
    appendAxis(L"RY", ry);
    appendAxis(L"RUDDER", rudder);
    appendAxis(L"THROTTLE", throttle);
    appendAxis(L"AUX2", aux2);
    appendAxis(L"AUX3", aux3);
    return invalidAxes;
}

void CalibrationSet::applyIdentityFallback()
{
    const auto fallback = [](AxisRange& axis) {
        if (!axis.hasUsableRange()) {
            axis.min = 0;
            axis.max = 255;
        }
    };

    fallback(x);
    fallback(y);
    fallback(rx);
    fallback(ry);
    fallback(rudder);
    fallback(throttle);
    fallback(aux2);
    fallback(aux3);
}

bool CalibrationSet::load(const std::wstring& path, std::wstring& error)
{
    std::wifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        error = L"Calibration file not found: " + path;
        return false;
    }

    std::wstring section;
    bool sawVersion = false;
    int version = 0;

    for (std::wstring line; std::getline(file, line); ) {
        const std::wstring trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == L';' || trimmed[0] == L'#')
            continue;

        if (trimmed.front() == L'[' && trimmed.back() == L']') {
            section = trim(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }

        const auto equals = trimmed.find(L'=');
        if (equals == std::wstring::npos)
            continue;

        const std::wstring key = trim(trimmed.substr(0, equals));
        const std::wstring value = trim(trimmed.substr(equals + 1));
        const std::wstring normalizedKey = toLower(key);

        if (section == L"Calibration") {
            if (normalizedKey == L"version") {
                wchar_t* end = nullptr;
                const long parsedVersion = std::wcstol(value.c_str(), &end, 10);
                if (end == value.c_str() || *end != L'\0') {
                    error = L"Calibration file has an invalid Version entry";
                    return false;
                }
                version = static_cast<int>(parsedVersion);
                sawVersion = true;
                continue;
            }

            const auto dot = key.rfind(L'.');
            if (dot == std::wstring::npos)
                continue;

            const std::wstring axisName = key.substr(0, dot);
            const std::wstring field = toLower(key.substr(dot + 1));
            AxisRange* axis = axisRangeForName(*this, axisName);
            if (axis == nullptr)
                continue;

            const auto parsed = parseByte(value);
            if (!parsed.has_value()) {
                error = L"Calibration file contains an invalid numeric value for " + key;
                return false;
            }

            if (field == L"min")
                axis->min = *parsed;
            else if (field == L"max")
                axis->max = *parsed;
        }
    }

    if (!sawVersion || version != kVersion) {
        error = L"Calibration file version mismatch or missing Version entry";
        return false;
    }

    if (!isValid()) {
        error = L"Calibration file is missing or incomplete; one or more axes are invalid";
        return false;
    }

    return true;
}

bool CalibrationSet::save(const std::wstring& path, std::wstring& error) const
{
    try {
        const std::filesystem::path filePath(path);
        if (!filePath.parent_path().empty())
            std::filesystem::create_directories(filePath.parent_path());

        std::wofstream output(filePath, std::ios::binary);
        if (!output.is_open()) {
            error = L"Unable to open calibration file for writing: " + path;
            return false;
        }

        const auto writeAxis = [&](const wchar_t* name, const AxisRange& axis) {
            output << name << L".Min=" << static_cast<int>(axis.min) << L"\n";
            output << name << L".Max=" << static_cast<int>(axis.max) << L"\n";
        };

        output << L"[Calibration]\n";
        output << L"Version=" << kVersion << L"\n";
        writeAxis(L"X", x);
        writeAxis(L"Y", y);
        writeAxis(L"RX", rx);
        writeAxis(L"RY", ry);
        writeAxis(L"RUDDER", rudder);
        writeAxis(L"THROTTLE", throttle);
        writeAxis(L"AUX2", aux2);
        writeAxis(L"AUX3", aux3);
        output.flush();
        return true;
    } catch (const std::exception& exc) {
        const std::string what = exc.what();
        error = L"Failed to write calibration file: " + std::wstring(what.begin(), what.end());
        return false;
    }
}

std::wstring defaultCalibrationFilePath()
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::filesystem::path exePath = std::filesystem::current_path();
    if (length > 0 && length < MAX_PATH) {
        exePath = std::filesystem::path(buffer);
    }

    return (exePath.parent_path().empty() ? std::filesystem::current_path() : exePath.parent_path()) /
           L"calibration.ini";
}

} // namespace phoenix

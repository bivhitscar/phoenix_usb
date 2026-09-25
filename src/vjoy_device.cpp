#include "vjoy_device.h"

#include <Windows.h>
#include <vjoyinterface.h>

namespace phoenix {
namespace {

LONG axisValue(std::uint8_t value)
{
    return static_cast<LONG>((static_cast<unsigned long>(value) * VJOY_AXIS_MAX_VALUE) / 255);
}

} // namespace

VjoyDevice::~VjoyDevice()
{
    close();
}

bool VjoyDevice::open(std::wstring& error)
{
    if (!vJoyEnabled()) {
        error = L"vJoy is not enabled or its driver is unavailable";
        return false;
    }

    const VjdStat status = GetVJDStatus(deviceId_);
    if (status == VJD_STAT_BUSY) {
        error = L"vJoy device " + std::to_wstring(deviceId_) +
                L" is already owned by another feeder";
        return false;
    }
    if (status == VJD_STAT_MISS) {
        error = L"vJoy device " + std::to_wstring(deviceId_) +
                L" is missing or disabled";
        return false;
    }
    if (status != VJD_STAT_FREE && status != VJD_STAT_OWN) {
        error = L"vJoy device " + std::to_wstring(deviceId_) +
                L" has an unavailable status";
        return false;
    }

    if (status == VJD_STAT_FREE && !AcquireVJD(deviceId_)) {
        error = L"Failed to acquire vJoy device " + std::to_wstring(deviceId_);
        return false;
    }
    acquired_ = true;
    return true;
}

bool VjoyDevice::update(const Report& report, std::uint8_t aux2, std::uint8_t aux3,
                        std::wstring& error)
{
    return update(report.x(), report.y(), report.rx(), report.ry(), report.rudder(),
                  report.throttle(), aux2, aux3, report.buttonA() != 0, error);
}

bool VjoyDevice::update(std::uint8_t x, std::uint8_t y, std::uint8_t rx, std::uint8_t ry,
                        std::uint8_t rudder, std::uint8_t throttle, std::uint8_t aux2,
                        std::uint8_t aux3, bool buttonA, std::wstring& error)
{
    if (!acquired_) {
        error = L"vJoy device is not acquired";
        return false;
    }

    JOYSTICK_POSITION position{};
    position.bDevice = static_cast<BYTE>(deviceId_);
    position.wAxisX = axisValue(x);
    position.wAxisY = axisValue(y);
    position.wAxisXRot = axisValue(rx);
    position.wAxisYRot = axisValue(ry);
    position.wRudder = axisValue(rudder);
    position.wThrottle = axisValue(throttle);
    position.wAxisZ = axisValue(aux3);
    position.wSlider = axisValue(aux2);
    position.lButtons = buttonA ? 1 : 0;

    if (!UpdateVJD(deviceId_, &position)) {
        error = L"Failed to update vJoy device " + std::to_wstring(deviceId_);
        return false;
    }
    return true;
}

void VjoyDevice::close()
{
    if (acquired_) {
        RelinquishVJD(deviceId_);
        acquired_ = false;
    }
}

} // namespace phoenix
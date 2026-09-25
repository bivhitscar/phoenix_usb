#pragma once

#include "report.h"

#include <Windows.h>

#include <string>

namespace phoenix {

class HidDevice {
public:
    HidDevice() = default;
    HidDevice(const HidDevice&) = delete;
    HidDevice& operator=(const HidDevice&) = delete;
    ~HidDevice();

    bool open(std::wstring& error);
    bool read(Report& report, std::wstring& error);
    void cancelPendingRead();
    void close();

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    HANDLE event_ = nullptr;
    DWORD inputReportLength_ = kReportSize;
};

} // namespace phoenix
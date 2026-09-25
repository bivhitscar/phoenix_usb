#include "hid_device.h"

#include <SetupAPI.h>
#include <hidsdi.h>

#include <array>
#include <vector>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

namespace phoenix {
namespace {

std::wstring errorMessage(const wchar_t* operation, DWORD code)
{
    return std::wstring(operation) + L" failed (error " + std::to_wstring(code) + L")";
}

} // namespace

HidDevice::~HidDevice()
{
    close();
}

bool HidDevice::open(std::wstring& error)
{
    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfo = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr,
                                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfo == INVALID_HANDLE_VALUE) {
        error = errorMessage(L"SetupDiGetClassDevs", GetLastError());
        return false;
    }

    bool found = false;
    for (DWORD index = 0; !found; ++index) {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(deviceInfo, nullptr, &hidGuid, index, &interfaceData)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;
            continue;
        }

        DWORD detailSize = 0;
        SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, nullptr, 0, &detailSize, nullptr);
        if (detailSize == 0)
            continue;

        std::vector<std::byte> detailBuffer(detailSize);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuffer.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, detail, detailSize,
                                              nullptr, nullptr))
            continue;

        HANDLE candidate = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                        FILE_FLAG_OVERLAPPED, nullptr);
        if (candidate == INVALID_HANDLE_VALUE)
            candidate = CreateFileW(detail->DevicePath, GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_OVERLAPPED, nullptr);
        if (candidate == INVALID_HANDLE_VALUE)
            continue;

        HIDD_ATTRIBUTES attributes{};
        attributes.Size = sizeof(attributes);
        if (HidD_GetAttributes(candidate, &attributes) &&
            attributes.VendorID == kVendorId && attributes.ProductID == kProductId) {
            handle_ = candidate;
            found = true;
        } else {
            CloseHandle(candidate);
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfo);
    if (!found) {
        error = L"No HID device with VID 1781 and PID 0898 was found";
        return false;
    }

    event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (event_ == nullptr) {
        error = errorMessage(L"CreateEvent", GetLastError());
        close();
        return false;
    }

    PHIDP_PREPARSED_DATA preparsedData = nullptr;
    if (!HidD_GetPreparsedData(handle_, &preparsedData)) {
        error = errorMessage(L"HidD_GetPreparsedData", GetLastError());
        close();
        return false;
    }

    HIDP_CAPS capabilities{};
    const NTSTATUS capsStatus = HidP_GetCaps(preparsedData, &capabilities);
    HidD_FreePreparsedData(preparsedData);
    if (capsStatus != HIDP_STATUS_SUCCESS) {
        error = L"HidP_GetCaps failed (status " + std::to_wstring(capsStatus) + L")";
        close();
        return false;
    }
    if (capabilities.InputReportByteLength != kReportSize &&
        capabilities.InputReportByteLength != kReportSize + 1) {
        error = L"HID input report length is " +
                std::to_wstring(capabilities.InputReportByteLength) +
                L" bytes (expected " + std::to_wstring(kReportSize) + L" or " +
                std::to_wstring(kReportSize + 1) + L")";
        close();
        return false;
    }
    inputReportLength_ = capabilities.InputReportByteLength;
    return true;
}

bool HidDevice::read(Report& report, std::wstring& error)
{
    if (handle_ == INVALID_HANDLE_VALUE || event_ == nullptr) {
        error = L"HID device is not open";
        return false;
    }

    std::array<std::uint8_t, kReportSize + 1> buffer{};
    OVERLAPPED overlapped{};
    overlapped.hEvent = event_;
    ResetEvent(event_);

    DWORD bytesRead = 0;
    BOOL result = ReadFile(handle_, buffer.data(), static_cast<DWORD>(buffer.size()),
                           &bytesRead, &overlapped);
    if (!result) {
        const DWORD readError = GetLastError();
        if (readError != ERROR_IO_PENDING) {
            error = errorMessage(L"ReadFile", readError);
            return false;
        }

        const DWORD waitResult = WaitForSingleObject(event_, INFINITE);
        if (waitResult != WAIT_OBJECT_0) {
            error = errorMessage(L"WaitForSingleObject", GetLastError());
            CancelIoEx(handle_, &overlapped);
            return false;
        }
        if (!GetOverlappedResult(handle_, &overlapped, &bytesRead, FALSE)) {
            error = errorMessage(L"GetOverlappedResult", GetLastError());
            CancelIoEx(handle_, &overlapped);
            return false;
        }
    }

    if (bytesRead != inputReportLength_) {
        error = L"Received a HID report with an unexpected length (received " +
                std::to_wstring(bytesRead) + L", expected " +
                std::to_wstring(inputReportLength_) + L" bytes)";
        return false;
    }

    const std::size_t payloadOffset = inputReportLength_ - kReportSize;
    if (!parseReport(buffer.data() + payloadOffset, kReportSize, report)) {
        error = L"Received a HID report with an unexpected length (expected 8 bytes)";
        return false;
    }
    return true;
}

void HidDevice::cancelPendingRead()
{
    if (handle_ != INVALID_HANDLE_VALUE)
        CancelIoEx(handle_, nullptr);
}

void HidDevice::close()
{
    if (handle_ != INVALID_HANDLE_VALUE) {
        cancelPendingRead();
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    if (event_ != nullptr) {
        CloseHandle(event_);
        event_ = nullptr;
    }
}

} // namespace phoenix
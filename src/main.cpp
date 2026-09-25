#include "axis_calibration.h"
#include "hid_device.h"
#include "vjoy_device.h"

#include <Windows.h>
#include <atomic>
#include <array>
#include <chrono>
#include <conio.h>
#include <cwchar>
#include <iomanip>
#include <iostream>
#include <limits>

#undef min
#undef max

namespace {
std::atomic_bool running{true};
phoenix::HidDevice* activeDevice = nullptr;

bool waitForReconnect()
{
    std::wcout << L"Waiting for PhoenixRC HID device to reconnect...\n";
    while (running) {
        Sleep(1000);
        if (running)
            return true;
    }
    return false;
}

BOOL WINAPI consoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
        running = false;
        if (activeDevice != nullptr)
            activeDevice->cancelPendingRead();
        return TRUE;
    }
    return FALSE;
}

void printReport(const phoenix::Report& report, bool aux2Sample,
                 const phoenix::CalibrationSet& calibration)
{
    const std::uint8_t aux2Value = aux2Sample ? report.misc() : 0;
    const std::uint8_t aux3Value = aux2Sample ? 0 : report.misc();

    std::cout << "raw:";
    for (const auto byte : report.raw)
        std::cout << " " << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    std::cout << std::dec << " | X=" << static_cast<int>(report.x())
              << " Y=" << static_cast<int>(report.y())
              << " RX=" << static_cast<int>(report.rx())
              << " RY=" << static_cast<int>(report.ry())
              << " RUDDER=" << static_cast<int>(report.rudder())
              << " THROTTLE=" << static_cast<int>(report.throttle())
              << (aux2Sample ? " AUX2=" : " AUX3=") << static_cast<int>(report.misc())
              << " BTN_A=" << static_cast<int>(report.buttonA())
              << " | normalized: X=" << static_cast<int>(calibration.x.normalize(report.x()))
              << " Y=" << static_cast<int>(calibration.y.normalize(report.y()))
              << " RX=" << static_cast<int>(calibration.rx.normalize(report.rx()))
              << " RY=" << static_cast<int>(calibration.ry.normalize(report.ry()))
              << " RUDDER=" << static_cast<int>(calibration.rudder.normalize(report.rudder()))
              << " THROTTLE=" << static_cast<int>(calibration.throttle.normalize(report.throttle()))
              << (aux2Sample ? " AUX2=" : " AUX3=")
              << static_cast<int>(aux2Sample ? calibration.aux2.normalize(aux2Value)
                                            : calibration.aux3.normalize(aux3Value))
              << " BTN_A=" << static_cast<int>(report.buttonA()) << '\n';
}

struct AuxCalibration {
    bool aux2Next = true;
    std::uint8_t aux2 = 128;
    std::uint8_t aux3 = 128;
};

bool calibrateAux(phoenix::HidDevice& device, AuxCalibration& calibration,
                  bool& controllerDisconnected, std::wstring& error)
{
    constexpr int sampleCount = 120;
    constexpr int preflightCount = 10;
    controllerDisconnected = false;
    std::array<std::uint8_t, 2> minimum{std::numeric_limits<std::uint8_t>::max(),
                                        std::numeric_limits<std::uint8_t>::max()};
    std::array<std::uint8_t, 2> maximum{0, 0};
    std::array<std::uint8_t, 2> latest{128, 128};

    std::array<phoenix::Report, preflightCount> preflightReports{};
    for (auto& report : preflightReports) {
        if (!device.read(report, error))
            return false;
        if (report.controllerDisconnected()) {
            controllerDisconnected = true;
            error = L"PhoenixRC controller is disconnected from the USB adapter";
            return false;
        }
    }

    std::wcout << L"Calibration: hold AUX 2 steady and move AUX 3 through its full range...\n"
               << std::flush;
    for (int index = 0; index < sampleCount; ++index) {
        phoenix::Report report{};
        if (index < preflightCount) {
            report = preflightReports[index];
        } else {
            if (!device.read(report, error))
                return false;
            if (report.controllerDisconnected()) {
                controllerDisconnected = true;
                error = L"PhoenixRC controller is disconnected from the USB adapter";
                return false;
            }
        }

        const int stream = index % 2;
        minimum[stream] = std::min(minimum[stream], report.misc());
        maximum[stream] = std::max(maximum[stream], report.misc());
        latest[stream] = report.misc();
    }

    const int range0 = maximum[0] - minimum[0];
    const int range1 = maximum[1] - minimum[1];
    if (std::max(range0, range1) < 10 || std::abs(range0 - range1) < 5) {
        error = L"AUX calibration could not distinguish the two channels; move AUX 3 farther and retry";
        return false;
    }

    const bool aux2Stream0 = range0 < range1;
    const int nextStream = sampleCount % 2;
    calibration.aux2Next = aux2Stream0 == (nextStream == 0);
    calibration.aux2 = latest[aux2Stream0 ? 0 : 1];
    calibration.aux3 = latest[aux2Stream0 ? 1 : 0];
    std::wcout << L"Calibration complete: AUX 2 range " << std::min(range0, range1)
               << L", AUX 3 range " << std::max(range0, range1) << L".\n";
    return true;
}

void printUsage()
{
    std::wcout << L"PhoenixRC USB adapter v" << PHOENIX_USB_VERSION << L"\n"
               << L"Reads the PhoenixRC HID controller and feeds its controls to vJoy.\n\n"
               << L"Usage:\n"
               << L"  phoenix_usb_adapter.exe [options]\n\n"
               << L"Options:\n"
               << L"  --help, -h                Show this help text and exit.\n"
               << L"  --debug                   Print raw and normalized HID values while streaming.\n"
               << L"  --vjoy-device <1-16>      Select the vJoy device (default: 1).\n"
               << L"  --calibrate               Run calibration mode, save calibration.ini, and exit.\n"
               << L"  --calibration-file <path> Override the calibration file path.\n"
               << L"  --skip-aux-detection      Skip AUX2/AUX3 auto-detection for controllers\n"
               << L"                            that don't provide a second AUX channel.\n\n"
               << L"Calibration workflow:\n"
               << L"  1. Run with --calibrate and move each control through its full range.\n"
               << L"  2. The saved calibration file is stored next to the executable by default.\n"
               << L"  3. Normal streaming reuses that file automatically when present.\n";
}

bool parseArguments(int argc, wchar_t* argv[], unsigned int& deviceId, bool& debugOutput,
                    bool& calibrate, std::wstring& calibrationFilePath, bool& skipAuxDetection)
{
    calibrationFilePath = phoenix::defaultCalibrationFilePath();
    for (int index = 1; index < argc; ++index) {
        const std::wstring argument = argv[index];
        if (argument == L"--debug") {
            debugOutput = true;
            continue;
        }
        if (argument == L"--calibrate") {
            calibrate = true;
            continue;
        }
        if (argument == L"--skip-aux-detection") {
            skipAuxDetection = true;
            continue;
        }
        if (argument == L"--calibration-file") {
            if (index + 1 >= argc)
                return false;
            calibrationFilePath = argv[++index];
            continue;
        }
        if (argument == L"--vjoy-device") {
            if (index + 1 >= argc)
                return false;
            wchar_t* end = nullptr;
            const unsigned long parsed = std::wcstoul(argv[++index], &end, 10);
            if (end == argv[index] || *end != L'\0' || parsed < 1 || parsed > 16)
                return false;
            deviceId = static_cast<unsigned int>(parsed);
            continue;
        }
        if (argument == L"--help" || argument == L"-h")
            return true;
        return false;
    }
    return true;
}

bool runCalibration(const std::wstring& calibrationPath, phoenix::HidDevice& device,
                    bool skipAuxDetection, std::wstring& error)
{
    phoenix::CalibrationSet calibration{};
    AuxCalibration auxCalibration{};
    bool controllerDisconnected = false;
    if (!skipAuxDetection && !calibrateAux(device, auxCalibration, controllerDisconnected, error))
        return false;

    std::wcout << L"Calibration: move all sticks, axes, and trims through their full range, including center, minimum, and maximum positions.\n"
               << L"Press any key when you are finished.\n";

    while (true) {
        if (_kbhit()) {
            _getwch();
            break;
        }

        phoenix::Report report{};
        if (!device.read(report, error))
            return false;
        if (report.controllerDisconnected()) {
            error = L"PhoenixRC controller is disconnected from the USB adapter";
            return false;
        }

        calibration.x.update(report.x());
        calibration.y.update(report.y());
        calibration.rx.update(report.rx());
        calibration.ry.update(report.ry());
        calibration.rudder.update(report.rudder());
        calibration.throttle.update(report.throttle());

        const bool aux2Sample = skipAuxDetection || auxCalibration.aux2Next;
        calibration.aux2.update(aux2Sample ? report.misc() : auxCalibration.aux2);
        calibration.aux3.update(aux2Sample ? auxCalibration.aux3 : report.misc());

        if (!skipAuxDetection) {
            if (auxCalibration.aux2Next)
                auxCalibration.aux2 = report.misc();
            else
                auxCalibration.aux3 = report.misc();
            auxCalibration.aux2Next = !auxCalibration.aux2Next;
        }
    }

    const std::wstring invalidAxes = calibration.invalidAxisSummary();
    if (!invalidAxes.empty()) {
        std::wcout << L"Calibration warning: no usable range detected for " << invalidAxes
                   << L"; those axes will remain in identity mode for this controller.\n";
        if (!calibration.hasAnyUsableAxis()) {
            error = L"No valid calibration ranges were detected on this controller; no axis reported a usable travel range.";
            return false;
        }
        calibration.applyIdentityFallback();
    }

    std::wcout << L"Calibration summary:\n"
               << L"  X: " << static_cast<int>(calibration.x.min) << L".." << static_cast<int>(calibration.x.max) << L"\n"
               << L"  Y: " << static_cast<int>(calibration.y.min) << L".." << static_cast<int>(calibration.y.max) << L"\n"
               << L"  RX: " << static_cast<int>(calibration.rx.min) << L".." << static_cast<int>(calibration.rx.max) << L"\n"
               << L"  RY: " << static_cast<int>(calibration.ry.min) << L".." << static_cast<int>(calibration.ry.max) << L"\n"
               << L"  RUDDER: " << static_cast<int>(calibration.rudder.min) << L".." << static_cast<int>(calibration.rudder.max) << L"\n"
               << L"  THROTTLE: " << static_cast<int>(calibration.throttle.min) << L".." << static_cast<int>(calibration.throttle.max) << L"\n"
               << L"  AUX2: " << static_cast<int>(calibration.aux2.min) << L".." << static_cast<int>(calibration.aux2.max) << L"\n"
               << L"  AUX3: " << static_cast<int>(calibration.aux3.min) << L".." << static_cast<int>(calibration.aux3.max) << L"\n";

    if (!calibration.save(calibrationPath, error))
        return false;

    std::wcout << L"Calibration saved to " << calibrationPath << L"\n";
    return true;
}
} // namespace

int wmain(int argc, wchar_t* argv[])
{
    for (int index = 1; index < argc; ++index) {
        if (std::wstring(argv[index]) == L"--help" || std::wstring(argv[index]) == L"-h") {
            printUsage();
            return 0;
        }
    }

    unsigned int vjoyDeviceId = 1;
    bool debugOutput = false;
    bool calibrate = false;
    bool skipAuxDetection = false;
    std::wstring calibrationFilePath;
    if (!parseArguments(argc, argv, vjoyDeviceId, debugOutput, calibrate, calibrationFilePath, skipAuxDetection)) {
        std::wcerr << L"Usage: phoenix_usb_adapter.exe [--debug] [--vjoy-device 1-16] [--calibrate] [--calibration-file <path>] [--skip-aux-detection]\n";
        return 2;
    }

    if (calibrate) {
        SetConsoleCtrlHandler(consoleHandler, TRUE);
        std::wcout << L"PhoenixRC USB adapter calibration mode\n";
        phoenix::HidDevice device;
        std::wstring error;
        if (!device.open(error)) {
            std::wcerr << L"Unable to open PhoenixRC HID device: " << error << L'\n';
            return 1;
        }

        activeDevice = &device;
        if (!runCalibration(calibrationFilePath, device, skipAuxDetection, error)) {
            std::wcerr << L"Calibration failed: " << error << L'\n';
            activeDevice = nullptr;
            device.close();
            return 1;
        }
        activeDevice = nullptr;
        device.close();
        return 0;
    }

    SetConsoleCtrlHandler(consoleHandler, TRUE);
    std::wcout << L"PhoenixRC USB adapter diagnostic shell\n"
               << L"Looking for HID VID 1781 PID 0898...\n";

    phoenix::HidDevice device;
    std::wstring error;
    phoenix::VjoyDevice vjoy(vjoyDeviceId);
    if (!vjoy.open(error)) {
        std::wcerr << L"Unable to open vJoy output: " << error << L'\n';
        return 1;
    }

    phoenix::CalibrationSet calibration{};
    if (!calibration.load(calibrationFilePath, error)) {
        std::wcout << L"Warning: calibration file is missing or invalid at " << calibrationFilePath
                   << L"; continuing with identity (0-255) scaling. Reason: " << error << L'\n';
    } else {
        std::wcout << L"Loaded calibration file from " << calibrationFilePath << L"\n";
    }

    while (running) {
        if (!device.open(error)) {
            std::wcerr << L"Unable to open PhoenixRC HID device: " << error << L'\n';
            if (!waitForReconnect())
                break;
            continue;
        }

        activeDevice = &device;
        AuxCalibration auxCalibration{};
        bool controllerDisconnected = false;
        if (!skipAuxDetection && !calibrateAux(device, auxCalibration, controllerDisconnected, error)) {
            if (running && !controllerDisconnected)
                std::wcerr << L"AUX calibration stopped: " << error << L'\n';
            activeDevice = nullptr;
            device.close();
            if (running)
                waitForReconnect();
            continue;
        }

        std::wcout << L"Device opened. Streaming 8-byte reports; press Ctrl+C to stop.\n";
        phoenix::Report report{};
        while (running) {
            if (!device.read(report, error)) {
                if (running)
                    std::wcerr << L"Read stopped: " << error << L'\n';
                break;
            }
            if (report.controllerDisconnected()) {
                std::wcout << L"PhoenixRC controller disconnected; waiting for it to reconnect...\n";
                break;
            }
            const bool aux2Sample = skipAuxDetection || auxCalibration.aux2Next;
            const std::uint8_t aux2Value = aux2Sample ? report.misc() : auxCalibration.aux2;
            const std::uint8_t aux3Value = aux2Sample ? auxCalibration.aux3 : report.misc();
            const std::uint8_t x = calibration.x.normalize(report.x());
            const std::uint8_t y = calibration.y.normalize(report.y());
            const std::uint8_t rx = calibration.rx.normalize(report.rx());
            const std::uint8_t ry = calibration.ry.normalize(report.ry());
            const std::uint8_t rudder = calibration.rudder.normalize(report.rudder());
            const std::uint8_t throttle = calibration.throttle.normalize(report.throttle());
            const std::uint8_t aux2 = calibration.aux2.normalize(aux2Value);
            const std::uint8_t aux3 = calibration.aux3.normalize(aux3Value);
            if (!vjoy.update(x, y, rx, ry, rudder, throttle, aux2, aux3, report.buttonA() != 0, error)) {
                std::wcerr << L"vJoy update stopped: " << error << L'\n';
                running = false;
                break;
            }
            if (!skipAuxDetection) {
                if (auxCalibration.aux2Next)
                    auxCalibration.aux2 = report.misc();
                else
                    auxCalibration.aux3 = report.misc();
                auxCalibration.aux2Next = !auxCalibration.aux2Next;
            }
            if (debugOutput)
                printReport(report, aux2Sample, calibration);
        }

        activeDevice = nullptr;
        device.close();
        if (running && !waitForReconnect())
            break;
    }

    activeDevice = nullptr;
    device.close();
    return 0;
}
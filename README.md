# PhoenixRC USB adapter

This is a Windows user-mode adapter described in `plan.md`. It targets
the HID variant of the PhoenixRC adapter (VID `1781`, PID `0898`) and prints
the raw eight-byte input report in debug mode, while feeding the values to
vJoy.

## AI Warning

This has been 100% vibe-coded. I've tested it (with a Spektrum DX8 ca. 2013) and it works well, but consider yourself warned.


## Prerequisites
- Install [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/)
	and select the **Desktop development with C++** workload. This includes the
	MSVC C++ build tools and Windows SDK required by the project.
- Install [CMake](https://cmake.org/download/).
- Install vJoy from [this fork](https://github.com/BrunnerInnovation/vJoy).
	Other vJoy builds may also work if they provide the matching x64
	`vJoyInterface.dll` and `vJoyInterface.lib`.
- Configure and enable at least one vJoy device. Device 1 is used by default.
	Enable the X, Y, Z, X Rotation, Y Rotation, Rudder, Throttle, and Slider
	controls, plus button 1; the adapter uses Z for AUX 3 and Slider for AUX 2.


## Build

From a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\phoenix_usb_adapter.exe
```

For a release build, use the same CMake build directory with the Release
configuration:

```powershell
cmake --build build --config Release
.\build\Release\phoenix_usb_adapter.exe
```

The Release executable and `vJoyInterface.dll` are written to `build\Release`.

To list the available options:

```powershell
.\build\Release\phoenix_usb_adapter.exe --help
```

Raw report output is hidden by default. Add `--debug` to print the raw stream:

```powershell
.\build\Debug\phoenix_usb_adapter.exe --debug
```

Use `--vjoy-device` to select a different configured vJoy device; device 1 is
the default:

```powershell
.\build\Debug\phoenix_usb_adapter.exe --vjoy-device 2
```

Run calibration mode to sample your controller and save a `calibration.ini`
next to the executable:

```powershell
.\build\Debug\phoenix_usb_adapter.exe --calibrate
```

Override the calibration file location when needed:

```powershell
.\build\Debug\phoenix_usb_adapter.exe --calibration-file C:\temp\phoenix_usb_calibration.ini
```

The adapter automatically loads the calibration file on startup and falls back
to identity scaling if the file is missing, malformed, or incomplete.

vJoy must be installed and the selected device must be enabled before
starting the adapter. The build copies the bundled `vJoyInterface.dll` beside
the executable; it must match the installed vJoy driver architecture.

The adapter retries HID enumeration after unplug (USB or 3.5mm), USB reset, or resume-related
read failures. Press Ctrl+C to stop it; no HID output reports are sent.

At each connection, calibration samples the alternating AUX channel for about
one second. Keep AUX 2 steady and move AUX 3 through its full range when
prompted. The lower-variation channel is assigned to AUX 2 (vJoy Slider), and
the wider-range channel is assigned to AUX 3 (vJoy Z).


## Acknowledgements

Thanks to Marcus Folkesson, who presumably worked out how this thing worked and wrote a linux
driver for it. It made life easier, even though I was lazy and got AI to do all the work anyway.

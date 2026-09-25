# PhoenixRC USB adapter

## What is this?

### Boring AI description
This is a Windows user-mode adapter described in `plan.md`. It targets
the HID variant of the PhoenixRC adapter (VID `1781`, PID `0898`) and prints
the raw eight-byte input report in debug mode, while feeding the values to
vJoy.

### Boring human description
It's an adapter/driver for the Phoenix RC simulator cable. It really annoyed me that I couldn't use my controller as a standard joystick. My first idea was to build a hardware device that would replace the need to use the Phoenix branded one (most likely using a DE-10 nano; an exquisite degree of overkill that still tempts me to try). Then I thought a simpler approach might be to reverse engineer the protocol from the Phoenix cable. Then I saw there was a linux driver for it already (and I was not surprised at all) and I thought I could work out how to port it to Windows. Then I realised I have a copilot licence, so I may as well learn nothing and instead get a result in a fraction of the time. And here we are. Now I'm crashing helicopters in X-Plane and I can blame the fact that collective and anti-torque are on the same stick instead of blaming the keyboard/mouse combo.


## AI Warning

This has been 99.999% vibe-coded. I've tested it (with a Spektrum DX8 ca. 2013) and it works well, but consider yourself warned.


## Run
- Install vJoy from [this fork](https://github.com/BrunnerInnovation/vJoy) (go to the releases section and download/install the .exe). It may or may not work with versions of vJoy other than this one.
- Configure and enable at least one vJoy device. Device 1 is used by default.
	Enable the X, Y, Z, X Rotation, Y Rotation, Rudder, Throttle, and Slider
	controls, plus button 1; the adapter uses Z for AUX 3 and Slider for AUX 2.

Then either download the binary from [Releases](https://github.com/bivhitscar/phoenix_usb/releases), or follow on to
build from source.

Make sure vJoy is running, then run the adapter. 


## Build

- Install [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/)
	and select the **Desktop development with C++** workload. This includes the
	MSVC C++ build tools and Windows SDK required by the project.
- Install [CMake](https://cmake.org/download/).

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


## Usage
To list the available options:

```powershell
phoenix_usb_adapter.exe --help
```

Raw report output is hidden by default. Add `--debug` to print the raw stream:

```powershell
phoenix_usb_adapter.exe --debug
```

Use `--vjoy-device` to select a different configured vJoy device; device 1 is
the default:

```powershell
phoenix_usb_adapter.exe --vjoy-device 2
```
### Calibration
Run calibration mode to sample your controller and save a `calibration.ini`
next to the executable. This is required to normalise the outputs from the controller/USB cable so that vJoy will produce full-scale travel on each axis. The adapter automatically loads the calibration file on startup and falls back
to identity scaling if the file is missing, malformed, or incomplete. To run calibration:

```powershell
phoenix_usb_adapter.exe --calibrate
```

Override the calibration file location when needed:

```powershell
phoenix_usb_adapter.exe --calibration-file C:\temp\phoenix_usb_calibration.ini
```

### AUX channel detection
The cable multiplexes AUX 2 and AUX 3 on the same channel. So, to work out which is which, at each connection the application samples the channel for about
one second. Keep AUX 2 steady and move AUX 3 through its full range when
prompted. The lower-variation channel is assigned to AUX 2 (vJoy Slider), and
the wider-range channel is assigned to AUX 3 (vJoy Z). Note that AUX 2 and AUX 3 are what the human in this process has on its DX8 transmitter, your model may be different and require some experimentation to work out which knob to twist. To skip the AUX 3 detection step:

```powershell
phoenix_usb_adapter.exe --skip-aux-detection
```


## Acknowledgements

Thanks to Marcus Folkesson, who presumably worked out how this thing worked and wrote a linux
driver for it. It made life easier, even though I was lazy and got AI to do all the work anyway.

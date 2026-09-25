cmake_minimum_required(VERSION 3.20)

project(phoenix_usb_adapter VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "phoenix_usb_adapter currently requires Windows HID APIs")
endif()

add_executable(phoenix_usb_adapter
    src/axis_calibration.cpp
    src/hid_device.cpp
    src/main.cpp
    src/report.cpp
    src/vjoy_device.cpp
)

add_executable(axis_calibration_tests
    src/axis_calibration.cpp
    tests/axis_calibration_tests.cpp
)

target_compile_features(phoenix_usb_adapter PRIVATE cxx_std_17)
target_compile_features(axis_calibration_tests PRIVATE cxx_std_17)
target_include_directories(axis_calibration_tests PRIVATE src)
target_compile_definitions(phoenix_usb_adapter PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX PHOENIX_USB_VERSION=L\"${PROJECT_VERSION}\")
target_compile_definitions(axis_calibration_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(phoenix_usb_adapter PRIVATE src vjoy/SDK/inc)
target_link_directories(phoenix_usb_adapter PRIVATE vjoy/SDK/lib/x64)
target_link_libraries(phoenix_usb_adapter PRIVATE hid setupapi vJoyInterface)

add_custom_command(TARGET phoenix_usb_adapter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/vjoy/SDK/lib/x64/vJoyInterface.dll"
            "$<TARGET_FILE_DIR:phoenix_usb_adapter>/vJoyInterface.dll")

if(MSVC)
    target_compile_options(phoenix_usb_adapter PRIVATE /W4 /permissive-)
else()
    target_compile_options(phoenix_usb_adapter PRIVATE -Wall -Wextra -Wpedantic)
endif()
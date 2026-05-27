cmake_minimum_required(VERSION 3.20)
project(WindowsMouseFix VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# User-mode application
# ---------------------------------------------------------------------------

set(APP_SOURCES
    app/main.cpp
    app/MouseHook.cpp
    app/ScrollAnalyzer.cpp
    app/DragCurve.cpp
    app/DragAnimator.cpp
    app/SubPixelator.h          # header-only
    app/AccelerationCurve.h     # header-only
    app/ContactMapper.cpp
    app/DriverClient.cpp
    app/ScrollPipeline.cpp
    app/Settings.cpp
    app/TrayIcon.cpp
)

add_executable(WindowsMouseFix WIN32 ${APP_SOURCES})

target_include_directories(WindowsMouseFix PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_SOURCE_DIR}/app
)

target_link_libraries(WindowsMouseFix PRIVATE
    dwmapi
    shell32
    user32
    advapi32
    comctl32
)

target_compile_definitions(WindowsMouseFix PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00   # Windows 10+
)

# ---------------------------------------------------------------------------
# Unit tests (console executable, no WinMain)
# ---------------------------------------------------------------------------

add_executable(WmfTests
    tests/test_main.cpp
)

target_include_directories(WmfTests PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_SOURCE_DIR}/app
)

target_compile_definitions(WmfTests PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)

# ---------------------------------------------------------------------------
# Driver — NOTE: The UMDF2 driver (WmfVirtualPad) must be built separately
# using Visual Studio with the Windows Driver Kit (WDK) installed.
# CMake does not support WDK driver projects natively.
# See driver/WmfVirtualPad.inf and driver/WmfVirtualPad.c for the driver source.
# Build instructions are in README.md.
# ---------------------------------------------------------------------------

message(STATUS "")
message(STATUS "=== Windows Mouse Fix Build ===")
message(STATUS "App target:   WindowsMouseFix")
message(STATUS "Tests target: WmfTests")
message(STATUS "Driver:       Build separately with Visual Studio + WDK")
message(STATUS "              See driver/ directory and README.md")
message(STATUS "")

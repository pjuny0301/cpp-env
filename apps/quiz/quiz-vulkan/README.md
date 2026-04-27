# Vulkan Quiz App

Native C++ quiz app rewrite with a Vulkan-backed UI renderer.

## Scope

The first target is the quiz playing app. The existing Tauri/React editor remains the authoring tool and continues to export quiz JSON. This project consumes that data through a native domain model and renders UI through a retained scene pipeline.

## Architecture

```text
platform/app shell
  -> domain services
      -> app_snapshot
      <- app_action
  -> ui subsystem
      modifier_interface
      scene_layout_data_modifier
      scene_layout_data
      layout_placer
      ui_renderer
      vulkan_renderer
```

## Current Status

- Domain and scene contracts are being established first.
- The first runnable milestone is a desktop window that can render a simple scene.
- Android Vulkan surface support is planned after the desktop pipeline is stable.

## Build

Requires a modern C++ toolchain with C++20 support. The preferred local Windows path avoids user-profile and Korean paths by pinning GCC and Ninja to ASCII-only paths.

Use the checked-in script for local builds:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\run_windows_mingw_ascii.ps1
```

The script temporarily clears the current user's `cmd.exe` AutoRun while CMake/Ninja run, then restores it. That matters because CMake's Ninja generator invokes `cmd.exe /C` during link steps and a global AutoRun `cd` can move the linker out of its build directory.

Equivalent manual commands, if `cmd.exe` AutoRun is disabled:

```powershell
cmake --preset windows-mingw-ascii
cmake --build --preset windows-mingw-ascii-debug
ctest --test-dir C:\aa\build\out\quiz\quiz-vulkan\windows-mingw-ascii --output-on-failure
```

Expected local tool paths:

```text
C:\qtmingw1310_ascii\bin\g++.exe
C:\qtmingw1310_ascii\x86_64-w64-mingw32\bin\ld.exe
C:\dev\tools\ninja-1.13.2\ninja.exe
C:\Program Files\CMake\bin\cmake.exe
```

MSVC presets are also available:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
```

Current useful preset names:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug

cmake --preset windows-msvc-vcpkg
cmake --build --preset windows-msvc-vcpkg-debug
```

The legacy `C:\MinGW\bin\g++.exe` found during initial setup is GCC 6.3.0 and is too old for the domain/app layer because it lacks standard `<optional>`, `<string_view>`, and `<variant>`. It was only used to smoke-test the C++14-compatible scene/layout headers before the ASCII GCC 13 toolchain was selected.

## Smoke Checks Run

```powershell
C:\MinGW\bin\g++.exe -std=c++1z -Wall -Wextra -pedantic -Isrc tests\scene\scene_layout_data_tests.cpp -o C:\aa\build\out\quiz\quiz-vulkan\manual\scene_layout_data_tests.exe
C:\MinGW\bin\g++.exe -std=c++1z -Wall -Wextra -pedantic -Isrc tests\layout\layout_placer_tests.cpp -o C:\aa\build\out\quiz\quiz-vulkan\manual\layout_placer_tests.exe
```

Both scene/layout smoke tests compiled and ran under the available MinGW. Domain and full app compile checks need a newer compiler.

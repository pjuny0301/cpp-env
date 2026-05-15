# Test Plan

## Scope

This plan covers verification for the current scaffold. Agent F owned additions live under `tests/`, `tools/`, and this document.

CTest targets are added by a mix of explicit registrations and auto-discovered test directories, so this document does not freeze a global test count. After configuring the Windows build, use this command to get the current authoritative inventory:

```powershell
& "C:\Program Files\CMake\bin\ctest.exe" --test-dir "C:\aa\build\out\quiz\quiz-vulkan\windows-mingw-ascii" -N
```

Representative smoke and contract targets include:

- `quiz_vulkan_app_input_router_tests`
- `quiz_vulkan_domain_smoke_test`
- `quiz_vulkan_scene_layout_data_tests`
- `quiz_vulkan_layout_placer_tests`
- `quiz_vulkan_app_quiz_screens_tests`
- `quiz_vulkan_renderer_tests`
- `quiz_vulkan_interface_contract_compile_tests`

The preferred local compiler is the ASCII-path GCC 13.1.0 toolchain at `C:\qtmingw1310_ascii`. The CMake targets require C++23. The legacy `C:\MinGW\bin\g++.exe` is GCC 6.3.0 and is useful only for old scene/layout smoke checks.

The local Windows user has a `cmd.exe` AutoRun entry. Use `tools/run_windows_mingw_ascii.ps1` so CMake/Ninja link steps run with AutoRun temporarily cleared and restored afterward.

## Verification Matrix

| Area | Test | Current compiler path | Expected result |
| --- | --- | --- | --- |
| Full app scaffold | `quiz_vulkan.exe` | `C:\qtmingw1310_ascii\bin\g++.exe` | Configure and build |
| App input routing | `quiz_vulkan_app_input_router_tests` | CTest via `windows-mingw-ascii` | Pass |
| Domain behavior | `quiz_vulkan_domain_smoke_test` | CTest via `windows-mingw-ascii` | Pass |
| Scene data and patching | `quiz_vulkan_scene_layout_data_tests` | CTest via `windows-mingw-ascii` | Pass |
| Layout placement | `quiz_vulkan_layout_placer_tests` | CTest via `windows-mingw-ascii` | Pass |
| Quiz screen patching | `quiz_vulkan_app_quiz_screens_tests` | CTest via `windows-mingw-ascii` | Pass |
| UI to Vulkan command bridge | `quiz_vulkan_renderer_tests` | CTest via `windows-mingw-ascii` | Pass |
| Aggregate interface lock | `quiz_vulkan_interface_contract_compile_tests` | CMake target via `windows-mingw-ascii` | Build succeeds |

## Preferred Windows Build

From Windows PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\run_windows_mingw_ascii.ps1
```

Use `ctest -N` from the configured build directory whenever a handoff needs the current suite count. Record the run-specific count in that handoff rather than in this static plan.

## MinGW Scene/Layout Smoke Tests

From Windows PowerShell:

```powershell
.\tools\run_mingw_scene_layout_smoke.ps1
```

Equivalent manual PowerShell commands:

```powershell
New-Item -ItemType Directory -Force C:\aa\build\out\quiz\quiz-vulkan\manual | Out-Null
C:\MinGW\bin\g++.exe -std=c++1z -Wall -Wextra -pedantic -Isrc tests\scene\scene_layout_data_tests.cpp -o C:\aa\build\out\quiz\quiz-vulkan\manual\scene_layout_data_tests.exe
C:\aa\build\out\quiz\quiz-vulkan\manual\scene_layout_data_tests.exe
C:\MinGW\bin\g++.exe -std=c++1z -Wall -Wextra -pedantic -Isrc tests\layout\layout_placer_tests.cpp -o C:\aa\build\out\quiz\quiz-vulkan\manual\layout_placer_tests.exe
C:\aa\build\out\quiz\quiz-vulkan\manual\layout_placer_tests.exe
```

From WSL or Git Bash:

```sh
./tools/run_mingw_scene_layout_smoke.sh
```

Equivalent manual WSL command using the installed Windows MinGW:

```sh
mkdir -p /mnt/c/aa/build/out/quiz/quiz-vulkan/manual
/mnt/c/MinGW/bin/g++.exe -std=c++1z -Wall -Wextra -pedantic -Isrc tests/scene/scene_layout_data_tests.cpp -o /mnt/c/aa/build/out/quiz/quiz-vulkan/manual/scene_layout_data_tests.exe
/mnt/c/aa/build/out/quiz/quiz-vulkan/manual/scene_layout_data_tests.exe
/mnt/c/MinGW/bin/g++.exe -std=c++1z -Wall -Wextra -pedantic -Isrc tests/layout/layout_placer_tests.cpp -o /mnt/c/aa/build/out/quiz/quiz-vulkan/manual/layout_placer_tests.exe
/mnt/c/aa/build/out/quiz/quiz-vulkan/manual/layout_placer_tests.exe
```

The helper scripts use a temporary directory under `C:\aa\build\out\quiz\quiz-vulkan\manual` and delete it after a successful run unless explicitly told to keep binaries.

## Modern Compiler Checks

Use these checks when a modern C++23 compiler is available. The local ASCII GCC 13 toolchain satisfies this for the CMake targets.

Domain smoke test, representative direct compile for legacy standalone checks:

```sh
g++ -std=c++23 -Wall -Wextra -pedantic -Isrc \
  tests/domain/domain_smoke_test.cpp \
  src/core/domain/app_action.cpp \
  src/core/domain/app_snapshot.cpp \
  src/core/domain/learning_state.cpp \
  src/core/domain/quiz_model.cpp \
  src/core/domain/quiz_session.cpp \
  -o /mnt/c/aa/build/out/quiz/quiz-vulkan/manual/domain_smoke_test
/mnt/c/aa/build/out/quiz/quiz-vulkan/manual/domain_smoke_test
```

Full app, preferred preset path on Windows with ASCII MinGW:

```powershell
cmake --preset windows-mingw-ascii
cmake --build --preset windows-mingw-ascii-debug
ctest --test-dir C:\aa\build\out\quiz\quiz-vulkan\windows-mingw-ascii -N
ctest --test-dir C:\aa\build\out\quiz\quiz-vulkan\windows-mingw-ascii --output-on-failure
```

MSVC and vcpkg presets remain available:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug

cmake --preset windows-msvc-vcpkg
cmake --build --preset windows-msvc-vcpkg-debug
```

## Metadata

`tests/test_manifest.json` records representative smoke commands, supported compiler profiles, and manual commands. It is not a full CTest inventory; use `ctest -N` from the configured build directory for the current target list.

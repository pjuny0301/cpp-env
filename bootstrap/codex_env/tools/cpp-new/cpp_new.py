#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ctypes
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
from pathlib import Path
from typing import Any

VERSION = "0.2.0"
SUPPORTED_KINDS = ("desktop", "game")


def is_windows() -> bool:
    return os.name == "nt"


def print_output(payload: dict[str, Any], as_json: bool) -> None:
    if as_json:
        print(json.dumps(payload, indent=2, ensure_ascii=False))
        return

    status = payload.get("status", "ok")
    print(f"status: {status}")
    for key, value in payload.items():
        if key == "status":
            continue
        if isinstance(value, (dict, list)):
            print(f"{key}: {json.dumps(value, indent=2, ensure_ascii=False)}")
        else:
            print(f"{key}: {value}")


def safe_identifier(name: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").lower()
    if not cleaned:
        cleaned = "sample_app"
    if cleaned[0].isdigit():
        cleaned = f"app_{cleaned}"
    return cleaned


def pascal_case(value: str) -> str:
    parts = re.split(r"[^A-Za-z0-9]+", value)
    merged = "".join(part.capitalize() for part in parts if part)
    return merged or "SampleApp"


def default_dev_root() -> Path:
    if "DEV_ROOT" in os.environ and os.environ["DEV_ROOT"]:
        return Path(os.environ["DEV_ROOT"])
    if is_windows():
        return Path("C:/dev")
    return Path.home() / "dev"


def default_vcpkg_root() -> Path:
    if "VCPKG_ROOT" in os.environ and os.environ["VCPKG_ROOT"]:
        return Path(os.environ["VCPKG_ROOT"])
    return default_dev_root() / "tooling" / "vcpkg"


def default_plan_template_root() -> Path:
    if "PLAN_TEMPLATE_ROOT" in os.environ and os.environ["PLAN_TEMPLATE_ROOT"]:
        return Path(os.environ["PLAN_TEMPLATE_ROOT"])
    return default_dev_root() / "templates"


def run_command(command: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=str(cwd) if cwd else None,
        capture_output=True,
        text=True,
        check=False,
    )


def detect_vcpkg_baseline(vcpkg_root: Path) -> str:
    if not (vcpkg_root / ".git").exists():
        raise SystemExit(
            f"Unable to determine vcpkg baseline because {vcpkg_root} is not a git checkout."
        )

    result = run_command(["git", "-C", str(vcpkg_root), "rev-parse", "HEAD"])
    if result.returncode != 0:
        raise SystemExit(result.stderr.strip() or "Failed to read vcpkg baseline.")
    return result.stdout.strip()


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content.rstrip() + "\n", encoding="utf-8")


def write_json(path: Path, payload: dict[str, Any]) -> None:
    write_text(path, json.dumps(payload, indent=2, ensure_ascii=False))


def set_user_env_var(name: str, value: str) -> None:
    os.environ[name] = value
    if not is_windows():
        return

    import winreg

    with winreg.OpenKey(
        winreg.HKEY_CURRENT_USER, "Environment", 0, winreg.KEY_SET_VALUE
    ) as key:
        winreg.SetValueEx(key, name, 0, winreg.REG_EXPAND_SZ, value)

    try:
        HWND_BROADCAST = 0xFFFF
        WM_SETTINGCHANGE = 0x001A
        SMTO_ABORTIFHUNG = 0x0002
        ctypes.windll.user32.SendMessageTimeoutW(
            HWND_BROADCAST,
            WM_SETTINGCHANGE,
            0,
            "Environment",
            SMTO_ABORTIFHUNG,
            5000,
            None,
        )
    except Exception:
        pass


def machine_layout_files(dev_root: Path) -> dict[Path, str]:
    theme = {
        "document": {
            "font_name": "Malgun Gothic" if is_windows() else "DejaVu Sans",
            "accent_hex": "1F4E79",
            "secondary_hex": "16324F",
            "muted_hex": "617B94",
        },
        "image_style": {
            "cover": "cinematic technical editorial art, polished presentation graphic, clean whitespace, strong hierarchy",
            "section": "editorial systems illustration, modern technical concept art, crisp shapes, layered gradients",
            "inline": "clean infographic-style concept art, grounded technical details, restrained composition",
        },
    }

    return {
        dev_root / "templates" / "cpp" / "README.md": textwrap.dedent(
            """\
            # C++ Scaffold Templates

            This directory stores machine-wide defaults for `cpp-new` and `plan-doc`.

            - `cpp-new` generates projects under `C:/dev/projects` by default.
            - `plan-doc` looks here for theme information when composing plan documents.
            - Keep this tree ASCII-only so local tools remain stable for any AI agent.
            """
        ),
        dev_root / "templates" / "plan-doc" / "theme.json": json.dumps(
            theme, indent=2, ensure_ascii=False
        ),
    }


def ensure_machine_layout() -> dict[str, Any]:
    dev_root = default_dev_root()
    directories = [
        dev_root / "projects",
        dev_root / "templates",
        dev_root / "templates" / "cpp",
        dev_root / "templates" / "plan-doc",
        dev_root / "tooling",
        dev_root / "cache" / "vcpkg",
        dev_root / "models",
        dev_root / "temp",
    ]
    for directory in directories:
        directory.mkdir(parents=True, exist_ok=True)

    env_updates = {
        "DEV_ROOT": str(dev_root),
        "VCPKG_ROOT": str(dev_root / "tooling" / "vcpkg"),
        "VCPKG_DEFAULT_BINARY_CACHE": str(dev_root / "cache" / "vcpkg"),
        "PLAN_TEMPLATE_ROOT": str(dev_root / "templates"),
        "TEMP": str(dev_root / "temp"),
        "TMP": str(dev_root / "temp"),
    }
    for key, value in env_updates.items():
        set_user_env_var(key, value)

    for path, content in machine_layout_files(dev_root).items():
        write_text(path, content)

    return {
        "status": "ok",
        "dev_root": str(dev_root),
        "env": env_updates,
        "created_directories": [str(path) for path in directories],
    }


def desktop_readme(display_name: str) -> str:
    return textwrap.dedent(
        f"""\
        # {display_name}

        Windows-first, Linux-portable desktop application scaffold generated by `cpp-new`.

        ## Build

        ### Windows / Visual Studio 2022

        ```powershell
        cmake --preset windows-vs-debug
        cmake --build --preset build-windows-vs-debug
        ```

        ### Linux

        ```bash
        cmake --preset linux-debug
        cmake --build --preset build-linux-debug
        ```

        ## Layout

        - `src/core` holds platform-neutral logic.
        - `src/platform/win32` and `src/platform/linux` hold OS adapters.
        - `plan/plan.md` is the planning source of truth.
        - `plan-doc <project-root>` generates the polished `plan_summary.docx`.
        - Start plan edits from `plan/plan/bundle_manifest.json` and `plan/plan/prompt_overrides.json` before opening the DOCX.
        - Write shared sub-agent instructions once in `agent/command_brief.json`, then have local agents read that file first.
        """
    )


def game_readme(display_name: str) -> str:
    return textwrap.dedent(
        f"""\
        # {display_name}

        Windows-first, Linux-portable game scaffold generated by `cpp-new`.

        ## Renderer Presets

        - Default debug presets use `VULKAN`.
        - Switch renderer with `-DRENDERER=VULKAN|OPENGL|D3D12` or use a renderer-specific preset.
        - `D3D12` is Windows-only. `VULKAN` and `OPENGL` are shared across Windows and Linux.

        ## Build

        ### Windows / Visual Studio 2022

        ```powershell
        cmake --preset windows-vs-debug
        cmake --build --preset build-windows-vs-debug
        ```

        ### Linux

        ```bash
        cmake --preset linux-debug
        cmake --build --preset build-linux-debug
        ```

        ## Layout

        - `src/core` is platform-neutral gameplay and renderer selection logic.
        - `src/platform/win32` and `src/platform/linux` isolate native hooks.
        - `plan/plan.md` is the planning source of truth.
        - `plan-doc <project-root>` generates the polished `plan_summary.docx`.
        - Start plan edits from `plan/plan/bundle_manifest.json` and `plan/plan/prompt_overrides.json` before opening the DOCX.
        - Write shared sub-agent instructions once in `agent/command_brief.json`, then have local agents read that file first.
        """
    )


def agents_md(namespace: str) -> str:
    return textwrap.dedent(
        f"""\
        # Project Agent Rules

        - Treat `CMakeLists.txt` and `CMakePresets.json` as the source of truth. Generated Visual Studio files are disposable.
        - Keep cross-platform code in `src/core` and `include/{namespace}/core`.
        - Keep OS-specific code in `src/platform/win32` and `src/platform/linux`.
        - Do not include `windows.h` or Win32-only symbols from `src/core` or `include/{namespace}/core`.
        - Preserve `vcpkg.json`, `vcpkg-configuration.json`, and preset triplets when updating dependencies.
        - Use `plan/plan.md` as the human-editable planning source and `plan-doc <project-root>` to generate the visual plan document.
        - For plan bundle updates, read `plan/plan/bundle_manifest.json` and `plan/plan/prompt_overrides.json` before the generated DOCX to keep token usage down.
        - For local multi-agent work, write the shared task brief once in `agent/command_brief.json` and have sub-agents read it before acting.
        """
    )


def stack_md(display_name: str, namespace: str, kind: str) -> str:
    return textwrap.dedent(
        f"""\
        # Stack Overview

        ## Identity

        - Project: `{display_name}`
        - Namespace root: `{namespace}`
        - Kind: `{kind}`

        ## Build System

        - CMake Presets drive every workflow.
        - `windows-vs-*` presets target Visual Studio 2022.
        - `linux-*` presets target Ninja on Linux.
        - Dependencies are managed with vcpkg manifest mode.

        ## Portability Contract

        - `src/core` stays OS-agnostic.
        - `src/platform/win32` and `src/platform/linux` provide adapters.
        - Runtime deployment uses `TARGET_RUNTIME_DLLS` on Windows and install/RPATH rules on Linux.
        - AI agents should update `plan/plan.md` before making large cross-cutting changes.
        """
    )


def agent_command_brief(display_name: str, kind: str) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "project": display_name,
        "kind": kind,
        "read_first": [
            "agent/command_brief.json",
            "AGENTS.md",
            "docs/stack.md",
        ],
        "working_set": [
            "AGENTS.md",
            "README.md",
            "docs/",
            "agent/",
            "plan/",
            "src/",
            "include/",
            "cmake/",
        ],
        "ignore_by_default": [
            "out/",
            "build/",
            ".vs/",
            ".cache/",
            "vcpkg_installed/",
        ],
        "plan_bundle_read_order": [
            "plan/plan/bundle_manifest.json",
            "plan/plan/prompt_overrides.json",
            "plan/plan/render_spec.json",
            "plan/plan/prompts.json",
            "plan/plan/plan_summary.docx",
        ],
        "delegate_when": [
            "bounded read-only file inspection",
            "simple issue detection",
            "installation wait and polling work",
            "long-running log monitoring",
            "lightweight document verification",
        ],
        "direct_only_when": [
            "safety-critical or high-stakes work",
            "destructive operations",
            "credential handling",
            "cross-platform architecture decisions",
        ],
        "token_budget_rules": [
            "Prefer compact JSON metadata before large rendered artifacts.",
            "Write shared task instructions once in this file before spawning additional local agents.",
        ],
    }


def plan_md(display_name: str, kind: str) -> str:
    return textwrap.dedent(
        f"""\
        # {display_name} Plan

        ## Summary
        - Describe the product goal, target users, and why the project exists.
        - Explain why the current `{kind}` scaffold is the right starting point.

        ## Key Changes
        - List the main subsystems to build.
        - Capture any public interfaces, save formats, protocols, or editor tooling.

        ## Architecture
        - Describe the role of `src/core`, `src/platform/win32`, and `src/platform/linux`.
        - Note how dependencies and runtime assets should be shared.

        ## Test Plan
        - Define smoke tests, platform checks, and packaging checks.
        - Add Linux portability validation when Windows-only work is introduced.

        ## Assumptions
        - Record platform constraints, renderer choices, third-party service assumptions, and rollout notes.
        """
    )


def editorconfig() -> str:
    return textwrap.dedent(
        """\
        root = true

        [*]
        charset = utf-8
        end_of_line = lf
        insert_final_newline = true
        indent_style = space
        indent_size = 4
        trim_trailing_whitespace = true

        [*.md]
        trim_trailing_whitespace = false
        """
    )


def gitignore() -> str:
    return textwrap.dedent(
        """\
        .vs/
        out/
        build/
        .cache/
        *.user
        *.VC.db
        *.VC.opendb
        *.suo
        """
    )


def copy_runtime_deps_cmake() -> str:
    return textwrap.dedent(
        """\
        function(copy_target_runtime_dlls target_name)
          if(WIN32)
            add_custom_command(
              TARGET ${target_name}
              POST_BUILD
              COMMAND ${CMAKE_COMMAND} -E copy_if_different
                      $<TARGET_RUNTIME_DLLS:${target_name}>
                      $<TARGET_FILE_DIR:${target_name}>
              COMMAND_EXPAND_LISTS
            )
          endif()
        endfunction()
        """
    )


def portability_check_cmake() -> str:
    return textwrap.dedent(
        """\
        function(add_core_portability_check target_name root_dir)
          set(check_target "check-${target_name}-portability")
          add_custom_target(
            ${check_target}
            COMMAND ${CMAKE_COMMAND}
                    -DROOT_DIR=${root_dir}
                    -P ${root_dir}/cmake/RunPortabilityCheck.cmake
            COMMENT "Checking src/core and include/*/core for platform leakage"
          )
          add_dependencies(${target_name} ${check_target})
        endfunction()
        """
    )


def run_portability_check_cmake() -> str:
    return textwrap.dedent(
        """\
        if(NOT DEFINED ROOT_DIR)
          message(FATAL_ERROR "ROOT_DIR is required.")
        endif()

        file(GLOB_RECURSE CORE_SOURCES
          LIST_DIRECTORIES FALSE
          "${ROOT_DIR}/src/core/*.*"
          "${ROOT_DIR}/include/*/core/*.*"
        )

        set(PATTERNS
          "windows\\.h"
          "\\<HWND\\>"
          "\\<HANDLE\\>"
          "\\<DWORD\\>"
          "CreateFileW\\("
          "ReadDirectoryChangesW\\("
          "MessageBoxW\\("
        )

        set(VIOLATIONS "")
        foreach(FILE_PATH IN LISTS CORE_SOURCES)
          file(READ "${FILE_PATH}" FILE_CONTENT)
          foreach(PATTERN IN LISTS PATTERNS)
            string(REGEX MATCH "${PATTERN}" HAS_MATCH "${FILE_CONTENT}")
            if(HAS_MATCH)
              list(APPEND VIOLATIONS "${FILE_PATH}: ${PATTERN}")
              break()
            endif()
          endforeach()
        endforeach()

        if(VIOLATIONS)
          string(REPLACE ";" "\\n" VIOLATION_TEXT "${VIOLATIONS}")
          message(FATAL_ERROR "Platform leakage detected in core code:\\n${VIOLATION_TEXT}")
        endif()
        """
    )


def runtime_platform_header(namespace: str) -> str:
    return textwrap.dedent(
        f"""\
        #pragma once

        #include <filesystem>
        #include <string>
        #include <string_view>

        namespace {namespace}::core::platform {{

        struct RuntimePlatformSnapshot {{
            std::string platform_name;
            std::filesystem::path user_data_root;
            std::string portability_note;
        }};

        RuntimePlatformSnapshot detect_runtime_platform(std::string_view app_name);

        }}  // namespace {namespace}::core::platform
        """
    )


def desktop_application_header(namespace: str) -> str:
    return textwrap.dedent(
        f"""\
        #pragma once

        #include <string>

        namespace {namespace}::core {{

        class Application {{
          public:
            explicit Application(std::string app_name);
            int run() const;

          private:
            std::string app_name_;
        }};

        }}  // namespace {namespace}::core
        """
    )


def desktop_application_cpp(namespace: str) -> str:
    return textwrap.dedent(
        f"""\
        #include <{namespace}/core/application.hpp>
        #include <{namespace}/core/platform/runtime_platform.hpp>

        #include <spdlog/spdlog.h>

        #include <utility>

        namespace {namespace}::core {{

        Application::Application(std::string app_name) : app_name_(std::move(app_name)) {{}}

        int Application::run() const {{
            const auto snapshot = platform::detect_runtime_platform(app_name_);
            spdlog::info("Application: {{}}", app_name_);
            spdlog::info("Host platform: {{}}", snapshot.platform_name);
            spdlog::info("User data root: {{}}", snapshot.user_data_root.string());
            spdlog::info("Portability note: {{}}", snapshot.portability_note);
            return 0;
        }}

        }}  // namespace {namespace}::core
        """
    )


def desktop_main_cpp(namespace: str, display_name: str) -> str:
    return textwrap.dedent(
        f"""\
        #include <{namespace}/core/application.hpp>

        #include <exception>

        #include <spdlog/spdlog.h>

        int main() {{
            try {{
                {namespace}::core::Application application("{display_name}");
                return application.run();
            }} catch (const std::exception& error) {{
                spdlog::error("Fatal error: {{}}", error.what());
                return 1;
            }}
        }}
        """
    )


def windows_runtime_cpp(namespace: str) -> str:
    return textwrap.dedent(
        f"""\
        #include <{namespace}/core/platform/runtime_platform.hpp>

        #include <cstdlib>
        #include <filesystem>

        namespace {namespace}::core::platform {{

        namespace {{

        std::filesystem::path env_path_or(const char* key, std::filesystem::path fallback) {{
            if (const char* value = std::getenv(key); value != nullptr && *value != '\\0') {{
                return std::filesystem::path(value);
            }}
            return fallback;
        }}

        }}  // namespace

        RuntimePlatformSnapshot detect_runtime_platform(std::string_view app_name) {{
            const auto base = env_path_or(
                "LOCALAPPDATA",
                env_path_or("APPDATA", std::filesystem::current_path() / ".local-app-data"));
            return RuntimePlatformSnapshot{{
                "windows",
                base / std::filesystem::path(app_name),
                "Keep Win32-only types inside src/platform/win32.",
            }};
        }}

        }}  // namespace {namespace}::core::platform
        """
    )


def linux_runtime_cpp(namespace: str) -> str:
    return textwrap.dedent(
        f"""\
        #include <{namespace}/core/platform/runtime_platform.hpp>

        #include <cstdlib>
        #include <filesystem>

        namespace {namespace}::core::platform {{

        namespace {{

        std::filesystem::path home_or_tmp() {{
            if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\\0') {{
                return std::filesystem::path(home);
            }}
            return std::filesystem::temp_directory_path();
        }}

        }}  // namespace

        RuntimePlatformSnapshot detect_runtime_platform(std::string_view app_name) {{
            std::filesystem::path base;
            if (const char* value = std::getenv("XDG_DATA_HOME"); value != nullptr && *value != '\\0') {{
                base = std::filesystem::path(value);
            }} else {{
                base = home_or_tmp() / ".local" / "share";
            }}

            return RuntimePlatformSnapshot{{
                "linux",
                base / std::filesystem::path(app_name),
                "Keep Linux-specific APIs inside src/platform/linux.",
            }};
        }}

        }}  // namespace {namespace}::core::platform
        """
    )


def desktop_tests_cpp(namespace: str, display_name: str) -> str:
    return textwrap.dedent(
        f"""\
        #include <{namespace}/core/application.hpp>

        int main() {{
            {namespace}::core::Application application("{display_name}");
            return application.run() == 0 ? 0 : 1;
        }}
        """
    )


def desktop_cmakelists(project_id: str) -> str:
    return textwrap.dedent(
        f"""\
        cmake_minimum_required(VERSION 3.27)
        project({project_id} VERSION 0.1.0 LANGUAGES CXX)

        set(CMAKE_CXX_STANDARD 23)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
        set(CMAKE_CXX_EXTENSIONS OFF)
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

        if(MSVC)
          add_compile_options(/W4 /permissive- /EHsc /utf-8)
        else()
          add_compile_options(-Wall -Wextra -Wpedantic)
        endif()

        include(CTest)
        include(cmake/CopyRuntimeDeps.cmake)
        include(cmake/PortabilityCheck.cmake)

        find_package(fmt CONFIG REQUIRED)
        find_package(spdlog CONFIG REQUIRED)
        find_package(nlohmann_json CONFIG REQUIRED)

        set(PROJECT_CORE_SOURCES
          src/core/application.cpp
        )

        if(WIN32)
          list(APPEND PROJECT_CORE_SOURCES src/platform/win32/runtime_platform.cpp)
        elseif(UNIX AND NOT APPLE)
          list(APPEND PROJECT_CORE_SOURCES src/platform/linux/runtime_platform.cpp)
        else()
          message(FATAL_ERROR "This scaffold currently targets Windows and Linux.")
        endif()

        add_library({project_id}_core ${{PROJECT_CORE_SOURCES}})
        target_include_directories({project_id}_core PUBLIC include)
        target_link_libraries(
          {project_id}_core
          PUBLIC
            fmt::fmt
            spdlog::spdlog
            nlohmann_json::nlohmann_json
        )
        target_compile_features({project_id}_core PUBLIC cxx_std_23)
        add_core_portability_check({project_id}_core ${{CMAKE_CURRENT_SOURCE_DIR}})

        add_executable({project_id}_app src/app/main.cpp)
        target_link_libraries({project_id}_app PRIVATE {project_id}_core)
        copy_target_runtime_dlls({project_id}_app)

        if(UNIX AND NOT APPLE)
          set_target_properties(
            {project_id}_app
            PROPERTIES
              BUILD_RPATH "$ORIGIN/../lib"
              INSTALL_RPATH "$ORIGIN/../lib"
          )
        endif()

        install(TARGETS {project_id}_app RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

        if(BUILD_TESTING)
          add_executable({project_id}_tests tests/smoke_tests.cpp)
          target_link_libraries({project_id}_tests PRIVATE {project_id}_core)
          add_test(NAME smoke COMMAND {project_id}_tests)
        endif()
        """
    )


def game_renderer_header(namespace: str) -> str:
    return textwrap.dedent(
        f"""\
        #pragma once

        #include <string_view>

        namespace {namespace}::game::renderers {{

        std::string_view renderer_name();
        bool renderer_is_windows_only();

        }}  // namespace {namespace}::game::renderers
        """
    )


def game_application_header(namespace: str) -> str:
    return textwrap.dedent(
        f"""\
        #pragma once

        namespace {namespace}::game {{

        int run();

        }}  // namespace {namespace}::game
        """
    )


def game_renderer_cpp(namespace: str) -> str:
    return textwrap.dedent(
        f"""\
        #include <{namespace}/game/renderers/renderer_profile.hpp>

        namespace {namespace}::game::renderers {{

        std::string_view renderer_name() {{
        #if defined(PROJECT_RENDERER_VULKAN)
            return "VULKAN";
        #elif defined(PROJECT_RENDERER_OPENGL)
            return "OPENGL";
        #elif defined(PROJECT_RENDERER_D3D12)
            return "D3D12";
        #else
            return "UNKNOWN";
        #endif
        }}

        bool renderer_is_windows_only() {{
        #if defined(PROJECT_RENDERER_D3D12)
            return true;
        #else
            return false;
        #endif
        }}

        }}  // namespace {namespace}::game::renderers
        """
    )


def game_application_cpp(namespace: str, display_name: str) -> str:
    return textwrap.dedent(
        f"""\
        #include <{namespace}/core/platform/runtime_platform.hpp>
        #include <{namespace}/game/game_app.hpp>
        #include <{namespace}/game/renderers/renderer_profile.hpp>

        #include <SDL3/SDL.h>
        #include <glm/vec3.hpp>
        #include <spdlog/spdlog.h>

        #include <stdexcept>

        namespace {namespace}::game {{

        int run() {{
            if (!SDL_Init(SDL_INIT_VIDEO)) {{
                throw std::runtime_error(SDL_GetError());
            }}

            const auto snapshot = core::platform::detect_runtime_platform("{display_name}");
            const glm::vec3 accent{{0.18f, 0.46f, 0.74f}};
            spdlog::info("Platform: {{}}", snapshot.platform_name);
            spdlog::info("Renderer profile: {{}}", renderers::renderer_name());
            spdlog::info("User data root: {{}}", snapshot.user_data_root.string());
            spdlog::info("Accent vector: {{:.2f}}, {{:.2f}}, {{:.2f}}", accent.x, accent.y, accent.z);

            SDL_Window* window = SDL_CreateWindow(
                "{display_name}",
                1280,
                720,
                SDL_WINDOW_RESIZABLE
            );
            if (window == nullptr) {{
                SDL_Quit();
                throw std::runtime_error(SDL_GetError());
            }}

            bool done = false;
            while (!done) {{
                SDL_Event event;
                while (SDL_PollEvent(&event)) {{
                    if (event.type == SDL_EVENT_QUIT) {{
                        done = true;
                    }}
                }}
                SDL_Delay(16);
            }}

            SDL_DestroyWindow(window);
            SDL_Quit();
            return 0;
        }}

        }}  // namespace {namespace}::game
        """
    )


def game_main_cpp(namespace: str) -> str:
    return textwrap.dedent(
        f"""\
        #include <{namespace}/game/game_app.hpp>

        #include <exception>

        #include <spdlog/spdlog.h>

        int main() {{
            try {{
                return {namespace}::game::run();
            }} catch (const std::exception& error) {{
                spdlog::error("Fatal error: {{}}", error.what());
                return 1;
            }}
        }}
        """
    )


def game_tests_cpp(namespace: str) -> str:
    return textwrap.dedent(
        f"""\
        #include <{namespace}/game/renderers/renderer_profile.hpp>

        int main() {{
            return {namespace}::game::renderers::renderer_name().empty() ? 1 : 0;
        }}
        """
    )


def game_native_stub(namespace: str, platform_name: str) -> str:
    return textwrap.dedent(
        f"""\
        #include <{namespace}/core/platform/runtime_platform.hpp>

        namespace {namespace}::platform::{platform_name} {{

        void native_placeholder() {{}}

        }}  // namespace {namespace}::platform::{platform_name}
        """
    )


def game_cmakelists(project_id: str) -> str:
    return textwrap.dedent(
        f"""\
        cmake_minimum_required(VERSION 3.27)
        project({project_id} VERSION 0.1.0 LANGUAGES CXX)

        set(CMAKE_CXX_STANDARD 23)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
        set(CMAKE_CXX_EXTENSIONS OFF)
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
        set(RENDERER "VULKAN" CACHE STRING "Renderer profile")
        set_property(CACHE RENDERER PROPERTY STRINGS VULKAN OPENGL D3D12)
        string(TOUPPER "${{RENDERER}}" RENDERER)

        if(MSVC)
          add_compile_options(/W4 /permissive- /EHsc /utf-8)
        else()
          add_compile_options(-Wall -Wextra -Wpedantic)
        endif()

        include(CTest)
        include(cmake/CopyRuntimeDeps.cmake)
        include(cmake/PortabilityCheck.cmake)

        find_package(fmt CONFIG REQUIRED)
        find_package(spdlog CONFIG REQUIRED)
        find_package(nlohmann_json CONFIG REQUIRED)
        find_package(SDL3 CONFIG REQUIRED)
        find_package(glm CONFIG REQUIRED)

        set(PROJECT_CORE_SOURCES
          src/core/renderers/renderer_profile.cpp
          src/core/application.cpp
        )

        if(WIN32)
          list(APPEND PROJECT_CORE_SOURCES
            src/platform/win32/runtime_platform.cpp
            src/platform/win32/native_window_stub.cpp
          )
        elseif(UNIX AND NOT APPLE)
          list(APPEND PROJECT_CORE_SOURCES
            src/platform/linux/runtime_platform.cpp
            src/platform/linux/native_window_stub.cpp
          )
        else()
          message(FATAL_ERROR "This scaffold currently targets Windows and Linux.")
        endif()

        add_library({project_id}_core ${{PROJECT_CORE_SOURCES}})
        target_include_directories({project_id}_core PUBLIC include)
        target_link_libraries(
          {project_id}_core
          PUBLIC
            fmt::fmt
            glm::glm
            nlohmann_json::nlohmann_json
            SDL3::SDL3
            spdlog::spdlog
        )
        target_compile_features({project_id}_core PUBLIC cxx_std_23)

        if(RENDERER STREQUAL "VULKAN")
          find_package(Vulkan REQUIRED)
          target_link_libraries({project_id}_core PUBLIC Vulkan::Vulkan)
          target_compile_definitions({project_id}_core PUBLIC PROJECT_RENDERER_VULKAN=1)
        elseif(RENDERER STREQUAL "OPENGL")
          find_package(OpenGL REQUIRED)
          target_link_libraries({project_id}_core PUBLIC OpenGL::GL)
          target_compile_definitions({project_id}_core PUBLIC PROJECT_RENDERER_OPENGL=1)
        elseif(RENDERER STREQUAL "D3D12")
          if(NOT WIN32)
            message(FATAL_ERROR "D3D12 is only supported on Windows.")
          endif()
          target_link_libraries({project_id}_core PUBLIC d3d12 dxgi dxguid)
          target_compile_definitions({project_id}_core PUBLIC PROJECT_RENDERER_D3D12=1)
        else()
          message(FATAL_ERROR "Unknown renderer '${{RENDERER}}'.")
        endif()

        add_core_portability_check({project_id}_core ${{CMAKE_CURRENT_SOURCE_DIR}})

        add_executable({project_id}_app src/app/main.cpp)
        target_link_libraries({project_id}_app PRIVATE {project_id}_core)
        copy_target_runtime_dlls({project_id}_app)

        if(UNIX AND NOT APPLE)
          set_target_properties(
            {project_id}_app
            PROPERTIES
              BUILD_RPATH "$ORIGIN/../lib"
              INSTALL_RPATH "$ORIGIN/../lib"
          )
        endif()

        install(TARGETS {project_id}_app RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

        if(BUILD_TESTING)
          add_executable({project_id}_tests tests/smoke_tests.cpp)
          target_link_libraries({project_id}_tests PRIVATE {project_id}_core)
          add_test(NAME smoke COMMAND {project_id}_tests)
        endif()
        """
    )


def desktop_presets() -> dict[str, Any]:
    return {
        "version": 6,
        "cmakeMinimumRequired": {"major": 3, "minor": 27, "patch": 0},
        "configurePresets": [
            {
                "name": "base",
                "hidden": True,
                "binaryDir": "${sourceDir}/out/build/${presetName}",
                "cacheVariables": {
                    "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
                    "CMAKE_INSTALL_PREFIX": "${sourceDir}/out/install/${presetName}",
                    "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
                },
            },
            {
                "name": "windows-base",
                "hidden": True,
                "inherits": "base",
                "generator": "Visual Studio 17 2022",
                "architecture": {"value": "x64", "strategy": "external"},
                "condition": {
                    "type": "equals",
                    "lhs": "${hostSystemName}",
                    "rhs": "Windows",
                },
                "cacheVariables": {
                    "VCPKG_TARGET_TRIPLET": "x64-windows",
                    "CMAKE_TOOLCHAIN_FILE": "C:/dev/tooling/vcpkg/scripts/buildsystems/vcpkg.cmake",
                },
            },
            {
                "name": "linux-base",
                "hidden": True,
                "inherits": "base",
                "generator": "Ninja",
                "condition": {
                    "type": "equals",
                    "lhs": "${hostSystemName}",
                    "rhs": "Linux",
                },
                "cacheVariables": {
                    "VCPKG_TARGET_TRIPLET": "x64-linux",
                },
            },
            {"name": "windows-vs-debug", "inherits": "windows-base", "cacheVariables": {"CMAKE_BUILD_TYPE": "Debug"}},
            {"name": "windows-vs-release", "inherits": "windows-base", "cacheVariables": {"CMAKE_BUILD_TYPE": "Release"}},
            {"name": "linux-debug", "inherits": "linux-base", "cacheVariables": {"CMAKE_BUILD_TYPE": "Debug"}},
            {"name": "linux-release", "inherits": "linux-base", "cacheVariables": {"CMAKE_BUILD_TYPE": "Release"}},
        ],
        "buildPresets": [
            {"name": "build-windows-vs-debug", "configurePreset": "windows-vs-debug"},
            {"name": "build-windows-vs-release", "configurePreset": "windows-vs-release"},
            {"name": "build-linux-debug", "configurePreset": "linux-debug"},
            {"name": "build-linux-release", "configurePreset": "linux-release"},
        ],
        "testPresets": [
            {"name": "test-windows-vs-debug", "configurePreset": "windows-vs-debug"},
            {"name": "test-linux-debug", "configurePreset": "linux-debug"},
        ],
    }


def game_presets() -> dict[str, Any]:
    base = desktop_presets()
    configure_presets = [
        preset for preset in base["configurePresets"] if preset["name"] in {"base", "windows-base", "linux-base"}
    ]
    configure_presets.extend(
        [
            {
                "name": "windows-vs-debug",
                "inherits": "windows-base",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Debug",
                    "RENDERER": "VULKAN",
                    "VCPKG_MANIFEST_FEATURES": "game;renderer-vulkan",
                },
            },
            {
                "name": "windows-vs-release",
                "inherits": "windows-base",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Release",
                    "RENDERER": "VULKAN",
                    "VCPKG_MANIFEST_FEATURES": "game;renderer-vulkan",
                },
            },
            {
                "name": "linux-debug",
                "inherits": "linux-base",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Debug",
                    "RENDERER": "VULKAN",
                    "VCPKG_MANIFEST_FEATURES": "game;renderer-vulkan",
                },
            },
            {
                "name": "linux-release",
                "inherits": "linux-base",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Release",
                    "RENDERER": "VULKAN",
                    "VCPKG_MANIFEST_FEATURES": "game;renderer-vulkan",
                },
            },
            {
                "name": "windows-vs-opengl-debug",
                "inherits": "windows-base",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Debug",
                    "RENDERER": "OPENGL",
                    "VCPKG_MANIFEST_FEATURES": "game;renderer-opengl",
                },
            },
            {
                "name": "windows-vs-d3d12-debug",
                "inherits": "windows-base",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Debug",
                    "RENDERER": "D3D12",
                    "VCPKG_MANIFEST_FEATURES": "game;renderer-d3d12",
                },
            },
            {
                "name": "linux-opengl-debug",
                "inherits": "linux-base",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Debug",
                    "RENDERER": "OPENGL",
                    "VCPKG_MANIFEST_FEATURES": "game;renderer-opengl",
                },
            },
        ]
    )
    return {
        "version": 6,
        "cmakeMinimumRequired": {"major": 3, "minor": 27, "patch": 0},
        "configurePresets": configure_presets,
        "buildPresets": [
            {"name": "build-windows-vs-debug", "configurePreset": "windows-vs-debug"},
            {"name": "build-windows-vs-release", "configurePreset": "windows-vs-release"},
            {"name": "build-linux-debug", "configurePreset": "linux-debug"},
            {"name": "build-linux-release", "configurePreset": "linux-release"},
            {"name": "build-windows-vs-opengl-debug", "configurePreset": "windows-vs-opengl-debug"},
            {"name": "build-windows-vs-d3d12-debug", "configurePreset": "windows-vs-d3d12-debug"},
            {"name": "build-linux-opengl-debug", "configurePreset": "linux-opengl-debug"},
        ],
        "testPresets": [
            {"name": "test-windows-vs-debug", "configurePreset": "windows-vs-debug"},
            {"name": "test-linux-debug", "configurePreset": "linux-debug"},
        ],
    }


def vcpkg_manifest(package_name: str, kind: str) -> dict[str, Any]:
    manifest: dict[str, Any] = {
        "name": package_name,
        "version-string": "0.1.0",
        "dependencies": [
            "fmt",
            "nlohmann-json",
            "spdlog",
        ],
    }
    if kind == "game":
        manifest["features"] = {
            "game": {
                "description": "Common cross-platform game dependencies.",
                "dependencies": ["glm", "sdl3"],
            },
            "renderer-vulkan": {
                "description": "Vulkan development profile.",
                "dependencies": ["vulkan"],
            },
            "renderer-opengl": {
                "description": "OpenGL development profile.",
                "dependencies": [],
            },
            "renderer-d3d12": {
                "description": "Windows-only D3D12 development profile.",
                "dependencies": [],
            },
        }
    return manifest


def vcpkg_configuration(baseline: str) -> dict[str, Any]:
    return {
        "default-registry": {
            "kind": "git",
            "repository": "https://github.com/microsoft/vcpkg",
            "baseline": baseline,
        }
    }


def desktop_files(project_id: str, package_name: str, display_name: str, namespace: str, baseline: str) -> dict[str, str]:
    return {
        ".editorconfig": editorconfig(),
        ".gitignore": gitignore(),
        "AGENTS.md": agents_md(namespace),
        "README.md": desktop_readme(display_name),
        "agent/command_brief.json": json.dumps(agent_command_brief(display_name, "desktop"), indent=2),
        "docs/stack.md": stack_md(display_name, namespace, "desktop"),
        "plan/plan.md": plan_md(display_name, "desktop"),
        "cmake/CopyRuntimeDeps.cmake": copy_runtime_deps_cmake(),
        "cmake/PortabilityCheck.cmake": portability_check_cmake(),
        "cmake/RunPortabilityCheck.cmake": run_portability_check_cmake(),
        "CMakeLists.txt": desktop_cmakelists(project_id),
        "CMakePresets.json": json.dumps(desktop_presets(), indent=2),
        "vcpkg.json": json.dumps(vcpkg_manifest(package_name, "desktop"), indent=2),
        "vcpkg-configuration.json": json.dumps(vcpkg_configuration(baseline), indent=2),
        f"include/{namespace}/core/application.hpp": desktop_application_header(namespace),
        f"include/{namespace}/core/platform/runtime_platform.hpp": runtime_platform_header(namespace),
        "src/core/application.cpp": desktop_application_cpp(namespace),
        "src/app/main.cpp": desktop_main_cpp(namespace, display_name),
        "src/platform/win32/runtime_platform.cpp": windows_runtime_cpp(namespace),
        "src/platform/linux/runtime_platform.cpp": linux_runtime_cpp(namespace),
        "tests/smoke_tests.cpp": desktop_tests_cpp(namespace, display_name),
    }


def game_files(project_id: str, package_name: str, display_name: str, namespace: str, baseline: str) -> dict[str, str]:
    return {
        ".editorconfig": editorconfig(),
        ".gitignore": gitignore(),
        "AGENTS.md": agents_md(namespace),
        "README.md": game_readme(display_name),
        "agent/command_brief.json": json.dumps(agent_command_brief(display_name, "game"), indent=2),
        "docs/stack.md": stack_md(display_name, namespace, "game"),
        "plan/plan.md": plan_md(display_name, "game"),
        "cmake/CopyRuntimeDeps.cmake": copy_runtime_deps_cmake(),
        "cmake/PortabilityCheck.cmake": portability_check_cmake(),
        "cmake/RunPortabilityCheck.cmake": run_portability_check_cmake(),
        "CMakeLists.txt": game_cmakelists(project_id),
        "CMakePresets.json": json.dumps(game_presets(), indent=2),
        "vcpkg.json": json.dumps(vcpkg_manifest(package_name, "game"), indent=2),
        "vcpkg-configuration.json": json.dumps(vcpkg_configuration(baseline), indent=2),
        f"include/{namespace}/core/platform/runtime_platform.hpp": runtime_platform_header(namespace),
        f"include/{namespace}/game/game_app.hpp": game_application_header(namespace),
        f"include/{namespace}/game/renderers/renderer_profile.hpp": game_renderer_header(namespace),
        "src/core/application.cpp": game_application_cpp(namespace, display_name),
        "src/core/renderers/renderer_profile.cpp": game_renderer_cpp(namespace),
        "src/app/main.cpp": game_main_cpp(namespace),
        "src/platform/win32/runtime_platform.cpp": windows_runtime_cpp(namespace),
        "src/platform/linux/runtime_platform.cpp": linux_runtime_cpp(namespace),
        "src/platform/win32/native_window_stub.cpp": game_native_stub(namespace, "win32"),
        "src/platform/linux/native_window_stub.cpp": game_native_stub(namespace, "linux"),
        "tests/smoke_tests.cpp": game_tests_cpp(namespace),
    }


def scaffold_project(name: str, kind: str, root: Path | None, force: bool) -> dict[str, Any]:
    ensure_machine_layout()

    display_name = name.strip()
    project_id = safe_identifier(display_name)
    package_name = project_id.replace("_", "-")
    namespace = project_id

    parent = root if root else default_dev_root() / "projects"
    project_root = parent / project_id
    if project_root.exists():
        if not force:
            raise SystemExit(f"{project_root} already exists. Use --force to replace it.")
        shutil.rmtree(project_root)

    vcpkg_root = default_vcpkg_root()
    baseline = detect_vcpkg_baseline(vcpkg_root)
    files = (
        desktop_files(project_id, package_name, display_name, namespace, baseline)
        if kind == "desktop"
        else game_files(project_id, package_name, display_name, namespace, baseline)
    )

    for relative_path, content in files.items():
        write_text(project_root / relative_path, content)

    return {
        "status": "ok",
        "project_root": str(project_root),
        "kind": kind,
        "project_id": project_id,
        "package_name": package_name,
        "baseline": baseline,
    }


def self_test() -> dict[str, Any]:
    ensure_machine_layout()
    with tempfile.TemporaryDirectory(prefix="cpp-new-") as temp_dir:
        temp_root = Path(temp_dir)
        desktop_result = scaffold_project("Desktop Sample", "desktop", temp_root, False)
        game_result = scaffold_project("Game Sample", "game", temp_root, False)

        required_paths = [
            Path(desktop_result["project_root"]) / "CMakePresets.json",
            Path(desktop_result["project_root"]) / "plan" / "plan.md",
            Path(game_result["project_root"]) / "CMakeLists.txt",
            Path(game_result["project_root"]) / "include" / "game_sample" / "game" / "renderers" / "renderer_profile.hpp",
        ]
        missing = [str(path) for path in required_paths if not path.exists()]
        if missing:
            raise SystemExit(f"Self-test failed. Missing files: {missing}")

        json.loads((Path(desktop_result["project_root"]) / "CMakePresets.json").read_text(encoding="utf-8"))
        json.loads((Path(game_result["project_root"]) / "vcpkg.json").read_text(encoding="utf-8"))

        return {
            "status": "ok",
            "desktop_project": desktop_result["project_root"],
            "game_project": game_result["project_root"],
        }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Create Windows-first, Linux-portable C++ desktop or game scaffolds."
    )
    parser.add_argument("name", nargs="?", help="Project display name.")
    parser.add_argument("--kind", choices=SUPPORTED_KINDS, default="desktop")
    parser.add_argument("--root", type=Path, help="Parent directory where the project will be created.")
    parser.add_argument("--force", action="store_true", help="Replace an existing project directory.")
    parser.add_argument("--ensure-machine-layout", action="store_true", help="Create C:/dev layout and machine env vars.")
    parser.add_argument("--self-test", action="store_true", help="Run tool self-test.")
    parser.add_argument("--version", action="store_true", help="Print tool version.")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON.")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.version:
        print(VERSION)
        return 0

    try:
        if args.ensure_machine_layout:
            print_output(ensure_machine_layout(), args.json)
            return 0

        if args.self_test:
            print_output(self_test(), args.json)
            return 0

        if not args.name:
            parser.error("name is required unless --ensure-machine-layout or --self-test is used.")

        result = scaffold_project(args.name, args.kind, args.root, args.force)
        print_output(result, args.json)
        return 0
    except SystemExit:
        raise
    except Exception as error:
        payload = {"status": "error", "message": str(error)}
        print_output(payload, args.json)
        return 1


if __name__ == "__main__":
    sys.exit(main())

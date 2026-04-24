#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ctypes
import json
import os
import re
import shutil
import sys
from pathlib import Path
from typing import Any

VERSION = "0.2.0"


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


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def source_root() -> Path:
    return repo_root() / "bootstrap" / "codex_env"


def set_user_env(name: str, value: str) -> None:
    os.environ[name] = value
    if os.name != "nt":
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


def copy_tree(source: Path, target: Path) -> None:
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target)


def validate_command_name(name: str, label: str) -> str:
    normalized = name.strip()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", normalized):
        raise SystemExit(
            f"Invalid {label}: {name!r}. Use ASCII letters, digits, '-', or '_', and start with a letter or digit."
        )
    return normalized


def default_local_agent_policy(delegation_mode: str) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "delegation_mode": delegation_mode,
        "safety_critical_always_direct": True,
        "trusted_local_agent": {
            "enabled": delegation_mode != "direct-only",
            "allowed_task_classes": [
                "bounded read-only repo search",
                "small file content inspection",
                "simple issue detection",
                "lightweight document verification",
                "non-blocking asset prompt drafting",
                "installation wait and polling work",
                "long-running log monitoring",
            ],
            "blocked_task_classes": [
                "safety-critical or high-stakes tasks",
                "destructive file operations",
                "secrets or credential handling",
                "cross-machine environment mutation without user approval",
                "blocking architectural decisions",
                "large context sweeps over many files",
            ],
        },
        "token_budget": {
            "prefer_compact_metadata": True,
            "read_manifest_before_docx": True,
            "avoid_large_context_when_bounded_read_is_enough": True,
        },
    }


def write_cmd_shim(tool_name: str, command_name: str, root: Path) -> Path:
    bin_root = root / "bin"
    bin_root.mkdir(parents=True, exist_ok=True)
    shim_path = bin_root / f"{command_name}.cmd"
    content = (
        "@echo off\n"
        "setlocal\n"
        "pushd \"%~dp0..\" >nul || (\n"
        "  echo Failed to resolve C:\\codex-local root. 1>&2\n"
        "  exit /b 1\n"
        ")\n"
        "set \"CODEX_LOCAL=%CD%\"\n"
        "set \"PYTHON=%CODEX_LOCAL%\\runtime\\python\\python.exe\"\n"
        f"set \"ENTRYPOINT=%CODEX_LOCAL%\\tools\\{tool_name}\\"
        + ("cpp_new.py" if tool_name == "cpp-new" else "plan_doc.py")
        + "\"\n"
        "if not exist \"%PYTHON%\" (\n"
        "  echo Missing durable Python runtime at \"%PYTHON%\". 1>&2\n"
        "  popd >nul\n"
        "  exit /b 1\n"
        ")\n"
        "if not exist \"%ENTRYPOINT%\" (\n"
        "  echo Missing tool entrypoint at \"%ENTRYPOINT%\". 1>&2\n"
        "  popd >nul\n"
        "  exit /b 1\n"
        ")\n"
        "\"%PYTHON%\" \"%ENTRYPOINT%\" %*\n"
        "set \"EXIT_CODE=%ERRORLEVEL%\"\n"
        "popd >nul\n"
        "exit /b %EXIT_CODE%\n"
    )
    shim_path.write_text(content, encoding="utf-8")
    return shim_path


def install_environment(
    cpp_command: str,
    plan_command: str,
    delegation_mode: str,
    runtime_source: Path | None,
) -> dict[str, Any]:
    codex_local = Path("C:/codex-local") if os.name == "nt" else Path.home() / ".codex-local"
    dev_root = Path("C:/dev") if os.name == "nt" else Path.home() / "dev"
    tools_source = source_root() / "tools"

    if not tools_source.exists():
        raise SystemExit(f"Missing repo-managed tool source at {tools_source}")

    (codex_local / "tools").mkdir(parents=True, exist_ok=True)
    (codex_local / "runtime").mkdir(parents=True, exist_ok=True)
    (codex_local / "defaults").mkdir(parents=True, exist_ok=True)
    (dev_root / "projects").mkdir(parents=True, exist_ok=True)
    (dev_root / "templates").mkdir(parents=True, exist_ok=True)
    (dev_root / "tooling").mkdir(parents=True, exist_ok=True)
    (dev_root / "cache" / "vcpkg").mkdir(parents=True, exist_ok=True)
    (dev_root / "models").mkdir(parents=True, exist_ok=True)
    (dev_root / "temp").mkdir(parents=True, exist_ok=True)

    copy_tree(tools_source / "cpp-new", codex_local / "tools" / "cpp-new")
    copy_tree(tools_source / "plan-doc", codex_local / "tools" / "plan-doc")

    defaults_source = source_root() / "defaults"
    if defaults_source.exists():
        copy_tree(defaults_source, codex_local / "defaults")

    if runtime_source:
        runtime_target = codex_local / "runtime" / "python"
        copy_tree(runtime_source, runtime_target)

    env_map = {
        "DEV_ROOT": str(dev_root),
        "VCPKG_ROOT": str(dev_root / "tooling" / "vcpkg"),
        "VCPKG_DEFAULT_BINARY_CACHE": str(dev_root / "cache" / "vcpkg"),
        "PLAN_TEMPLATE_ROOT": str(dev_root / "templates"),
        "TEMP": str(dev_root / "temp"),
        "TMP": str(dev_root / "temp"),
    }
    for key, value in env_map.items():
        set_user_env(key, value)

    cpp_shim = write_cmd_shim("cpp-new", cpp_command, codex_local)
    plan_shim = write_cmd_shim("plan-doc", plan_command, codex_local)

    local_agent_policy_path = codex_local / "defaults" / "local_agent_policy.json"
    policy_payload = default_local_agent_policy(delegation_mode)
    if local_agent_policy_path.exists():
        policy_payload = json.loads(local_agent_policy_path.read_text(encoding="utf-8"))
    policy_payload["delegation_mode"] = delegation_mode
    policy_payload["safety_critical_always_direct"] = True
    policy_payload.setdefault("trusted_local_agent", {})
    policy_payload["trusted_local_agent"]["enabled"] = delegation_mode != "direct-only"
    local_agent_policy_path.write_text(
        json.dumps(policy_payload, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    install_record = {
        "cpp_command": cpp_command,
        "plan_command": plan_command,
        "delegation_mode": delegation_mode,
        "runtime_source": str(runtime_source) if runtime_source else None,
        "repo_root": str(repo_root()),
    }
    (codex_local / "bootstrap-source.json").write_text(
        json.dumps(install_record, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    return {
        "status": "ok",
        "repo_root": str(repo_root()),
        "codex_local": str(codex_local),
        "dev_root": str(dev_root),
        "cpp_shim": str(cpp_shim),
        "plan_shim": str(plan_shim),
        "delegation_mode": delegation_mode,
        "local_agent_policy": str(local_agent_policy_path),
        "runtime_installed": runtime_source is not None,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Install repo-managed Codex tools to C:/codex-local.")
    parser.add_argument("--cpp-command", default="cpp-new", help="Command name for the C++ scaffold tool.")
    parser.add_argument("--plan-command", default="plan-doc", help="Command name for the plan document tool.")
    parser.add_argument(
        "--delegation-mode",
        choices=("ask-user", "prefer-local-readonly", "direct-only"),
        default="ask-user",
        help="Default policy for low-risk local delegation on this machine.",
    )
    parser.add_argument(
        "--runtime-source",
        type=Path,
        help="Optional ASCII-path Python runtime directory to copy into C:/codex-local/runtime/python.",
    )
    parser.add_argument("--version", action="store_true", help="Print installer version.")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON.")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.version:
        print(VERSION)
        return 0

    args.cpp_command = validate_command_name(args.cpp_command, "--cpp-command")
    args.plan_command = validate_command_name(args.plan_command, "--plan-command")
    if args.cpp_command == args.plan_command:
        parser.error("--cpp-command and --plan-command must be different.")

    if args.runtime_source and not args.runtime_source.exists():
        parser.error(f"--runtime-source does not exist: {args.runtime_source}")

    try:
        result = install_environment(
            args.cpp_command,
            args.plan_command,
            args.delegation_mode,
            args.runtime_source,
        )
        print_output(result, args.json)
        return 0
    except SystemExit:
        raise
    except Exception as error:
        print_output({"status": "error", "message": str(error)}, args.json)
        return 1


if __name__ == "__main__":
    sys.exit(main())

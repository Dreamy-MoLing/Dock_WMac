#!/usr/bin/env python3
"""
helper.py — Dock_WMac Python helper

IPC over stdin/stdout JSON.
C++ sends commands, this process responds.
No third-party dependencies. Pure stdlib.
"""

import json
import sys
import os
from pathlib import Path
from configparser import ConfigParser


# ─── Configuration ─────────────────────────────────────────────

def _config_path() -> Path:
    """Return path to config.json (Linux: ~/.config/Dock_WMac/)."""
    xdg_config = os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config"))
    return Path(xdg_config) / "Dock_WMac" / "config.json"


def read_config() -> dict:
    """Read config.json. Return empty dict if not found."""
    path = _config_path()
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return {}


def write_config(data: dict) -> bool:
    """Write data to config.json. Create dirs if needed."""
    path = _config_path()
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            json.dumps(data, indent=4, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        return True
    except OSError:
        return False


# ─── Desktop file parsing ──────────────────────────────────────

def parse_desktop(file_path: str) -> dict:
    """
    Parse a .desktop file and return a DockItemData-like dict.

    Returns:
        {"status": "ok", "data": {appId, displayName, execPath, icon, wmClass}}
        or {"status": "error", "message": "..."}
    """
    path = Path(file_path)
    if not path.exists():
        return {"status": "error", "message": f"File not found: {file_path}"}

    try:
        config = ConfigParser(strict=False, interpolation=None)
        config.read(file_path, encoding="utf-8")
    except Exception as e:
        return {"status": "error", "message": str(e)}

    if not config.has_section("Desktop Entry"):
        return {"status": "error", "message": "Missing [Desktop Entry]"}

    de = config["Desktop Entry"]

    # Filter out invalid entries
    if de.get("NoDisplay", "false").lower() == "true":
        return {"status": "skip", "reason": "NoDisplay"}
    if de.get("Hidden", "false").lower() == "true":
        return {"status": "skip", "reason": "Hidden"}
    if de.get("Type", "") != "Application":
        return {"status": "skip", "reason": "Not Application"}
    if not de.get("Exec"):
        return {"status": "skip", "reason": "No Exec"}

    import re

    exec_path = de.get("Exec", "")
    # Strip %f %F %u %U etc.
    exec_path = re.sub(r"\s+%[fFuU]", "", exec_path)

    app_id = path.stem
    icon_name = de.get("Icon", "")

    data = {
        "appId": app_id,
        "displayName": de.get("Name", app_id),
        "execPath": exec_path,
        "iconPath": icon_name,
        "wmClass": de.get("StartupWMClass", app_id.split(".")[-1]),
        # Signal whether this is a pinned/known app for Dock_WMac
        "isPinned": de.get("X-Dock-Pinned", "false").lower() == "true",
    }
    return {"status": "ok", "data": data}


# ─── Process detection ─────────────────────────────────────────

def check_processes(names: list) -> dict:
    """
    Batch check which process names are currently running.

    Reads /proc/*/comm. More reliable than pgrep because we
    avoid extra fork and get all names in one pass.

    Returns:
        {"status": "ok", "data": {"is_running": {name: bool, ...}}}
    """
    running = set()
    proc_dir = Path("/proc")
    if not proc_dir.exists():
        return {"status": "ok", "data": {"is_running": {n: False for n in names}}}

    for entry in proc_dir.iterdir():
        if not entry.name.isdigit():
            continue
        comm_file = entry / "comm"
        try:
            name = comm_file.read_text(encoding="utf-8").strip()
            running.add(name)
        except (OSError, UnicodeDecodeError):
            continue

    result = {}
    for name in names:
        exec_name = Path(name).name  # strip path
        result[name] = exec_name in running

    return {"status": "ok", "data": {"is_running": result}}


def scan_running_apps() -> dict:
    """
    Scan all running processes and return unique executable names.

    Returns:
        {"status": "ok", "data": {"processes": [name1, name2, ...]}}
    """
    proc_dir = Path("/proc")
    if not proc_dir.exists():
        return {"status": "ok", "data": {"processes": []}}

    names = set()
    for entry in proc_dir.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            exe = (entry / "exe").resolve()
            if exe.exists():
                names.add(exe.name)
        except OSError:
            # Try comm as fallback
            try:
                comm = (entry / "comm").read_text(encoding="utf-8").strip()
                names.add(comm)
            except OSError:
                continue

    return {"status": "ok", "data": {"processes": sorted(names)}}


# ─── Desktop file scanning ─────────────────────────────────────

DESKTOP_PATHS = [
    os.path.expanduser("~/.local/share/applications"),
    "/usr/share/applications",
    "/usr/local/share/applications",
]

DEFAULT_APPS = [
    "org.gnome.Nautilus",
    "firefox",
    "org.gnome.Terminal",
    "org.gnome.TextEditor",
    "org.gnome.Calculator",
]


def scan_desktop_files() -> dict:
    """
    Scan all .desktop files across standard paths.

    Returns list of parsed items for pinned/x-dock-pinned apps
    and the default app list.

    Returns:
        {"status": "ok", "data": {"items": [...]}}
    """
    seen = set()
    items = []

    for dir_path in DESKTOP_PATHS:
        dp = Path(dir_path)
        if not dp.exists():
            continue

        for desktop_file in dp.glob("*.desktop"):
            app_id = desktop_file.stem
            if app_id in seen:
                continue

            result = parse_desktop(str(desktop_file))
            if result.get("status") != "ok":
                continue

            data = result["data"]
            # Only include pinned or default apps
            if data["isPinned"] or app_id in DEFAULT_APPS:
                items.append(data)
                seen.add(app_id)

    # Fallback: if nothing found, try default apps directly
    if not items:
        for app_id in DEFAULT_APPS:
            if app_id in seen:
                continue
            for dir_path in DESKTOP_PATHS:
                fp = Path(dir_path) / f"{app_id}.desktop"
                if fp.exists():
                    result = parse_desktop(str(fp))
                    if result.get("status") == "ok":
                        items.append(result["data"])
                        seen.add(app_id)
                        break

    return {"status": "ok", "data": {"items": items}}


# ─── IPC Main Loop ─────────────────────────────────────────────

COMMANDS = {
    "read_config": lambda params: {"status": "ok", "data": read_config()},
    "write_config": lambda params: {
        "status": "ok" if write_config(params.get("data", {})) else "error"
    },
    "parse_desktop": lambda params: parse_desktop(params.get("path", "")),
    "check_processes": lambda params: check_processes(params.get("names", [])),
    "scan_running_apps": lambda params: scan_running_apps(),
    "scan_desktop_files": lambda params: scan_desktop_files(),
    "ping": lambda params: {"status": "ok", "data": {"version": "1.0.0"}},
}


def main():
    """Read JSON commands from stdin, write JSON responses to stdout."""
    # Signal readiness
    print(json.dumps({"event": "ready", "version": "1.0.0"}))
    sys.stdout.flush()

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        try:
            request = json.loads(line)
        except json.JSONDecodeError as e:
            response = {"status": "error", "message": f"Invalid JSON: {e}"}
            print(json.dumps(response))
            sys.stdout.flush()
            continue

        cmd = request.get("cmd", "")
        params = request.get("params", {})

        handler = COMMANDS.get(cmd)
        if handler is None:
            response = {"status": "error", "message": f"Unknown command: {cmd}"}
        else:
            try:
                response = handler(params)
            except Exception as e:
                response = {"status": "error", "message": str(e)}

        print(json.dumps(response))
        sys.stdout.flush()


if __name__ == "__main__":
    main()

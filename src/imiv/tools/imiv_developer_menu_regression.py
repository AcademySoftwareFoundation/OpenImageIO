#!/usr/bin/env python3
"""Regression check for the debug-only Developer menu in imiv."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path


ERROR_PATTERNS = (
    "VUID-",
    "fatal Vulkan error",
    "developer menu regression: demo window did not open",
)
SKIP_MARKER = "developer menu regression skipped: not available in release build"


def _default_binary(repo_root: Path) -> Path:
    candidates = [
        repo_root / "build_u" / "bin" / "imiv",
        repo_root / "build" / "bin" / "imiv",
        repo_root / "build_u" / "src" / "imiv" / "imiv",
        repo_root / "build" / "src" / "imiv" / "imiv",
        repo_root / "build" / "Debug" / "imiv.exe",
        repo_root / "build" / "Release" / "imiv.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def _load_env_from_script(script_path: Path) -> dict[str, str]:
    env = dict(os.environ)
    if not script_path.exists() or shutil.which("bash") is None:
        return env

    quoted = shlex.quote(str(script_path))
    proc = subprocess.run(
        ["bash", "-lc", f"source {quoted} >/dev/null 2>&1; env -0"],
        check=True,
        stdout=subprocess.PIPE,
    )
    loaded: dict[str, str] = {}
    for item in proc.stdout.split(b"\0"):
        if not item:
            continue
        key, _, value = item.partition(b"=")
        if not key:
            continue
        loaded[key.decode("utf-8", errors="ignore")] = value.decode(
            "utf-8", errors="ignore"
        )
    env.update(loaded)
    return env


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"
    default_out = repo_root / "build_u" / "imiv_captures" / "developer_menu_regression"
    default_env_script = repo_root / "build_u" / "imiv_env.sh"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument("--env-script", default=str(default_env_script), help="Optional shell env setup script")
    ap.add_argument("--open", default=str(default_image), help="Image to open")
    ap.add_argument("--out-dir", default=str(default_out), help="Output directory")
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return _fail(f"binary not found: {exe}")

    cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()
    image_path = Path(args.open).expanduser().resolve()
    if not image_path.exists():
        return _fail(f"image not found: {image_path}")

    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    layout_path = out_dir / "developer_menu_layout.json"
    log_path = out_dir / "developer_menu.log"

    env = _load_env_from_script(Path(args.env_script).expanduser())
    env.update(
        {
            "IMIV_IMGUI_TEST_ENGINE": "1",
            "IMIV_IMGUI_TEST_ENGINE_EXIT_ON_FINISH": "1",
            "IMIV_IMGUI_TEST_ENGINE_DEVELOPER_MENU_METRICS": "1",
            "IMIV_IMGUI_TEST_ENGINE_DEVELOPER_MENU_LAYOUT_OUT": str(layout_path),
            "IMIV_IMGUI_TEST_ENGINE_DEVELOPER_MENU_LAYOUT_ITEMS": "1",
            "IMIV_IMGUI_TEST_ENGINE_DEVELOPER_MENU_LAYOUT_DEPTH": "8",
        }
    )
    if args.trace:
        env["IMIV_IMGUI_TEST_ENGINE_TRACE"] = "1"

    cmd = [str(exe), str(image_path)]
    with log_path.open("w", encoding="utf-8") as log_handle:
        proc = subprocess.run(
            cmd,
            cwd=str(cwd),
            env=env,
            check=False,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            timeout=60,
        )

    if proc.returncode != 0:
        return _fail(f"imiv exited with code {proc.returncode}")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    if SKIP_MARKER in log_text:
        print("skip: developer menu not available in this build")
        return 77

    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            return _fail(f"found runtime error pattern: {pattern}")

    deadline = time.monotonic() + 2.0
    while not layout_path.exists() and time.monotonic() < deadline:
        time.sleep(0.05)

    if not layout_path.exists():
        return _fail(f"layout json not written: {layout_path}")

    window_names: list[str] = []
    deadline = time.monotonic() + 2.0
    while time.monotonic() < deadline:
        try:
            data = json.loads(layout_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            time.sleep(0.05)
            continue
        window_names = [window.get("name", "") for window in data.get("windows", [])]
        if "Dear ImGui Demo" in window_names:
            break
        time.sleep(0.05)

    if "Dear ImGui Demo" not in window_names:
        return _fail("layout json does not contain Dear ImGui Demo window")

    print("layout:", layout_path)
    print("log:", log_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

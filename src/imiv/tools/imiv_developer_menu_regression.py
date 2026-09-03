#!/usr/bin/env python3
"""Regression check for the runtime-enabled Developer menu in imiv."""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

from imiv_test_utils import (
    default_binary,
    default_env_script,
    fail,
    load_env_from_script,
    repo_root as imiv_repo_root,
    resolve_run_cwd,
    run_logged_process,
)


ERROR_PATTERNS = (
    "VUID-",
    "fatal Vulkan error",
    "developer menu regression: demo window did not open",
)

def main() -> int:
    repo_root = imiv_repo_root()
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"
    default_out = repo_root / "build_u" / "imiv_captures" / "developer_menu_regression"
    env_script_default = default_env_script(repo_root)

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed directly to imiv",
    )
    ap.add_argument("--env-script", default=str(env_script_default), help="Optional shell env setup script")
    ap.add_argument("--open", default=str(default_image), help="Image to open")
    ap.add_argument("--out-dir", default=str(default_out), help="Output directory")
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")

    cwd = resolve_run_cwd(exe, args.cwd)
    image_path = Path(args.open).expanduser().resolve()
    if not image_path.exists():
        return fail(f"image not found: {image_path}")

    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    layout_path = out_dir / "developer_menu_layout.json"
    log_path = out_dir / "developer_menu.log"

    env = load_env_from_script(Path(args.env_script).expanduser())
    env.update(
        {
            "OIIO_DEVMODE": "1",
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

    cmd = [str(exe)]
    if args.backend:
        cmd.extend(["--backend", args.backend])
    cmd.append(str(image_path))
    proc = run_logged_process(
        cmd, cwd=cwd, env=env, timeout=60, log_path=log_path
    )

    if proc.returncode != 0:
        return fail(f"imiv exited with code {proc.returncode}")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            return fail(f"found runtime error pattern: {pattern}")

    deadline = time.monotonic() + 2.0
    while not layout_path.exists() and time.monotonic() < deadline:
        time.sleep(0.05)

    if not layout_path.exists():
        return fail(f"layout json not written: {layout_path}")

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
        return fail("layout json does not contain Dear ImGui Demo window")

    print("layout:", layout_path)
    print("log:", log_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

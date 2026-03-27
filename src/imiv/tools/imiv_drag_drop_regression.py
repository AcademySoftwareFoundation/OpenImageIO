#!/usr/bin/env python3
"""Regression check for multi-file drag/drop into the shared image library."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


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
    for item in proc.stdout.split(b"\0"):
        if not item:
            continue
        key, _, value = item.partition(b"=")
        if not key:
            continue
        env[key.decode("utf-8", errors="ignore")] = value.decode(
            "utf-8", errors="ignore"
        )
    return env


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def main() -> int:
    repo_root = _repo_root()
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_env_script = repo_root / "build" / "imiv_env.sh"
    default_out_dir = repo_root / "build" / "imiv_captures" / "drag_drop_regression"
    default_images = [
        repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png",
        repo_root / "testsuite" / "imiv" / "images" / "CC988_ACEScg.exr",
    ]

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument(
        "--env-script",
        default=str(default_env_script),
        help="Optional shell env setup script",
    )
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument(
        "--image",
        dest="images",
        action="append",
        default=[],
        help="Dropped image path; may be repeated",
    )
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve(strict=False)
    if not runner.exists():
        return _fail(f"runner not found: {runner}")

    images = [Path(p).expanduser().resolve() for p in args.images] if args.images else default_images
    if len(images) < 2:
        return _fail("regression requires at least two dropped images")
    for image in images:
        if not image.exists():
            return _fail(f"image not found: {image}")

    cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()
    out_dir = Path(args.out_dir).expanduser().resolve()
    state_path = out_dir / "drag_drop.state.json"
    layout_path = out_dir / "drag_drop.layout.json"
    log_path = out_dir / "drag_drop.log"
    drop_paths_file = out_dir / "drop_paths.txt"

    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    drop_paths_file.write_text(
        "".join(f"{image}\n" for image in images), encoding="utf-8"
    )

    env = dict(os.environ)
    env.update(_load_env_from_script(Path(args.env_script).expanduser()))
    config_home = out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)
    env["IMIV_IMGUI_TEST_ENGINE_DROP_APPLY_FRAME"] = "2"
    env["IMIV_IMGUI_TEST_ENGINE_DROP_PATHS_FILE"] = str(drop_paths_file)

    cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(cwd),
        "--layout-json-out",
        str(layout_path),
        "--layout-items",
        "--state-json-out",
        str(state_path),
        "--state-delay-frames",
        "8",
    ]
    if args.backend:
        cmd.extend(["--backend", args.backend])
    if args.trace:
        cmd.append("--trace")

    print("run:", " ".join(cmd))
    proc = subprocess.run(
        cmd,
        cwd=str(repo_root),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        print(proc.stdout, end="")
        return _fail(f"runner exited with code {proc.returncode}")

    if not state_path.exists():
        return _fail(f"state output not found: {state_path}")

    state = json.loads(state_path.read_text(encoding="utf-8"))
    if not bool(state.get("image_loaded", False)):
        return _fail("state does not report a loaded image after drop")
    if state.get("image_path") != str(images[0]):
        return _fail("drop did not load the first dropped image")
    if int(state.get("loaded_image_count", 0)) != len(images):
        return _fail("shared image library count does not match dropped images")
    if int(state.get("current_image_index", -1)) != 0:
        return _fail("drop did not select the first dropped image in the queue")
    if not bool(state.get("image_list_visible", False)):
        return _fail("Image List did not auto-open after multi-file drop")

    print(f"layout: {layout_path}")
    print(f"state: {state_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

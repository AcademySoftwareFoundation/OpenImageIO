#!/usr/bin/env python3
"""Basic screenshot smoke test for the OpenGL imiv backend."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


ERROR_PATTERNS = (
    "error: imiv exited with code",
    "OpenGL preview draw failed",
    "OpenGL OCIO preview draw failed",
)


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


def main() -> int:
    repo_root = _repo_root()
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_out_dir = repo_root / "build_u" / "imiv_captures" / "opengl_smoke_regression"
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument("--env-script", default=str(default_env_script), help="Optional shell env setup script")
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument("--open", default=str(default_image), help="Image to open")
    ap.add_argument("--trace", action="store_true", help="Enable runner tracing")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        print(f"error: binary not found: {exe}", file=sys.stderr)
        return 2

    cwd = Path(args.cwd).resolve() if args.cwd else exe.parent.resolve()
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    image_path = Path(args.open).resolve()
    if not image_path.exists():
        print(f"error: image not found: {image_path}", file=sys.stderr)
        return 2

    env = _load_env_from_script(Path(args.env_script).resolve())
    env["IMIV_CONFIG_HOME"] = str(out_dir / "cfg")

    screenshot_path = out_dir / "opengl_smoke.png"
    layout_path = out_dir / "opengl_smoke.layout.json"
    state_path = out_dir / "opengl_smoke.state.json"
    log_path = out_dir / "opengl_smoke.log"

    cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(cwd),
    ]
    if args.backend:
        cmd.extend(["--backend", args.backend])
    cmd.extend(
        [
            "--open",
            str(image_path),
            "--screenshot-out",
            str(screenshot_path),
            "--layout-json-out",
            str(layout_path),
            "--layout-items",
            "--state-json-out",
            str(state_path),
        ]
    )
    if args.trace:
        cmd.append("--trace")

    with log_path.open("w", encoding="utf-8") as log_handle:
        proc = subprocess.run(
            cmd,
            cwd=str(repo_root),
            env=env,
            check=False,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            timeout=90,
        )
    if proc.returncode != 0:
        print(f"error: runner exited with code {proc.returncode}", file=sys.stderr)
        return 1

    for required in (screenshot_path, layout_path, state_path):
        if not required.exists():
            print(f"error: missing output: {required}", file=sys.stderr)
            return 1

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            print(f"error: found runtime error pattern: {pattern}", file=sys.stderr)
            return 1

    layout = json.loads(layout_path.read_text(encoding="utf-8"))
    if not any(window.get("name") == "Image" for window in layout.get("windows", [])):
        print("error: layout dump missing Image window", file=sys.stderr)
        return 1

    state = json.loads(state_path.read_text(encoding="utf-8"))
    current_path = state.get("image_path") or ""
    if not current_path:
        print("error: state dump missing image_path", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

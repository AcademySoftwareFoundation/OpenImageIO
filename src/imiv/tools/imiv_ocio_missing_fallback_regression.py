#!/usr/bin/env python3
"""Regression check for imiv fallback when no OCIO config is available."""

from __future__ import annotations

import argparse
import filecmp
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


ERROR_PATTERNS = (
    "VUID-",
    "fatal Vulkan error",
    "error: imiv exited with code",
)


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


def _run_case(
    repo_root: Path,
    runner: Path,
    exe: Path,
    cwd: Path,
    env: dict[str, str],
    image_path: Path,
    out_dir: Path,
    name: str,
    ocio_use: bool,
    trace: bool,
) -> tuple[Path, Path]:
    screenshot_path = out_dir / f"{name}.png"
    log_path = out_dir / f"{name}.log"
    cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(cwd),
        "--open",
        str(image_path),
        "--ocio-use",
        "true" if ocio_use else "false",
        "--screenshot-out",
        str(screenshot_path),
    ]
    if trace:
        cmd.append("--trace")

    with log_path.open("w", encoding="utf-8") as log_handle:
        proc = subprocess.run(
            cmd,
            cwd=str(repo_root),
            env=env,
            check=False,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            timeout=60,
        )
    if proc.returncode != 0:
        raise RuntimeError(f"{name}: runner exited with code {proc.returncode}")
    if not screenshot_path.exists():
        raise RuntimeError(f"{name}: screenshot not written")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            raise RuntimeError(f"{name}: found runtime error pattern: {pattern}")
    return screenshot_path, log_path


if __name__ == "__main__":
    repo_root = Path(__file__).resolve().parents[3]
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"
    default_out = repo_root / "build_u" / "imiv_captures" / "ocio_missing_fallback_regression"
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"

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
        raise SystemExit(_fail(f"binary not found: {exe}"))

    cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()
    image_path = Path(args.open).expanduser().resolve()
    if not image_path.exists():
        raise SystemExit(_fail(f"image not found: {image_path}"))

    runner = default_runner.resolve()
    if not runner.exists():
        raise SystemExit(_fail(f"runner not found: {runner}"))

    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    env = _load_env_from_script(Path(args.env_script).expanduser())
    env.pop("OCIO", None)
    env["IMIV_CONFIG_HOME"] = str(out_dir / "config_home")

    try:
        off_png, off_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            env,
            image_path,
            out_dir,
            "ocio_off",
            False,
            args.trace,
        )
        on_png, on_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            env,
            image_path,
            out_dir,
            "ocio_on",
            True,
            args.trace,
        )
    except (subprocess.SubprocessError, RuntimeError) as exc:
        raise SystemExit(_fail(str(exc)))

    if not filecmp.cmp(off_png, on_png, shallow=False):
        raise SystemExit(
            _fail(
                "missing-OCIO fallback changed rendered output; expected "
                "Use OCIO on/off to match when no config is available"
            )
        )

    print("off:", off_png)
    print("on:", on_png)
    print("off_log:", off_log)
    print("on_log:", on_log)

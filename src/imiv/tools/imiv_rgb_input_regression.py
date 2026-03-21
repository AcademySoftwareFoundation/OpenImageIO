#!/usr/bin/env python3
"""Regression check for loading a true RGB input image in imiv."""

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
    "OpenGL texture upload failed",
    "OpenGL preview draw failed",
    "OpenGL OCIO preview draw failed",
    "failed to create Metal source texture",
    "failed to create Metal upload pipeline",
    "failed to create Metal source upload buffer",
    "Metal source upload compute dispatch failed",
    "failed to create Metal preview texture",
    "Metal preview render failed",
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


def _default_oiiotool(repo_root: Path) -> Path:
    candidates = [
        repo_root / "build_u" / "bin" / "oiiotool",
        repo_root / "build" / "bin" / "oiiotool",
        repo_root / "build_u" / "src" / "oiiotool" / "oiiotool",
        repo_root / "build" / "src" / "oiiotool" / "oiiotool",
        repo_root / "build" / "Debug" / "oiiotool.exe",
        repo_root / "build" / "Release" / "oiiotool.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    which = shutil.which("oiiotool")
    return Path(which) if which else candidates[0]


def _default_env_script(repo_root: Path, exe: Path | None = None) -> Path:
    candidates: list[Path] = []
    if exe is not None:
        exe = exe.resolve()
        candidates.extend([exe.parent / "imiv_env.sh", exe.parent.parent / "imiv_env.sh"])
    candidates.extend([repo_root / "build" / "imiv_env.sh", repo_root / "build_u" / "imiv_env.sh"])
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


def _run_checked(cmd: list[str], *, cwd: Path) -> None:
    print("run:", " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd), check=True)


def _generate_rgb_fixture(oiiotool: Path, source_path: Path, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    _run_checked(
        [
            str(oiiotool),
            str(source_path),
            "--ch",
            "R,G,B",
            "-d",
            "uint8",
            "-o",
            str(out_path),
        ],
        cwd=out_path.parent,
    )


def main() -> int:
    repo_root = _repo_root()
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_source = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument(
        "--oiiotool", default=str(_default_oiiotool(repo_root)), help="oiiotool executable"
    )
    ap.add_argument("--env-script", default="", help="Optional shell env setup script")
    ap.add_argument("--out-dir", default="", help="Output directory")
    ap.add_argument(
        "--source-image",
        default=str(default_source),
        help="Source image used to generate a 3-channel RGB fixture",
    )
    ap.add_argument("--trace", action="store_true", help="Enable runner tracing")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        print(f"error: binary not found: {exe}", file=sys.stderr)
        return 2

    oiiotool = Path(args.oiiotool).resolve()
    if not oiiotool.exists():
        print(f"error: oiiotool not found: {oiiotool}", file=sys.stderr)
        return 2

    cwd = Path(args.cwd).resolve() if args.cwd else exe.parent.resolve()
    if args.out_dir:
        out_dir = Path(args.out_dir).resolve()
    else:
        out_dir = exe.parent.parent / "imiv_captures" / "rgb_input_regression"
    out_dir.mkdir(parents=True, exist_ok=True)

    source_path = Path(args.source_image).resolve()
    if not source_path.exists():
        print(f"error: source image not found: {source_path}", file=sys.stderr)
        return 2

    env_script = (
        Path(args.env_script).resolve()
        if args.env_script
        else _default_env_script(repo_root, exe)
    )
    env = _load_env_from_script(env_script)
    env["IMIV_CONFIG_HOME"] = str(out_dir / "cfg")

    rgb_fixture = out_dir / "rgb_input_fixture_u8.tif"
    _generate_rgb_fixture(oiiotool, source_path, rgb_fixture)

    layout_path = out_dir / "rgb_input.layout.json"
    state_path = out_dir / "rgb_input.state.json"
    log_path = out_dir / "rgb_input.log"

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
            str(rgb_fixture),
            "--layout-json-out",
            str(layout_path),
            "--layout-items",
            "--state-json-out",
            str(state_path),
            "--post-action-delay-frames",
            "2",
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

    for required in (layout_path, state_path):
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
    if not state.get("image_loaded"):
        print("error: state dump says image is not loaded", file=sys.stderr)
        return 1

    current_path = state.get("image_path") or ""
    if not current_path:
        print("error: state dump missing image_path", file=sys.stderr)
        return 1
    try:
        current_resolved = Path(current_path).resolve()
    except Exception:
        current_resolved = Path(current_path)
    if current_resolved != rgb_fixture.resolve():
        print(
            f"error: loaded path mismatch: expected {rgb_fixture}, got {current_path}",
            file=sys.stderr,
        )
        return 1

    image_size = state.get("image_size", [0, 0])
    if (
        not isinstance(image_size, list)
        or len(image_size) != 2
        or int(image_size[0]) <= 0
        or int(image_size[1]) <= 0
    ):
        print(f"error: invalid image_size in state dump: {image_size}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

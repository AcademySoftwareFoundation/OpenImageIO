#!/usr/bin/env python3
"""Regression check for startup folder-open queue filtering."""

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


def _run(cmd: list[str], *, cwd: Path, env: dict[str, str], log_path: Path) -> None:
    proc = subprocess.run(
        cmd,
        cwd=str(cwd),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {' '.join(cmd)}\n{proc.stdout}"
        )


def main() -> int:
    repo_root = _repo_root()
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_env_script = repo_root / "build" / "imiv_env.sh"
    default_out_dir = repo_root / "build" / "imiv_captures" / "open_folder_regression"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument(
        "--oiiotool",
        default=str(repo_root / "build" / "bin" / "oiiotool"),
        help="oiiotool executable",
    )
    ap.add_argument(
        "--env-script",
        default=str(default_env_script),
        help="Optional shell env setup script",
    )
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return _fail(f"binary not found: {exe}")
    oiiotool = Path(args.oiiotool).expanduser().resolve()
    if not oiiotool.exists():
        return _fail(f"oiiotool not found: {oiiotool}")
    runner = runner.resolve()
    if not runner.exists():
        return _fail(f"runner not found: {runner}")

    cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()
    out_dir = Path(args.out_dir).expanduser().resolve()
    runtime_dir = out_dir / "runtime"
    state_path = out_dir / "open_folder.state.json"
    log_path = out_dir / "open_folder.log"

    shutil.rmtree(out_dir, ignore_errors=True)
    runtime_dir.mkdir(parents=True, exist_ok=True)

    env = dict(os.environ)
    env.update(_load_env_from_script(Path(args.env_script).expanduser()))
    config_home = out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)

    source_logo = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"
    if not source_logo.exists():
        return _fail(f"source image not found: {source_logo}")

    folder_dir = runtime_dir / "mixed_folder"
    folder_dir.mkdir(parents=True, exist_ok=True)
    image_a = folder_dir / "01_logo.png"
    image_b = folder_dir / "02_logo.exr"
    notes = folder_dir / "03_notes.txt"
    blob = folder_dir / "04_blob.bin"

    try:
        _run(
            [str(oiiotool), str(source_logo), "-o", str(image_a)],
            cwd=repo_root,
            env=env,
            log_path=runtime_dir / "make_png.log",
        )
        _run(
            [str(oiiotool), str(source_logo), "--ch", "R,G,B", "-d", "half", "-o", str(image_b)],
            cwd=repo_root,
            env=env,
            log_path=runtime_dir / "make_exr.log",
        )
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    notes.write_text("not an image\n", encoding="utf-8")
    blob.write_bytes(b"\x00\x01\x02\x03")

    cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(cwd),
        "--open",
        str(folder_dir),
        "--state-json-out",
        str(state_path),
    ]
    if args.backend:
        cmd.extend(["--backend", args.backend])
    if args.trace:
        cmd.append("--trace")

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
        return _fail("state does not report a loaded image")
    if int(state.get("loaded_image_count", 0)) != 2:
        return _fail(
            f"expected 2 supported images from folder, got {state.get('loaded_image_count')}"
        )
    if not bool(state.get("image_list_visible", False)):
        return _fail("Image List did not auto-open for folder queue")

    image_path = Path(str(state.get("image_path", "")))
    if image_path.name != "01_logo.png":
        return _fail(f"unexpected first loaded image: {image_path}")

    print(f"state: {state_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

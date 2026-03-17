#!/usr/bin/env python3
"""Basic smoke regression for the Metal imiv backend.

This runner intentionally avoids screenshot/readback so it can validate the
Metal backend before `renderer_screen_capture()` is implemented.
"""

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
    "MTLCreateSystemDefaultDevice failed",
    "failed to create Metal command queue",
    "failed to create Metal preview texture",
    "Metal preview state is not initialized",
    "Metal renderer state is not initialized",
    "Metal window/device is not initialized",
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


def _default_env_script(repo_root: Path, exe: Path | None = None) -> Path:
    candidates: list[Path] = []
    if exe is not None:
        exe = exe.resolve()
        candidates.extend(
            [
                exe.parent / "imiv_env.sh",
                exe.parent.parent / "imiv_env.sh",
            ]
        )
    candidates.extend(
        [
            repo_root / "build" / "imiv_env.sh",
            repo_root / "build_u" / "imiv_env.sh",
        ]
    )
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


def _run_case(
    repo_root: Path,
    runner: Path,
    exe: Path,
    cwd: Path,
    out_dir: Path,
    name: str,
    env: dict[str, str],
    *,
    open_path: Path | None = None,
    extra_args: list[str] | None = None,
    trace: bool = False,
    want_layout: bool = True,
) -> tuple[dict, dict | None, str]:
    state_path = out_dir / f"{name}.state.json"
    layout_path = out_dir / f"{name}.layout.json"
    log_path = out_dir / f"{name}.log"
    config_home = out_dir / f"cfg_{name}"
    shutil.rmtree(config_home, ignore_errors=True)
    config_home.mkdir(parents=True, exist_ok=True)

    cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(cwd),
        "--state-json-out",
        str(state_path),
        "--post-action-delay-frames",
        "2",
    ]
    if want_layout:
        cmd.extend(
            [
                "--layout-json-out",
                str(layout_path),
                "--layout-items",
            ]
        )
    if open_path is not None:
        cmd.extend(["--open", str(open_path)])
    if trace:
        cmd.append("--trace")
    if extra_args:
        cmd.extend(extra_args)

    case_env = dict(env)
    case_env["IMIV_CONFIG_HOME"] = str(config_home)

    with log_path.open("w", encoding="utf-8") as log_handle:
        proc = subprocess.run(
            cmd,
            cwd=str(repo_root),
            env=case_env,
            check=False,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            timeout=90,
        )
    if proc.returncode != 0:
        raise RuntimeError(f"{name}: runner exited with code {proc.returncode}")
    if not state_path.exists():
        raise RuntimeError(f"{name}: missing state output")
    if want_layout and not layout_path.exists():
        raise RuntimeError(f"{name}: missing layout output")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            raise RuntimeError(f"{name}: found runtime error pattern: {pattern}")

    state = json.loads(state_path.read_text(encoding="utf-8"))
    layout = None
    if want_layout:
        layout = json.loads(layout_path.read_text(encoding="utf-8"))
    return state, layout, log_text


def _has_window(layout: dict, name: str) -> bool:
    return any(window.get("name") == name for window in layout.get("windows", []))


def _area_probe_initialized(state: dict) -> bool:
    lines = state.get("area_probe_lines", [])
    if not lines:
        return False
    for line in lines:
        if line == "Area Probe:":
            continue
        if "-----" in line:
            return False
    return True


def _selection_has_area(state: dict) -> bool:
    bounds = state.get("selection_bounds", [0, 0, 0, 0])
    if len(bounds) != 4:
        return False
    x0, y0, x1, y1 = bounds
    return x1 > x0 and y1 > y0


def main() -> int:
    repo_root = _repo_root()
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--env-script",
        default="",
        help="Optional shell env setup script",
    )
    ap.add_argument("--out-dir", default="", help="Output directory")
    ap.add_argument("--open", default=str(default_image), help="Image to open")
    ap.add_argument("--trace", action="store_true", help="Enable runner tracing")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        print(f"error: binary not found: {exe}", file=sys.stderr)
        return 2

    cwd = Path(args.cwd).resolve() if args.cwd else exe.parent.resolve()
    if args.out_dir:
        out_dir = Path(args.out_dir).resolve()
    else:
        out_dir = exe.parent.parent / "imiv_captures" / "metal_smoke_regression"
    out_dir.mkdir(parents=True, exist_ok=True)

    open_path = Path(args.open).resolve()
    if not open_path.exists():
        print(f"error: image not found: {open_path}", file=sys.stderr)
        return 2

    env_script = (
        Path(args.env_script).resolve()
        if args.env_script
        else _default_env_script(repo_root, exe)
    )
    env = _load_env_from_script(env_script)

    startup_state, startup_layout, _ = _run_case(
        repo_root,
        runner,
        exe,
        cwd,
        out_dir,
        "startup_empty",
        env,
        trace=args.trace,
    )
    if startup_state.get("image_loaded"):
        print("error: startup_empty unexpectedly loaded an image", file=sys.stderr)
        return 1
    if startup_state.get("loaded_image_count") not in (0, None):
        print("error: startup_empty has non-zero loaded_image_count", file=sys.stderr)
        return 1
    if not _has_window(startup_layout, "Image"):
        print("error: startup_empty layout missing Image window", file=sys.stderr)
        return 1

    open_state, open_layout, _ = _run_case(
        repo_root,
        runner,
        exe,
        cwd,
        out_dir,
        "open_image",
        env,
        open_path=open_path,
        trace=args.trace,
    )
    if not open_state.get("image_loaded"):
        print("error: open_image did not load image", file=sys.stderr)
        return 1
    if not open_state.get("image_path"):
        print("error: open_image state missing image_path", file=sys.stderr)
        return 1
    image_size = open_state.get("image_size", [0, 0])
    if len(image_size) != 2 or image_size[0] <= 0 or image_size[1] <= 0:
        print("error: open_image reported invalid image_size", file=sys.stderr)
        return 1
    if not _has_window(open_layout, "Image"):
        print("error: open_image layout missing Image window", file=sys.stderr)
        return 1

    area_state, _, _ = _run_case(
        repo_root,
        runner,
        exe,
        cwd,
        out_dir,
        "area_sample_drag",
        env,
        open_path=open_path,
        extra_args=[
            "--key-chord",
            "ctrl+a",
            "--mouse-pos-image-rel",
            "0.30",
            "0.30",
            "--mouse-drag",
            "120",
            "96",
            "--mouse-drag-button",
            "0",
        ],
        trace=args.trace,
        want_layout=False,
    )
    if not area_state.get("selection_active"):
        print("error: area_sample_drag did not leave a selection", file=sys.stderr)
        return 1
    if not _selection_has_area(area_state):
        print("error: area_sample_drag selection_bounds has no area", file=sys.stderr)
        return 1
    if not _area_probe_initialized(area_state):
        print("error: area_sample_drag did not initialize area_probe_lines", file=sys.stderr)
        return 1

    summary = {
        "backend": "metal",
        "cases": {
            "startup_empty": str(out_dir / "startup_empty.log"),
            "open_image": str(out_dir / "open_image.log"),
            "area_sample_drag": str(out_dir / "area_sample_drag.log"),
        },
    }
    (out_dir / "summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(f"ok: Metal smoke regression outputs are in {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

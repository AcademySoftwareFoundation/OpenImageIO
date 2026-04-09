#!/usr/bin/env python3
"""Regression check for Pixel Closeup suppression during Area Sample drag."""

from __future__ import annotations

import argparse
import json
import os
import shlex
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


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def _load_env_from_script(script_path: Path) -> dict[str, str]:
    env = dict(os.environ)
    if not script_path.exists():
        return env
    proc = subprocess.run(
        ["bash", "-lc", f"source {shlex.quote(str(script_path))} >/dev/null 2>&1; env -0"],
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


def _area_probe_is_placeholder(state: dict) -> bool:
    lines = state.get("area_probe_lines", [])
    if not lines:
        return False
    for line in lines:
        if line == "Area Probe:":
            continue
        if "-----" not in line:
            return False
    return True


def _run_runner(
    cmd: list[str], repo_root: Path, env: dict[str, str], label: str
) -> int:
    proc = subprocess.run(
        cmd, cwd=str(repo_root), env=env, check=False, timeout=120
    )
    if proc.returncode != 0:
        return _fail(f"{label}: runner exited with code {proc.returncode}")
    return 0


def main() -> int:
    repo_root = _repo_root()
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_out_dir = (
        repo_root / "build_u" / "imiv_captures" / "area_probe_closeup_regression"
    )
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
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return _fail(f"binary not found: {exe}")
    cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()
    image_path = Path(args.open).expanduser().resolve()
    if not image_path.exists():
        return _fail(f"image not found: {image_path}")

    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    if not runner.exists():
        return _fail(f"runner not found: {runner}")

    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    probe_state_path = out_dir / "pixel_closeup_probe_state.json"
    screenshot_path = out_dir / "area_probe_closeup.png"
    layout_path = out_dir / "area_probe_closeup.json"
    state_path = out_dir / "area_probe_closeup_state.json"
    svg_path = out_dir / "area_probe_closeup.svg"

    env = _load_env_from_script(Path(args.env_script).expanduser())
    env["IMIV_IMGUI_TEST_ENGINE_SHOW_PIXEL"] = "1"

    probe_cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(cwd),
    ]
    if args.backend:
        probe_cmd.extend(["--backend", args.backend])
    probe_cmd.extend(
        [
            "--open",
            str(image_path),
            "--mouse-pos-image-rel",
            "0.55",
            "0.55",
            "--state-json-out",
            str(probe_state_path),
        ]
    )

    status = _run_runner(probe_cmd, repo_root, env, "pixel closeup probe")
    if status != 0:
        return status

    if not probe_state_path.exists():
        return _fail(f"probe state output not found: {probe_state_path}")

    probe_state = json.loads(probe_state_path.read_text(encoding="utf-8"))
    if not probe_state.get("probe_valid", False):
        return _fail("pixel closeup probe did not become valid")
    probe_pos = probe_state.get("probe_pos", [])
    image_size = probe_state.get("image_size", [])
    if len(probe_pos) != 2 or len(image_size) != 2:
        return _fail("probe state dump missing probe_pos or image_size")
    probe_x = int(probe_pos[0])
    probe_y = int(probe_pos[1])
    image_w = int(image_size[0])
    image_h = int(image_size[1])
    if not (0 <= probe_x < image_w and 0 <= probe_y < image_h):
        return _fail(f"probe position out of bounds: ({probe_x}, {probe_y})")
    if probe_x < max(1, image_w // 10) or probe_y < max(1, image_h // 10):
        return _fail(
            f"probe position unexpectedly near top-left: ({probe_x}, {probe_y})"
        )

    env["IMIV_IMGUI_TEST_ENGINE_SHOW_AREA"] = "1"
    area_cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(cwd),
    ]
    if args.backend:
        area_cmd.extend(["--backend", args.backend])
    area_cmd.extend(
        [
            "--open",
            str(image_path),
            "--mouse-pos-image-rel",
            "0.55",
            "0.55",
            "--mouse-drag-hold",
            "120",
            "80",
            "--mouse-drag-hold-button",
            "0",
            "--mouse-drag-hold-frames",
            "2",
            "--screenshot-out",
            str(screenshot_path),
            "--layout-json-out",
            str(layout_path),
            "--layout-items",
            "--svg-out",
            str(svg_path),
            "--svg-items",
            "--svg-labels",
            "--state-json-out",
            str(state_path),
        ]
    )

    status = _run_runner(area_cmd, repo_root, env, "area probe closeup")
    if status != 0:
        return status

    if not layout_path.exists():
        return _fail(f"layout output not found: {layout_path}")
    if not state_path.exists():
        return _fail(f"state output not found: {state_path}")

    state = json.loads(state_path.read_text(encoding="utf-8"))
    if not state.get("area_probe_drag_active", False):
        return _fail("area probe drag was not active during held-drag capture")

    layout = json.loads(layout_path.read_text(encoding="utf-8"))
    item_debug_labels = []
    for window in layout.get("windows", []):
        for item in window.get("items", []):
            debug = item.get("debug")
            if isinstance(debug, str):
                item_debug_labels.append(debug)

    if not _area_probe_is_placeholder(state):
        return _fail("Area Probe should stay in placeholder mode until drag release")
    if "text: Pixel Closeup overlay" in item_debug_labels:
        return _fail("Pixel Closeup overlay was visible during held-drag capture")

    print(f"screenshot: {screenshot_path}")
    print(f"layout: {layout_path}")
    print(f"state: {state_path}")
    print(f"svg: {svg_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Regression check for Pixel Closeup suppression during Area Sample drag."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from imiv_test_utils import (
    default_binary,
    default_env_script,
    fail,
    load_env_from_script,
    repo_root as imiv_repo_root,
    resolve_run_cwd,
    run_logged_process,
    runner_command,
)


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
    proc = run_logged_process(cmd, cwd=repo_root, env=env, timeout=120)
    if proc.returncode != 0:
        return fail(f"{label}: runner exited with code {proc.returncode}")
    return 0


def main() -> int:
    repo_root = imiv_repo_root()
    env_script_default = default_env_script(repo_root)
    default_out_dir = (
        repo_root / "build_u" / "imiv_captures" / "area_probe_closeup_regression"
    )
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument("--env-script", default=str(env_script_default), help="Optional shell env setup script")
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument("--open", default=str(default_image), help="Image to open")
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
    probe_state_path = out_dir / "pixel_closeup_probe_state.json"
    screenshot_path = out_dir / "area_probe_closeup.png"
    layout_path = out_dir / "area_probe_closeup.json"
    state_path = out_dir / "area_probe_closeup_state.json"
    svg_path = out_dir / "area_probe_closeup.svg"

    env = load_env_from_script(Path(args.env_script).expanduser())
    env["IMIV_IMGUI_TEST_ENGINE_SHOW_PIXEL"] = "1"

    probe_cmd = runner_command(exe, cwd, args.backend)
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
        return fail(f"probe state output not found: {probe_state_path}")

    probe_state = json.loads(probe_state_path.read_text(encoding="utf-8"))
    if not probe_state.get("probe_valid", False):
        return fail("pixel closeup probe did not become valid")
    probe_pos = probe_state.get("probe_pos", [])
    image_size = probe_state.get("image_size", [])
    if len(probe_pos) != 2 or len(image_size) != 2:
        return fail("probe state dump missing probe_pos or image_size")
    probe_x = int(probe_pos[0])
    probe_y = int(probe_pos[1])
    image_w = int(image_size[0])
    image_h = int(image_size[1])
    if not (0 <= probe_x < image_w and 0 <= probe_y < image_h):
        return fail(f"probe position out of bounds: ({probe_x}, {probe_y})")
    if probe_x < max(1, image_w // 10) or probe_y < max(1, image_h // 10):
        return fail(
            f"probe position unexpectedly near top-left: ({probe_x}, {probe_y})"
        )

    env["IMIV_IMGUI_TEST_ENGINE_SHOW_AREA"] = "1"
    area_cmd = runner_command(exe, cwd, args.backend)
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
        return fail(f"layout output not found: {layout_path}")
    if not state_path.exists():
        return fail(f"state output not found: {state_path}")

    state = json.loads(state_path.read_text(encoding="utf-8"))
    if not state.get("area_probe_drag_active", False):
        return fail("area probe drag was not active during held-drag capture")

    layout = json.loads(layout_path.read_text(encoding="utf-8"))
    item_debug_labels = []
    for window in layout.get("windows", []):
        for item in window.get("items", []):
            debug = item.get("debug")
            if isinstance(debug, str):
                item_debug_labels.append(debug)

    if not _area_probe_is_placeholder(state):
        return fail("Area Probe should stay in placeholder mode until drag release")
    if "text: Pixel Closeup overlay" in item_debug_labels:
        return fail("Pixel Closeup overlay was visible during held-drag capture")

    print(f"screenshot: {screenshot_path}")
    print(f"layout: {layout_path}")
    print(f"state: {state_path}")
    print(f"svg: {svg_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

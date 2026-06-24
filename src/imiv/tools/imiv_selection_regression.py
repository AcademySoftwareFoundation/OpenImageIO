#!/usr/bin/env python3
"""Regression check for persistent image selection interactions."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path

from imiv_test_utils import (
    default_binary,
    default_env_script,
    default_oiiotool,
    fail,
    load_env_from_script,
    repo_root as imiv_repo_root,
    resolve_existing_tool,
    resolve_run_cwd,
    run_logged_process,
    runner_command,
)


def _generate_fixture(oiiotool: Path, out_path: Path, width: int, height: int) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    run_logged_process(
        [
            str(oiiotool),
            "--pattern",
            "fill:top=0.15,0.18,0.22,bottom=0.75,0.82,0.95",
            f"{width}x{height}",
            "3",
            "-d",
            "uint8",
            "-o",
            str(out_path),
        ],
        cwd=out_path.parent,
        check=True,
    )


def _run_case(
    repo_root: Path,
    exe: Path,
    cwd: Path,
    image_path: Path,
    out_dir: Path,
    name: str,
    extra_args: list[str],
    env: dict[str, str],
    extra_env: dict[str, str] | None,
    trace: bool,
    want_layout: bool = False,
) -> tuple[dict, dict | None]:
    state_path = out_dir / f"{name}.state.json"
    layout_path = out_dir / f"{name}.layout.json"
    log_path = out_dir / f"{name}.log"
    config_home = out_dir / f"cfg_{name}"
    shutil.rmtree(config_home, ignore_errors=True)

    cmd = runner_command(exe, cwd)
    cmd.extend(
        [
            "--open",
            str(image_path),
            "--state-json-out",
            str(state_path),
            "--post-action-delay-frames",
            "2",
        ]
    )
    if want_layout:
        cmd.extend(["--layout-json-out", str(layout_path), "--layout-items"])
    if trace:
        cmd.append("--trace")
    cmd.extend(extra_args)

    case_env = dict(env)
    case_env["IMIV_CONFIG_HOME"] = str(config_home)
    if extra_env:
        case_env.update(extra_env)

    proc = run_logged_process(
        cmd, cwd=repo_root, env=case_env, timeout=90, log_path=log_path
    )
    if proc.returncode != 0:
        raise RuntimeError(f"{name}: runner exited with code {proc.returncode}")
    if not state_path.exists():
        raise RuntimeError(f"{name}: state file not written")

    state = json.loads(state_path.read_text(encoding="utf-8"))
    state["_state_path"] = str(state_path)
    state["_log_path"] = str(log_path)

    layout = None
    if want_layout:
        if not layout_path.exists():
            raise RuntimeError(f"{name}: layout file not written")
        layout = json.loads(layout_path.read_text(encoding="utf-8"))

    return state, layout


def _layout_has_debug_label(layout: dict | None, label: str) -> bool:
    if layout is None:
        return False
    for window in layout.get("windows", []):
        for item in window.get("items", []):
            if item.get("debug") == label:
                return True
    return False


def _area_probe_is_initialized(state: dict) -> bool:
    for line in state.get("area_probe_lines", []):
        if "-----" in line:
            return False
    return True


def _area_probe_is_reset(state: dict) -> bool:
    return all(
        "-----" in line or line == "Area Probe:"
        for line in state.get("area_probe_lines", [])
    )


def main() -> int:
    repo_root = imiv_repo_root()
    env_script_default = default_env_script(repo_root)
    default_out_dir = repo_root / "build_u" / "imiv_captures" / "selection_regression"
    default_drag_image = default_out_dir / "selection_drag_input.tif"
    default_pan_image = default_out_dir / "selection_pan_input.tif"
    default_viewport_image = default_out_dir / "selection_viewport_input.tif"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--oiiotool", default=str(default_oiiotool(repo_root)), help="oiiotool executable"
    )
    ap.add_argument(
        "--env-script",
        default=str(env_script_default),
        help="Optional shell env setup script",
    )
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument(
        "--drag-image",
        default=str(default_drag_image),
        help="Generated fixture used for drag/image-click selection cases",
    )
    ap.add_argument(
        "--pan-image",
        default=str(default_pan_image),
        help="Generated large fixture used for pan/no-selection case",
    )
    ap.add_argument(
        "--viewport-image",
        default=str(default_viewport_image),
        help="Generated fixture used for empty-viewport deselect case",
    )
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")
    oiiotool = resolve_existing_tool(args.oiiotool, default_oiiotool(repo_root))
    if not oiiotool.exists():
        return fail(f"oiiotool not found: {oiiotool}")

    cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    drag_image = Path(args.drag_image).expanduser().resolve()
    pan_image = Path(args.pan_image).expanduser().resolve()
    viewport_image = Path(args.viewport_image).expanduser().resolve()

    try:
        _generate_fixture(oiiotool, drag_image, 320, 240)
        _generate_fixture(oiiotool, pan_image, 3072, 2048)
        _generate_fixture(oiiotool, viewport_image, 320, 32)
    except subprocess.SubprocessError as exc:
        return fail(f"failed to generate selection fixtures: {exc}")

    env = load_env_from_script(Path(args.env_script).expanduser())
    area_env = {"IMIV_IMGUI_TEST_ENGINE_SHOW_AREA": "1"}

    try:
        drag_state, drag_layout = _run_case(
            repo_root,
            exe,
            cwd,
            drag_image,
            out_dir,
            "drag_select",
            [
                "--mouse-pos-image-rel",
                "0.25",
                "0.35",
                "--mouse-drag",
                "120",
                "90",
                "--mouse-drag-button",
                "0",
            ],
            env,
            area_env,
            args.trace,
            want_layout=True,
        )
        select_all_state, _ = _run_case(
            repo_root,
            exe,
            cwd,
            drag_image,
            out_dir,
            "select_all",
            ["--key-chord", "ctrl+shift+a"],
            env,
            area_env,
            args.trace,
        )
        deselect_image_state, _ = _run_case(
            repo_root,
            exe,
            cwd,
            drag_image,
            out_dir,
            "deselect_image_click",
            [
                "--key-chord",
                "ctrl+shift+a",
                "--mouse-pos-image-rel",
                "0.50",
                "0.50",
                "--mouse-click",
                "0",
            ],
            env,
            area_env,
            args.trace,
        )
        deselect_viewport_state, _ = _run_case(
            repo_root,
            exe,
            cwd,
            viewport_image,
            out_dir,
            "deselect_viewport_click",
            [
                "--key-chord",
                "ctrl+shift+a",
                "--mouse-pos-window-rel",
                "0.50",
                "0.95",
                "--mouse-click",
                "0",
            ],
            env,
            area_env,
            args.trace,
        )
        area_sample_pan_baseline_state, _ = _run_case(
            repo_root,
            exe,
            cwd,
            pan_image,
            out_dir,
            "area_sample_pan_baseline",
            [],
            env,
            area_env,
            args.trace,
        )
        toggle_area_off_pan_state, _ = _run_case(
            repo_root,
            exe,
            cwd,
            pan_image,
            out_dir,
            "toggle_area_off_left_drag_pan",
            [
                "--key-chord",
                "ctrl+a",
                "--mouse-pos-image-rel",
                "0.50",
                "0.50",
                "--mouse-drag",
                "180",
                "120",
                "--mouse-drag-button",
                "0",
            ],
            env,
            area_env,
            args.trace,
        )
    except (RuntimeError, subprocess.SubprocessError) as exc:
        return fail(str(exc))

    for name, state in (
        ("drag_select", drag_state),
        ("select_all", select_all_state),
        ("deselect_image_click", deselect_image_state),
        ("deselect_viewport_click", deselect_viewport_state),
        ("area_sample_pan_baseline", area_sample_pan_baseline_state),
        ("toggle_area_off_left_drag_pan", toggle_area_off_pan_state),
    ):
        if not state.get("image_loaded", False):
            return fail(f"{name}: image not loaded")

    if not drag_state.get("selection_active", False):
        return fail("drag_select: selection was not created")
    drag_bounds = drag_state.get("selection_bounds", [])
    if len(drag_bounds) != 4:
        return fail("drag_select: selection bounds missing")
    if not (
        int(drag_bounds[2]) > int(drag_bounds[0])
        and int(drag_bounds[3]) > int(drag_bounds[1])
    ):
        return fail(f"drag_select: selection bounds are empty: {drag_bounds}")
    if not _layout_has_debug_label(drag_layout, "rect: Image selection overlay"):
        return fail("drag_select: selection overlay was not present in layout dump")
    if not _area_probe_is_initialized(drag_state):
        return fail("drag_select: area probe statistics were not initialized")

    image_size = select_all_state.get("image_size", [])
    if len(image_size) != 2:
        return fail("select_all: image size missing")
    expected_bounds = [0, 0, int(image_size[0]), int(image_size[1])]
    if not select_all_state.get("selection_active", False):
        return fail("select_all: selection is not active")
    if [int(v) for v in select_all_state.get("selection_bounds", [])] != expected_bounds:
        return fail(
            "select_all: wrong selection bounds: "
            f"expected {expected_bounds}, got {select_all_state.get('selection_bounds')}"
        )
    if not _area_probe_is_initialized(select_all_state):
        return fail("select_all: area probe statistics were not initialized")

    if deselect_image_state.get("selection_active", False):
        return fail("deselect_image_click: selection remained active")
    if any(int(v) != 0 for v in deselect_image_state.get("selection_bounds", [])):
        return fail(
            "deselect_image_click: selection bounds were not cleared: "
            f"{deselect_image_state.get('selection_bounds')}"
        )
    if not _area_probe_is_reset(deselect_image_state):
        return fail("deselect_image_click: area probe was not reset")

    if deselect_viewport_state.get("selection_active", False):
        return fail("deselect_viewport_click: selection remained active")
    if any(int(v) != 0 for v in deselect_viewport_state.get("selection_bounds", [])):
        return fail(
            "deselect_viewport_click: selection bounds were not cleared: "
            f"{deselect_viewport_state.get('selection_bounds')}"
        )
    if not _area_probe_is_reset(deselect_viewport_state):
        return fail("deselect_viewport_click: area probe was not reset")

    if toggle_area_off_pan_state.get("selection_active", False):
        return fail("toggle_area_off_left_drag_pan: selection became active")
    if any(int(v) != 0 for v in toggle_area_off_pan_state.get("selection_bounds", [])):
        return fail(
            "toggle_area_off_left_drag_pan: selection bounds were not cleared: "
            f"{toggle_area_off_pan_state.get('selection_bounds')}"
        )
    if not _area_probe_is_reset(toggle_area_off_pan_state):
        return fail("toggle_area_off_left_drag_pan: area probe was not reset")

    baseline_scroll = [
        float(v) for v in area_sample_pan_baseline_state.get("scroll", [0.0, 0.0])
    ]
    toggle_pan_scroll = [
        float(v) for v in toggle_area_off_pan_state.get("scroll", [0.0, 0.0])
    ]
    if len(baseline_scroll) != 2 or len(toggle_pan_scroll) != 2:
        return fail("toggle_area_off_left_drag_pan: scroll state missing")
    if baseline_scroll == toggle_pan_scroll:
        return fail(
            "toggle_area_off_left_drag_pan: left drag did not pan the image: "
            f"baseline={baseline_scroll}, after_drag={toggle_pan_scroll}"
        )

    print("drag_select:", drag_state["_state_path"])
    print("select_all:", select_all_state["_state_path"])
    print("deselect_image_click:", deselect_image_state["_state_path"])
    print("deselect_viewport_click:", deselect_viewport_state["_state_path"])
    print(
        "area_sample_pan_baseline:",
        area_sample_pan_baseline_state["_state_path"],
    )
    print("toggle_area_off_left_drag_pan:", toggle_area_off_pan_state["_state_path"])
    print("artifacts:", out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

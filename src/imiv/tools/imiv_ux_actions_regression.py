#!/usr/bin/env python3
"""Regression check for combined imiv UX actions in a single app run."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from imiv_test_utils import (
    default_binary,
    default_env_script,
    default_image,
    default_oiiotool,
    fail,
    load_env_from_script,
    repo_root as imiv_repo_root,
    resolve_existing_tool,
    resolve_run_cwd,
    run_logged_process,
    runner_command,
    runner_path,
)


_fail = fail

def _generate_logo_fixture(oiiotool: Path, source_path: Path, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    run_logged_process(
        [
            str(oiiotool),
            str(source_path),
            "--resize",
            "2200x1547",
            "-o",
            str(out_path),
        ],
        cwd=out_path.parent,
        check=True,
    )


def _scenario_step(
    root: ET.Element, name: str, **attrs: str | int | bool
) -> None:
    step = ET.SubElement(root, "step")
    step.set("name", name)
    for key, value in attrs.items():
        if isinstance(value, bool):
            step.set(key, "true" if value else "false")
        else:
            step.set(key, str(value))


def _write_scenario(path: Path, runtime_dir_rel: str) -> None:
    root = ET.Element("imiv-scenario")
    root.set("out_dir", runtime_dir_rel)

    _scenario_step(
        root,
        "select_drag",
        key_chord="ctrl+a",
        mouse_pos_image_rel="0.18,0.25",
        mouse_drag="180,120",
        mouse_drag_button=0,
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "reselect_drag",
        mouse_pos_image_rel="0.58,0.52",
        mouse_drag="160,110",
        mouse_drag_button=0,
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "select_all",
        key_chord="ctrl+shift+a",
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "deselect_shortcut",
        key_chord="ctrl+d",
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "select_all_again",
        key_chord="ctrl+shift+a",
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "deselect_outside_click",
        mouse_pos_window_rel="0.98,0.50",
        mouse_click_button=0,
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "select_all_third",
        key_chord="ctrl+shift+a",
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "deselect_image_click",
        mouse_pos_image_rel="0.50,0.50",
        mouse_click_button=0,
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "area_sample_off",
        key_chord="ctrl+a",
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "normal_size",
        key_chord="ctrl+0",
        state=True,
        post_action_delay_frames=3,
    )
    _scenario_step(
        root,
        "pan_left_drag",
        mouse_pos_image_rel="0.50,0.50",
        mouse_drag="-220,120",
        mouse_drag_button=0,
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "recenter",
        key_chord="ctrl+period",
        state=True,
        post_action_delay_frames=3,
    )
    _scenario_step(
        root,
        "zoom_in_right_drag",
        mouse_pos_image_rel="0.50,0.50",
        mouse_drag="0,120",
        mouse_drag_button=1,
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "zoom_out_right_drag",
        mouse_pos_image_rel="0.50,0.50",
        mouse_drag="0,-120",
        mouse_drag_button=1,
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "fit_image_to_window",
        key_chord="alt+f",
        state=True,
        post_action_delay_frames=3,
    )
    _scenario_step(
        root,
        "zoom_in_shortcut_after_fit",
        key_chord="ctrl+shift+equal",
        state=True,
        post_action_delay_frames=3,
    )
    _scenario_step(
        root,
        "zoom_in_wheel_after_fit",
        mouse_pos_image_rel="0.50,0.50",
        mouse_wheel="0,1",
        state=True,
        post_action_delay_frames=3,
    )

    tree = ET.ElementTree(root)
    path.parent.mkdir(parents=True, exist_ok=True)
    tree.write(path, encoding="utf-8", xml_declaration=True)


def _load_state(path: Path) -> dict:
    if not path.exists():
        raise RuntimeError(f"state file not written: {path}")
    state = json.loads(path.read_text(encoding="utf-8"))
    state["_state_path"] = str(path)
    return state


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


def _selection_is_cleared(state: dict) -> bool:
    return (not state.get("selection_active", False)) and all(
        int(v) == 0 for v in state.get("selection_bounds", [])
    )


def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)
    default_out_dir = repo_root / "build_u" / "imiv_captures" / "ux_actions_regression"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument(
        "--oiiotool", default=str(default_oiiotool(repo_root)), help="oiiotool executable"
    )
    ap.add_argument(
        "--env-script",
        default=str(default_env_script(repo_root)),
        help="Optional shell env setup script",
    )
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument(
        "--image",
        default=str(default_image(repo_root)),
        help="Generated panoramic fixture used for the UX scenario",
    )
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")
    oiiotool = resolve_existing_tool(args.oiiotool, default_oiiotool(repo_root))
    if not oiiotool.exists():
        return fail(f"oiiotool not found: {oiiotool}")
    if not runner.exists():
        return fail(f"runner not found: {runner}")

    cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = Path(args.out_dir).expanduser().resolve()
    runtime_dir = out_dir / "runtime"
    source_image_path = Path(args.image).expanduser().resolve()
    image_path = out_dir / "ux_actions_input.png"
    scenario_path = out_dir / "ux_actions.scenario.xml"
    log_path = out_dir / "ux_actions.log"
    config_home = out_dir / "cfg"

    shutil.rmtree(runtime_dir, ignore_errors=True)
    shutil.rmtree(config_home, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    if not source_image_path.exists():
        return fail(f"image not found: {source_image_path}")
    try:
        _generate_logo_fixture(oiiotool, source_image_path, image_path)
    except subprocess.SubprocessError as exc:
        return fail(f"failed to generate logo fixture: {exc}")

    runtime_dir_rel = os.path.relpath(runtime_dir, cwd)
    _write_scenario(scenario_path, runtime_dir_rel)

    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(["--open", str(image_path), "--scenario", str(scenario_path)])
    if args.trace:
        cmd.append("--trace")

    env = load_env_from_script(Path(args.env_script).expanduser())
    env["IMIV_CONFIG_HOME"] = str(config_home)

    proc = run_logged_process(
        cmd,
        cwd=repo_root,
        env=env,
        log_path=log_path,
        timeout=180,
    )
    if proc.returncode != 0:
        return fail(f"runner exited with code {proc.returncode}: {log_path}")

    try:
        select_drag = _load_state(runtime_dir / "select_drag.state.json")
        reselect_drag = _load_state(runtime_dir / "reselect_drag.state.json")
        select_all = _load_state(runtime_dir / "select_all.state.json")
        deselect_shortcut = _load_state(runtime_dir / "deselect_shortcut.state.json")
        select_all_again = _load_state(runtime_dir / "select_all_again.state.json")
        deselect_image_click = _load_state(
            runtime_dir / "deselect_image_click.state.json"
        )
        select_all_third = _load_state(runtime_dir / "select_all_third.state.json")
        deselect_outside_click = _load_state(
            runtime_dir / "deselect_outside_click.state.json"
        )
        area_sample_off = _load_state(runtime_dir / "area_sample_off.state.json")
        normal_size = _load_state(runtime_dir / "normal_size.state.json")
        pan_left_drag = _load_state(runtime_dir / "pan_left_drag.state.json")
        recenter = _load_state(runtime_dir / "recenter.state.json")
        zoom_in_right_drag = _load_state(
            runtime_dir / "zoom_in_right_drag.state.json"
        )
        zoom_out_right_drag = _load_state(
            runtime_dir / "zoom_out_right_drag.state.json"
        )
        fit_image_to_window = _load_state(
            runtime_dir / "fit_image_to_window.state.json"
        )
        zoom_in_shortcut_after_fit = _load_state(
            runtime_dir / "zoom_in_shortcut_after_fit.state.json"
        )
        zoom_in_wheel_after_fit = _load_state(
            runtime_dir / "zoom_in_wheel_after_fit.state.json"
        )
    except RuntimeError as exc:
        return _fail(str(exc))

    states = (
        ("select_drag", select_drag),
        ("reselect_drag", reselect_drag),
        ("select_all", select_all),
        ("deselect_shortcut", deselect_shortcut),
        ("select_all_again", select_all_again),
        ("deselect_image_click", deselect_image_click),
        ("select_all_third", select_all_third),
        ("deselect_outside_click", deselect_outside_click),
        ("area_sample_off", area_sample_off),
        ("normal_size", normal_size),
        ("pan_left_drag", pan_left_drag),
        ("recenter", recenter),
        ("zoom_in_right_drag", zoom_in_right_drag),
        ("zoom_out_right_drag", zoom_out_right_drag),
        ("fit_image_to_window", fit_image_to_window),
        ("zoom_in_shortcut_after_fit", zoom_in_shortcut_after_fit),
        ("zoom_in_wheel_after_fit", zoom_in_wheel_after_fit),
    )
    for name, state in states:
        if not state.get("image_loaded", False):
            return _fail(f"{name}: image not loaded")

    if not select_drag.get("selection_active", False):
        return _fail("select_drag: selection was not created")
    first_bounds = [int(v) for v in select_drag.get("selection_bounds", [])]
    if len(first_bounds) != 4 or not (
        first_bounds[2] > first_bounds[0] and first_bounds[3] > first_bounds[1]
    ):
        return _fail(f"select_drag: invalid selection bounds: {first_bounds}")
    if not _area_probe_is_initialized(select_drag):
        return _fail("select_drag: area probe statistics were not initialized")

    second_bounds = [int(v) for v in reselect_drag.get("selection_bounds", [])]
    if second_bounds == first_bounds:
        return _fail("reselect_drag: selection bounds did not change")
    if not reselect_drag.get("selection_active", False):
        return _fail("reselect_drag: selection is not active")

    image_size = select_all.get("image_size", [])
    if len(image_size) != 2:
        return _fail("select_all: image size missing")
    expected_select_all = [0, 0, int(image_size[0]), int(image_size[1])]
    if [int(v) for v in select_all.get("selection_bounds", [])] != expected_select_all:
        return _fail(
            "select_all: wrong selection bounds: "
            f"expected {expected_select_all}, got {select_all.get('selection_bounds')}"
        )
    if not _area_probe_is_initialized(select_all):
        return _fail("select_all: area probe statistics were not initialized")

    if not _selection_is_cleared(deselect_shortcut):
        return _fail("deselect_shortcut: selection was not cleared")
    if not _area_probe_is_reset(deselect_shortcut):
        return _fail("deselect_shortcut: area probe was not reset")

    if [int(v) for v in select_all_again.get("selection_bounds", [])] != expected_select_all:
        return _fail("select_all_again: select all did not replace the selection")

    if not _selection_is_cleared(deselect_image_click):
        return _fail("deselect_image_click: selection was not cleared")
    if not _area_probe_is_reset(deselect_image_click):
        return _fail("deselect_image_click: area probe was not reset")

    if [int(v) for v in select_all_third.get("selection_bounds", [])] != expected_select_all:
        return _fail("select_all_third: select all did not replace the selection")

    if not _selection_is_cleared(deselect_outside_click):
        return _fail("deselect_outside_click: selection was not cleared")
    if not _area_probe_is_reset(deselect_outside_click):
        return _fail("deselect_outside_click: area probe was not reset")

    if not _selection_is_cleared(area_sample_off):
        return _fail("area_sample_off: selection remained active")
    if not _area_probe_is_reset(area_sample_off):
        return _fail("area_sample_off: area probe was not reset")

    normal_zoom = float(normal_size.get("zoom", 0.0))
    if abs(normal_zoom - 1.0) > 1.0e-3:
        return _fail(f"normal_size: expected zoom 1.0, got {normal_zoom:.6f}")
    if bool(normal_size.get("fit_image_to_window", False)):
        return _fail("normal_size: fit_image_to_window remained enabled")

    normal_scroll = [float(v) for v in normal_size.get("scroll", [0.0, 0.0])]
    panned_scroll = [float(v) for v in pan_left_drag.get("scroll", [0.0, 0.0])]
    if len(normal_scroll) != 2 or len(panned_scroll) != 2:
        return _fail("pan_left_drag: scroll data missing")
    if abs(panned_scroll[0] - normal_scroll[0]) <= 1.0:
        return _fail(
            "pan_left_drag: left drag did not pan the image enough: "
            f"baseline={normal_scroll}, panned={panned_scroll}"
        )
    if pan_left_drag.get("selection_active", False):
        return _fail("pan_left_drag: selection became active with area sample off")

    recenter_scroll = [float(v) for v in recenter.get("norm_scroll", [0.0, 0.0])]
    if len(recenter_scroll) != 2:
        return _fail("recenter: normalized scroll missing")
    if abs(recenter_scroll[0] - 0.5) > 0.1 or abs(recenter_scroll[1] - 0.5) > 0.1:
        return _fail(
            f"recenter: expected normalized scroll near [0.5, 0.5], got {recenter_scroll}"
        )

    zoom_in = float(zoom_in_right_drag.get("zoom", 0.0))
    zoom_out = float(zoom_out_right_drag.get("zoom", 0.0))
    recenter_zoom = float(recenter.get("zoom", 0.0))
    if zoom_in <= recenter_zoom + 1.0e-3:
        return _fail(
            "zoom_in_right_drag: zoom did not increase: "
            f"before={recenter_zoom:.6f}, after={zoom_in:.6f}"
        )
    if zoom_out >= zoom_in - 1.0e-3:
        return _fail(
            "zoom_out_right_drag: zoom did not decrease: "
            f"before={zoom_in:.6f}, after={zoom_out:.6f}"
        )

    fit_zoom = float(fit_image_to_window.get("zoom", 0.0))
    if not bool(fit_image_to_window.get("fit_image_to_window", False)):
        return _fail("fit_image_to_window: fit flag was not enabled")
    if fit_zoom >= normal_zoom - 1.0e-3:
        return _fail(
            "fit_image_to_window: expected fit zoom smaller than 1:1: "
            f"fit={fit_zoom:.6f}, normal={normal_zoom:.6f}"
        )

    shortcut_zoom = float(zoom_in_shortcut_after_fit.get("zoom", 0.0))
    if shortcut_zoom <= fit_zoom + 1.0e-3:
        return _fail(
            "zoom_in_shortcut_after_fit: zoom did not increase from fit: "
            f"fit={fit_zoom:.6f}, shortcut={shortcut_zoom:.6f}"
        )
    if bool(zoom_in_shortcut_after_fit.get("fit_image_to_window", False)):
        return _fail(
            "zoom_in_shortcut_after_fit: fit_image_to_window remained enabled"
        )

    wheel_zoom = float(zoom_in_wheel_after_fit.get("zoom", 0.0))
    if wheel_zoom <= shortcut_zoom + 1.0e-3:
        return _fail(
            "zoom_in_wheel_after_fit: mouse wheel did not increase zoom: "
            f"shortcut={shortcut_zoom:.6f}, wheel={wheel_zoom:.6f}"
        )
    if bool(zoom_in_wheel_after_fit.get("fit_image_to_window", False)):
        return _fail(
            "zoom_in_wheel_after_fit: fit_image_to_window remained enabled"
        )

    print("select_drag:", select_drag["_state_path"])
    print("reselect_drag:", reselect_drag["_state_path"])
    print("normal_size:", normal_size["_state_path"])
    print("fit_image_to_window:", fit_image_to_window["_state_path"])
    print(
        "zoom_in_shortcut_after_fit:",
        zoom_in_shortcut_after_fit["_state_path"],
    )
    print("zoom_in_wheel_after_fit:", zoom_in_wheel_after_fit["_state_path"])
    print("artifacts:", out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Regression check for GUI-driven Save Selection As crop export."""

from __future__ import annotations

import argparse
import json
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

from imiv_test_utils import (
    default_binary,
    default_env_script,
    default_idiff,
    default_oiiotool,
    fail,
    load_env_from_script,
    path_for_imiv_output,
    repo_root as imiv_repo_root,
    resolve_existing_tool,
    resolve_run_cwd,
    run_captured_process,
    runner_command,
    runner_path,
)


def _scenario_step(root: ET.Element, name: str, **attrs: str | int | bool) -> None:
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
    root.set("layout_items", "true")

    _scenario_step(
        root,
        "enable_area_sample",
        key_chord="ctrl+a",
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "select_drag",
        mouse_pos_image_rel="0.18,0.25",
        mouse_drag="180,120",
        mouse_drag_button=0,
        state=True,
        post_action_delay_frames=3,
    )
    _scenario_step(
        root,
        "save_selection",
        key_chord="ctrl+alt+s",
        state=True,
        post_action_delay_frames=6,
    )

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"
    default_out_dir = repo_root / "build" / "imiv_captures" / "save_selection_regression"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument("--backend", default="", help="Optional runtime backend override")
    ap.add_argument("--oiiotool", default=str(default_oiiotool(repo_root)), help="oiiotool executable")
    ap.add_argument("--idiff", default=str(default_idiff(repo_root)), help="idiff executable")
    ap.add_argument("--env-script", default=str(default_env_script(repo_root)), help="Optional shell env setup script")
    ap.add_argument("--image", default=str(default_image), help="Input image path")
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve(strict=False)
    if not runner.exists():
        return fail(f"runner not found: {runner}")
    image = Path(args.image).expanduser().resolve()
    if not image.exists():
        return fail(f"image not found: {image}")

    oiiotool = resolve_existing_tool(args.oiiotool, default_oiiotool(repo_root))
    if not oiiotool.exists():
        return fail(f"oiiotool not found: {oiiotool}")
    idiff = resolve_existing_tool(args.idiff, default_idiff(repo_root))
    if not idiff.exists():
        return fail(f"idiff not found: {idiff}")

    cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = Path(args.out_dir).expanduser().resolve()
    runtime_dir = out_dir / "runtime"
    scenario_path = out_dir / "save_selection.scenario.xml"
    select_state_path = runtime_dir / "select_drag.state.json"
    save_state_path = runtime_dir / "save_selection.state.json"
    log_path = out_dir / "save_selection.log"
    fixture_path = out_dir / "save_selection_input.png"
    saved_path = out_dir / "saved_selection.tif"
    expected_path = out_dir / "expected_selection.tif"

    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    prep = run_captured_process(
        [
            str(oiiotool),
            str(image),
            "--resize",
            "2200x1547",
            "-o",
            str(fixture_path),
        ],
        cwd=repo_root,
    )
    if prep.returncode != 0:
        print(prep.stdout, end="")
        return fail("failed to prepare save-selection fixture")

    env = load_env_from_script(Path(args.env_script).expanduser())
    config_home = out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)
    env["IMIV_TEST_SAVE_IMAGE_PATH"] = str(saved_path)

    _write_scenario(scenario_path, path_for_imiv_output(runtime_dir, cwd))
    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(["--open", str(fixture_path), "--scenario", str(scenario_path)])
    if args.trace:
        cmd.append("--trace")

    proc = run_captured_process(cmd, cwd=repo_root, env=env)
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        print(proc.stdout, end="")
        return fail(f"runner exited with code {proc.returncode}")

    if not select_state_path.exists():
        return fail(f"selection state output not found: {select_state_path}")
    if not save_state_path.exists():
        return fail(f"save state output not found: {save_state_path}")
    if not saved_path.exists():
        return fail(f"saved selection output not found: {saved_path}")

    select_state = json.loads(select_state_path.read_text(encoding="utf-8"))
    bounds = [int(v) for v in select_state.get("selection_bounds", [])]
    if len(bounds) != 4:
        return fail("selection_bounds missing from selection state")
    xbegin, ybegin, xend, yend = bounds
    if xend <= xbegin or yend <= ybegin:
        return fail(f"invalid selection bounds: {bounds}")

    expected = run_captured_process(
        [
            str(oiiotool),
            str(fixture_path),
            "--cut",
            f"{xend - xbegin}x{yend - ybegin}+{xbegin}+{ybegin}",
            "-o",
            str(expected_path),
        ],
        cwd=repo_root,
    )
    if expected.returncode != 0:
        print(expected.stdout, end="")
        return fail("failed to generate expected crop")

    diff = run_captured_process(
        [str(idiff), "-q", "-a", str(expected_path), str(saved_path)],
        cwd=repo_root,
    )
    if diff.returncode != 0:
        print(diff.stdout, end="")
        return fail("saved selection did not match expected crop")

    print(f"fixture: {fixture_path}")
    print(f"select_state: {select_state_path}")
    print(f"save_state: {save_state_path}")
    print(f"saved: {saved_path}")
    print(f"expected: {expected_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Regression check for GUI-driven Export As OCIO export."""

from __future__ import annotations

import argparse
import json
import math
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


def _scenario_step(root: ET.Element, name: str, **attrs: str | int | float | bool) -> None:
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
        "enable_ocio_recipe",
        ocio_use=True,
        ocio_image_color_space="ACEScg",
        exposure=0.0,
        gamma=1.0,
        offset=0.0,
        state=True,
        post_action_delay_frames=4,
    )
    _scenario_step(
        root,
        "save_window",
        key_chord="ctrl+shift+s",
        state=True,
        post_action_delay_frames=6,
    )

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _assert_close(actual: float, expected: float, name: str) -> None:
    if not math.isclose(actual, expected, rel_tol=1.0e-5, abs_tol=1.0e-5):
        raise AssertionError(f"{name} mismatch: expected {expected}, got {actual}")


def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)
    default_image = repo_root / "testsuite" / "imiv" / "images" / "CC988_ACEScg.exr"
    default_out_dir = repo_root / "build" / "imiv_captures" / "save_window_ocio_regression"

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
    scenario_path = out_dir / "save_window_ocio.scenario.xml"
    recipe_state_path = runtime_dir / "enable_ocio_recipe.state.json"
    save_state_path = runtime_dir / "save_window.state.json"
    log_path = out_dir / "save_window_ocio.log"
    saved_path = out_dir / "saved_window_ocio.tif"
    expected_path = out_dir / "expected_window_ocio.tif"

    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    env = load_env_from_script(Path(args.env_script).expanduser())
    config_home = out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)
    env["IMIV_TEST_SAVE_IMAGE_PATH"] = str(saved_path)

    _write_scenario(scenario_path, path_for_imiv_output(runtime_dir, cwd))
    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(["--open", str(image), "--scenario", str(scenario_path)])
    if args.trace:
        cmd.append("--trace")

    proc = run_captured_process(cmd, cwd=repo_root, env=env)
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        print(proc.stdout, end="")
        return fail(f"runner exited with code {proc.returncode}")

    if not recipe_state_path.exists():
        return fail(f"recipe state output not found: {recipe_state_path}")
    if not save_state_path.exists():
        return fail(f"save state output not found: {save_state_path}")
    if not saved_path.exists():
        return fail(f"saved window output not found: {saved_path}")

    recipe_state = json.loads(recipe_state_path.read_text(encoding="utf-8"))
    recipe = recipe_state.get("view_recipe", {})
    ocio = recipe_state.get("ocio", {})
    try:
        _assert_close(float(recipe.get("exposure", 0.0)), 0.0, "window exposure")
        _assert_close(float(recipe.get("gamma", 0.0)), 1.0, "window gamma")
        _assert_close(float(recipe.get("offset", 0.0)), 0.0, "window offset")
    except (TypeError, ValueError, AssertionError) as exc:
        return fail(str(exc))
    if not bool(recipe.get("use_ocio", False)):
        return fail("window export did not keep OCIO enabled")

    resolved_display = str(ocio.get("resolved_display", "")).strip()
    resolved_view = str(ocio.get("resolved_view", "")).strip()
    if not resolved_display or not resolved_view:
        return fail("resolved OCIO display/view missing from state output")

    expect = run_captured_process(
        [
            str(oiiotool),
            str(image),
            "--ociodisplay:from=ACEScg",
            resolved_display,
            resolved_view,
            "-d",
            "float",
            "-o",
            str(expected_path),
        ],
        cwd=repo_root,
    )
    if expect.returncode != 0:
        print(expect.stdout, end="")
        return fail("failed to prepare expected OCIO save-window output")

    diff = run_captured_process(
        [
            str(idiff),
            "-q",
            "-a",
            str(expected_path),
            str(saved_path),
        ],
        cwd=repo_root,
    )
    if diff.returncode != 0:
        print(diff.stdout, end="")
        return fail("saved OCIO window output does not match expected recipe")

    print(f"recipe_state: {recipe_state_path}")
    print(f"save_state: {save_state_path}")
    print(f"saved: {saved_path}")
    print(f"expected: {expected_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Regression check for independent per-view preview recipe state."""

from __future__ import annotations

import argparse
import json
import math
import shutil
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

from imiv_test_utils import (
    default_binary,
    default_env_script,
    fail,
    load_env_from_script,
    path_for_imiv_output,
    repo_root as imiv_repo_root,
    resolve_run_cwd,
    run_captured_process,
    runner_command,
    runner_path,
)


def _load_json(path: Path) -> dict:
    if not path.exists():
        raise FileNotFoundError(path)
    return json.loads(path.read_text(encoding="utf-8"))


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
        "open_second_in_new_view",
        image_list_open_new_view_index=1,
        state=True,
        post_action_delay_frames=4,
    )
    _scenario_step(
        root,
        "tune_second_view",
        exposure=1.25,
        gamma=1.75,
        offset=0.125,
        ocio_use=True,
        linear_interpolation=True,
        state=True,
        post_action_delay_frames=4,
    )
    _scenario_step(
        root,
        "activate_primary_view",
        view_activate_index=0,
        state=True,
        post_action_delay_frames=4,
    )
    _scenario_step(
        root,
        "activate_secondary_view",
        view_activate_index=1,
        state=True,
        post_action_delay_frames=4,
    )

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _assert_close(actual: float, expected: float, name: str) -> None:
    if not math.isclose(actual, expected, rel_tol=1.0e-5, abs_tol=1.0e-5):
        raise AssertionError(f"{name} mismatch: expected {expected}, got {actual}")


def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)
    env_script_default = default_env_script(repo_root)
    default_out_dir = repo_root / "build" / "imiv_captures" / "view_recipe_regression"
    default_images = [
        repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png",
        repo_root / "testsuite" / "imiv" / "images" / "CC988_ACEScg.exr",
    ]

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument(
        "--env-script",
        default=str(env_script_default),
        help="Optional shell env setup script",
    )
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument(
        "--image",
        dest="images",
        action="append",
        default=[],
        help="Startup image path; may be repeated",
    )
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve(strict=False)
    if not runner.exists():
        return fail(f"runner not found: {runner}")

    images = [Path(p).expanduser().resolve() for p in args.images] if args.images else default_images
    if len(images) < 2:
        return fail("regression requires at least two startup images")
    for image in images:
        if not image.exists():
            return fail(f"image not found: {image}")

    out_dir = Path(args.out_dir).expanduser().resolve()
    run_out_dir = Path(tempfile.mkdtemp(prefix="imiv_view_recipe_"))
    runtime_dir = run_out_dir / "runtime"
    cwd_dir = run_out_dir / "cwd"
    scenario_path = run_out_dir / "view_recipe.scenario.xml"
    step1_path = runtime_dir / "open_second_in_new_view.state.json"
    step2_path = runtime_dir / "tune_second_view.state.json"
    step3_path = runtime_dir / "activate_primary_view.state.json"
    step4_path = runtime_dir / "activate_secondary_view.state.json"
    log_path = run_out_dir / "view_recipe.log"

    shutil.rmtree(out_dir, ignore_errors=True)
    run_out_dir.mkdir(parents=True, exist_ok=True)
    cwd_dir.mkdir(parents=True, exist_ok=True)
    cwd = resolve_run_cwd(exe, args.cwd) if args.cwd else cwd_dir
    _write_scenario(scenario_path, path_for_imiv_output(runtime_dir, cwd))

    env = load_env_from_script(Path(args.env_script).expanduser())
    config_home = run_out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)

    cmd = runner_command(exe, cwd, args.backend)
    for image in images:
        cmd.extend(["--open", str(image)])
    cmd.extend(["--scenario", str(scenario_path)])
    if args.trace:
        cmd.append("--trace")

    proc = run_captured_process(cmd, cwd=repo_root, env=env)
    log_path.write_text(proc.stdout, encoding="utf-8")
    shutil.copytree(run_out_dir, out_dir, dirs_exist_ok=True)
    if proc.returncode != 0:
        print(proc.stdout, end="")
        return fail(f"runner exited with code {proc.returncode}")

    try:
        step1 = _load_json(step1_path)
        step2 = _load_json(step2_path)
        step3 = _load_json(step3_path)
        step4 = _load_json(step4_path)
    except FileNotFoundError as exc:
        return fail(f"state output not found: {exc}")

    if int(step1.get("view_count", 0)) < 2:
        return fail("new view was not created")
    if step1.get("image_path") != str(images[1]):
        return fail("new view did not open the second image")

    recipe2 = step2.get("view_recipe", {})
    if step2.get("image_path") != str(images[1]):
        return fail("second view does not show the second image")
    if not bool(recipe2.get("linear_interpolation", False)):
        return fail("second view did not keep linear interpolation override")
    if not bool(step2.get("ocio", {}).get("use_ocio", False)):
        return fail("second view did not keep OCIO enabled")
    try:
        _assert_close(float(recipe2.get("exposure", 0.0)), 1.25, "second view exposure")
        _assert_close(float(recipe2.get("gamma", 0.0)), 1.75, "second view gamma")
        _assert_close(float(recipe2.get("offset", 0.0)), 0.125, "second view offset")
    except (TypeError, ValueError, AssertionError) as exc:
        return fail(str(exc))

    recipe3 = step3.get("view_recipe", {})
    if step3.get("image_path") != str(images[0]):
        return fail("primary view activation did not restore the first image")
    if bool(recipe3.get("linear_interpolation", True)):
        return fail("primary view unexpectedly inherited interpolation override")
    if bool(step3.get("ocio", {}).get("use_ocio", True)):
        return fail("primary view unexpectedly inherited OCIO state")
    try:
        _assert_close(float(recipe3.get("exposure", 99.0)), 0.0, "primary view exposure")
        _assert_close(float(recipe3.get("gamma", 99.0)), 1.0, "primary view gamma")
        _assert_close(float(recipe3.get("offset", 99.0)), 0.0, "primary view offset")
    except (TypeError, ValueError, AssertionError) as exc:
        return fail(str(exc))

    recipe4 = step4.get("view_recipe", {})
    if step4.get("image_path") != str(images[1]):
        return fail("secondary view activation did not restore the second image")
    if not bool(recipe4.get("linear_interpolation", False)):
        return fail("secondary view did not preserve interpolation override")
    if not bool(step4.get("ocio", {}).get("use_ocio", False)):
        return fail("secondary view did not preserve OCIO state")
    try:
        _assert_close(float(recipe4.get("exposure", 0.0)), 1.25, "restored second view exposure")
        _assert_close(float(recipe4.get("gamma", 0.0)), 1.75, "restored second view gamma")
        _assert_close(float(recipe4.get("offset", 0.0)), 0.125, "restored second view offset")
    except (TypeError, ValueError, AssertionError) as exc:
        return fail(str(exc))

    print(f"step1: {step1_path}")
    print(f"step2: {step2_path}")
    print(f"step3: {step3_path}")
    print(f"step4: {step4_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

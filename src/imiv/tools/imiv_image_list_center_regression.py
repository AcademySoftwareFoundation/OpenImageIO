#!/usr/bin/env python3
"""Regression check for preserving centered scroll when opening Image List."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

from imiv_test_utils import (
    default_binary,
    default_env_script,
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


def _write_scenario(path: Path, runtime_dir_rel: str) -> None:
    root = ET.Element("imiv-scenario")
    root.set("out_dir", runtime_dir_rel)

    baseline = ET.SubElement(root, "step")
    baseline.set("name", "baseline")
    baseline.set("state", "true")
    baseline.set("post_action_delay_frames", "4")

    show_list = ET.SubElement(root, "step")
    show_list.set("name", "show_image_list")
    show_list.set("image_list_visible", "true")
    show_list.set("state", "true")
    show_list.set("post_action_delay_frames", "4")

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _build_wide_fixture(oiiotool: Path, out_path: Path) -> None:
    cmd = [
        str(oiiotool),
        "--pattern",
        "constant:color=0.25,0.5,0.75",
        "10000x2000",
        "3",
        "-d",
        "half",
        "-o",
        str(out_path),
    ]
    subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def _load_json(path: Path) -> dict:
    if not path.exists():
        raise FileNotFoundError(path)
    return json.loads(path.read_text(encoding="utf-8"))


def _norm_scroll_centered(state: dict, tol: float = 0.08) -> bool:
    norm_scroll = state.get("norm_scroll")
    if not (
        isinstance(norm_scroll, list)
        and len(norm_scroll) == 2
        and all(isinstance(v, (int, float)) for v in norm_scroll)
    ):
        return False
    return abs(float(norm_scroll[0]) - 0.5) <= tol and abs(float(norm_scroll[1]) - 0.5) <= tol


def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)
    env_script_default = default_env_script(repo_root)
    default_out_dir = repo_root / "build" / "imiv_captures" / "image_list_center_regression"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="opengl",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument(
        "--env-script",
        default=str(env_script_default),
        help="Optional shell env setup script",
    )
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument(
        "--oiiotool", default=str(default_oiiotool(repo_root)), help="oiiotool executable"
    )
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")
    runner = runner.resolve()
    if not runner.exists():
        return fail(f"runner not found: {runner}")
    oiiotool = resolve_existing_tool(args.oiiotool, default_oiiotool(repo_root))
    if not oiiotool.exists():
        return fail(f"oiiotool not found: {oiiotool}")

    cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = Path(args.out_dir).expanduser().resolve()
    runtime_dir = out_dir / "runtime"
    scenario_path = out_dir / "image_list_center.scenario.xml"
    fixture_path = out_dir / "wide.exr"
    baseline_state_path = runtime_dir / "baseline.state.json"
    show_state_path = runtime_dir / "show_image_list.state.json"
    log_path = out_dir / "image_list_center.log"

    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    _build_wide_fixture(oiiotool, fixture_path)
    _write_scenario(scenario_path, path_for_imiv_output(runtime_dir, cwd))

    env = load_env_from_script(Path(args.env_script).expanduser())
    config_home = out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)

    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(["--open", str(fixture_path), "--scenario", str(scenario_path)])
    if args.trace:
        cmd.append("--trace")

    proc = run_captured_process(cmd, cwd=repo_root, env=env)
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        print(proc.stdout, end="")
        return fail(f"runner exited with code {proc.returncode}")

    try:
        baseline_state = _load_json(baseline_state_path)
        show_state = _load_json(show_state_path)
    except FileNotFoundError as exc:
        return fail(f"state output not found: {exc}")

    if not bool(baseline_state.get("image_loaded", False)):
        return fail("baseline state does not report a loaded image")
    if bool(baseline_state.get("image_list_visible", True)):
        return fail("baseline state unexpectedly reports Image List as visible")
    if not _norm_scroll_centered(baseline_state):
        return fail("baseline image was not centered")

    if not bool(show_state.get("image_loaded", False)):
        return fail("show-image-list state does not report a loaded image")
    if not bool(show_state.get("image_list_visible", False)):
        return fail("show-image-list state does not report Image List as visible")
    if not bool(show_state.get("image_list_drawn", False)):
        return fail("show-image-list state does not report Image List as drawn")
    if not _norm_scroll_centered(show_state):
        return fail("opening Image List changed the centered scroll position")

    scroll = show_state.get("scroll")
    if not (
        isinstance(scroll, list)
        and len(scroll) == 2
        and isinstance(scroll[0], (int, float))
        and float(scroll[0]) > 1.0
    ):
        return fail("wide-image regression did not produce horizontal scrolling")

    print(f"baseline_state: {baseline_state_path}")
    print(f"show_state: {show_state_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Regression check for opening a second image view window."""

from __future__ import annotations

import argparse
import json
import shutil
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


def _write_scenario(path: Path, runtime_dir_rel: str) -> None:
    root = ET.Element("imiv-scenario")
    root.set("out_dir", runtime_dir_rel)

    step = ET.SubElement(root, "step")
    step.set("name", "new_view")
    step.set("key_chord", "ctrl+shift+n")
    step.set("state", "true")
    step.set("post_action_delay_frames", "4")

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)
    env_script_default = default_env_script(repo_root)
    default_out_dir = repo_root / "build" / "imiv_captures" / "multiview_regression"
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"

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
    ap.add_argument("--open", default=str(default_image), help="Image path to open")
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")
    runner = runner.resolve()
    if not runner.exists():
        return fail(f"runner not found: {runner}")

    cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = Path(args.out_dir).expanduser().resolve()
    runtime_dir = out_dir / "runtime"
    scenario_path = out_dir / "multiview.scenario.xml"
    state_path = runtime_dir / "new_view.state.json"
    log_path = out_dir / "multiview.log"

    shutil.rmtree(runtime_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    _write_scenario(scenario_path, path_for_imiv_output(runtime_dir, cwd))

    env = load_env_from_script(Path(args.env_script).expanduser())
    config_home = out_dir / "cfg"
    shutil.rmtree(config_home, ignore_errors=True)
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)

    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(
        [
            "--open",
            str(Path(args.open).expanduser().resolve()),
            "--scenario",
            str(scenario_path),
        ]
    )
    if args.trace:
        cmd.append("--trace")

    proc = run_captured_process(cmd, cwd=repo_root, env=env)
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        print(proc.stdout, end="")
        return fail(f"runner exited with code {proc.returncode}")

    if not state_path.exists():
        return fail(f"state output not found: {state_path}")

    state = json.loads(state_path.read_text(encoding="utf-8"))
    if int(state.get("view_count", 0)) < 2:
        return fail("state does not report multiple image views")
    if not state.get("image_loaded", False):
        return fail("state does not report a loaded image after opening a new view")
    if int(state.get("active_view_id", 0)) <= 1:
        return fail("state does not report the new image view as active")
    if not bool(state.get("active_view_docked", False)):
        return fail("state does not report the new image view as docked")

    print(f"state: {state_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

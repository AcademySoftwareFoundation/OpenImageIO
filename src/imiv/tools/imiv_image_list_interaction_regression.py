#!/usr/bin/env python3
"""Regression check for Image List row actions and multi-view behavior."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
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


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def _load_json(path: Path) -> dict:
    if not path.exists():
        raise FileNotFoundError(path)
    return json.loads(path.read_text(encoding="utf-8"))


def _row_open_view_ids(state: dict, index: int) -> list[int]:
    rows = state.get("image_list_open_view_ids", [])
    if not isinstance(rows, list) or index < 0 or index >= len(rows):
        return []
    row = rows[index]
    if not isinstance(row, list):
        return []
    view_ids: list[int] = []
    for value in row:
        try:
            view_ids.append(int(value))
        except (TypeError, ValueError):
            continue
    return view_ids


def _scenario_step(root: ET.Element, name: str, **attrs: str | int | bool) -> None:
    step = ET.SubElement(root, "step")
    step.set("name", name)
    for key, value in attrs.items():
        if isinstance(value, bool):
            step.set(key, "true" if value else "false")
        else:
            step.set(key, str(value))


def _write_interaction_scenario(path: Path, runtime_dir_rel: str) -> None:
    root = ET.Element("imiv-scenario")
    root.set("out_dir", runtime_dir_rel)

    _scenario_step(
        root,
        "click_second",
        image_list_select_index=1,
        state=True,
        post_action_delay_frames=4,
    )
    _scenario_step(
        root,
        "double_click_first",
        image_list_open_new_view_index=0,
        state=True,
        post_action_delay_frames=4,
    )
    _scenario_step(
        root,
        "close_first_in_active",
        image_list_close_active_index=0,
        state=True,
        post_action_delay_frames=4,
    )
    _scenario_step(
        root,
        "remove_second_from_session",
        image_list_remove_index=1,
        state=True,
        post_action_delay_frames=4,
    )

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _run_runner(
    *,
    runner: Path,
    exe: Path,
    cwd: Path,
    repo_root: Path,
    env: dict[str, str],
    images: list[Path],
    scenario_path: Path,
    backend: str,
    trace: bool,
    log_path: Path,
) -> int:
    cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(cwd),
    ]
    for image in images:
        cmd.extend(["--open", str(image)])
    cmd.extend(["--scenario", str(scenario_path)])
    if backend:
        cmd.extend(["--backend", backend])
    if trace:
        cmd.append("--trace")

    print("run:", " ".join(cmd))
    proc = subprocess.run(
        cmd,
        cwd=str(repo_root),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        print(proc.stdout, end="")
    return proc.returncode


def main() -> int:
    repo_root = _repo_root()
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_env_script = repo_root / "build" / "imiv_env.sh"
    default_out_dir = repo_root / "build" / "imiv_captures" / "image_list_interaction_regression"
    default_images = [
        repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png",
        repo_root / "testsuite" / "imiv" / "images" / "CC988_ACEScg.exr",
    ]

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument(
        "--env-script",
        default=str(default_env_script),
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
    runner = runner.resolve()
    if not runner.exists():
        return _fail(f"runner not found: {runner}")

    images = [Path(p).expanduser().resolve() for p in args.images] if args.images else default_images
    if len(images) < 2:
        return _fail("regression requires at least two startup images")
    for image in images:
        if not image.exists():
            return _fail(f"image not found: {image}")

    cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()
    out_dir = Path(args.out_dir).expanduser().resolve()
    interaction_runtime_dir = out_dir / "runtime"
    interaction_scenario_path = out_dir / "image_list_interactions.scenario.xml"
    click_state_path = interaction_runtime_dir / "click_second.state.json"
    double_click_state_path = interaction_runtime_dir / "double_click_first.state.json"
    close_state_path = interaction_runtime_dir / "close_first_in_active.state.json"
    remove_state_path = interaction_runtime_dir / "remove_second_from_session.state.json"
    interaction_log_path = out_dir / "image_list_interactions.log"

    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    env = dict(os.environ)
    env.update(_load_env_from_script(Path(args.env_script).expanduser()))
    config_home = out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)

    _write_interaction_scenario(
        interaction_scenario_path, os.path.relpath(interaction_runtime_dir, cwd)
    )
    rc = _run_runner(
        runner=runner,
        exe=exe,
        cwd=cwd,
        repo_root=repo_root,
        env=env,
        images=images,
        scenario_path=interaction_scenario_path,
        backend=args.backend,
        trace=args.trace,
        log_path=interaction_log_path,
    )
    if rc != 0:
        return _fail(f"interaction runner exited with code {rc}")

    try:
        click_state = _load_json(click_state_path)
        double_click_state = _load_json(double_click_state_path)
        close_state = _load_json(close_state_path)
        remove_state = _load_json(remove_state_path)
    except FileNotFoundError as exc:
        return _fail(f"state output not found: {exc}")

    second_image = str(images[1])
    first_image = str(images[0])

    if click_state.get("image_path") != second_image:
        return _fail("single-click did not load the second image into the active view")
    if int(click_state.get("current_image_index", -1)) != 1:
        return _fail("single-click did not switch current_image_index to the second image")
    if not bool(click_state.get("image_list_visible", False)):
        return _fail("single-click state does not report Image List as visible")

    if int(double_click_state.get("view_count", 0)) < 2:
        return _fail("double-click did not open a new image view")
    if int(double_click_state.get("loaded_view_count", 0)) < 2:
        return _fail("double-click did not leave two loaded image views open")
    if int(double_click_state.get("active_view_id", 0)) <= 1:
        return _fail("double-click did not activate the new image view")
    if double_click_state.get("image_path") != first_image:
        return _fail("double-click did not open the first image in the new view")
    if not bool(double_click_state.get("active_view_docked", False)):
        return _fail("double-click state does not report the new image view as docked")
    if _row_open_view_ids(double_click_state, 0) != [int(double_click_state["active_view_id"])]:
        return _fail("first Image List row did not report the secondary view id")
    if _row_open_view_ids(double_click_state, 1) != [1]:
        return _fail("second Image List row did not preserve the primary view id")

    if bool(close_state.get("image_loaded", True)):
        return _fail("close-in-active-view did not clear the active image view")
    if int(close_state.get("loaded_view_count", -1)) != 1:
        return _fail("close-in-active-view did not leave exactly one loaded view")

    if int(remove_state.get("loaded_image_count", -1)) != 1:
        return _fail("remove-from-session did not shrink the session queue")
    if bool(remove_state.get("image_list_visible", True)):
        return _fail("Image List did not hide after the queue shrank to one image")
    if int(remove_state.get("loaded_view_count", -1)) != 0:
        return _fail("remove-from-session did not close views showing the removed image")

    print(f"click_state: {click_state_path}")
    print(f"double_click_state: {double_click_state_path}")
    print(f"close_state: {close_state_path}")
    print(f"remove_state: {remove_state_path}")
    print(f"log: {interaction_log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

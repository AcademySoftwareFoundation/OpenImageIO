#!/usr/bin/env python3
"""Regression check for multi-file drag/drop into the shared image library."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

from imiv_test_utils import (
    default_binary,
    default_env_script,
    fail,
    load_env_from_script,
    repo_root as imiv_repo_root,
    resolve_run_cwd,
    run_captured_process,
    runner_command,
    runner_path,
)


def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)
    env_script_default = default_env_script(repo_root)
    default_out_dir = repo_root / "build" / "imiv_captures" / "drag_drop_regression"
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
        help="Dropped image path; may be repeated",
    )
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve(strict=False)
    if not runner.exists():
        return fail(f"runner not found: {runner}")

    images = [Path(p).expanduser().resolve() for p in args.images] if args.images else default_images
    if len(images) < 2:
        return fail("regression requires at least two dropped images")
    for image in images:
        if not image.exists():
            return fail(f"image not found: {image}")

    cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = Path(args.out_dir).expanduser().resolve()
    state_path = out_dir / "drag_drop.state.json"
    layout_path = out_dir / "drag_drop.layout.json"
    log_path = out_dir / "drag_drop.log"
    drop_paths_file = out_dir / "drop_paths.txt"

    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    drop_paths_file.write_text(
        "".join(f"{image}\n" for image in images), encoding="utf-8"
    )

    env = load_env_from_script(Path(args.env_script).expanduser())
    config_home = out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)
    env["IMIV_IMGUI_TEST_ENGINE_DROP_APPLY_FRAME"] = "2"
    env["IMIV_IMGUI_TEST_ENGINE_DROP_PATHS_FILE"] = str(drop_paths_file)

    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(
        [
            "--layout-json-out",
            str(layout_path),
            "--layout-items",
            "--state-json-out",
            str(state_path),
            "--state-delay-frames",
            "8",
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
    if not bool(state.get("image_loaded", False)):
        return fail("state does not report a loaded image after drop")
    if state.get("image_path") != str(images[0]):
        return fail("drop did not load the first dropped image")
    if int(state.get("loaded_image_count", 0)) != len(images):
        return fail("shared image library count does not match dropped images")
    if int(state.get("current_image_index", -1)) != 0:
        return fail("drop did not select the first dropped image in the queue")
    if not bool(state.get("image_list_visible", False)):
        return fail("Image List did not auto-open after multi-file drop")

    print(f"layout: {layout_path}")
    print(f"state: {state_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

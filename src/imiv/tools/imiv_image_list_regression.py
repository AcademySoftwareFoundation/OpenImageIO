#!/usr/bin/env python3
"""Regression check for default Image List visibility on multi-file load."""

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
    default_out_dir = repo_root / "build" / "imiv_captures" / "image_list_regression"
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

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")
    runner = runner.resolve()
    if not runner.exists():
        return fail(f"runner not found: {runner}")

    images = [Path(p).expanduser().resolve() for p in args.images] if args.images else default_images
    if len(images) < 2:
        return fail("regression requires at least two startup images")
    for image in images:
        if not image.exists():
            return fail(f"image not found: {image}")

    cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = Path(args.out_dir).expanduser().resolve()
    layout_path = out_dir / "image_list.layout.json"
    state_path = out_dir / "image_list.state.json"
    log_path = out_dir / "image_list.log"

    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    env = load_env_from_script(Path(args.env_script).expanduser())
    config_home = out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)

    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(["--layout-json-out", str(layout_path), "--state-json-out", str(state_path)])
    for image in images:
        cmd.extend(["--open", str(image)])
    if args.trace:
        cmd.append("--trace")

    proc = run_captured_process(cmd, cwd=repo_root, env=env)
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        print(proc.stdout, end="")
        return fail(f"runner exited with code {proc.returncode}")

    if not layout_path.exists():
        return fail(f"layout output not found: {layout_path}")
    if not state_path.exists():
        return fail(f"state output not found: {state_path}")

    state = json.loads(state_path.read_text(encoding="utf-8"))

    if int(state.get("loaded_image_count", 0)) < 2:
        return fail("state does not report a multi-image queue")
    if not bool(state.get("image_list_visible", False)):
        return fail("state does not report Image List as visible")

    if not bool(state.get("image_list_drawn", False)):
        return fail("state does not report Image List as drawn")
    if not bool(state.get("image_list_docked", False)):
        return fail("state does not report Image List as docked")

    image_list_size = state.get("image_list_size")
    if not (
        isinstance(image_list_size, list)
        and len(image_list_size) == 2
        and isinstance(image_list_size[0], (int, float))
    ):
        return fail("state does not report Image List size")

    image_list_width = float(image_list_size[0])
    if image_list_width < 150.0 or image_list_width > 260.0:
        return fail(f"unexpected Image List width: {image_list_width:.1f}")

    print(f"layout: {layout_path}")
    print(f"state: {state_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

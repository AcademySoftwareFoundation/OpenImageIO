#!/usr/bin/env python3
"""Regression check for loading a true RGB input image in imiv."""

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
    runner_path,
)


ERROR_PATTERNS = (
    "error: imiv exited with code",
    "OpenGL texture upload failed",
    "OpenGL preview draw failed",
    "OpenGL OCIO preview draw failed",
    "failed to create Metal source texture",
    "failed to create Metal upload pipeline",
    "failed to create Metal source upload buffer",
    "Metal source upload compute dispatch failed",
    "failed to create Metal preview texture",
    "Metal preview render failed",
)

def _run_checked(cmd: list[str], *, cwd: Path) -> None:
    print("run:", " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd), check=True)


def _generate_rgb_fixture(oiiotool: Path, source_path: Path, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    _run_checked(
        [
            str(oiiotool),
            str(source_path),
            "--ch",
            "R,G,B",
            "-d",
            "uint8",
            "-o",
            str(out_path),
        ],
        cwd=out_path.parent,
    )


def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)
    default_source = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"

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
    ap.add_argument("--env-script", default="", help="Optional shell env setup script")
    ap.add_argument("--out-dir", default="", help="Output directory")
    ap.add_argument(
        "--source-image",
        default=str(default_source),
        help="Source image used to generate a 3-channel RGB fixture",
    )
    ap.add_argument("--trace", action="store_true", help="Enable runner tracing")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")

    oiiotool = resolve_existing_tool(args.oiiotool, default_oiiotool(repo_root))
    if not oiiotool.exists():
        return fail(f"oiiotool not found: {oiiotool}")

    cwd = resolve_run_cwd(exe, args.cwd)
    if args.out_dir:
        out_dir = Path(args.out_dir).resolve()
    else:
        out_dir = exe.parent.parent / "imiv_captures" / "rgb_input_regression"
    out_dir.mkdir(parents=True, exist_ok=True)

    source_path = Path(args.source_image).resolve()
    if not source_path.exists():
        return fail(f"source image not found: {source_path}")

    env_script = (
        Path(args.env_script).resolve()
        if args.env_script
        else default_env_script(repo_root, exe)
    )
    env = load_env_from_script(env_script)
    env["IMIV_CONFIG_HOME"] = str(out_dir / "cfg")

    rgb_fixture = out_dir / "rgb_input_fixture_u8.tif"
    _generate_rgb_fixture(oiiotool, source_path, rgb_fixture)

    layout_path = out_dir / "rgb_input.layout.json"
    state_path = out_dir / "rgb_input.state.json"
    log_path = out_dir / "rgb_input.log"

    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(
        [
            "--open",
            str(rgb_fixture),
            "--layout-json-out",
            str(layout_path),
            "--layout-items",
            "--state-json-out",
            str(state_path),
            "--post-action-delay-frames",
            "2",
        ]
    )
    if args.trace:
        cmd.append("--trace")

    proc = run_logged_process(
        cmd, cwd=repo_root, env=env, timeout=90, log_path=log_path
    )
    if proc.returncode != 0:
        return fail(f"runner exited with code {proc.returncode}")

    for required in (layout_path, state_path):
        if not required.exists():
            return fail(f"missing output: {required}")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            return fail(f"found runtime error pattern: {pattern}")

    layout = json.loads(layout_path.read_text(encoding="utf-8"))
    if not any(window.get("name") == "Image" for window in layout.get("windows", [])):
        return fail("layout dump missing Image window")

    state = json.loads(state_path.read_text(encoding="utf-8"))
    if not state.get("image_loaded"):
        return fail("state dump says image is not loaded")

    current_path = state.get("image_path") or ""
    if not current_path:
        return fail("state dump missing image_path")
    try:
        current_resolved = Path(current_path).resolve()
    except Exception:
        current_resolved = Path(current_path)
    if current_resolved != rgb_fixture.resolve():
        return fail(
            f"loaded path mismatch: expected {rgb_fixture}, got {current_path}"
        )

    image_size = state.get("image_size", [0, 0])
    if (
        not isinstance(image_size, list)
        or len(image_size) != 2
        or int(image_size[0]) <= 0
        or int(image_size[1]) <= 0
    ):
        return fail(f"invalid image_size in state dump: {image_size}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

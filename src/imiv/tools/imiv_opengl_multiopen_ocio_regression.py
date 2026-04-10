#!/usr/bin/env python3
"""Regression check for OpenGL OCIO preview with multiple startup images."""

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
    run_logged_process,
    runner_command,
    runner_path,
)


ERROR_PATTERNS = (
    "error: imiv exited with code",
    "OpenGL preview draw failed",
    "OpenGL OCIO preview draw failed",
)

def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)
    env_script_default = default_env_script(repo_root)
    default_out_dir = (
        repo_root / "build_u" / "imiv_captures" / "opengl_multiopen_ocio_regression"
    )
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument("--backend", default="opengl", help="Runtime backend override")
    ap.add_argument(
        "--env-script", default=str(env_script_default), help="Optional shell env setup script"
    )
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument("--open", default=str(default_image), help="Source image to duplicate")
    ap.add_argument("--trace", action="store_true", help="Enable runner tracing")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")

    cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    image_a = Path(args.open).resolve()
    if not image_a.exists():
        return fail(f"image not found: {image_a}")
    image_b = out_dir / f"{image_a.stem}_copy{image_a.suffix}"
    shutil.copyfile(image_a, image_b)

    env = load_env_from_script(Path(args.env_script).resolve())
    env["IMIV_CONFIG_HOME"] = str(out_dir / "cfg")

    screenshot_path = out_dir / "opengl_multiopen_ocio.png"
    layout_path = out_dir / "opengl_multiopen_ocio.layout.json"
    state_path = out_dir / "opengl_multiopen_ocio.state.json"
    log_path = out_dir / "opengl_multiopen_ocio.log"

    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(
        [
            "--open",
            str(image_a),
            "--open",
            str(image_b),
            "--ocio-use",
            "true",
            "--screenshot-out",
            str(screenshot_path),
            "--layout-json-out",
            str(layout_path),
            "--layout-items",
            "--state-json-out",
            str(state_path),
        ]
    )
    if args.trace:
        cmd.append("--trace")

    proc = run_logged_process(
        cmd, cwd=repo_root, env=env, timeout=90, log_path=log_path
    )
    if proc.returncode != 0:
        return fail(f"runner exited with code {proc.returncode}")

    for required in (screenshot_path, layout_path, state_path):
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
    if not state.get("image_path"):
        return fail("state dump missing image_path")
    if int(state.get("loaded_image_count", 0)) < 2:
        return fail("expected at least 2 loaded images in session")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

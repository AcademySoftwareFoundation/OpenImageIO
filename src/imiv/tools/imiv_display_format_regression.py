#!/usr/bin/env python3
"""Regression check for display-format request and fallback reporting."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from imiv_test_utils import (
    default_binary,
    default_env_script,
    default_image,
    fail,
    load_env_from_script,
    repo_root as imiv_repo_root,
    resolve_run_cwd,
    run_logged_process,
    runner_command,
)


ERROR_PATTERNS = (
    "error: imiv exited with code",
    "failed to create Vulkan",
    "failed to create Metal",
    "failed to create OpenGL",
    "OpenGL preview draw failed",
    "OpenGL OCIO preview draw failed",
    "Metal preview render failed",
    "Vulkan preview draw failed",
)

VALID_DYNAMIC_RANGES = {"unknown", "SDR", "EDR", "HDR"}


def _validate_display_state(state_path: Path, requested_format: str) -> int:
    state = json.loads(state_path.read_text(encoding="utf-8"))
    if not state.get("image_loaded"):
        return fail("state dump says image is not loaded")

    backend = state.get("backend")
    if not isinstance(backend, dict):
        return fail("state dump missing backend object")

    display = backend.get("display")
    if not isinstance(display, dict):
        return fail("state dump missing backend.display object")

    actual_request = display.get("requested_format")
    if actual_request != requested_format:
        return fail(
            f"expected requested_format={requested_format}, got {actual_request}"
        )

    color_bits = display.get("color_bits")
    if not isinstance(color_bits, int) or color_bits <= 0:
        return fail(f"invalid display color_bits: {color_bits}")

    dynamic_range = display.get("dynamic_range")
    if dynamic_range not in VALID_DYNAMIC_RANGES:
        return fail(f"invalid display dynamic_range: {dynamic_range}")

    presentation = display.get("presentation")
    if not isinstance(presentation, str) or not presentation:
        return fail("missing display presentation label")

    fell_back = display.get("format_request_fell_back")
    if not isinstance(fell_back, bool):
        return fail(f"invalid display fallback flag: {fell_back}")

    if requested_format == "rgb10a2":
        if fell_back and color_bits >= 10:
            return fail("rgb10a2 fallback was reported with 10-bit output")
        if not fell_back and color_bits < 10:
            return fail("rgb10a2 was reported active with less than 10 bits")

    return 0


def main() -> int:
    repo_root = imiv_repo_root()

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument(
        "--display-format",
        default="rgb10a2",
        choices=("auto", "rgba8", "rgb10a2"),
        help="Display-format request to validate",
    )
    ap.add_argument("--env-script", default="", help="Optional shell env setup script")
    ap.add_argument("--out-dir", default="", help="Output directory")
    ap.add_argument("--open", default=str(default_image(repo_root)), help="Image to open")
    ap.add_argument("--trace", action="store_true", help="Enable runner tracing")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")

    cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = (
        Path(args.out_dir).resolve()
        if args.out_dir
        else exe.parent.parent / "imiv_captures" / "display_format_regression"
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    image_path = Path(args.open).resolve()
    if not image_path.exists():
        return fail(f"image not found: {image_path}")

    env_script = (
        Path(args.env_script).resolve()
        if args.env_script
        else default_env_script(repo_root, exe)
    )
    env = load_env_from_script(env_script)
    env["IMIV_CONFIG_HOME"] = str(out_dir / "cfg")

    layout_path = out_dir / f"display_format_{args.display_format}.layout.json"
    state_path = out_dir / f"display_format_{args.display_format}.state.json"
    log_path = out_dir / f"display_format_{args.display_format}.log"

    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(
        [
            "--display-format",
            args.display_format,
            "--open",
            str(image_path),
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

    return _validate_display_state(state_path, args.display_format)


if __name__ == "__main__":
    raise SystemExit(main())

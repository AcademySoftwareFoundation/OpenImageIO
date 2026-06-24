#!/usr/bin/env python3
"""Regression check for startup folder-open queue filtering."""

from __future__ import annotations

import argparse
import json
import shutil
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
    run_captured_process,
    runner_command,
    runner_path,
)


def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)
    env_script_default = default_env_script(repo_root)
    default_out_dir = repo_root / "build" / "imiv_captures" / "open_folder_regression"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument(
        "--oiiotool",
        default=str(default_oiiotool(repo_root)),
        help="oiiotool executable",
    )
    ap.add_argument(
        "--env-script",
        default=str(env_script_default),
        help="Optional shell env setup script",
    )
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")
    oiiotool = resolve_existing_tool(args.oiiotool, default_oiiotool(repo_root))
    if not oiiotool.exists():
        return fail(f"oiiotool not found: {oiiotool}")
    runner = runner.resolve()
    if not runner.exists():
        return fail(f"runner not found: {runner}")

    cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = Path(args.out_dir).expanduser().resolve()
    runtime_dir = out_dir / "runtime"
    state_path = out_dir / "open_folder.state.json"
    log_path = out_dir / "open_folder.log"

    shutil.rmtree(out_dir, ignore_errors=True)
    runtime_dir.mkdir(parents=True, exist_ok=True)

    env = load_env_from_script(Path(args.env_script).expanduser())
    config_home = out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)

    source_logo = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"
    if not source_logo.exists():
        return fail(f"source image not found: {source_logo}")

    folder_dir = runtime_dir / "mixed_folder"
    folder_dir.mkdir(parents=True, exist_ok=True)
    image_a = folder_dir / "01_logo.png"
    image_b = folder_dir / "02_logo.exr"
    notes = folder_dir / "03_notes.txt"
    blob = folder_dir / "04_blob.bin"

    try:
        prep_png = run_captured_process(
            [str(oiiotool), str(source_logo), "-o", str(image_a)],
            cwd=repo_root,
            env=env,
        )
        (runtime_dir / "make_png.log").write_text(prep_png.stdout, encoding="utf-8")
        if prep_png.returncode != 0:
            raise RuntimeError(
                f"command failed ({prep_png.returncode}): {oiiotool} png copy\n"
                f"{prep_png.stdout}"
            )
        prep_exr = run_captured_process(
            [str(oiiotool), str(source_logo), "--ch", "R,G,B", "-d", "half", "-o", str(image_b)],
            cwd=repo_root,
            env=env,
        )
        (runtime_dir / "make_exr.log").write_text(prep_exr.stdout, encoding="utf-8")
        if prep_exr.returncode != 0:
            raise RuntimeError(
                f"command failed ({prep_exr.returncode}): {oiiotool} exr convert\n"
                f"{prep_exr.stdout}"
            )
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    notes.write_text("not an image\n", encoding="utf-8")
    blob.write_bytes(b"\x00\x01\x02\x03")

    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(["--open", str(folder_dir), "--state-json-out", str(state_path)])
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
        return fail("state does not report a loaded image")
    if int(state.get("loaded_image_count", 0)) != 2:
        return fail(
            f"expected 2 supported images from folder, got {state.get('loaded_image_count')}"
        )
    if not bool(state.get("image_list_visible", False)):
        return fail("Image List did not auto-open for folder queue")

    image_path = Path(str(state.get("image_path", "")))
    if image_path.name != "01_logo.png":
        return fail(f"unexpected first loaded image: {image_path}")

    print(f"state: {state_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

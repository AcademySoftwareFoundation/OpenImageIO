#!/usr/bin/env python3
"""Regression check for large-image queue switching on GPU backends."""

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


COMMON_ERROR_PATTERNS = (
    "error: imiv exited with code",
)

VULKAN_ERROR_PATTERNS = (
    "imiv: Vulkan error",
    "fatal Vulkan error",
    "VK_ERROR_DEVICE_LOST",
    "vkUpdateDescriptorSets():",
    "maxStorageBufferRange",
)

OPENGL_ERROR_PATTERNS = (
    "OpenGL texture upload failed",
    "OpenGL striped texture upload failed",
    "OpenGL preview draw failed",
)

METAL_ERROR_PATTERNS = (
    "failed to create Metal striped upload buffer",
    "failed to create Metal source upload buffer",
    "Metal source upload compute dispatch failed",
)


def _write_scenario(path: Path, runtime_dir_rel: str) -> None:
    root = ET.Element("imiv-scenario")
    root.set("out_dir", runtime_dir_rel)

    step = ET.SubElement(root, "step")
    step.set("name", "next_1")
    step.set("key_chord", "pagedown")
    step.set("post_action_delay_frames", "4")

    step = ET.SubElement(root, "step")
    step.set("name", "next_2")
    step.set("key_chord", "pagedown")
    step.set("post_action_delay_frames", "4")
    step.set("state", "true")
    step.set("layout", "true")
    step.set("layout_items", "true")

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _run_checked(cmd: list[str], *, cwd: Path) -> None:
    print("run:", " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd), check=True)


def _generate_large_rgb_fixture(
    oiiotool: Path, out_path: Path, color: str, width: int, height: int
) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    _run_checked(
        [
            str(oiiotool),
            "--pattern",
            f"constant:color={color}",
            f"{width}x{height}",
            "3",
            "-d",
            "uint16",
            "-o",
            str(out_path),
        ],
        cwd=out_path.parent,
    )


def _backend_error_patterns(backend: str) -> tuple[str, ...]:
    backend = backend.lower()
    if backend == "vulkan":
        return COMMON_ERROR_PATTERNS + VULKAN_ERROR_PATTERNS
    if backend == "opengl":
        return COMMON_ERROR_PATTERNS + OPENGL_ERROR_PATTERNS
    if backend == "metal":
        return COMMON_ERROR_PATTERNS + METAL_ERROR_PATTERNS
    return COMMON_ERROR_PATTERNS


def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument("--backend", default="vulkan", help="Runtime backend override")
    ap.add_argument("--oiiotool", default=str(default_oiiotool(repo_root)), help="oiiotool executable")
    ap.add_argument("--env-script", default="", help="Optional shell env setup script")
    ap.add_argument("--out-dir", default="", help="Output directory")
    ap.add_argument("--trace", action="store_true", help="Enable runner trace")
    args = ap.parse_args()

    backend = args.backend.lower()
    if backend not in ("vulkan", "opengl", "metal"):
        print("skip: large image switch regression currently targets Vulkan/OpenGL/Metal only")
        return 77

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
        out_dir = exe.parent.parent / "imiv_captures" / f"large_image_switch_regression_{backend}"
    runtime_dir = out_dir / "runtime"
    input_dir = out_dir / "inputs"
    scenario_path = out_dir / "large_image_switch.scenario.xml"
    log_path = out_dir / "large_image_switch.log"
    state_path = runtime_dir / "next_2.state.json"
    layout_path = runtime_dir / "next_2.layout.json"

    shutil.rmtree(runtime_dir, ignore_errors=True)
    shutil.rmtree(input_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    _write_scenario(scenario_path, path_for_imiv_output(runtime_dir, cwd))

    width = 4096
    height = 4096
    image_paths = [
        input_dir / "large_rgb_a.tif",
        input_dir / "large_rgb_b.tif",
        input_dir / "large_rgb_c.tif",
    ]
    colors = ["0.10,0.20,0.30", "0.30,0.20,0.10", "0.20,0.30,0.10"]
    for path, color in zip(image_paths, colors):
        _generate_large_rgb_fixture(oiiotool, path, color, width, height)

    env_script = (
        Path(args.env_script).resolve()
        if args.env_script
        else default_env_script(repo_root, exe)
    )
    env = load_env_from_script(env_script)
    config_home = out_dir / "cfg"
    shutil.rmtree(config_home, ignore_errors=True)
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)
    if backend == "vulkan":
        env["IMIV_VULKAN_MAX_STORAGE_BUFFER_RANGE_OVERRIDE"] = str(64 * 1024 * 1024)
    elif backend == "opengl":
        env["IMIV_OPENGL_MAX_UPLOAD_CHUNK_BYTES_OVERRIDE"] = str(64 * 1024 * 1024)
    elif backend == "metal":
        env["IMIV_METAL_MAX_UPLOAD_CHUNK_BYTES_OVERRIDE"] = str(64 * 1024 * 1024)

    cmd = runner_command(exe, cwd, backend)
    cmd.extend(["--scenario", str(scenario_path)])
    for path in image_paths:
        cmd.extend(["--open", str(path)])
    if args.trace:
        cmd.append("--trace")

    proc = run_captured_process(
        cmd, cwd=repo_root, env=env, timeout=180
    )
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        print(proc.stdout, end="")
        return fail(f"runner exited with code {proc.returncode}")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in _backend_error_patterns(backend):
        if pattern in log_text:
            return fail(f"found runtime error pattern: {pattern}")

    if not state_path.exists():
        return fail(f"state output not found: {state_path}")
    if not layout_path.exists():
        return fail(f"layout output not found: {layout_path}")

    state = json.loads(state_path.read_text(encoding="utf-8"))
    if not state.get("image_loaded", False):
        return fail("state does not report a loaded image after large-image switching")
    if int(state.get("loaded_image_count", 0)) != 3:
        return fail("state does not report the expected loaded-image queue size")
    if int(state.get("current_image_index", -1)) != 2:
        return fail("state does not report the third image as active after two switches")
    if Path(state.get("image_path", "")).resolve() != image_paths[2]:
        return fail("state does not report the expected third image path")

    print(f"state: {state_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

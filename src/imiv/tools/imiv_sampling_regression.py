#!/usr/bin/env python3
"""Regression check for nearest vs linear preview sampling in a single app run."""

from __future__ import annotations

import argparse
import json
import math
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
    run_logged_process,
    runner_command,
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
    "screenshot failed: framebuffer readback failed",
)


def _write_prefs(config_home: Path) -> Path:
    prefs_dir = config_home / "OpenImageIO" / "imiv"
    prefs_dir.mkdir(parents=True, exist_ok=True)
    prefs_path = prefs_dir / "imiv.inf"
    prefs_text = (
        "[ImivApp][State]\n"
        "linear_interpolation=0\n"
        "fit_image_to_window=1\n"
        "pixelview_follows_mouse=1\n"
        "show_mouse_mode_selector=0\n"
    )
    prefs_path.write_text(prefs_text, encoding="utf-8")
    return prefs_path


def _scenario_step(root: ET.Element, name: str, **attrs: str | int | bool) -> None:
    step = ET.SubElement(root, "step")
    step.set("name", name)
    for key, value in attrs.items():
        if isinstance(value, bool):
            step.set(key, "true" if value else "false")
        else:
            step.set(key, str(value))


def _write_scenario(path: Path, runtime_dir_rel: str) -> None:
    root = ET.Element("imiv-scenario")
    root.set("out_dir", runtime_dir_rel)
    root.set("layout_items", "true")

    _scenario_step(
        root,
        "nearest",
        delay_frames=3,
        linear_interpolation=False,
        screenshot=True,
        layout=True,
    )
    _scenario_step(
        root,
        "linear",
        linear_interpolation=True,
        post_action_delay_frames=2,
        screenshot=True,
        layout=True,
    )

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)

def _image_crop_rect(layout_path: Path) -> tuple[int, int, int, int]:
    data = json.loads(layout_path.read_text(encoding="utf-8"))
    image_window = None
    for window in data.get("windows", []):
        if window.get("name") == "Image":
            image_window = window
            break
    if image_window is None:
        raise RuntimeError(f"{layout_path.name}: missing Image window")

    viewport_id = image_window.get("viewport_id")
    origin_x = float(image_window["rect"]["min"][0])
    origin_y = float(image_window["rect"]["min"][1])
    for window in data.get("windows", []):
        if window.get("viewport_id") != viewport_id:
            continue
        rect = window.get("rect")
        if not rect:
            continue
        origin_x = min(origin_x, float(rect["min"][0]))
        origin_y = min(origin_y, float(rect["min"][1]))

    chosen_rect = None
    for item in image_window.get("items", []):
        debug = item.get("debug") or ""
        if debug == "image: Image" or "image_canvas" in debug:
            chosen_rect = item.get("rect_clipped") or item.get("rect_full")
            break

    if chosen_rect is None:
        best_rect = None
        best_area = -1.0
        for item in image_window.get("items", []):
            rect = item.get("rect_clipped") or item.get("rect_full")
            if not rect:
                continue
            min_v = rect.get("min")
            max_v = rect.get("max")
            if not min_v or not max_v:
                continue
            width = max(0.0, float(max_v[0]) - float(min_v[0]))
            height = max(0.0, float(max_v[1]) - float(min_v[1]))
            area = width * height
            if area > best_area:
                best_area = area
                best_rect = rect
        chosen_rect = best_rect

    if chosen_rect is None:
        raise RuntimeError(f"{layout_path.name}: missing image rect")

    x0 = int(math.floor(float(chosen_rect["min"][0]) - origin_x)) + 1
    y0 = int(math.floor(float(chosen_rect["min"][1]) - origin_y)) + 1
    x1 = int(math.ceil(float(chosen_rect["max"][0]) - origin_x)) - 2
    y1 = int(math.ceil(float(chosen_rect["max"][1]) - origin_y)) - 2
    if x1 <= x0 or y1 <= y0:
        raise RuntimeError(f"{layout_path.name}: invalid crop rect")
    return x0, y0, x1, y1


def _crop_to_ppm(
    oiiotool: Path, source: Path, crop_rect: tuple[int, int, int, int], dest: Path
) -> None:
    x0, y0, x1, y1 = crop_rect
    subprocess.run(
        [
            str(oiiotool),
            str(source),
            "--cut",
            f"{x0},{y0},{x1},{y1}",
            "--ch",
            "R,G,B",
            "-o",
            str(dest),
        ],
        check=True,
    )


def _resize_to_ppm(
    oiiotool: Path, source: Path, width: int, height: int, dest: Path
) -> None:
    subprocess.run(
        [
            str(oiiotool),
            str(source),
            "--resize",
            f"{width}x{height}",
            "--ch",
            "R,G,B",
            "-o",
            str(dest),
        ],
        check=True,
    )


def _read_ppm(path: Path) -> tuple[int, int, bytes]:
    with path.open("rb") as handle:
        magic = handle.readline().strip()
        if magic != b"P6":
            raise RuntimeError(f"{path.name}: unsupported PPM format {magic!r}")

        def _next_non_comment() -> bytes:
            while True:
                line = handle.readline()
                if not line:
                    raise RuntimeError(f"{path.name}: truncated PPM header")
                line = line.strip()
                if not line or line.startswith(b"#"):
                    continue
                return line

        dims = _next_non_comment().split()
        if len(dims) != 2:
            raise RuntimeError(f"{path.name}: invalid PPM dimensions")
        width = int(dims[0])
        height = int(dims[1])
        max_value = int(_next_non_comment())
        if max_value != 255:
            raise RuntimeError(f"{path.name}: unsupported max value {max_value}")

        pixels = handle.read(width * height * 3)
        if len(pixels) != width * height * 3:
            raise RuntimeError(f"{path.name}: truncated PPM payload")
        return width, height, pixels


def _mean_abs_diff(lhs: Path, rhs: Path) -> float:
    lhs_w, lhs_h, lhs_pixels = _read_ppm(lhs)
    rhs_w, rhs_h, rhs_pixels = _read_ppm(rhs)
    if (lhs_w, lhs_h) != (rhs_w, rhs_h):
        raise RuntimeError(
            f"image size mismatch: {lhs.name}={lhs_w}x{lhs_h}, {rhs.name}={rhs_w}x{rhs_h}"
        )
    total = 0
    for a, b in zip(lhs_pixels, rhs_pixels):
        total += abs(a - b)
    return total / float(len(lhs_pixels))


def _midtone_fraction(path: Path) -> float:
    _, _, pixels = _read_ppm(path)
    midtone = 0
    pixel_count = len(pixels) // 3
    for index in range(0, len(pixels), 3):
        value = (int(pixels[index]) + int(pixels[index + 1]) + int(pixels[index + 2])) // 3
        if 8 < value < 247:
            midtone += 1
    return midtone / float(pixel_count)


def _build_fixture(oiiotool: Path, path: Path) -> None:
    subprocess.run(
        [
            str(oiiotool),
            "--pattern",
            "checker:width=1:height=1:color1=0,0,0:color2=1,1,1",
            "17x17",
            "3",
            "-d",
            "uint8",
            "-o",
            str(path),
        ],
        check=True,
    )


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
    ap.add_argument("--oiiotool", default=str(default_oiiotool(repo_root)), help="oiiotool executable")
    ap.add_argument("--env-script", default="", help="Optional shell env setup script")
    ap.add_argument("--out-dir", default="", help="Output directory")
    ap.add_argument("--trace", action="store_true", help="Enable runner tracing")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")

    oiiotool = resolve_existing_tool(args.oiiotool, default_oiiotool(repo_root))
    if not oiiotool.exists():
        return fail(f"oiiotool not found: {oiiotool}")

    cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = (
        Path(args.out_dir).resolve()
        if args.out_dir
        else exe.parent.parent / "imiv_captures" / "sampling_regression"
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    env_script = (
        Path(args.env_script).resolve()
        if args.env_script
        else default_env_script(repo_root, exe)
    )
    env = load_env_from_script(env_script)
    env["IMIV_CONFIG_HOME"] = str(out_dir / "cfg")
    _write_prefs(Path(env["IMIV_CONFIG_HOME"]))

    image_path = out_dir / "sampling_checker_input.tif"
    _build_fixture(oiiotool, image_path)

    scenario_path = out_dir / "sampling.scenario.xml"
    runtime_dir = out_dir / "runtime"
    runtime_dir_rel = path_for_imiv_output(runtime_dir, cwd)
    _write_scenario(scenario_path, runtime_dir_rel=runtime_dir_rel)

    log_path = out_dir / "sampling.log"
    cmd = runner_command(exe, cwd, args.backend)
    cmd.extend(["--open", str(image_path), "--scenario", str(scenario_path)])
    if args.trace:
        cmd.append("--trace")

    proc = run_logged_process(
        cmd, cwd=repo_root, env=env, timeout=120, log_path=log_path
    )
    if proc.returncode != 0:
        return fail(f"runner exited with code {proc.returncode}")

    nearest_screenshot = runtime_dir / "nearest.png"
    nearest_layout = runtime_dir / "nearest.layout.json"
    linear_screenshot = runtime_dir / "linear.png"
    linear_layout = runtime_dir / "linear.layout.json"
    for required in (
        nearest_screenshot,
        nearest_layout,
        linear_screenshot,
        linear_layout,
    ):
        if not required.exists():
            return fail(f"missing output: {required}")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            return fail(f"found runtime error pattern: {pattern}")

    crop_dir = out_dir / "crops"
    crop_dir.mkdir(parents=True, exist_ok=True)
    nearest_crop = crop_dir / "nearest.ppm"
    linear_crop = crop_dir / "linear.ppm"
    _crop_to_ppm(oiiotool, nearest_screenshot, _image_crop_rect(nearest_layout), nearest_crop)
    _crop_to_ppm(oiiotool, linear_screenshot, _image_crop_rect(linear_layout), linear_crop)

    nearest_w, nearest_h, _ = _read_ppm(nearest_crop)
    linear_w, linear_h, _ = _read_ppm(linear_crop)
    common_w = min(nearest_w, linear_w)
    common_h = min(nearest_h, linear_h)
    if common_w <= 0 or common_h <= 0:
        return fail("invalid normalized crop size")

    nearest_norm = crop_dir / "nearest.norm.ppm"
    linear_norm = crop_dir / "linear.norm.ppm"
    _resize_to_ppm(oiiotool, nearest_crop, common_w, common_h, nearest_norm)
    _resize_to_ppm(oiiotool, linear_crop, common_w, common_h, linear_norm)

    diff = _mean_abs_diff(nearest_norm, linear_norm)
    nearest_midtones = _midtone_fraction(nearest_norm)
    linear_midtones = _midtone_fraction(linear_norm)

    print(f"nearest: {nearest_norm}")
    print(f"linear: {linear_norm}")
    print(
        "scores: "
        f"mean_abs_diff={diff:.4f}, "
        f"nearest_midtones={nearest_midtones:.4f}, "
        f"linear_midtones={linear_midtones:.4f}"
    )

    if diff < 8.0:
        return fail(
            "nearest and linear preview crops are too similar "
            f"(mean abs diff={diff:.4f})"
        )
    if nearest_midtones > 0.35:
        return fail(
            "nearest preview still looks blurred "
            f"(midtone fraction={nearest_midtones:.4f})"
        )
    if linear_midtones <= nearest_midtones + 0.10:
        return fail(
            "linear preview does not look materially smoother than nearest "
            f"(nearest={nearest_midtones:.4f}, linear={linear_midtones:.4f})"
        )

    print(f"ok: sampling regression outputs are in {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

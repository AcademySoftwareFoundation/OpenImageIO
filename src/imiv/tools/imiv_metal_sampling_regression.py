#!/usr/bin/env python3
"""Regression check for Metal nearest vs linear preview sampling."""

from __future__ import annotations

import argparse
import json
import math
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


ERROR_PATTERNS = (
    "error: imiv exited with code",
    "MTLCreateSystemDefaultDevice failed",
    "failed to create Metal command queue",
    "failed to create Metal preview texture",
    "Metal preview state is not initialized",
    "Metal renderer state is not initialized",
    "Metal window/device is not initialized",
    "screenshot failed: framebuffer readback failed",
)


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def _default_binary(repo_root: Path) -> Path:
    candidates = [
        repo_root / "build" / "bin" / "imiv",
        repo_root / "build_u" / "bin" / "imiv",
        repo_root / "build" / "src" / "imiv" / "imiv",
        repo_root / "build_u" / "src" / "imiv" / "imiv",
        repo_root / "build" / "Debug" / "imiv.exe",
        repo_root / "build" / "Release" / "imiv.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def _default_oiiotool(repo_root: Path) -> Path:
    candidates = [
        repo_root / "build" / "bin" / "oiiotool",
        repo_root / "build_u" / "bin" / "oiiotool",
        repo_root / "build" / "src" / "oiiotool" / "oiiotool",
        repo_root / "build_u" / "src" / "oiiotool" / "oiiotool",
        repo_root / "build" / "Debug" / "oiiotool.exe",
        repo_root / "build" / "Release" / "oiiotool.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return Path("oiiotool")


def _default_env_script(repo_root: Path, exe: Path | None = None) -> Path:
    candidates: list[Path] = []
    if exe is not None:
        exe = exe.resolve()
        candidates.extend(
            [
                exe.parent / "imiv_env.sh",
                exe.parent.parent / "imiv_env.sh",
            ]
        )
    candidates.extend(
        [
            repo_root / "build" / "imiv_env.sh",
            repo_root / "build_u" / "imiv_env.sh",
        ]
    )
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


def _write_prefs(config_home: Path, *, linear_interpolation: bool) -> Path:
    prefs_dir = config_home / "OpenImageIO" / "imiv"
    prefs_dir.mkdir(parents=True, exist_ok=True)
    prefs_path = prefs_dir / "imiv.inf"
    prefs_text = (
        "[ImivApp][State]\n"
        f"linear_interpolation={1 if linear_interpolation else 0}\n"
        "fit_image_to_window=1\n"
        "pixelview_follows_mouse=1\n"
        "show_mouse_mode_selector=0\n"
    )
    prefs_path.write_text(prefs_text, encoding="utf-8")
    return prefs_path


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
        if item.get("debug") == "image: Image":
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


def _run_case(
    repo_root: Path,
    runner: Path,
    exe: Path,
    cwd: Path,
    base_env: dict[str, str],
    out_dir: Path,
    image_path: Path,
    name: str,
    *,
    linear_interpolation: bool,
    trace: bool,
) -> tuple[Path, Path, Path]:
    case_dir = out_dir / name
    case_dir.mkdir(parents=True, exist_ok=True)
    config_home = case_dir / "cfg"
    _write_prefs(config_home, linear_interpolation=linear_interpolation)
    env = dict(base_env)
    env["IMIV_CONFIG_HOME"] = str(config_home)

    screenshot_path = case_dir / f"{name}.png"
    layout_path = case_dir / f"{name}.layout.json"
    log_path = case_dir / f"{name}.log"

    cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(cwd),
        "--open",
        str(image_path),
        "--screenshot-out",
        str(screenshot_path),
        "--layout-json-out",
        str(layout_path),
        "--layout-items",
    ]
    if trace:
        cmd.append("--trace")

    with log_path.open("w", encoding="utf-8") as log_handle:
        proc = subprocess.run(
            cmd,
            cwd=str(repo_root),
            env=env,
            check=False,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            timeout=90,
        )
    if proc.returncode != 0:
        raise RuntimeError(f"{name}: runner exited with code {proc.returncode}")
    if not screenshot_path.exists():
        raise RuntimeError(f"{name}: screenshot not written")
    if not layout_path.exists():
        raise RuntimeError(f"{name}: layout json not written")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            raise RuntimeError(f"{name}: found runtime error pattern: {pattern}")

    return screenshot_path, layout_path, log_path


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
    repo_root = _repo_root()
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument("--oiiotool", default=str(_default_oiiotool(repo_root)), help="oiiotool executable")
    ap.add_argument("--env-script", default="", help="Optional shell env setup script")
    ap.add_argument("--out-dir", default="", help="Output directory")
    ap.add_argument("--trace", action="store_true", help="Enable runner tracing")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        print(f"error: binary not found: {exe}", file=sys.stderr)
        return 2

    oiiotool = Path(args.oiiotool).resolve()
    if not oiiotool.exists():
        print(f"error: oiiotool not found: {oiiotool}", file=sys.stderr)
        return 2

    cwd = Path(args.cwd).resolve() if args.cwd else exe.parent.resolve()
    out_dir = (
        Path(args.out_dir).resolve()
        if args.out_dir
        else exe.parent.parent / "imiv_captures" / "metal_sampling_regression"
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    env_script = (
        Path(args.env_script).resolve()
        if args.env_script
        else _default_env_script(repo_root, exe)
    )
    base_env = _load_env_from_script(env_script)

    image_path = out_dir / "sampling_checker_input.tif"
    _build_fixture(oiiotool, image_path)

    nearest_screenshot, nearest_layout, _ = _run_case(
        repo_root,
        runner,
        exe,
        cwd,
        base_env,
        out_dir,
        image_path,
        "nearest",
        linear_interpolation=False,
        trace=args.trace,
    )
    linear_screenshot, linear_layout, _ = _run_case(
        repo_root,
        runner,
        exe,
        cwd,
        base_env,
        out_dir,
        image_path,
        "linear",
        linear_interpolation=True,
        trace=args.trace,
    )

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
        print("error: invalid normalized crop size", file=sys.stderr)
        return 1

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
        print(
            f"error: nearest and linear preview crops are too similar (mean abs diff={diff:.4f})",
            file=sys.stderr,
        )
        return 1
    if nearest_midtones > 0.35:
        print(
            "error: nearest preview still looks blurred "
            f"(midtone fraction={nearest_midtones:.4f})",
            file=sys.stderr,
        )
        return 1
    if linear_midtones <= nearest_midtones + 0.10:
        print(
            "error: linear preview does not look materially smoother than nearest "
            f"(nearest={nearest_midtones:.4f}, linear={linear_midtones:.4f})",
            file=sys.stderr,
        )
        return 1

    print(f"ok: Metal sampling regression outputs are in {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

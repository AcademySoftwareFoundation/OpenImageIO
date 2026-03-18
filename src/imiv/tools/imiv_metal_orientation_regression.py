#!/usr/bin/env python3
"""Regression check for Metal image orientation in imiv."""

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


def _crop_image(
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


def _resize_image(oiiotool: Path, source: Path, width: int, height: int, dest: Path) -> None:
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


def _transform_image(
    oiiotool: Path,
    source: Path,
    width: int,
    height: int,
    dest: Path,
    *,
    flip: bool = False,
    flop: bool = False,
) -> None:
    cmd = [str(oiiotool), str(source), "--resize", f"{width}x{height}"]
    if flip:
        cmd.append("--flip")
    if flop:
        cmd.append("--flop")
    cmd.extend(["--ch", "R,G,B"])
    cmd.extend(["-o", str(dest)])
    subprocess.run(cmd, check=True)


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
    count = len(lhs_pixels)
    for a, b in zip(lhs_pixels, rhs_pixels):
        total += abs(a - b)
    return total / max(1, count)


def main() -> int:
    repo_root = _repo_root()
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument("--env-script", default="", help="Optional shell env setup script")
    ap.add_argument("--oiiotool", default=str(_default_oiiotool(repo_root)), help="oiiotool executable")
    ap.add_argument("--out-dir", default="", help="Output directory")
    ap.add_argument("--open", default=str(default_image), help="Image to open")
    ap.add_argument("--trace", action="store_true", help="Enable runner tracing")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        print(f"error: binary not found: {exe}", file=sys.stderr)
        return 2

    cwd = Path(args.cwd).resolve() if args.cwd else exe.parent.resolve()
    out_dir = (
        Path(args.out_dir).resolve()
        if args.out_dir
        else exe.parent.parent / "imiv_captures" / "metal_orientation_regression"
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    image_path = Path(args.open).resolve()
    if not image_path.exists():
        print(f"error: image not found: {image_path}", file=sys.stderr)
        return 2

    oiiotool = Path(args.oiiotool).expanduser()
    if not oiiotool.exists():
        found = shutil.which(str(oiiotool))
        if found is None:
            print(f"error: oiiotool not found: {oiiotool}", file=sys.stderr)
            return 2
        oiiotool = Path(found)
    oiiotool = oiiotool.resolve()

    env_script = (
        Path(args.env_script).resolve()
        if args.env_script
        else _default_env_script(repo_root, exe)
    )
    env = _load_env_from_script(env_script)
    env["IMIV_CONFIG_HOME"] = str(out_dir / "cfg")

    screenshot_path = out_dir / "metal_orientation.png"
    layout_path = out_dir / "metal_orientation.layout.json"
    state_path = out_dir / "metal_orientation.state.json"
    log_path = out_dir / "metal_orientation.log"

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
        "--state-json-out",
        str(state_path),
    ]
    if args.trace:
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
        print(f"error: runner exited with code {proc.returncode}", file=sys.stderr)
        return 1

    for required in (screenshot_path, layout_path, state_path):
        if not required.exists():
            print(f"error: missing output: {required}", file=sys.stderr)
            return 1

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            print(f"error: found runtime error pattern: {pattern}", file=sys.stderr)
            return 1

    crop_rect = _image_crop_rect(layout_path)
    crop_path = out_dir / "metal_orientation.crop.ppm"
    expected_path = out_dir / "metal_orientation.expected.ppm"
    flop_path = out_dir / "metal_orientation.flop.ppm"
    flip_path = out_dir / "metal_orientation.flip.ppm"
    flopflip_path = out_dir / "metal_orientation.flopflip.ppm"

    _crop_image(oiiotool, screenshot_path, crop_rect, crop_path)
    width, height, _ = _read_ppm(crop_path)
    if width <= 0 or height <= 0:
        print("error: invalid cropped image size", file=sys.stderr)
        return 1
    _resize_image(oiiotool, image_path, width, height, expected_path)
    _transform_image(oiiotool, image_path, width, height, flop_path, flop=True)
    _transform_image(oiiotool, image_path, width, height, flip_path, flip=True)
    _transform_image(
        oiiotool,
        image_path,
        width,
        height,
        flopflip_path,
        flip=True,
        flop=True,
    )

    scores = {
        "expected": _mean_abs_diff(crop_path, expected_path),
        "flop": _mean_abs_diff(crop_path, flop_path),
        "flip": _mean_abs_diff(crop_path, flip_path),
        "flopflip": _mean_abs_diff(crop_path, flopflip_path),
    }
    best_name, best_score = min(scores.items(), key=lambda item: item[1])
    if best_name != "expected":
        print(
            "error: Metal orientation mismatch; best match was "
            f"{best_name} (scores: {scores})",
            file=sys.stderr,
        )
        return 1

    wrong_best = min(
        value for key, value in scores.items() if key != "expected"
    )
    if not (scores["expected"] < wrong_best):
        print(
            "error: Metal orientation comparison was inconclusive "
            f"(scores: {scores})",
            file=sys.stderr,
        )
        return 1

    print(
        "ok: Metal orientation regression outputs are in "
        f"{out_dir} (scores: {scores})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

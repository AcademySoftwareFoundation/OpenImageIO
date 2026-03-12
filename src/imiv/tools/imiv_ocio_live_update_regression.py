#!/usr/bin/env python3
"""Regression check for live OCIO display/view updates in imiv."""

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
    "VUID-",
    "fatal Vulkan error",
    "OCIO runtime shader preflight failed",
    "vkCreateShaderModule failed",
    "error: imiv exited with code",
)


def _default_binary(repo_root: Path) -> Path:
    candidates = [
        repo_root / "build_u" / "bin" / "imiv",
        repo_root / "build" / "bin" / "imiv",
        repo_root / "build_u" / "src" / "imiv" / "imiv",
        repo_root / "build" / "src" / "imiv" / "imiv",
        repo_root / "build" / "Debug" / "imiv.exe",
        repo_root / "build" / "Release" / "imiv.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def _default_oiiotool(repo_root: Path) -> Path:
    candidates = [
        repo_root / "build_u" / "bin" / "oiiotool",
        repo_root / "build" / "bin" / "oiiotool",
        Path("/mnt/f/UBc/Release/bin/oiiotool"),
        Path("/mnt/f/UBc/Debug/bin/oiiotool"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return Path("oiiotool")


def _default_idiff(repo_root: Path) -> Path:
    candidates = [
        repo_root / "build_u" / "bin" / "idiff",
        repo_root / "build" / "bin" / "idiff",
        Path("/mnt/f/UBc/Release/bin/idiff"),
        Path("/mnt/f/UBc/Debug/bin/idiff"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return Path("idiff")


def _default_ocio_config(repo_root: Path) -> Path:
    preferred = (
        repo_root
        / "temp"
        / "studio-config-all-views-v4.0.0_aces-v2.0_ocio-v2.5.ocio"
    )
    if preferred.exists():
        return preferred
    for candidate in sorted((repo_root / "temp").glob("*.ocio")):
        return candidate
    return preferred


def _resolve_existing_tool(requested: str, fallback: Path) -> Path:
    candidate = Path(requested)
    if requested:
        candidate = candidate.expanduser()
        if candidate.exists():
            return candidate.resolve()
    if fallback.exists():
        return fallback.resolve()
    return candidate


def _load_env_from_script(script_path: Path) -> dict[str, str]:
    env = dict(os.environ)
    if not script_path.exists():
        return env
    if shutil.which("bash") is None:
        return env

    quoted = shlex.quote(str(script_path))
    proc = subprocess.run(
        ["bash", "-lc", f"source {quoted} >/dev/null 2>&1; env -0"],
        check=True,
        stdout=subprocess.PIPE,
    )
    loaded: dict[str, str] = {}
    for item in proc.stdout.split(b"\0"):
        if not item:
            continue
        key, _, value = item.partition(b"=")
        if not key:
            continue
        loaded[key.decode("utf-8", errors="ignore")] = value.decode(
            "utf-8", errors="ignore"
        )
    env.update(loaded)
    return env


def _generate_probe_image(oiiotool: Path, image_path: Path) -> None:
    image_path.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(oiiotool),
        "--create",
        "96x48",
        "3",
        "--d",
        "float",
        "--fill:color=4.0,0.5,0.1",
        "0,0,96,48",
        "--attrib",
        "oiio:ColorSpace",
        "ACEScg",
        "-o",
        str(image_path),
    ]
    subprocess.run(cmd, check=True)


def _run_case(
    exe: Path,
    cwd: Path,
    env: dict[str, str],
    out_dir: Path,
    image_path: Path,
    name: str,
    extra_args: list[str],
    extra_env: dict[str, str] | None = None,
) -> tuple[Path, Path, Path]:
    screenshot_path = out_dir / f"{name}.png"
    layout_path = out_dir / f"{name}.json"
    log_path = out_dir / f"{name}.log"
    exe_cmd = [str(exe), "-F", *extra_args, str(image_path)]

    run_env = dict(env)
    run_env.update(
        {
            "OCIO": run_env["OCIO"],
            "IMIV_IMGUI_TEST_ENGINE": "1",
            "IMIV_IMGUI_TEST_ENGINE_EXIT_ON_FINISH": "1",
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT": "1",
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_OUT": str(screenshot_path),
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_DELAY_FRAMES": "10",
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_FRAMES": "1",
            "IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP": "1",
            "IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_OUT": str(layout_path),
            "IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_ITEMS": "1",
            "IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_DELAY_FRAMES": "10",
        }
    )
    if extra_env:
        run_env.update(extra_env)

    with log_path.open("w", encoding="utf-8") as log_handle:
        proc = subprocess.run(
            exe_cmd,
            cwd=str(cwd),
            env=run_env,
            check=False,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            timeout=45,
        )
    if proc.returncode != 0:
        raise RuntimeError(f"{name}: imiv exited with code {proc.returncode}")
    if not screenshot_path.exists():
        raise RuntimeError(f"{name}: screenshot not written")
    if not layout_path.exists():
        raise RuntimeError(f"{name}: layout json not written")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            raise RuntimeError(f"{name}: found runtime error pattern: {pattern}")

    return screenshot_path, layout_path, log_path


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

    items = image_window.get("items", [])
    best_rect = None
    best_area = -1.0
    for item in items:
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

    if best_rect is None:
        rect = image_window.get("rect")
        if not rect:
            raise RuntimeError(f"{layout_path.name}: missing crop rect")
        best_rect = rect

    x0 = int(math.floor(float(best_rect["min"][0]) - origin_x)) + 1
    y0 = int(math.floor(float(best_rect["min"][1]) - origin_y)) + 1
    x1 = int(math.ceil(float(best_rect["max"][0]) - origin_x)) - 2
    y1 = int(math.ceil(float(best_rect["max"][1]) - origin_y)) - 2
    if x1 <= x0 or y1 <= y0:
        raise RuntimeError(f"{layout_path.name}: invalid crop rect")
    return x0, y0, x1, y1


def _crop_image(oiiotool: Path, source: Path, crop_rect: tuple[int, int, int, int], dest: Path) -> None:
    x0, y0, x1, y1 = crop_rect
    cmd = [
        str(oiiotool),
        str(source),
        "--crop",
        f"{x0},{y0},{x1},{y1}",
        "-o",
        str(dest),
    ]
    subprocess.run(cmd, check=True)


def _images_identical(idiff: Path, lhs: Path, rhs: Path) -> bool:
    proc = subprocess.run(
        [str(idiff), "-q", "-a", str(lhs), str(rhs)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    return proc.returncode == 0


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    default_out = repo_root / "build_u" / "imiv_captures" / "ocio_live_update_regression"
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_image = default_out / "ocio_live_input.exr"
    default_config = _default_ocio_config(repo_root)

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument("--oiiotool", default=str(_default_oiiotool(repo_root)), help="oiiotool executable")
    ap.add_argument("--idiff", default=str(_default_idiff(repo_root)), help="idiff executable")
    ap.add_argument("--env-script", default=str(default_env_script), help="Optional shell env script")
    ap.add_argument("--out-dir", default=str(default_out), help="Artifact directory")
    ap.add_argument("--image", default=str(default_image), help="Generated input image path")
    ap.add_argument("--ocio-config", default=str(default_config), help="OCIO config path")
    ap.add_argument("--display", default="sRGB - Display", help="Initial OCIO display")
    ap.add_argument("--target-display", default="",
                    help="Target OCIO display (defaults to --display)")
    ap.add_argument("--raw-view", default="Raw", help="Initial OCIO view")
    ap.add_argument("--target-view", default="ACES 2.0 - SDR 100 nits (Rec.709)",
                    help="Live switch target OCIO view")
    ap.add_argument("--image-color-space", default="ACEScg", help="Input image color space")
    ap.add_argument("--apply-frame", type=int, default=5,
                    help="Frame number for live OCIO override")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        return _fail(f"binary not found: {exe}")

    oiiotool = _resolve_existing_tool(
        args.oiiotool, _default_oiiotool(repo_root)
    )
    idiff = _resolve_existing_tool(args.idiff, _default_idiff(repo_root))
    ocio_config = Path(args.ocio_config).resolve()
    if not ocio_config.exists():
        return _fail(f"OCIO config not found: {ocio_config}")

    run_cwd = Path(args.cwd).resolve() if args.cwd else exe.parent.resolve()
    out_dir = Path(args.out_dir).resolve()
    image_path = Path(args.image).resolve()
    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    config_home = out_dir / "config_home"
    config_home.mkdir(parents=True, exist_ok=True)

    env = _load_env_from_script(Path(args.env_script).resolve())
    env["IMIV_CONFIG_HOME"] = str(config_home)
    env["OCIO"] = str(ocio_config)

    target_display = args.target_display if args.target_display else args.display

    common_args = [
        "--display",
        args.display,
        "--view",
        args.raw_view,
        "--image-color-space",
        args.image_color_space,
    ]
    target_args = [
        "--display",
        target_display,
        "--view",
        args.target_view,
        "--image-color-space",
        args.image_color_space,
    ]

    try:
        _generate_probe_image(oiiotool, image_path)

        static_raw_png, static_raw_layout, static_raw_log = _run_case(
            exe, run_cwd, env, out_dir, image_path, "static_raw", common_args
        )
        static_target_png, static_target_layout, static_target_log = _run_case(
            exe, run_cwd, env, out_dir, image_path, "static_target", target_args
        )
        live_noop_png, live_noop_layout, live_noop_log = _run_case(
            exe,
            run_cwd,
            env,
            out_dir,
            image_path,
            "live_noop_raw",
            common_args,
            {
                "IMIV_IMGUI_TEST_ENGINE_OCIO_DISPLAY": args.display,
                "IMIV_IMGUI_TEST_ENGINE_OCIO_VIEW": args.raw_view,
                "IMIV_IMGUI_TEST_ENGINE_OCIO_IMAGE_COLOR_SPACE": args.image_color_space,
                "IMIV_IMGUI_TEST_ENGINE_OCIO_APPLY_FRAME": str(args.apply_frame),
            },
        )
        live_switch_png, live_switch_layout, live_switch_log = _run_case(
            exe,
            run_cwd,
            env,
            out_dir,
            image_path,
            "live_switch",
            common_args,
            {
                "IMIV_IMGUI_TEST_ENGINE_OCIO_DISPLAY": target_display,
                "IMIV_IMGUI_TEST_ENGINE_OCIO_VIEW": args.target_view,
                "IMIV_IMGUI_TEST_ENGINE_OCIO_IMAGE_COLOR_SPACE": args.image_color_space,
                "IMIV_IMGUI_TEST_ENGINE_OCIO_APPLY_FRAME": str(args.apply_frame),
            },
        )

        crop_dir = out_dir / "crops"
        crop_dir.mkdir(parents=True, exist_ok=True)
        static_raw_crop = crop_dir / "static_raw.png"
        static_target_crop = crop_dir / "static_target.png"
        live_noop_crop = crop_dir / "live_noop_raw.png"
        live_switch_crop = crop_dir / "live_switch.png"

        _crop_image(
            oiiotool,
            static_raw_png,
            _image_crop_rect(static_raw_layout),
            static_raw_crop,
        )
        _crop_image(
            oiiotool,
            static_target_png,
            _image_crop_rect(static_target_layout),
            static_target_crop,
        )
        _crop_image(
            oiiotool,
            live_noop_png,
            _image_crop_rect(live_noop_layout),
            live_noop_crop,
        )
        _crop_image(
            oiiotool,
            live_switch_png,
            _image_crop_rect(live_switch_layout),
            live_switch_crop,
        )
    except (OSError, subprocess.CalledProcessError, RuntimeError, subprocess.TimeoutExpired) as exc:
        return _fail(str(exc))

    if not _images_identical(idiff, static_raw_crop, live_noop_crop):
        return _fail("live noop OCIO update changed the image region unexpectedly")
    if _images_identical(idiff, static_raw_crop, live_switch_crop):
        return _fail("live OCIO view switch did not update the image region")
    if not _images_identical(idiff, static_target_crop, live_switch_crop):
        return _fail("live OCIO view switch does not match the static target view")

    print("display:", args.display)
    print("initial display:", args.display)
    print("target display:", target_display)
    print("initial view:", args.raw_view)
    print("target view:", args.target_view)
    print("image color space:", args.image_color_space)
    print("OCIO config:", ocio_config)
    print("static raw:", static_raw_png)
    print("static target:", static_target_png)
    print("live noop:", live_noop_png)
    print("live switch:", live_switch_png)
    print("crop static raw:", static_raw_crop)
    print("crop static target:", static_target_crop)
    print("crop live noop:", live_noop_crop)
    print("crop live switch:", live_switch_crop)
    print("logs:", static_raw_log, static_target_log, live_noop_log, live_switch_log)
    print("artifacts:", out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

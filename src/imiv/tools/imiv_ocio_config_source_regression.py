#!/usr/bin/env python3
"""Regression check for OCIO config-source selection and builtin fallback."""

from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional

from imiv_test_utils import (
    default_binary,
    default_env_script,
    default_idiff,
    default_oiiotool,
    fail,
    load_env_from_script,
    repo_root as imiv_repo_root,
    resolve_existing_tool,
    resolve_run_cwd,
    runner_path,
)


ERROR_PATTERNS = (
    "VUID-",
    "fatal Vulkan error",
    "error: imiv exited with code",
)


SOURCE_GLOBAL = 0
SOURCE_BUILTIN = 1
SOURCE_USER = 2


def _default_case_timeout() -> float:
    return 180.0 if os.name == "nt" else 60.0


def _default_ocio_config(repo_root: Path) -> str:
    return "ocio://default"


def _resolve_ocio_config_argument(value: str) -> str:
    candidate = str(value).strip()
    if candidate.startswith("ocio://"):
        return candidate
    return str(Path(candidate).expanduser().resolve())


def _json_load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _write_prefs(
    config_home: Path,
    *,
    use_ocio: bool,
    ocio_config_source: int,
    ocio_display: str,
    ocio_view: str,
    ocio_image_color_space: str,
    ocio_user_config_path: str = "",
) -> Path:
    prefs_dir = config_home / "OpenImageIO" / "imiv"
    prefs_dir.mkdir(parents=True, exist_ok=True)
    prefs_path = prefs_dir / "imiv.inf"
    prefs_text = (
        "[ImivApp][State]\n"
        f"use_ocio={1 if use_ocio else 0}\n"
        f"ocio_config_source={ocio_config_source}\n"
        f"ocio_display={ocio_display}\n"
        f"ocio_view={ocio_view}\n"
        f"ocio_image_color_space={ocio_image_color_space}\n"
        f"ocio_user_config_path={ocio_user_config_path}\n"
    )
    prefs_path.write_text(prefs_text, encoding="utf-8")
    return prefs_path


def _generate_probe_image(repo_root: Path, oiiotool: Path, image_path: Path) -> None:
    image_path.parent.mkdir(parents=True, exist_ok=True)
    chart_image = repo_root / "testsuite" / "imiv" / "images" / "CC988_ACEScg.exr"
    if chart_image.exists():
        shutil.copyfile(chart_image, image_path)
        return
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
    repo_root: Path,
    runner: Path,
    exe: Path,
    cwd: Path,
    backend: str,
    env: dict[str, str],
    image_path: Path,
    out_dir: Path,
    name: str,
    trace: bool,
    case_timeout: float,
    *,
    capture_screenshot: bool = True,
    capture_layout: bool = True,
    capture_state: bool = True,
) -> tuple[Optional[Path], Optional[Path], Optional[Path], Path]:
    screenshot_path = out_dir / f"{name}.png" if capture_screenshot else None
    layout_path = out_dir / f"{name}.json" if capture_layout else None
    state_path = out_dir / f"{name}.state.json" if capture_state else None
    log_path = out_dir / f"{name}.log"
    cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(cwd),
    ]
    if backend:
        cmd.extend(["--backend", backend])
    cmd.extend(["--open", str(image_path)])
    if screenshot_path is not None:
        cmd.extend(["--screenshot-out", str(screenshot_path)])
    if layout_path is not None:
        cmd.extend(["--layout-json-out", str(layout_path), "--layout-items"])
    if state_path is not None:
        cmd.extend(["--state-json-out", str(state_path)])
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
            timeout=case_timeout,
        )
    if proc.returncode != 0:
        raise RuntimeError(f"{name}: runner exited with code {proc.returncode}")
    if screenshot_path is not None and not screenshot_path.exists():
        raise RuntimeError(f"{name}: screenshot not written")
    if layout_path is not None and not layout_path.exists():
        raise RuntimeError(f"{name}: layout json not written")
    if state_path is not None and not state_path.exists():
        raise RuntimeError(f"{name}: state json not written")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            raise RuntimeError(f"{name}: found runtime error pattern: {pattern}")
    return screenshot_path, layout_path, state_path, log_path


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


def _crop_image(
    oiiotool: Path, source: Path, crop_rect: tuple[int, int, int, int], dest: Path
) -> None:
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


def _write_rgb_ppm(
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
    count = len(lhs_pixels)
    for a, b in zip(lhs_pixels, rhs_pixels):
        total += abs(a - b)
    return total / max(1, count)


def _normalized_rgb_diff(
    oiiotool: Path, lhs: Path, rhs: Path, work_dir: Path, stem: str
) -> float:
    lhs_ppm = work_dir / f"{stem}.lhs.ppm"
    rhs_ppm = work_dir / f"{stem}.rhs.ppm"
    _write_rgb_ppm(oiiotool, lhs, 256, 256, lhs_ppm)
    _write_rgb_ppm(oiiotool, rhs, 256, 256, rhs_ppm)
    return _mean_abs_diff(lhs_ppm, rhs_ppm)


def _validate_ocio_state(
    name: str,
    state_path: Path,
    *,
    image_path: Path,
    expected_use_ocio: bool,
    expected_requested_source: str,
    expected_resolved_source: str,
    expected_fallback_applied: bool,
    expected_resolved_config_path: str,
) -> dict:
    state = _json_load(state_path)
    if not bool(state.get("image_loaded")):
        raise RuntimeError(f"{name}: image was not loaded")
    if str(state.get("image_path", "")).strip() != str(image_path):
        raise RuntimeError(
            f"{name}: unexpected image path {state.get('image_path', '')!r}"
        )

    ocio = state.get("ocio")
    if not isinstance(ocio, dict):
        raise RuntimeError(f"{name}: missing ocio state block")
    if bool(ocio.get("use_ocio")) != expected_use_ocio:
        raise RuntimeError(
            f"{name}: unexpected use_ocio={ocio.get('use_ocio')!r}"
        )
    if str(ocio.get("requested_source", "")).strip() != expected_requested_source:
        raise RuntimeError(
            f"{name}: unexpected requested_source="
            f"{ocio.get('requested_source', '')!r}"
        )
    if str(ocio.get("resolved_source", "")).strip() != expected_resolved_source:
        raise RuntimeError(
            f"{name}: unexpected resolved_source="
            f"{ocio.get('resolved_source', '')!r}"
        )
    if bool(ocio.get("fallback_applied")) != expected_fallback_applied:
        raise RuntimeError(
            f"{name}: unexpected fallback_applied="
            f"{ocio.get('fallback_applied')!r}"
        )
    if (
        str(ocio.get("resolved_config_path", "")).strip()
        != expected_resolved_config_path
    ):
        raise RuntimeError(
            f"{name}: unexpected resolved_config_path="
            f"{ocio.get('resolved_config_path', '')!r}"
        )
    if not bool(ocio.get("menu_data_ok")):
        raise RuntimeError(
            f"{name}: OCIO menu data unavailable: "
            f"{str(ocio.get('menu_error', '')).strip()}"
        )

    displays = ocio.get("available_displays")
    if not isinstance(displays, list) or not displays:
        raise RuntimeError(f"{name}: available_displays is empty")
    resolved_display = str(ocio.get("resolved_display", "")).strip()
    if not resolved_display:
        raise RuntimeError(f"{name}: resolved_display is empty")
    if resolved_display not in [str(item).strip() for item in displays]:
        raise RuntimeError(
            f"{name}: resolved_display {resolved_display!r} is not in "
            "available_displays"
        )

    views_by_display = ocio.get("views_by_display")
    if not isinstance(views_by_display, dict):
        raise RuntimeError(f"{name}: missing views_by_display")
    display_views = views_by_display.get(resolved_display)
    if not isinstance(display_views, list) or not display_views:
        raise RuntimeError(
            f"{name}: no views advertised for display {resolved_display!r}"
        )
    resolved_view = str(ocio.get("resolved_view", "")).strip()
    if not resolved_view:
        raise RuntimeError(f"{name}: resolved_view is empty")
    if resolved_view not in [str(item).strip() for item in display_views]:
        raise RuntimeError(
            f"{name}: resolved_view {resolved_view!r} is not valid for "
            f"display {resolved_display!r}"
        )

    color_spaces = ocio.get("available_image_color_spaces")
    if not isinstance(color_spaces, list) or not color_spaces:
        raise RuntimeError(f"{name}: available_image_color_spaces is empty")
    return state


if __name__ == "__main__":
    repo_root = imiv_repo_root()
    default_runner = runner_path(repo_root)
    default_out = repo_root / "build_u" / "imiv_captures" / "ocio_config_source_regression"

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
        default=str(default_env_script(repo_root)),
        help="Optional shell env setup script",
    )
    ap.add_argument("--out-dir", default=str(default_out), help="Output directory")
    ap.add_argument("--ocio-config", default=str(_default_ocio_config(repo_root)), help="OCIO config to use")
    ap.add_argument("--oiiotool", default=str(default_oiiotool(repo_root)), help="oiiotool path")
    ap.add_argument("--idiff", default=str(default_idiff(repo_root)), help="idiff path")
    ap.add_argument(
        "--case-timeout",
        type=float,
        default=_default_case_timeout(),
        help="Per-case GUI runner timeout in seconds",
    )
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        raise SystemExit(fail(f"binary not found: {exe}"))
    cwd = resolve_run_cwd(exe, args.cwd)

    runner = default_runner.resolve()
    if not runner.exists():
        raise SystemExit(fail(f"runner not found: {runner}"))

    ocio_config = _resolve_ocio_config_argument(args.ocio_config)
    if not ocio_config.startswith("ocio://"):
        ocio_config_path = Path(ocio_config)
        if not ocio_config_path.exists():
            raise SystemExit(fail(f"OCIO config not found: {ocio_config_path}"))

    oiiotool = resolve_existing_tool(args.oiiotool, default_oiiotool(repo_root))
    if not oiiotool.exists():
        raise SystemExit(fail(f"oiiotool not found: {oiiotool}"))

    idiff = resolve_existing_tool(args.idiff, default_idiff(repo_root))
    if not idiff.exists():
        raise SystemExit(fail(f"idiff not found: {idiff}"))

    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    image_path = out_dir / "ocio_source_fixture.exr"
    _generate_probe_image(repo_root, oiiotool, image_path)

    if ocio_config.startswith("ocio://"):
        external_display = "default"
        external_view = "default"
        external_color_space = "auto"
    else:
        external_display = "sRGB - Display"
        external_view = "Un-tone-mapped"
        external_color_space = "ACEScg"

    builtin_display = "default"
    builtin_view = "default"
    builtin_color_space = "auto"

    base_env = load_env_from_script(Path(args.env_script).expanduser())
    base_env.pop("OCIO", None)

    baseline_cfg = out_dir / "cfg_baseline"
    _write_prefs(
        baseline_cfg,
        use_ocio=False,
        ocio_config_source=SOURCE_GLOBAL,
        ocio_display=builtin_display,
        ocio_view=builtin_view,
        ocio_image_color_space=builtin_color_space,
    )
    baseline_env = dict(base_env)
    baseline_env["IMIV_CONFIG_HOME"] = str(baseline_cfg)

    global_cfg = out_dir / "cfg_global"
    _write_prefs(
        global_cfg,
        use_ocio=True,
        ocio_config_source=SOURCE_GLOBAL,
        ocio_display=external_display,
        ocio_view=external_view,
        ocio_image_color_space=external_color_space,
    )
    global_env = dict(base_env)
    global_env["IMIV_CONFIG_HOME"] = str(global_cfg)
    global_env["OCIO"] = ocio_config

    global_default_cfg = out_dir / "cfg_global_default"
    _write_prefs(
        global_default_cfg,
        use_ocio=True,
        ocio_config_source=SOURCE_GLOBAL,
        ocio_display="default",
        ocio_view="default",
        ocio_image_color_space=external_color_space,
    )
    global_default_env = dict(base_env)
    global_default_env["IMIV_CONFIG_HOME"] = str(global_default_cfg)
    global_default_env["OCIO"] = ocio_config

    global_invalid_cfg = out_dir / "cfg_global_invalid_selection"
    _write_prefs(
        global_invalid_cfg,
        use_ocio=True,
        ocio_config_source=SOURCE_GLOBAL,
        ocio_display="Missing Display",
        ocio_view="Missing View",
        ocio_image_color_space=external_color_space,
    )
    global_invalid_env = dict(base_env)
    global_invalid_env["IMIV_CONFIG_HOME"] = str(global_invalid_cfg)
    global_invalid_env["OCIO"] = ocio_config

    global_builtin_cfg = out_dir / "cfg_global_builtin"
    _write_prefs(
        global_builtin_cfg,
        use_ocio=True,
        ocio_config_source=SOURCE_GLOBAL,
        ocio_display=builtin_display,
        ocio_view=builtin_view,
        ocio_image_color_space=builtin_color_space,
    )
    global_builtin_env = dict(base_env)
    global_builtin_env["IMIV_CONFIG_HOME"] = str(global_builtin_cfg)

    builtin_cfg = out_dir / "cfg_builtin"
    _write_prefs(
        builtin_cfg,
        use_ocio=True,
        ocio_config_source=SOURCE_BUILTIN,
        ocio_display=builtin_display,
        ocio_view=builtin_view,
        ocio_image_color_space=builtin_color_space,
    )
    builtin_env = dict(base_env)
    builtin_env["IMIV_CONFIG_HOME"] = str(builtin_cfg)

    user_cfg = out_dir / "cfg_user"
    _write_prefs(
        user_cfg,
        use_ocio=True,
        ocio_config_source=SOURCE_USER,
        ocio_display=external_display,
        ocio_view=external_view,
        ocio_image_color_space=external_color_space,
        ocio_user_config_path=ocio_config,
    )
    user_env = dict(base_env)
    user_env["IMIV_CONFIG_HOME"] = str(user_cfg)

    user_missing_builtin_cfg = out_dir / "cfg_user_missing_builtin"
    _write_prefs(
        user_missing_builtin_cfg,
        use_ocio=True,
        ocio_config_source=SOURCE_USER,
        ocio_display=builtin_display,
        ocio_view=builtin_view,
        ocio_image_color_space=builtin_color_space,
        ocio_user_config_path=str(out_dir / "missing_user_config.ocio"),
    )
    user_missing_builtin_env = dict(base_env)
    user_missing_builtin_env["IMIV_CONFIG_HOME"] = str(user_missing_builtin_cfg)

    expected_builtin_config_path = "ocio://default"
    expected_external_config_path = ocio_config

    try:
        baseline_png, baseline_layout, baseline_state, baseline_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            args.backend,
            baseline_env,
            image_path,
            out_dir,
            "baseline",
            args.trace,
            args.case_timeout,
        )
        global_png, global_layout, global_state, global_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            args.backend,
            global_env,
            image_path,
            out_dir,
            "global",
            args.trace,
            args.case_timeout,
        )
        global_default_png, global_default_layout, global_default_state, global_default_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            args.backend,
            global_default_env,
            image_path,
            out_dir,
            "global_default",
            args.trace,
            args.case_timeout,
        )
        global_invalid_png, global_invalid_layout, global_invalid_state, global_invalid_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            args.backend,
            global_invalid_env,
            image_path,
            out_dir,
            "global_invalid_selection",
            args.trace,
            args.case_timeout,
            capture_screenshot=False,
            capture_layout=False,
            capture_state=True,
        )
        global_builtin_png, global_builtin_layout, global_builtin_state, global_builtin_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            args.backend,
            global_builtin_env,
            image_path,
            out_dir,
            "global_builtin_fallback",
            args.trace,
            args.case_timeout,
        )
        builtin_png, builtin_layout, builtin_state, builtin_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            args.backend,
            builtin_env,
            image_path,
            out_dir,
            "builtin",
            args.trace,
            args.case_timeout,
        )
        user_png, user_layout, user_state, user_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            args.backend,
            user_env,
            image_path,
            out_dir,
            "user",
            args.trace,
            args.case_timeout,
        )
        user_missing_builtin_png, user_missing_builtin_layout, user_missing_builtin_state, user_missing_builtin_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            args.backend,
            user_missing_builtin_env,
            image_path,
            out_dir,
            "user_missing_builtin",
            args.trace,
            args.case_timeout,
        )
    except (subprocess.SubprocessError, RuntimeError) as exc:
        raise SystemExit(fail(str(exc)))

    try:
        baseline_state_data = _validate_ocio_state(
            "baseline",
            baseline_state,
            image_path=image_path,
            expected_use_ocio=False,
            expected_requested_source="global",
            expected_resolved_source="builtin",
            expected_fallback_applied=True,
            expected_resolved_config_path=expected_builtin_config_path,
        )
        global_state_data = _validate_ocio_state(
            "global",
            global_state,
            image_path=image_path,
            expected_use_ocio=True,
            expected_requested_source="global",
            expected_resolved_source="global",
            expected_fallback_applied=False,
            expected_resolved_config_path=expected_external_config_path,
        )
        global_default_state_data = _validate_ocio_state(
            "global_default",
            global_default_state,
            image_path=image_path,
            expected_use_ocio=True,
            expected_requested_source="global",
            expected_resolved_source="global",
            expected_fallback_applied=False,
            expected_resolved_config_path=expected_external_config_path,
        )
        global_invalid_state_data = _validate_ocio_state(
            "global_invalid_selection",
            global_invalid_state,
            image_path=image_path,
            expected_use_ocio=True,
            expected_requested_source="global",
            expected_resolved_source="global",
            expected_fallback_applied=False,
            expected_resolved_config_path=expected_external_config_path,
        )
        global_builtin_state_data = _validate_ocio_state(
            "global_builtin_fallback",
            global_builtin_state,
            image_path=image_path,
            expected_use_ocio=True,
            expected_requested_source="global",
            expected_resolved_source="builtin",
            expected_fallback_applied=True,
            expected_resolved_config_path=expected_builtin_config_path,
        )
        builtin_state_data = _validate_ocio_state(
            "builtin",
            builtin_state,
            image_path=image_path,
            expected_use_ocio=True,
            expected_requested_source="builtin",
            expected_resolved_source="builtin",
            expected_fallback_applied=False,
            expected_resolved_config_path=expected_builtin_config_path,
        )
        user_state_data = _validate_ocio_state(
            "user",
            user_state,
            image_path=image_path,
            expected_use_ocio=True,
            expected_requested_source="user",
            expected_resolved_source="user",
            expected_fallback_applied=False,
            expected_resolved_config_path=expected_external_config_path,
        )
        user_missing_builtin_state_data = _validate_ocio_state(
            "user_missing_builtin",
            user_missing_builtin_state,
            image_path=image_path,
            expected_use_ocio=True,
            expected_requested_source="user",
            expected_resolved_source="builtin",
            expected_fallback_applied=True,
            expected_resolved_config_path=expected_builtin_config_path,
        )
    except RuntimeError as exc:
        raise SystemExit(fail(str(exc)))

    global_default_ocio = global_default_state_data["ocio"]
    global_invalid_ocio = global_invalid_state_data["ocio"]
    if (
        str(global_invalid_ocio.get("display", "")).strip() != "Missing Display"
        or str(global_invalid_ocio.get("view", "")).strip() != "Missing View"
    ):
        raise SystemExit(
            fail(
                "global_invalid_selection: persisted invalid display/view were "
                "not preserved in state output"
            )
        )
    if (
        str(global_invalid_ocio.get("resolved_display", "")).strip()
        != str(global_default_ocio.get("resolved_display", "")).strip()
        or str(global_invalid_ocio.get("resolved_view", "")).strip()
        != str(global_default_ocio.get("resolved_view", "")).strip()
    ):
        raise SystemExit(
            fail(
                "global_invalid_selection: invalid persisted display/view did "
                "not resolve to the config defaults"
            )
        )

    baseline_crop = out_dir / "baseline_crop.png"
    global_crop = out_dir / "global_crop.png"
    global_default_crop = out_dir / "global_default_crop.png"
    global_builtin_crop = out_dir / "global_builtin_fallback_crop.png"
    builtin_crop = out_dir / "builtin_crop.png"
    user_crop = out_dir / "user_crop.png"
    user_missing_builtin_crop = out_dir / "user_missing_builtin_crop.png"

    _crop_image(oiiotool, baseline_png, _image_crop_rect(baseline_layout), baseline_crop)
    _crop_image(oiiotool, global_png, _image_crop_rect(global_layout), global_crop)
    _crop_image(
        oiiotool,
        global_default_png,
        _image_crop_rect(global_default_layout),
        global_default_crop,
    )
    _crop_image(
        oiiotool,
        global_builtin_png,
        _image_crop_rect(global_builtin_layout),
        global_builtin_crop,
    )
    _crop_image(oiiotool, builtin_png, _image_crop_rect(builtin_layout), builtin_crop)
    _crop_image(oiiotool, user_png, _image_crop_rect(user_layout), user_crop)
    _crop_image(
        oiiotool,
        user_missing_builtin_png,
        _image_crop_rect(user_missing_builtin_layout),
        user_missing_builtin_crop,
    )

    baseline_global_diff = _normalized_rgb_diff(
        oiiotool, baseline_crop, global_crop, out_dir, "baseline_vs_global"
    )
    if baseline_global_diff <= 4.0:
        raise SystemExit(
            fail(
                "global OCIO source matched non-OCIO baseline; expected a real "
                f"OCIO transform result (mean abs RGB diff={baseline_global_diff:.4f})"
            )
        )
    global_user_diff = _normalized_rgb_diff(
        oiiotool, global_crop, user_crop, out_dir, "global_vs_user"
    )
    if global_user_diff > 2.0:
        raise SystemExit(
            fail(
                "user OCIO source output differs from global "
                f"(mean abs RGB diff={global_user_diff:.4f})"
            )
        )
    global_builtin_diff = _normalized_rgb_diff(
        oiiotool,
        global_builtin_crop,
        builtin_crop,
        out_dir,
        "global_builtin_vs_builtin",
    )
    if global_builtin_diff > 2.0:
        raise SystemExit(
            fail(
                "global source did not fall back to builtin when $OCIO was "
                f"missing (mean abs RGB diff={global_builtin_diff:.4f})"
            )
        )
    builtin_user_missing_diff = _normalized_rgb_diff(
        oiiotool,
        builtin_crop,
        user_missing_builtin_crop,
        out_dir,
        "builtin_vs_user_missing_builtin",
    )
    if builtin_user_missing_diff > 2.0:
        raise SystemExit(
            fail(
                "user source did not fall back to builtin when user config was "
                f"missing (mean abs RGB diff={builtin_user_missing_diff:.4f})"
            )
        )

    print("baseline:", baseline_png)
    print("global:", global_png)
    print("global_default:", global_default_png)
    print("global_builtin_fallback:", global_builtin_png)
    print("builtin:", builtin_png)
    print("user:", user_png)
    print("user_missing_builtin:", user_missing_builtin_png)
    print("baseline_log:", baseline_log)
    print("global_log:", global_log)
    print("global_default_log:", global_default_log)
    print("global_invalid_selection_log:", global_invalid_log)
    print("global_builtin_fallback_log:", global_builtin_log)
    print("builtin_log:", builtin_log)
    print("user_log:", user_log)
    print("user_missing_builtin_log:", user_missing_builtin_log)
    print("baseline_state:", baseline_state)
    print("global_state:", global_state)
    print("global_default_state:", global_default_state)
    print("global_invalid_selection_state:", global_invalid_state)
    print("global_builtin_fallback_state:", global_builtin_state)
    print("builtin_state:", builtin_state)
    print("user_state:", user_state)
    print("user_missing_builtin_state:", user_missing_builtin_state)

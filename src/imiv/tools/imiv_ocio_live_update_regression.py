#!/usr/bin/env python3
"""Regression check for live OCIO display/view updates in imiv."""

from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

from imiv_test_utils import (
    default_binary,
    fail,
    load_env_from_script,
    path_for_imiv_output,
    repo_root as imiv_repo_root,
    resolve_existing_tool,
    resolve_run_cwd,
    run_logged_process,
    runner_command,
    runner_path,
)


ERROR_PATTERNS = (
    "VUID-",
    "fatal Vulkan error",
    "OCIO runtime shader preflight failed",
    "vkCreateShaderModule failed",
    "error: imiv exited with code",
)


def _default_case_timeout() -> float:
    return 180.0 if os.name == "nt" else 45.0


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


def _default_ocio_config(_: Path) -> str:
    return "ocio://default"


def _resolve_ocio_config_argument(value: str) -> str:
    candidate = str(value).strip()
    if not candidate:
        return ""
    if candidate.startswith("ocio://"):
        return candidate
    return str(Path(candidate).expanduser().resolve())


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


def _json_load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _string_list(value: object) -> list[str]:
    if not isinstance(value, list):
        return []
    result: list[str] = []
    for item in value:
        text = str(item).strip()
        if text:
            result.append(text)
    return result


def _views_by_display(ocio_state: dict) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    raw = ocio_state.get("views_by_display")
    if not isinstance(raw, dict):
        return result
    for key, value in raw.items():
        display_name = str(key).strip()
        if not display_name:
            continue
        result[display_name] = _string_list(value)
    return result


def _pick_first_other(values: list[str], current: str) -> str:
    for value in values:
        if value != current:
            return value
    return ""


def _pick_preferred_target_view(values: list[str], current: str) -> str:
    ranked: list[str] = []
    for value in values:
        if value == current:
            continue
        lowered = value.lower()
        penalty = 0
        if "raw" in lowered:
            penalty += 100
        if "video" in lowered:
            penalty += 40
        ranked.append((penalty, value))
    if not ranked:
        return ""
    ranked.sort(key=lambda item: (item[0], item[1]))
    return ranked[0][1]


def _display_priority(display_name: str, current_display: str) -> tuple[int, str]:
    name = display_name.lower()
    score = 100
    if display_name == current_display:
        score += 1000
    if "hdr" in name or "2100" in name or "pq" in name or "st2084" in name:
        score -= 50
    if "p3" in name:
        score -= 20
    if "1886" in name or "gamma 2.2" in name:
        score += 10
    return score, display_name


def _pick_image_color_space(
    requested: str, available_color_spaces: list[str]
) -> str:
    text = requested.strip()
    if text:
        return text
    if "ACEScg" in available_color_spaces:
        return "ACEScg"
    return "auto"


def _resolve_live_targets(
    args: argparse.Namespace, probe_state_path: Path
) -> tuple[str, str, str, str, str]:
    state_data = _json_load(probe_state_path)
    ocio_state = state_data.get("ocio")
    if not isinstance(ocio_state, dict):
        raise RuntimeError(f"{probe_state_path.name}: missing ocio state block")
    if not bool(ocio_state.get("menu_data_ok")):
        raise RuntimeError(
            "OCIO menu data unavailable: "
            + str(ocio_state.get("menu_error", "")).strip()
        )

    displays = _string_list(ocio_state.get("available_displays"))
    available_color_spaces = _string_list(
        ocio_state.get("available_image_color_spaces")
    )
    display_views = _views_by_display(ocio_state)

    resolved_display = str(ocio_state.get("resolved_display", "")).strip()
    resolved_view = str(ocio_state.get("resolved_view", "")).strip()
    if not resolved_display:
        resolved_display = "default"
    if not resolved_view:
        resolved_view = "default"

    requested_display = args.display.strip()
    if requested_display and (
        requested_display == "default" or requested_display in displays
    ):
        initial_display = requested_display
    else:
        initial_display = resolved_display

    current_views = display_views.get(resolved_display, [])
    requested_view = args.raw_view.strip()
    if requested_view and (
        requested_view == "default" or requested_view in current_views
    ):
        initial_view = requested_view
    else:
        initial_view = resolved_view

    image_color_space = _pick_image_color_space(
        args.image_color_space, available_color_spaces
    )

    switch_mode = args.switch_mode
    if switch_mode == "auto":
        if args.target_display.strip():
            switch_mode = "display"
        else:
            target_views = display_views.get(initial_display, [])
            switch_mode = (
                "view"
                if _pick_first_other(target_views, initial_view)
                else "display"
            )

    if switch_mode == "view":
        target_display = args.target_display.strip() or initial_display
        target_views = display_views.get(target_display, [])
        target_view = args.target_view.strip()
        if not target_view:
            target_view = _pick_first_other(target_views, initial_view)
        if not target_view:
            raise RuntimeError(
                "no alternate OCIO view is available in the active config"
            )
    elif switch_mode == "display":
        target_display = args.target_display.strip()
        if not target_display:
            ranked_displays = sorted(
                displays, key=lambda value: _display_priority(value, initial_display)
            )
            target_display = _pick_first_other(ranked_displays, initial_display)
        if not target_display:
            raise RuntimeError(
                "no alternate OCIO display is available in the active config"
            )
        target_views = display_views.get(target_display, [])
        target_view = args.target_view.strip()
        if not target_view:
            target_view = _pick_preferred_target_view(target_views, initial_view)
            if not target_view:
                target_view = (
                    target_views[0]
                    if target_views
                    else ("default" if initial_view != "default" else initial_view)
                )
    else:
        raise RuntimeError(f"unsupported switch mode: {switch_mode}")

    return (
        initial_display,
        initial_view,
        target_display,
        target_view,
        image_color_space,
    )


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
    oiiotool: Path,
    source: Path,
    crop_rect: tuple[int, int, int, int],
    dest: Path,
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


def _run_gui_case(
    repo_root: Path,
    exe: Path,
    run_cwd: Path,
    backend: str,
    env: dict[str, str],
    image_path: Path,
    out_dir: Path,
    name: str,
    extra_args: list[str],
    case_timeout: float,
    trace: bool,
) -> tuple[Path, Path, Path, Path]:
    screenshot_path = out_dir / f"{name}.png"
    layout_path = out_dir / f"{name}.json"
    state_path = out_dir / f"{name}.state.json"
    log_path = out_dir / f"{name}.log"
    cmd = runner_command(exe, run_cwd, backend)
    cmd.extend(
        [
            "--open",
            str(image_path),
            "--screenshot-out",
            str(screenshot_path),
            "--layout-json-out",
            str(layout_path),
            "--layout-items",
            "--state-json-out",
            str(state_path),
            *extra_args,
        ]
    )
    if trace:
        cmd.append("--trace")

    proc = run_logged_process(
        cmd,
        cwd=repo_root,
        env=env,
        timeout=case_timeout,
        log_path=log_path,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"{name}: runner exited with code {proc.returncode}")
    for path, label in (
        (screenshot_path, "screenshot"),
        (layout_path, "layout json"),
        (state_path, "state json"),
    ):
        if not path.exists():
            raise RuntimeError(f"{name}: {label} not written")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            raise RuntimeError(f"{name}: found runtime error pattern: {pattern}")
    return screenshot_path, layout_path, state_path, log_path


def _scenario_step(
    root: ET.Element, name: str, **attrs: str | int | bool
) -> None:
    step = ET.SubElement(root, "step")
    step.set("name", name)
    for key, value in attrs.items():
        if isinstance(value, bool):
            step.set(key, "true" if value else "false")
        else:
            step.set(key, str(value))


def _write_live_scenario(
    path: Path,
    runtime_dir_rel: str,
    initial_display: str,
    initial_view: str,
    target_display: str,
    target_view: str,
    image_color_space: str,
    apply_delay_frames: int,
) -> None:
    root = ET.Element("imiv-scenario")
    root.set("out_dir", runtime_dir_rel)
    root.set("layout_items", "true")

    _scenario_step(
        root,
        "static_raw",
        delay_frames=10,
        ocio_use=True,
        ocio_display=initial_display,
        ocio_view=initial_view,
        ocio_image_color_space=image_color_space,
        screenshot=True,
        layout=True,
        state=True,
        post_action_delay_frames=apply_delay_frames,
    )
    _scenario_step(
        root,
        "live_noop_raw",
        ocio_use=True,
        ocio_display=initial_display,
        ocio_view=initial_view,
        ocio_image_color_space=image_color_space,
        screenshot=True,
        layout=True,
        state=True,
        post_action_delay_frames=apply_delay_frames,
    )
    _scenario_step(
        root,
        "live_switch",
        ocio_use=True,
        ocio_display=target_display,
        ocio_view=target_view,
        ocio_image_color_space=image_color_space,
        screenshot=True,
        layout=True,
        state=True,
        post_action_delay_frames=apply_delay_frames,
    )
    _scenario_step(
        root,
        "static_target",
        delay_frames=max(2, apply_delay_frames),
        ocio_use=True,
        ocio_display=target_display,
        ocio_view=target_view,
        ocio_image_color_space=image_color_space,
        screenshot=True,
        layout=True,
        state=True,
    )

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _run_scenario_case(
    repo_root: Path,
    exe: Path,
    run_cwd: Path,
    backend: str,
    env: dict[str, str],
    image_path: Path,
    out_dir: Path,
    scenario_path: Path,
    case_timeout: float,
    trace: bool,
) -> Path:
    scenario_log = out_dir / "scenario.log"
    cmd = runner_command(exe, run_cwd, backend)
    cmd.extend(
        [
            "--open",
            str(image_path),
            "--scenario",
            str(scenario_path),
        ]
    )
    if trace:
        cmd.append("--trace")

    proc = run_logged_process(
        cmd,
        cwd=repo_root,
        env=env,
        timeout=case_timeout,
        log_path=scenario_log,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"scenario: runner exited with code {proc.returncode}")

    log_text = scenario_log.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            raise RuntimeError(f"scenario: found runtime error pattern: {pattern}")
    return scenario_log


def main() -> int:
    repo_root = imiv_repo_root()
    runner = runner_path(repo_root)
    default_out = repo_root / "build_u" / "imiv_captures" / "ocio_live_update_regression"
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_image = default_out / "ocio_live_input.exr"
    default_config = _default_ocio_config(repo_root)

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument("--oiiotool", default=str(_default_oiiotool(repo_root)), help="oiiotool executable")
    ap.add_argument("--idiff", default=str(_default_idiff(repo_root)), help="idiff executable")
    ap.add_argument("--env-script", default=str(default_env_script), help="Optional shell env script")
    ap.add_argument("--out-dir", default=str(default_out), help="Artifact directory")
    ap.add_argument("--image", default=str(default_image), help="Generated input image path")
    ap.add_argument(
        "--ocio-config",
        default=str(default_config),
        help="OCIO config path or URI",
    )
    ap.add_argument(
        "--switch-mode",
        choices=("auto", "view", "display"),
        default="view",
        help="How to choose the live OCIO switch target",
    )
    ap.add_argument("--display", default="", help="Initial OCIO display")
    ap.add_argument("--target-display", default="", help="Target OCIO display")
    ap.add_argument("--raw-view", default="", help="Initial OCIO view")
    ap.add_argument("--target-view", default="", help="Live switch target OCIO view")
    ap.add_argument("--image-color-space", default="", help="Input image color space")
    ap.add_argument(
        "--apply-frame",
        type=int,
        default=5,
        help="Frames to wait after each live OCIO override before capture",
    )
    ap.add_argument(
        "--case-timeout",
        type=float,
        default=_default_case_timeout(),
        help="Per-launch GUI runner timeout in seconds",
    )
    ap.add_argument("--trace", action="store_true", help="Enable GUI runner trace")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        return fail(f"binary not found: {exe}")
    if not runner.exists():
        return fail(f"runner not found: {runner}")

    oiiotool = resolve_existing_tool(args.oiiotool, _default_oiiotool(repo_root))
    idiff = resolve_existing_tool(args.idiff, _default_idiff(repo_root))
    _ = idiff
    ocio_config = _resolve_ocio_config_argument(args.ocio_config)
    if not ocio_config.startswith("ocio://"):
        ocio_config_path = Path(ocio_config)
        if not ocio_config_path.exists():
            return fail(f"OCIO config not found: {ocio_config_path}")

    run_cwd = resolve_run_cwd(exe, args.cwd)
    out_dir = Path(args.out_dir).resolve()
    runtime_dir = out_dir / "runtime"
    image_path = Path(args.image).resolve()
    probe_config_home = out_dir / "config_home_probe"
    scenario_config_home = out_dir / "config_home_runtime"
    scenario_path = out_dir / "ocio_live.scenario.xml"

    shutil.rmtree(out_dir, ignore_errors=True)
    runtime_dir.mkdir(parents=True, exist_ok=True)
    probe_config_home.mkdir(parents=True, exist_ok=True)
    scenario_config_home.mkdir(parents=True, exist_ok=True)

    env = load_env_from_script(Path(args.env_script).resolve())
    env["OCIO"] = ocio_config

    try:
        _generate_probe_image(repo_root, oiiotool, image_path)

        probe_env = dict(env)
        probe_env["IMIV_CONFIG_HOME"] = str(probe_config_home)
        _, _, probe_state, probe_log = _run_gui_case(
            repo_root,
            exe,
            run_cwd,
            args.backend,
            probe_env,
            image_path,
            out_dir,
            "probe_menu",
            [],
            case_timeout=args.case_timeout,
            trace=args.trace,
        )

        (
            initial_display,
            initial_view,
            target_display,
            target_view,
            image_color_space,
        ) = _resolve_live_targets(args, probe_state)

        runtime_dir_rel = path_for_imiv_output(runtime_dir, run_cwd)
        _write_live_scenario(
            scenario_path,
            runtime_dir_rel,
            initial_display,
            initial_view,
            target_display,
            target_view,
            image_color_space,
            max(0, args.apply_frame),
        )

        scenario_env = dict(env)
        scenario_env["IMIV_CONFIG_HOME"] = str(scenario_config_home)
        scenario_log = _run_scenario_case(
            repo_root,
            exe,
            run_cwd,
            args.backend,
            scenario_env,
            image_path,
            out_dir,
            scenario_path,
            case_timeout=args.case_timeout,
            trace=args.trace,
        )

        static_raw_png = runtime_dir / "static_raw.png"
        static_raw_layout = runtime_dir / "static_raw.layout.json"
        static_raw_state = runtime_dir / "static_raw.state.json"
        static_target_png = runtime_dir / "static_target.png"
        static_target_layout = runtime_dir / "static_target.layout.json"
        static_target_state = runtime_dir / "static_target.state.json"
        live_noop_png = runtime_dir / "live_noop_raw.png"
        live_noop_layout = runtime_dir / "live_noop_raw.layout.json"
        live_noop_state = runtime_dir / "live_noop_raw.state.json"
        live_switch_png = runtime_dir / "live_switch.png"
        live_switch_layout = runtime_dir / "live_switch.layout.json"
        live_switch_state = runtime_dir / "live_switch.state.json"

        required_paths = [
            static_raw_png,
            static_raw_layout,
            static_raw_state,
            static_target_png,
            static_target_layout,
            static_target_state,
            live_noop_png,
            live_noop_layout,
            live_noop_state,
            live_switch_png,
            live_switch_layout,
            live_switch_state,
        ]
        for required in required_paths:
            if not required.exists():
                raise RuntimeError(f"scenario: expected output missing: {required}")

        crop_dir = runtime_dir / "crops"
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
        return fail(str(exc))

    static_noop_diff = _normalized_rgb_diff(
        oiiotool, static_raw_crop, live_noop_crop, crop_dir, "static_vs_noop"
    )
    if static_noop_diff > 2.0:
        return fail(
            "live noop OCIO update changed the image region unexpectedly "
            f"(mean abs RGB diff={static_noop_diff:.4f})"
        )

    static_switch_diff = _normalized_rgb_diff(
        oiiotool, static_raw_crop, live_switch_crop, crop_dir, "static_vs_switch"
    )
    if static_switch_diff <= 4.0:
        return fail(
            f"live OCIO {args.switch_mode} switch did not update the image region "
            f"(mean abs RGB diff={static_switch_diff:.4f})"
        )

    target_switch_diff = _normalized_rgb_diff(
        oiiotool, static_target_crop, live_switch_crop, crop_dir, "target_vs_switch"
    )
    if target_switch_diff > 2.0:
        return fail(
            f"live OCIO {args.switch_mode} switch does not match the settled target state "
            f"(mean abs RGB diff={target_switch_diff:.4f})"
        )

    print("probe log:", probe_log)
    print("scenario log:", scenario_log)
    print("display:", initial_display)
    print("initial display:", initial_display)
    print("target display:", target_display)
    print("initial view:", initial_view)
    print("target view:", target_view)
    print("image color space:", image_color_space)
    print("OCIO config:", ocio_config)
    print("static raw:", static_raw_png)
    print("static target:", static_target_png)
    print("live noop:", live_noop_png)
    print("live switch:", live_switch_png)
    print("crop static raw:", static_raw_crop)
    print("crop static target:", static_target_crop)
    print("crop live noop:", live_noop_crop)
    print("crop live switch:", live_switch_crop)
    print(
        "state dumps:",
        probe_state,
        static_raw_state,
        static_target_state,
        live_noop_state,
        live_switch_state,
    )
    print("logs:", probe_log, scenario_log)
    print("artifacts:", out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

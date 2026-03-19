#!/usr/bin/env python3
"""Regression check for builtin OCIO fallback when $OCIO is unavailable."""

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
    "error: imiv exited with code",
)


SOURCE_GLOBAL = 0
SOURCE_BUILTIN = 1


def _default_case_timeout() -> float:
    return 180.0 if os.name == "nt" else 60.0


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


def _json_load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


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


def _write_prefs(config_home: Path, *, ocio_config_source: int) -> Path:
    prefs_dir = config_home / "OpenImageIO" / "imiv"
    prefs_dir.mkdir(parents=True, exist_ok=True)
    prefs_path = prefs_dir / "imiv.inf"
    prefs_text = (
        "[ImivApp][State]\n"
        "use_ocio=1\n"
        f"ocio_config_source={ocio_config_source}\n"
        "ocio_display=default\n"
        "ocio_view=default\n"
        "ocio_image_color_space=auto\n"
        "ocio_user_config_path=\n"
    )
    prefs_path.write_text(prefs_text, encoding="utf-8")
    return prefs_path


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
 ) -> tuple[Path, Path, Path, Path]:
    screenshot_path = out_dir / f"{name}.png"
    layout_path = out_dir / f"{name}.json"
    state_path = out_dir / f"{name}.state.json"
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
        ]
    )
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
    if not screenshot_path.exists():
        raise RuntimeError(f"{name}: screenshot not written")
    if not layout_path.exists():
        raise RuntimeError(f"{name}: layout json not written")
    if not state_path.exists():
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
    expected_requested_source: str,
    expected_resolved_source: str,
    expected_fallback_applied: bool,
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
    if not bool(ocio.get("use_ocio")):
        raise RuntimeError(f"{name}: use_ocio was not enabled")
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
    if str(ocio.get("resolved_config_path", "")).strip() != "ocio://default":
        raise RuntimeError(
            f"{name}: unexpected resolved_config_path="
            f"{ocio.get('resolved_config_path', '')!r}"
        )
    if not bool(ocio.get("menu_data_ok")):
        raise RuntimeError(
            f"{name}: OCIO menu data unavailable: "
            f"{str(ocio.get('menu_error', '')).strip()}"
        )
    resolved_display = str(ocio.get("resolved_display", "")).strip()
    resolved_view = str(ocio.get("resolved_view", "")).strip()
    if not resolved_display:
        raise RuntimeError(f"{name}: resolved_display is empty")
    if not resolved_view:
        raise RuntimeError(f"{name}: resolved_view is empty")
    return state


if __name__ == "__main__":
    repo_root = Path(__file__).resolve().parents[3]
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"
    default_out = repo_root / "build_u" / "imiv_captures" / "ocio_missing_fallback_regression"
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_oiiotool = _default_oiiotool(repo_root)
    default_idiff = _default_idiff(repo_root)

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument("--env-script", default=str(default_env_script), help="Optional shell env setup script")
    ap.add_argument("--open", default=str(default_image), help="Image to open")
    ap.add_argument("--oiiotool", default=str(default_oiiotool), help="oiiotool executable")
    ap.add_argument("--idiff", default=str(default_idiff), help="idiff executable")
    ap.add_argument("--out-dir", default=str(default_out), help="Output directory")
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
        raise SystemExit(_fail(f"binary not found: {exe}"))

    cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()
    image_path = Path(args.open).expanduser().resolve()
    if not image_path.exists():
        raise SystemExit(_fail(f"image not found: {image_path}"))

    runner = default_runner.resolve()
    if not runner.exists():
        raise SystemExit(_fail(f"runner not found: {runner}"))
    oiiotool = Path(args.oiiotool).expanduser()
    if not oiiotool.exists() and shutil.which(str(oiiotool)) is None:
        raise SystemExit(_fail(f"oiiotool not found: {oiiotool}"))
    idiff = Path(args.idiff).expanduser()
    if not idiff.exists():
        found = shutil.which(str(idiff))
        if not found:
            raise SystemExit(_fail(f"idiff not found: {idiff}"))
        idiff = Path(found)
    idiff = idiff.resolve()

    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    base_env = _load_env_from_script(Path(args.env_script).expanduser())
    base_env.pop("OCIO", None)

    global_cfg = out_dir / "cfg_global"
    _write_prefs(global_cfg, ocio_config_source=SOURCE_GLOBAL)
    global_env = dict(base_env)
    global_env["IMIV_CONFIG_HOME"] = str(global_cfg)

    builtin_cfg = out_dir / "cfg_builtin"
    _write_prefs(builtin_cfg, ocio_config_source=SOURCE_BUILTIN)
    builtin_env = dict(base_env)
    builtin_env["IMIV_CONFIG_HOME"] = str(builtin_cfg)

    try:
        global_png, global_layout, global_state, global_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            args.backend,
            global_env,
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
    except (subprocess.SubprocessError, RuntimeError) as exc:
        raise SystemExit(_fail(str(exc)))

    try:
        global_state_data = _validate_ocio_state(
            "global_builtin_fallback",
            global_state,
            image_path=image_path,
            expected_requested_source="global",
            expected_resolved_source="builtin",
            expected_fallback_applied=True,
        )
        builtin_state_data = _validate_ocio_state(
            "builtin",
            builtin_state,
            image_path=image_path,
            expected_requested_source="builtin",
            expected_resolved_source="builtin",
            expected_fallback_applied=False,
        )
    except RuntimeError as exc:
        raise SystemExit(_fail(str(exc)))

    global_ocio = global_state_data["ocio"]
    builtin_ocio = builtin_state_data["ocio"]
    if (
        str(global_ocio.get("resolved_display", "")).strip()
        != str(builtin_ocio.get("resolved_display", "")).strip()
        or str(global_ocio.get("resolved_view", "")).strip()
        != str(builtin_ocio.get("resolved_view", "")).strip()
    ):
        raise SystemExit(
            _fail(
                "global_builtin_fallback: fallback did not resolve to the same "
                "display/view as the explicit builtin source"
            )
        )

    global_crop = out_dir / "global_builtin_fallback.crop.png"
    builtin_crop = out_dir / "builtin.crop.png"
    try:
        _crop_image(
            oiiotool, global_png, _image_crop_rect(global_layout), global_crop
        )
        _crop_image(
            oiiotool, builtin_png, _image_crop_rect(builtin_layout), builtin_crop
        )
    except (subprocess.SubprocessError, RuntimeError) as exc:
        raise SystemExit(_fail(str(exc)))

    diff = _normalized_rgb_diff(
        oiiotool, global_crop, builtin_crop, out_dir, "global_builtin_fallback"
    )
    if diff > 2.0:
        raise SystemExit(
            _fail(
                "global OCIO source did not match explicit builtin source when "
                f"$OCIO was unavailable (mean abs RGB diff={diff:.4f})"
            )
        )

    print("global_builtin_fallback:", global_png)
    print("builtin:", builtin_png)
    print("global_builtin_fallback_log:", global_log)
    print("builtin_log:", builtin_log)
    print("global_builtin_fallback_state:", global_state)
    print("builtin_state:", builtin_state)

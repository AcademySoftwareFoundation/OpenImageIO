#!/usr/bin/env python3
"""Regression check for OCIO config-source selection and builtin fallback."""

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
SOURCE_USER = 2


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


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


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
    repo_root: Path,
    runner: Path,
    exe: Path,
    cwd: Path,
    env: dict[str, str],
    image_path: Path,
    out_dir: Path,
    name: str,
    trace: bool,
) -> tuple[Path, Path, Path]:
    screenshot_path = out_dir / f"{name}.png"
    layout_path = out_dir / f"{name}.json"
    log_path = out_dir / f"{name}.log"
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
            timeout=60,
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


def _images_equal(idiff: Path, lhs: Path, rhs: Path) -> bool:
    proc = subprocess.run(
        [str(idiff), "-a", str(lhs), str(rhs)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if proc.returncode == 0:
        return True
    if proc.returncode in (1, 2):
        return False
    raise RuntimeError(f"idiff failed for '{lhs.name}' vs '{rhs.name}': {proc.stdout}")


if __name__ == "__main__":
    repo_root = Path(__file__).resolve().parents[3]
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_out = repo_root / "build_u" / "imiv_captures" / "ocio_config_source_regression"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument("--env-script", default=str(default_env_script), help="Optional shell env setup script")
    ap.add_argument("--out-dir", default=str(default_out), help="Output directory")
    ap.add_argument("--ocio-config", default=str(_default_ocio_config(repo_root)), help="OCIO config to use")
    ap.add_argument("--oiiotool", default=str(_default_oiiotool(repo_root)), help="oiiotool path")
    ap.add_argument("--idiff", default=str(_default_idiff(repo_root)), help="idiff path")
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        raise SystemExit(_fail(f"binary not found: {exe}"))
    cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()

    runner = default_runner.resolve()
    if not runner.exists():
        raise SystemExit(_fail(f"runner not found: {runner}"))

    ocio_config = Path(args.ocio_config).expanduser().resolve()
    if not ocio_config.exists():
        raise SystemExit(_fail(f"OCIO config not found: {ocio_config}"))

    oiiotool = Path(args.oiiotool).expanduser()
    if not oiiotool.exists():
        found = shutil.which(str(oiiotool))
        if not found:
            raise SystemExit(_fail(f"oiiotool not found: {oiiotool}"))
        oiiotool = Path(found)
    oiiotool = oiiotool.resolve()

    idiff = Path(args.idiff).expanduser()
    if not idiff.exists():
        found = shutil.which(str(idiff))
        if not found:
            raise SystemExit(_fail(f"idiff not found: {idiff}"))
        idiff = Path(found)
    idiff = idiff.resolve()

    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    image_path = out_dir / "ocio_source_fixture.exr"
    _generate_probe_image(oiiotool, image_path)

    external_display = "sRGB - Display"
    external_view = "Un-tone-mapped"
    external_color_space = "ACEScg"

    builtin_display = "default"
    builtin_view = "default"
    builtin_color_space = "auto"

    base_env = _load_env_from_script(Path(args.env_script).expanduser())
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
    global_env["OCIO"] = str(ocio_config)

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
    global_default_env["OCIO"] = str(ocio_config)

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
    global_invalid_env["OCIO"] = str(ocio_config)

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
        ocio_user_config_path=str(ocio_config),
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

    try:
        baseline_png, baseline_layout, baseline_log = _run_case(
            repo_root, runner, exe, cwd, baseline_env, image_path, out_dir, "baseline", args.trace
        )
        global_png, global_layout, global_log = _run_case(
            repo_root, runner, exe, cwd, global_env, image_path, out_dir, "global", args.trace
        )
        global_default_png, global_default_layout, global_default_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            global_default_env,
            image_path,
            out_dir,
            "global_default",
            args.trace,
        )
        global_invalid_png, global_invalid_layout, global_invalid_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            global_invalid_env,
            image_path,
            out_dir,
            "global_invalid_selection",
            args.trace,
        )
        global_builtin_png, global_builtin_layout, global_builtin_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            global_builtin_env,
            image_path,
            out_dir,
            "global_builtin_fallback",
            args.trace,
        )
        builtin_png, builtin_layout, builtin_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            builtin_env,
            image_path,
            out_dir,
            "builtin",
            args.trace,
        )
        user_png, user_layout, user_log = _run_case(
            repo_root, runner, exe, cwd, user_env, image_path, out_dir, "user", args.trace
        )
        user_missing_builtin_png, user_missing_builtin_layout, user_missing_builtin_log = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            user_missing_builtin_env,
            image_path,
            out_dir,
            "user_missing_builtin",
            args.trace,
        )
    except (subprocess.SubprocessError, RuntimeError) as exc:
        raise SystemExit(_fail(str(exc)))

    baseline_crop = out_dir / "baseline_crop.png"
    global_crop = out_dir / "global_crop.png"
    global_default_crop = out_dir / "global_default_crop.png"
    global_invalid_crop = out_dir / "global_invalid_selection_crop.png"
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
        global_invalid_png,
        _image_crop_rect(global_invalid_layout),
        global_invalid_crop,
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

    if _images_equal(idiff, baseline_crop, global_crop):
        raise SystemExit(
            _fail(
                "global OCIO source matched non-OCIO baseline; expected a real OCIO transform result"
            )
        )
    if not _images_equal(idiff, global_default_crop, global_invalid_crop):
        raise SystemExit(
            _fail(
                "invalid persisted display/view selection did not fall back to the active config defaults"
            )
        )
    if not _images_equal(idiff, global_crop, user_crop):
        raise SystemExit(_fail("user OCIO source output differs from global"))
    if not _images_equal(idiff, global_builtin_crop, builtin_crop):
        raise SystemExit(
            _fail("global source did not fall back to builtin when $OCIO was missing")
        )
    if not _images_equal(idiff, builtin_crop, user_missing_builtin_crop):
        raise SystemExit(
            _fail("user source did not fall back to builtin when user config was missing")
        )

    print("baseline:", baseline_png)
    print("global:", global_png)
    print("global_default:", global_default_png)
    print("global_invalid_selection:", global_invalid_png)
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

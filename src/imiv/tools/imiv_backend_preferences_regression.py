#!/usr/bin/env python3
"""Regression check for Preferences backend selection and restart semantics."""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


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


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def _write_prefs(config_home: Path) -> Path:
    prefs_dir = config_home / "OpenImageIO" / "imiv"
    prefs_dir.mkdir(parents=True, exist_ok=True)
    prefs_path = prefs_dir / "imiv.inf"
    prefs_path.write_text("[ImivApp][State]\nrenderer_backend=auto\n", encoding="utf-8")
    return prefs_path


def _scenario_step(root: ET.Element, name: str, **attrs: str | int | bool) -> None:
    step = ET.SubElement(root, "step")
    step.set("name", name)
    for key, value in attrs.items():
        if isinstance(value, bool):
            step.set(key, "true" if value else "false")
        else:
            step.set(key, str(value))


def _backend_display_name(name: str) -> str:
    lowered = name.strip().lower()
    if lowered == "vulkan":
        return "Vulkan"
    if lowered == "metal":
        return "Metal"
    if lowered == "opengl":
        return "OpenGL"
    if lowered == "auto":
        return "Auto"
    raise RuntimeError(f"unsupported backend name: {name}")


def _write_scenario(
    path: Path,
    runtime_dir_rel: str,
    *,
    combo_center: tuple[float, float],
    choice_points: dict[str, tuple[float, float]],
) -> None:
    root = ET.Element("imiv-scenario")
    root.set("out_dir", runtime_dir_rel)

    _scenario_step(
        root,
        "open_preferences",
        key_chord="ctrl+comma",
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "open_alternate_backend_combo",
        mouse_pos=f"{combo_center[0]:.3f},{combo_center[1]:.3f}",
        mouse_click_button=0,
        post_action_delay_frames=1,
    )
    _scenario_step(
        root,
        "select_alternate_backend",
        mouse_pos=f"{choice_points['alternate'][0]:.3f},{choice_points['alternate'][1]:.3f}",
        mouse_click_button=0,
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "open_active_backend_combo",
        mouse_pos=f"{combo_center[0]:.3f},{combo_center[1]:.3f}",
        mouse_click_button=0,
        post_action_delay_frames=1,
    )
    _scenario_step(
        root,
        "select_active_backend",
        mouse_pos=f"{choice_points['active'][0]:.3f},{choice_points['active'][1]:.3f}",
        mouse_click_button=0,
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "open_auto_backend_combo",
        mouse_pos=f"{combo_center[0]:.3f},{combo_center[1]:.3f}",
        mouse_click_button=0,
        post_action_delay_frames=1,
    )
    _scenario_step(
        root,
        "select_auto_backend",
        mouse_pos=f"{choice_points['auto'][0]:.3f},{choice_points['auto'][1]:.3f}",
        mouse_click_button=0,
        state=True,
        post_action_delay_frames=2,
    )

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _parse_list_backends(output: str) -> tuple[list[str], str]:
    built: list[str] = []
    build_default = ""
    line_re = re.compile(r"^\s+([A-Za-z0-9]+)\s+\(([^)]+)\)\s+:\s+(.*)$")
    for raw_line in output.splitlines():
        match = line_re.match(raw_line)
        if not match:
            continue
        _display_name, cli_name, description = match.groups()
        cli_name = cli_name.strip().lower()
        description = description.strip().lower()
        if "built" in description and "not built" not in description:
            built.append(cli_name)
        if "build default backend" in description:
            build_default = cli_name
    return built, build_default


def _load_state(path: Path) -> dict:
    if not path.exists():
        raise RuntimeError(f"state file not written: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def _find_combo_rect(layout: dict, label: str) -> dict:
    for window in layout.get("windows", []):
        if window.get("name") != "iv Preferences":
            continue
        for item in window.get("items", []):
            if item.get("debug") == label:
                rect = item.get("rect_full")
                if isinstance(rect, dict):
                    return rect
    raise RuntimeError(f"layout does not contain Preferences item: {label}")


def _rect_center(rect: dict) -> tuple[float, float]:
    rect_min = rect.get("min", [0.0, 0.0])
    rect_max = rect.get("max", [0.0, 0.0])
    return (
        (float(rect_min[0]) + float(rect_max[0])) * 0.5,
        (float(rect_min[1]) + float(rect_max[1])) * 0.5,
    )


def _backend_popup_choice_points(
    combo_rect: dict, *, alternate_backend: str, active_backend: str
) -> dict[str, tuple[float, float]]:
    order = {"auto": 0, "vulkan": 1, "metal": 2, "opengl": 3}
    rect_min = combo_rect.get("min", [0.0, 0.0])
    rect_max = combo_rect.get("max", [0.0, 0.0])
    min_x = float(rect_min[0])
    min_y = float(rect_min[1])
    max_x = float(rect_max[0])
    max_y = float(rect_max[1])
    row_h = max(1.0, max_y - min_y)
    choice_x = min_x + max(24.0, (max_x - min_x) * 0.35)

    def _point(name: str) -> tuple[float, float]:
        return (choice_x, max_y + row_h * (order[name] + 0.5))

    return {
        "alternate": _point(alternate_backend),
        "active": _point(active_backend),
        "auto": _point("auto"),
    }


def _assert_backend_state(
    state: dict,
    *,
    label: str,
    active_backend: str,
    requested_backend: str,
    next_launch_backend: str,
    restart_required: bool,
    compiled_backends: list[str],
) -> None:
    backend = state.get("backend")
    if not isinstance(backend, dict):
        raise RuntimeError(f"{label}: backend state block missing")

    if backend.get("active") != active_backend:
        raise RuntimeError(
            f"{label}: active backend {backend.get('active')!r} != {active_backend!r}"
        )
    if backend.get("requested") != requested_backend:
        raise RuntimeError(
            f"{label}: requested backend {backend.get('requested')!r} != {requested_backend!r}"
        )
    if backend.get("next_launch") != next_launch_backend:
        raise RuntimeError(
            f"{label}: next_launch backend {backend.get('next_launch')!r} != {next_launch_backend!r}"
        )
    if bool(backend.get("restart_required")) != restart_required:
        raise RuntimeError(
            f"{label}: restart_required {backend.get('restart_required')!r} != {restart_required!r}"
        )

    actual_compiled = backend.get("compiled")
    if actual_compiled != compiled_backends:
        raise RuntimeError(
            f"{label}: compiled backends {actual_compiled!r} != {compiled_backends!r}"
        )


def main() -> int:
    repo_root = _repo_root()
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_out_dir = repo_root / "build_u" / "imiv_captures" / "backend_preferences_regression"
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed to imiv",
    )
    ap.add_argument("--env-script", default=str(default_env_script), help="Optional shell env setup script")
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument("--open", default=str(default_image), help="Image to open")
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return _fail(f"binary not found: {exe}")

    run_cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()
    image_path = Path(args.open).expanduser().resolve()
    if not image_path.exists():
        return _fail(f"image not found: {image_path}")

    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    probe_dir = out_dir / "probe"
    probe_dir.mkdir(parents=True, exist_ok=True)
    runtime_dir = out_dir / "runtime"
    runtime_dir.mkdir(parents=True, exist_ok=True)
    probe_log_path = out_dir / "backend_preferences_probe.log"
    log_path = out_dir / "backend_preferences.log"
    scenario_path = out_dir / "backend_preferences.scenario.xml"

    env = _load_env_from_script(Path(args.env_script).expanduser())

    list_proc = subprocess.run(
        [str(exe), "--list-backends"],
        cwd=str(run_cwd),
        env=env,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if list_proc.returncode != 0:
        return _fail(f"--list-backends exited with code {list_proc.returncode}")
    compiled_backends, build_default_backend = _parse_list_backends(list_proc.stdout)
    if len(compiled_backends) < 2:
        print("skip: backend preferences regression requires at least two compiled backends")
        return 77
    if not build_default_backend:
        return _fail("could not determine build default backend from --list-backends")

    active_backend = args.backend.strip().lower() if args.backend else build_default_backend
    if active_backend not in compiled_backends:
        return _fail(f"requested active backend is not compiled: {active_backend}")

    alternate_backend = next(
        (name for name in compiled_backends if name != active_backend),
        "",
    )
    if not alternate_backend:
        print("skip: backend preferences regression requires an alternate compiled backend")
        return 77

    config_home = out_dir / "config_home"
    _write_prefs(config_home)
    env["IMIV_CONFIG_HOME"] = str(config_home)

    probe_layout_path = probe_dir / "preferences.layout.json"
    probe_state_path = probe_dir / "preferences.state.json"
    probe_cmd = [
        sys.executable,
        str(repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"),
        "--bin",
        str(exe),
        "--cwd",
        str(run_cwd),
        "--open",
        str(image_path),
        "--key-chord",
        "ctrl+comma",
        "--layout-json-out",
        str(probe_layout_path),
        "--layout-items",
        "--state-json-out",
        str(probe_state_path),
    ]
    if args.backend:
        probe_cmd.extend(["--backend", active_backend])
    if args.trace:
        probe_cmd.append("--trace")
    with probe_log_path.open("w", encoding="utf-8") as log_handle:
        probe_proc = subprocess.run(
            probe_cmd,
            cwd=str(repo_root),
            env=env,
            check=False,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
    if probe_proc.returncode != 0:
        return _fail(f"probe runner exited with code {probe_proc.returncode}")

    probe_layout = json.loads(probe_layout_path.read_text(encoding="utf-8"))
    combo_rect = _find_combo_rect(probe_layout, "Renderer backend")
    combo_center = _rect_center(combo_rect)
    choice_points = _backend_popup_choice_points(
        combo_rect,
        alternate_backend=alternate_backend,
        active_backend=active_backend,
    )

    runtime_dir_rel = os.path.relpath(runtime_dir, run_cwd)
    _write_scenario(
        scenario_path,
        runtime_dir_rel,
        combo_center=combo_center,
        choice_points=choice_points,
    )

    cmd = [
        sys.executable,
        str(repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"),
        "--bin",
        str(exe),
        "--cwd",
        str(run_cwd),
        "--scenario",
        str(scenario_path),
        "--open",
        str(image_path),
    ]
    if args.backend:
        cmd.extend(["--backend", active_backend])
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
            timeout=120,
        )
    if proc.returncode != 0:
        return _fail(f"scenario runner exited with code {proc.returncode}")

    open_state = _load_state(runtime_dir / "open_preferences.state.json")
    alt_state = _load_state(runtime_dir / "select_alternate_backend.state.json")
    active_state = _load_state(runtime_dir / "select_active_backend.state.json")
    auto_state = _load_state(runtime_dir / "select_auto_backend.state.json")

    _assert_backend_state(
        open_state,
        label="open_preferences",
        active_backend=active_backend,
        requested_backend="auto",
        next_launch_backend=build_default_backend,
        restart_required=(build_default_backend != active_backend),
        compiled_backends=compiled_backends,
    )
    _assert_backend_state(
        alt_state,
        label="select_alternate_backend",
        active_backend=active_backend,
        requested_backend=alternate_backend,
        next_launch_backend=alternate_backend,
        restart_required=(alternate_backend != active_backend),
        compiled_backends=compiled_backends,
    )
    _assert_backend_state(
        active_state,
        label="select_active_backend",
        active_backend=active_backend,
        requested_backend=active_backend,
        next_launch_backend=active_backend,
        restart_required=False,
        compiled_backends=compiled_backends,
    )
    _assert_backend_state(
        auto_state,
        label="select_auto_backend",
        active_backend=active_backend,
        requested_backend="auto",
        next_launch_backend=build_default_backend,
        restart_required=(build_default_backend != active_backend),
        compiled_backends=compiled_backends,
    )

    print("runtime:", runtime_dir)
    print("log:", log_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

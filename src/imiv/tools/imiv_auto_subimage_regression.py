#!/usr/bin/env python3
"""Regression check for iv-style hidden auto-subimage-from-zoom behavior."""

from __future__ import annotations

import argparse
import json
import math
import os
import shlex
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


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
        repo_root / "build_u" / "src" / "oiiotool" / "oiiotool",
        repo_root / "build" / "src" / "oiiotool" / "oiiotool",
        repo_root / "build" / "Debug" / "oiiotool.exe",
        repo_root / "build" / "Release" / "oiiotool.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    which = shutil.which("oiiotool")
    return Path(which) if which else candidates[0]


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


def _run_checked(cmd: list[str], *, cwd: Path) -> None:
    print("run:", " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd), check=True)


def _generate_subimage_fixture(oiiotool: Path, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    tmp_specs = [
        ("sub0_4096.tif", "1,0,0", 4096),
        ("sub1_2048.tif", "0,1,0", 2048),
        ("sub2_1024.tif", "0,0,1", 1024),
        ("sub3_0512.tif", "1,1,0", 512),
    ]
    tmp_paths: list[Path] = []
    for name, color, dim in tmp_specs:
        path = out_path.parent / name
        tmp_paths.append(path)
        _run_checked(
            [
                str(oiiotool),
                "--pattern",
                f"constant:color={color}",
                f"{dim}x{dim}",
                "3",
                "-d",
                "uint8",
                "-o",
                str(path),
            ],
            cwd=out_path.parent,
        )

    cmd = [str(oiiotool)]
    cmd.extend(str(path) for path in tmp_paths)
    cmd.extend(["--siappendall", "-o", str(out_path)])
    _run_checked(cmd, cwd=out_path.parent)


def _run_case(
    repo_root: Path,
    runner: Path,
    exe: Path,
    cwd: Path,
    backend: str,
    image_path: Path,
    out_dir: Path,
    name: str,
    extra_args: list[str],
    env: dict[str, str],
    trace: bool,
) -> dict:
    state_path = out_dir / f"{name}.json"
    log_path = out_dir / f"{name}.log"
    config_home = out_dir / f"cfg_{name}"
    shutil.rmtree(config_home, ignore_errors=True)
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
            "--state-json-out",
            str(state_path),
        ]
    )
    if trace:
        cmd.append("--trace")
    cmd.extend(extra_args)

    case_env = dict(env)
    case_env["IMIV_CONFIG_HOME"] = str(config_home)

    with log_path.open("w", encoding="utf-8") as log_handle:
        proc = subprocess.run(
            cmd,
            cwd=str(repo_root),
            env=case_env,
            check=False,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            timeout=90,
        )
    if proc.returncode != 0:
        raise RuntimeError(f"{name}: runner exited with code {proc.returncode}")
    if not state_path.exists():
        raise RuntimeError(f"{name}: state file not written")

    with state_path.open("r", encoding="utf-8") as handle:
        state = json.load(handle)
    state["_state_path"] = str(state_path)
    state["_log_path"] = str(log_path)
    return state


def _path_for_imiv_output(path: Path, run_cwd: Path) -> str:
    try:
        return os.path.relpath(path, run_cwd)
    except ValueError:
        return str(path)


def _write_scenario(path: Path, runtime_dir_rel: str) -> None:
    root = ET.Element("imiv-scenario")
    root.set("out_dir", runtime_dir_rel)

    step = ET.SubElement(root, "step")
    step.set("name", "enable_auto")
    step.set("key_chord", "shift+comma")
    step.set("state", "true")
    step.set("post_action_delay_frames", "2")

    step = ET.SubElement(root, "step")
    step.set("name", "zoom_out")
    step.set("key_chord", "ctrl+minus")
    step.set("state", "true")
    step.set("post_action_delay_frames", "3")

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _run_scenario(
    repo_root: Path,
    runner: Path,
    exe: Path,
    cwd: Path,
    backend: str,
    image_path: Path,
    out_dir: Path,
    scenario_path: Path,
    env: dict[str, str],
    trace: bool,
) -> dict[str, dict]:
    runtime_dir = out_dir / "runtime"
    runtime_dir.mkdir(parents=True, exist_ok=True)
    config_home = out_dir / "cfg_scenario"
    shutil.rmtree(config_home, ignore_errors=True)
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
            "--scenario",
            str(scenario_path),
        ]
    )
    if trace:
        cmd.append("--trace")

    case_env = dict(env)
    case_env["IMIV_CONFIG_HOME"] = str(config_home)

    with (out_dir / "scenario.log").open("w", encoding="utf-8") as log_handle:
        proc = subprocess.run(
            cmd,
            cwd=str(repo_root),
            env=case_env,
            check=False,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            timeout=90,
        )
    if proc.returncode != 0:
        raise RuntimeError(f"scenario: runner exited with code {proc.returncode}")

    result: dict[str, dict] = {}
    for step_name in ("enable_auto", "zoom_out"):
        state_path = runtime_dir / f"{step_name}.state.json"
        if not state_path.exists():
            raise RuntimeError(f"scenario: missing state output for {step_name}")
        with state_path.open("r", encoding="utf-8") as handle:
            state = json.load(handle)
        state["_state_path"] = str(state_path)
        state["_log_path"] = str(out_dir / "scenario.log")
        result[step_name] = state
    return result


def _calc_expected_subimage_from_zoom(
    current_subimage: int, nsubimages: int, zoom: float
) -> tuple[int, float]:
    rel_subimage = math.trunc(math.log2(1.0 / max(1.0e-6, zoom)))
    target = max(0, min(current_subimage + rel_subimage, nsubimages - 1))
    adjusted_zoom = zoom
    if not (current_subimage == 0 and zoom > 1.0) and not (
        current_subimage == nsubimages - 1 and zoom < 1.0
    ):
        adjusted_zoom *= math.pow(2.0, float(rel_subimage))
    return target, adjusted_zoom


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    default_runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_out = repo_root / "build_u" / "imiv_captures" / "auto_subimage_regression"
    default_image = default_out / "auto_subimages.tif"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument(
        "--oiiotool", default=str(_default_oiiotool(repo_root)), help="oiiotool executable"
    )
    ap.add_argument(
        "--env-script",
        default=str(default_env_script),
        help="Optional shell env setup script",
    )
    ap.add_argument("--out-dir", default=str(default_out), help="Output directory")
    ap.add_argument(
        "--image",
        default=str(default_image),
        help="Generated multi-subimage TIFF output path",
    )
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return _fail(f"binary not found: {exe}")
    oiiotool = Path(args.oiiotool).expanduser().resolve()
    if not oiiotool.exists():
        return _fail(f"oiiotool not found: {oiiotool}")
    runner = default_runner.resolve()
    if not runner.exists():
        return _fail(f"runner not found: {runner}")

    cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()
    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    image_path = Path(args.image).expanduser().resolve()

    try:
        _generate_subimage_fixture(oiiotool, image_path)
    except subprocess.SubprocessError as exc:
        return _fail(f"failed to generate subimage fixture: {exc}")

    env = _load_env_from_script(Path(args.env_script).expanduser())

    try:
        baseline = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            args.backend,
            image_path,
            out_dir,
            "baseline",
            [],
            env,
            args.trace,
        )
        scenario_path = out_dir / "auto_subimage.scenario.xml"
        runtime_dir = out_dir / "runtime"
        _write_scenario(scenario_path, _path_for_imiv_output(runtime_dir, cwd))
        scenario_states = _run_scenario(
            repo_root,
            runner,
            exe,
            cwd,
            args.backend,
            image_path,
            out_dir,
            scenario_path,
            env,
            args.trace,
        )
        enabled = scenario_states["enable_auto"]
        auto_zoom = scenario_states["zoom_out"]
    except (RuntimeError, subprocess.SubprocessError) as exc:
        return _fail(str(exc))

    for name, state in (
        ("baseline", baseline),
        ("enable_auto", enabled),
        ("auto_zoom_out", auto_zoom),
    ):
        if not state.get("image_loaded", False):
            return _fail(f"{name}: image not loaded")

    baseline_zoom = float(baseline["zoom"])
    if not (baseline_zoom > 0.0 and baseline_zoom < 1.0):
        return _fail(f"baseline zoom expected fit-in-window range, got {baseline_zoom:.6f}")
    if bool(baseline["auto_subimage"]):
        return _fail("baseline unexpectedly started with auto_subimage enabled")
    if int(baseline["subimage"]) != 0:
        return _fail(f"baseline subimage expected 0, got {baseline['subimage']}")

    if not bool(enabled["auto_subimage"]):
        return _fail("Shift+, did not enable auto_subimage")
    if int(enabled["subimage"]) != 0:
        return _fail(f"enable_auto changed subimage unexpectedly: {enabled['subimage']}")

    expected_subimage, expected_zoom = _calc_expected_subimage_from_zoom(
        current_subimage=0, nsubimages=4, zoom=float(enabled["zoom"]) * 0.5
    )
    actual_subimage = int(auto_zoom["subimage"])
    actual_zoom = float(auto_zoom["zoom"])
    if not bool(auto_zoom["auto_subimage"]):
        return _fail("auto_zoom_out did not keep auto_subimage enabled")
    if actual_subimage != expected_subimage:
        return _fail(
            f"auto_zoom_out landed on wrong subimage: expected {expected_subimage}, got {actual_subimage}"
        )
    if expected_subimage <= 0:
        return _fail(
            f"test fixture did not force a real auto-subimage switch: expected_subimage={expected_subimage}"
        )
    if abs(actual_zoom - expected_zoom) > 0.05:
        return _fail(
            f"auto_zoom_out restored wrong zoom: expected {expected_zoom:.6f}, got {actual_zoom:.6f}"
        )

    print("baseline:", baseline["_state_path"])
    print("enable_auto:", enabled["_state_path"])
    print("auto_zoom_out:", auto_zoom["_state_path"])
    print("artifacts:", out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

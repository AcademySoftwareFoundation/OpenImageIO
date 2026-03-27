#!/usr/bin/env python3
"""Regression check for GUI-driven Export As recipe export."""

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


def _default_idiff(repo_root: Path) -> Path:
    candidates = [
        repo_root / "build" / "bin" / "idiff",
        repo_root / "build_u" / "bin" / "idiff",
        repo_root / "build" / "src" / "idiff" / "idiff",
        repo_root / "build_u" / "src" / "idiff" / "idiff",
        repo_root / "build" / "Debug" / "idiff.exe",
        repo_root / "build" / "Release" / "idiff.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return Path("idiff")


def _default_env_script(repo_root: Path, exe: Path | None = None) -> Path:
    candidates: list[Path] = []
    if exe is not None:
        exe = exe.resolve()
        candidates.extend([exe.parent / "imiv_env.sh", exe.parent.parent / "imiv_env.sh"])
    candidates.extend([repo_root / "build" / "imiv_env.sh", repo_root / "build_u" / "imiv_env.sh"])
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


def _run(
    cmd: list[str], *, cwd: Path, env: dict[str, str] | None = None
) -> subprocess.CompletedProcess[str]:
    print("run:", " ".join(cmd))
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def _path_for_imiv_output(path: Path, run_cwd: Path) -> str:
    try:
        return os.path.relpath(path, run_cwd)
    except ValueError:
        return str(path)


def _scenario_step(root: ET.Element, name: str, **attrs: str | int | float | bool) -> None:
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

    _scenario_step(
        root,
        "set_blue_channel",
        key_chord="b",
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "set_single_channel",
        key_chord="1",
        state=True,
        post_action_delay_frames=2,
    )
    _scenario_step(
        root,
        "adjust_recipe",
        exposure=0.75,
        gamma=1.6,
        offset=0.125,
        state=True,
        post_action_delay_frames=3,
    )
    _scenario_step(
        root,
        "save_window",
        key_chord="ctrl+shift+s",
        state=True,
        post_action_delay_frames=6,
    )

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _assert_close(actual: float, expected: float, name: str) -> None:
    if not math.isclose(actual, expected, rel_tol=1.0e-5, abs_tol=1.0e-5):
        raise AssertionError(f"{name} mismatch: expected {expected}, got {actual}")


def main() -> int:
    repo_root = _repo_root()
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"
    default_out_dir = repo_root / "build" / "imiv_captures" / "save_window_regression"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument("--backend", default="", help="Optional runtime backend override")
    ap.add_argument("--oiiotool", default=str(_default_oiiotool(repo_root)), help="oiiotool executable")
    ap.add_argument("--idiff", default=str(_default_idiff(repo_root)), help="idiff executable")
    ap.add_argument("--env-script", default=str(_default_env_script(repo_root)), help="Optional shell env setup script")
    ap.add_argument("--image", default=str(default_image), help="Input image path")
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve(strict=False)
    if not runner.exists():
        return _fail(f"runner not found: {runner}")
    image = Path(args.image).expanduser().resolve()
    if not image.exists():
        return _fail(f"image not found: {image}")

    oiiotool = Path(args.oiiotool).expanduser()
    if not oiiotool.exists():
        found = shutil.which(str(oiiotool))
        if not found:
            return _fail(f"oiiotool not found: {oiiotool}")
        oiiotool = Path(found)
    oiiotool = oiiotool.resolve()

    idiff = Path(args.idiff).expanduser()
    if not idiff.exists():
        found = shutil.which(str(idiff))
        if not found:
            return _fail(f"idiff not found: {idiff}")
        idiff = Path(found)
    idiff = idiff.resolve()

    cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()
    out_dir = Path(args.out_dir).expanduser().resolve()
    runtime_dir = out_dir / "runtime"
    scenario_path = out_dir / "save_window.scenario.xml"
    adjust_state_path = runtime_dir / "adjust_recipe.state.json"
    save_state_path = runtime_dir / "save_window.state.json"
    log_path = out_dir / "save_window.log"
    fixture_path = out_dir / "save_window_input.png"
    saved_path = out_dir / "saved_window.tif"
    expected_path = out_dir / "expected_window.tif"

    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    prep = _run(
        [
            str(oiiotool),
            str(image),
            "--resize",
            "2200x1547",
            "-o",
            str(fixture_path),
        ],
        cwd=repo_root,
    )
    if prep.returncode != 0:
        print(prep.stdout, end="")
        return _fail("failed to prepare save-window fixture")

    env = dict(os.environ)
    env.update(_load_env_from_script(Path(args.env_script).expanduser()))
    config_home = out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)
    env["IMIV_TEST_SAVE_IMAGE_PATH"] = str(saved_path)

    _write_scenario(scenario_path, _path_for_imiv_output(runtime_dir, cwd))
    cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(cwd),
        "--open",
        str(fixture_path),
        "--scenario",
        str(scenario_path),
    ]
    if args.backend:
        cmd.extend(["--backend", args.backend])
    if args.trace:
        cmd.append("--trace")

    proc = _run(cmd, cwd=repo_root, env=env)
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        print(proc.stdout, end="")
        return _fail(f"runner exited with code {proc.returncode}")

    if not adjust_state_path.exists():
        return _fail(f"adjust state output not found: {adjust_state_path}")
    if not save_state_path.exists():
        return _fail(f"save state output not found: {save_state_path}")
    if not saved_path.exists():
        return _fail(f"saved window output not found: {saved_path}")

    adjust_state = json.loads(adjust_state_path.read_text(encoding="utf-8"))
    recipe = adjust_state.get("view_recipe", {})
    try:
        _assert_close(float(recipe.get("exposure", 0.0)), 0.75, "window exposure")
        _assert_close(float(recipe.get("gamma", 0.0)), 1.6, "window gamma")
        _assert_close(float(recipe.get("offset", 0.0)), 0.125, "window offset")
    except (TypeError, ValueError, AssertionError) as exc:
        return _fail(str(exc))
    if int(recipe.get("current_channel", -1)) != 3:
        return _fail("window export did not keep blue channel selection")
    if int(recipe.get("color_mode", -1)) != 2:
        return _fail("window export did not keep single-channel mode")

    exposure_scale = 2.0 ** 0.75
    inv_gamma = 1.0 / 1.6
    expect = _run(
        [
            str(oiiotool),
            str(fixture_path),
            "--ch",
            "B,B,B,A=1.0",
            "--addc",
            "0.125,0.125,0.125,0.0",
            "--mulc",
            f"{exposure_scale},{exposure_scale},{exposure_scale},1.0",
            "--powc",
            f"{inv_gamma},{inv_gamma},{inv_gamma},1.0",
            "-d",
            "float",
            "-o",
            str(expected_path),
        ],
        cwd=repo_root,
    )
    if expect.returncode != 0:
        print(expect.stdout, end="")
        return _fail("failed to prepare expected save-window output")

    diff = _run(
        [
            str(idiff),
            "-q",
            "-a",
            str(expected_path),
            str(saved_path),
        ],
        cwd=repo_root,
    )
    if diff.returncode != 0:
        print(diff.stdout, end="")
        return _fail("saved window output does not match expected recipe")

    print(f"fixture: {fixture_path}")
    print(f"adjust_state: {adjust_state_path}")
    print(f"save_state: {save_state_path}")
    print(f"saved: {saved_path}")
    print(f"expected: {expected_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

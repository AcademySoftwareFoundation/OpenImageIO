#!/usr/bin/env python3
"""Regression check for preserving centered scroll when opening Image List."""

from __future__ import annotations

import argparse
import json
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


def _write_scenario(path: Path, runtime_dir_rel: str) -> None:
    root = ET.Element("imiv-scenario")
    root.set("out_dir", runtime_dir_rel)

    baseline = ET.SubElement(root, "step")
    baseline.set("name", "baseline")
    baseline.set("state", "true")
    baseline.set("post_action_delay_frames", "4")

    show_list = ET.SubElement(root, "step")
    show_list.set("name", "show_image_list")
    show_list.set("image_list_visible", "true")
    show_list.set("state", "true")
    show_list.set("post_action_delay_frames", "4")

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _build_wide_fixture(oiiotool: Path, out_path: Path) -> None:
    cmd = [
        str(oiiotool),
        "--pattern",
        "constant:color=0.25,0.5,0.75",
        "10000x2000",
        "3",
        "-d",
        "half",
        "-o",
        str(out_path),
    ]
    subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def _load_json(path: Path) -> dict:
    if not path.exists():
        raise FileNotFoundError(path)
    return json.loads(path.read_text(encoding="utf-8"))


def _norm_scroll_centered(state: dict, tol: float = 0.08) -> bool:
    norm_scroll = state.get("norm_scroll")
    if not (
        isinstance(norm_scroll, list)
        and len(norm_scroll) == 2
        and all(isinstance(v, (int, float)) for v in norm_scroll)
    ):
        return False
    return abs(float(norm_scroll[0]) - 0.5) <= tol and abs(float(norm_scroll[1]) - 0.5) <= tol


def main() -> int:
    repo_root = _repo_root()
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_env_script = repo_root / "build" / "imiv_env.sh"
    default_out_dir = repo_root / "build" / "imiv_captures" / "image_list_center_regression"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument(
        "--backend",
        default="opengl",
        help="Optional runtime backend override passed through to imiv",
    )
    ap.add_argument(
        "--env-script",
        default=str(default_env_script),
        help="Optional shell env setup script",
    )
    ap.add_argument("--out-dir", default=str(default_out_dir), help="Output directory")
    ap.add_argument(
        "--oiiotool", default=str(_default_oiiotool(repo_root)), help="oiiotool executable"
    )
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    if not exe.exists():
        return _fail(f"binary not found: {exe}")
    runner = runner.resolve()
    if not runner.exists():
        return _fail(f"runner not found: {runner}")
    oiiotool = Path(args.oiiotool).expanduser()
    if not oiiotool.exists():
        found = shutil.which(str(oiiotool))
        if found is None:
            return _fail(f"oiiotool not found: {oiiotool}")
        oiiotool = Path(found)
    oiiotool = oiiotool.resolve()

    cwd = Path(args.cwd).expanduser().resolve() if args.cwd else exe.parent.resolve()
    out_dir = Path(args.out_dir).expanduser().resolve()
    runtime_dir = out_dir / "runtime"
    scenario_path = out_dir / "image_list_center.scenario.xml"
    fixture_path = out_dir / "wide.exr"
    baseline_state_path = runtime_dir / "baseline.state.json"
    show_state_path = runtime_dir / "show_image_list.state.json"
    log_path = out_dir / "image_list_center.log"

    shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    _build_wide_fixture(oiiotool, fixture_path)
    _write_scenario(scenario_path, os.path.relpath(runtime_dir, cwd))

    env = dict(os.environ)
    env.update(_load_env_from_script(Path(args.env_script).expanduser()))
    config_home = out_dir / "cfg"
    config_home.mkdir(parents=True, exist_ok=True)
    env["IMIV_CONFIG_HOME"] = str(config_home)

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

    print("run:", " ".join(cmd))
    proc = subprocess.run(
        cmd,
        cwd=str(repo_root),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        print(proc.stdout, end="")
        return _fail(f"runner exited with code {proc.returncode}")

    try:
        baseline_state = _load_json(baseline_state_path)
        show_state = _load_json(show_state_path)
    except FileNotFoundError as exc:
        return _fail(f"state output not found: {exc}")

    if not bool(baseline_state.get("image_loaded", False)):
        return _fail("baseline state does not report a loaded image")
    if bool(baseline_state.get("image_list_visible", True)):
        return _fail("baseline state unexpectedly reports Image List as visible")
    if not _norm_scroll_centered(baseline_state):
        return _fail("baseline image was not centered")

    if not bool(show_state.get("image_loaded", False)):
        return _fail("show-image-list state does not report a loaded image")
    if not bool(show_state.get("image_list_visible", False)):
        return _fail("show-image-list state does not report Image List as visible")
    if not bool(show_state.get("image_list_drawn", False)):
        return _fail("show-image-list state does not report Image List as drawn")
    if not _norm_scroll_centered(show_state):
        return _fail("opening Image List changed the centered scroll position")

    scroll = show_state.get("scroll")
    if not (
        isinstance(scroll, list)
        and len(scroll) == 2
        and isinstance(scroll[0], (int, float))
        and float(scroll[0]) > 1.0
    ):
        return _fail("wide-image regression did not produce horizontal scrolling")

    print(f"baseline_state: {baseline_state_path}")
    print(f"show_state: {show_state_path}")
    print(f"log: {log_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""OpenGL-focused selection regression with real UI toggles."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


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


def _run_checked(cmd: list[str], *, cwd: Path) -> None:
    print("run:", " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd), check=True)


def _generate_fixture(oiiotool: Path, out_path: Path, width: int, height: int) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    _run_checked(
        [
            str(oiiotool),
            "--pattern",
            "fill:top=0.12,0.18,0.24,bottom=0.78,0.84,0.96",
            f"{width}x{height}",
            "3",
            "-d",
            "uint8",
            "-o",
            str(out_path),
        ],
        cwd=out_path.parent,
    )


def _run_case(
    repo_root: Path,
    runner: Path,
    exe: Path,
    cwd: Path,
    image_path: Path,
    out_dir: Path,
    name: str,
    env: dict[str, str],
    extra_args: list[str],
    *,
    want_layout: bool = False,
    trace: bool = False,
) -> tuple[dict, dict | None]:
    state_path = out_dir / f"{name}.state.json"
    layout_path = out_dir / f"{name}.layout.json"
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
        "--open",
        str(image_path),
        "--state-json-out",
        str(state_path),
        "--post-action-delay-frames",
        "2",
    ]
    if want_layout:
        cmd.extend(["--layout-json-out", str(layout_path), "--layout-items"])
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
            timeout=120,
        )
    if proc.returncode != 0:
        raise RuntimeError(f"{name}: runner exited with code {proc.returncode}")
    if not state_path.exists():
        raise RuntimeError(f"{name}: state file not written")

    state = json.loads(state_path.read_text(encoding="utf-8"))
    state["_state_path"] = str(state_path)
    state["_log_path"] = str(log_path)
    layout = None
    if want_layout:
        if not layout_path.exists():
            raise RuntimeError(f"{name}: layout file not written")
        layout = json.loads(layout_path.read_text(encoding="utf-8"))
    return state, layout


def _selection_bounds_valid(state: dict) -> bool:
    bounds = state.get("selection_bounds", [])
    if len(bounds) != 4:
        return False
    return bounds[2] > bounds[0] and bounds[3] > bounds[1]


def _selection_is_cleared(state: dict) -> bool:
    bounds = state.get("selection_bounds", [])
    if len(bounds) != 4:
        return False
    return (
        not state.get("selection_active", False)
        and bounds[0] == 0
        and bounds[1] == 0
        and bounds[2] == 0
        and bounds[3] == 0
    )


def _area_probe_is_initialized(state: dict) -> bool:
    lines = state.get("area_probe_lines", [])
    return bool(lines) and all("-----" not in line for line in lines[1:])


def main() -> int:
    repo_root = _repo_root()
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_out_dir = repo_root / "build_u" / "imiv_captures" / "opengl_selection_regression"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", required=True, help="imiv executable")
    ap.add_argument("--cwd", required=True, help="Working directory for imiv")
    ap.add_argument("--oiiotool", required=True, help="oiiotool executable")
    ap.add_argument("--env-script", default=str(default_env_script))
    ap.add_argument("--out-dir", default=str(default_out_dir))
    ap.add_argument("--trace", action="store_true")
    args = ap.parse_args()

    exe = Path(args.bin).expanduser().resolve()
    cwd = Path(args.cwd).expanduser().resolve()
    oiiotool = Path(args.oiiotool).expanduser().resolve()
    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if not exe.exists():
        return _fail(f"binary not found: {exe}")
    if not oiiotool.exists():
        return _fail(f"oiiotool not found: {oiiotool}")
    if not runner.exists():
        return _fail(f"runner not found: {runner}")

    select_image = out_dir / "select_input.tif"
    pan_image = out_dir / "pan_input.tif"
    try:
        _generate_fixture(oiiotool, select_image, 320, 240)
        _generate_fixture(oiiotool, pan_image, 3072, 2048)
    except subprocess.SubprocessError as exc:
        return _fail(f"failed to generate fixtures: {exc}")

    env = _load_env_from_script(Path(args.env_script).expanduser())

    try:
        select_state, _ = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            select_image,
            out_dir,
            "select_drag",
            env,
            [
                "--key-chord",
                "ctrl+a",
                "--mouse-pos-image-rel",
                "0.25",
                "0.35",
                "--mouse-drag",
                "120",
                "90",
                "--mouse-drag-button",
                "0",
            ],
            want_layout=False,
            trace=args.trace,
        )
        pan_state, _ = _run_case(
            repo_root,
            runner,
            exe,
            cwd,
            pan_image,
            out_dir,
            "left_drag_pan",
            env,
            [
                "--mouse-pos-image-rel",
                "0.50",
                "0.50",
                "--mouse-drag",
                "220",
                "0",
                "--mouse-drag-button",
                "0",
            ],
            want_layout=False,
            trace=args.trace,
        )
    except subprocess.TimeoutExpired as exc:
        return _fail(str(exc))
    except (subprocess.SubprocessError, RuntimeError) as exc:
        return _fail(str(exc))

    if not select_state.get("selection_active", False):
        return _fail("selection drag did not activate a selection")
    if not _selection_bounds_valid(select_state):
        return _fail("selection drag did not produce valid selection bounds")
    if not _area_probe_is_initialized(select_state):
        return _fail("selection drag did not initialize area probe statistics")

    if not _selection_is_cleared(pan_state):
        return _fail("left drag pan created or retained a selection")
    scroll = pan_state.get("scroll", [0.0, 0.0])
    scroll_x = float(scroll[0]) if isinstance(scroll, list) and len(scroll) > 0 else 0.0
    scroll_y = float(scroll[1]) if isinstance(scroll, list) and len(scroll) > 1 else 0.0
    if abs(scroll_x) < 1.0 and abs(scroll_y) < 1.0:
        return _fail("left drag pan did not move the viewport")

    print(f"select_drag: {select_state['_state_path']}")
    print(f"left_drag_pan: {pan_state['_state_path']}")
    print(f"artifacts: {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

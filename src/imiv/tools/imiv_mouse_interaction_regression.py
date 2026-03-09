#!/usr/bin/env python3
"""Regression check for imiv click-zoom and middle-drag pan."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def _run_case(
    python_exe: str,
    runner: Path,
    repo_root: Path,
    out_dir: Path,
    image_path: Path,
    name: str,
    extra_args: list[str],
    bin_path: str,
    cwd_path: str,
    trace: bool,
) -> dict:
    screenshot_out = out_dir / f"{name}.png"
    state_out = out_dir / f"{name}.json"
    junit_out = out_dir / f"{name}.junit.xml"
    cmd = [
        python_exe,
        str(runner),
        "--open",
        str(image_path),
        "--key-chord",
        "ctrl+0",
        "--mouse-pos-image-rel",
        "0.5",
        "0.5",
        "--screenshot-out",
        str(screenshot_out),
        "--state-json-out",
        str(state_out),
        "--junit-out",
        str(junit_out),
    ]
    if bin_path:
        cmd.extend(["--bin", bin_path])
    if cwd_path:
        cmd.extend(["--cwd", cwd_path])
    if trace:
        cmd.append("--trace")
    cmd.extend(extra_args)

    print("run:", " ".join(cmd))
    subprocess.run(cmd, cwd=str(repo_root), check=True)

    with state_out.open("r", encoding="utf-8") as handle:
        state = json.load(handle)
    state["screenshot_sha256"] = _sha256(screenshot_out)
    state["screenshot_path"] = str(screenshot_out)
    state["state_path"] = str(state_out)
    return state


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    default_image = repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"
    default_out = repo_root / "build_u" / "imiv_captures" / "mouse_interaction_regression"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--open", default=str(default_image), help="Image to open")
    ap.add_argument("--out-dir", default=str(default_out), help="Output directory")
    ap.add_argument("--bin", default="", help="Optional imiv binary override")
    ap.add_argument("--cwd", default="", help="Optional imiv working directory override")
    ap.add_argument("--trace", action="store_true", help="Enable runner trace")
    args = ap.parse_args()

    image_path = Path(args.open).resolve()
    if not image_path.exists():
        return _fail(f"image not found: {image_path}")

    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    runner = repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"

    baseline = _run_case(
        sys.executable,
        runner,
        repo_root,
        out_dir,
        image_path,
        "baseline_ctrl0",
        [],
        args.bin,
        args.cwd,
        args.trace,
    )
    left_click = _run_case(
        sys.executable,
        runner,
        repo_root,
        out_dir,
        image_path,
        "left_click_zoom_in",
        ["--mouse-click", "0"],
        args.bin,
        args.cwd,
        args.trace,
    )
    right_click = _run_case(
        sys.executable,
        runner,
        repo_root,
        out_dir,
        image_path,
        "right_click_zoom_out",
        ["--mouse-click", "1"],
        args.bin,
        args.cwd,
        args.trace,
    )
    middle_drag = _run_case(
        sys.executable,
        runner,
        repo_root,
        out_dir,
        image_path,
        "middle_drag_pan",
        ["--mouse-drag-button", "2", "--mouse-drag", "120", "80"],
        args.bin,
        args.cwd,
        args.trace,
    )

    for name, state in (
        ("baseline", baseline),
        ("left_click", left_click),
        ("right_click", right_click),
        ("middle_drag", middle_drag),
    ):
        if not state.get("image_loaded", False):
            return _fail(f"{name}: image not loaded")

    baseline_zoom = float(baseline["zoom"])
    left_zoom = float(left_click["zoom"])
    right_zoom = float(right_click["zoom"])
    middle_zoom = float(middle_drag["zoom"])
    if abs(baseline_zoom - 1.0) > 1.0e-3:
        return _fail(f"baseline zoom expected 1.0, got {baseline_zoom:.6f}")
    if left_zoom <= baseline_zoom + 1.0e-3:
        return _fail(
            f"left click did not zoom in: baseline={baseline_zoom:.6f}, left={left_zoom:.6f}"
        )
    if right_zoom >= baseline_zoom - 1.0e-3:
        return _fail(
            f"right click did not zoom out: baseline={baseline_zoom:.6f}, right={right_zoom:.6f}"
        )
    if abs(middle_zoom - baseline_zoom) > 1.0e-3:
        return _fail(
            f"middle drag changed zoom unexpectedly: baseline={baseline_zoom:.6f}, middle={middle_zoom:.6f}"
        )

    baseline_scroll = baseline["scroll"]
    middle_scroll = middle_drag["scroll"]
    scroll_dx = abs(float(middle_scroll[0]) - float(baseline_scroll[0]))
    scroll_dy = abs(float(middle_scroll[1]) - float(baseline_scroll[1]))
    if max(scroll_dx, scroll_dy) <= 1.0:
        return _fail(
            "middle drag did not change scroll enough: "
            f"baseline={baseline_scroll}, middle={middle_scroll}"
        )

    if left_click["screenshot_sha256"] == baseline["screenshot_sha256"]:
        return _fail("left click screenshot matches baseline")
    if right_click["screenshot_sha256"] == baseline["screenshot_sha256"]:
        return _fail("right click screenshot matches baseline")
    if middle_drag["screenshot_sha256"] == baseline["screenshot_sha256"]:
        return _fail("middle drag screenshot matches baseline")

    print("baseline zoom:", f"{baseline_zoom:.6f}", "scroll:", baseline_scroll)
    print("left click zoom:", f"{left_zoom:.6f}")
    print("right click zoom:", f"{right_zoom:.6f}")
    print("middle drag zoom:", f"{middle_zoom:.6f}", "scroll:", middle_scroll)
    print("artifacts:", out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

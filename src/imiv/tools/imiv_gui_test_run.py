#!/usr/bin/env python3
"""Run imiv automation via Dear ImGui Test Engine.

Examples:
  # Screenshot only
  python3 src/imiv/tools/imiv_gui_test_run.py \
    --screenshot-out build_u/test_captures/smoke.png

  # Layout JSON + SVG
  python3 src/imiv/tools/imiv_gui_test_run.py \
    --layout-json-out build_u/test_captures/layout_items.json \
    --layout-items \
    --svg-out build_u/test_captures/layout_items.svg \
    --svg-items --svg-labels

  # Screenshot + layout + junit xml
  python3 src/imiv/tools/imiv_gui_test_run.py \
    --screenshot-out build_u/test_captures/smoke.png \
    --layout-json-out build_u/test_captures/layout.json \
    --junit-out build_u/test_captures/imiv_tests.junit.xml
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def _resolve_path(path: str, root: Path) -> Path:
    p = Path(path)
    if p.is_absolute():
        return p
    return (root / p).resolve()


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


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    default_bin = _default_binary(repo_root)

    ap = argparse.ArgumentParser(description="imiv automation runner")
    ap.add_argument("--bin", default=str(default_bin), help="imiv executable path")
    ap.add_argument("--cwd", default="", help="Working directory for imiv (default: binary dir)")
    ap.add_argument("--open", default="", help="Optional image path to open at startup")

    ap.add_argument("--screenshot-out", default="", help="Enable screenshot test and write to this output path")
    ap.add_argument("--screenshot-frames", type=int, default=1, help="Number of screenshot frames")
    ap.add_argument("--screenshot-delay-frames", type=int, default=3, help="Initial delay before screenshot capture")
    ap.add_argument("--screenshot-save-all", action="store_true", help="Save all screenshot frames")

    ap.add_argument("--layout-json-out", default="", help="Enable layout JSON dump and write to this path")
    ap.add_argument("--layout-items", action="store_true", help="Include per-item data in layout JSON")
    ap.add_argument("--layout-depth", type=int, default=8, help="Layout gather depth")
    ap.add_argument("--layout-delay-frames", type=int, default=3, help="Initial delay before layout dump")
    ap.add_argument("--state-json-out", default="", help="Enable viewer state JSON dump and write to this path")
    ap.add_argument("--state-delay-frames", type=int, default=3, help="Initial delay before viewer state dump")

    ap.add_argument("--svg-out", default="", help="Post-convert layout JSON to SVG at this path")
    ap.add_argument("--svg-items", action="store_true", help="Draw items in SVG (implies --layout-items)")
    ap.add_argument("--svg-no-items", action="store_true", help="Disable items in SVG")
    ap.add_argument("--svg-items-clipped", action="store_true", help="Use clipped item rects in SVG")
    ap.add_argument("--svg-labels", action="store_true", help="Draw labels in SVG")

    ap.add_argument("--junit-out", default="", help="Enable JUnit XML export to this path")
    ap.add_argument("--trace", action="store_true", help="Enable test engine trace logs")
    ap.add_argument("--show-drag-overlay", action="store_true",
                    help="Force the drag-and-drop dimming overlay during automation")
    ap.add_argument("--ocio-use", default="",
                    help="Optional OCIO enable override for automation (true/false)")
    ap.add_argument("--ocio-display", default="",
                    help="Optional live OCIO display override for automation")
    ap.add_argument("--ocio-view", default="",
                    help="Optional live OCIO view override for automation")
    ap.add_argument("--ocio-image-color-space", default="",
                    help="Optional live OCIO image color space override for automation")
    ap.add_argument("--ocio-apply-frame", type=int, default=0,
                    help="Frame number at which automation OCIO overrides should begin")
    ap.add_argument("--key-chord", default="",
                    help="Optional ImGui key chord before capture/layout, e.g. ctrl+i or ctrl+0")
    ap.add_argument("--mouse-pos", nargs=2, type=float, metavar=("X", "Y"), default=None,
                    help="Move mouse to absolute position before capture/layout")
    ap.add_argument("--mouse-pos-window-rel", nargs=2, type=float, metavar=("X", "Y"), default=None,
                    help="Move mouse to viewport-relative position [0..1] before capture/layout")
    ap.add_argument("--mouse-pos-image-rel", nargs=2, type=float, metavar=("U", "V"), default=None,
                    help="Move mouse to image-relative position [0..1] before capture/layout")
    ap.add_argument("--mouse-click", type=int, default=None,
                    help="Optional mouse click button index before capture/layout")
    ap.add_argument("--mouse-wheel", nargs=2, type=float, metavar=("DX", "DY"), default=None,
                    help="Optional mouse wheel delta before capture/layout")
    ap.add_argument("--mouse-drag", nargs=2, type=float, metavar=("DX", "DY"), default=None,
                    help="Optional mouse drag delta before capture/layout")
    ap.add_argument("--mouse-drag-button", type=int, default=0,
                    help="Mouse button index for --mouse-drag")
    args = ap.parse_args()

    exe = _resolve_path(args.bin, repo_root)
    if not exe.exists():
        print(f"error: binary not found: {exe}", file=sys.stderr)
        return 2

    run_cwd = Path(args.cwd).resolve() if args.cwd else exe.parent.resolve()

    layout_json_out = args.layout_json_out
    if args.svg_out and not layout_json_out:
        svg_path = _resolve_path(args.svg_out, repo_root)
        layout_json_out = str(svg_path.with_suffix(".json"))

    want_screenshot = bool(args.screenshot_out)
    want_layout = bool(layout_json_out)
    want_state = bool(args.state_json_out)
    want_svg = bool(args.svg_out)
    want_junit = bool(args.junit_out)

    if not (want_screenshot or want_layout or want_state):
        print(
            "error: select at least one automation task: --screenshot-out, --layout-json-out, --state-json-out, or --svg-out",
            file=sys.stderr,
        )
        return 2

    env = dict(os.environ)
    env["IMIV_IMGUI_TEST_ENGINE"] = "1"
    env["IMIV_IMGUI_TEST_ENGINE_EXIT_ON_FINISH"] = "1"

    if args.open:
        open_path = _resolve_path(args.open, repo_root)
        env["IMIV_IMGUI_TEST_ENGINE_OPEN_PATH"] = str(open_path)

    if args.trace:
        env["IMIV_IMGUI_TEST_ENGINE_TRACE"] = "1"

    if args.show_drag_overlay:
        env["IMIV_IMGUI_TEST_ENGINE_SHOW_DRAG_OVERLAY"] = "1"

    if args.ocio_use:
        env["IMIV_IMGUI_TEST_ENGINE_OCIO_USE"] = args.ocio_use
    if args.ocio_display:
        env["IMIV_IMGUI_TEST_ENGINE_OCIO_DISPLAY"] = args.ocio_display
    if args.ocio_view:
        env["IMIV_IMGUI_TEST_ENGINE_OCIO_VIEW"] = args.ocio_view
    if args.ocio_image_color_space:
        env["IMIV_IMGUI_TEST_ENGINE_OCIO_IMAGE_COLOR_SPACE"] = args.ocio_image_color_space
    if args.ocio_apply_frame > 0:
        env["IMIV_IMGUI_TEST_ENGINE_OCIO_APPLY_FRAME"] = str(args.ocio_apply_frame)

    if args.key_chord:
        env["IMIV_IMGUI_TEST_ENGINE_KEY_CHORD"] = args.key_chord

    if args.mouse_pos:
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_X"] = str(args.mouse_pos[0])
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_Y"] = str(args.mouse_pos[1])

    if args.mouse_pos_window_rel:
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_WINDOW_REL_X"] = str(args.mouse_pos_window_rel[0])
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_WINDOW_REL_Y"] = str(args.mouse_pos_window_rel[1])

    if args.mouse_pos_image_rel:
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_IMAGE_REL_X"] = str(args.mouse_pos_image_rel[0])
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_IMAGE_REL_Y"] = str(args.mouse_pos_image_rel[1])

    if args.mouse_click is not None:
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_CLICK_BUTTON"] = str(args.mouse_click)

    if args.mouse_wheel:
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_WHEEL_X"] = str(args.mouse_wheel[0])
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_WHEEL_Y"] = str(args.mouse_wheel[1])

    if args.mouse_drag:
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_DRAG_DX"] = str(args.mouse_drag[0])
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_DRAG_DY"] = str(args.mouse_drag[1])
        env["IMIV_IMGUI_TEST_ENGINE_MOUSE_DRAG_BUTTON"] = str(args.mouse_drag_button)

    if want_screenshot:
        out = _resolve_path(args.screenshot_out, repo_root)
        out.parent.mkdir(parents=True, exist_ok=True)
        env["IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT"] = "1"
        env["IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_OUT"] = str(out)
        env["IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_FRAMES"] = str(max(1, args.screenshot_frames))
        env["IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_DELAY_FRAMES"] = str(max(0, args.screenshot_delay_frames))
        if args.screenshot_save_all:
            env["IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_SAVE_ALL"] = "1"

    if want_layout:
        out = _resolve_path(layout_json_out, repo_root)
        out.parent.mkdir(parents=True, exist_ok=True)
        env["IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP"] = "1"
        env["IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_OUT"] = str(out)
        env["IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_DEPTH"] = str(max(1, args.layout_depth))
        env["IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_DELAY_FRAMES"] = str(max(0, args.layout_delay_frames))
        if args.layout_items or args.svg_items or (want_svg and not args.svg_no_items):
            env["IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_ITEMS"] = "1"

    if want_state:
        out = _resolve_path(args.state_json_out, repo_root)
        out.parent.mkdir(parents=True, exist_ok=True)
        env["IMIV_IMGUI_TEST_ENGINE_STATE_DUMP"] = "1"
        env["IMIV_IMGUI_TEST_ENGINE_STATE_DUMP_OUT"] = str(out)
        env["IMIV_IMGUI_TEST_ENGINE_STATE_DUMP_DELAY_FRAMES"] = str(max(0, args.state_delay_frames))

    if want_junit:
        junit_out = _resolve_path(args.junit_out, repo_root)
        junit_out.parent.mkdir(parents=True, exist_ok=True)
        env["IMIV_IMGUI_TEST_ENGINE_JUNIT_XML"] = "1"
        env["IMIV_IMGUI_TEST_ENGINE_JUNIT_OUT"] = str(junit_out)

    print(f"run: {exe}")
    print(f"cwd: {run_cwd}")
    rc = subprocess.run(
        [str(exe), "-F"], cwd=str(run_cwd), env=env, check=False
    ).returncode
    if rc != 0:
        print(f"error: imiv exited with code {rc}", file=sys.stderr)
        return rc

    if want_svg:
        if not want_layout:
            print("error: internal: svg requested without layout json", file=sys.stderr)
            return 2

        json_path = _resolve_path(layout_json_out, repo_root)
        svg_path = _resolve_path(args.svg_out, repo_root)
        svg_path.parent.mkdir(parents=True, exist_ok=True)
        converter = repo_root / "src" / "imiv" / "tools" / "imiv_layout_json_to_svg.py"
        cmd = [sys.executable, str(converter), "--in", str(json_path), "--out", str(svg_path)]
        if args.svg_items:
            cmd.append("--items")
        if args.svg_no_items:
            cmd.append("--no-items")
        if args.svg_items_clipped:
            cmd.append("--items-clipped")
        if args.svg_labels:
            cmd.append("--labels")

        print("post:", " ".join(cmd))
        rc_svg = subprocess.run(cmd, cwd=str(repo_root), check=False).returncode
        if rc_svg != 0:
            return rc_svg

    if want_junit:
        junit_path = _resolve_path(args.junit_out, repo_root)
        if not junit_path.exists():
            print(f"error: junit output not found: {junit_path}", file=sys.stderr)
            return 2
        try:
            root = ET.parse(junit_path).getroot()
        except ET.ParseError as exc:
            print(f"error: failed to parse junit xml '{junit_path}': {exc}", file=sys.stderr)
            return 2

        failures = 0
        errors = 0
        if root.tag == "testsuite":
            failures += int(root.attrib.get("failures", "0") or "0")
            errors += int(root.attrib.get("errors", "0") or "0")
        else:
            for suite in root.iter("testsuite"):
                failures += int(suite.attrib.get("failures", "0") or "0")
                errors += int(suite.attrib.get("errors", "0") or "0")

        if failures > 0 or errors > 0:
            print(f"error: junit reported failures={failures}, errors={errors}", file=sys.stderr)
            return 1

    if want_state:
        state_path = _resolve_path(args.state_json_out, repo_root)
        if not state_path.exists():
            print(f"error: state output not found: {state_path}", file=sys.stderr)
            return 2

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

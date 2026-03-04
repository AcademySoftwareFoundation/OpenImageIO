#!/usr/bin/env python3
"""Convert imiv ImGui Test Engine layout JSON to an SVG overlay."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any, Iterable, Tuple


def _die(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    raise SystemExit(2)


def _xml_escape(s: str) -> str:
    return (
        s.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
        .replace("'", "&apos;")
    )


def _as_f(v: Any) -> float:
    try:
        return float(v)
    except Exception:
        return float("nan")


def _vec2(v: Any) -> Tuple[float, float]:
    if not isinstance(v, list) or len(v) != 2:
        return (float("nan"), float("nan"))
    return (_as_f(v[0]), _as_f(v[1]))


def _rect(obj: Any, key: str | None = None) -> Tuple[float, float, float, float]:
    r = obj if key is None else obj.get(key)
    if not isinstance(r, dict):
        return (float("nan"), float("nan"), float("nan"), float("nan"))
    mn = _vec2(r.get("min"))
    mx = _vec2(r.get("max"))
    return (mn[0], mn[1], mx[0], mx[1])


def _finite(vals: Iterable[float]) -> Iterable[float]:
    for x in vals:
        if math.isfinite(x):
            yield x


def _bounds_from_rects(rects: Iterable[Tuple[float, float, float, float]]) -> Tuple[float, float, float, float]:
    xs: list[float] = []
    ys: list[float] = []
    for x0, y0, x1, y1 in rects:
        xs.extend([x0, x1])
        ys.extend([y0, y1])
    fx = list(_finite(xs))
    fy = list(_finite(ys))
    if not fx or not fy:
        return (0.0, 0.0, 0.0, 0.0)
    return (min(fx), min(fy), max(fx), max(fy))


def _iter_window_rects(data: dict[str, Any]) -> Iterable[Tuple[float, float, float, float]]:
    for w in data.get("windows", []) or []:
        yield _rect(w, "rect")


def _iter_item_rects(data: dict[str, Any], use_clipped: bool) -> Iterable[Tuple[float, float, float, float]]:
    key = "rect_clipped" if use_clipped else "rect_full"
    for w in data.get("windows", []) or []:
        for it in w.get("items", []) or []:
            yield _rect(it, key)


def _svg_rect(x0: float, y0: float, x1: float, y1: float) -> Tuple[float, float, float, float]:
    x = min(x0, x1)
    y = min(y0, y1)
    w = abs(x1 - x0)
    h = abs(y1 - y0)
    return (x, y, w, h)


def _rect_polygon_points(x0: float, y0: float, x1: float, y1: float) -> str:
    x, y, w, h = _svg_rect(x0, y0, x1, y1)
    x2 = x + w
    y2 = y + h
    return f"{x:.3f},{y:.3f} {x2:.3f},{y:.3f} {x2:.3f},{y2:.3f} {x:.3f},{y2:.3f}"


def main() -> int:
    ap = argparse.ArgumentParser(description="Convert imiv layout JSON to SVG overlay.")
    ap.add_argument("--in", dest="in_path", required=True, help="Input layout JSON path.")
    ap.add_argument("--out", dest="out_path", required=True, help="Output SVG path.")
    ap.add_argument("--items", action="store_true", help="Force drawing items (buttons/widgets).")
    ap.add_argument("--no-items", action="store_true", help="Disable drawing items.")
    ap.add_argument("--items-clipped", action="store_true", help="Use clipped rects for items (rect_clipped).")
    ap.add_argument("--labels", action="store_true", help="Draw window names as text labels.")
    ap.add_argument("--bg", default="#ffffff", help="Background color (default: #ffffff).")
    ap.add_argument("--window-stroke", default="#ef4444", help="Window fill color.")
    ap.add_argument("--item-stroke", default="#3b82f6", help="Item fill color.")
    ap.add_argument("--window-fill-opacity", type=float, default=0.08, help="Window fill opacity.")
    ap.add_argument("--item-fill-opacity", type=float, default=0.12, help="Item fill opacity.")
    ap.add_argument("--pad", type=float, default=10.0, help="Padding around bounds (default: 10).")
    args = ap.parse_args()

    in_path = Path(args.in_path)
    if not in_path.exists():
        _die(f"input not found: {in_path}")

    data = json.loads(in_path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        _die("input json is not an object")

    has_items = any(
        isinstance(w, dict) and isinstance(w.get("items"), list) and len(w.get("items")) > 0
        for w in (data.get("windows", []) or [])
    )
    draw_items = (args.items or has_items) and not args.no_items

    win_bounds = _bounds_from_rects(_iter_window_rects(data))
    item_bounds = (0.0, 0.0, 0.0, 0.0)
    if draw_items:
        item_bounds = _bounds_from_rects(_iter_item_rects(data, args.items_clipped))

    rects = [win_bounds]
    if draw_items:
        rects.append(item_bounds)
    x0, y0, x1, y1 = _bounds_from_rects(rects)

    pad = float(args.pad)
    x0 -= pad
    y0 -= pad
    x1 += pad
    y1 += pad

    width = max(0.0, x1 - x0)
    height = max(0.0, y1 - y0)
    if width <= 0.0 or height <= 0.0:
        _die("computed empty bounds (no windows/items?)")

    ox, oy = x0, y0

    def nx(x: float) -> float:
        return x - ox

    def ny(y: float) -> float:
        return y - oy

    out_path = Path(args.out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    svg_lines: list[str] = []
    svg_lines.append('<?xml version="1.0" encoding="UTF-8"?>')
    svg_lines.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" '
        f'width="{width:.0f}" height="{height:.0f}" viewBox="0 0 {width:.3f} {height:.3f}">'
    )
    svg_lines.append("<style>")
    svg_lines.append(
        "  text { font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: 12px; }"
    )
    svg_lines.append("  .bg { shape-rendering: crispEdges; }")
    svg_lines.append("  .win { stroke: none; }")
    svg_lines.append("  .item { stroke: none; }")
    svg_lines.append("</style>")

    svg_lines.append(
        f'<rect class="bg" x="0" y="0" width="{width:.3f}" height="{height:.3f}" '
        f'fill="{_xml_escape(args.bg)}" stroke="none"/>'
    )

    win_fill_opacity = max(0.0, min(1.0, float(args.window_fill_opacity)))
    svg_lines.append('<g id="windows">')
    for window in data.get("windows", []) or []:
        rx0, ry0, rx1, ry1 = _rect(window, "rect")
        if not all(math.isfinite(v) for v in (rx0, ry0, rx1, ry1)):
            continue
        x, y, w, h = _svg_rect(nx(rx0), ny(ry0), nx(rx1), ny(ry1))
        points = _rect_polygon_points(x, y, x + w, y + h)
        svg_lines.append(
            f'<polygon class="win" points="{points}" '
            f'fill="{_xml_escape(args.window_stroke)}" fill-opacity="{win_fill_opacity:.3f}" stroke="none"/>'
        )
        if args.labels:
            name = str(window.get("name") or "")
            svg_lines.append(
                f'<text x="{x + 4.0:.3f}" y="{y + 14.0:.3f}" fill="{_xml_escape(args.window_stroke)}">'
                f"{_xml_escape(name)}</text>"
            )
    svg_lines.append("</g>")

    if draw_items:
        key = "rect_clipped" if args.items_clipped else "rect_full"
        item_fill_opacity = max(0.0, min(1.0, float(args.item_fill_opacity)))
        svg_lines.append('<g id="items">')
        for window in data.get("windows", []) or []:
            for item in window.get("items", []) or []:
                rx0, ry0, rx1, ry1 = _rect(item, key)
                if not all(math.isfinite(v) for v in (rx0, ry0, rx1, ry1)):
                    continue
                x, y, w, h = _svg_rect(nx(rx0), ny(ry0), nx(rx1), ny(ry1))
                points = _rect_polygon_points(x, y, x + w, y + h)
                svg_lines.append(
                    f'<polygon class="item" points="{points}" '
                    f'fill="{_xml_escape(args.item_stroke)}" fill-opacity="{item_fill_opacity:.3f}" stroke="none"/>'
                )
                if args.labels:
                    label = str(item.get("debug") or "")
                    if label:
                        svg_lines.append(
                            f'<text x="{x + 2.0:.3f}" y="{y + 11.0:.3f}" fill="{_xml_escape(args.item_stroke)}">'
                            f"{_xml_escape(label)}</text>"
                        )
        svg_lines.append("</g>")

    svg_lines.append("</svg>")
    out_path.write_text("\n".join(svg_lines) + "\n", encoding="utf-8")
    print(f"wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

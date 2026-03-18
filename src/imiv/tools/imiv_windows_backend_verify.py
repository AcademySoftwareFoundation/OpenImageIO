#!/usr/bin/env python3
"""Configure, build, and run imiv backend regressions on Windows.

Intended usage from an active Python environment, e.g. a uv venv:

  python src\\imiv\\tools\\imiv_windows_backend_verify.py ^
    --backend vulkan ^
    --build-dir build ^
    --config Debug ^
    --out-dir windows_verify ^
    --trace
"""

from __future__ import annotations

import argparse
import os
import platform
import subprocess
import sys
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def _default_image(repo_root: Path) -> Path:
    return repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"


def _write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _run_capture(cmd: list[str], *, cwd: Path, env: dict[str, str] | None = None) -> str:
    proc = subprocess.run(
        cmd,
        cwd=str(cwd),
        env=env,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    output = proc.stdout or ""
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {' '.join(cmd)}\n{output}"
        )
    return output


def _run_logged(
    cmd: list[str],
    *,
    cwd: Path,
    log_path: Path,
    env: dict[str, str] | None = None,
) -> int:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as log_handle:
        proc = subprocess.Popen(
            cmd,
            cwd=str(cwd),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        assert proc.stdout is not None
        for line in proc.stdout:
            sys.stdout.write(line)
            log_handle.write(line)
        return proc.wait()


def _find_program(build_dir: Path, config: str, stem: str) -> Path | None:
    candidates = [
        build_dir / "bin" / config / f"{stem}.exe",
        build_dir / config / f"{stem}.exe",
        build_dir / "bin" / f"{stem}.exe",
        build_dir / "src" / stem / config / f"{stem}.exe",
        build_dir / "src" / stem / f"{stem}.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    return None


def _discover_env_script(build_dir: Path, config: str) -> Path | None:
    candidates = [
        build_dir / "imiv_env.sh",
        build_dir / config / "imiv_env.sh",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    return None


def _make_base_py_cmd() -> list[str]:
    return [sys.executable]


def _generic_smoke_runner_cmd(
    repo_root: Path,
    exe: Path,
    run_cwd: Path,
    out_dir: Path,
    image: Path,
    *,
    trace: bool,
) -> list[str]:
    cmd = _make_base_py_cmd() + [
        str(repo_root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"),
        "--bin",
        str(exe),
        "--cwd",
        str(run_cwd),
        "--open",
        str(image),
        "--screenshot-out",
        str(out_dir / "smoke.png"),
        "--layout-json-out",
        str(out_dir / "smoke.layout.json"),
        "--layout-items",
        "--state-json-out",
        str(out_dir / "smoke.state.json"),
    ]
    if trace:
        cmd.append("--trace")
    return cmd


def _backend_smoke_cmd(
    repo_root: Path,
    backend: str,
    exe: Path,
    run_cwd: Path,
    out_dir: Path,
    image: Path,
    *,
    env_script: Path | None,
    trace: bool,
) -> list[str]:
    if backend == "opengl":
        cmd = _make_base_py_cmd() + [
            str(repo_root / "src" / "imiv" / "tools" / "imiv_opengl_smoke_regression.py"),
            "--bin",
            str(exe),
            "--cwd",
            str(run_cwd),
            "--out-dir",
            str(out_dir),
            "--open",
            str(image),
        ]
        if env_script is not None:
            cmd.extend(["--env-script", str(env_script)])
        if trace:
            cmd.append("--trace")
        return cmd
    return _generic_smoke_runner_cmd(
        repo_root,
        exe,
        run_cwd,
        out_dir,
        image,
        trace=trace,
    )


def _selection_cmd(
    repo_root: Path,
    backend: str,
    exe: Path,
    run_cwd: Path,
    oiiotool: Path,
    out_dir: Path,
    *,
    env_script: Path | None,
    trace: bool,
) -> list[str]:
    script = (
        repo_root / "src" / "imiv" / "tools" / "imiv_opengl_selection_regression.py"
        if backend == "opengl"
        else repo_root / "src" / "imiv" / "tools" / "imiv_selection_regression.py"
    )
    cmd = _make_base_py_cmd() + [
        str(script),
        "--bin",
        str(exe),
        "--cwd",
        str(run_cwd),
        "--oiiotool",
        str(oiiotool),
        "--out-dir",
        str(out_dir),
    ]
    if env_script is not None:
        cmd.extend(["--env-script", str(env_script)])
    if trace:
        cmd.append("--trace")
    return cmd


def _ocio_missing_cmd(
    repo_root: Path,
    exe: Path,
    run_cwd: Path,
    oiiotool: Path,
    idiff: Path,
    out_dir: Path,
    image: Path,
    *,
    env_script: Path | None,
    trace: bool,
) -> list[str]:
    cmd = _make_base_py_cmd() + [
        str(
            repo_root
            / "src"
            / "imiv"
            / "tools"
            / "imiv_ocio_missing_fallback_regression.py"
        ),
        "--bin",
        str(exe),
        "--cwd",
        str(run_cwd),
        "--oiiotool",
        str(oiiotool),
        "--idiff",
        str(idiff),
        "--out-dir",
        str(out_dir),
        "--open",
        str(image),
    ]
    if env_script is not None:
        cmd.extend(["--env-script", str(env_script)])
    if trace:
        cmd.append("--trace")
    return cmd


def _ocio_config_source_cmd(
    repo_root: Path,
    exe: Path,
    run_cwd: Path,
    oiiotool: Path,
    idiff: Path,
    out_dir: Path,
    ocio_config: str,
    *,
    env_script: Path | None,
    trace: bool,
) -> list[str]:
    cmd = _make_base_py_cmd() + [
        str(
            repo_root
            / "src"
            / "imiv"
            / "tools"
            / "imiv_ocio_config_source_regression.py"
        ),
        "--bin",
        str(exe),
        "--cwd",
        str(run_cwd),
        "--oiiotool",
        str(oiiotool),
        "--idiff",
        str(idiff),
        "--out-dir",
        str(out_dir),
        "--ocio-config",
        ocio_config,
    ]
    if env_script is not None:
        cmd.extend(["--env-script", str(env_script)])
    if trace:
        cmd.append("--trace")
    return cmd


def _ocio_live_cmd(
    repo_root: Path,
    exe: Path,
    run_cwd: Path,
    oiiotool: Path,
    idiff: Path,
    out_dir: Path,
    ocio_config: str,
    switch_mode: str,
    *,
    env_script: Path | None,
    trace: bool,
) -> list[str]:
    cmd = _make_base_py_cmd() + [
        str(
            repo_root
            / "src"
            / "imiv"
            / "tools"
            / "imiv_ocio_live_update_regression.py"
        ),
        "--bin",
        str(exe),
        "--cwd",
        str(run_cwd),
        "--oiiotool",
        str(oiiotool),
        "--idiff",
        str(idiff),
        "--out-dir",
        str(out_dir),
        "--ocio-config",
        ocio_config,
        "--switch-mode",
        switch_mode,
    ]
    if env_script is not None:
        cmd.extend(["--env-script", str(env_script)])
    if trace:
        cmd.append("--trace")
    return cmd


def _system_info_text(args: argparse.Namespace, repo_root: Path) -> str:
    lines = [
        f"repo_root={repo_root}",
        f"backend={args.backend}",
        f"build_dir={args.build_dir}",
        f"out_dir={args.out_dir}",
        f"config={args.config}",
        f"image={args.image}",
        f"ocio_config={args.ocio_config or 'ocio://default'}",
        "",
        f"platform={platform.platform()}",
        f"python={sys.version}",
        f"executable={sys.executable}",
        "",
    ]
    env_names = [
        "VULKAN_SDK",
        "OCIO",
        "PATH",
        "VisualStudioVersion",
        "VSCMD_VER",
    ]
    for name in env_names:
        lines.append(f"{name}={os.environ.get(name, '')}")
    lines.append("")
    for cmd in (
        ["cmd", "/c", "ver"],
        ["cmake", "--version"],
        ["ninja", "--version"],
        ["where", "python"],
        ["where", "cmake"],
    ):
        lines.append(f"$ {' '.join(cmd)}")
        try:
            lines.append(_run_capture(cmd, cwd=repo_root).strip())
        except Exception as exc:  # pragma: no cover - best effort
            lines.append(f"<failed: {exc}>")
        lines.append("")
    return "\n".join(lines)


def main() -> int:
    repo_root = _repo_root()
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--backend",
        default="vulkan",
        choices=("vulkan", "opengl"),
        help="Renderer backend to configure and verify",
    )
    ap.add_argument(
        "--build-dir",
        default=str(repo_root / "build"),
        help="CMake build directory",
    )
    ap.add_argument(
        "--out-dir",
        default="",
        help="Output/log directory (default: <build>/imiv_captures/<backend>_verify)",
    )
    ap.add_argument(
        "--config",
        default="Debug",
        help="Build configuration for multi-config generators",
    )
    ap.add_argument(
        "--jobs",
        type=int,
        default=int(os.environ.get("IMIV_JOBS", "8")),
        help="Parallel build jobs",
    )
    ap.add_argument(
        "--image",
        default=str(_default_image(repo_root)),
        help="Image to open for smoke/fallback checks",
    )
    ap.add_argument(
        "--ocio-config",
        default="",
        help="Optional external OCIO config path or URI (default: ocio://default)",
    )
    ap.add_argument(
        "--trace",
        action="store_true",
        help="Enable verbose Python regression tracing",
    )
    ap.add_argument(
        "--skip-configure",
        action="store_true",
        help="Skip the cmake configure step",
    )
    ap.add_argument(
        "--skip-build",
        action="store_true",
        help="Skip the cmake build step",
    )
    args = ap.parse_args()

    build_dir = Path(args.build_dir).expanduser().resolve()
    out_dir = (
        Path(args.out_dir).expanduser().resolve()
        if args.out_dir
        else (build_dir / "imiv_captures" / f"{args.backend}_verify").resolve()
    )
    out_dir.mkdir(parents=True, exist_ok=True)
    image_path = Path(args.image).expanduser().resolve()
    ocio_config = args.ocio_config.strip() or "ocio://default"

    system_info_log = out_dir / "system_info.txt"
    configure_log = out_dir / "cmake_configure.log"
    build_log = out_dir / "cmake_build.log"
    smoke_log = out_dir / "verify_smoke.log"
    selection_log = out_dir / "verify_selection.log"
    ocio_missing_log = out_dir / "verify_ocio_missing.log"
    ocio_config_log = out_dir / "verify_ocio_config_source.log"
    ocio_live_log = out_dir / "verify_ocio_live.log"
    ocio_live_display_log = out_dir / "verify_ocio_live_display.log"

    _write_text(system_info_log, _system_info_text(args, repo_root))

    if not image_path.exists():
        print(f"error: image not found: {image_path}", file=sys.stderr)
        return 2

    if not args.skip_configure:
        configure_cmd = [
            "cmake",
            "-S",
            str(repo_root),
            "-B",
            str(build_dir),
            f"-DOIIO_IMIV_RENDERER={args.backend}",
        ]
        if _run_logged(configure_cmd, cwd=repo_root, log_path=configure_log) != 0:
            print(f"error: configure failed, see {configure_log}", file=sys.stderr)
            return 1

    if not args.skip_build:
        build_cmd = [
            "cmake",
            "--build",
            str(build_dir),
            "--config",
            args.config,
            "--target",
            "imiv",
            "oiiotool",
            "idiff",
            "--parallel",
            str(max(1, args.jobs)),
        ]
        if _run_logged(build_cmd, cwd=repo_root, log_path=build_log) != 0:
            print(f"error: build failed, see {build_log}", file=sys.stderr)
            return 1

    imiv = _find_program(build_dir, args.config, "imiv")
    oiiotool = _find_program(build_dir, args.config, "oiiotool")
    idiff = _find_program(build_dir, args.config, "idiff")
    if imiv is None:
        print(f"error: could not locate imiv under {build_dir}", file=sys.stderr)
        return 1
    if oiiotool is None:
        print(f"error: could not locate oiiotool under {build_dir}", file=sys.stderr)
        return 1
    if idiff is None:
        print(f"error: could not locate idiff under {build_dir}", file=sys.stderr)
        return 1

    env_script = _discover_env_script(build_dir, args.config)
    run_cwd = imiv.parent

    failures: list[str] = []

    checks: list[tuple[str, list[str], Path]] = [
        (
            "smoke",
            _backend_smoke_cmd(
                repo_root,
                args.backend,
                imiv,
                run_cwd,
                out_dir / "runtime",
                image_path,
                env_script=env_script,
                trace=args.trace,
            ),
            smoke_log,
        ),
        (
            "selection",
            _selection_cmd(
                repo_root,
                args.backend,
                imiv,
                run_cwd,
                oiiotool,
                out_dir / "runtime_selection",
                env_script=env_script,
                trace=args.trace,
            ),
            selection_log,
        ),
        (
            "ocio_missing",
            _ocio_missing_cmd(
                repo_root,
                imiv,
                run_cwd,
                oiiotool,
                idiff,
                out_dir / "runtime_ocio_missing",
                image_path,
                env_script=env_script,
                trace=args.trace,
            ),
            ocio_missing_log,
        ),
        (
            "ocio_config_source",
            _ocio_config_source_cmd(
                repo_root,
                imiv,
                run_cwd,
                oiiotool,
                idiff,
                out_dir / "runtime_ocio_config_source",
                ocio_config,
                env_script=env_script,
                trace=args.trace,
            ),
            ocio_config_log,
        ),
        (
            "ocio_live",
            _ocio_live_cmd(
                repo_root,
                imiv,
                run_cwd,
                oiiotool,
                idiff,
                out_dir / "runtime_ocio_live",
                ocio_config,
                "view",
                env_script=env_script,
                trace=args.trace,
            ),
            ocio_live_log,
        ),
        (
            "ocio_live_display",
            _ocio_live_cmd(
                repo_root,
                imiv,
                run_cwd,
                oiiotool,
                idiff,
                out_dir / "runtime_ocio_live_display",
                ocio_config,
                "display",
                env_script=env_script,
                trace=args.trace,
            ),
            ocio_live_display_log,
        ),
    ]

    for name, cmd, log_path in checks:
        rc = _run_logged(cmd, cwd=repo_root, log_path=log_path)
        if rc != 0:
            failures.append(name)

    print("")
    print(f"Verification logs written to: {out_dir}")
    print(f"  system:     {system_info_log}")
    print(f"  configure:  {configure_log}")
    print(f"  build:      {build_log}")
    print(f"  smoke:      {smoke_log}")
    print(f"  selection:  {selection_log}")
    print(f"  ocio-miss:  {ocio_missing_log}")
    print(f"  ocio-src:   {ocio_config_log}")
    print(f"  ocio-live:  {ocio_live_log}")
    print(f"  ocio-disp:  {ocio_live_display_log}")
    print(f"  runtime:    {out_dir / 'runtime'}")
    print(f"  runtime+sel:{out_dir / 'runtime_selection'}")
    print(f"  runtime+om: {out_dir / 'runtime_ocio_missing'}")
    print(f"  runtime+os: {out_dir / 'runtime_ocio_config_source'}")
    print(f"  runtime+ol: {out_dir / 'runtime_ocio_live'}")
    print(f"  runtime+od: {out_dir / 'runtime_ocio_live_display'}")

    if failures:
        print("")
        print("Failed checks:", ", ".join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

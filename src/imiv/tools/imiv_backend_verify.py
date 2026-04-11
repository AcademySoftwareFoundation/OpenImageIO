#!/usr/bin/env python3
"""Configure, build, and run imiv backend regressions across platforms.

Canonical usage from the repo root:

  python src/imiv/tools/imiv_backend_verify.py --backend vulkan --build-dir build_u

If you invoke this from the repo root with `uv run`, use `--no-project`:

  uv run --no-project python src/imiv/tools/imiv_backend_verify.py --backend vulkan

Without `--no-project`, uv may try to build/install the repository's Python
package first because this checkout has a `pyproject.toml`.
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import sys
from pathlib import Path

from imiv_test_utils import (
    default_backend as imiv_default_backend,
    default_build_dir as imiv_default_build_dir,
    default_config as imiv_default_config,
    default_image as imiv_default_image,
    discover_env_script,
    find_program,
    format_elapsed,
    generic_smoke_runner_command,
    is_linux,
    is_macos,
    is_windows,
    load_env_from_script,
    repo_root as imiv_repo_root,
    run_capture_output,
    run_timed_logged_process,
    script_command,
    supported_backends as imiv_supported_backends,
    write_text,
)

def _smoke_checks(
    repo_root: Path,
    backend: str,
    exe: Path,
    run_cwd: Path,
    image: Path,
    out_dir: Path,
    env_script: Path | None,
    trace: bool,
) -> list[tuple[str, list[str], Path, dict[str, str] | None]]:
    checks: list[tuple[str, list[str], Path, dict[str, str] | None]] = []
    if backend == "metal":
        checks.append(
            (
                "smoke",
                script_command(
                    repo_root
                    / "src"
                    / "imiv"
                    / "tools"
                    / "imiv_metal_screenshot_regression.py",
                    backend=backend,
                    exe=exe,
                    run_cwd=run_cwd,
                    out_dir=out_dir / "runtime",
                    trace=trace,
                    extra=["--open", str(image)],
                    env_script=env_script,
                ),
                out_dir / "verify_smoke.log",
                None,
            )
        )
        return checks

    if backend == "opengl":
        checks.append(
            (
                "smoke",
                script_command(
                    repo_root / "src" / "imiv" / "tools" / "imiv_opengl_smoke_regression.py",
                    backend=backend,
                    exe=exe,
                    run_cwd=run_cwd,
                    out_dir=out_dir / "runtime",
                    trace=trace,
                    extra=["--open", str(image)],
                    env_script=env_script,
                ),
                out_dir / "verify_smoke.log",
                None,
            )
        )
        return checks

    smoke_out = out_dir / "runtime"
    smoke_env = {"IMIV_CONFIG_HOME": str(smoke_out / "cfg")}
    checks.append(
        (
            "smoke",
            generic_smoke_runner_command(
                exe=exe,
                run_cwd=run_cwd,
                backend=backend,
                image=image,
                out_dir=smoke_out,
                trace=trace,
            ),
            out_dir / "verify_smoke.log",
            smoke_env,
        )
    )
    return checks


def _ux_checks(
    repo_root: Path,
    backend: str,
    exe: Path,
    run_cwd: Path,
    oiiotool: Path,
    out_dir: Path,
    env_script: Path | None,
    trace: bool,
) -> list[tuple[str, list[str], Path, dict[str, str] | None]]:
    script = repo_root / "src" / "imiv" / "tools" / "imiv_ux_actions_regression.py"
    cmd = script_command(
        script,
        backend=backend,
        exe=exe,
        run_cwd=run_cwd,
        out_dir=out_dir / "runtime_ux",
        trace=trace,
        extra=["--oiiotool", str(oiiotool)],
        env_script=env_script,
    )
    return [("ux", cmd, out_dir / "verify_ux.log", None)]


def _sampling_checks(
    repo_root: Path,
    backend: str,
    exe: Path,
    run_cwd: Path,
    oiiotool: Path,
    out_dir: Path,
    env_script: Path | None,
    trace: bool,
) -> list[tuple[str, list[str], Path, dict[str, str] | None]]:
    script = repo_root / "src" / "imiv" / "tools" / "imiv_sampling_regression.py"
    cmd = script_command(
        script,
        backend=backend,
        exe=exe,
        run_cwd=run_cwd,
        out_dir=out_dir / "runtime_sampling",
        trace=trace,
        extra=["--oiiotool", str(oiiotool)],
        env_script=env_script,
    )
    return [("sampling", cmd, out_dir / "verify_sampling.log", None)]


def _rgb_checks(
    repo_root: Path,
    backend: str,
    exe: Path,
    run_cwd: Path,
    oiiotool: Path,
    out_dir: Path,
    source_image: Path,
    env_script: Path | None,
    trace: bool,
) -> list[tuple[str, list[str], Path, dict[str, str] | None]]:
    script = repo_root / "src" / "imiv" / "tools" / "imiv_rgb_input_regression.py"
    cmd = script_command(
        script,
        backend=backend,
        exe=exe,
        run_cwd=run_cwd,
        out_dir=out_dir / "runtime_rgb",
        trace=trace,
        extra=[
            "--oiiotool",
            str(oiiotool),
            "--source-image",
            str(source_image),
        ],
        env_script=env_script,
    )
    return [("rgb", cmd, out_dir / "verify_rgb.log", None)]


def _ocio_checks(
    repo_root: Path,
    backend: str,
    exe: Path,
    run_cwd: Path,
    oiiotool: Path,
    idiff: Path,
    out_dir: Path,
    image: Path,
    ocio_config: str,
    env_script: Path | None,
    trace: bool,
) -> list[tuple[str, list[str], Path, dict[str, str] | None]]:
    checks: list[tuple[str, list[str], Path, dict[str, str] | None]] = []
    checks.append(
        (
            "ocio_missing",
            script_command(
                repo_root
                / "src"
                / "imiv"
                / "tools"
                / "imiv_ocio_missing_fallback_regression.py",
                backend=backend,
                exe=exe,
                run_cwd=run_cwd,
                out_dir=out_dir / "runtime_ocio_missing",
                trace=trace,
                extra=[
                    "--oiiotool",
                    str(oiiotool),
                    "--idiff",
                    str(idiff),
                    "--open",
                    str(image),
                ],
                env_script=env_script,
            ),
            out_dir / "verify_ocio_missing.log",
            None,
        )
    )
    checks.append(
        (
            "ocio_config_source",
            script_command(
                repo_root
                / "src"
                / "imiv"
                / "tools"
                / "imiv_ocio_config_source_regression.py",
                backend=backend,
                exe=exe,
                run_cwd=run_cwd,
                out_dir=out_dir / "runtime_ocio_config_source",
                trace=trace,
                extra=[
                    "--oiiotool",
                    str(oiiotool),
                    "--idiff",
                    str(idiff),
                    "--ocio-config",
                    ocio_config,
                ],
                env_script=env_script,
            ),
            out_dir / "verify_ocio_config_source.log",
            None,
        )
    )
    for mode, name, log_name, runtime_dir in (
        ("view", "ocio_live", "verify_ocio_live.log", "runtime_ocio_live"),
        (
            "display",
            "ocio_live_display",
            "verify_ocio_live_display.log",
            "runtime_ocio_live_display",
        ),
    ):
        checks.append(
            (
                name,
                script_command(
                    repo_root
                    / "src"
                    / "imiv"
                    / "tools"
                    / "imiv_ocio_live_update_regression.py",
                    backend=backend,
                    exe=exe,
                    run_cwd=run_cwd,
                    out_dir=out_dir / runtime_dir,
                    trace=trace,
                    extra=[
                        "--oiiotool",
                        str(oiiotool),
                        "--idiff",
                        str(idiff),
                        "--ocio-config",
                        ocio_config,
                        "--switch-mode",
                        mode,
                    ],
                    env_script=env_script,
                ),
                out_dir / log_name,
                None,
            )
        )
    return checks


def _system_info_text(args: argparse.Namespace, repo_root: Path) -> str:
    lines = [
        f"repo_root={repo_root}",
        f"backend={args.backend}",
        f"build_dir={args.build_dir}",
        f"out_dir={args.out_dir}",
        f"config={args.config or '<single-config>'}",
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
        "DISPLAY",
        "WAYLAND_DISPLAY",
        "XDG_SESSION_TYPE",
        "WSL_DISTRO_NAME",
        "VisualStudioVersion",
        "VSCMD_VER",
    ]
    for name in env_names:
        lines.append(f"{name}={os.environ.get(name, '')}")
    lines.append("")

    commands: list[list[str]] = [["cmake", "--version"], ["ninja", "--version"]]
    if is_windows():
        commands.extend([
            ["cmd", "/c", "ver"],
            ["where", "python"],
            ["where", "cmake"],
        ])
    else:
        commands.extend([
            ["uname", "-a"],
            ["python3", "--version"],
            ["clang++", "--version"],
        ])
        if shutil.which("g++"):
            commands.append(["g++", "--version"])
        if is_macos():
            commands.extend([["sw_vers"], ["xcode-select", "-p"]])
        elif is_linux() and shutil.which("lsb_release"):
            commands.append(["lsb_release", "-a"])

    for cmd in commands:
        lines.append(f"$ {' '.join(cmd)}")
        try:
            lines.append(run_capture_output(cmd, cwd=repo_root).strip())
        except Exception as exc:  # pragma: no cover - best effort
            lines.append(f"<failed: {exc}>")
        lines.append("")

    if is_linux():
        try:
            osrelease = Path("/proc/sys/kernel/osrelease").read_text(encoding="utf-8")
            lines.append(f"wsl={'1' if 'microsoft' in osrelease.lower() else '0'}")
        except Exception:
            pass
        try:
            lines.append("")
            lines.append(Path("/etc/os-release").read_text(encoding="utf-8").strip())
        except Exception:
            pass

    return "\n".join(lines)


def main() -> int:
    repo_root = imiv_repo_root()
    supported_backends = imiv_supported_backends()
    default_build_dir = imiv_default_build_dir(repo_root)

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--backend",
        default=imiv_default_backend(),
        choices=supported_backends,
        help="Renderer backend to configure and verify",
    )
    ap.add_argument(
        "--build-dir",
        default=str(default_build_dir),
        help="CMake build directory",
    )
    ap.add_argument(
        "--out-dir",
        default="",
        help="Output/log directory (default: <build>/imiv_captures/<backend>_verify)",
    )
    ap.add_argument(
        "--config",
        default=imiv_default_config(),
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
        default=str(imiv_default_image(repo_root)),
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
    args.build_dir = str(build_dir)
    args.out_dir = str(out_dir)

    image_path = Path(args.image).expanduser().resolve()
    args.image = str(image_path)
    ocio_config = args.ocio_config.strip() or "ocio://default"

    system_info_log = out_dir / "system_info.txt"
    configure_log = out_dir / "cmake_configure.log"
    build_log = out_dir / "cmake_build.log"
    timings: list[tuple[str, float]] = []

    write_text(system_info_log, _system_info_text(args, repo_root))

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
        configure_rc, configure_elapsed = run_timed_logged_process(
            configure_cmd,
            cwd=repo_root,
            log_path=configure_log,
            label="configure",
        )
        timings.append(("configure", configure_elapsed))
        if configure_rc != 0:
            print(f"error: configure failed, see {configure_log}", file=sys.stderr)
            return 1
    else:
        write_text(configure_log, "skip: configure step skipped\n")
        timings.append(("configure(skipped)", 0.0))

    if not args.skip_build:
        build_cmd = [
            "cmake",
            "--build",
            str(build_dir),
        ]
        if args.config:
            build_cmd.extend(["--config", args.config])
        build_cmd.extend([
            "--target",
            "imiv",
            "oiiotool",
            "idiff",
            "--parallel",
            str(max(1, args.jobs)),
        ])
        build_rc, build_elapsed = run_timed_logged_process(
            build_cmd,
            cwd=repo_root,
            log_path=build_log,
            label="build",
        )
        timings.append(("build", build_elapsed))
        if build_rc != 0:
            print(f"error: build failed, see {build_log}", file=sys.stderr)
            return 1
    else:
        write_text(build_log, "skip: build step skipped\n")
        timings.append(("build(skipped)", 0.0))

    imiv = find_program(build_dir, args.config, "imiv")
    oiiotool = find_program(build_dir, args.config, "oiiotool")
    idiff = find_program(build_dir, args.config, "idiff")
    if imiv is None:
        print(f"error: could not locate imiv under {build_dir}", file=sys.stderr)
        return 1
    if oiiotool is None:
        print(f"error: could not locate oiiotool under {build_dir}", file=sys.stderr)
        return 1
    if idiff is None:
        print(f"error: could not locate idiff under {build_dir}", file=sys.stderr)
        return 1

    env_script = discover_env_script(build_dir, args.config)
    base_env = load_env_from_script(env_script)
    run_cwd = imiv.parent

    checks: list[tuple[str, list[str], Path, dict[str, str] | None]] = []
    checks.extend(
        _smoke_checks(
            repo_root,
            args.backend,
            imiv,
            run_cwd,
            image_path,
            out_dir,
            env_script,
            args.trace,
        )
    )
    checks.extend(
        _rgb_checks(
            repo_root,
            args.backend,
            imiv,
            run_cwd,
            oiiotool,
            out_dir,
            image_path,
            env_script,
            args.trace,
        )
    )
    checks.extend(
        _ux_checks(
            repo_root,
            args.backend,
            imiv,
            run_cwd,
            oiiotool,
            out_dir,
            env_script,
            args.trace,
        )
    )
    checks.extend(
        _sampling_checks(
            repo_root,
            args.backend,
            imiv,
            run_cwd,
            oiiotool,
            out_dir,
            env_script,
            args.trace,
        )
    )
    checks.extend(
        _ocio_checks(
            repo_root,
            args.backend,
            imiv,
            run_cwd,
            oiiotool,
            idiff,
            out_dir,
            image_path,
            ocio_config,
            env_script,
            args.trace,
        )
    )

    failures: list[str] = []
    smoke_failed = False
    skip_after_smoke = {
        "ocio_missing",
        "ocio_config_source",
        "ocio_live",
        "ocio_live_display",
    }
    for name, cmd, log_path, env_override in checks:
        if smoke_failed and name in skip_after_smoke:
            message = "skip: skipped because smoke failed\n"
            write_text(log_path, message)
            sys.stdout.write(f"==> {name}: skipped because smoke failed\n")
            timings.append((f"{name}(skipped)", 0.0))
            continue
        env = dict(base_env)
        if env_override:
            env.update(env_override)
        rc, elapsed = run_timed_logged_process(
            cmd,
            cwd=repo_root,
            log_path=log_path,
            env=env,
            label=name,
        )
        timings.append((name, elapsed))
        if rc != 0:
            failures.append(name)
            if name == "smoke":
                smoke_failed = True

    print("")
    print(f"Verification logs written to: {out_dir}")
    print(f"  system:      {system_info_log}")
    print(f"  configure:   {configure_log}")
    print(f"  build:       {build_log}")
    print(f"  smoke:       {out_dir / 'verify_smoke.log'}")
    print(f"  runtime+s:   {out_dir / 'runtime'}")
    print(f"  rgb:         {out_dir / 'verify_rgb.log'}")
    print(f"  runtime+rgb: {out_dir / 'runtime_rgb'}")
    print(f"  ux:          {out_dir / 'verify_ux.log'}")
    print(f"  runtime+ux:  {out_dir / 'runtime_ux'}")
    print(f"  sampling:    {out_dir / 'verify_sampling.log'}")
    print(f"  runtime+sa:  {out_dir / 'runtime_sampling'}")
    print(f"  ocio-miss:   {out_dir / 'verify_ocio_missing.log'}")
    print(f"  runtime+om:  {out_dir / 'runtime_ocio_missing'}")
    print(f"  ocio-src:    {out_dir / 'verify_ocio_config_source.log'}")
    print(f"  runtime+os:  {out_dir / 'runtime_ocio_config_source'}")
    print(f"  ocio-live:   {out_dir / 'verify_ocio_live.log'}")
    print(f"  runtime+ol:  {out_dir / 'runtime_ocio_live'}")
    print(f"  ocio-disp:   {out_dir / 'verify_ocio_live_display.log'}")
    print(f"  runtime+od:  {out_dir / 'runtime_ocio_live_display'}")
    print("  timings:")
    for name, elapsed in timings:
        print(f"    {name:<18} {format_elapsed(elapsed)}")

    if failures:
        print("")
        print("Failed checks:", ", ".join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

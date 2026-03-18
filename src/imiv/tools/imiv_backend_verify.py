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
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Iterable


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def _default_image(repo_root: Path) -> Path:
    return repo_root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"


def _is_windows() -> bool:
    return os.name == "nt"


def _is_macos() -> bool:
    return sys.platform == "darwin"


def _is_linux() -> bool:
    return sys.platform.startswith("linux")


def _default_build_dir(repo_root: Path) -> Path:
    if _is_linux() and (repo_root / "build_u").exists():
        return repo_root / "build_u"
    return repo_root / "build"


def _default_backend() -> str:
    return "metal" if _is_macos() else "vulkan"


def _supported_backends() -> tuple[str, ...]:
    if _is_macos():
        return ("metal", "opengl", "vulkan")
    return ("vulkan", "opengl")


def _default_config() -> str:
    return "Debug" if _is_windows() else ""


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


def _candidate_paths(build_dir: Path, config: str, stem: str) -> Iterable[Path]:
    suffixes = [stem]
    if _is_windows():
        suffixes = [f"{stem}.exe", stem]
    for suffix in suffixes:
        yield build_dir / "bin" / config / suffix if config else build_dir / "bin" / suffix
        if config:
            yield build_dir / config / suffix
        yield build_dir / "bin" / suffix
        yield build_dir / "src" / stem / config / suffix if config else build_dir / "src" / stem / suffix
        yield build_dir / "src" / stem / suffix
        if config:
            yield build_dir / "Release" / suffix
            yield build_dir / "Debug" / suffix
            yield build_dir / "bin" / "Release" / suffix
            yield build_dir / "bin" / "Debug" / suffix


def _find_program(build_dir: Path, config: str, stem: str) -> Path | None:
    seen: set[Path] = set()
    for candidate in _candidate_paths(build_dir, config, stem):
        if candidate in seen:
            continue
        seen.add(candidate)
        if candidate.exists():
            return candidate.resolve()
    return None


def _discover_env_script(build_dir: Path, config: str) -> Path | None:
    candidates = [build_dir / "imiv_env.sh"]
    if config:
        candidates.append(build_dir / config / "imiv_env.sh")
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    return None


def _load_env_from_script(script_path: Path | None) -> dict[str, str]:
    env = dict(os.environ)
    if script_path is None or not script_path.exists() or shutil.which("bash") is None:
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


def _base_py_cmd() -> list[str]:
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
    cmd = _base_py_cmd() + [
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


def _script_cmd(
    script: Path,
    *,
    exe: Path,
    run_cwd: Path,
    out_dir: Path,
    trace: bool,
    extra: list[str] | None = None,
    env_script: Path | None = None,
) -> list[str]:
    cmd = _base_py_cmd() + [
        str(script),
        "--bin",
        str(exe),
        "--cwd",
        str(run_cwd),
        "--out-dir",
        str(out_dir),
    ]
    if extra:
        cmd.extend(extra)
    if env_script is not None:
        cmd.extend(["--env-script", str(env_script)])
    if trace:
        cmd.append("--trace")
    return cmd


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
                _script_cmd(
                    repo_root / "src" / "imiv" / "tools" / "imiv_metal_smoke_regression.py",
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
        checks.append(
            (
                "screenshot",
                _script_cmd(
                    repo_root
                    / "src"
                    / "imiv"
                    / "tools"
                    / "imiv_metal_screenshot_regression.py",
                    exe=exe,
                    run_cwd=run_cwd,
                    out_dir=out_dir / "runtime_screenshot",
                    trace=trace,
                    extra=["--open", str(image)],
                    env_script=env_script,
                ),
                out_dir / "verify_screenshot.log",
                None,
            )
        )
        checks.append(
            (
                "sampling",
                _script_cmd(
                    repo_root
                    / "src"
                    / "imiv"
                    / "tools"
                    / "imiv_metal_sampling_regression.py",
                    exe=exe,
                    run_cwd=run_cwd,
                    out_dir=out_dir / "runtime_sampling",
                    trace=trace,
                    env_script=env_script,
                ),
                out_dir / "verify_sampling.log",
                None,
            )
        )
        checks.append(
            (
                "orientation",
                _script_cmd(
                    repo_root
                    / "src"
                    / "imiv"
                    / "tools"
                    / "imiv_metal_orientation_regression.py",
                    exe=exe,
                    run_cwd=run_cwd,
                    out_dir=out_dir / "runtime_orientation",
                    trace=trace,
                    extra=["--open", str(image)],
                    env_script=env_script,
                ),
                out_dir / "verify_orientation.log",
                None,
            )
        )
        return checks

    if backend == "opengl":
        checks.append(
            (
                "smoke",
                _script_cmd(
                    repo_root / "src" / "imiv" / "tools" / "imiv_opengl_smoke_regression.py",
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
            _generic_smoke_runner_cmd(
                repo_root,
                exe,
                run_cwd,
                smoke_out,
                image,
                trace=trace,
            ),
            out_dir / "verify_smoke.log",
            smoke_env,
        )
    )
    return checks


def _selection_checks(
    repo_root: Path,
    backend: str,
    exe: Path,
    run_cwd: Path,
    oiiotool: Path,
    out_dir: Path,
    env_script: Path | None,
    trace: bool,
) -> list[tuple[str, list[str], Path, dict[str, str] | None]]:
    if backend == "metal":
        return []
    script = (
        repo_root / "src" / "imiv" / "tools" / "imiv_opengl_selection_regression.py"
        if backend == "opengl"
        else repo_root / "src" / "imiv" / "tools" / "imiv_selection_regression.py"
    )
    cmd = _script_cmd(
        script,
        exe=exe,
        run_cwd=run_cwd,
        out_dir=out_dir / "runtime_selection",
        trace=trace,
        extra=["--oiiotool", str(oiiotool)],
        env_script=env_script,
    )
    return [("selection", cmd, out_dir / "verify_selection.log", None)]


def _ocio_checks(
    repo_root: Path,
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
            _script_cmd(
                repo_root
                / "src"
                / "imiv"
                / "tools"
                / "imiv_ocio_missing_fallback_regression.py",
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
            _script_cmd(
                repo_root
                / "src"
                / "imiv"
                / "tools"
                / "imiv_ocio_config_source_regression.py",
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
                _script_cmd(
                    repo_root
                    / "src"
                    / "imiv"
                    / "tools"
                    / "imiv_ocio_live_update_regression.py",
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
    if _is_windows():
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
        if _is_macos():
            commands.extend([["sw_vers"], ["xcode-select", "-p"]])
        elif _is_linux() and shutil.which("lsb_release"):
            commands.append(["lsb_release", "-a"])

    for cmd in commands:
        lines.append(f"$ {' '.join(cmd)}")
        try:
            lines.append(_run_capture(cmd, cwd=repo_root).strip())
        except Exception as exc:  # pragma: no cover - best effort
            lines.append(f"<failed: {exc}>")
        lines.append("")

    if _is_linux():
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
    repo_root = _repo_root()
    supported_backends = _supported_backends()
    default_build_dir = _default_build_dir(repo_root)

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--backend",
        default=_default_backend(),
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
        default=_default_config(),
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
    args.build_dir = str(build_dir)
    args.out_dir = str(out_dir)

    image_path = Path(args.image).expanduser().resolve()
    args.image = str(image_path)
    ocio_config = args.ocio_config.strip() or "ocio://default"

    system_info_log = out_dir / "system_info.txt"
    configure_log = out_dir / "cmake_configure.log"
    build_log = out_dir / "cmake_build.log"

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
    else:
        _write_text(configure_log, "skip: configure step skipped\n")

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
        if _run_logged(build_cmd, cwd=repo_root, log_path=build_log) != 0:
            print(f"error: build failed, see {build_log}", file=sys.stderr)
            return 1
    else:
        _write_text(build_log, "skip: build step skipped\n")

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
    base_env = _load_env_from_script(env_script)
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
        _selection_checks(
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
    for name, cmd, log_path, env_override in checks:
        env = dict(base_env)
        if env_override:
            env.update(env_override)
        rc = _run_logged(cmd, cwd=repo_root, log_path=log_path, env=env)
        if rc != 0:
            failures.append(name)

    print("")
    print(f"Verification logs written to: {out_dir}")
    print(f"  system:      {system_info_log}")
    print(f"  configure:   {configure_log}")
    print(f"  build:       {build_log}")
    print(f"  smoke:       {out_dir / 'verify_smoke.log'}")
    print(f"  runtime+s:   {out_dir / 'runtime'}")
    if args.backend != "metal":
        print(f"  selection:   {out_dir / 'verify_selection.log'}")
        print(f"  runtime+sel: {out_dir / 'runtime_selection'}")
    if args.backend == "metal":
        print(f"  screenshot:  {out_dir / 'verify_screenshot.log'}")
        print(f"  runtime+ss:  {out_dir / 'runtime_screenshot'}")
        print(f"  sampling:    {out_dir / 'verify_sampling.log'}")
        print(f"  runtime+sa:  {out_dir / 'runtime_sampling'}")
        print(f"  orient:      {out_dir / 'verify_orientation.log'}")
        print(f"  runtime+or:  {out_dir / 'runtime_orientation'}")
    print(f"  ocio-miss:   {out_dir / 'verify_ocio_missing.log'}")
    print(f"  runtime+om:  {out_dir / 'runtime_ocio_missing'}")
    print(f"  ocio-src:    {out_dir / 'verify_ocio_config_source.log'}")
    print(f"  runtime+os:  {out_dir / 'runtime_ocio_config_source'}")
    print(f"  ocio-live:   {out_dir / 'verify_ocio_live.log'}")
    print(f"  runtime+ol:  {out_dir / 'runtime_ocio_live'}")
    print(f"  ocio-disp:   {out_dir / 'verify_ocio_live_display.log'}")
    print(f"  runtime+od:  {out_dir / 'runtime_ocio_live_display'}")

    if failures:
        print("")
        print("Failed checks:", ", ".join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

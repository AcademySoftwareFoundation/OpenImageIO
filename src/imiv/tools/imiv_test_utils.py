#!/usr/bin/env python3
"""Shared helpers for imiv Python regression scripts."""

from __future__ import annotations

import os
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Iterable, Mapping, Sequence


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def _first_existing_path(candidates: Sequence[Path]) -> Path | None:
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def _default_tool_candidates(
    root: Path, tool_name: str, source_dir: str
) -> list[Path]:
    exe_name = f"{tool_name}.exe"
    return [
        root / "build_u" / "bin" / tool_name,
        root / "build" / "bin" / tool_name,
        root / "build_u" / "src" / source_dir / tool_name,
        root / "build" / "src" / source_dir / tool_name,
        root / "build" / "Debug" / exe_name,
        root / "build" / "Release" / exe_name,
    ]


def default_binary(root: Path | None = None) -> Path:
    root = repo_root() if root is None else root
    candidates = _default_tool_candidates(root, "imiv", "imiv")
    return _first_existing_path(candidates) or candidates[0]


def default_oiiotool(root: Path | None = None) -> Path:
    root = repo_root() if root is None else root
    candidates = _default_tool_candidates(root, "oiiotool", "oiiotool")
    found = _first_existing_path(candidates)
    if found is not None:
        return found
    which = shutil.which("oiiotool")
    if which is not None:
        return Path(which)
    return candidates[0]


def default_idiff(root: Path | None = None) -> Path:
    root = repo_root() if root is None else root
    candidates = _default_tool_candidates(root, "idiff", "idiff")
    found = _first_existing_path(candidates)
    if found is not None:
        return found
    which = shutil.which("idiff")
    if which is not None:
        return Path(which)
    return candidates[0]


def default_image(root: Path | None = None) -> Path:
    root = repo_root() if root is None else root
    return root / "ASWF" / "logos" / "openimageio-stacked-gradient.png"


def default_env_script(root: Path | None = None, exe: Path | None = None) -> Path:
    root = repo_root() if root is None else root
    candidates: list[Path] = []
    if exe is not None:
        exe = exe.resolve()
        candidates.extend(
            [exe.parent / "imiv_env.sh", exe.parent.parent / "imiv_env.sh"]
        )
    candidates.extend(
        [root / "build" / "imiv_env.sh", root / "build_u" / "imiv_env.sh"]
    )
    return _first_existing_path(candidates) or candidates[0]


def runner_path(root: Path | None = None) -> Path:
    root = repo_root() if root is None else root
    return root / "src" / "imiv" / "tools" / "imiv_gui_test_run.py"


def is_windows() -> bool:
    return os.name == "nt"


def is_macos() -> bool:
    return sys.platform == "darwin"


def is_linux() -> bool:
    return sys.platform.startswith("linux")


def default_build_dir(root: Path | None = None) -> Path:
    root = repo_root() if root is None else root
    if is_linux() and (root / "build_u").exists():
        return root / "build_u"
    return root / "build"


def default_backend() -> str:
    return "metal" if is_macos() else "vulkan"


def supported_backends() -> tuple[str, ...]:
    return ("metal", "opengl", "vulkan") if is_macos() else ("vulkan", "opengl")


def default_config() -> str:
    return "Debug" if is_windows() else ""


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def format_elapsed(seconds: float) -> str:
    return f"{seconds:.2f}s"


def _candidate_paths(build_dir: Path, config: str, stem: str) -> Iterable[Path]:
    suffixes = [f"{stem}.exe", stem] if is_windows() else [stem]
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


def find_program(build_dir: Path, config: str, stem: str) -> Path | None:
    seen: set[Path] = set()
    for candidate in _candidate_paths(build_dir, config, stem):
        if candidate in seen:
            continue
        seen.add(candidate)
        if candidate.exists():
            return candidate.resolve()
    return None


def discover_env_script(build_dir: Path, config: str) -> Path | None:
    candidates = [build_dir / "imiv_env.sh"]
    if config:
        candidates.append(build_dir / config / "imiv_env.sh")
    return _first_existing_path(candidates)


def load_env_from_script(script_path: Path | None) -> dict[str, str]:
    env = dict(os.environ)
    if (
        script_path is None
        or not script_path.exists()
        or shutil.which("bash") is None
    ):
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


def path_for_imiv_output(path: Path, run_cwd: Path) -> str:
    try:
        return os.path.relpath(path, run_cwd)
    except ValueError:
        return str(path)


def resolve_run_cwd(exe: Path, cwd_arg: str) -> Path:
    return Path(cwd_arg).expanduser().resolve() if cwd_arg else exe.parent.resolve()


def resolve_existing_tool(
    requested: str | Path, fallback: Path, *, allow_which: bool = True
) -> Path:
    candidate = Path(requested).expanduser() if requested else fallback
    if candidate.exists():
        return candidate.resolve()
    if fallback.exists():
        return fallback.resolve()
    if allow_which:
        found = shutil.which(str(candidate))
        if found is not None:
            return Path(found).resolve()
    return candidate.resolve()


def runner_command(exe: Path, run_cwd: Path, backend: str = "") -> list[str]:
    cmd = [
        sys.executable,
        str(runner_path()),
        "--bin",
        str(exe),
        "--cwd",
        str(run_cwd),
    ]
    if backend:
        cmd.extend(["--backend", backend])
    return cmd


def script_command(
    script: Path,
    *,
    exe: Path,
    run_cwd: Path,
    backend: str = "",
    out_dir: Path | None = None,
    trace: bool = False,
    extra: Sequence[str | Path] | None = None,
    env_script: Path | None = None,
) -> list[str]:
    cmd = [
        sys.executable,
        str(script),
        "--bin",
        str(exe),
        "--cwd",
        str(run_cwd),
    ]
    if backend:
        cmd.extend(["--backend", backend])
    if out_dir is not None:
        cmd.extend(["--out-dir", str(out_dir)])
    if extra:
        cmd.extend(str(part) for part in extra)
    if env_script is not None:
        cmd.extend(["--env-script", str(env_script)])
    if trace:
        cmd.append("--trace")
    return cmd


def generic_smoke_runner_command(
    *,
    exe: Path,
    run_cwd: Path,
    backend: str,
    image: Path,
    out_dir: Path,
    trace: bool = False,
) -> list[str]:
    cmd = runner_command(exe, run_cwd, backend)
    cmd.extend(
        [
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
    )
    if trace:
        cmd.append("--trace")
    return cmd


def run_capture_output(
    cmd: Sequence[str | Path],
    *,
    cwd: Path,
    env: Mapping[str, str] | None = None,
    timeout: float | None = None,
) -> str:
    proc = run_captured_process(cmd, cwd=cwd, env=env, timeout=timeout)
    output = proc.stdout or ""
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {' '.join(str(part) for part in cmd)}\n{output}"
        )
    return output


def run_logged_process(
    cmd: Sequence[str | Path],
    *,
    cwd: Path,
    env: Mapping[str, str] | None = None,
    timeout: float | None = None,
    log_path: Path | None = None,
    check: bool = False,
) -> subprocess.CompletedProcess:
    argv = [str(part) for part in cmd]
    print("run:", " ".join(argv))
    if log_path is None:
        return subprocess.run(
            argv, cwd=str(cwd), env=dict(env) if env is not None else None,
            check=check, timeout=timeout
        )

    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as log_handle:
        return subprocess.run(
            argv,
            cwd=str(cwd),
            env=dict(env) if env is not None else None,
            check=check,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            timeout=timeout,
        )


def run_captured_process(
    cmd: Sequence[str | Path],
    *,
    cwd: Path,
    env: Mapping[str, str] | None = None,
    timeout: float | None = None,
    check: bool = False,
) -> subprocess.CompletedProcess[str]:
    argv = [str(part) for part in cmd]
    print("run:", " ".join(argv))
    return subprocess.run(
        argv,
        cwd=str(cwd),
        env=dict(env) if env is not None else None,
        check=check,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=timeout,
    )


def run_timed_logged_process(
    cmd: Sequence[str | Path],
    *,
    cwd: Path,
    log_path: Path,
    env: Mapping[str, str] | None = None,
    label: str | None = None,
) -> tuple[int, float]:
    argv = [str(part) for part in cmd]
    log_path.parent.mkdir(parents=True, exist_ok=True)
    step_label = label or log_path.stem
    command_text = " ".join(shlex.quote(part) for part in argv)
    start = time.monotonic()
    with log_path.open("w", encoding="utf-8") as log_handle:
        header = f"==> {step_label}: {command_text}\n"
        sys.stdout.write(header)
        log_handle.write(header)
        proc = subprocess.Popen(
            argv,
            cwd=str(cwd),
            env=dict(env) if env is not None else None,
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
        rc = proc.wait()
        elapsed = time.monotonic() - start
        footer = f"<== {step_label}: rc={rc} elapsed={format_elapsed(elapsed)}\n"
        sys.stdout.write(footer)
        log_handle.write(footer)
        return rc, elapsed

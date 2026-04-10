#!/usr/bin/env python3
"""Shared helpers for imiv Python regression scripts."""

from __future__ import annotations

import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Mapping, Sequence


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

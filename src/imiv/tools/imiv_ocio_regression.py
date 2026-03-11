#!/usr/bin/env python3
"""Regression check for imiv OCIO auto colorspace resolution."""

from __future__ import annotations

import argparse
import hashlib
import os
import shlex
import subprocess
import sys
from pathlib import Path


ERROR_PATTERNS = (
    "VUID-",
    "fatal Vulkan error",
    "OCIO runtime shader preflight failed",
    "vkCreateShaderModule failed",
    "error: imiv exited with code",
)


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


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
        repo_root / "build_u" / "bin" / "oiiotool",
        repo_root / "build" / "bin" / "oiiotool",
        Path("/mnt/f/UBc/Release/bin/oiiotool"),
        Path("/mnt/f/UBc/Debug/bin/oiiotool"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return Path("oiiotool")


def _default_iinfo(repo_root: Path) -> Path:
    candidates = [
        repo_root / "build_u" / "bin" / "iinfo",
        repo_root / "build" / "bin" / "iinfo",
        Path("/mnt/f/UBc/Release/bin/iinfo"),
        Path("/mnt/f/UBc/Debug/bin/iinfo"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return Path("iinfo")


def _load_env_from_script(script_path: Path) -> dict[str, str]:
    env = dict(os.environ)
    if not script_path.exists():
        return env

    quoted = shlex.quote(str(script_path))
    proc = subprocess.run(
        ["bash", "-lc", f"source {quoted} >/dev/null 2>&1; env -0"],
        check=True,
        stdout=subprocess.PIPE,
    )
    loaded = {}
    for item in proc.stdout.split(b"\0"):
        if not item:
            continue
        key, _, value = item.partition(b"=")
        if not key:
            continue
        loaded[key.decode("utf-8", errors="ignore")] = value.decode(
            "utf-8", errors="ignore"
        )
    env.update(loaded)
    return env


def _generate_probe_image(oiiotool: Path, image_path: Path) -> None:
    image_path.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(oiiotool),
        "--create",
        "96x48",
        "3",
        "--d",
        "float",
        "--fill:color=0.02,0.18,0.72",
        "0,0,96,48",
        "--attrib",
        "oiio:ColorSpace",
        "srgb_texture",
        "-o",
        str(image_path),
    ]
    subprocess.run(cmd, check=True)


def _detect_metadata_colorspace(iinfo: Path, image_path: Path) -> str:
    proc = subprocess.run(
        [str(iinfo), "-v", str(image_path)],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    for line in proc.stdout.splitlines():
        if "oiio:ColorSpace:" not in line:
            continue
        _, _, tail = line.partition(":")
        _, _, value = tail.partition(":")
        value = value.strip().strip('"')
        if value:
            return value
    raise RuntimeError("failed to detect oiio:ColorSpace from iinfo output")


def _run_case(
    exe: Path,
    cwd: Path,
    env: dict[str, str],
    out_dir: Path,
    image_path: Path,
    name: str,
    extra_args: list[str],
) -> tuple[str, Path, Path]:
    screenshot_path = out_dir / f"{name}.png"
    log_path = out_dir / f"{name}.log"
    cmd = [str(exe), "-F", *extra_args, str(image_path)]

    run_env = dict(env)
    run_env.update(
        {
            "OCIO": run_env.get("OCIO", "ocio://default"),
            "IMIV_IMGUI_TEST_ENGINE": "1",
            "IMIV_IMGUI_TEST_ENGINE_EXIT_ON_FINISH": "1",
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT": "1",
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_OUT": str(screenshot_path),
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_DELAY_FRAMES": "5",
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_FRAMES": "1",
        }
    )

    with log_path.open("w", encoding="utf-8") as log_handle:
        proc = subprocess.run(
            cmd,
            cwd=str(cwd),
            env=run_env,
            check=False,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            timeout=45,
        )
    if proc.returncode != 0:
        raise RuntimeError(f"{name}: imiv exited with code {proc.returncode}")
    if not screenshot_path.exists():
        raise RuntimeError(f"{name}: screenshot not written")

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    for pattern in ERROR_PATTERNS:
        if pattern in log_text:
            raise RuntimeError(f"{name}: found runtime error pattern: {pattern}")

    return _sha256(screenshot_path), screenshot_path, log_path


def _fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    default_out = repo_root / "build_u" / "imiv_captures" / "ocio_auto_regression"
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_image = default_out / "ocio_auto_input.exr"

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=str(_default_binary(repo_root)), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv")
    ap.add_argument("--oiiotool", default=str(_default_oiiotool(repo_root)), help="oiiotool executable")
    ap.add_argument("--iinfo", default=str(_default_iinfo(repo_root)), help="iinfo executable")
    ap.add_argument("--env-script", default=str(default_env_script), help="Optional shell env script")
    ap.add_argument("--out-dir", default=str(default_out), help="Artifact directory")
    ap.add_argument("--image", default=str(default_image), help="Generated input image path")
    args = ap.parse_args()

    exe = Path(args.bin).resolve()
    if not exe.exists():
        return _fail(f"binary not found: {exe}")

    oiiotool = Path(args.oiiotool)
    iinfo = Path(args.iinfo)
    run_cwd = Path(args.cwd).resolve() if args.cwd else exe.parent.resolve()
    out_dir = Path(args.out_dir).resolve()
    image_path = Path(args.image).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    env = _load_env_from_script(Path(args.env_script).resolve())

    try:
        _generate_probe_image(oiiotool, image_path)
        metadata_color_space = _detect_metadata_colorspace(iinfo, image_path)

        auto_hash, auto_png, auto_log = _run_case(
            exe,
            run_cwd,
            env,
            out_dir,
            image_path,
            "auto",
            ["--image-color-space", "auto"],
        )
        explicit_hash, explicit_png, explicit_log = _run_case(
            exe,
            run_cwd,
            env,
            out_dir,
            image_path,
            "explicit_metadata",
            ["--image-color-space", metadata_color_space],
        )
        scene_linear_hash, scene_linear_png, scene_linear_log = _run_case(
            exe,
            run_cwd,
            env,
            out_dir,
            image_path,
            "forced_scene_linear",
            ["--image-color-space", "scene_linear"],
        )
    except (OSError, subprocess.CalledProcessError, RuntimeError, subprocess.TimeoutExpired) as exc:
        return _fail(str(exc))

    if auto_hash != explicit_hash:
        return _fail(
            "auto colorspace does not match explicit metadata colorspace: "
            f"auto={auto_hash} explicit={explicit_hash}"
        )
    if auto_hash == scene_linear_hash:
        return _fail(
            "auto colorspace unexpectedly matches forced scene_linear output: "
            f"auto={auto_hash}"
        )

    print("metadata colorspace:", metadata_color_space)
    print("auto hash:", auto_hash)
    print("explicit metadata hash:", explicit_hash)
    print("forced scene_linear hash:", scene_linear_hash)
    print("auto screenshot:", auto_png)
    print("explicit screenshot:", explicit_png)
    print("scene_linear screenshot:", scene_linear_png)
    print("auto log:", auto_log)
    print("explicit log:", explicit_log)
    print("scene_linear log:", scene_linear_log)
    print("artifacts:", out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

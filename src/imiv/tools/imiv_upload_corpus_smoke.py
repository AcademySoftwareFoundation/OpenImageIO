#!/usr/bin/env python3
"""Batched smoke test for the imiv upload corpus."""

from __future__ import annotations

import argparse
import csv
import json
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

from imiv_test_utils import (
    default_binary,
    fail,
    load_env_from_script,
    path_for_imiv_output,
    repo_root as imiv_repo_root,
    resolve_run_cwd,
    runner_path,
)


def _parse_duration_seconds(text: str) -> int:
    value = text.strip().lower()
    if not value:
        raise ValueError("empty duration")
    if value[-1].isdigit():
        return max(1, int(value))

    scale_map = {"s": 1, "m": 60, "h": 3600}
    scale = scale_map.get(value[-1])
    if scale is None:
        raise ValueError(f"unsupported duration suffix: {text}")
    return max(1, int(value[:-1]) * scale)


def _chunks(items: list[Path], chunk_size: int) -> list[list[Path]]:
    return [items[i : i + chunk_size] for i in range(0, len(items), chunk_size)]


def _step_name(index: int, image: Path) -> str:
    stem = image.stem
    safe = "".join(c if c.isalnum() or c in "._-" else "_" for c in stem)
    if not safe:
        safe = "image"
    return f"{index:03d}_{safe}"


@dataclass(frozen=True)
class BatchImage:
    index: int
    path: Path
    step_name: str
    screenshot_out: Path
    state_out: Path


def _write_scenario(
    path: Path,
    *,
    runtime_dir_rel: str,
    images: list[BatchImage],
    post_action_delay_frames: int,
) -> None:
    root = ET.Element("imiv-scenario")
    root.set("out_dir", runtime_dir_rel)

    for image in images:
        step = ET.SubElement(root, "step")
        step.set("name", image.step_name)
        step.set("image_list_select_index", str(image.index))
        step.set("state", "true")
        step.set("screenshot", "true")
        step.set("post_action_delay_frames", str(max(0, post_action_delay_frames)))
        if image.index == 0:
            step.set("image_list_visible", "true")

    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _copy_if_exists(src: Path, dst: Path) -> None:
    if not src.exists():
        return
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dst)


def _validation_reason(
    *,
    state_path: Path,
    expected_image: Path,
    screenshot_path: Path,
    batch_reason: str | None,
) -> tuple[str, str]:
    if batch_reason is not None:
        return "FAIL", batch_reason
    if not screenshot_path.exists():
        return "FAIL", "no_screenshot_saved"
    if not state_path.exists():
        return "FAIL", "no_state_saved"

    try:
        state = json.loads(state_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return "FAIL", "invalid_state_json"
    if not bool(state.get("image_loaded", False)):
        return "FAIL", "image_not_loaded"

    actual_path_value = str(state.get("image_path", "")).strip()
    if not actual_path_value:
        return "FAIL", "missing_image_path"
    actual_path = Path(actual_path_value).expanduser()
    if actual_path.exists():
        try:
            actual_path = actual_path.resolve()
        except OSError:
            pass
    try:
        expected_resolved = expected_image.resolve()
    except OSError:
        expected_resolved = expected_image

    if actual_path != expected_resolved:
        return "FAIL", "wrong_loaded_image"

    return "PASS", "ok"


def _run_batch(
    *,
    repo_root: Path,
    runner: Path,
    exe: Path,
    corpus_images: list[Path],
    batch_index: int,
    result_dir: Path,
    env: dict[str, str],
    run_cwd: Path,
    trace: bool,
    timeout_seconds: int,
    post_action_delay_frames: int,
) -> tuple[list[tuple[Path, str, str, Path, Path]], int]:
    batch_id = f"batch_{batch_index:03d}"
    runtime_dir = result_dir / "runtime" / batch_id
    logs_dir = result_dir / "logs"
    screenshots_dir = result_dir / "screenshots"
    states_dir = result_dir / "states"
    scenarios_dir = result_dir / "scenarios"
    log_path = logs_dir / f"{batch_id}.log"
    scenario_path = scenarios_dir / f"{batch_id}.scenario.xml"

    runtime_dir.mkdir(parents=True, exist_ok=True)
    logs_dir.mkdir(parents=True, exist_ok=True)
    screenshots_dir.mkdir(parents=True, exist_ok=True)
    states_dir.mkdir(parents=True, exist_ok=True)

    batch_images = [
        BatchImage(
            index=i,
            path=image,
            step_name=_step_name(i, image),
            screenshot_out=screenshots_dir / f"{image.stem}.png",
            state_out=states_dir / f"{image.stem}.state.json",
        )
        for i, image in enumerate(corpus_images)
    ]

    _write_scenario(
        scenario_path,
        runtime_dir_rel=path_for_imiv_output(runtime_dir, run_cwd),
        images=batch_images,
        post_action_delay_frames=post_action_delay_frames,
    )

    cmd = [
        sys.executable,
        str(runner),
        "--bin",
        str(exe),
        "--cwd",
        str(run_cwd),
        "--scenario",
        str(scenario_path),
    ]
    for image in corpus_images:
        cmd.extend(["--open", str(image)])
    if trace:
        cmd.append("--trace")

    print(f"batch {batch_index + 1}: {len(corpus_images)} images")
    print("run:", " ".join(cmd))

    proc_stdout = ""
    return_code = 0
    batch_reason: str | None = None
    try:
        proc = subprocess.run(
            cmd,
            cwd=str(repo_root),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=timeout_seconds,
        )
        proc_stdout = proc.stdout or ""
        return_code = proc.returncode
        if return_code != 0:
            batch_reason = f"runner_exit_{return_code}"
    except subprocess.TimeoutExpired as exc:
        proc_stdout = (exc.stdout or "") if isinstance(exc.stdout, str) else ""
        return_code = 124
        batch_reason = f"timeout_{timeout_seconds}s"

    log_path.write_text(proc_stdout, encoding="utf-8")

    if batch_reason is None:
        for pattern in ("upload failed", "vk[error][validation]", "VUID-"):
            if pattern in proc_stdout:
                batch_reason = "validation_or_upload_error"
                break

    results: list[tuple[Path, str, str, Path, Path]] = []
    for batch_image in batch_images:
        step_base = runtime_dir / batch_image.step_name
        screenshot_path = step_base.with_suffix(".png")
        state_path = step_base.with_suffix(".state.json")
        _copy_if_exists(screenshot_path, batch_image.screenshot_out)
        _copy_if_exists(state_path, batch_image.state_out)
        result, reason = _validation_reason(
            state_path=state_path,
            expected_image=batch_image.path,
            screenshot_path=screenshot_path,
            batch_reason=batch_reason,
        )
        results.append(
            (
                batch_image.path,
                result,
                reason,
                log_path,
                batch_image.screenshot_out,
            )
        )

    return results, return_code


def main() -> int:
    repo_root = imiv_repo_root()
    default_corpus_dir = repo_root / "build_u" / "testsuite" / "imiv" / "upload_corpus" / "images"
    default_result_dir = repo_root / "build_u" / "testsuite" / "imiv" / "upload_corpus" / "results"
    default_runner = runner_path(repo_root)
    default_env_script = repo_root / "build_u" / "imiv_env.sh"
    default_bin = default_binary(repo_root)

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--corpus-dir", default=str(default_corpus_dir), help="Corpus image directory")
    ap.add_argument("--result-dir", default=str(default_result_dir), help="Output directory")
    ap.add_argument("--runner", default=str(default_runner), help="imiv runner script")
    ap.add_argument("--bin", default=str(default_bin), help="imiv executable")
    ap.add_argument("--cwd", default="", help="Working directory for imiv (default: binary dir)")
    ap.add_argument("--env-script", default=str(default_env_script), help="Optional shell env setup script")
    ap.add_argument(
        "--per-case-timeout",
        default="45s",
        help="Baseline timeout per image; used to scale batch timeout",
    )
    ap.add_argument(
        "--batch-size",
        type=int,
        default=32,
        help="Maximum number of images to verify in one imiv launch",
    )
    ap.add_argument(
        "--timeout-slop-seconds",
        type=int,
        default=15,
        help="Extra seconds added per image when scaling batch timeout",
    )
    ap.add_argument(
        "--post-action-delay-frames",
        type=int,
        default=3,
        help="Frames to wait after changing the active image before capture",
    )
    ap.add_argument("--trace", action="store_true", help="Enable verbose runner trace")
    args = ap.parse_args()

    corpus_dir = Path(args.corpus_dir).expanduser().resolve()
    result_dir = Path(args.result_dir).expanduser().resolve()
    runner = Path(args.runner).expanduser().resolve()
    exe = Path(args.bin).expanduser().resolve()
    env_script = Path(args.env_script).expanduser().resolve(strict=False)

    if not corpus_dir.is_dir():
        return fail(f"corpus directory not found: {corpus_dir}")
    if not runner.exists():
        return fail(f"runner script not found: {runner}")
    if not exe.exists():
        return fail(f"imiv executable not found: {exe}")
    if args.batch_size <= 0:
        return fail("batch-size must be greater than zero")
    if args.timeout_slop_seconds < 0:
        return fail("timeout-slop-seconds must be non-negative")

    images = sorted(
        path
        for path in corpus_dir.iterdir()
        if path.is_file() and path.suffix.lower() in {".tif", ".tiff", ".exr"}
    )
    if not images:
        return fail(f"no corpus images found in {corpus_dir}")

    try:
        per_case_timeout_seconds = _parse_duration_seconds(args.per_case_timeout)
    except ValueError as exc:
        return fail(str(exc))

    shutil.rmtree(result_dir, ignore_errors=True)
    result_dir.mkdir(parents=True, exist_ok=True)

    summary_csv = result_dir / "summary.csv"
    with summary_csv.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["image", "result", "reason", "log", "screenshot"])

    env = load_env_from_script(env_script if env_script.exists() else None)
    run_cwd = resolve_run_cwd(exe, args.cwd)

    pass_count = 0
    fail_count = 0

    batches = _chunks(images, args.batch_size)
    for batch_index, batch_images in enumerate(batches):
        batch_env = dict(env)
        batch_config_home = result_dir / "cfg" / f"batch_{batch_index:03d}"
        batch_config_home.mkdir(parents=True, exist_ok=True)
        batch_env["IMIV_CONFIG_HOME"] = str(batch_config_home)
        batch_timeout_seconds = (
            per_case_timeout_seconds + args.timeout_slop_seconds * len(batch_images)
        )

        batch_results, _ = _run_batch(
            repo_root=repo_root,
            runner=runner,
            exe=exe,
            corpus_images=batch_images,
            batch_index=batch_index,
            result_dir=result_dir,
            env=batch_env,
            run_cwd=run_cwd,
            trace=args.trace,
            timeout_seconds=batch_timeout_seconds,
            post_action_delay_frames=args.post_action_delay_frames,
        )

        with summary_csv.open("a", encoding="utf-8", newline="") as handle:
            writer = csv.writer(handle)
            for image, result, reason, log_path, screenshot_path in batch_results:
                writer.writerow(
                    [
                        str(image),
                        result,
                        reason,
                        str(log_path),
                        str(screenshot_path),
                    ]
                )
                print(f"{result}: {image.name} ({reason})")
                if result == "PASS":
                    pass_count += 1
                else:
                    fail_count += 1

    print("")
    print(f"smoke test summary: pass={pass_count} fail={fail_count} total={len(images)}")
    print(f"summary csv: {summary_csv}")

    return 0 if fail_count == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())

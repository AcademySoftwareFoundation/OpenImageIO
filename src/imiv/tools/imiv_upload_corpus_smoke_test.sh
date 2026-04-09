#!/usr/bin/env bash
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
corpus_dir="${1:-$repo_root/build_u/testsuite/imiv/upload_corpus/images}"
result_dir="${2:-$repo_root/build_u/testsuite/imiv/upload_corpus/results}"
driver_py="${DRIVER_PY:-$repo_root/src/imiv/tools/imiv_upload_corpus_smoke.py}"
runner_py="${RUNNER_PY:-$repo_root/src/imiv/tools/imiv_gui_test_run.py}"
python_bin="${PYTHON_BIN:-python3}"
env_script="${IMIV_ENV_SCRIPT:-$repo_root/build_u/imiv_env.sh}"
per_case_timeout="${PER_CASE_TIMEOUT:-45s}"
runner_trace="${RUNNER_TRACE:-0}"
screenshot_delay_frames="${SCREENSHOT_DELAY_FRAMES:-3}"
screenshot_frames="${SCREENSHOT_FRAMES:-1}"
batch_size="${BATCH_SIZE:-32}"
timeout_slop_seconds="${BATCH_TIMEOUT_SLOP_SECONDS:-15}"

if [[ ! -d "$corpus_dir" ]]; then
    echo "error: corpus directory not found: $corpus_dir" >&2
    exit 2
fi
if [[ ! -f "$driver_py" ]]; then
    echo "error: upload smoke driver script not found: $driver_py" >&2
    exit 2
fi
if [[ ! -f "$runner_py" ]]; then
    echo "error: imiv runner script not found: $runner_py" >&2
    exit 2
fi
if [[ "$screenshot_frames" != "1" ]]; then
    echo "warning: batched upload smoke uses one scenario capture per image; SCREENSHOT_FRAMES=${screenshot_frames} is ignored" >&2
fi

cmd=(
    "$python_bin" "-u" "$driver_py"
    "--corpus-dir" "$corpus_dir"
    "--result-dir" "$result_dir"
    "--runner" "$runner_py"
    "--env-script" "$env_script"
    "--per-case-timeout" "$per_case_timeout"
    "--batch-size" "$batch_size"
    "--timeout-slop-seconds" "$timeout_slop_seconds"
    "--post-action-delay-frames" "$screenshot_delay_frames"
)

if [[ "$runner_trace" == "1" ]]; then
    cmd+=("--trace")
fi

exec "${cmd[@]}"

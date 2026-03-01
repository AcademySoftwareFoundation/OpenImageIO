#!/usr/bin/env bash
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
corpus_dir="${1:-$repo_root/build_u/imiv_upload_corpus/images}"
result_dir="${2:-$repo_root/build_u/imiv_upload_corpus/results}"
runner_py="${RUNNER_PY:-$repo_root/src/imiv/tools/imiv_gui_test_run.py}"
python_bin="${PYTHON_BIN:-python3}"
env_script="${IMIV_ENV_SCRIPT:-$repo_root/build_u/imiv_env.sh}"
per_case_timeout="${PER_CASE_TIMEOUT:-45s}"
runner_trace="${RUNNER_TRACE:-0}"
screenshot_delay_frames="${SCREENSHOT_DELAY_FRAMES:-3}"
screenshot_frames="${SCREENSHOT_FRAMES:-1}"

if [[ ! -d "$corpus_dir" ]]; then
    echo "error: corpus directory not found: $corpus_dir" >&2
    exit 2
fi
if [[ ! -f "$runner_py" ]]; then
    echo "error: imiv runner script not found: $runner_py" >&2
    exit 2
fi

mkdir -p "$result_dir/screenshots"
mkdir -p "$result_dir/logs"

summary_csv="$result_dir/summary.csv"
cat > "$summary_csv" <<'EOF'
image,result,reason,log,screenshot
EOF

mapfile -t images < <(find "$corpus_dir" -maxdepth 1 -type f \
    \( -name '*.tif' -o -name '*.tiff' -o -name '*.exr' \) | sort)

if [[ ${#images[@]} -eq 0 ]]; then
    echo "error: no corpus images found in $corpus_dir" >&2
    exit 2
fi

pass_count=0
fail_count=0

for image in "${images[@]}"; do
    base_name="$(basename "$image")"
    stem="${base_name%.*}"
    shot_path="$result_dir/screenshots/${stem}.png"
    log_path="$result_dir/logs/${stem}.log"

    cmd=(
        "$python_bin" "-u" "$runner_py"
        "--open" "$image"
        "--screenshot-out" "$shot_path"
        "--screenshot-delay-frames" "$screenshot_delay_frames"
        "--screenshot-frames" "$screenshot_frames"
    )
    if [[ "$runner_trace" == "1" ]]; then
        cmd+=("--trace")
    fi

    if [[ -f "$env_script" ]]; then
        quoted_cmd=""
        for arg in "${cmd[@]}"; do
            printf -v qarg '%q' "$arg"
            quoted_cmd+="${qarg} "
        done
        run_cmd=(bash -lc "source \"$env_script\"; ${quoted_cmd}")
    else
        run_cmd=("${cmd[@]}")
    fi

    set +e
    timeout "$per_case_timeout" "${run_cmd[@]}" > "$log_path" 2>&1
    rc=$?
    set -e

    reason="ok"
    result="PASS"

    if [[ $rc -eq 124 ]]; then
        result="FAIL"
        reason="timeout_${per_case_timeout}"
    elif [[ $rc -ne 0 ]]; then
        result="FAIL"
        reason="runner_exit_${rc}"
    elif ! grep -q "Saved '" "$log_path"; then
        result="FAIL"
        reason="no_screenshot_saved"
    elif grep -Eq "upload failed|vk\\[error\\]\\[validation\\]|VUID-" "$log_path"; then
        result="FAIL"
        reason="validation_or_upload_error"
    fi

    if [[ "$result" == "PASS" ]]; then
        pass_count=$((pass_count + 1))
    else
        fail_count=$((fail_count + 1))
    fi

    printf '%s,%s,%s,%s,%s\n' \
        "$image" "$result" "$reason" "$log_path" "$shot_path" >> "$summary_csv"
    echo "${result}: ${base_name} (${reason})"
done

echo
echo "smoke test summary: pass=${pass_count} fail=${fail_count} total=${#images[@]}"
echo "summary csv: $summary_csv"

if [[ $fail_count -ne 0 ]]; then
    exit 1
fi

#!/usr/bin/env bash
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
build_dir="${1:-$repo_root/build_u}"

corpus_root="${build_dir}/imiv_upload_corpus"
images_dir="${corpus_root}/images"
manifest_csv="${corpus_root}/corpus_manifest.csv"
results_dir="${corpus_root}/results"

generate_script="${repo_root}/src/imiv/tools/imiv_generate_upload_corpus.sh"
smoke_script="${repo_root}/src/imiv/tools/imiv_upload_corpus_smoke_test.sh"

if [[ -z "${IMIV_ENV_SCRIPT:-}" ]]; then
    export IMIV_ENV_SCRIPT="${build_dir}/imiv_env.sh"
fi

export PER_CASE_TIMEOUT="${PER_CASE_TIMEOUT:-90s}"
export RUNNER_TRACE="${RUNNER_TRACE:-0}"

"${generate_script}" "${images_dir}" "${manifest_csv}"
"${smoke_script}" "${images_dir}" "${results_dir}"

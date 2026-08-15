#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
python_bin="${PYTHON:-python3}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --python)
            python_bin="$2"
            shift 2
            ;;
        *)
            break
            ;;
    esac
done

exec "${python_bin}" "${repo_root}/src/imiv/tools/imiv_backend_verify.py" "$@"

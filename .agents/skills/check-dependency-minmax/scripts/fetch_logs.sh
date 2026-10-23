#!/bin/bash
# Download every job log of a GitHub Actions run, in parallel.
# Usage: fetch_logs.sh <run-id-or-url> <outdir> [--repo owner/name]
# Logs land in <outdir>/logs/<jobid>.log. Needs `gh` (run outside the sandbox).
set -u
run="${1:?run id or url}"; out="${2:?outdir}"; shift 2
repo_args=("$@")
run="${run##*/runs/}"; run="${run%%/*}"   # accept a full URL
mkdir -p "$out/logs"
ids=$(gh run view "$run" "${repo_args[@]}" --json jobs --jq '.jobs[].databaseId') || exit 1
gh run view "$run" "${repo_args[@]}" --json jobs \
    --jq '.jobs[] | "\(.databaseId)\t\(.conclusion)\t\(.name)"' > "$out/jobs.tsv"
for id in $ids; do
    gh run view "${repo_args[@]}" --job "$id" --log > "$out/logs/$id.log" 2> "$out/logs/$id.err" &
done
wait
# Parallel fetches race on gh's zip cache ("not a valid zip file"); retry
# any empty log serially.
for id in $ids; do
    for try in 1 2 3; do
        [ -s "$out/logs/$id.log" ] && break
        gh run view "${repo_args[@]}" --job "$id" --log > "$out/logs/$id.log" 2> "$out/logs/$id.err"
    done
done
echo "fetched $(ls "$out"/logs/*.log | wc -l) logs into $out/logs (job list: $out/jobs.tsv)"

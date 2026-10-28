#!/bin/bash
# Download every job log of a GitHub Actions run, in parallel.
# Usage: fetch_logs.sh <run-id-or-url> <outdir> [--repo owner/name]
# Logs land in <outdir>/logs/<jobid>.log. Needs `gh` (run outside the sandbox).
set -u
run="${1:?run id or url}"; out="${2:?outdir}"; shift 2
# The remaining args ("$@") are passed to gh. Not copied to an array: an
# empty array under `set -u` is an error in macOS's bash 3.2.
run="${run##*/runs/}"; run="${run%%/*}"   # accept a full URL
mkdir -p "$out/logs"
ids=$(gh run view "$run" "$@" --json jobs --jq '.jobs[].databaseId') || exit 1
gh run view "$run" "$@" --json jobs \
    --jq '.jobs[] | "\(.databaseId)\t\(.conclusion)\t\(.name)"' > "$out/jobs.tsv"
for id in $ids; do
    gh run view "$@" --job "$id" --log > "$out/logs/$id.log" 2> "$out/logs/$id.err" &
done
wait
# Parallel fetches race on gh's zip cache ("not a valid zip file"); retry
# any empty log serially.
for id in $ids; do
    for try in 1 2 3; do
        [ -s "$out/logs/$id.log" ] && break
        gh run view "$@" --job "$id" --log > "$out/logs/$id.log" 2> "$out/logs/$id.err"
    done
done
echo "fetched $(ls "$out"/logs/*.log | wc -l) logs into $out/logs (job list: $out/jobs.tsv)"

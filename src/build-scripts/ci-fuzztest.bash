#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Fuzz testing step for CI: optionally lints the fuzz corpus directory
# coverage, then (if OIIO_FUZZ_FORMAT is set) seeds, runs, and summarizes a
# libFuzzer session for that format. Controlled by:
#   OIIO_FUZZ_CORPUS_LINT   "true" to run the corpus coverage lint
#   OIIO_FUZZ_FORMAT        format name to fuzz (empty = skip fuzz run)
#   OIIO_FUZZ_MAX_TIME      seconds for -max_total_time (default 3600)

set -e

FUZZ_BIN="$OpenImageIO_ROOT/bin/oiio_fuzz_image"

#
# Verify every format reported by `oiio_fuzz_image --list-formats` has a
# corpus directory in src/fuzz/corpora/.
#
if [[ "${OIIO_FUZZ_CORPUS_LINT}" == "true" ]]; then
    "$FUZZ_BIN" --list-formats | sort > /tmp/formats.txt
    ls src/fuzz/corpora/ | sort > /tmp/corpus_dirs.txt
    missing=$(comm -23 /tmp/formats.txt /tmp/corpus_dirs.txt)
    if [[ -n "$missing" ]]; then
        echo "ERROR: Missing corpus director(ies) for compiled-in format(s):"
        echo "$missing" | sed 's/^/  src\/fuzz\/corpora\//'
        echo ""
        echo "Add src/fuzz/corpora/<format>/ with at least a .gitkeep for each."
        exit 1
    fi
    count=$(wc -l < /tmp/formats.txt | tr -d ' ')
    echo "OK: all ${count} compiled-in formats have corpus directories."
fi

#
# Seed the corpus, run the fuzzer, and write a job summary for one format.
#
if [[ -n "${OIIO_FUZZ_FORMAT}" ]]; then
    corpus_dir="corpus/${OIIO_FUZZ_FORMAT}"
    mkdir -p "$corpus_dir"
    python3 src/fuzz/populate_corpora.py --format "$OIIO_FUZZ_FORMAT" --dest corpus
    cp -rn "src/fuzz/corpora/${OIIO_FUZZ_FORMAT}/"* "$corpus_dir/" 2>/dev/null || true

    if [[ ! -x "$FUZZ_BIN" ]]; then
        echo "::error::$FUZZ_BIN not found — was OIIO_BUILD_FUZZ_TARGETS=ON passed to cmake?"
        exit 1
    fi

    skipped=0
    fuzz_status=0
    if ! ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0" \
         "$FUZZ_BIN" --list-formats | grep -qx "$OIIO_FUZZ_FORMAT"; then
        echo "::notice::Format '${OIIO_FUZZ_FORMAT}' not compiled in this build; skipping fuzz run."
        skipped=1
    else
        set +e
        "$FUZZ_BIN" "$corpus_dir" \
            -max_total_time="${OIIO_FUZZ_MAX_TIME:-3600}" \
            -max_len=16777216 \
            -rss_limit_mb=4096 \
            -malloc_limit_mb=2048 \
            -timeout=60 \
            -detect_leaks=0 \
            -artifact_prefix="crash_${OIIO_FUZZ_FORMAT}_" \
            -jobs=$(nproc) -workers=$(nproc)
        fuzz_status=$?
        set -e
    fi

    crash_count=$(find . -maxdepth 1 -name "crash_${OIIO_FUZZ_FORMAT}_*" | wc -l | tr -d ' ')
    if [[ "$fuzz_status" -ne 0 ]]; then
        {
            echo "Format: ${OIIO_FUZZ_FORMAT}"
            echo "Status: FAILED"
            echo "Detail: fuzzer ran and found a problem (crash/timeout/OOM); see uploaded crash artifacts"
            echo "Crash artifacts found: ${crash_count}"
            echo ""
        } >> "$GITHUB_STEP_SUMMARY"
    elif [[ "$skipped" -eq 1 ]]; then
        {
            echo "Format: ${OIIO_FUZZ_FORMAT}"
            echo "Status: SKIPPED"
            echo "Detail: fuzzing was not run because this format is not compiled/implemented in this build"
            echo ""
        } >> "$GITHUB_STEP_SUMMARY"
    fi

    exit "$fuzz_status"
fi

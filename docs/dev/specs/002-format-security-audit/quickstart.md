# Quickstart: Executing One Format Audit Phase

How to run a single format's hardening phase end-to-end. Repeat per format.

## Prerequisites

- Build works: `make` (Release) and `make debug`.
- Sanitizer build available for verification (ASan+UBSan).
- Fuzz targets buildable: configure with `-DOIIO_BUILD_FUZZ_TARGETS=ON`.

## 1. Branch off per format

```bash
git checkout main && git pull
git checkout -b fix/<format>-hardening   # one branch/PR per format
```

## 2. Census the reader

Read `src/<format>.imageio/<format>input.cpp`. List: dimension/offset fields,
underlying-library calls, `ptr,len` hand-offs, metadata-blob parsing. (See
`contracts/per-format-audit.md` step 1.)

## 3. Apply the checklist

Use `contracts/reader-hardening-api.md` for exact signatures:

- Add `check_open(spec, ROI)` near the top of `open()`.
- Add `check_compression_ratio(spec, filesize)` before decode/allocation.
- Confirm/repair `valid_file()`.
- Promote size/offset math to `imagesize_t`/`int64_t`; validate factors first.
- Check every library return; `errorfmt()`+`false` on failure, free resources.
- Convert `ptr,len` → `span`/`cspan`; `valid_raw_span_size()` for decode targets.
- Wire `imageinput:strict` for metadata; hard-error pixel corruption.

## 4. Add malformed-input coverage

Create fixtures/seeds per exploitable hazard class (H1–H9, data-model.md):

```bash
# regression fixtures
testsuite/<format>/ref/ ...            # per TESTSUITE-README.md
# fuzz seeds
src/fuzz/corpora/<format>/ ...         # small, representative + the adversarial repro
```

Add a minimal reproducer for each fixed defect (constitution Principle V).

## 5. Verify

```bash
# functional + sanitizer
make clean && make debug
ctest --test-dir build -R <format> --output-on-failure

# sanitizer build (ASan+UBSan) — must be clean on the new corpus
cmake -B build-asan -S . -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
      -DOIIO_BUILD_FUZZ_TARGETS=ON
cmake --build build-asan
# run oiio_fuzz_image against the format seed corpus for a smoke duration

# valid-file no-regression: outputs byte-identical to pre-change
make clang-format
```

Confirm: no crash/hang/OOB/sanitizer report on malformed inputs; every valid file
still reads identically; oversized/bomb inputs rejected with clear errors.

## 6. Document + open PR

- PR description: C1–C10 outcome table + hazard-class corpus additions (FR-011).
- Update `data-model.md` inventory `status` → audited.
- Log any deferred writer/other findings in `docs/dev/format-hardening-deferrals.md`.
- Add a `CHANGES.md` entry.
- Title: `fix(<format>): harden reader against malformed/adversarial input`.

## Done when

All boxes in `contracts/per-format-audit.md` "Acceptance criteria" are checked. The
overall effort is done when no format in the inventory remains `pending` (SC-007).

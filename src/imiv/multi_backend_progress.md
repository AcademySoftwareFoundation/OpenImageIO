# imiv Multi-Backend Progress

Last updated: 2026-03-19

## Current status

Work already landed in the tree:

- Multi-backend design note:
  - `src/imiv/multi_backend.md`
- Build-time backend capabilities:
  - `src/imiv/imiv_build_config.h.in`
- Runtime backend metadata and selection:
  - `src/imiv/imiv_backend.h`
  - `src/imiv/imiv_renderer.cpp`
- Renderer seam moved to runtime backend dispatch:
  - `src/imiv/imiv_renderer.h`
  - `src/imiv/imiv_renderer.cpp`
  - `src/imiv/imiv_renderer_backend.h`
  - `src/imiv/imiv_renderer_vulkan.cpp`
  - `src/imiv/imiv_renderer_opengl.cpp`
  - `src/imiv/imiv_renderer_metal.mm`
- GLFW platform init now selects backend at runtime:
  - `src/imiv/imiv_platform_glfw.h`
  - `src/imiv/imiv_platform_glfw.cpp`
- App startup honors:
  - CLI `--backend`
  - persisted `renderer_backend`
  - runtime fallback if requested backend is not compiled
- `imiv --list-backends` reports compiled/default backends.

## This session's additional work

Backend override plumbing for tests is implemented, and initial runtime
validation is complete on Linux/WSL.

Changed in this slice:

- `src/imiv/tools/imiv_gui_test_run.py`
  - added `--backend`
  - passes `--backend ...` through to `imiv`
- `src/imiv/tools/imiv_backend_verify.py`
  - now passes backend override through all smoke / UX / OCIO launches
- Regression scripts updated to accept `--backend`:
  - `src/imiv/tools/imiv_ux_actions_regression.py`
  - `src/imiv/tools/imiv_ocio_missing_fallback_regression.py`
  - `src/imiv/tools/imiv_ocio_config_source_regression.py`
  - `src/imiv/tools/imiv_ocio_live_update_regression.py`
  - `src/imiv/tools/imiv_opengl_smoke_regression.py`
  - `src/imiv/tools/imiv_metal_smoke_regression.py`
  - `src/imiv/tools/imiv_metal_screenshot_regression.py`
  - `src/imiv/tools/imiv_metal_sampling_regression.py`
  - `src/imiv/tools/imiv_metal_orientation_regression.py`
  - `src/imiv/tools/imiv_developer_menu_regression.py`
  - `src/imiv/tools/imiv_area_probe_closeup_regression.py`
  - `src/imiv/tools/imiv_auto_subimage_regression.py`
- `src/imiv/CMakeLists.txt`
  - added `OIIO_IMIV_ADD_BACKEND_VERIFY_CTEST` option
  - default-backend GUI tests now pass explicit `--backend`
  - added optional per-backend shared verification `ctest` entries:
    - `imiv_backend_verify_vulkan`
    - `imiv_backend_verify_opengl`
    - `imiv_backend_verify_metal`
- Backend state is now present in viewer-state JSON:
  - `src/imiv/imiv_frame.h`
  - `src/imiv/imiv_frame.cpp`
  - includes:
    - `active`
    - `requested`
    - `next_launch`
    - `restart_required`
    - compiled/unavailable backend lists
- Added focused backend-selector regression:
  - `src/imiv/tools/imiv_backend_preferences_regression.py`
  - `src/imiv/CMakeLists.txt`
  - verifies:
    - selecting an alternate backend changes `requested` / `next_launch`
    - restart-required semantics
    - selecting active backend clears restart-required
    - selecting `Auto` resolves back to the build default
- Preferences UI now exposes renderer selection:
  - `src/imiv/imiv_aux_windows.cpp`
  - `src/imiv/imiv_ui.h`
  - `src/imiv/imiv_frame.cpp`
  - dropdown: `Auto / compiled backends`
  - shows current backend
  - shows restart-required warning only when next-launch backend would differ
  - shows `not built` for unavailable backends

## Validation done

Passed:

- Python syntax check:
  - `python3 -m py_compile ...`
  - covered all touched Python scripts in this slice
- Reconfigure + rebuild:
  - `cmake -S . -B build_u -D OIIO_IMIV_ENABLE_VULKAN=AUTO -D OIIO_IMIV_ENABLE_OPENGL=AUTO -D OIIO_IMIV_ENABLE_METAL=OFF -D OIIO_IMIV_DEFAULT_RENDERER=vulkan`
  - `ninja -C build_u imiv oiiotool idiff`
- Backend listing:
  - `build_u/bin/imiv --list-backends`
  - current build reports Vulkan + OpenGL compiled
- Explicit Vulkan launch through the new backend override path:
  - `imiv_gui_test_run.py --backend vulkan ...`
- Explicit OpenGL smoke through the new backend override path:
  - `imiv_opengl_smoke_regression.py --backend opengl ...`
- Default-backend UX regression after the Preferences backend UI change:
  - `ctest --test-dir build_u -V -R '^imiv_ux_actions_regression$'`
- Backend selector regression:
  - `ctest --test-dir build_u -V -R '^imiv_backend_preferences_regression$'`
- Optional backend-wide `ctest` registration path:
  - `cmake -S . -B build_u ... -D OIIO_IMIV_ADD_BACKEND_VERIFY_CTEST=ON`
  - `ctest --test-dir build_u -N | rg 'imiv_backend_verify|imiv_backend_preferences'`
- Backend-wide non-default runtime verification from one build:
  - `ctest --test-dir build_u -V -R '^imiv_backend_verify_opengl$'`

Still not validated in this build tree:

- macOS Metal build with the new Preferences backend selector
- Windows multi-backend runtime with the explicit backend test path

## Next commands after WSL restart

From repo root:

```bash
cmake -S . -B build_u \
  -D OIIO_IMIV_ENABLE_VULKAN=AUTO \
  -D OIIO_IMIV_ENABLE_OPENGL=AUTO \
  -D OIIO_IMIV_ENABLE_METAL=OFF \
  -D OIIO_IMIV_DEFAULT_RENDERER=vulkan
```

```bash
ninja -C build_u imiv oiiotool idiff
```

Quick explicit backend smoke:

```bash
python3 src/imiv/tools/imiv_gui_test_run.py \
  --bin build_u/bin/imiv \
  --cwd build_u/bin \
  --backend vulkan \
  --open ASWF/logos/openimageio-stacked-gradient.png \
  --state-json-out build_u/imiv_captures/backend_probe_vulkan/state.json
```

```bash
python3 src/imiv/tools/imiv_opengl_smoke_regression.py \
  --bin build_u/bin/imiv \
  --cwd build_u/bin \
  --backend opengl \
  --env-script build_u/imiv_env.sh \
  --out-dir build_u/imiv_captures/backend_probe_opengl
```

Shared verifier from one multi-backend build:

```bash
python3 src/imiv/tools/imiv_backend_verify.py \
  --backend vulkan \
  --build-dir build_u \
  --out-dir build_u/imiv_captures/verify_vulkan \
  --skip-configure \
  --skip-build
```

```bash
python3 src/imiv/tools/imiv_backend_verify.py \
  --backend opengl \
  --build-dir build_u \
  --out-dir build_u/imiv_captures/verify_opengl \
  --skip-configure \
  --skip-build
```

Optional: turn on the shared backend-wide `ctest` entries and list them:

```bash
cmake -S . -B build_u \
  -D OIIO_IMIV_ENABLE_VULKAN=AUTO \
  -D OIIO_IMIV_ENABLE_OPENGL=AUTO \
  -D OIIO_IMIV_ENABLE_METAL=OFF \
  -D OIIO_IMIV_DEFAULT_RENDERER=vulkan \
  -D OIIO_IMIV_ADD_BACKEND_VERIFY_CTEST=ON
```

Then list them:

```bash
ctest --test-dir build_u -N | rg imiv_backend_verify
```

Proof that one Vulkan-default build can drive OpenGL through `ctest`:

```bash
ctest --test-dir build_u -V -R '^imiv_backend_verify_opengl$'
```

Focused backend selector regression:

```bash
ctest --test-dir build_u -V -R '^imiv_backend_preferences_regression$'
```

## Remaining planned work

- Validate `OIIO_IMIV_ADD_BACKEND_VERIFY_CTEST=ON`.
- Validate the new Preferences backend selector on macOS and Windows.
- If that is clean, the next backend-switch slice is:
  - decide whether `imiv_backend_preferences_regression` should also be folded
    into `imiv_backend_verify.py`
  - present more explicit availability status in Preferences
  - decide whether to duplicate more individual `ctest` cases per enabled
    backend, or keep the shared `imiv_backend_verify.py` path as the backend
    matrix entrypoint

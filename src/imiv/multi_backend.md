# imiv Multi-Backend Design

## Goal

Support multiple renderer backends in one `imiv` binary on the same platform,
with launch-time selection from CLI or saved preferences.

This document describes the current implementation, not a future-only plan.


## Current State

`imiv` now supports:

- one binary with some or all of:
  - `Vulkan`
  - `Metal`
  - `OpenGL`
- runtime backend request via:
  - `--backend`
  - saved `renderer_backend` preference
  - platform/default fallback
- backend inspection via:
  - `--list-backends`
- persistent backend preference in `imiv.inf`
- Preferences UI backend selector with restart-required semantics

At the time of writing:

- Vulkan remains the reference renderer for renderer-side feature work
- the shared macOS backend verifier is green on:
  - `Vulkan`
  - `Metal`
  - `OpenGL`

This design does **not** attempt live in-process backend switching. Backend
changes apply on the next launch.


## Build Model

Backends are compiled independently through CMake:

```text
-D OIIO_IMIV_ENABLE_VULKAN=AUTO|ON|OFF
-D OIIO_IMIV_ENABLE_METAL=AUTO|ON|OFF
-D OIIO_IMIV_ENABLE_OPENGL=AUTO|ON|OFF
-D OIIO_IMIV_DEFAULT_RENDERER=auto|vulkan|metal|opengl
```

Generated build metadata lives in:

- [imiv_build_config.h.in](/mnt/f/gh/openimageio/src/imiv/imiv_build_config.h.in)

Generated build-capability macros:

- `IMIV_WITH_VULKAN`
- `IMIV_WITH_METAL`
- `IMIV_WITH_OPENGL`
- `IMIV_BUILD_DEFAULT_BACKEND_KIND`

Compatibility note:

- `OIIO_IMIV_RENDERER` still exists as a deprecated compatibility alias and is
  treated as `OIIO_IMIV_DEFAULT_RENDERER`


Embedded assets
---------------

Current multi-backend builds package two kinds of embedded runtime assets:

- fonts:
  - controlled by `OIIO_IMIV_EMBED_FONTS`
  - default `ON`
  - embeds `DroidSans.ttf` and `DroidSansMono.ttf`
- Vulkan static shaders:
  - embedded automatically when the Vulkan backend is compiled
  - covers the fixed upload/preview SPIR-V shaders from `src/imiv/shaders/`

This keeps one binary self-contained for the most common UI and static Vulkan
renderer assets. It does **not** eliminate all runtime shader work:

- Vulkan OCIO shaders are still generated at runtime from the active OCIO
  configuration.
- OpenGL still compiles GLSL at runtime.
- Metal still compiles MSL source at runtime.


## Runtime Model

The runtime backend metadata layer lives in:

- [imiv_backend.h](/mnt/f/gh/openimageio/src/imiv/imiv_backend.h)
- [imiv_backend.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_backend.cpp)

Core types:

- `BackendKind`
- `BackendInfo`
- `BackendRuntimeInfo`

The runtime model answers:

- which backends are compiled in
- which compiled backends are currently available
- why a compiled backend is unavailable
- which backend is the build default
- which backend is the platform default
- which backend was requested
- which backend is resolved for the current launch

Launch-time selection precedence:

1. CLI `--backend`
2. saved `renderer_backend` preference
3. configured default renderer, if runtime-available
4. platform-default compiled backend, if runtime-available
5. first runtime-available compiled backend fallback

Runtime availability is probed through the renderer/backend seam and cached in
the backend registry. The current implementation exposes per-backend
availability and unavailability reasons to:

- `--list-backends`
- Preferences
- test-engine state JSON


## CLI

Implemented CLI options:

- `--backend auto|vulkan|metal|opengl`
- `--list-backends`

Entry point:

- [imiv_main.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_main.cpp)

Behavior:

- `--backend` overrides the saved preference for that launch only
- `--list-backends` prints compiled backend support, runtime availability, and
  any unavailability reason, then exits


## Preferences UX

Implemented in:

- [imiv_aux_windows.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_aux_windows.cpp)

Behavior:

- backend selection is shown as equal-width buttons
- runtime-available compiled backend choices are selectable
- compiled but runtime-unavailable backends are shown disabled with a reason
- changing the requested backend updates the next-launch backend
- the current process keeps using the already-active backend
- the UI shows a restart-required note when the next launch would differ
- invalid or unavailable persisted backend requests are reset to `Auto` when
  Preferences closes

Persistence:

- `renderer_backend=auto|vulkan|metal|opengl`
- stored in `imiv.inf`
- loaded/saved through:
  - [imiv_viewer.h](/mnt/f/gh/openimageio/src/imiv/imiv_viewer.h)
  - [imiv_viewer.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_viewer.cpp)


## Platform Policy

Current runtime default resolution in [imiv_backend.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_backend.cpp):

### Windows

Preferred order:

- `Vulkan`
- `OpenGL`
- `Metal`

Typical compiled set:

- `Vulkan`
- `OpenGL`

### Linux / WSL

Preferred order:

- `Vulkan`
- `OpenGL`
- `Metal`

Typical compiled set:

- `Vulkan`
- `OpenGL`

### macOS

Preferred order:

- `Metal`
- `Vulkan`
- `OpenGL`

Typical compiled set:

- `Metal`
- `OpenGL`
- optional `Vulkan` via MoltenVK / Vulkan loader availability


## Renderer Boundary

The multi-backend architecture still depends on the renderer seam and backend
split introduced earlier.

Shared renderer interfaces:

- [imiv_renderer.h](/mnt/f/gh/openimageio/src/imiv/imiv_renderer.h)
- [imiv_renderer_backend.h](/mnt/f/gh/openimageio/src/imiv/imiv_renderer_backend.h)
- [imiv_renderer.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_renderer.cpp)

Backend implementations:

- Vulkan:
  - [imiv_renderer_vulkan.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_renderer_vulkan.cpp)
  - [imiv_vulkan_setup.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_vulkan_setup.cpp)
  - [imiv_vulkan_runtime.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_vulkan_runtime.cpp)
  - [imiv_vulkan_texture.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_vulkan_texture.cpp)
  - [imiv_vulkan_preview.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_vulkan_preview.cpp)
  - [imiv_vulkan_ocio.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_vulkan_ocio.cpp)
- Metal:
  - [imiv_renderer_metal.mm](/mnt/f/gh/openimageio/src/imiv/imiv_renderer_metal.mm)
- OpenGL:
  - [imiv_renderer_opengl.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_renderer_opengl.cpp)

Platform layer:

- [imiv_platform_glfw.h](/mnt/f/gh/openimageio/src/imiv/imiv_platform_glfw.h)
- [imiv_platform_glfw.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_platform_glfw.cpp)


## Validation

Canonical shared verifier:

- [imiv_backend_verify.py](/mnt/f/gh/openimageio/src/imiv/tools/imiv_backend_verify.py)

Shared suite:

- `smoke`
- `rgb`
- `ux`
- `sampling`
- `ocio_missing`
- `ocio_config_source`
- `ocio_live`
- `ocio_live_display`

Related focused regressions:

- [imiv_rgb_input_regression.py](/mnt/f/gh/openimageio/src/imiv/tools/imiv_rgb_input_regression.py)
- [imiv_sampling_regression.py](/mnt/f/gh/openimageio/src/imiv/tools/imiv_sampling_regression.py)
- [imiv_backend_preferences_regression.py](/mnt/f/gh/openimageio/src/imiv/tools/imiv_backend_preferences_regression.py)

Optional shared per-backend CTest entries:

- `imiv_backend_verify_vulkan`
- `imiv_backend_verify_opengl`
- `imiv_backend_verify_metal`

Enable with:

- `OIIO_IMIV_ADD_BACKEND_VERIFY_CTEST=ON`


## What Is Done

Completed slices:

- generated build-capability metadata
- runtime backend metadata API
- CLI backend request and listing
- persisted backend preference
- multi-backend CMake enable switches
- Preferences backend selector
- restart-required behavior
- shared backend verifier
- current macOS green shared-suite coverage on all three backends


## Remaining Work

Reasonable next steps, if needed:

1. expose richer runtime availability/failure reasons in the backend metadata
2. decide how far to take backend-specific CI coverage
3. decide whether distributed macOS Vulkan builds should bundle MoltenVK
4. keep backend docs and verification coverage aligned as behavior changes


## Non-Goals

These are still intentionally out of scope:

- live in-process renderer switching
- automatic app restart after backend change
- collapsing renderer-specific code back into shared UI/viewer layers

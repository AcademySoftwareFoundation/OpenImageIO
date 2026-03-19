# imiv Multi-Backend Plan

## Goal

Allow one `imiv` binary to support multiple renderer backends on the same
platform, with backend selection exposed in Preferences and applied on the next
launch.

Target UX:

- Preferences contains a backend selector.
- Changing backend shows a restart-required note.
- The app restarts later using the requested backend.
- CLI can override the backend for one launch.


## Scope

Renderer backends:

- `Vulkan`
- `Metal`
- `OpenGL`

Platform backend:

- `GLFW`

This plan does not attempt live in-process renderer switching. Backend changes
apply only on restart.


## Current State

`imiv` already has:

- a renderer seam
- backend-specific renderer implementations
- backend-specific CMake source buckets

But the build is still single-backend:

- CMake chooses exactly one renderer at configure time
- the binary is compiled with one `IMIV_BACKEND_*` policy macro
- runtime cannot choose between multiple compiled backends


## Desired Architecture

### 1. Build-time capabilities

Build one binary with some or all renderer backends compiled in.

Generated build config should expose:

- `IMIV_WITH_VULKAN`
- `IMIV_WITH_METAL`
- `IMIV_WITH_OPENGL`

These indicate compiled capabilities, not the selected runtime backend.


### 2. Runtime backend registry

Introduce a backend registry layer that can answer:

- which backends are compiled in
- which backend is the active one for this launch
- which backend was requested by CLI or prefs

Later this registry should also answer:

- runtime availability
- failure reasons


### 3. Selection precedence

For each launch:

1. CLI `--backend`
2. saved preference from `imiv.inf`
3. platform default
4. fallback to first available compiled backend


### 4. Preferences UX

Preferences should eventually expose:

- `Auto`
- `Vulkan`
- `Metal`
- `OpenGL`

Each backend should report one of:

- available
- built but unavailable
- not built

Changing it should:

- save the request in `imiv.inf`
- mark restart required


## Platform Policy

### Windows

Default build:

- `Vulkan`
- `OpenGL`

Default runtime preference:

- `Vulkan`

Fallback:

- `OpenGL`


### Linux / WSL

Default build:

- `Vulkan`
- `OpenGL`

Default runtime preference:

- `Vulkan`

Fallback:

- `OpenGL`


### macOS

Default build:

- `Metal`
- `OpenGL`

Optional build:

- `Vulkan` if Vulkan loader / MoltenVK is available

Default runtime preference:

- `Metal`

Suggested fallback order:

- `Metal`
- `Vulkan`
- `OpenGL`


## MoltenVK

Treat macOS Vulkan as optional in the first phase.

Phase 1:

- if `find_package(Vulkan)` succeeds on macOS, compile Vulkan support
- let runtime probing decide if it is usable

Phase 2:

- packaging and bundling MoltenVK for distribution

Do not block multi-backend runtime selection on MoltenVK packaging.


## CMake Plan

Replace the current single selector:

- `OIIO_IMIV_RENDERER=auto|vulkan|metal|opengl`

With per-backend enable controls:

- `OIIO_IMIV_ENABLE_VULKAN=AUTO|ON|OFF`
- `OIIO_IMIV_ENABLE_METAL=AUTO|ON|OFF`
- `OIIO_IMIV_ENABLE_OPENGL=AUTO|ON|OFF`

Keep:

- `OIIO_IMIV_DEFAULT_RENDERER=auto|vulkan|metal|opengl`

Semantics:

- `AUTO` means enable if supported and dependencies are found
- `ON` means hard requirement
- `OFF` excludes the backend

The first implementation slice may keep the current single-backend build while
introducing generated capability metadata and runtime selection plumbing.


## Runtime Plan

Introduce backend metadata types:

- `BackendKind`
- `BackendInfo`

Later add:

- `BackendVTable`
- backend registry functions

Each renderer implementation should eventually export a backend vtable instead
of relying on compile-time global dispatch.


## Persistence

Store requested backend in `imiv.inf`:

- `renderer_backend=auto|vulkan|metal|opengl`

CLI override should not rewrite persisted settings.


## CLI

Add:

- `--backend auto|vulkan|metal|opengl`
- `--list-backends`

`--backend` overrides prefs for one launch.


## Implementation Phases

### Phase 1: metadata and request plumbing

- add generated build-capability header
- add runtime backend metadata API
- add CLI parsing for backend request and listing
- save/load requested backend in prefs
- keep current single-backend runtime behavior

### Phase 2: multi-backend CMake

- allow multiple backends to compile into one binary
- generate capability macros from enabled backend set
- keep current runtime using one active backend chosen at launch

### Phase 3: runtime dispatch

- replace compile-time backend dispatch with backend registry + vtables
- launch selected backend dynamically

### Phase 4: Preferences UX

- add backend selector
- add restart-required note
- add fallback / unavailable messaging

### Phase 5: packaging

- decide which backends ship on each platform
- handle optional macOS Vulkan / MoltenVK packaging


## First Slice Started

This branch should begin with:

- `multi_backend.md`
- generated `imiv_build_config.h`
- backend metadata / request API
- `AppConfig` backend request field
- CLI `--backend` and `--list-backends`
- persisted `renderer_backend` preference

That is enough to start shaping the runtime selection path without destabilizing
the current renderer implementations.

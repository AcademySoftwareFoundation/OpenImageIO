# imiv Backend Coverage

## Goal

Keep `imiv` app/viewer/UI code backend-agnostic, and keep renderer-specific
constraints explicit in one place.

This document is the working contract for:

- backend feature coverage
- backend-specific constraints
- backend-specific technical debt
- what is allowed in shared code vs renderer code

## Backend Model

Current backend selection is:

- platform: `GLFW`
- renderer: `Vulkan`, `Metal`, or `OpenGL`

Selected by CMake:

```text
-D OIIO_IMIV_RENDERER=auto|vulkan|metal|opengl
```

Current default selection:

- non-Apple: `vulkan`
- Apple: `metal`

## Support Levels

- `Primary`: expected to carry the full `imiv` feature set
- `Development`: intended to become usable, but still missing visible features
- `Skeleton`: bootstrap only; not yet a real viewer backend

## Shared-Code Rules

These rules are intentional and should stay true as backend work continues.

1. Shared app/viewer/UI code must not expose backend-native types.
   Examples: no `Vk*`, `MTL*`, or raw GL object types in shared public
   interfaces.
2. Backend-specific work belongs behind the renderer seam.
   Current seam entry points live in:
   - [imiv_renderer.h](/mnt/f/gh/openimageio/src/imiv/imiv_renderer.h)
   - [imiv_renderer_backend.h](/mnt/f/gh/openimageio/src/imiv/imiv_renderer_backend.h)
3. Platform-specific window/bootstrap work belongs in:
   - [imiv_platform_glfw.h](/mnt/f/gh/openimageio/src/imiv/imiv_platform_glfw.h)
   - [imiv_platform_glfw.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_platform_glfw.cpp)
4. Backend-specific code should live in backend-specific translation units:
   - Vulkan: `imiv_renderer_vulkan.cpp` + `imiv_vulkan_*`
   - Metal: `imiv_renderer_metal.mm`
   - OpenGL: `imiv_renderer_opengl.cpp`

## Coverage Matrix

Legend:

- `Yes`: implemented and expected to work
- `Partial`: implemented with known gaps
- `No`: not implemented

| Feature | Vulkan | OpenGL | Metal |
|---|---|---|---|
| App bootstrap and main window | Yes | Yes | Yes |
| Dear ImGui backend | Yes | Yes | Yes |
| Direct image upload | Yes | Yes | No |
| Preview rendering | Yes | Yes | No |
| Exposure / gamma / offset | Yes | Yes | No |
| Channel / luma / heatmap modes | Yes | Yes | No |
| Orientation-aware preview | Yes | Yes | No |
| Linear / nearest preview sampling | Yes | Yes | No |
| Pixel closeup window | Yes | Yes | No |
| Area Sample / selection UI | Yes | Yes | No |
| Drag and drop | Yes | Yes | Yes |
| Screenshot / readback | Yes | Yes | No |
| OCIO display/view | Yes | Yes | No |
| Runtime OCIO config switching | Yes | Partial | No |
| Automated GUI regression coverage | Yes | Partial | No |

## Vulkan

Status:

- `Primary`

Implementation:

- renderer seam:
  - [imiv_renderer_vulkan.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_renderer_vulkan.cpp)
- Vulkan-specific modules:
  - [imiv_vulkan_setup.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_vulkan_setup.cpp)
  - [imiv_vulkan_runtime.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_vulkan_runtime.cpp)
  - [imiv_vulkan_texture.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_vulkan_texture.cpp)
  - [imiv_vulkan_preview.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_vulkan_preview.cpp)
  - [imiv_vulkan_ocio.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_vulkan_ocio.cpp)
  - [imiv_capture.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_capture.cpp)

Constraints:

- uses runtime shader compilation for OCIO
- uses Vulkan SPIR-V pipelines
- remains the canonical backend for feature parity and regressions

Notes:

- New viewer features should land on Vulkan first if they need renderer work.
- Other backends should match Vulkan behavior, not invent incompatible UI
  semantics.

## OpenGL

Status:

- `Development`

Implementation:

- [imiv_renderer_opengl.cpp](/mnt/f/gh/openimageio/src/imiv/imiv_renderer_opengl.cpp)

Hard constraints:

1. OpenGL must stay a non-compute backend.
2. OpenGL must not use SPIR-V.
3. OpenGL preview shaders must be native GLSL compiled with the GL driver.
4. Direct source upload is preferred over a Vulkan-style upload/compute stage.
5. OCIO must use OCIO GLSL output, not the Vulkan runtime shader
   path.

Target API level:

- macOS: `OpenGL 3.2 Core`
- non-Apple: keep the feature set within the same envelope where practical

Current design:

- source image is uploaded directly as a GL texture
- preview is rendered through a GLSL fragment shader into a preview texture
- OCIO preview uses a separate GLSL program built from OCIO GPU shader output
- no compute stage
- no SPIR-V stage

Current gaps:

- no live OCIO display/view update regression equivalent to Vulkan yet
- no dedicated OpenGL selection/interaction regression target yet
- no Metal-equivalent backend parity to compare against

Current coverage:

- direct source upload
- basic preview controls
- orientation-aware preview
- screenshot/readback
- OCIO config selection and builtin/global/user fallback
- OCIO GLSL preview path
- OpenGL-only screenshot smoke regression

OCIO notes:

- startup preflight for OpenGL now validates the OCIO runtime/config path
  without using Vulkan SPIR-V compilation
- OpenGL OCIO regressions currently rely on the shared config/fallback tests,
  not the Vulkan-only runtime-glslang live-update tests

## Metal

Status:

- `Skeleton`

Implementation:

- [imiv_renderer_metal.mm](/mnt/f/gh/openimageio/src/imiv/imiv_renderer_metal.mm)

Current scope:

- GLFW + Cocoa window hookup
- Metal device / command queue creation
- CAMetalLayer setup
- Dear ImGui Metal backend hookup

Current gaps:

- no image upload
- no preview rendering
- no screenshot/readback
- no OCIO path

Planned direction:

- use backend-native Metal rendering
- do not try to reuse Vulkan pipeline code directly
- OCIO should later use OCIO MSL output, not the Vulkan shader path

## Feature Mapping Rules

These should stay consistent across backends where the feature exists.

1. Viewer/UI state is shared.
   Examples:
   - selection
   - Area Sample
   - loaded-image list
   - OCIO UI state
2. Preview behavior should match across backends where implemented.
   Examples:
   - orientation
   - channel display modes
   - exposure/gamma/offset
3. Backend-specific gaps should degrade explicitly, not silently.
   Examples:
   - Metal currently disables OCIO
   - non-implemented screenshot paths should report that clearly

## Current Priority Order

1. Keep Vulkan stable as the reference backend.
2. Finish OpenGL bring-up enough to support real GUI validation:
   - expand OpenGL-specific regression coverage beyond smoke
   - add live OCIO update coverage
3. Keep OpenGL OCIO GLSL path aligned with Vulkan behavior.
4. Use the Metal skeleton as the macOS landing point for real backend work.
5. Do not let shared app code regress back into Vulkan-only assumptions.

## Change Checklist

When adding or changing backend behavior, update this file if the answer to any
of these is `yes`:

- Did feature coverage change?
- Did a backend constraint change?
- Did a backend become usable for a new class of tests?
- Did the canonical path for OCIO / preview / capture change?

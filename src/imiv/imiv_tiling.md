# imiv Tiling and Large-Image Design

## Goal

Add a scalable image-viewing path for large images across all `imiv`
backends while keeping color/format normalization on the GPU where practical.

This document started as a design note. The first Vulkan/OpenGL/Metal safety slice
is now implemented:

- large Vulkan uploads no longer rely on one giant
  `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` descriptor range;
- the current Vulkan compute upload path uses aligned row stripes with dynamic
  storage-buffer offsets; and
- the current OpenGL source upload path allocates first and then uploads large
  images in row stripes via `glTexSubImage2D`;
- the current Metal source upload path keeps compute normalization but
  dispatches it with stripe-sized `MTLBuffer` inputs; and
- the broader shared CPU/GPU tile-cache architecture described below remains
  future work.


## Problem

Current `imiv` behavior is a full-image path:

- load the full image into `LoadedImage`
- upload the full source image to the backend
- create full-size preview resources

This is acceptable for ordinary still images and the current regression suite,
but it does not scale well for:

- large texture plates
- stitched panoramas
- very high resolution scans
- multi-image sessions with several large images loaded at once

The immediate Vulkan bug that triggered this discussion is one symptom of that
design:

- current Vulkan compute upload binds the full raw source payload as one
  `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`
- large images can exceed `VkPhysicalDeviceLimits::maxStorageBufferRange`

That is not a general Vulkan image-size limitation. It is a limitation of the
current upload model.


## What iv Did

Old `iv` did not upload a whole image as one giant GPU resource.

Its OpenGL path:

- computed the visible image rectangle
- split that rectangle into texture-sized tiles
- uploaded needed image patches tile-by-tile
- rendered those tiles through the same shader/display path

Relevant behavior in `src/iv/ivgl.cpp`:

- texture size is clamped, with a practical cap of `4096`
- visible region is snapped to tile boundaries
- `load_texture()` uploads only the patch needed for a tile

So `iv` already behaved like a tiled viewer, not a full-image uploader.


## Requirements

1. Large images must not require one monolithic raw GPU upload buffer.
2. Small and medium images should keep a simple fast path.
3. GPU normalization should remain available:
   - `RGB -> RGBA`
   - integer normalization
   - half/float handling
4. The design should work across:
   - Vulkan
   - OpenGL
   - Metal
5. The backend-independent view model should not depend on one backend's
   upload mechanism.


## Non-goals

Not in the first implementation:

- full sparse virtual-texture system
- recursive multi-resolution pyramid management for arbitrary formats
- backend-specific feature divergence
- replacing all current full-image paths immediately


## Recommended Model

Split the problem into three layers.

### 1. CPU image/tile layer

Responsible for:

- visible-region computation
- tile indexing
- tile reads from `ImageBuf` / `ImageCache`
- CPU-side cache of source tiles

This layer should know nothing about Vulkan, OpenGL, or Metal.

Suggested concepts:

- `ImageTileKey`
  - image identity
  - subimage
  - miplevel / proxy level
  - tile x/y
- `ImageTileRequest`
  - tile key
  - pixel ROI
  - source type / channel count
- `CpuTile`
  - tile pixel data
  - row pitch
  - source type
  - channel count

### 2. GPU tile normalization layer

Responsible for:

- taking one source tile
- converting it to the backend sampling format
- storing the normalized tile in a GPU cache

This is where `RGB -> RGBA` should happen on GPU where the backend supports it
well.

Suggested output format:

- `RGBA16F` when available and appropriate
- `RGBA32F` fallback when needed

The output of this layer is not "the whole image".
It is one normalized tile.

### 3. Preview/render layer

Responsible for:

- deciding which tiles are visible
- drawing the visible normalized tiles
- applying preview controls
- applying OCIO preview

This layer should work from a set of normalized tiles, not from one full image
texture.


## View Model

The tiled system should preserve the current view-centric direction:

- one shared image library
- multiple views
- per-view `ViewRecipe`

Each view should drive its own tile requests based on:

- active image
- zoom
- pan/center
- viewport size
- orientation

`ViewRecipe` remains independent from the tile cache.


## Small-image vs Large-image Mode

Recommended hybrid model:

### Full-image mode

Keep the current full-image path for:

- ordinary images
- regression simplicity
- lower complexity on common cases

### Tiled mode

Use the tiled path when any of these become true:

- raw upload exceeds backend-safe limits
- image dimensions exceed a configured threshold
- total estimated source upload memory is too large
- user explicitly enables large-image mode later

This avoids forcing complexity on small images while fixing the large-image
case correctly.


## Backend Strategies

### Vulkan

Preferred direction:

- per-tile source upload
- per-tile compute normalization into a normalized tile image

Avoid:

- one full-image storage-buffer descriptor
- one monolithic upload buffer for the entire image

For Vulkan, a tile is the natural unit for:

- staging upload
- compute dispatch
- descriptor binding

This also avoids the current `maxStorageBufferRange` problem.

### Metal

Preferred direction:

- keep the compute normalization model already used in Metal
- move it from "whole image" to "per tile"

This is aligned with the current Metal design and should be a relatively
natural extension.

### OpenGL

Two valid paths:

1. Direct native upload per tile
   - upload `R/RG/RGB/RGBA` tiles directly
   - sample directly in preview shaders

2. Normalized tile pass
   - upload source tile
   - render/convert into normalized RGBA tile via FBO/shader

Recommendation:

- keep OpenGL practical
- do not force an OpenGL compute-style design just for symmetry

OpenGL should keep native GL strengths, but still fit the same higher-level
tile-cache model.


## Tile Cache

Two caches are needed.

### CPU tile cache

Stores source tiles read from OIIO.

Used to avoid repeated disk/cache reads while panning around the same area.

### GPU tile cache

Stores normalized backend-ready tiles.

Used to avoid repeated normalization/upload for tiles already visible or
recently viewed.

Suggested properties:

- LRU-style eviction
- byte-budget based
- backend-specific capacity


## Tile Size

Initial recommendation:

- default tile size around `512` or `1024`
- backend may clamp or tune

Tile size should be chosen based on:

- upload overhead
- GPU cache friendliness
- descriptor/update pressure
- preview redraw cost

It does not need to match file-internal tiling exactly.


## OCIO and Preview Controls

Keep the existing conceptual split:

- source/normalization
- preview rendering

Per-view preview controls should remain preview-stage operations:

- exposure
- gamma
- offset
- channel/color mode
- OCIO display/view
- interpolation choice

These should not force source-tile re-normalization unless a backend
implementation truly needs that.


## Huge Images and Proxy Levels

The first tiled design does not need a full mip/proxy system.

But it should leave room for:

- reduced-resolution tiles for zoomed-out display
- source subimage/miplevel mapping
- future `ImageCache`-driven proxy logic

Suggested rule for the first pass:

- tile the currently selected image resolution
- do not add automatic proxy selection yet

Then add proxy selection in a later phase if needed.


## Phased Implementation

### Phase 1: Immediate Vulkan/OpenGL/Metal safety fix

Goal:

- stop using one monolithic full-image upload step for oversized images

Preferred approach:

- introduce striped Vulkan source upload
- introduce striped OpenGL source upload
- introduce striped Metal source upload
- keep Vulkan/Metal compute normalization on GPU

Avoid if possible:

- large-image CPU fallback as the long-term answer

### Phase 2: Shared tiling framework

Add backend-neutral:

- visible tile computation
- tile keys/requests
- CPU tile cache
- GPU tile cache interface

### Phase 3: Vulkan tiled path

Make Vulkan use the shared tile framework as the first real backend.

This becomes the reference implementation.

### Phase 4: Metal tiled path

Port the same model to Metal with per-tile compute normalization.

### Phase 5: OpenGL tiled path

Port the shared model to OpenGL using direct native tile upload or normalized
tile rendering.

### Phase 6: Proxy levels

Add optional reduced-resolution / proxy selection for zoomed-out views.


## Testing

Add focused tests in this order:

1. large-image multi-file switch regression
   - verifies no crash on next/previous image
2. tile visibility regression
   - verifies visible region requests expected tiles
3. pan/zoom large-image regression
   - verifies tile reuse/eviction behavior at a basic level
4. backend parity regression
   - same large image set on Vulkan/OpenGL/Metal

Test data should include:

- large RGB image
- large RGBA image
- large integer image
- large half/float image when practical


## Recommendation

Proceed with a tiled architecture for all backends.

Do not abandon GPU normalization.

The correct direction is:

- tile-based CPU image access
- tile-based GPU upload
- per-tile GPU normalization
- per-view preview rendering from normalized tile caches

That preserves the current renderer goals while fixing the class of large-image
bugs that the monolithic full-image upload path is now hitting.

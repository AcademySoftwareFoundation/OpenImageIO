# GPU Texture System

Create a new test to prototype a new C++ texture system that can work on a GPU.
We will call this GPU "the device", and the only requirement from it is that it can
run a kernel written in C++. The test will create an executable following the
pattern of the few existing tests that already do that.

The purpose of the test is to simulate execution of a texture system on the device.
For this we will write a class that models the device interface as seen from the 
CPU. For now it will just be a fake GPU. Here's an example:
```cpp
class MockDevice {
  using Kernel = void (*)(int x, int y, tagged_ptr<void> data);

  tagged_ptr<void> alloc(size_t bytes, const char* purpose);
  void free(tagged_ptr<void> p);
  void copy_to(tagged_ptr<void> device, tagged_ptr<const void> host,
               size_t bytes);
  void copy_from(tagged_ptr<void> host, tagged_ptr<const void> device,
                 size_t bytes);
  void run(int width, int height, Kernel kernel, tagged_ptr<void> data);
};
```

The MockDevice class is minimal. We are going to assume that what you allocate in the
device cannot be accessed by the host and vice versa. But truly it is just a mock-up,
`copy_to` and `copy_from` will just be `memcpy`. But we write the rest of the code as
if these pointers were real host, GPU.


The test is split between host orchestration (`host.cpp`) and the kernel source
(`blend.cpp`). Both are compiled together and linked to OIIO for the main test.
There is also a sanity-check binary where the kernel/device-side code must
compile itself alone to an executable, even if it is useless. This separate binary:
  * Can include any OIIO header. BUT ...
  * It must **not** link against OIIO.

And that is our way of ensuring the kernel with the device texture system can 
actually run on a GPU. Do this as a separate build target in the `CMakeLists.txt` so
cmake itself enforces it.


## Test Exercise

The idea of this test is to do a simple operation. A kernel will access two on-disk
textures via file name plus u, v coordinates (with derivatives). It will do a blend
of the two where one uses pixel size derivatives and the other uses something 4
times bigger so we see a blur. Then the output image will be written to disk by the
host side using OIIO.

## Purpose of the test

Gather all the functionality we need from OIIO to prototype the texture system so
we can then publish it in OIIO headers one thing at a time. In the first stage, if
something is not yet exposed to be inlined, we will copy&paste it into the test
code. And when we are happy with the result we can start moving code to the official 
headers inside OIIO.

## General Guidelines
### OIIO Style

The common OpenImageIO coding style should be followed and a few existing headers
and sources should be explored. We are going to be borrowing code for texture
filtering and mip mapping, so this should be located first. Implement the full 
EWA/anisotropic footprint pipeline (ellipse_axes, anisotropic_aspect, 
compute_miplevels, compute_ellipse_sampling) as in `texturesys.cpp`.

### Start Simple but with Full Filtering

This prototype is a proof of concept and we must use simple approaches except 
for mip mapping. The footprint computation (ellipse_axes, anisotropic_aspect, 
compute_miplevels, compute_ellipse_sampling) should be close to the original 
but keep the inner sampling to bilinear-only (no bicubic, no stochastic).

This is the hardest part of the effort. The logic needs to be decoupled from `ImageCache` 
and extracted without SIMD to headers in the test for inlining. You may need
several logical headers to organize the code.

### Prepare for Inlining

All tooling code that has to run on the device must be inlined. For later
incorporation in OpenImageIO we will keep it in headers inside the test. If the
implementation is more than a few lines we will split headers in blah_decl.h and
blah_impl.h.

### SIMD vs scalar on device

OIIO's existing texture filtering uses `simd::vfloat4` internally (with scalar fallbacks
when `OIIO_SIMD` is 0). Use only scalar logic and ignore SIMD.


# Design

The device texture system uses a launch, fail and retry cycle. This means we try
to run the kernel and if something is missing execution is aborted so the host can
load what the kernel needs. The following data structures are needed:
  * A device-side map from ustringhash to texture descriptor. This descriptor holds
    data and display window, as well as mip level ranges. It will also keep a
    pointer to the following ...
  * Another per-texture map from tile coordinates (mip level, x and y) to the
    pixels of the tile, that is float RGBA colors.
  * A request queue. Requests can be missing textures (file name ustringhash) or
    missing tiles (ustringhash plus tile coordinates).

All these maps are pointerless hash tables that work on the device. Defined as a
template on the Key and Value, they are always created, filled and resized on the
host. Then copied in just one block to the device. The device kernel can query
them but not modify or grow.

We will use a closed hash (open addressing) with linear probing. OIIO has `hash.h`
with `farmhash` but no pointerless hash table template. Write the closed-hash table
from scratch but using similar patterns. If possible reuse the same hash functions.

In the current prototype, the request queue is implemented as a closed hash map
(`Request -> bool`) with dedup semantics. This keeps request insertion idempotent
across many pixels and avoids repeated host work. The queue can overflow when full;
on host side this triggers a grow+retry path (`needs_retry()`), which clears the
queue and re-runs the launch to recollect missing requests at the new capacity.

Note all pixels in the kernel run are executed even if one fails. We want to collect
as many requests as possible. Concurrent access to the queue will be based on atomic
integers. It is ok to use the CPU ones for now, but we can use a 
`DTextureSystem::atomic_inc()` method to abstract it.


## The tile hash map

Tiles are big, so it wouldn't be efficient to store them in a closed hash table.
Instead we will store them in a sidecar vector ordered as they come. Then the hash
map will just store integer offsets into this table.

## Example execution

This is the workflow that the test will follow, starting with completely empty
data structures:
  1. Launches the kernel
  2. Kernel tries to read a texture by ustringhash, u, v, etc...
  3. Texture is missing. File a request, give up on this pixel.
  4. Host finds the kernel failed, handle requests and update tables on device.
  5. Repeat

The host fetches the request queue after a kernel run (step 4). A failure exists if
it is not empty.

On the second run textures may be found but tiles will be missing so more requests
will be filed for the host to satisfy until the kernel runs to completion. When
the kernel looks up a known texture, it computes the footprint of the filter, and from
that it decides which tiles it needs from what mip levels. Whatever is missing it's
requested and otherwise the lookup is computed and returned.

## Main APIs

The texture system lives both on the host and on device. It is a templated C++ class
with a managed (device) specialization and manager (host) specialization. It keeps:
  * The hash map of texture descriptors
  * A hash map from tile coordinates to tile-pool indices
  * A paged tile pool (sidecar stream) storing tile pixel payloads
  * The request queue

All of these have identical copies on the device side. It is structured like this:
```cpp
template<class Arena, class ManagedArena = NullArena>
class DTextureSystem {
  // Device-side lookup path
  RGBA lookup(ustringhash, float u, float v, Vec2 du, Vec2 dv);

  // Host-side orchestration path (when IsManager=true)
  bool needs_retry();
  template<class Func> bool process_requests(Func&&);
  void sync_to_managed();
  void sync_from_managed();

  // Resident state
  ClosedHashMap<ustringhash, TextureRecordIndex> textures;
  ClosedHashMap<TileCoords, TilePoolIndex>       tile_index;
  Stream<TileRecord>                              tile_pool;
  ClosedHashMap<Request, bool>                    requests;
};

class TextureLoader {
  bool process_request(const Request&, DTextureSystem<Host, MockDevice>*);
};

template<class T, size_t N>
struct vector_lite : public std::array<T, N> {
  // Fixed-capacity storage with dynamic active size.
  unsigned size() const;
  void push_back(const T&);
};

struct RequestHash {
  size_t operator()(const Request&) const;
  static uint64_t hash_mix_u64(uint64_t h, uint64_t v);
};

struct MipSelection {
  unsigned mip_levels[2];
  float mip_blend;
};
```

The key idea is that `DTextureSystem` is device data and code with host-manager
specialization support. The implementation is split in declaration/implementation
headers for inlining (`texture_device_decl.h` + `texture_device_impl.h`), and host
texture resolution/payload loading is delegated to `TextureLoader`.

Filtering sample generation uses a return-by-value fixed-capacity container
(`vector_lite<Sample, kMaxSamples * 4>`) so we can keep stack-local ownership while
still tracking runtime sample count. Tile-loading and accumulation loops consume
the dynamic active size instead of always iterating the full capacity.

OIIO's tile size defaults to 64. Use 64x64 as the hardcoded tile size and assume 
RGBA shading values (accept RGB or RGBA input textures; use alpha=1 for RGB). The buffers for the tiles are grown and copied again 
when new data is added. This is inefficient but good enough for a first draft. 
Also we are using `RGBA` and `Vec2`. These should be:
  - `RGBA` → `Imath::C4f` (`.r`, `.g`, `.b`, `.a`) 
  - `Vec2` → `Imath::V2f` (already available via OIIO's Imath dependency).


## Example Kernel

The device runs the kernel on a grid of pixels with a tagged data pointer.
```cpp
void blend_kernel(int x, int y, tagged_ptr<void> data){
  tagged_ptr<BlendOp> op(data);
  float resx = float(op->width), resy = float(op->height);
  float u = x / resx, v = y / resy;
  Vec2 duA = { 1 / resx, 0 }, dvA = { 0, 1 / resy };
  Vec2 duB = 4 * duA, dvB = 4 * dvA; // Make it blurry
  RGBA A = op->texture_system.lookup(op->name_a, u, v, duA, dvA);
  RGBA B = op->texture_system.lookup(op->name_b, u, v, duB, dvB);
  // We let both lookups run to file as many requests as possible, but now we can
  // early exit.
  if (op->texture_system.failures())
    return;
  op->output_buffer[y * op->width + x] = 0.5 * A + 0.5 * B;
}
```

The 'MockDevice::run()' method just runs this kernel for all pixels using an OIIO
parallel loop to check that we don't run into race conditions. This method
implementation can live in host.cpp for the mock-up, only the kernel lives in
blend.cpp.

# Steps

This is a suggested rough plan
  1. Create basic test files, cmake and cpp.
  2. Write an implementation of a pointerless hash table.
  3. Write unit tests within the existing test that writes, uploads and queries
     the hash. Make sure these unit tests pass.
  4. Write and test the launch, fail, repeat cycle.
  5. Design the request queue implementation and also unit test it.
  6. Gather the filtering functionality that we need for the kernel.
  7. Finally write the blending test.

`texture-device` should be added to the `oiio_add_tests()` list in 
`src/cmake/testing.cmake` so it's discovered by CTest. This is part of step 1 
(creating basic test files). The mentioned unit tests should be assert-based self-tests
within the main test executable that run before the blend kernel, exiting non-zero
on failure.

For the blend test use the existing `testsuite/common/textures/grid.tx` and
`checker.tx` (pre-MIP-mapped, already in repo). And we read everything using OIIO
`ImageInput`. Convert to 3 channel if needed and do not clean up any generated files
(output images, build directories) automatically, the expectation is that `run.py` 
leaves artifacts behind for debugging. Also use exr/tif output and image comparison
via `diff_command` in `run.py` for the blend test validation.

# Codebase Analysis

## Existing test patterns

Only 2 test suites produce their own executables:
- `testsuite/cmake-consumer/` -- standalone `CMakeLists.txt`, builds `consumer` target 
  linked to OIIO, run via `run.py`
- `testsuite/docs-examples-cpp/` -- standalone `CMakeLists.txt`, builds 8 executables
  (`docs-examples-texturesys`, etc.), all linked to both OIIO and Imath

All other 142+ tests are `runtest.py`-based, using pre-built OIIO tools (`testtex`, 
`oiiotool`, `maketx`).

The `cmake-consumer` pattern is closest to what we need: a `CMakeLists.txt` that builds 
executables and a `run.py` that invokes cmake + runs them. Tests are registered in 
`src/cmake/testing.cmake` via `oiio_add_tests()`.

No existing test uses a `host.cpp` / `blend.cpp` split or has one target linked to OIIO 
and another not.

## Key OIIO headers and types for device-side code

**GPU-safe headers already in OIIO:**
- `OpenImageIO/ustring.h` -- `ustringhash` is header-only, stores a single `uint64_t`, 
  fully `constexpr`/device-safe
- `OpenImageIO/hash.h` -- `farmhash::inlined::Hash64` is `OIIO_HOSTDEVICE constexpr`
- `OpenImageIO/fmath.h` -- `fast_sincos`, `fast_atan2`, `safe_sqrt`, `madd`, `clamp`,
  `fast_exp`, etc. all `OIIO_HOSTDEVICE`
- `OpenImageIO/platform.h` -- `OIIO_HOSTDEVICE`, `OIIO_DEVICE_CONSTEXPR` macros
- `OpenImageIO/simd.h` -- `vfloat4`, `vint4`, etc. have scalar fallbacks when `OIIO_SIMD` is 0
- `OpenImageIO/texture.h` -- `TextureOpt`, `MipMode`, `InterpMode`, `Wrap` enums (no GPU 
   annotations, but pure data)

**NOT GPU-safe:**
- `ustring` (not `ustringhash`) -- uses global hash table with mutex/malloc
- SIMD types with `OIIO_SIMD > 0` -- use SSE/AVX intrinsics

**Types:**
- Vec2/Vec3: OIIO wraps Imath (`Imath::V2f`, `Imath::V3f`) via `OpenImageIO/Imath.h`
- RGBA color: `Imath::C4f` (`.r`, `.g`, `.b`, `.a`) or raw `float[4]`
- OIIO internally uses `simd::vfloat3` (padded to 4) for color accumulation

## Texture filtering code location

All in `src/libtexture/texturesys.cpp`:
- `ellipse_axes()` -- EWA footprint computation (pure math, ~40 lines)
- `anisotropic_aspect()` -- anisotropy clamping (in `texture_pvt.h`, ~35 lines)
- `adjust_blur()` -- sblur/tblur application
- `compute_miplevels()` -- mip level selection with blend weights (~80 lines)
- `compute_ellipse_sampling()` -- sample placement along major axis (~55 lines)
- `st_to_texel()` -- coordinate to texel mapping (in `texture_pvt.h`, ~15 lines)
- `sample_bilinear()` -- bilinear interpolation within a mip level (depends heavily on 
  tile cache, ~200+ lines)
- `texture_lookup()` -- full EWA anisotropic lookup, orchestrates all of the above 
  (~230 lines)

The tile cache layer (`ImageCacheImpl`, `ImageCacheFile`, `ImageCacheTile`) is the biggest 
piece that needs adaptation for GPU.

## Source textures available in repo

- `testsuite/common/textures/grid.tx` -- pre-MIP-mapped grid, used by 25+ existing tests
- `testsuite/common/textures/checker.tx` -- pre-MIP-mapped checkerboard, used by `texture-blurtube`
- `testsuite/common/grid.tif` -- 1000x1000 plain TIFF grid, can be `maketx`'d at runtime
- `testsuite/common/checker_with_alpha.exr` -- checkerboard with alpha

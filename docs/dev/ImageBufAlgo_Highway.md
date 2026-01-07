ImageBufAlgo Highway (hwy) Implementation Guide
==============================================

This document explains how OpenImageIO uses Google Highway (hwy) to accelerate
selected `ImageBufAlgo` operations, and how to add or modify kernels in a way
that preserves OIIO semantics while keeping the code maintainable.

This is a developer-facing document about the implementation structure in
`src/libOpenImageIO/`. It does not describe the public API behavior of the
algorithms.


Goals and non-goals
-------------------

Goals:
- Make the hwy-backed code paths easy to read and easy to extend.
- Centralize repetitive boilerplate (type conversion, tails, ROI pointer math).
- Preserve OIIO's numeric semantics (normalized integer model).
- Keep scalar fallbacks as the source of truth for tricky layout cases.

Non-goals:
- Explain Highway itself. Refer to the upstream Highway documentation.
- Guarantee that every ImageBufAlgo op has a hwy implementation.


Where the code lives
--------------------

Core helpers:
- `src/libOpenImageIO/imagebufalgo_hwy_pvt.h`

Typical hwy call sites:
- `src/libOpenImageIO/imagebufalgo_addsub.cpp`
- `src/libOpenImageIO/imagebufalgo_muldiv.cpp`
- `src/libOpenImageIO/imagebufalgo_mad.cpp`
- `src/libOpenImageIO/imagebufalgo_pixelmath.cpp`
- `src/libOpenImageIO/imagebufalgo_xform.cpp` (some ops are hwy-accelerated)


Enabling and gating the hwy path
-------------------------------

The hwy path is only used when:
- Highway usage is enabled at runtime (`OIIO::pvt::enable_hwy`).
- The relevant `ImageBuf` objects have local pixel storage (`localpixels()` is
  non-null), meaning the data is in process memory rather than accessed through
  an `ImageCache` tile abstraction.
- The operation can be safely expressed as contiguous streams of pixels/channels
  for the hot path, or the code falls back to a scalar implementation for
  strided/non-contiguous layouts.

The common gating pattern looks like:
- In a typed `*_impl` dispatcher: check `OIIO::pvt::enable_hwy` and `localpixels`
  and then call a `*_impl_hwy` function; otherwise call `*_impl_scalar`.

Important: the hwy path is an optimization. Correctness must not depend on hwy.


OIIO numeric semantics: why we promote to float
----------------------------------------------

OIIO treats integer image pixels as normalized values:
- Unsigned integers represent [0, 1].
- Signed integers represent approximately [-1, 1] with clamping for INT_MIN.

Therefore, most pixel math must be performed in float (or double) space, even
when the stored data is integer. This is why the hwy layer uses the
"LoadPromote/Operate/DemoteStore" pattern.

For additional discussion (and pitfalls of saturating integer arithmetic), see:
- `HIGHWAY_SATURATING_ANALYSIS.md`


The core pattern: LoadPromote -> RunHwy* -> DemoteStore
-------------------------------------------------------

The helper header `imagebufalgo_hwy_pvt.h` defines the reusable building blocks:

1) Computation type selection
   - `SimdMathType<T>` selects `float` for most types, and `double` only when
     the destination type is `double`.

   Rationale:
   - Float math is significantly faster on many targets.
   - For OIIO, integer images are normalized to [0,1] (or ~[-1,1]), so float
     precision is sufficient for typical image processing workloads.

2) Load and promote (with normalization)
   - `LoadPromote(d, ptr)` and `LoadPromoteN(d, ptr, count)` load values and
     normalize integer ranges into the computation space.

   Rationale:
   - Consolidates all normalization and conversion logic in one place.
   - Prevents subtle drift where each operation re-implements integer scaling.
   - Ensures tail handling ("N" variants) is correct and consistent.

3) Demote and store (with denormalization/clamp/round)
   - `DemoteStore(d, ptr, v)` and `DemoteStoreN(d, ptr, v, count)` reverse the
     normalization and store results in the destination pixel type.

   Rationale:
   - Centralizes rounding and clamping behavior for all destination types.
   - Ensures output matches OIIO scalar semantics.

4) Generic kernel runners (streaming arrays)
   - `RunHwyUnaryCmd`, `RunHwyCmd` (binary), `RunHwyTernaryCmd`
   - These are the primary entry points for most hwy kernels.

   Rationale:
   - Encapsulates lane iteration and tail processing once.
   - The call sites only provide the per-lane math lambda, not the boilerplate.


Native integer runners: when they are valid
-------------------------------------------

Some operations are "scale-invariant" under OIIO's normalized integer model.
For example, for unsigned integer add:
- `(a/max + b/max)` in float space, then clamped to [0,1], then scaled by max
  matches saturated integer add `SaturatedAdd(a, b)` for the same bit depth.

For those cases, `imagebufalgo_hwy_pvt.h` provides:
- `RunHwyUnaryNativeInt<T>`
- `RunHwyBinaryNativeInt<T>`

These should only be used when all of the following are true:
- The operation is known to be scale-invariant under the normalization model.
- Input and output types are the same integral type.
- The operation does not depend on mixed types or float-range behavior.

Rationale:
- Avoids promotion/demotion overhead and can be materially faster.
- Must be opt-in and explicit, because many operations are NOT compatible with
  raw integer arithmetic (e.g. multiplication, division, pow).


Local pixel pointer helpers: reducing boilerplate safely
-------------------------------------------------------

Most hwy call sites need repeated pointer and stride computations:
- Pixel size in bytes.
- Scanline size in bytes.
- Base pointer to local pixels.
- Per-row pointer for a given ROI and scanline.
- Per-pixel pointer for non-contiguous fallbacks.

To centralize that, `imagebufalgo_hwy_pvt.h` defines:
- `HwyPixels(ImageBuf&)` and `HwyPixels(const ImageBuf&)`
  returning a small view (`HwyLocalPixelsView`) with:
  - base pointer (`std::byte*` / `const std::byte*`)
  - `pixel_bytes`, `scanline_bytes`
  - `xbegin`, `ybegin`, `nchannels`
- `RoiNChannels(roi)` for `roi.chend - roi.chbegin`
- `ChannelsContiguous<T>(view, nchannels)`:
  true only when the pixel stride exactly equals `nchannels * sizeof(T)`
- `PixelBase(view, x, y)`, `ChannelPtr<T>(view, x, y, ch)`
- `RoiRowPtr<T>(view, y, roi)` for the start of the ROI row at `roi.xbegin` and
  `roi.chbegin`.

Rationale:
- Avoids duplicating fragile byte-offset math across many ops.
- Makes it visually obvious what the code is doing: "get row pointer" vs
  "compute offset by hand."
- Makes non-contiguous fallback paths less error-prone by reusing the same
  pointer computations.

Important: these helpers are only valid for `ImageBuf` instances with local
pixels (`localpixels()` non-null). The call sites must check that before using
them.


Contiguous fast path vs non-contiguous fallback
-----------------------------------------------

Most operations implement two paths:

1) Contiguous fast path:
   - Used when pixels are tightly packed for the ROI's channel range.
   - The operation is executed as a 1D stream of length:
     `roi.width() * (roi.chend - roi.chbegin)`
   - Uses `RunHwy*Cmd` (or native-int runner) and benefits from:
     - fewer branches
     - fewer pointer computations
     - auto tail handling

2) Non-contiguous fallback:
   - Used when pixels have padding, unusual strides, or channel subsets that do
     not form a dense stream.
   - Typically loops pixel-by-pixel and channel-by-channel.
   - May still use the `ChannelPtr` helpers to compute correct addresses.

Rationale:
- The contiguous path is where SIMD delivers large gains.
- Trying to SIMD-optimize arbitrary strided layouts often increases complexity
  and risk for marginal benefit. Keeping a scalar fallback preserves
  correctness and maintainability.


How to add a new hwy kernel
---------------------------

Step 1: Choose the kernel shape
- Unary: `R = f(A)` -> use `RunHwyUnaryCmd`
- Binary: `R = f(A, B)` -> use `RunHwyCmd`
- Ternary: `R = f(A, B, C)` -> use `RunHwyTernaryCmd`

Step 2: Decide if a native-int fast path is valid
- Only for scale-invariant ops and same-type integral inputs/outputs.
- Use `RunHwyUnaryNativeInt` / `RunHwyBinaryNativeInt` when safe.
- Otherwise, always use the promote/demote runners.

Step 3: Implement the hwy body with a contig check
Typical structure inside `*_impl_hwy`:
- Acquire views once:
  - `auto Rv = HwyPixels(R);`
  - `auto Av = HwyPixels(A);` etc.
- In the parallel callback:
  - compute `nchannels = RoiNChannels(roi)`
  - compute `contig = ChannelsContiguous<...>(...)` for each image
  - for each scanline y:
    - `Rtype* r_row = RoiRowPtr<Rtype>(Rv, y, roi);`
    - `const Atype* a_row = RoiRowPtr<Atype>(Av, y, roi);` etc.
    - if contig: call `RunHwy*` with `n = roi.width() * nchannels`
    - else: fall back per pixel, per channel

Step 4: Keep the scalar path as the reference
- The scalar implementation should remain correct for all layouts and types.
- The hwy path should match scalar results for supported cases.


Design rationale summary
------------------------

This design intentionally separates concerns:
- Type conversion and normalization are centralized (`LoadPromote`,
  `DemoteStore`).
- SIMD lane iteration and tail handling are centralized (`RunHwy*` runners).
- Image address computations are centralized (`HwyPixels`, `RoiRowPtr`,
  `ChannelPtr`).
- Operation-specific code is reduced to short lambdas expressing the math.

This makes the hwy layer:
- Easier to maintain: fewer places to fix bugs when semantics change.
- Easier to extend: adding an op mostly means writing the math lambda and the
  dispatch glue.
- Safer: correctness for unusual layouts remains in scalar fallbacks.


Notes on `half`
---------------

The hwy conversion helpers handle `half` by converting through
`hwy::float16_t`. This currently assumes the underlying `half` representation
is compatible with how Highway loads/stores 16-bit floats.

If this assumption is revisited in the future, it should be changed as a
separate, explicit correctness/performance project.


<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->



# About

This KTX plugin support obviously nullifies the benefits of using KTX in the
first place (i.e., to reduce upload time to GPUs or totally eliminate the need for
transcoding to GPU-conformant format before uploading). That being said, this
plugin is still useful so that end users don't have to convert back and forth
between KTX <-> supported format (e.g., PNG). It is also useful to convert to
and from KTX2 format.

An important note about DDS -> KTX conversion:

 - [KTX-Software][libktx] will provide tools for lossless DDS to KTX conversion
   without having to decode then encode to KTX format. A PR is currently being
   worked on.
 - If you use OIIO for this conversion, then the quality will almost certainly
   degrade.

An example use-case would be Blender and its glTf import/export plugin.

Ideally, at some point in the future, OIIO may introduce a new API to
accommodate texture formats that are mainly used for fast texture uploads tow
GPUs. This is outside the scope of this basic format support addition.

Below you will find a set of notes on why this plugin is implemented the way
it is. It took me some time to understand how libktx works and what it provides
(and why). Some terminology is also defined here.

## KTX2 - Brief Introduction

KTX2 (the 2 here is to distinguish it from deprecated KTX/KTX1) is a binary
container format that is intended for usage for fast loading of textures to the
GPU. KTX2 contains GPU-native formats (e.g., block-compressed format BC7) with
an optional additional layer of compression (hereafter referred to as
*supercompression*).

As per the specs, KTX2 formats may store downsampled texture data for each mip
level (not necessarily the whole pyramid). This introduces problems for the
KTX2 writer (at `ktxoutput.cpp`) because the spec doesn't force the mention of
which filter/downsampler was used to create the mip levels.

## GPU Block Compression Formats

As opposed to compressed images, the term *compressed textures* usually refers
to GPU block-compressed textures. Compressed textures have the following
requirements:

 - Random access (to some degree, you still pay the price for decoding a very
 small number of neighboring pixels to access a given pixel).
 - Fixed-rate encoding (requirement for random access)
 - Support for hardware-decoding on the GPU (i.e., extremely fast to decode
 and results in better performance due to lower cache usage).

### BCn

All BCn formats encode a 4x4 block of pixels (could be 1 channel, or 2, or 3, or
4 depending on the particular format) into a fixed-size data (i.e., no
variable-rate encoding). BC7 is the go-to format on desktop hardware for LDR
textures. BC6HU/BC6HS is the go-to format on desktop hardware for HDR formats.

Microsoft has some fairly well-written explanation of each format. From the
perspective of OIIO, we just don't care since the work to decode/encode these
formats is offloaded to libktx.

### ASTC

libktx provides ASTC encoders/decoders and we don't have to deal with ASTC's
extreme complexity (e.g., there are many different block sizes).

### ETC2

libktx provides ETC2 encoders/decoders (have to double verify) but the
dependency has some weird licensing (afraid non-permissive as Mark @KTX-Software
pointed out). ETC formats are therefore not supported.

## KTX Supercompression

**supercompression**: a compression on top of another compression (i.e., layered
compression) for better disk storage/network transmission. Unlike GPU block
compression, supercompression has the flexibility to employ variable-rate
encoding. In this context, supercompression is employed on top of fixed-rate,
endpoint-compressed formats (like BCn, ASTC, etc.) that have hardware-decoding
support in commodity GPUs (of course, depends on GPU - mobile vs desktop, etc.).

Depending on the used Basis Universal codec (if any), supercompression may be
applied. **For ETC1S, supercompression must be used (usually BasisLZ)**. This is
the reason why you constantly see the notation "BasisLZ/ETC1S" which
*probably (have to verify)* reads: *BasisLZ over ETC1S*.

For UASTC, we *may* apply Zstandard supercompression (i.e., `KTX_SS_ZSTD`).

### KTS\_SS\_BASIS\_LZ (BasisLZ)

This is intended to be used to super-compress Basis Universal ETC1S format.
The expected workflow is as follows:

```
Basis LZ -> transcode to GPU format (e.g., block-compressed BC7)
```

For OIIO use-case, we can directly use libktx to transcode into raw bytes:

```
Basis LZ → transcode (using ktxTexture2_TranscodeBasis) → raw RGBA
```

The `ktxTexture2_TranscodeBasis` function provided by libktx can transcode
directly into raw RGBA values which is very handy. It however doesn't
provide/expose the functionality to just decode a single miplevel/subimage
(maybe this is simply not doable with Basis LZ - have to verify). Either way,
I might open a PR to provide single image/texture decoders.

## Supported Encoders/Decoders

- Supported/Tested texture kinds:
  - [ ] `SINGLE_TEXTURE_1D` (TODO)
  - [X] `SINGLE_TEXTURE_2D`
  - [ ] `SINGLE_TEXTURE_3D` (TODO)
  - [ ] `CUBEMAP_TEXTURE` (TODO)
  - [ ] `ARRAY_TEXTURE_1D` (TODO)
  - [ ] `ARRAY_TEXTURE_2D` (TODO)
  - [ ] `ARRAY_TEXTURE_3D` (not planned)
  - [ ] `ARRAY_TEXTURE_CUBEMAP` (not planned)

- Supported/Tested raw VkFormats (decoder + encoder):
  - [X] `VK_FORMAT_R8_UNORM`
  - [X] `VK_FORMAT_R8G8_SRGB`
  - [X] `VK_FORMAT_R8G8B8_SRGB`
  - [X] `VK_FORMAT_R8G8B8A8_SRGB`

- Block-compressed formats (decoder + encoder):
  - [X] ASTC
  - [ ] BCn (waiting on libktx BCn support PR merge)

- Basis Universal schemes (encoder + decoder):
  - [X] `UASTC`
  - [X] `ETC1S`

- Supercompression schemes (decompressor + compressor):
  - [X] `ZLIB`
  - [X] `ZSTD`

## Limitations

- If original KTX2 format contained generated mip maps, there is simply no way
to know which filter and its parameters that were used to regenerate these
mipmaps. To avoid any issues, we simply early quit (return false) in `open()`
if `get_int_attribute("ktx:miplevels") > 1`.

- KTX2 supports many GPU-block-compression encoders and each one may have many
different parameters that change the encoding quality (as usual, quality-speed
trade-off). To regenerate same input KTX2 format, we rely on the heuristic that
whatever created the original KTX2 input also supplied `KTXwriterScParams`
metadata field which should provide all non-default arguments provided to
`ktx create/encode` to create the texture.

- As stated in the comments in `ktxinput.cpp`, if given ktx texture is
supercompressed then it has to be all decompressed (i.e., NOT the decompression
of the underlying GPU texture format but rather just the supercompression). This
means that if you just need a particular subimage/miplevel, you pay the memory
price of loading the whole KTX texture (which might be very large for 3D
textures and texture arrays).

  - Per the specs:
  > Discussion: Should each mip level be supercompressed independently or should
  > the scheme, zlib, zstd, etc., be applied to all levels as a unit? The latter
  > may result in slightly smaller size though that is unclear. However it would
  > also mean levels could not be streamed or randomly accessed.
  >
  > Resolved: Yes. The benefits of streaming and random access outweigh what is
  > expected to be a small increase in size.

- KTX2 writer writes the whole texture (i.e., all subimages/mipmaps)  in the
`close()` function (i.e., when the ImageOutput object is destroyed or requested
to close). libktx does not provide a way to append or write subimages (is this
problematic or contrary to the way OIIO expects us to write files?).

- <s>KTX1 format is not yet supported. Adding support for it after finishing KTX2
*should be* relatively straightforward (Note: KTX1 is officially deprecated and
KTX-Software provides tools to convert from KTX1 to KTX2).</s> => support is not
planned for the moment.

- Only LDR formats (to be more precise, only TypeDesc::UINT8). Adding support
for HDR is straightforward (conversions for large number of enum values from
VkFormat have to be written).

## Dependencies

We only depend on libktx and nothing else. If CPU decoding/encoding of a format
is not supported by libktx, open a PR there that adds support to it. I tried the
approach of implementing formats here (e.g., BCn) and this results in extremely
harder to maintain and much more complex code here (see first commit with
12 000 changed lines).

[libktx][libktx]: for general KTX@ format support (loading of KTX2 files,
transcoding support, supercompression decompression support, etc.).

  - Commit hash: see `OpenImageIO/src/cmake/build_Ktx.cmake`
  - License: Many subresources.

## Resources

- [KTX2 Specs](https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html)
- [Official Implementation (KTX-Software)](https://github.com/KhronosGroup/KTX-Software)
- [Basis Universal Supercompression Implementation (used by libktx)](https://github.com/BinomialLLC/basis_universal)
- [Comparing-BCn-texture-decoders](https://aras-p.info/blog/2022/06/23/Comparing-BCn-texture-decoders/)

[libktx]: https://github.com/KhronosGroup/KTX-Software.git

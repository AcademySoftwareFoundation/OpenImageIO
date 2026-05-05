# About

This KTX plugin support obviously nulifies the benefits of using KTX in the
first place. That being said, this plugin is still useful so that end users
don't have to convert back and forth between KTX <-> supported format (e.g., PNG).
It is also useful to convert to and from KTX2 format.

An example usecase would be Blender and its glTf import/export plugin.

Ideally, at some point in the future, OIIO may introduce a new API to accomodate
texture formats that are mainly used for fast texture uploads to GPUs.

Below you will find a set of notes about why this plugin is implemented the way
it is. It took me some time to understand how libktx works and what it provides
(and why). Some terminology is also defined here.

## KTX2 - Brief Introduction

KTX2 (the 2 here is to distinguish it from deprecated KTX/KTX1) is a binary
container format that is intended for usage for fast loading of textures to the
GPU. KTX2 contains GPU-native formats (e.g., block compressed format BC7) with
an optional additional layer of compression (hereafter refered to as
*supercompression*).

As per the specs, KTX formats may store downsampled texture data for each mip
level (not necessarily the whole pyramid). This introduces problems for the
KTX2 writer (at `ktxoutput.cpp`) because

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
variable-rate encoding).

#### BC1/DXT1

64 bytes (4x4 block) => 8 bytes
alpha channed is encoded using 1 bit

### ASTC

TODO
Luckily libktx provides ASTC encoders/decoders and we don't have to deal with
ASTC's extreme complexity (e.g., there are many different block sizes).

## KTX Supercompression

**supercompression**: a compression on top of another compression (i.e., layered
compression) for better disk storage/network transmission. Unlike GPU block
compression, supercompression has the flexibilty to employ variable-rate
encoding. In this context, supercompression is employed on top of fixed-rate,
endpoint-compressed formats (like BCn, ETC2, etc.) that have hardware-decoding
support in commodity GPUs (of course, depends on GPU - mobile vs desktop, etc.).

Depending on the used Basis Universal codec (if any),
supercompression may be applied. **For ETC1S, supercompression must be used
(usually BasisLZ)**. This is the reason why you constantly see the notation
"BasisLZ/ETC1S" which *probably (have to verify)* reads: *BasisLZ over ETC1S*.

For UASTC, we *may* apply Zstandard supercompression (i.e., `KTX_SS_ZSTD`).

### KTS\_SS\_BASIS\_LZ (BasisLZ)

This is intended to be used to supercompress Basis Universal ETC1S format.
The expected workflow is as follows:

```
Basis LZ -> transcode to GPU format (e.g., block-compressed BC7)
```

For OIIO usecase, we can directly use libktx to transcode into raw bytes:

```
Basis LZ → transcode (using ktxTexture2_TranscodeBasis) → raw RGBA
```

The `ktxTexture2_TranscodeBasis` function provided by libktx can transcode
directly into raw RGBA values which is very handy. It however doesn't
provide/expose the functionality to just decode a single miplevel/subimage
(maybe this is simply not doable with Basis LZ - have to verify).

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

- Supported/Tested VkFormats:
  - [X] `VK_FORMAT_R8_UNORM`
  - [X] `VK_FORMAT_R8G8_SRGB`
  - [X] `VK_FORMAT_R8G8B8_SRGB`
  - [X] `VK_FORMAT_R8G8B8A8_SRGB`
  - [X] `VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK`
  - [X] `VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK`
  - [X] `VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK`
  - [X] `VK_FORMAT_BC1_RGB_SRGB_BLOCK`
  - [X] `VK_FORMAT_BC3_SRGB_BLOCK`
  - [X] `VK_FORMAT_BC4_UNORM_BLOCK`
  - [X] `VK_FORMAT_BC5_UNORM_BLOCK`
  - [X] `VK_FORMAT_BC7_SRGB_BLOCK`
  - [X] `VK_FORMAT_ASTC_4x4_SRGB_BLOCK`

- BCn GPU block-compressed formats:
  - [X] BC1 encoder/decoder
  - [ ] BC2 encoder/decoder (not implemented)
  - [X] BC3 encoder/decoder
  - [X] BC4 encoder/decoder
  - [X] BC5 encoder/decoder
  - [ ] BC6HS/BC6HU encoder/decoder (partially implemented but not tested)
  - [X] BC7 encoder/decoder

- ETC2 GPU block-compressed formats:
  - [X] `ETC2_RGB` (aka ETC1) decoder
  - [X] `ETC2_RGB_A1` decoder
  - [X] `ETC2_RGBA` decoder
  - [ ] ETC2 encoder (there are a few ETC2 decoders - including ConvectionKernels and etcpack)

- ASTC GPU block-compressed formats:
  - [X] `ASTC` decoder (using libktx's `ktxTexture2_DecodeAstc`)
  - [X] `ASTC` encoder (using libktx's `ktxTexture2_CompressAstc`)

- Basis Universal schemes:
  - [X] `UASTC` encoder/decoder
  - [X] `ETC1S` encoder/decoder

- Supercompression schemes:
  - [X] `ZLIB` decompressor/compressor (using libktx's `ktxTexture2_DeflateZLIB`)
  - [X] `ZSTD` decompressor/compressor (using libktx's `ktxTexture2_DeflateZstd`)

## Limitations

- If original KTX2 format contained generated mip maps, there is simply no way
to know which filter and its parameters that were used to regenerate these
mipmaps. To avoid any issues, we simply early quit (return false) in `open()`
if `get_int_attribute("ktx:miplevels") > 1`.

- KTX2 supports many GPU-block-compression encoders and each one may have many
different parameters that change the encoding quality (as usual, quality-speed
tradeoff). There is simply no way to regenerate the exact same input texture
without knowing these parameters and nor the KTX2 specs nor libktx nor
KTX-Software tooling stores any (or sufficient) information about these params
in the metadata.

- As stated in the comments in `ktxinput.cpp`, if given ktx texture is
supercompressed then it has to be all decompressed (i.e., NOT the decompression
of the underlying GPU texture format but rather the supercompression). This
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

- KTX1 format is not yet supported. Adding support for it after finishing KTX2
*should be* relatively straightforward (Note: KTX1 is officially deprecated and
KTX-Software provides tools to convert from KTX1 to KTX2).

- Only LDR formats (to be more precise, only TypeDesc::UINT8). Adding support
for HDR is straightforward (conversions for large number of enum values from
VkFormat have to be written).

- bc7enc\_rdo dependency for encoding/decoding *BC1-7* formats does not (
contrary to what the repo description suggests) support BC6HS/BC6HU HDR formats.
See below on what we can use for BC6HU/BC6HS encoding/decoding.

## Dependencies

[libktx][libktx]: for general KTX@ format support (loading of KTX2 files, transcoding
support, supercompression decompression support, etc.).

  - Commit hash: see `OpenImageIO/src/cmake/build_Ktx.cmake`
  - License: Many subresources. TODO: `lib/etcdec.cxx`'s license is not open
  source but libktx exposes a function to decode ETC formats, do we use it?

[bc7enc\_rdo][bc7enc]: for BC1, BC2, BC3, BC4, BC5 and BC7 decoding/encoding.

  - Commit hash:
    ```
    dbe416d28a5530b4e8cc45b14bf034dc6b96bbde
    ```
  - License: MIT License
  - Note: I am working on a PR to push `ktxTexture2_DecodeBcn` function
  to KTX-Software (libktx) so that we no longer have to include these
  dependencies here.

[etcdec][etcdec]: ETC2/EAC decoding.

  - Commit hash:
    ```
    972875d403ed8ac27e0f35c2f29d819e710a688a
    ```
  - License: MIT LICENSE
  - Note: libktx has etcunpack included by default. Maybe we can use it and
  remove this dependency?

Note: for BC6HS/BC6HU and ETC encoding support, we can use
[ConvectionKernels][ConvectionKernels]. It is better to have this dependency
built seperately (i.e., not copied into source ktx.imageio directory).

Personally, for ETC encoding, I would prefer to use ETCPACK since libktx already
uses its decoder and is the more *standard* choice (i.e., seems more official).

For BC6HU/BC6HS decoding, we can use the same dependency used by the DDS format
(bcdec.h). For encoding, we extract the needed function from DirectX Texture
Library (MIT license) or I write it myself.

## Resources

- [KTX2 Specs](https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html)
- [Official Implementation (KTX-Software)](https://github.com/KhronosGroup/KTX-Software)
- [Basis Universal Supercompression Implementation (used by libktx)](https://github.com/BinomialLLC/basis_universal)
- [BC1-7 encoders/decoders with RDO](https://github.com/richgel999/bc7enc_rdo)
- [Comparing-BCn-texture-decoders](https://aras-p.info/blog/2022/06/23/Comparing-BCn-texture-decoders/)
- [ConvectionKernels][ConvectionKernels]

[libktx]: https://github.com/KhronosGroup/KTX-Software.git
[bc7enc]: https://github.com/richgel999/bc7enc_rdo.git
[etcdec]: https://github.com/iOrange/etcdec.git
[ConvectionKernels]: https://github.com/elasota/ConvectionKernels.git

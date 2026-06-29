// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <csetjmp>
#include <cstdint>

// Per KTX-Software BUILDING.md:
//  > When linking to the static library, make sure to
//  > define `KHRONOS_STATIC` before including KTX header files.
//  > This is especially important on Windows.
#ifndef BUILD_SHARED_LIBS
#    define KHRONOS_STATIC 1
#endif

#include "ktx_pvt.h"
#include <ktx.h>
#include <optional>

OIIO_PLUGIN_NAMESPACE_BEGIN

class KtxInput final : public ImageInput {
public:
    KtxInput() {}

    ~KtxInput() override { close(); }

    const char* format_name(void) const override { return "ktx"; }

    int supports(string_view feature) const override
    {
        return (
            // as per the KTX1/2 specs:
            // https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_keyvalue_data
            feature == "arbitrary_metadata" ||
            /* ktx supports 3D textures, cubmap textures, texture arrays, etc. */
            feature == "multiimage" ||
            /* ktx supports storage of mipmaps */
            feature == "mipmap");
    }

    bool valid_file(Filesystem::IOProxy* ioproxy) const override;

    bool open(const std::string& name, ImageSpec& newspec) override;

    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;

    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;

    bool read_native_scanlines(int subimage, int miplevel, int ybegin, int yend,
                               int z, void* data) override;

    bool read_native_scanlines(int subimage, int miplevel, int ybegin, int yend,
                               span<std::byte> data) override;

    const std::string& filename() const { return m_filename; }

    bool close() override;

    int current_subimage(void) const override
    {
        lock_guard lock(*this);
        return m_subimage;
    }

    int current_miplevel(void) const override
    {
        lock_guard lock(*this);
        return m_miplevel;
    }

    bool seek_subimage(int subimage, int miplevel) override;

private:
    std::string m_filename;

    /// Buffer to hold the decoded GPU-block-compressed format. This is only
    /// used to hold BCn or ECT decompressed data for a particular
    /// miplevel/subimage.
    std::vector<uint8_t> m_buf;

    /// Non-owning pointer to KTX2 texture. The texture is managed by libktx
    /// and should be destroyed via a 'ktxTexture_Destroy()' call.
    ktxTexture* m_tex { nullptr };

    /// m_tex2 reinterpret_cast'ed to KtxTexture2* for convenience.
    ktxTexture2* m_tex2 { nullptr };

    /// Non-owning pointer to first byte of the requested (miplevel, slice).
    ///
    /// For non-GPU-compressed formats, this points to first byte of the whole
    /// texture data.
    ///
    /// For GPU-compressed formats:
    ///  - BCn: this is simply m_buf.data()
    ///  - ETC: this is simply m_buf.data()
    ///  - ASTC: this points to first byte of the whole decompressed texture
    uint8_t* m_data_ptr { nullptr };

    ktx_uint32_t m_pitch { 0 };  ///< Row pitch for current mip level.
    ktx_size_t m_offset { 0 };   ///< Current offset from subimage call.
    int m_subimage { -1 };       ///< What subimage are we looking at?
    int m_nbrsubimages { -1 };   ///< Number of slices/faces in texture
    int m_miplevel { -1 };       ///< What mip level are we looking at?
    int m_nbrmiplevels { -1 };   ///< Number of mip levels

    /// GPU block compression kind (only set in case of GPU-block-compressed KTX
    /// textures).
    BlockCompression m_cmp = BlockCompression::NONE;

    /// Original VkFormat (i.e., before applying any decompression or transcoding).
    VkFormat m_vkformat;

    std::unique_ptr<ImageSpec> m_config;  ///< Saved copy of configuration spec

    /// TODO: add gl, direct3d, and metal format support
    std::optional<KTXglFormat> m_glFormat { std::nullopt };
    std::optional<uint32_t> m_dxgiFormat { std::nullopt };
    std::optional<uint32_t> m_metalFormat { std::nullopt };

    /// Helper function: performs the actual pixel decoding.
    bool internal_readimg(unsigned char* dst, int w, int h, int d);

    bool ktx_magic_cmp(const uint8_t* KTX_MAGIC, const uint8_t* sig,
                       size_t start) const;

    TextureKind get_texture_kind() const;

    std::string get_colorspace() const;
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int ktx_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
ktx_imageio_library_version()
{
    return "ktx v5.0.0-rc1";
}  // hardcoded because I couldn't expose KTX_VERSION
OIIO_EXPORT ImageInput*
ktx_input_imageio_create()
{
    return new KtxInput;
}
OIIO_EXPORT const char* ktx_input_extensions[] = { "ktx2", nullptr };

OIIO_PLUGIN_EXPORTS_END



bool
KtxInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    //
    // OIIO API is limited for certain KTX texture types (e.g., 3D array
    // textures, cubemap array textures). Therefore we add the option to specify
    // which layer to use in case these textures are used. This is ignored for
    // other types of textures (e.g., 2D array textures, cubemaps, etc.)
    //
    // m_array_layer_idx = config.get_int_attribute("ktx:ArrayLayerIndex",
    //                                              m_array_layer_idx);

    // Check 'config' for any special requests
    // if (config.get_int_attribute("oiio:UnassociatedAlpha", 0) == 1)
    //     m_keep_unassociated_alpha = true;
    // m_linear_premult = config.get_int_attribute("png:linear_premult",
    //                                             OIIO::get_int_attribute(
    //                                                 "png:linear_premult"));
    ioproxy_retrieve_from_config(config);
    m_config.reset(new ImageSpec(config));  // save config spec
    return open(name, newspec);
}



/// Opens the file with given name and seek to the first subimage in the
/// file.  Various file attributes are put in `newspec` and a copy
/// is also saved internally to the `ImageInput` (retrievable via
/// `spec()`.  From examining `newspec` or `spec()`, you can
/// discern the resolution, if it's tiled, number of channels, native
/// data format, and other metadata about the image.
///
/// @param name
///         Filename to open, UTF-8 encoded.
///
/// @param newspec
///         Reference to an ImageSpec in which to deposit a full
///         description of the contents of the first subimage of the
///         file.
///
/// @returns
///         `true` if the file was found and opened successfully.
bool
KtxInput::open(const std::string& name, ImageSpec& newspec)
{
    m_filename = name;

    if (!ioproxy_use_or_open(name)) {
        errorfmt("ioproxy_use_or_open(\"{}\") failed", name);
        return false;
    }

    // If an IOProxy was passed, it had better be a File or a MemReader
    Filesystem::IOProxy* m_io = ioproxy();
    std::string proxytype     = m_io->proxytype();
    if (proxytype != "file" && proxytype != "memreader") {
        errorfmt("ktx reader can't handle proxy type {}", proxytype);
        return false;
    }

    // check if magic to insure that this is a KTX2 file
    if (!this->valid_file(m_io)) {
        // close_file();
        errorfmt("\"{}\" is not a KTX2 file, magic number doesn't match", name);
        return false;
    }

    //
    // IMPORTANT:
    //
    // KTX can hold layered compressions (i.e., on top of the potential GPU-
    // compatible compression like ASTC, the whole data can be furthermore
    // compressed using a super compression scheme).
    //
    // If KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT is provided for any
    // of the ktxTexture_CreateFrom* calls, then libktx will allocate an internal
    // buffer large enough to hold all data inflated IF AND ONLY IF
    // supercompressionScheme == KTX_SS_ZSTD or KTX_SS_ZLIB.
    //
    // Whithin the same call, ALL the texture data is then loaded. This is not
    // ideal especially when dealing with, for instance, 3D textures, or even
    // worse, 3D array textures.
    //
    // TODO:
    // Implementing the per-subimage allocation approach requires some effort.
    // For the moment, let's make sure this approach is working (i.e.,
    // all tests are passing) then let's profile and see what more experienced
    // users might say about this.
    //
    // For under-the-hood details, see official libktx repo:
    //  https://github.com/KhronosGroup/KTX-Software/blob/main/lib/src/texture.c
    //
    if (proxytype == "file") {
        auto fd  = reinterpret_cast<Filesystem::IOFile*>(m_io)->handle();
        auto res = ktxTexture_CreateFromStdioStream(
            fd, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &m_tex);
        if (KTX_SUCCESS != res) {
            errorfmt("Failed to create ktx texture using "
                     "ktxTexture_CreateFromStdioStream");
            return false;
        }
    } else /* (proxytype == "memreader") */ {
        OIIO_ASSERT(proxytype == "memreader");
        auto buff = reinterpret_cast<Filesystem::IOMemReader*>(m_io)->buffer();
        auto res  = ktxTexture_CreateFromMemory(
            buff.data(), buff.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &m_tex);
        if (KTX_SUCCESS != res) {
            errorfmt(
                "Failed to create ktx texture using ktxTexture_CreateFromMemory");
            return false;
        }
    }

    m_tex2         = reinterpret_cast<ktxTexture2*>(m_tex);
    m_nbrmiplevels = m_tex2->numLevels;
    m_nbrsubimages = m_tex2->numFaces;

    m_spec       = ImageSpec(m_tex2->baseWidth, m_tex2->baseHeight,
                             4 /* dummy value - will be overwritten */);
    m_spec.depth = m_spec.full_depth = m_tex2->baseDepth;
    std::string colorspace           = get_colorspace();
    m_spec.set_colorspace(colorspace);

    //
    // Make sure to save everything that is needed to recreate this exact same
    // KTX texture from OIIO API (i.e., fields of `ktxTextureCreateInfo`).
    //
    // Note:
    // KtxTexture fields may change after some libktx calls that take
    // KtxTexture* argument because they may potentially modify the texture
    // (e.g., in ktxTexture2_DecodeAstc supercompressionScheme is overwritten to
    // none, ktxTexture2_TranscodeBasis overwrites texture format, etc.).
    //
    // TODO: save original supercompressionScheme BEFORE infalting the texture
    m_spec.extra_attribs.attribute("ktx:supercompressionscheme",
                                   TypeDesc::UINT32, 1,
                                   cspan<uint32_t>(
                                       m_tex2->supercompressionScheme));

    m_spec.extra_attribs.attribute("ktx:texturekind", TypeDesc::UINT32, 1,
                                   cspan<uint32_t>(static_cast<uint32_t>(
                                       get_texture_kind())));

    // save as string
    m_spec.extra_attribs.attribute("ktx:version", "2.0");

    // Contrary to the specs' layerCount, numLayers is always >= 1 (even for
    // non-array types)
    m_spec.extra_attribs.attribute("ktx:nlayers", TypeDesc::UINT32, 1,
                                   cspan<uint32_t>(m_tex->numLayers));

    m_spec.extra_attribs.attribute("ktx:miplevels", TypeDesc::UINT32, 1,
                                   cspan<uint32_t>(m_tex->numLevels));

    m_spec.extra_attribs.attribute("ktx:generatemipmaps", TypeDesc::UINT8, 1,
                                   cspan<uint8_t>(m_tex->generateMipmaps));

    // Store colormodel so that if a KTX2 is requested to be generated, we know
    // if a Basis Universal scheme has to be applied.
    m_spec.extra_attribs.attribute("ktx:colormodel", TypeDesc::UINT32, 1,
                                   cspan<uint32_t>(
                                       ktxTexture2_GetColorModel_e(m_tex2)));

    // m_spec.extra_attribs.attribute("ktx:transferfunction", TypeDesc::UINT32, 1,
    //                                cspan<uint32_t>(transfer_function));

    // TODO: do we actually need the dfd data to re-generate the same KTX2 file?
    // uint32_t dfdTotalSize = *m_tex2->pDfd;
    // m_spec.extra_attribs.attribute("ktx:dfd", TypeDesc::UINT8, dfdTotalSize,
    //                                make_cspan(reinterpret_cast<const uint8_t*>(
    //                                               m_tex2->pDfd),
    //                                           dfdTotalSize));

    //
    // Save arbitrary metadata. KTX allows for the storage of arbitrary
    // key/value metadata pairs as per the specification here:
    //  https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_keyvalue_data
    //
    // KTX2 spec. defines a predifined set of key/value metadata at
    //  https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_predefined_keyvalue_pairs
    //
    // Predifined keys we care about:
    //
    //  - KTXcubemapIncomplete: 1 byte bitfield
    //  - KTXorientation: null-terminated string
    //
    //  - KTXglFormat:
    //    + UInt32 glInternalformat
    //    + UInt32 glFormat
    //    + UInt32 glType
    //
    //  - KTXdxgiFormat__: UInt32
    //  - KTXmetalPixelFormat: UInt32
    //
    auto kventry = m_tex->kvDataHead;
    if (kventry)
        do {
            auto status = KTX_SUCCESS;
            unsigned int keylen { 0 };
            unsigned int vallen { 0 };
            char* key { nullptr };
            void* val { nullptr };

            if ((status = ktxHashListEntry_GetKey(kventry, &keylen, &key))
                != KTX_SUCCESS)
                continue;

            // "The key must be terminated by a NUL character"
            // This will probably never occur, but it doesn't hurt to be safe
            if (keylen <= 1)
                continue;

            if ((status = ktxHashListEntry_GetValue(kventry, &vallen, &val))
                != KTX_SUCCESS)
                continue;

            // vallen checks are done below depending on the attribute name

            auto attr_name              = std::string(key, key + (keylen - 1));
            auto ktx_prefixed_attr_name = fmt::format("ktx:{}", attr_name);

            if (attr_name == KTX_WRITER_KEY) {
                // KTXwriter identifies the program used to write this KTX file
                // Should be NUL terminated.
                if (vallen <= 1)
                    continue;
                auto char_ptr = reinterpret_cast<const char*>(val);
                m_spec.extra_attribs.attribute(
                    ktx_prefixed_attr_name,
                    std::string(char_ptr, char_ptr + (vallen - 1)));
            } else if (attr_name == "KTXcubemapIncomplete") {
                OIIO_ASSERT(vallen == 1);
                // TODO: handle KTXcubemapIncomplete
            } else if (attr_name == KTX_ORIENTATION_KEY) {
                //
                // KTX may define a different orientation than the one used by OIIO. See:
                //  https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_ktxorientation
                // E.g., for KTX1 (OpenGL) without any re-orientation logic images are
                // flipped over X axis (top becomes down).
                //

                // TODO: set orientation functions
            } else if (attr_name == "KTXglFormat") {
                OIIO_ASSERT(vallen == sizeof(KTXglFormat) /* 12 bytes */);
                KTXglFormat glFormat;
                glFormat.glInternalformat = *reinterpret_cast<uint32_t*>(val);
                glFormat.glFormat = *(reinterpret_cast<uint32_t*>(val) + 1);
                glFormat.glType   = *(reinterpret_cast<uint32_t*>(val) + 2);
                m_glFormat        = glFormat;
                m_spec.extra_attribs.attribute(
                    ktx_prefixed_attr_name, TypeDesc::UINT32, 3,
                    make_cspan(reinterpret_cast<const uint32_t*>(val), 3));
            } else if (attr_name == "KTXdxgiFormat__") {
                OIIO_ASSERT(vallen == sizeof(uint32_t));
                m_dxgiFormat = *reinterpret_cast<uint32_t*>(val);
                m_spec.extra_attribs.attribute(ktx_prefixed_attr_name,
                                               m_dxgiFormat.value());
            } else if (attr_name == "KTXmetalPixelFormat") {
                OIIO_ASSERT(vallen == sizeof(uint32_t));
                m_metalFormat = *reinterpret_cast<uint32_t*>(val);
                m_spec.extra_attribs.attribute(ktx_prefixed_attr_name,
                                               m_metalFormat.value());
            } else {
                // otherwise store the arbitrary value as a byte string
                m_spec.extra_attribs.attribute(
                    ktx_prefixed_attr_name, TypeDesc::UCHAR, vallen,
                    make_cspan(reinterpret_cast<const uint8_t*>(val), vallen));
            }

        } while ((kventry = ktxHashList_Next(kventry)));

    //
    // We only support KTX_SS_NONE, KTX_SS_ZLIB, KTX_SS_ZSTD, and
    // KTX_SS_BASIS_LZ supercompression schemes. New schemes may be added to the
    // spec hence why we do a strict if check.
    //
    if (m_tex2->supercompressionScheme != KTX_SS_NONE
        && m_tex2->supercompressionScheme != KTX_SS_ZSTD
        && m_tex2->supercompressionScheme != KTX_SS_ZLIB
        && m_tex2->supercompressionScheme != KTX_SS_BASIS_LZ) {
        // vendor-specific or newly introduced supercompression schemes (not
        // supported)
        errorfmt("unsuppoted supercompression scheme: {}",
                 static_cast<uint32_t>(m_tex2->supercompressionScheme));
        return false;
    }

    //
    // Store original VkFormat (i.e., after Basic Universal transcoding and
    // before potential GPU block format decompression).
    //
    // Important:
    // Call this BEFORE (potential) ktxTexture2_TranscodeBasis call
    //
    m_spec.extra_attribs.attribute("ktx:vkformat", TypeDesc::UINT32, 1,
                                   cspan<uint32_t>(static_cast<uint32_t>(
                                       m_tex2->vkFormat)));

    //
    // Do we need to transcode this texture (i.e., is this a Basis Universal
    // texture format)?
    //
    // KTX2 provides transcoders that can directly target raw, uncompressed
    // pixels (via the KTX_TTF_RGBA32 flag).
    //
    // Important:
    // This modifies the KtxTexture2 (m_tex) therefore make sure to save
    // essential properties for proper KTX2 regeneration.
    //
    if (ktxTexture2_NeedsTranscoding(m_tex2)) {
        if (auto status = ktxTexture2_TranscodeBasis(
                m_tex2, ktx_transcode_fmt_e::KTX_TTF_RGBA32, 0);
            status != KTX_SUCCESS) {
            errorfmt("failed to transcode KTX2 texture to raw pixels. "
                     "ktxTexture2_TranscodeBasis returned Ktx error code: {}",
                     static_cast<uint32_t>(status));
            return false;
        }
    }

    //
    // This could mean one of the following as per the specs at:
    // https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_use_of_vk_format_undefined
    //
    //  1.  For custom formats that do not have any equivalent in GPU APIs.
    //      This is currently not supported.
    //
    //  2.  ETC1S/UASTC supercompression scheme: makes no sense since we
    //      transcoded it above to uncompressed format.
    //
    //  3.  For any formats from any GPU APIs that do not have Vulkan
    //      equivalents. E.g., OpenGL/Direct3D/Metal formats.
    //      In this case, one of the following metadata entries have to be
    //      present:
    //      - "KTXglFormat" for OpenGL
    //      - "KTXdxgiFormat__" for Direct3D
    //      - "KTXmetalPixelFormat" for Metal
    //      TODO
    //
    //  4.  Compressed color models in Section 5.6 of [KDF14] or successors that
    //      do not have corresponding Vulkan formats.
    //      TODO
    //
    if (m_tex2->vkFormat == VK_FORMAT_UNDEFINED) {
        // TODO: check case (4) - color model

        // check case (3) - non-Vulkan GPU formats (here we simply map these
        // formats to VkFormat and call it a day)
        if (m_glFormat.has_value()) {
            // TODO: add glformat support
            errorfmt("Loading KTX textures with OpenGL formats but no vkFormat "
                     "(i.e., VK_FORMAT_UNDEFINED) is currently not supported");
            return false;
        } else if (m_dxgiFormat.has_value()) {
            // TODO: add direct3d format support
            errorfmt(
                "Loading KTX textures with Direct3D formats but no vkFormat "
                "(i.e., VK_FORMAT_UNDEFINED) is currently not supported");
            return false;
        } else if (m_metalFormat.has_value()) {
            // TODO: add metal format support
            errorfmt("Loading KTX textures with Metal formats but no vkFormat "
                     "(i.e., VK_FORMAT_UNDEFINED) is currently not supported");
            return false;
        }

        // error for other cases (case (2) should not occur and case (1) is
        // not supported)
        errorfmt(
            "VkFormat of provided KTX texture is VK_FORMAT_UNDEFINED "
            "which potentially means that a custom format with no equivalent "
            "in GPU APIs is provided. This is not supported.");
        return false;
    }

    //
    // In case this KTX texture is GPU block compressed, we need to map its
    // vkformat to the corresponding decompressed VkFormat.
    //
    //  E.g., VK_FORMAT_BC7_SRGB_BLOCK --> VK_FORMAT_R8G8B8A8_SRGB
    //
    // We do this so that we can save the correct crucial stats in the spec
    // (e.g., nchannels, colorspace, typedesc, etc.) and because the internal
    // state of data in OIIO is always decompressed (i.e., we never return
    // block-compressed data from read_native_scanline(s) functions).
    //
    auto format = static_cast<VkFormat>(m_tex2->vkFormat);
    if (m_tex2->isCompressed) {
        FormatInfo format_info;
        if (!get_info_from_vkformat(static_cast<VkFormat>(format),
                                    format_info)) {
            errorfmt(
                "Failed to extract info (e.g., nchannels, typedesc, etc.) from VkFormat: {}",
                static_cast<uint32_t>(format));
            return false;
        }
        if (format_info.compression == BlockCompression::NONE
            || format_info.decompressed_format == VK_FORMAT_UNDEFINED) {
            errorfmt(
                "KTX texture is GPU-block-compressed using unsuppoted format: {}",
                static_cast<uint32_t>(format));
            return false;
        }
        format = format_info.decompressed_format;
        m_cmp  = format_info.compression;
    }

    //
    // Important:
    // Call this AFTER transcoding the basis universal scheme (i.e., after
    // ktxTexture2_TranscodeBasis) and AFTER detecting which GPU block
    // compression scheme is used.
    //
    {
        FormatInfo format_info;
        if (!get_info_from_vkformat(static_cast<VkFormat>(format),
                                    format_info)) {
            errorfmt(
                "Failed to extract info (e.g., nchannels, typedesc, etc.) from VkFormat: {}",
                static_cast<uint32_t>(format));
            return false;
        }

        m_spec.set_format(format_info.typedesc);
        m_spec.nchannels = format_info.nbrchannels;
    }

    // TODO: verify the x, y, z limits (probably not 65535)
    if (!check_open(m_spec, { 0, 65535, 0, 65535, 0, 65535, 0, 4 }))
        return false;

    if (!seek_subimage(0, 0))
        // errorfmt is set via seek_subimage
        return false;

    newspec = m_spec;
    return true;
}



bool
KtxInput::close()
{
    // Check if already closed
    if (!ioproxy_opened())
        return true;
    if (m_tex) {
        ktxTexture_Destroy(m_tex);
        m_tex = nullptr;
    }
    ioproxy_clear();
    return true;
};



//
// In the context of KTX, `subimage` CAN be interpreted as (1D textures are
// considered 2D textures with height set to 1):
//  1. array layer (if texture is a 2D texture array)
//  2. depth slice (if texture is 3D)
//  3. cube map face (if texture is a cubemap)
//  4. depth slice (of first 3D texture if texture is a 3D texture array)
//  5. cube map face (of first cubemap texture if texture is a cubmap array)
//
// `miplevel` is simply interpreted as a mip level of the above `subimage`.
//
// In other cases, if subimage is > 0, it is invalid.
//
bool
KtxInput::seek_subimage(int subimage, int miplevel)
{
    lock_guard lock(*this);

    //
    // Before doing any calls, check if provided subimage and mip lvl are valid.
    // This is how OIIO figures out the number of subimages/miplevels.
    //
    if (subimage < 0 || miplevel < 0 || subimage >= m_nbrsubimages
        || miplevel >= m_nbrmiplevels)
        /* don't errorfmt here */
        return false;

    // if same subimage and miplevel as current => early out
    if (this->current_subimage() == subimage
        && this->current_miplevel() == miplevel)
        return true;

    m_subimage = subimage;
    m_miplevel = miplevel;

    // cast to ktx_uint32_t to stop the compiler/clangd from complaining
    auto _subimage = static_cast<ktx_uint32_t>(subimage);

    ktx_uint32_t arr_layer { 0 };   // array layer
    ktx_uint32_t face_slice { 0 };  // 3d texture slice or cubemap face

    // is this a cubemap? (i.e., subimage means cubemap face)
    if (m_tex->isCubemap)
        face_slice = _subimage;

    // is this an array texture? (i.e., subimage means array layer)
    if (m_tex->isArray)
        arr_layer = _subimage;

    // is this a 3D texture? (i.e., subimage means face slice)
    if (m_tex->numDimensions == 3)
        face_slice = _subimage;

    //
    // According to official libktx source code, this is how they compute
    // dimensions of a miplevel. See:
    //  https://github.com/KhronosGroup/KTX-Software/lib/src/texture.c
    //
    const size_t width  = std::max(m_tex2->baseWidth >> miplevel, 1u);
    const size_t height = std::max(m_tex2->baseHeight >> miplevel, 1u);
    const size_t depth  = std::max(m_tex2->baseDepth >> miplevel, 1u);

    m_spec.width  = width;
    m_spec.height = height;
    m_spec.depth  = depth;

    //
    // Decode GPU-compression if any. Supported formats:
    //
    //    ASTC: libktx provides decoders for ASTC block compression via the
    //          ktxTexture2_DecodeAstc call.
    //          This currently decodes the whole texture (all miplevels, all
    //          slices, etc.) into memory.
    //          TODO: wait for my PR in libktx to implement decode_astc for
    //                per-miplvl/subimage decoding.
    //
    //    BCn:  libktx will provide decoders/encoders for BCn block compression
    //          via ktxTexture2_DecodeBCn. TODO: wait for my RP in libktx to get
    //          merged then add BCn support.
    //
    //    ETC2: TODO: some licensing clarification is needed from the part of
    //                etcunpack usage in libktx.
    //
    //    PVRTC: TODO: wait for libktx PR.
    //
    if (m_tex2->isCompressed /* i.e., is GPU block compressed? */) {
        ktx_size_t offset;
        if (auto status = ktxTexture2_GetImageOffset(m_tex2, miplevel,
                                                     arr_layer, face_slice,
                                                     &offset);
            status != KTX_SUCCESS) {
            return status;
        }
        // TODO: Are pointer indices [offset, offset + size[ safe?
        // Encoded blocks
        cspan<uint8_t> src_span(m_tex2->pData + offset,
                                ktxTexture_GetImageSize(ktxTexture(m_tex),
                                                        miplevel));

        switch (m_cmp) {
            /* BCn LDR formats */
        case BlockCompression::BC1:
        case BlockCompression::BC2:
        case BlockCompression::BC3:
        case BlockCompression::BC4:
        case BlockCompression::BC5:
        case BlockCompression::BC7:
            //
            // TODO: wait for my PR in libktx to be merged
            //
            // Note:
            // ktxTexture2_DecodeBCn internally creates a new ktxTexture2 texture
            // and populates it with decoded data from the originally provided
            // texture. At the end, it moves the decoded data to m_tex and
            // destroys the temporarily created texture.
            //
            // This operation is expensive (both in memory and CPU cycles).
            // After this, m_tex2->isCompressed will be false => this will only
            // be called once.
            //
            // if (auto status = ktxTexture2_DecodeBCn(m_tex2);
            //     status != KTX_SUCCESS) {
            //     errorfmt("failed to decode BCn-compressed texture. "
            //              "ktxTexture2_DecodeBCn returned Ktx error code: {}",
            //              static_cast<uint32_t>(status));
            //     return false;
            // }
            // break;
            return false;

            /* BCn HDR formats - TODO */
        case BlockCompression::BC6HU:
        case BlockCompression::BC6HS:
            return false;

            /* ETC formats */
        case BlockCompression::ETC2_RGB:
        case BlockCompression::ETC2_RGB_A1:
        case BlockCompression::ETC2_RGBA:
            return false;

            /* ASTC formats */
        case BlockCompression::ASTC:
            //
            // Note:
            // ktxTexture2_DecodeAstc internally creates a new ktxTexture2 texture
            // and populates it with decoded data from the originally provided
            // texture. At the end, it moves the decoded data to m_tex and
            // destroys the temporarily created texture.
            //
            // This operation is expensive (both in memory and CPU cycles).
            // After this, m_tex2->isCompressed will be false => this will only
            // be called once.
            //
            if (auto status = ktxTexture2_DecodeAstc(m_tex2);
                status != KTX_SUCCESS) {
                errorfmt("failed to decode ASTC-compressed texture. "
                         "ktxTexture2_DecodeAstc returned Ktx error code: {}",
                         static_cast<uint32_t>(status));
                return false;
            }
            break;

        default:
            errorfmt("Unknown/unsupported GPU block compression kind: {}",
                     static_cast<uint32_t>(m_cmp));
            return false;
        }

        m_pitch = width * m_spec.nchannels
                  * m_spec.format.size() /* 1 for LDR, 2 for HDR formats */;
        m_data_ptr = m_buf.data();
    }

    // Do NOT change this to `else` statement because this handles the ASTC and
    // BCn cases above (which, again, sets m_tex2->isCompressed to `false`)
    if (!m_tex2->isCompressed) {
        //
        // GetImageOffset implements internal checks depending on texture kind (e.g.,
        // 3D, cubemap, etc.) and incase of invalid input, KTX_INVALID_OPERATION is
        // returned.
        //
        ktx_size_t offset;
        if (auto status = ktxTexture_GetImageOffset(m_tex, miplevel, arr_layer,
                                                    face_slice, &offset);
            status != KTX_SUCCESS) {
            errorfmt("ktxTexture_GetImageOffset failed with exit code: {}",
                     static_cast<uint32_t>(status));
            return false;
        }
        m_pitch    = ktxTexture_GetRowPitch(m_tex, miplevel);
        m_data_ptr = m_tex2->pData + offset;
    }
    return true;
}



bool
KtxInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    lock_guard lock(*this);
    return read_native_scanlines(subimage, miplevel, y, y + 1,
                                 as_writable_bytes(data, m_spec.scanline_bytes(
                                                             true)));
}



bool
KtxInput::read_native_scanlines(int subimage, int miplevel, int ybegin,
                                int yend, int /* z */, void* data)
{
    lock_guard lock(*this);

    if (ybegin >= yend) {
        errorfmt("Invalid scanline range requested: {}-{}", ybegin, yend);
        return false;
    }

    // avoid calling seek_subimage because this will NOT be thread-safe and
    // we have to introduce a lock which will make this slower (read note above
    // about how libktx inflates all data in open()).
    if (!seek_subimage(subimage, miplevel))
        return false;

    size_t size = m_spec.scanline_bytes(true) * size_t(yend - ybegin);
    return read_native_scanlines(subimage, miplevel, ybegin, yend,
                                 as_writable_bytes(data, size));
}


bool
KtxInput::read_native_scanlines(int subimage, int miplevel, int ybegin,
                                int yend, span<std::byte> data)
{
    lock_guard lock(*this);
    // is provided [ybegin, yend[ valid?
    if (ybegin < 0 || ybegin >= yend || yend > m_spec.height) {
        // out of range scanlines
        errorfmt("KTX read_native_scanlines: Out of valid range scanline indices "
                 "(b={} e={}).",
                 ybegin, yend);
        return false;
    }

    // can the provided span hold the requested scanlines?
    if (!valid_raw_span_size(data, m_spec, 0, m_spec.width, ybegin, yend))
        // errorfmt is set within valid_raw_span_size
        return false;

    // since miplevel is valid => get number of bytes in a row for this mip
    memcpy(data.data(), m_data_ptr, m_pitch * (yend - ybegin));
    // std::cout << "read_native_scanlines(" << subimage << ", " << miplevel
    //           << ", " << ybegin << ", " << yend << ")" << '\n';
    return true;
}


bool
OpenImageIO::KtxInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    // Check magic number to assure this is a KTX2 file
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Read)
        return false;

    // per KTX2 specs: the first 12 bytes of a KTX2 file are used to identify it
    uint8_t magic[12] {};
    const size_t numRead = ioproxy->pread(magic, sizeof(magic), 0);

    return (numRead == sizeof(magic))
           && this->ktx_magic_cmp(KTX2_IDENTIFIER, magic, 0);
}


bool
KtxInput::ktx_magic_cmp(const uint8_t* KTX_MAGIC, const uint8_t* sig,
                        size_t start) const
{
    for (size_t i = start; (i - start) < sizeof(KTX_MAGIC); ++i)
        if (sig[i] != KTX_MAGIC[i])
            return false;
    return true;
}


TextureKind
KtxInput::get_texture_kind() const
{
    switch (m_tex->numDimensions) {
    case 1:
        if (m_tex->isArray)
            return TextureKind::ARRAY_TEXTURE_1D;
        return TextureKind::SINGLE_TEXTURE_1D;
    case 2:
        if (m_tex->isArray && m_tex->isCubemap)
            return TextureKind::ARRAY_TEXTURE_CUBEMAP;
        else if (m_tex->isArray)
            return TextureKind::ARRAY_TEXTURE_2D;
        return TextureKind::SINGLE_TEXTURE_2D;
    case 3:
        if (m_tex->isArray)
            return TextureKind::ARRAY_TEXTURE_3D;
        return TextureKind::SINGLE_TEXTURE_3D;
    default: return TextureKind::SINGLE_TEXTURE_2D;
    }
}


std::string
KtxInput::get_colorspace() const
{
    // for set of, see:
    //  https://github.com/KhronosGroup/KTX-Software/blob/main/external/dfdutils/KHR/khr_df.h
    // for OIIO colorspaces, see:
    //  https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/src/libOpenImageIO/color_ocio.cpp
    khr_df_transfer_e transfer_function = ktxTexture2_GetTransferFunction_e(
        m_tex2);
    khr_df_primaries_e primaries = ktxTexture2_GetPrimaries_e(m_tex2);
    // std::cout << "tf: " << transfer_function << "; primaries: " << primaries
    //           << '\n';
    switch (transfer_function) {
    case KHR_DF_TRANSFER_SRGB:
        switch (primaries) {
        case KHR_DF_PRIMARIES_BT709: return "srgb_rec709_scene";
        default: break;
        }
        break;
    case KHR_DF_TRANSFER_LINEAR:
        switch (primaries) {
        case KHR_DF_PRIMARIES_BT709: return "lin_rec709_scene";
        default: break;
        }
        break;
    // case KHR_DF_TRANSFER_DCIP3: colorspace = "lin_rec709_scene"; return true;
    default: break;
    }
    // TODO: need to generate test files before adding support for any other
    // colorspaces
    return "unknown";
}

OIIO_PLUGIN_NAMESPACE_END

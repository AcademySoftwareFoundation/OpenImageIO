// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "ktx_pvt.h"
#include <charconv>
#include <optional>
#include <regex>

OIIO_PLUGIN_NAMESPACE_BEGIN

class KtxInput final : public ImageInput {
public:
    KtxInput() {}

    ~KtxInput() override { close(); }

    const char* format_name(void) const override { return "ktx"; }

    int supports(string_view feature) const override
    {
        return (
            feature == "ioproxy" ||
            // as per the KTX1/2 specs:
            // https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_keyvalue_data
            feature == "arbitrary_metadata" ||
            // KTX2 supports 2D texture arrays, 3D texture arrays, and cubemap
            // arrays. That being said, 2D texture arrays is the only one
            // supported by this OIIO plugin.
            feature == "multiimage" ||
            // KTX2 supports mipmaps. 3D texture mipmaps are treated as a
            // per-slice mipmap.
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

    // TODO: why there is no `read_native_scanlines` that takes a span<std::byte>
    // but also a `z` slice index (same as unsafe `read_native_scanlines`)?
    // bool read_native_scanlines(int subimage, int miplevel, int ybegin, int yend,
    //                            span<std::byte> data) override;

    const std::string& filename() const { return m_filename; }

    bool close() override;

    int current_subimage(void) const override { return m_subimage; }

    int current_miplevel(void) const override { return m_miplevel; }

    bool seek_subimage(int subimage, int miplevel) override;

private:
    std::string m_filename;

    /// KTX2 texture.
    std::unique_ptr<ktxTexture2, decltype(ktxTexture2_Destroy)*> m_tex {
        nullptr, ktxTexture2_Destroy
    };

    int m_subimage { -1 };  ///< What subimage are we looking at. This is not
                            ///< used anywhere else except in current_subimage()

    int m_miplevel { -1 };  ///< What mip level are we looking at. This is not
                            ///< used anywhere else except in current_miplevel()

    /// GPU block compression kind (only set in case of GPU-block-compressed KTX
    /// textures).
    BlockCompression m_cmp { BlockCompression::NONE };

    /// Original VkFormat (i.e., before applying any decompression or transcoding).
    VkFormat m_vkformat { VK_FORMAT_UNDEFINED };

    std::unique_ptr<ImageSpec> m_config;  ///< Saved copy of configuration spec

    /// TODO: add gl, direct3d, and metal format support
    std::optional<KTXglFormat> m_glFormat { std::nullopt };
    std::optional<uint32_t> m_dxgiFormat { std::nullopt };
    std::optional<uint32_t> m_metalFormat { std::nullopt };

    /// Helper function: performs the actual pixel decoding.
    bool internal_readimg(unsigned char* dst, int w, int h, int d);

    /// Checks the magic
    bool ktx_magic_cmp(const uint8_t* sig, size_t start) const;

    std::string get_colorspace() const;

    void parse_ktx_sc_params_metadata(std::string_view ktx_sc_params);

    bool check(int subimage, int miplevel) const;
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int ktx_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
ktx_imageio_library_version()
{
    return "ktx v" OIIO_STRINGIZE(Ktx_VERSION_MAJOR) "." OIIO_STRINGIZE(
        Ktx_VERSION_MINOR) "." OIIO_STRINGIZE(Ktx_VERSION_PATCH);
}
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

    if (!ioproxy_use_or_open(name))
        return false;

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
    // KTX can hold layered compressions (i.e., on top of the potential GPU-
    // compatible compression like ASTC, the whole data can be furthermore
    // compressed using a super compression scheme). We call such compression
    // `supercompression` and does NOT refer to the usual compression (e.g.,
    // GPU-block compression).
    //
    // If KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT is provided for any
    // of the ktxTexture_CreateFrom* calls, then libktx will allocate an
    // internal buffer large enough to hold all data inflated IF AND ONLY IF
    // supercompressionScheme == KTX_SS_ZSTD or KTX_SS_ZLIB.
    //
    // Whithin the same call, ALL the texture data is then loaded. This is not
    // ideal especially when dealing with, for instance, 3D textures, or even
    // worse, 3D array textures.
    //
    // TODO:
    // Implementing the per-subimage allocation approach requires some effort.
    // For the moment, let's make sure this approach is working (i.e., all tests
    // are passing).
    //
    // For under-the-hood details, see official libktx repo:
    // https://github.com/KhronosGroup/KTX-Software/blob/main/lib/src/texture.c
    //
    if (proxytype == "file") {
        FILE* fd = reinterpret_cast<Filesystem::IOFile*>(m_io)->handle();
        ktxTexture2* p_tex = nullptr;
        auto res
            = ktxTexture2_CreateFromStdioStream(fd, KTX_TEXTURE_CREATE_NO_FLAGS,
                                                &p_tex);
        m_tex.reset(p_tex);
        if (KTX_SUCCESS != res) {
            errorfmt("Failed to create ktx texture using "
                     "ktxTexture_CreateFromStdioStream");
            return false;
        }
    } else /* (proxytype == "memreader") */ {
        OIIO_ASSERT(proxytype == "memreader");
        auto buff = reinterpret_cast<Filesystem::IOMemReader*>(m_io)->buffer();
        ktxTexture2* p_tex = nullptr;
        auto res = ktxTexture2_CreateFromMemory(buff.data(), buff.size(),
                                                KTX_TEXTURE_CREATE_NO_FLAGS,
                                                &p_tex);
        m_tex.reset(p_tex);
        if (KTX_SUCCESS != res) {
            errorfmt(
                "Failed to create ktx texture using ktxTexture_CreateFromMemory");
            return false;
        }
    }

    if (m_tex->isArray && m_tex->numDimensions == 3) {
        errorfmt("3D texture arrays are not supported");
        return false;
    }

    if (m_tex->isArray && m_tex->numFaces > 1) {
        errorfmt("Cubemap texture arrays are not supported");
        return false;
    }

    m_spec       = ImageSpec(m_tex->baseWidth, m_tex->baseHeight,
                             4 /* dummy value - will be overwritten */,
                             TypeDesc::UINT8);
    m_spec.depth = m_spec.full_depth = m_tex->baseDepth;
    std::string colorspace           = get_colorspace();
    m_spec.set_colorspace(colorspace);

    // Set textureformat attribute
    // TODO: we don't use this in ktxoutput, is this needed?
    if (m_tex->numDimensions == 2) {
        if (m_tex->numFaces > 1)
            m_spec.attribute("textureformat", "CubeFace Environment");
        else
            m_spec.attribute("textureformat", "Plain Texture");
    } else if (m_tex->numDimensions == 3) {
        m_spec.attribute("textureformat", "Volume Texture");
    } else {
        m_spec.attribute("textureformat", "unknown");
    }

    //
    // Make sure to save everything that is needed to recreate this exact same
    // KTX texture from OIIO API (i.e., fields of `ktxTextureCreateInfo`).
    //
    // KtxTexture fields may change after some libktx calls that take
    // KtxTexture* argument because they may potentially modify the texture
    // (e.g., in ktxTexture2_DecodeAstc supercompressionScheme is overwritten to
    // none, ktxTexture2_TranscodeBasis overwrites texture format, etc.). So,
    // store these now and NOT after libktx calls (e.g.,
    // ktxTexture2_TranscodeBasis).
    //
    m_spec.extra_attribs.attribute("ktx:supercompressionscheme",
                                   (uint32_t)m_tex->supercompressionScheme);
    // save as string (for future use, in case KTX1 is added)
    m_spec.extra_attribs.attribute("ktx:version", 2.0f);
    // Contrary to the specs' layerCount, numLayers is always >= 1
    m_spec.extra_attribs.attribute("ktx:nlayers", m_tex->numLayers);
    m_spec.extra_attribs.attribute("ktx:miplevels", m_tex->numLevels);
    m_spec.extra_attribs.attribute("ktx:generatemipmaps",
                                   m_tex->generateMipmaps);
    // TODO: do we need this?
    m_spec.extra_attribs.attribute("ktx:colormodel",
                                   (uint32_t)KHR_DFDVAL(m_tex->pDfd + 1, MODEL));
    m_spec.extra_attribs.attribute("ktx:vkformat", (uint32_t)m_tex->vkFormat);

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
                // KTXwriter identifies the program used to write this KTX file.
                // We don't care about such entry
                continue;
            } else if (attr_name == KTX_WRITER_SCPARAMS_KEY) {
                // KTXwriterScParams is used to report all kinds of non-default parameters used by ktx tools to write this KTX2 file.
                // This includes:
                //    non default Basis Universal params (i.e., for UASTC/ETC1S), non-default supercompression params, non-default mipmap generation params, etc.
                // Should be NUL terminated.
                if (vallen <= 1)
                    continue;
                // auto char_ptr = reinterpret_cast<const char*>(val);

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
    if (m_tex->supercompressionScheme != KTX_SS_NONE
        && m_tex->supercompressionScheme != KTX_SS_ZSTD
        && m_tex->supercompressionScheme != KTX_SS_ZLIB
        && m_tex->supercompressionScheme != KTX_SS_BASIS_LZ) {
        // vendor-specific or newly introduced supercompression schemes (not
        // supported)
        errorfmt("unsuppoted supercompression scheme: {}",
                 static_cast<uint32_t>(m_tex->supercompressionScheme));
        return false;
    }

    // Load the actual image data (pBuffer is NULL => m_tex own the buffer in
    // which the data will be loaded)
    if (auto result = ktxTexture2_LoadImageData(m_tex.get(), NULL, 0);
        result != KTX_SUCCESS) {
        errorfmt("ktxTexture2_LoadImageData returned Ktx error code: {}",
                 static_cast<uint32_t>(result));
        return false;
    }

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
    if (ktxTexture2_NeedsTranscoding(m_tex.get())) {
        if (auto status = ktxTexture2_TranscodeBasis(
                m_tex.get(), ktx_transcode_fmt_e::KTX_TTF_RGBA32, 0);
            status != KTX_SUCCESS) {
            errorfmt("failed to transcode KTX2 texture to raw pixels. "
                     "ktxTexture2_TranscodeBasis returned Ktx error code: {}",
                     static_cast<uint32_t>(status));
            return false;
        }
    }

    //
    // Decode GPU-compression if any (using libktx). Supported formats:
    //
    //    ASTC: libktx provides decoders for ASTC block compression via the
    //          ktxTexture2_DecodeAstc call.
    //          This currently decodes the whole texture (all miplevels, all
    //          slices, etc.) into memory.
    //
    //    BCn:  libktx will provide decoders/encoders for BCn block compression
    //          via ktxTexture2_DecodeBCn.
    //          TODO: wait for my RP in libktx to get merged then add BCn
    //          support.
    //
    //    ETC2: libktx provides decoders but they fall under non-open-source
    //          license. To quote KTX-Software: "The file lib/etcdec.cxx is not
    //          open source. It is made available under the terms of an Ericsson
    //          license, found in the file itself."
    //
    //    PVRTC: not planned (there are pending PRs in libktx).
    //
    if (m_tex->isCompressed /* i.e., is GPU block compressed? */) {
        // m_cmp = get_block_compression_from_format(m_tex->vkFormat);
        FormatInfo format_info;
        if (!get_info_from_vkformat(static_cast<VkFormat>(m_tex->vkFormat),
                                    format_info)) {
            close();
            errorfmt("Could not extract format info from provided "
                     "VkFormat: {}. This format is unsupported",
                     m_tex->vkFormat);
            return false;
        }
        m_cmp = format_info.compression;
        switch (m_cmp) {
#if 0  // TODO: wait for my PR in libktx to be merged
            /* BCn GPU formats */
        case BlockCompression::BC1:
        case BlockCompression::BC1A:
        case BlockCompression::BC2:
        case BlockCompression::BC3:
        case BlockCompression::BC4:
        case BlockCompression::BC5:
        case BlockCompression::BC6HU:
        case BlockCompression::BC6HS:
        case BlockCompression::BC7:
            //
            // Note:
            // ktxTexture2_DecodeBCn internally creates a new ktxTexture2 texture
            // and populates it with decoded data from the originally provided
            // texture. At the end, it moves the decoded data to m_tex and
            // destroys the temporarily created texture.
            //
            // This operation is expensive (both in memory and CPU cycles).
            // After this, m_tex->isCompressed will be false => this will only
            // be called once.
            //
            if (auto status = ktxTexture2_DecodeBCn(m_tex);
                status != KTX_SUCCESS) {
                errorfmt("failed to decode BCn-compressed texture. "
                         "ktxTexture2_DecodeBCn returned Ktx error code: {}",
                         static_cast<uint32_t>(status));
                return false;
            }
            break;
#endif

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
            // After this, m_tex->isCompressed will be false => this will only
            // be called once.
            //
            if (auto status = ktxTexture2_DecodeAstc(m_tex.get());
                status != KTX_SUCCESS) {
                errorfmt("failed to decode ASTC-compressed texture. "
                         "ktxTexture2_DecodeAstc returned Ktx error code: {}",
                         static_cast<uint32_t>(status));
                return false;
            }
            break;

        default:
            errorfmt("{} GPU-compressed formats are not supported",
                     block_compression_name(m_cmp));
            return false;
        }
    }

    OIIO_ASSERT(!m_tex->isCompressed);

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
    if (m_tex->vkFormat == VK_FORMAT_UNDEFINED) {
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

    auto format = static_cast<VkFormat>(m_tex->vkFormat);

    //
    // Important:
    // Call this AFTER transcoding the basis universal scheme (i.e., after
    // ktxTexture2_TranscodeBasis) and AFTER detecting which GPU block
    // compression scheme is used.
    //
    FormatInfo format_info;
    if (!get_info_from_vkformat(static_cast<VkFormat>(format), format_info)) {
        errorfmt(
            "Failed to extract info (e.g., nchannels, typedesc, etc.) from VkFormat: {}",
            static_cast<uint32_t>(format));
        return false;
    }
    m_spec.set_format(format_info.typedesc);
    m_spec.nchannels = format_info.nbrchannels;

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
    ioproxy_clear();
    return true;
};



bool
KtxInput::check(int subimage, int miplevel) const
{
    //
    // Before doing any calls, check if provided subimage and mip lvl are valid.
    // This is how OIIO figures out the number of subimages/miplevels.
    //
    if (subimage < 0 || miplevel < 0 || (uint32_t)subimage >= m_tex->numLayers
        || (uint32_t)miplevel >= m_tex->numLevels)
        // don't errorfmt here
        return false;
    return true;
}



//
// Since we load the whole KTX texture data in open(), actual seeking makes no
// sense in this context. You can just read at any pixel by just computing an
// offset.
//
// What this seek_subimage does, is essentially parameter verification (i.e.,
// are provided subimage and miplevel sane for the current texture kind).
//
// In the context of KTX:
// - For 3D textures (i.e., depth > 1): `subimage` does NOT reflect the
//   3D texture depth slice. subimage SHOULD be 0. TODO: how to read a 3D slice then?
// - For Cubemaps (i.e., tile_width/tile_height > 1): `subimage` does NOT reflect
//   the cubemap face. subimage SHOULD be 0. TODO: how to read a cubemap face tile?
// - Arrays of 2D textures: `subimage` maps to layer in libktx (i.e., index in
//   array). subimage SHOULD be [0, m_tex->numLayers[.
// - Arrays of 3D textures: same as array of 2D textures except each `subimage`
//   refer to 3D texture instead of a 2D one.
//
// For all kinds of textures, `miplevel` is ALWAYS interpreted as a mip level of
// the above `subimage` (e.g., `miplevel` 0 of 3D texture refers to base-level
// volume).
//
bool
KtxInput::seek_subimage(int subimage, int miplevel)
{
    if (!check(subimage, miplevel))
        return false;

    // if same subimage and miplevel as current => early out
    if (this->current_subimage() == subimage
        && this->current_miplevel() == miplevel)
        return true;

    m_subimage = subimage;
    m_miplevel = miplevel;

    //
    // According to official libktx source code, this is how they compute
    // dimensions of a miplevel. See:
    //  https://github.com/KhronosGroup/KTX-Software/lib/src/texture.c
    //
    const size_t width  = std::max(m_tex->baseWidth >> miplevel, 1u);
    const size_t height = std::max(m_tex->baseHeight >> miplevel, 1u);
    const size_t depth  = std::max(m_tex->baseDepth >> miplevel, 1u);

    m_spec.width  = width;
    m_spec.height = height;
    m_spec.depth  = depth;

    return true;
}



bool
KtxInput::read_native_scanline(int subimage, int miplevel, int y, int z,
                               void* data)
{
    return read_native_scanlines(subimage, miplevel, y, y + 1, z, data);
}



bool
KtxInput::read_native_scanlines(int subimage, int miplevel, int ybegin,
                                int yend, int z, void* data)
{
    const int width    = std::max(m_tex->baseWidth >> miplevel, 1u);
    const int height   = std::max(m_tex->baseHeight >> miplevel, 1u);
    const int depth    = std::max(m_tex->baseDepth >> miplevel, 1u);
    const size_t pitch = m_spec.pixel_bytes() * width;
    ktx_size_t offset;

    if (!check(subimage, miplevel)) {
        errorfmt("KTX read_native_scanlines: invalid subimage or miplevel");
        return false;
    }

    if (ybegin < 0 || ybegin >= yend || yend > height || z < 0 || z >= depth) {
        errorfmt(
            "KTX read_native_scanlines: Out of valid range scanline indices");
        return false;
    }

    OIIO_ASSERT(pitch
                == ktxTexture_GetRowPitch((ktxTexture*)m_tex.get(), miplevel));

    // Use this in case OIIO API provides read_native_scanlines with `data` as
    // `span<std::byte>` and a `z` slice param
#if 0
    // Can the provided span hold the requested scanlines?
    // This only accesses nchannels of the m_spec, so this is thread safe.
    if (!valid_raw_span_size(data, m_spec, 0, width, ybegin, yend))
        // errorfmt is set within valid_raw_span_size
        return false;
#endif

    //
    // GetImageOffset implements internal checks depending on texture kind (e.g.,
    // 3D, cubemap, etc.) and incase of invalid input, KTX_INVALID_OPERATION is
    // returned.
    //
    // TODO: face slice idx
    //
    if (auto status = ktxTexture2_GetImageOffset(m_tex.get(), miplevel,
                                                 subimage, z, &offset);
        status != KTX_SUCCESS) {
        errorfmt("ktxTexture_GetImageOffset failed with exit code: {}",
                 static_cast<uint32_t>(status));
        return false;
    }

    auto data_ptr = m_tex->pData + offset;

    // since miplevel is valid => get number of bytes in a row for this mip
    memcpy(data, data_ptr, pitch * size_t(yend - ybegin));
    DBG std::cout << fmt::format(
        "[ktxinput] read_native_scanlines(subimage={},miplevel={},ybegin={},yend={},z={})\n",
        subimage, miplevel, ybegin, yend, z);
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

    return (numRead == sizeof(magic)) && this->ktx_magic_cmp(magic, 0);
}



bool
KtxInput::ktx_magic_cmp(const uint8_t* sig, size_t start) const
{
    // this is: "«KTX 20»\r\n\x1A\n"
    const uint8_t KTX2_IDENTIFIER[12] { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32,
                                        0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
    for (size_t i = start; (i - start) < sizeof(KTX2_IDENTIFIER); ++i)
        if (sig[i] != KTX2_IDENTIFIER[i])
            return false;
    return true;
}



std::string
KtxInput::get_colorspace() const
{
    //
    // for set of, see:
    //  https://github.com/KhronosGroup/KTX-Software/blob/main/external/dfdutils/KHR/khr_df.h
    // for OIIO colorspaces, see:
    //  https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/src/libOpenImageIO/color_ocio.cpp
    //
    //  Don't use ktxTexture2_GetPrimaries_e/ktxTexture2_GetTransferFunction_e as these are only
    //  available in newer versions of libktx (>= 5.0.0, I think)
    //
    const auto transfer_function = static_cast<khr_df_transfer_e>(
        KHR_DFDVAL(m_tex->pDfd + 1, TRANSFER));
    const auto primaries = static_cast<khr_df_primaries_e>(
        KHR_DFDVAL(m_tex->pDfd + 1, PRIMARIES));
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



template<typename T>
inline bool
parse_number(const std::string& str, T& num)
{
    auto [_, ec] = std::from_chars(str.data(), str.data() + str.size(), num);
    return ec != std::errc {};
}



///
/// Parses KTXwriterScParams metadata and sets relevant attributes accordingly.
/// E.g., if Basis Universal non-default params were found, they are set.
///
/// One cannot know from the KTX2 file itself the kind of parameters that were
/// used to create/encode it (e.g., RDO params).
///
/// This is only useful when reading a KTX2 via OIIO and re-writing it again
/// (which is a bad idea since each re-write cycle worsens the quality). That
/// being said, if the user intends to use OIIO this way, the written KTX2 file
/// should be as similar as possible to the given input (assuming a read-write
/// of a KTX2 file without any change to its data).
///
void
KtxInput::parse_ktx_sc_params_metadata(const std::string_view ktx_sc_params)
{
    std::cmatch m;
    const auto f = std::regex_constants::icase;

    {  // UASTC params (see KTX-Software/tools/ktx/encode_utils_basis.h)
        std::regex uastc_quality_re("--uastc-quality\\s+(\\d+)", f);
        std::regex uastc_rdo_re("--uastc-rdo", f);
        std::regex uastc_rdo_l_re("--uastc-rdo-l\\s+((\\d*[.])?\\d+)", f);
        std::regex uastc_rdo_d_re("--uastc-rdo-d\\s+(\\d+)", f);
        std::regex uastc_rdo_b_re("--uastc-rdo-b\\s+((\\d*[.])?\\d+)", f);
        std::regex uastc_rdo_s_re("--uastc-rdo-s\\s+((\\d*[.])?\\d+)", f);
        std::regex uastc_rdo_f_re("--uastc-rdo-f", f);
        std::regex uastc_rdo_m_re("--uastc-rdo-m", f);
        std::regex uastc_rdo_uber_mode_re("--uastc-hdr-uber-mode", f);
        std::regex uastc_rdo_ultra_quant_re("--uastc-hdr-ultra-quant", f);
        std::regex uastc_rdo_favor_astc_re("--uastc-hdr-favor-astc", f);
        std::regex uastc_hdr_lambda_re("--uastc-hdr-lambda\\s+((\\d*[.])?\\d+)",
                                       f);
        std::regex uastc_hdr_6x6i_level_re("--uastc-hdr-6x6i-level\\s+(\\d+)",
                                           f);

        if (std::regex_search(ktx_sc_params.cbegin(), ktx_sc_params.cend(), m,
                              uastc_quality_re)
            && m.size() == 2) {
            uint32_t uastc_quality;
            if (parse_number(m[1].str(), uastc_quality)) {
                const uint32_t uastc_flags
                    = (unsigned int)~KTX_PACK_UASTC_LEVEL_MASK | uastc_quality;
                m_spec.extra_attribs.attribute("ktx:uastcFlags", uastc_flags);
                m_spec.extra_attribs.attribute("ktx:uastcHDRLevel",
                                               uastc_quality);
            }
        }

        if (std::regex_match(ktx_sc_params.cbegin(), ktx_sc_params.cend(),
                             uastc_rdo_re))
            m_spec.extra_attribs.attribute("ktx:uastcRDO", true);

        if (std::regex_search(ktx_sc_params.cbegin(), ktx_sc_params.cend(), m,
                              uastc_rdo_l_re)
            && m.size() == 2) {
            float uastc_rdo_l;
            if (parse_number(m[1].str(), uastc_rdo_l)) {
                m_spec.extra_attribs.attribute("ktx:uastcRDOQualityScalar",
                                               uastc_rdo_l);
            }
        }

        if (std::regex_search(ktx_sc_params.cbegin(), ktx_sc_params.cend(), m,
                              uastc_rdo_d_re)
            && m.size() == 2) {
            uint32_t uastc_rdo_d;
            if (parse_number(m[1].str(), uastc_rdo_d)) {
                m_spec.extra_attribs.attribute("ktx:uastcRDODictSize",
                                               uastc_rdo_d);
            }
        }

        if (std::regex_search(ktx_sc_params.cbegin(), ktx_sc_params.cend(), m,
                              uastc_rdo_b_re)
            && m.size() == 2) {
            float uastc_rdo_b;
            if (parse_number(m[1].str(), uastc_rdo_b)) {
                m_spec.extra_attribs.attribute(
                    "ktx:uastcRDOMaxSmoothBlockErrorScale", uastc_rdo_b);
            }
        }

        if (std::regex_search(ktx_sc_params.cbegin(), ktx_sc_params.cend(), m,
                              uastc_rdo_s_re)
            && m.size() == 2) {
            float uastc_rdo_s;
            if (parse_number(m[1].str(), uastc_rdo_s)) {
                m_spec.extra_attribs.attribute(
                    "ktx:uastcRDOMaxSmoothBlockStdDev", uastc_rdo_s);
            }
        }

        if (std::regex_match(ktx_sc_params.cbegin(), ktx_sc_params.cend(),
                             uastc_rdo_f_re))
            m_spec.extra_attribs.attribute("ktx:uastcRDODontFavorSimplerModes",
                                           true);

        if (std::regex_match(ktx_sc_params.cbegin(), ktx_sc_params.cend(),
                             uastc_rdo_m_re))
            m_spec.extra_attribs.attribute("ktx:uastcRDONoMultithreading",
                                           true);

        if (std::regex_match(ktx_sc_params.cbegin(), ktx_sc_params.cend(),
                             uastc_rdo_uber_mode_re))
            m_spec.extra_attribs.attribute("ktx:uastcHDRUberMode", true);

        if (std::regex_match(ktx_sc_params.cbegin(), ktx_sc_params.cend(),
                             uastc_rdo_ultra_quant_re))
            m_spec.extra_attribs.attribute("ktx:uastcHDRUltraQuant", true);

        if (std::regex_match(ktx_sc_params.cbegin(), ktx_sc_params.cend(),
                             uastc_rdo_favor_astc_re))
            m_spec.extra_attribs.attribute("ktx:uastcHDRFavorAstc", true);

        if (std::regex_search(ktx_sc_params.cbegin(), ktx_sc_params.cend(), m,
                              uastc_hdr_lambda_re)
            && m.size() == 2) {
            float uastc_hdr_lambda;
            if (parse_number(m[1].str(), uastc_hdr_lambda)) {
                m_spec.extra_attribs.attribute("ktx:uastcHDRLambda",
                                               uastc_hdr_lambda);
            }
        }

        if (std::regex_search(ktx_sc_params.cbegin(), ktx_sc_params.cend(), m,
                              uastc_hdr_6x6i_level_re)
            && m.size() == 2) {
            uint32_t uastc_hdr_6x6i_level;
            if (parse_number(m[1].str(), uastc_hdr_6x6i_level)) {
                m_spec.extra_attribs.attribute("ktx:uastcHDRLevel",
                                               uastc_hdr_6x6i_level);
            }
        }
    }

    {  // ETC1S params (see KTX-Software/tools/ktx/encode_utils_basis.h)
        std::regex etc1s_clevel("--clevel\\s+(\\d+)", f);
        std::regex etc1s_qlevel("--qlevel\\s+(\\d+)", f);
        std::regex etc1s_max_endpoints("--max-endpoints\\s+(\\d+)", f);
        std::regex etc1s_endpoint_rdo_threshold(
            "--endpoint-rdo-threshold\\s+((\\d*[.])?\\d+)", f);
        std::regex etc1s_max_selectors("--max-selectors\\s+(\\d+)", f);

        std::regex uastc_rdo_re("", f);
        std::regex uastc_rdo_l_re("--uastc-rdo-l\\s+((\\d*[.])?\\d+)", f);
        std::regex uastc_rdo_d_re("--uastc-rdo-d\\s+(\\d+)", f);
        std::regex uastc_rdo_b_re("--uastc-rdo-b\\s+((\\d*[.])?\\d+)", f);
        std::regex uastc_rdo_s_re("--uastc-rdo-s\\s+((\\d*[.])?\\d+)", f);
        std::regex uastc_rdo_f_re("--uastc-rdo-f", f);
        std::regex uastc_rdo_m_re("--uastc-rdo-m", f);
        std::regex uastc_rdo_uber_mode_re("--uastc-hdr-uber-mode", f);
        std::regex uastc_rdo_ultra_quant_re("--uastc-hdr-ultra-quant", f);
        std::regex uastc_rdo_favor_astc_re("--uastc-hdr-favor-astc", f);
        std::regex uastc_hdr_lambda_re("--uastc-hdr-lambda\\s+((\\d*[.])?\\d+)",
                                       f);
        std::regex uastc_hdr_6x6i_level_re("--uastc-hdr-6x6i-level\\s+(\\d+)",
                                           f);
    }
}

OIIO_PLUGIN_NAMESPACE_END

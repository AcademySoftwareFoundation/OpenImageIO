// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "OpenImageIO/imageio.h"
#include "ktx_pvt.h"
#include <cstdint>

OIIO_PLUGIN_NAMESPACE_BEGIN

// TODO:
//  - Should we add support to 2D texture arrays (i.e., "multiimage")?

class KtxOutput final : public ImageOutput {
public:
    KtxOutput()
    {
#if defined(KHRONOS_STATIC)
        // This has to be set if libktx is statically built
        DBG std::cout << "KHRONOS_STATIC set to 1" << '\n';
#endif
    }

    ~KtxOutput() override { close(); }

    const char* format_name(void) const override { return "ktx"; }

    int supports(string_view feature) const override
    {
        return (
            feature == "alpha" || feature == "ioproxy" ||
            // as per the KTX2 specs:
            //  registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_keyvalue_data
            feature == "arbitrary_metadata" ||
            // KTX2 supports 2D texture arrays, cubmap arrays, and 2D texture
            // arrays. That being said, we only support 2D texture arrays.
            // feature == "multiimage" ||
            // ktx supports mipmaps
            feature == "mipmap" ||
            // Ktx supports 3D textures
            feature == "volumes" || feature == "random_access"
            // Can write in any order whatsoever
        );
    }

    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;

    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;

    bool write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                         const void* data, stride_t xstride = AutoStride,
                         stride_t ystride = AutoStride) override;

    bool close() override;

private:
    std::string m_filename;

    bool m_initialized { false };  ///< Has open() with mode == Create was

    /// Uncompressed Vulkan format to create the initial texture with.
    VkFormat m_vkformat { VK_FORMAT_UNDEFINED };

    uint32_t m_miplevel_idx { 0 };  ///< Current MIP level

    uint32_t m_max_nmiplevels { 1 };  ///< Max number allowable MIP levels

    uint32_t m_basewidth { 0 };  ///< MIP level 0 width

    uint32_t m_baseheight { 0 };  ///< MIP level 0 height

    uint32_t m_basedepth { 0 };  ///< MIP level 0 depth

    ktxSupercmpScheme m_superCmp { KTX_SS_NONE };

    /// Whether to generate MIP maps when loading texture to graphics API. This
    /// will be passed to ktxTextureCreateInfo's generateMipmaps param.
    bool m_generate_mipmaps { false };

    BlockCompression m_cmp { BlockCompression::NONE };

    bool m_use_basis_universal { false };

    ktxBasisParams m_basis_params { 0 };

    ktxAstcParams m_astc_params { 0 };  ///< Only for ASTC block compression

    // ktxBCnParams m_bcn_params { 0 }; ///< Only for BCn block compression

    uint32_t m_zlib_level { 9 };  ///< Only for Zlib supercompression. Defaults
                                  ///< to highest compression level.

    uint32_t m_zstd_level { 22 };  ///< Only for ZSTD supercompression. Defaults
                                   ///< to highest compression level.

    std::vector<unsigned char> m_scratch;

    ///
    /// Container for raw (i.e., uncompressed) texture data structured as
    /// follows:
    ///   mip level -> slice/face -> pixels
    ///
    /// The number of slices/faces is known before hand (i.e., appending
    /// slices/faces is not supported). This significantly simplifies the
    /// implementation.
    ///
    /// Q. Why store all subimages/levels/slices/faces here?
    /// A. libktx only supports writing whole images (i.e.,
    ///    miplevel+layer+slice/face hence why we keep a large std::vector at
    ///    all times. This is also needed because we apply compression upon file
    ///    closure and not each time on write_scanline(s).
    ///
    /// To access underlying data: [miplevel_idx][slice_idx + face_idx]
    /// Note: slice_idx and face_idx are mutually exclusive so summing them is
    ///       perfectly fine.
    ///
    std::vector<std::vector<std::vector<uint8_t>>> m_imgs;

    void append_mipmaps_vector();

    void init();

    bool write_ktx2();

    bool construct_basis_params(ktxBasisParams& params, std::string_view codec,
                                uint32_t threads = 1) const;

    // bool construct_astc_params(ktxAstcParams& param) const;

    // bool construct_bcn_params(ktxBCnParams& params) const;
};



OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
ktx_output_imageio_create()
{
    return new KtxOutput;
}
OIIO_EXPORT const char* ktx_output_extensions[] = { "ktx2", nullptr };

OIIO_PLUGIN_EXPORTS_END



bool
KtxOutput::open(const std::string& name, const ImageSpec& userspec,
                OpenMode mode)
{
    if (mode == Create) {
        if (userspec.depth
            > 1) {  // Volume texture are limited to 4096x4096x4096
            if (!check_open(mode, userspec, { 0, 4096, 0, 4096, 0, 4096, 0, 4 }))
                return false;
        } else if (userspec.tile_width
                   > 1) {  // Cubemap texture are limited to 16384x16384
            if (!check_open(mode, userspec,
                            { 0, 16384, 0, 16384 * 6, 0, 1, 0, 4 }))
                return false;
        } else {  // 2D texture are limited to 32768x32768
            if (!check_open(mode, userspec, { 0, 32768, 0, 32768, 0, 1, 0, 4 }))
                return false;
        }

        m_basewidth  = m_spec.width;
        m_baseheight = m_spec.height;
        m_basedepth  = m_spec.depth;

        // Save name and spec for later use
        m_filename = name;

        // If not uint8, default to uint8 (HDR not yet supported)
        if (m_spec.format
            != TypeDesc::UINT8 /* && m_spec.format != TypeDesc::UINT16 */)
            m_spec.set_format(TypeDesc::UINT8);

        ioproxy_retrieve_from_config(m_spec);
        if (!ioproxy_use_or_open(m_filename))
            return false;

        // Try to figure the texture kind (i.e., plain 2D texture, volume, or cubemap)
        // const auto textureformat = m_spec.get_string_attribute("textureformat");
        // if (textureformat == "Volume Texture") {
        //     if (m_spec.depth <= 1) {
        //         errorfmt("Volume textures are expected to have a depth > 1");
        //         close();
        //         return false;
        //     }
        //     m_texturekind = TextureKind::SINGLE_TEXTURE_3D;
        // } else if (textureformat == "CubeFace Environment") {
        //     // TODO: check on tiles
        //     m_texturekind = TextureKind::CUBEMAP_TEXTURE;
        // } else /* unknown */ {
        //     m_texturekind = TextureKind::SINGLE_TEXTURE_2D;
        // }

        const auto compression = m_spec.get_string_attribute("compression",
                                                             "NONE");
        if (iequals(compression, "NONE")) {
            m_cmp = BlockCompression::NONE;
        } else if (iequals(compression, "ASTC")) {
            m_cmp = BlockCompression::ASTC;
        } else {
            errorfmt(
                "Unsupported/Unknown compression from string attribute \"compression\": ",
                compression);
            return false;
        }

        // Currently only two colorspaces are tested, linear or REC709 sRGB
        const auto colorspace = m_spec.get_string_attribute("oiio:ColorSpace",
                                                            "srgb_rec709_scene");
        bool is_srgb          = colorspace == "srgb_rec709_scene";
        m_spec.set_colorspace(colorspace);

        // TODO: get_int_attribute causes a segfault and I have no idea why ...
        // Weirdly, calling find_attribute directly (and checking the resulting
        // pointer) works, but not get_int_attribute ...

        //
        // Use sensible default in case the input data did not originate from a KTX2
        // file and the user did not provide a supercompression scheme. KTX2 usually
        // uses Basis LZ supercompression scheme to benefit from both: smaller
        // disk filesizes and on-the-fly transcoding to a supported native GPU
        // format.
        //
        const auto& supercompression_str
            = m_spec.get_string_attribute("ktx:supercompressionscheme", "NONE");
        if (iequals(supercompression_str, "NONE")) {
            m_superCmp = ktxSupercmpScheme::KTX_SS_NONE;
        } else if (iequals(supercompression_str, "ZSTD")) {
            m_superCmp = ktxSupercmpScheme::KTX_SS_ZSTD;
        } else if (iequals(supercompression_str, "ZLIB")) {
            m_superCmp = ktxSupercmpScheme::KTX_SS_ZLIB;
        } else {
            close();
            errorfmt("unsupported super compression scheme: {}",
                     static_cast<uint32_t>(m_superCmp));
            return false;
        }
        if (auto Q = m_spec.find_attribute("", TypeDesc::STRING)) {
            const auto& supercompression_str = Q->get_string();

            m_superCmp = static_cast<ktxSupercmpScheme>(
                *(uint32_t*)(Q->data()));
            // Do an early check on supported supercompressionscheme values
            if (m_superCmp != KTX_SS_BASIS_LZ && m_superCmp != KTX_SS_NONE) {
                // doing an `errorfmt()` then `close()` causes a seg fault...
            }
            DBG std::cout << "[ktxoutput] supercompression scheme: "
                          << m_superCmp << '\n';
        }

        if (auto Q = m_spec.find_attribute("ktx:generatemipmaps",
                                           TypeDesc::INT)) {
            m_generate_mipmaps = static_cast<bool>(Q->get_int());
            DBG std::cout << "[ktxoutput] generate mipmaps: " << std::boolalpha
                          << m_generate_mipmaps << '\n';
        }

        // We can check m_basis_params.codec != NONE but this won't work for
        // libktx 4.3.2 which is why we just use a bool var
        m_use_basis_universal = false;
        if (auto Q = m_spec.find_attribute("ktx:codec", TypeDesc::STRING)) {
            const auto& codec = Q->get_string();
            if (!construct_basis_params(m_basis_params, codec)) {
                close();
                // construct_basis_params calls errorfmt
                return false;
            }
            m_use_basis_universal = m_basis_params.codec
                                    != KTX_BASIS_CODEC_NONE;
            DBG std::cout
                << "[ktxoutput] basis universal codec (from \"ktx:codec\"): "
                << codec << '\n';
        }

        //
        // User provided nothing about neither the target GPU block compression
        // nor the target Basis Universal codec. Default to writing uncompressed
        // VK_FORMAT with ZSTD supercompression (write as losseless KTX2
        // output).
        //
        // If we intend to compress to BasisLZ/ETC1S or UASTC then we need to
        // figure the VkFormat so that ktxTexture_SetImageFromMemory does not
        // segfault. (makes sense, since we are creating a KTX texture and
        // telling it to allocate storage, how would it know the size of a given
        // subimage if we provide it with VK_FORMAT_UNDEFINED?)
        //
        m_vkformat = get_vkformat_from_info(m_spec.nchannels, m_spec.format,
                                            is_srgb);
        if (m_vkformat == VK_FORMAT_UNDEFINED) {
            close();
            errorfmt(
                "Failed to determine VkFormat from nchannels={} format={} colorspace={}",
                m_spec.nchannels, m_spec.format, colorspace);
            return false;
        }


        if (m_spec.depth > 1)
            m_max_nmiplevels
                = (uint32_t)floor(
                      logf(std::min(std::min(m_spec.width, m_spec.height),
                                    m_spec.depth))
                      / logf(2))
                  + 1;
        else
            m_max_nmiplevels = (uint32_t)floor(
                                   logf(std::min(m_spec.width, m_spec.height))
                                   / logf(2))
                               + 1;

        DBG std::cout << "[ktxoutput] max number mip levels allowed: "
                      << m_max_nmiplevels << std::endl;

        // Initialize slices/faces container if not already initialized by a
        // previous call to open(name, subimages, specs)
        OIIO_ASSERT(m_imgs.empty()
                    && "Expected mip levels container to be empty");
        OIIO_ASSERT(m_miplevel_idx == 0);

        // Reserve space for base-level mipmap (level 0)
        append_mipmaps_vector();

        m_initialized = true;
        return true;
    }  // mode == Create

    if (mode == AppendMIPLevel) {
        if (!m_initialized) {
            errorfmt("Cannot append a MIP level if no file has been opened");
            return false;
        }

        if ((m_miplevel_idx + 1) >= m_max_nmiplevels) {
            errorfmt("Maximum number of mip levels is reached");
            return false;
        }

        uint32_t miplevel_width  = std::max(1u,
                                            m_basewidth >> (m_miplevel_idx + 1));
        uint32_t miplevel_height = std::max(1u, m_baseheight
                                                    >> (m_miplevel_idx + 1));
        uint32_t miplevel_depth  = std::max(1u,
                                            m_basedepth >> (m_miplevel_idx + 1));

        // Copy the new mip level size.  Keep everything else from the
        // original level.
        if ((uint32_t)userspec.width != miplevel_width
            || (uint32_t)userspec.height != miplevel_height
            || (uint32_t)userspec.depth != miplevel_depth) {
            errorfmt(
                "Expected (widht,height,depth): ({},{},{}) but got: ({},{},{})",
                miplevel_width, miplevel_height, miplevel_depth, userspec.width,
                userspec.height, userspec.depth);
            return false;
        }
        m_spec.width  = userspec.width;
        m_spec.height = userspec.height;
        m_spec.depth  = userspec.depth;
        ++m_miplevel_idx;

        // Reserve memory for this mip level
        append_mipmaps_vector();

        return true;
    }  // mode == AppendMIPLevel

    // (mode == AppendSubimage) is NOT supported. Pre-declaring the number of
    // subimages is easier to implement (random access to volume slices is
    // already provided).

    return false;
}



bool
KtxOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    return write_scanlines(y, y + 1, z, format, data, xstride);
}



bool
KtxOutput::write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                           const void* data, stride_t xstride, stride_t ystride)
{
    const int width  = std::max(m_basewidth >> m_miplevel_idx, 1u);
    const int height = std::max(m_baseheight >> m_miplevel_idx, 1u);
    if (ybegin < 0 || ybegin >= yend || yend > height || z < 0
        || z >= (int)m_basedepth) {
        errorfmt(
            "KTX write_scanlines: Out of valid range scanline indices. "
            "Provided: ybegin={} yend={} z={}. "
            "Constraints: ybegin: [0, min({},yend)[, yend>ybegin, z: [0,{}[",
            ybegin, yend, z, height, m_basedepth);
        return false;
    }
    stride_t zstride = AutoStride;
    m_spec.auto_stride(xstride, ystride, zstride, format, m_spec.nchannels,
                       width, height);
    //
    // Convert to the native format the current specs expects. This is needed,
    // for instance, to convert a given TypeDesc::FLOAT into native format that
    // this KTX2 writer expects (i.e., TypeDesc::UINT8 or TypeDesc::UINT16).
    //
    // Returned data pointer may be the same as the provided pointer (i.e., no
    // conversion is needed because supplied data is already in native format).
    //
    data = to_native_rectangle(/* m_spec.x */ 0, /* m_spec.x + */ width, ybegin,
                               yend, z, z + 1, format, data, xstride, ystride,
                               zstride, m_scratch, false, 0, ybegin, z);

    // data should now be contiguous and of the expected format (UINT8 or
    // UINT16) so simply memcpy into internal buffer that will be written on
    // close().
    const size_t pitch = m_spec.scanline_bytes();
    auto pSrc          = reinterpret_cast<const uint8_t*>(data);
    size_t offset      = ybegin * pitch;
    size_t datalen     = (yend - ybegin) * pitch;
    memcpy(m_imgs[m_miplevel_idx][z].data() + offset, pSrc, datalen);
    DBG std::cout << "write_scanlines wrote " << datalen << " bytes"
                  << std::endl;
    return true;
}



bool
KtxOutput::close()
{
    DBG std::cout << "[ktxoutput] close() called" << std::endl;
    // closing an un-opened ImageOutput instance is fine
    if (!m_initialized)
        return true;
    // Check if already closed => if so, then the KTX2 file is already saved
    if (!ioproxy_opened()) {
        init();
        return true;
    }
    bool result = write_ktx2();  // TODO: can this throw? (prob not)
    init();
    return result;
}



void
KtxOutput::append_mipmaps_vector()
{
    m_imgs.emplace_back(m_spec.depth);
    for (auto& slices_vec : m_imgs[m_miplevel_idx]) {
        slices_vec.resize(m_spec.scanline_bytes() * m_spec.height);
    }
}



void
KtxOutput::init()
{
    // TODO: calling open() after close() on this hasn't been tested yet ...
    m_initialized         = false;
    m_filename            = std::string();
    m_vkformat            = VK_FORMAT_UNDEFINED;
    m_miplevel_idx        = 0;
    m_max_nmiplevels      = 1;
    m_basewidth           = 0;
    m_baseheight          = 0;
    m_basedepth           = 0;
    m_superCmp            = KTX_SS_NONE;
    m_generate_mipmaps    = false;
    m_cmp                 = BlockCompression::NONE;
    m_use_basis_universal = false;
    m_basis_params        = { 0 };
    m_astc_params         = { 0 };
    // m_bcn_params = { 0 };
    m_zlib_level = 9;
    m_zstd_level = 22;
    m_imgs.clear();
    ioproxy_clear();
}



///
/// Contruct ktxBasisParams struct from given input (from the provided
/// ImageSpec). There are two ways to approach this:
///   1. Expect the whole ktxBasisParams to be provided by the user
///   2. Search for each individual field
///
/// Option 1) has the following benefits:
///   -  Expose only one attribute "ktx:basisparams"
/// But:
///   -  The user has to be aware of which libktx version is used (very bad)
///   -  Less safe (?)
///
/// Options 2) has the following benefits:
///   -  User just has to provide each parameter separately thus => don't have
///      to care about which libktx version is used.
///   -  Safer (?)
/// But:
///   -  A lot of "ktx:<param-name>" attributes have to be exposed. We can
///      provide "high-level" parameters but that will make writing KTX2 output
///      using OIIO significantly less customizable.
///
/// TODO: update comment when we finally agree on which approach.
///
/// For the moment, option 2) option 2) is opted for.
///
bool
KtxOutput::construct_basis_params(ktxBasisParams& params,
                                  std::string_view codec,
                                  uint32_t threads) const
{
    // Set defaults
    params             = { 0 };
    params.structSize  = sizeof(ktxBasisParams);
    params.threadCount = threads;

    params.etc1sCompressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;

    params.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT;

    if (iequals(codec, "NONE")) {
        // no need to fill remaining struct members
        params.codec = KTX_BASIS_CODEC_NONE;
        return true;
    }

    if (iequals(codec, "uastc") || iequals(codec, "uastc-ldr")
        || iequals(codec, "uastc-ldr-4x4")) {
        params.codec = KTX_BASIS_CODEC_UASTC_LDR_4x4;
    } else if (iequals(codec, "uastc-hdr") || iequals(codec, "uastc-hdr-4x4")) {
        params.codec = KTX_BASIS_CODEC_UASTC_HDR_4x4;
    } else if (iequals(codec, "etc1s")) {
        params.codec = KTX_BASIS_CODEC_ETC1S;
    } else if (iequals(codec, "uastc-hdr-6x6")) {
        params.codec = KTX_BASIS_CODEC_UASTC_HDR_6x6_INTERMEDIATE;
    } else {
        errorfmt(
            "Provided Basis Universal codec \"{}\" is invalid. Supported values: \"uastc\" \"etc1s\" \"uastc-hdr-4x4\" \"uastc-hdr-6x6\"",
            codec);
        return false;
    }

    if (params.codec == KTX_BASIS_CODEC_ETC1S) {
        // Params that only apply to ETC1S
        if (auto Q = m_spec.find_attribute("ktx:etc1sCompressionLevel",
                                           TypeDesc::UINT32))
            params.etc1sCompressionLevel = *(uint32_t*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:etc1sQualityLevel",
                                           TypeDesc::UINT32))
            params.qualityLevel = *(uint32_t*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:etc1sMaxEndpoints",
                                           TypeDesc::UINT32))
            params.maxEndpoints = *(uint32_t*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:etc1sEndpointRDOThreshold",
                                           TypeDesc::FLOAT))
            params.endpointRDOThreshold = *(float*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:etc1sMaxSelectors",
                                           TypeDesc::UINT32))
            params.maxSelectors = *(uint32_t*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:etc1sSelectorRDOThreshold",
                                           TypeDesc::FLOAT))
            params.selectorRDOThreshold = *(float*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:etc1sNoEndpointRDO"))
            params.noEndpointRDO = static_cast<bool>(Q->get_int());
        if (auto Q = m_spec.find_attribute("ktx:etc1sNoSelectorRDO",
                                           TypeDesc::INT))
            params.noSelectorRDO = static_cast<bool>(Q->get_int());
    } else if (params.codec == KTX_BASIS_CODEC_UASTC_LDR_4x4) {
        // Params that only apply to UASTC
        if (auto Q = m_spec.find_attribute("ktx:uastcFlags", TypeDesc::UINT32))
            params.uastcFlags = *(uint32_t*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:uastcRDO", TypeDesc::INT))
            params.uastcRDO = static_cast<bool>(Q->get_int());
        if (auto Q = m_spec.find_attribute("ktx:uastcRDOQualityScalar",
                                           TypeDesc::FLOAT))
            params.uastcRDOQualityScalar = *(float*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:uastcRDODictSize",
                                           TypeDesc::UINT32))
            params.uastcRDODictSize = *(uint32_t*)Q->data();
        if (auto Q
            = m_spec.find_attribute("ktx:uastcRDOMaxSmoothBlockErrorScale",
                                    TypeDesc::FLOAT))
            params.uastcRDOMaxSmoothBlockErrorScale = *(float*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:uastcRDOMaxSmoothBlockStdDev",
                                           TypeDesc::FLOAT))
            params.uastcRDOMaxSmoothBlockStdDev = *(float*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:uastcRDODontFavorSimplerModes",
                                           TypeDesc::INT))
            params.uastcRDODontFavorSimplerModes = static_cast<bool>(
                Q->get_int());
        if (auto Q = m_spec.find_attribute("ktx:uastcRDONoMultithreading",
                                           TypeDesc::INT))
            params.uastcRDONoMultithreading = static_cast<bool>(Q->get_int());
    } else if (params.codec == KTX_BASIS_CODEC_UASTC_HDR_4x4
               || params.codec == KTX_BASIS_CODEC_UASTC_HDR_6x6_INTERMEDIATE) {
        if (auto Q = m_spec.find_attribute("ktx:uastcHDRQuality",
                                           TypeDesc::UINT32))
            params.uastcHDRQuality = *(uint32_t*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:uastcHDRUberMode",
                                           TypeDesc::INT))
            params.uastcHDRUberMode = static_cast<bool>(Q->get_int());
        if (auto Q = m_spec.find_attribute("ktx:uastcHDRUltraQuant",
                                           TypeDesc::INT))
            params.uastcHDRUltraQuant = static_cast<bool>(Q->get_int());
        if (auto Q = m_spec.find_attribute("ktx:uastcHDRFavorAstc",
                                           TypeDesc::INT))
            params.uastcHDRFavorAstc = static_cast<bool>(Q->get_int());
        if (auto Q = m_spec.find_attribute("ktx:uastcHDRLambda",
                                           TypeDesc::FLOAT))
            params.uastcHDRLambda = *(float*)Q->data();
        if (auto Q = m_spec.find_attribute("ktx:uastcHDRLevel",
                                           TypeDesc::UINT32))
            params.uastcHDRLevel = *(uint32_t*)Q->data();
    }

    // Params that apply to both ETC1S and UASTC
    if (auto Q = m_spec.find_attribute("ktx:noSSE", TypeDesc::INT))
        params.noSSE = static_cast<bool>(Q->get_int());
    if (auto Q = m_spec.find_attribute("ktx:normalMap", TypeDesc::INT))
        params.normalMap = static_cast<bool>(Q->get_int());
    if (auto Q = m_spec.find_attribute("ktx:inputSwizzle",
                                       TypeDesc(TypeDesc::CHAR, 4)))
        memcpy(params.inputSwizzle, Q->data(), 4);
    if (auto Q = m_spec.find_attribute("ktx:preSwizzle", TypeDesc::INT))
        params.preSwizzle = static_cast<bool>(Q->get_int());

    return true;
}



bool
KtxOutput::write_ktx2()
{
    ///
    /// KTX2 texture RAII'fied via a unique_ptr.
    /// Q. Why not create this in open()?
    /// A. open() can be called with AppendMIPLevel mode which means we don't
    ///    actually know the number of miplevels to create this texture with
    ///    until a call to close(). libktx doesn't support changing texture
    ///    attributes after its creation.
    ///
    std::unique_ptr<ktxTexture2, decltype(ktxTexture2_Destroy)*> tex {
        nullptr, ktxTexture2_Destroy
    };

    OIIO_ASSERT(
        m_vkformat != VK_FORMAT_UNDEFINED
        && "VkFormat should never be VK_FORMAT_UNDEFINED when creating a KTX2 texture");
    ktxTextureCreateInfo create_info;
    create_info.glInternalformat = 0;  // Ignored as this is not a KTX1 texture
    create_info.vkFormat         = m_vkformat;
    create_info.pDfd             = nullptr;
    create_info.baseWidth        = m_basewidth;
    create_info.baseHeight       = m_baseheight;
    create_info.baseDepth        = m_basedepth;
    create_info.numDimensions    = m_basedepth > 1 ? 3u : 2u;
    create_info.numLevels        = m_miplevel_idx + 1;
    create_info.numLayers = 1;  // Can't support this with current OIIO API
    create_info.numFaces  = 1;  // TODO
    create_info.isArray = KTX_FALSE;  // Can't support this with current OIIO API
    create_info.generateMipmaps = m_generate_mipmaps;

    DBG std::cout << "calling ktxTexture2_Create with: "
                  << "vkFormat=" << create_info.vkFormat << "; "
                  << "baseWidth=" << create_info.baseWidth << "; "
                  << "baseHeight=" << create_info.baseHeight << "; "
                  << "baseDepth=" << create_info.baseDepth << "; "
                  << "numDimensions=" << create_info.numDimensions << "; "
                  << "numLevels=" << create_info.numLevels << "; "
                  << "numLayers=" << create_info.numLayers << "; "
                  << "numFaces=" << create_info.numFaces << "; "
                  << "isArray=" << create_info.isArray << "; "
                  << "generateMipmaps=" << create_info.generateMipmaps << "; "
                  << std::endl;

    ktxTexture2* p_tex = nullptr;
    auto result        = ktxTexture2_Create(&create_info,
                                            KTX_TEXTURE_CREATE_ALLOC_STORAGE, &p_tex);
    tex.reset(p_tex);

    if (result != KTX_SUCCESS) {
        errorfmt("ktxTexture2_Create returnned ktx_error_code: {}",
                 static_cast<uint32_t>(result));
        return false;
    }

    DBG std::cout << "ktxTexture2_Create created texture successfully"
                  << std::endl;

    //
    // At first, set uncompressed data for all miplevels, layers, slices, etc.
    // The loop over slices and the loop over faces are mutually exclusive
    // (i.e., if tex->numLayers > 1 then tex->numFaces == 1, and vice versa).
    //
    // Note 1:
    //  You may notice the `generateMipmaps` flag in the ktxTexture struct, it
    //  is just used to instruct Vulkan or OpenGL to generate mipmaps for the
    //  texture to be uploaded NOT for mipmap generation on the CPU.
    //
    // Important:
    //  If the VkFormat related to the KTX texture creation is wrongly set, this
    //  will cause a segfault!
    //
    for (uint32_t level_idx = 0; level_idx < tex->numLevels; ++level_idx) {
        const uint32_t depth = std::max(tex->baseDepth >> level_idx, 1u);
        // Since array layers are not supported, this loop will only execute once
        for (ktx_uint32_t layer_idx = 0; layer_idx < tex->numLayers;
             ++layer_idx) {
            for (uint32_t face_idx = 0; face_idx < tex->numFaces; ++face_idx) {
                for (uint32_t slice_idx = 0; slice_idx < depth; ++slice_idx) {
                    // Faces and Slices are mutually exclusive, addition is fine
                    auto data_ptr
                        = m_imgs[level_idx][slice_idx + face_idx].data();
                    const size_t data_size
                        = m_imgs[level_idx][slice_idx + face_idx].size();

                    // Before anything, be absolutely certain that what we are about
                    // to write is of the exact same size (in bytes) of what libktx
                    // expects us to write for this mip level.
                    const size_t expected_size
                        = ktxTexture2_GetImageSize(tex.get(), level_idx);
                    if (data_size != expected_size) {
                        errorfmt(
                            "libktx expects {} bytes to be written for this mip level {} but {} bytes are instead attempted to be written",
                            expected_size, level_idx, data_size);
                        return false;
                    }

                    auto status = ktxTexture_SetImageFromMemory(
                        (ktxTexture*)tex.get(), level_idx, 0,
                        face_idx + slice_idx, data_ptr, data_size);
                    if (status != KTX_SUCCESS) {
                        errorfmt(
                            "ktxTexture_SetImageFromMemory returned KTX exit error code: {}",
                            static_cast<uint32_t>(status));
                        return false;
                    }
                    DBG std::cout << fmt::format(
                        "ktxTexture_SetImageFromMemory for slice_idx={} face_idx={} level_idx={} wrote {} bytes",
                        slice_idx, face_idx, level_idx, data_size)
                                  << std::endl;
                }  // slices
            }      // faces
        }          // layers
    }              // mip levels


    //
    // After having written all necessary uncompressed data, check if the
    // texture is expected to be compressed (i.e., data should be compressed
    // using some specified GPU-block-compression format). If so, compress
    // said data using libktx.
    //
    if (m_cmp != BlockCompression::NONE) {
        switch (m_cmp) {
            // ASTC
        case BlockCompression::ASTC: {
            // TODO: expose ASTC compression quality parameter as spec attribute
            if (auto status = ktxTexture2_CompressAstcEx(tex.get(),
                                                         &m_astc_params);
                status != KTX_SUCCESS) {
                errorfmt(
                    "ktxTexture2_CompressAstc returned KTX exit error code: {}",
                    static_cast<uint32_t>(status));
                return false;
            }
            break;
        }
#if 0
       // BCn
    case BlockCompression::BC1:
    case BlockCompression::BC3:
    case BlockCompression::BC4:
    case BlockCompression::BC5:
    case BlockCompression::BC7:
        // TODO: expose BCn compression quality parameter as spec attribute
        if (auto status = ktxTexture2_CompressBCn(tex, nullptr);
            status != KTX_SUCCESS) {
            errorfmt("ktxTexture2_CompressBCn returned KTX exit error code: {}",
                     static_cast<uint32_t>(status));
            return false;
        }
#endif
        default:
            errorfmt("Writing/Encoding {} compression is not supported",
                     block_compression_name(m_cmp));
            return false;
        }
    }

    //
    // If a Basis Universal compression is requested (i.e., to BasisLZ/ETC1S
    // or UASTC), compress the texture before writing. This is mutually
    // exclusive with the isCompressed check above (i.e., you can't have a
    // texture that is compressed using some GPU-block-format that also uses
    // some Basis Universal latent format).
    //
    if (m_use_basis_universal) {
        if (auto status = ktxTexture2_CompressBasisEx(tex.get(),
                                                      &m_basis_params);
            status != KTX_SUCCESS) {
            errorfmt("ktxTexture2_CompressBasisEx returned error code: ",
                     static_cast<uint32_t>(status));
            return false;
        }
    }

    //
    // Finally, apply the supercompression scheme (if any). Supercompression
    // can be applied (especially if RDO is used with BCn/ASTC) to significantly
    // reduce disk file size at the expense of additional CPU data load time
    // (i.e., data now has to be inflated before being uploaded to the GPU).
    //
    if (m_superCmp == KTX_SS_ZLIB) {
        if (auto status = ktxTexture2_DeflateZLIB(tex.get(), m_zlib_level);
            status != KTX_SUCCESS) {
            errorfmt("ktxTexture2_DeflateZLIB returned KTX exit error code: {}",
                     static_cast<uint32_t>(status));
            return false;
        }
    } else if (m_superCmp == KTX_SS_ZSTD) {
        if (auto status = ktxTexture2_DeflateZstd(tex.get(), m_zstd_level);
            status != KTX_SUCCESS) {
            errorfmt("ktxTexture2_DeflateZstd returned KTX exit error code: {}",
                     static_cast<uint32_t>(status));
            return false;
        }
    }

    //
    // Now write key/value data (KVD). Ideally, we should follow the KTX2 spec
    // about which KVD we are encouraged to write and also follow same process
    // as KTX-Software (e.g., write used mipmap filter in KTXScWriterParams
    // entry, write KTXWriter fields, etc.)
    //

    // Add/overwrite the KTXwriter metadata entry. The specs encourages us to do
    // so.
    char writer[100];
    snprintf(writer, sizeof(writer), "oiio version %d - plugin version %d",
             OPENIMAGEIO_VERSION, OIIO_PLUGIN_VERSION);
    ktxHashList_AddKVPair(&tex->kvDataHead, KTX_WRITER_KEY,
                          (ktx_uint32_t)strlen(writer) + 1, writer);
    // std::cout << "KTXwrite: " << writer << '\n';


    Filesystem::IOProxy* m_io = ioproxy();
    if (!strcmp(m_io->proxytype(), "file")) {
        auto fd = reinterpret_cast<Filesystem::IOFile*>(m_io)->handle();
        if (auto status = ktxTexture2_WriteToStdioStream(tex.get(), fd);
            status != KTX_SUCCESS) {
            errorfmt(
                "ktxTexture2_WriteToStdioStream returned KTX exit error code: {}",
                static_cast<uint32_t>(status));
            return false;
        }
        return true;
    }

    if (!strcmp(m_io->proxytype(), "vecoutput")) {
        auto proxy = reinterpret_cast<Filesystem::IOVecOutput*>(m_io);
        ktx_uint8_t* buff;
        ktx_size_t buff_size;
        if (auto status = ktxTexture2_WriteToMemory(tex.get(), &buff,
                                                    &buff_size);
            status != KTX_SUCCESS) {
            errorfmt(
                "ktxTexture2_WriteToMemory returned KTX exit error code: {}",
                static_cast<uint32_t>(status));
            return false;
        }
        // Cleanup when we go out of scope or on exception (make sure to use
        // matching deallocator, i.e., free())
        auto _ = std::unique_ptr<ktx_uint8_t, decltype(std::free)*>(buff,
                                                                    std::free);
        proxy->write(buff, buff_size);
        return true;
    }

    // OIIO should guarantee that this never happens
    errorfmt("unexpected IOProxy type: {}", m_io->proxytype());
    return false;
}

OIIO_PLUGIN_NAMESPACE_END

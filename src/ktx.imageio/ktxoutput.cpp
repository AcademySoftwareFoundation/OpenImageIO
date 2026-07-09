// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// TODO: only set this if libktx is statically built/linked against
// Per KTX-Software BUILDING.md:
//  > When linking to the static library, make sure to
//  > define `KHRONOS_STATIC` before including KTX header files.
//  > This is especially important on Windows.
#ifndef BUILD_SHARED_LIBS
#    define KHRONOS_STATIC 1
#endif

#include "ktx_pvt.h"
#include <cstdint>
#include <ktx.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

class KtxOutput final : public ImageOutput {
public:
    KtxOutput() {}

    ~KtxOutput() override { close(); }

    const char* format_name(void) const override { return "ktx"; }

    int supports(string_view feature) const override
    {
        return (
            feature == "alpha" || feature == "ioproxy" ||
            // as per the KTX1/2 specs:
            //  registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_keyvalue_data
            feature == "arbitrary_metadata" ||
            /* ktx supports 3D textures, cubmap textures, texture arrays, ... */
            feature == "multiimage" ||
            /* not sure ... this naming is confusing */
            feature == "mipmap");
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

    /// KTX2 texture.
    std::unique_ptr<ktxTexture2, decltype(ktxTexture_Deleter)*> m_tex {
        nullptr, ktxTexture_Deleter
    };

    uint32_t m_nlayers { 1 };

    uint32_t m_miplevels { 1 };

    ktxSupercmpScheme m_superCmp { KTX_SS_NONE };

    khr_df_model_e m_colormodel { KHR_DF_MODEL_UNSPECIFIED };

    BlockCompression m_cmp { BlockCompression::NONE };

    std::vector<unsigned char> m_scratch;

    /// libktx only supports writing whole images (i.e., (miplevel, layer, face_slice/depth)
    /// hence why we keep a large std::vector at all times. This is also needed because we
    /// apply compression upon file closure and not each time on write_scanline(s).
    std::vector<uint8_t> m_img;

    // TODO: what about volumetric textures? do we keep an array of these very
    // large vectors? For the moment, these are just not supported.

    void init();

    bool basisu_basislz_compress();

    bool basisu_uastc_compress();

    bool write_ktx2();

    // void generate_mip_levels(const image_span<const std::byte>& base_lvl_image,
    //                          ImageInput& inputFile, uint32_t numMipLevels,
    //                          uint32_t layerIndex, uint32_t faceIndex,
    //                          uint32_t depthSliceIndex);
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
KtxOutput::open(const std::string& name, const ImageSpec& newspec,
                OpenMode mode)
{
    // TODO: verify the x, y, z limits (probably not 65535)
    // This does: m_spec = newspec
    if (!check_open(mode, newspec, { 0, 65535, 0, 65535, 0, 65535, 0, 4 }))
        return false;

    // Save name and spec for later use
    m_filename = name;

    // If not uint8, default to uint8 (HDR not yet supported)
    if (m_spec.format
        != TypeDesc::UINT8 /* && m_spec.format != TypeDesc::UINT16 */)
        m_spec.set_format(TypeDesc::UINT8);

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(m_filename))
        return false;

    std::string colorspace = m_spec.get_string_attribute("oiio:ColorSpace",
                                                         "srgb_rec709_scene");
    bool is_srgb           = colorspace == "srgb_rec709_scene";

    // TODO: get_int_attribute causes a segfault and I have no idea why ...
    // Weirdly, calling find_attribute directly (and checking the resulting
    // pointer) works, but not get_int_attribute ...

    // keep this commented in case we need it
    /*
    cspan<uint8_t> dfd;
    ParamValue* dfdQ = m_spec.find_attribute("ktx:dfd", TypeDesc::UINT8);
    if (dfdQ) {
        dfd = dfdQ->as_cspan<uint8_t>();
        std::cout << "[ktxoutput] found dfd with len: " << dfdQ->nvalues()
                  << '\n';
    }

    // Copy dfd data from const span because ktxTextureCreateInfo does not take
    // a const uint8_t ptr.
    ktx_uint32_t* pDfd { nullptr };
    std::vector<uint8_t> dfd_copy;
    if (!dfd.empty()) {
        dfd_copy.assign(dfd.begin(), dfd.end());
        pDfd = reinterpret_cast<ktx_uint32_t*>(dfd_copy.data());
        std::cout << "pDfd is set to != nullptr" << '\n';
    }
    */

    //
    // Use sensible default in case the input data did not originate from a KTX2
    // file and the user did not provide a supercompression scheme. KTX2 usually
    // uses Basis LZ supercompression scheme to benefit from both: smaller
    // disk filesizes and on-the-fly transcoding to a supported native GPU
    // format.
    //
    ParamValue* superCmpSchemQ
        = m_spec.find_attribute("ktx:supercompressionscheme", TypeDesc::UINT32);
    if (superCmpSchemQ) {
        m_superCmp = static_cast<ktxSupercmpScheme>(
            *reinterpret_cast<const uint32_t*>(superCmpSchemQ->data()));
        // std::cout << "[ktxoutput] found supercompression scheme: " << m_superCmp
        //           << '\n';
    }


    // Get transfer function (for color space conversions)
    // ParamValue* tfQ = m_spec.find_attribute("ktx:transferfunction",
    //                                         TypeDesc::UINT32);
    // if (tfQ) {
    //     m_tf = static_cast<khr_df_transfer_e>(
    //         *reinterpret_cast<const uint32_t*>(tfQ->data()));
    //     std::cout << "[ktxoutput] found tf: " << m_tf << '\n';
    // }

    // Get color model (to detect GPU compression, Basis Universal format, etc.)
    ParamValue* colorModelQ = m_spec.find_attribute("ktx:colormodel",
                                                    TypeDesc::UINT32);
    if (colorModelQ) {
        m_colormodel = static_cast<khr_df_model_e>(
            *reinterpret_cast<const uint32_t*>(colorModelQ->data()));
        // std::cout << "[ktxoutput] found color model: " << m_colormodel << '\n';
    }

    // Do an early check on supported supercompressionscheme values
    if (m_superCmp != KTX_SS_BASIS_LZ && m_superCmp != KTX_SS_NONE) {
        // doing an `errorfmt()` then `close()` causes a seg fault...
        close();
        errorfmt("unsupported super compression scheme: {}",
                 static_cast<uint32_t>(m_superCmp));
        return false;
    }

    //
    // If provided, get target VkFormat explicitly set via the "ktx:vkformat"
    // attribute.
    //
    auto vkFormat         = VkFormat::VK_FORMAT_UNDEFINED;
    ParamValue* vkFormatQ = m_spec.find_attribute("ktx:vkformat",
                                                  TypeDesc::UINT32);
    if (vkFormatQ) {
        vkFormat = static_cast<VkFormat>(
            *reinterpret_cast<const uint32_t*>(vkFormatQ->data()));
        // Get GPU-block-compression from provided VkFormat
        if (vkFormat != VK_FORMAT_UNDEFINED) {
            FormatInfo format_info;
            if (!get_info_from_vkformat(vkFormat, format_info)) {
                close();
                errorfmt("Could not extract format info from provided "
                         "VkFormat: {}. This format is probably unsupported.",
                         static_cast<uint32_t>(vkFormat));
                return false;
            }
            m_cmp = format_info.compression;
        }
        // std::cout << "[ktxoutput] found vkformat: " << vkFormat << '\n';
    }

    //
    // User provided nothing about neither the target VkFormat nor the target
    // super-compression scheme. Choose a sane default from provided spec (i.e.,
    // nchannels + bit depth). Two choices: compress to raw format and nullify
    // the benefit of writing a KTX2 at the benefit of a losseless write. Or,
    // use UASTC format which is what is typically used within KTX.
    //
    if (vkFormat == VK_FORMAT_UNDEFINED
        && m_colormodel == KHR_DF_MODEL_UNSPECIFIED) {
        if (m_spec.format != TypeDesc::UINT8) {
            close();
            errorfmt("Non-LDR input is not yet supported.");
            return false;
        }
        m_colormodel = KHR_DF_MODEL_UASTC;
    }

    //
    // Since we are using libktx's ktxTexture2_CompressAstc, the format has to
    // be set to an uncompressed VkFormat otherwise we get KTX_INVALID_OPERATION
    // error code.
    //
    if (m_cmp == BlockCompression::ASTC)
        vkFormat = VK_FORMAT_R8G8B8A8_SRGB;

    //
    // If a Basis Universal format compression is not requested and
    // "ktx:vkformat" is VK_FORMAT_UNDEFINED, then we error out. The user has to
    // set the vkformat so that we know in which format we write the texture to.
    //
    if ((m_colormodel != KHR_DF_MODEL_ETC1S
         && m_colormodel != KHR_DF_MODEL_UASTC)
        && vkFormat == VK_FORMAT_UNDEFINED) {
        close();
        errorfmt(
            "VkFormat is set to VK_FORMAT_UNDEFINED even though the "
            "supercompression scheme is not BasisLZ. You have to set the "
            "target VkFormat by setting the ImageSpec's attribute 'ktx:vkformat'.");
        return false;
    }

    //
    // If we intend to compress to BasisLZ/ETC1S or UASTC then we need to figure
    // the VkFormat so that ktxTexture_SetImageFromMemory does not segfault.
    // (makes sense, since we are creating a KTX texture and telling it to
    // allocate storage, how would it know the size of a given subimage if we
    // provide it with VK_FORMAT_UNDEFINED?)
    //
    if ((m_colormodel == KHR_DF_MODEL_ETC1S
         || m_colormodel == KHR_DF_MODEL_UASTC)
        && vkFormat == VK_FORMAT_UNDEFINED) {
        vkFormat = get_vkformat_from_info(m_spec.nchannels, m_spec.format,
                                          is_srgb);
    } else if (m_colormodel == KHR_DF_MODEL_ETC1S
               || m_colormodel == KHR_DF_MODEL_UASTC) {
        // TODO: It could be that the user explicitly provided a vkformat - in which
        // case we have to make sure it aligns with the spec.
        // if (!is_vkformat_aligned_with_spec()) ...
        close();
        errorfmt("Expected vkformat to be VK_FORMAT_UNDEFINED for Basis "
                 "Universal (UASTC or ETC1S) target KTX textures.");
        return false;
    }

    // get number of mip levels
    m_miplevels            = 1;
    ParamValue* miplevelsQ = m_spec.find_attribute("ktx:miplevels",
                                                   TypeDesc::UINT32);
    if (miplevelsQ) {
        m_miplevels = *reinterpret_cast<const uint32_t*>(miplevelsQ->data());
    }

    if (m_miplevels > 1) {
        close();
        errorfmt("Cannot re-generate mip levels because there is no way to "
                 "know the original filter that was used to generate them.");
        return false;
    }

    //
    // TODO: sanity checks on provided attributes (e.g., certain
    // supercompression schemes cannot be applied to certain basisu formats,
    // etc.)
    //

    // get number of layers
    m_nlayers            = 1;
    ParamValue* nlayersQ = m_spec.find_attribute("ktx:nlayers",
                                                 TypeDesc::UINT32);
    if (nlayersQ) {
        m_nlayers = *reinterpret_cast<const uint32_t*>(nlayersQ->data());
    }

    // std::cout << "vkformat: " << vkFormat << '\n';
    // std::cout << "mip levels: " << m_miplevels << '\n';
    // std::cout << "nlayers: " << m_nlayers << '\n';
    // std::cout << "[width, height, depth]: [" << m_spec.width << ", "
    //           << m_spec.height << ", " << m_spec.depth << "] \n";

    // TODO: avoid some of these static casts into ktx_uint*_t types by storing
    // uint*_t as unsigned integers and not as integers.
    OIIO_ASSERT(vkFormat != VK_FORMAT_UNDEFINED);  // otherwise segfault
    ktxTextureCreateInfo create_info;
    create_info.glInternalformat = 0;  // Ignored as we'll create a KTX2 texture
    create_info.vkFormat         = vkFormat;
    create_info.pDfd             = nullptr;
    create_info.baseWidth        = static_cast<ktx_uint32_t>(m_spec.width);
    create_info.baseHeight       = static_cast<ktx_uint32_t>(m_spec.height);
    create_info.baseDepth        = static_cast<ktx_uint32_t>(m_spec.depth);
    create_info.numDimensions    = 2;  // TODO: this is currently hardcoded
    create_info.numLevels        = 1;  // static_cast<ktx_uint32_t>(m_miplevels)
    create_info.numLayers        = 1;  // static_cast<ktx_uint32_t>(nlayers)
    create_info.numFaces         = 1;  // TODO: this is currently hardcoded
    create_info.isArray          = KTX_FALSE;
    create_info.generateMipmaps  = KTX_FALSE;

    ktxTexture2* p_tex = nullptr;
    auto result        = ktxTexture2_Create(&create_info,
                                            KTX_TEXTURE_CREATE_ALLOC_STORAGE, &p_tex);
    m_tex.reset(p_tex);

    if (result != KTX_SUCCESS) {
        close();
        errorfmt("ktxTexture_Create return KTX exit error code: {}",
                 static_cast<uint32_t>(result));
        return false;
    }

    // Reserve space for base level mipmap
    if (!m_tex->isCompressed) {
        m_img.resize(
            ktxTexture_GetImageSize(reinterpret_cast<ktxTexture*>(m_tex.get()),
                                    0));
    } else {
        // TODO:
        // Not compressed => make sure that vector's size matches the expected
        // size from the set raw VkFormat:
        // (e.g., VK_FORMAT_R8G8_SRGB => width * height * 3 )
        m_img.resize(m_spec.scanline_bytes() * m_spec.height);
    }

    return true;
}



bool
KtxOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    return write_scanlines(y, y + 1, z, format, data, xstride);
}



// TODO: use the span alternative. Apparently, there isn't one that takes a
// depth parameter (i.e., z).
bool
KtxOutput::write_scanlines(int ybegin, int yend,
                           int z /* slice or face or layer */, TypeDesc format,
                           const void* data, stride_t xstride, stride_t ystride)
{
    // std::cout << "write_scanlines called with: ybegin=" << ybegin
    //           << "; yend=" << yend << "; z=" << z << "; format=" << format
    //           << "; xstride=" << xstride << '\n';

    stride_t zstride = AutoStride;
    m_spec.auto_stride(xstride, ystride, zstride, format, spec().nchannels,
                       m_spec.width, m_spec.height);
    // const void* origdata = data;

    // to_native_rectangle will do this check and assignment. Keep it here for
    // consistency with JPEG writer
    if (format == TypeUnknown)
        format = m_spec.format;

    //
    // Convert to the native format the current specs expects. This is needed,
    // for instance, to convert a given TypeDesc::FLOAT into native format that
    // this KTX2 writer expects (i.e., TypeDesc::UINT8 or TypeDesc::UINT16).
    //
    // Returned data pointer may be the same as the provided pointer (i.e., no
    // conversion is needed because supplied data is already in native format).
    //
    data = to_native_rectangle(m_spec.x, m_spec.x + m_spec.width, ybegin, yend,
                               z, z + 1, format, data, xstride, ystride,
                               zstride, m_scratch, false, 0, ybegin, z);

    // data should now be contiguous and of the expected format (UINT8 or
    // UINT16) so simply memcpy into internal buffer that will be written on
    // close().
    const size_t pitch = m_spec.scanline_bytes();
    auto pSrc          = reinterpret_cast<const uint8_t*>(data);
    size_t offset      = ybegin * pitch;
    size_t datalen     = (yend - ybegin) * pitch;

    memcpy(m_img.data() + offset, pSrc, datalen);
    // std::cout << "write_scanlines success" << '\n';
    return true;
}



bool
KtxOutput::close()
{
    // Check if already closed => if so, then the KTX2 file is already saved
    if (!ioproxy_opened()) {
        init();
        return true;
    }

    bool result = true;
    if (m_tex) {
        // Apparently we can't do (or I don't know yet how to) partial writes
        // using libktx. We can only write whole ktxTextures all together.
        result = write_ktx2();  // TODO: can this throw? (prob not)
    }
    init();
    return result;
}



void
KtxOutput::init()
{
    // TODO: calling open() after close() on this hasn't been testes yet ...
    m_tex        = nullptr;
    m_superCmp   = KTX_SS_NONE;
    m_colormodel = KHR_DF_MODEL_UNSPECIFIED;
    // TODO: other stuff...
    ioproxy_clear();
}



//
// Applies BasisLZ/ETC1S supercompression to this KTX2 texture. The ImageSpec is
// queried (searched) for attribute that determine the BasisLZ/ETC1S compression
// params (see ktxBasisParams struct in libktx).
//
bool
KtxOutput::basisu_basislz_compress()
{
    // TODO: retrieve BasisLZ/ETC1S compression params. `ktx info` prints some
    // Basis Supercompression Global Data that might be useful in figuring out
    // what params the original data was compressed with so that we can reproduce
    // it.
    // TODO: expose as "ktx:" attribute(s)
    ktxBasisParams params = { 0 };
    params.structSize     = sizeof(ktxBasisParams);
#if Ktx_VERSION >= OIIO_MAKE_VERSION(5, 0, 0) || Ktx_VERSION == Ktx_VERSIONLESS
    params.codec                 = ktx_basis_codec_e::KTX_BASIS_CODEC_ETC1S;
    params.etc1sCompressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
#else
    params.uastc            = false;
    params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
#endif
    params.verbose     = false;
    params.noSSE       = false;
    params.threadCount = 1;
    // TODO: expose RDO support for ETC1S
    if (auto status = ktxTexture2_CompressBasisEx(m_tex.get(), &params);
        status != KTX_SUCCESS) {
        errorfmt("ktxTexture2_CompressBasisEx returned error code: ",
                 static_cast<uint32_t>(status));
        return false;
    }
    return true;
}



//
// Applies UASTC basis universal 'compression' to this KTX2 texture.
// The ImageSpec is queried (searched) for attribute that determine the UASTC
// compression params (see ktxBasisParams struct in libktx).
//
bool
KtxOutput::basisu_uastc_compress()
{
    // TODO: expose parameters
    ktxBasisParams params = { 0 };
    params.structSize     = sizeof(ktxBasisParams);
#if Ktx_VERSION >= OIIO_MAKE_VERSION(5, 0, 0) || Ktx_VERSION == Ktx_VERSIONLESS
    params.codec = ktx_basis_codec_e::KTX_BASIS_CODEC_UASTC_LDR_4x4;
#else
    params.uastc = true;
#endif
    params.verbose     = false;
    params.noSSE       = false;
    params.threadCount = 1;
    params.uastcFlags  = KTX_PACK_UASTC_LEVEL_DEFAULT;
    params.uastcRDO    = false;
    // TODO: expose RDO support for UASTC
    if (auto status = ktxTexture2_CompressBasisEx(m_tex.get(), &params);
        status != KTX_SUCCESS) {
        errorfmt("ktxTexture2_CompressBasisEx returned error code: ",
                 static_cast<uint32_t>(status));
        return false;
    }
    return true;
}



bool
KtxOutput::write_ktx2()
{
    // TODO: this attribute should be ignored in testing.
    // Add/overwrite the KTXwriter metadata entry. The specs encourages us to do
    // so.
    char writer[100];
    snprintf(writer, sizeof(writer), "oiio version %d - plugin version %d",
             OPENIMAGEIO_VERSION, OIIO_PLUGIN_VERSION);
    ktxHashList_AddKVPair(&m_tex->kvDataHead, KTX_WRITER_KEY,
                          (ktx_uint32_t)strlen(writer) + 1, writer);
    // std::cout << "KTXwrite: " << writer << '\n';

    //
    // In case data was read from an input KTX2 file with mipmaps, we have to
    // write the base level then generate mipmaps up to the specified level
    // (via the "ktx:miplevels" attribute). KTX-Software (not necessarily
    // libktx), surely has a function somewhere that generates these mipmaps.
    // Ideally, we should follow the exact same implementation used in
    // KTX-Software to generate the mipmaps.
    //
    // Note 1:
    //  You may notice the `generateMipmaps` flag in the ktxTexture struct, it
    //  is just used to instruct Vulkan or OpenGL to generate mipmaps for the
    //  texture to be uploaded NOT for mipmap generation on the CPU.
    //
    // Note 2:
    //  There is apparently no metadata to know which filter (+ params) that
    //  was used to generate the mipmaps.
    //
    // Important:
    //  If the VkFormat related to the KTX texture creation is wrongly set, this
    //  will cause a segfault!
    //
    if (!m_tex->isCompressed) {
        if (auto status = ktxTexture_SetImageFromMemory(ktxTexture(m_tex.get()),
                                                        0, 0, 0, m_img.data(),
                                                        m_img.size());
            status != KTX_SUCCESS) {
            errorfmt(
                "ktxTexture_SetImageFromMemory returned KTX exit error code: {}",
                static_cast<uint32_t>(status));
            return false;
        }
    } else if (m_cmp == BlockCompression::BC1 || m_cmp == BlockCompression::BC3
               || m_cmp == BlockCompression::BC4
               || m_cmp == BlockCompression::BC5
               || m_cmp == BlockCompression::BC7) {
        // First set uncompressed texture
        // if (auto status = ktxTexture_SetImageFromMemory(ktxTexture(m_tex), 0, 0,
        //                                                 0, m_img.data(),
        //                                                 m_img.size());
        //     status != KTX_SUCCESS) {
        //     errorfmt(
        //         "ktxTexture_SetImageFromMemory returned KTX exit error code: {}",
        //         static_cast<uint32_t>(status));
        //     return false;
        // }

        // Then compress the whole texture to BCn format
        // TODO: expose BCn compression quality parameter as spec attribute
        // if (auto status = ktxTexture2_CompressBCn(m_tex, nullptr);
        //     status != KTX_SUCCESS) {
        //     errorfmt("ktxTexture2_CompressBCn returned KTX exit error code: {}",
        //              static_cast<uint32_t>(status));
        //     return false;
        // }
        errorfmt("Writing/Encoding BCn compression is not yet supported.");
        return false;
    } else if (m_cmp == BlockCompression::ASTC) {
        // First set uncompressed images
        if (auto status = ktxTexture_SetImageFromMemory(
                reinterpret_cast<ktxTexture*>(m_tex.get()), 0, 0, 0,
                m_img.data(), m_img.size());
            status != KTX_SUCCESS) {
            errorfmt(
                "ktxTexture_SetImageFromMemory returned KTX exit error code: {}",
                static_cast<uint32_t>(status));
            return false;
        }

        // Then compress the whole texture to ASTC format
        // TODO: expose ASTC compression quality parameter as spec attribute
        if (auto status = ktxTexture2_CompressAstc(m_tex.get(), 0);
            status != KTX_SUCCESS) {
            errorfmt("ktxTexture2_CompressAstc returned KTX exit error code: {}",
                     static_cast<uint32_t>(status));
            return false;
        }
    }

    // If basis universal compression is requested (i.e., to BasisLZ/ETC1S
    // or UASTC), compress the texture before writing.
    if (m_colormodel == KHR_DF_MODEL_ETC1S) {
        if (!basisu_basislz_compress())
            return false;
    } else if (m_colormodel == KHR_DF_MODEL_UASTC) {
        if (!basisu_uastc_compress())
            return false;
    }

    // Finally, apply the supercompression scheme (if any)
    if (m_superCmp == KTX_SS_ZLIB) {
        if (auto status = ktxTexture2_DeflateZLIB(m_tex.get(), 0);
            status != KTX_SUCCESS) {
            errorfmt("ktxTexture2_DeflateZLIB returned KTX exit error code: {}",
                     static_cast<uint32_t>(status));
            return false;
        }

    } else if (m_superCmp == KTX_SS_ZSTD) {
        if (auto status = ktxTexture2_DeflateZstd(m_tex.get(), 0);
            status != KTX_SUCCESS) {
            errorfmt("ktxTexture2_DeflateZstd returned KTX exit error code: {}",
                     static_cast<uint32_t>(status));
            return false;
        }
    }

    Filesystem::IOProxy* m_io = ioproxy();
    if (!strcmp(m_io->proxytype(), "file")) {
        auto fd = reinterpret_cast<Filesystem::IOFile*>(m_io)->handle();
        if (auto status = ktxTexture_WriteToStdioStream(
                reinterpret_cast<ktxTexture*>(m_tex.get()), fd);
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
        if (auto status = ktxTexture_WriteToMemory(
                reinterpret_cast<ktxTexture*>(m_tex.get()), &buff, &buff_size);
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



// void
// KtxOutput::generate_mip_levels(const image_span<const std::byte>& base_lvl_image,
//                                ImageInput& inputFile, uint32_t numMipLevels,
//                                uint32_t layerIndex, uint32_t faceIndex,
//                                uint32_t depthSliceIndex)
// {
//     //if (isFormatINT(static_cast<VkFormat>(texture->vkFormat)))
//     //    fatal(rc::NOT_SUPPORTED, "Mipmap generation for SINT or UINT format {} is not supported.",
//     //          toString(static_cast<VkFormat>(texture->vkFormat)));
//
//     for (uint32_t mipLevelIndex = 1; mipLevelIndex < numMipLevels;
//          ++mipLevelIndex) {
//         const auto mipImageWidth  = std::max(1u, m_tex->baseWidth
//                                                      >> (mipLevelIndex));
//         const auto mipImageHeight = std::max(1u, m_tex->baseHeight
//                                                      >> (mipLevelIndex));
//
//         ROI roi(0, mipImageHeight, 0, mipImageWidth, 0, 1, /*chans:*/ 0, 4);
//         ImageBuf dst = ImageBufAlgo::resample(Src, true, roi);
//         // if (options.normalize)
//         //     image->normalize();
//
//         // const auto imageData = convert(levelImage, options.vkFormat, inputFile,
//         //                                true);
//
//         const auto ret = ktxTexture_SetImageFromMemory(
//             m_tex, mipLevelIndex, layerIndex,
//             faceIndex
//                 + depthSliceIndex,  // Faces and Depths are mutually exclusive, Addition is acceptable
//             NULL, 0);
//         // (ret == KTX_SUCCESS && "Internal error");
//     }
// }

OIIO_PLUGIN_NAMESPACE_END

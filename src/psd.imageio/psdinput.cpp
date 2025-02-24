// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


//
// General information about PSD:
// https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
//


#include <csetjmp>
#include <functional>
#include <map>
#include <memory>
#include <vector>
#include <zlib.h>

#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/tiffutils.h>

// #include "jpeg_memory_src.h"
#include "psd_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace psd_pvt;


class PSDInput final : public ImageInput {
public:
    PSDInput();
    ~PSDInput() override { close(); }
    const char* format_name(void) const override { return "psd"; }
    int supports(string_view feature) const override
    {
        return (feature == "exif" || feature == "iptc" || feature == "thumbnail"
                || feature == "ioproxy");
    }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool close() override;
    int current_subimage() const override { return m_subimage; }
    bool seek_subimage(int subimage, int miplevel) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool get_thumbnail(ImageBuf& thumb, int subimage) override
    {
        thumb = m_thumbnail;
        return m_thumbnail.initialized();
    }

private:
    enum ColorMode {
        ColorMode_Bitmap       = 0,
        ColorMode_Grayscale    = 1,
        ColorMode_Indexed      = 2,
        ColorMode_RGB          = 3,
        ColorMode_CMYK         = 4,
        ColorMode_Multichannel = 7,
        ColorMode_Duotone      = 8,
        ColorMode_Lab          = 9
    };

    enum Compression {
        Compression_Raw         = 0,
        Compression_RLE         = 1,
        Compression_ZIP         = 2,
        Compression_ZIP_Predict = 3
    };

    enum ChannelID {
        ChannelID_Transparency = -1,
        ChannelID_LayerMask    = -2,
        ChannelID_UserMask     = -3
    };

    // Image resource loaders to handle loading certain image resources
    // into ImageSpec
    struct ResourceLoader {
        uint16_t resource_id;
        std::function<bool(PSDInput*, uint32_t)> load;
    };

    // Map image resource ID to image resource block
    typedef std::map<uint16_t, ImageResourceBlock> ImageResourceMap;

    struct ResolutionInfo {
        float hRes         = 0.0f;
        int16_t hResUnit   = 0;
        int16_t widthUnit  = 0;
        float vRes         = 0.0f;
        int16_t vResUnit   = 0;
        int16_t heightUnit = 0;

        enum ResolutionUnit { PixelsPerInch = 1, PixelsPerCentimeter = 2 };

        enum Unit {
            Inches      = 1,
            Centimeters = 2,
            Points      = 3,
            Picas       = 4,
            Columns     = 5
        };
    };

    struct LayerMaskInfo {
        uint64_t length;
        int64_t begin;
        int64_t end;

        struct LayerInfo {
            uint64_t length;
            int16_t layer_count;
            int64_t begin;
            int64_t end;
        };

        LayerInfo layer_info;
    };

    struct ChannelInfo {
        uint32_t row_length;
        int16_t channel_id;
        uint64_t data_length;
        int64_t data_pos;
        uint16_t compression;

        uint32_t width;
        uint32_t height;

        // This data is only relevant for the compression
        // codecs zip and zipprediction as we need to preallocate
        // the memory this vector is already decompressed and byteswapped
        std::vector<char> decompressed_data;

        std::vector<uint32_t> rle_lengths;
        std::vector<int64_t> row_pos;
    };

    struct Layer {
        uint32_t top, left, bottom, right;
        uint32_t width, height;
        uint16_t channel_count;

        std::vector<ChannelInfo> channel_info;
        std::map<int16_t, ChannelInfo*> channel_id_map;

        char bm_key[4];
        uint8_t opacity;
        uint8_t clipping;
        uint8_t flags;
        uint32_t extra_length;

        struct MaskData {
            uint32_t top, left, bottom, right;
            uint8_t default_color;
            uint8_t flags;
        };

        MaskData mask_data;

        //TODO: layer blending ranges?

        std::string name;

        struct AdditionalInfo {
            char key[4];
            uint64_t length;
            int64_t pos;
        };

        std::vector<AdditionalInfo> additional_info;
    };

    struct GlobalMaskInfo {
        uint16_t overlay_color_space;
        uint16_t color_components[4];
        uint16_t opacity;
        int8_t kind;
    };

    struct ImageDataSection {
        std::vector<ChannelInfo> channel_info;
        //When the layer count is negative, this is true and indicates that
        //the first alpha channel should be used as transparency (for the
        //merged image)
        bool transparency;
    };

    std::string m_filename;
    //Current subimage
    int m_subimage;
    //Subimage count (1 + layer count)
    int m_subimage_count;
    std::vector<ImageSpec> m_specs;
    static const ResourceLoader resource_loaders[];
    //This holds the attributes for the merged image (subimage 0)
    ImageSpec m_composite_attribs;
    //This holds common attributes that apply to all subimages
    ImageSpec m_common_attribs;
    //psd:RawData config option, indicates that the user wants the raw,
    //unconverted channel data
    bool m_WantRaw;
    TypeDesc m_type_desc;
    //This holds all the ChannelInfos for all subimages
    //Example: m_channels[subimg][channel]
    std::vector<std::vector<ChannelInfo*>> m_channels;
    //Alpha Channel Names, not currently used
    std::vector<std::string> m_alpha_names;
    //Index of the transparent color, if any (for Indexed color mode only)
    int16_t m_transparency_index;
    //Background color
    float m_background_color[4];
    ///< Do not convert unassociated alpha
    bool m_keep_unassociated_alpha;

    FileHeader m_header;
    ColorModeData m_color_data;
    LayerMaskInfo m_layer_mask_info;
    std::vector<Layer> m_layers;
    GlobalMaskInfo m_global_mask_info;
    ImageDataSection m_image_data;
    ImageBuf m_thumbnail;

    //Reset to initial state
    void init();

    //File Header
    bool load_header();
    bool read_header();
    bool validate_header();
    static bool validate_signature(const char signature[4]);

    //Color Mode Data
    bool load_color_data();
    bool validate_color_data();

    //Image Resources
    bool load_resources();
    bool read_resource(ImageResourceBlock& block);
    bool validate_resource(ImageResourceBlock& block);
    //Call the resource_loaders to load the resources into an ImageSpec
    //m_specs should be resized to m_subimage_count first
    bool handle_resources(ImageResourceMap& resources);
    //ResolutionInfo
    bool load_resource_1005(uint32_t length);
    //Alpha Channel Names
    bool load_resource_1006(uint32_t length);
    //Background Color
    bool load_resource_1010(uint32_t length);
    //JPEG thumbnail (Photoshop 4.0)
    bool load_resource_1033(uint32_t length);
    //JPEG thumbnail (Photoshop 5.0)
    bool load_resource_1036(uint32_t length);
    // ICC Profile (Photoshop 5.0)
    bool load_resource_1039(uint32_t length);
    //Transparency index (Indexed color mode)
    bool load_resource_1047(uint32_t length);
    //Exif data 1
    bool load_resource_1058(uint32_t length);
    //Exif data 3
    bool load_resource_1059(uint32_t length);
    //XMP metadata
    bool load_resource_1060(uint32_t length);
    //Pixel Aspect Ratio
    bool load_resource_1064(uint32_t length);

    //Load thumbnail resource, used for resources 1033 and 1036
    bool load_resource_thumbnail(uint32_t length, bool isBGR);

    //Layers
    bool load_layers();
    bool load_layer(Layer& layer);
    bool load_layer_channels(Layer& layer);
    bool load_layer_channel(Layer& layer, ChannelInfo& channel_info);
    bool read_rle_lengths(uint32_t height, std::vector<uint32_t>& rle_lengths);

    //Global Mask Info
    bool load_global_mask_info();

    //Global Additional Layer Info
    bool load_global_additional();

    //Layers for 16- and 32-bit documents
    bool load_layers_16_32(uint64_t length);

    //Image Data Section
    bool load_image_data();

    void set_type_desc();
    //Setup m_specs and m_channels
    void setup();
    void fill_channel_names(ImageSpec& spec, bool transparency);

    //Read a row of channel data
    bool read_channel_row(ChannelInfo& channel_info, uint32_t row, char* data);

    // Interleave channels (RRRGGGBBB -> RGBRGBRGB) while copying from
    // channel_buffers[0..nchans-1] to dst.
    template<typename T>
    static void
    interleave_row(T* dst, cspan<std::vector<unsigned char>> channel_buffers,
                   int width, int nchans);

    // Convert the channel data to RGB
    bool indexed_to_rgb(span<unsigned char> dst, cspan<unsigned char> src,
                        int width) const;
    bool bitmap_to_rgb(span<unsigned char> dst, cspan<unsigned char> src,
                       int width) const;

    // Convert from photoshop native alpha to
    // associated/premultiplied
    template<class T>
    OIIO_NO_SANITIZE_UNDEFINED void
    removeBackground(T* data, int size, int nchannels, int alpha_channel,
                     const float* background) const
    {
        // RGB = CompRGB - (1 - alpha) * Background;
        float scale = std::numeric_limits<T>::is_integer
                          ? 1.0f / float(std::numeric_limits<T>::max())
                          : 1.0f;
        for (; size; --size, data += nchannels)
            for (int c = 0; c < nchannels; c++)
                if (c != alpha_channel) {
                    float alpha = data[alpha_channel] * scale;
                    float f     = data[c];
                    data[c] = T(f - (((1.0f - alpha) * background[c]) / scale));
                }
    }

    template<class T>
    static void unassociateAlpha(T* data, int size, int nchannels,
                                 int alpha_channel, const float* background)
    {
        // RGB = (CompRGB - (1 - alpha) * Background) / alpha
        float scale = std::numeric_limits<T>::is_integer
                          ? 1.0f / float(std::numeric_limits<T>::max())
                          : 1.0f;

        for (; size; --size, data += nchannels)
            for (int c = 0; c < nchannels; c++)
                if (c != alpha_channel) {
                    float alpha = data[alpha_channel] * scale;
                    float f     = data[c];
                    if (alpha > 0.0f)
                        data[c] = T(
                            (f - (((1.0f - alpha) * background[c]) / scale))
                            / alpha);
                    else
                        data[c] = 0;
                }
    }

    template<class T>
    static void associateAlpha(T* data, int size, int nchannels,
                               int alpha_channel)
    {
        float scale = std::numeric_limits<T>::is_integer
                          ? 1.0f / float(std::numeric_limits<T>::max())
                          : 1.0f;
        for (; size; --size, data += nchannels)
            for (int c = 0; c < nchannels; c++)
                if (c != alpha_channel) {
                    float f = data[c];
                    data[c] = T(f * (data[alpha_channel] * scale));
                }
    }

    void background_to_assocalpha(int n, void* data, int nchannels,
                                  int alpha_channel, TypeDesc format) const;
    void background_to_unassalpha(int n, void* data, int nchannels,
                                  int alpha_channel, TypeDesc format) const;
    void unassalpha_to_assocalpha(int n, void* data, int nchannels,
                                  int alpha_channel, TypeDesc format) const;

    template<typename T>
    static void cmyk_to_rgb(int n, cspan<T> cmyk, size_t cmyk_stride,
                            span<T> rgb, size_t rgb_stride)
    {
        OIIO_DASSERT(size_t(n) * cmyk_stride <= std::size(cmyk));
        OIIO_DASSERT(size_t(n) * rgb_stride <= std::size(rgb));
        for (int i = 0; i < n; ++i) {
            float C = convert_type<T, float>(cmyk[i * cmyk_stride + 0]);
            float M = convert_type<T, float>(cmyk[i * cmyk_stride + 1]);
            float Y = convert_type<T, float>(cmyk[i * cmyk_stride + 2]);
            float K = convert_type<T, float>(cmyk[i * cmyk_stride + 3]);
#if 0
            // WHY doesn't this work if it's cmyk?
            float R = (1.0f - C) * (1.0f - K);
            float G = (1.0f - M) * (1.0f - K);
            float B = (1.0f - Y) * (1.0f - K);
#else
            // But this gives the right results????? WTF?
            // Is it because it's subtractive and PhotoShop records it
            // as MAX-val?
            float R = C * (K);
            float G = M * (K);
            float B = Y * (K);
#endif
            rgb[i * rgb_stride + 0] = convert_type<float, T>(R);
            rgb[i * rgb_stride + 1] = convert_type<float, T>(G);
            rgb[i * rgb_stride + 2] = convert_type<float, T>(B);

            if (cmyk_stride == 5 && rgb_stride == 4) {
                rgb[i * rgb_stride + 3] = convert_type<float, T>(
                    cmyk[i * cmyk_stride + 4]);
            }
        }
    }

    //This may be a bit inefficient but I think it's worth the convenience.
    //This takes care of things like reading a 32-bit BE into a 64-bit LE.
    template<typename TStorage, typename TVariable>
    bool read_bige(TVariable& value)
    {
        TStorage buffer;
        if (!ioread((char*)&buffer, sizeof(buffer)))
            return false;
        if (!bigendian())
            swap_endian(&buffer);
        value = buffer;
        return true;
    }

    int read_pascal_string(std::string& s, uint16_t mod_padding);

    // Swap a planar bytespan representing the bytes of a float vector to its
    // interleaved byte order. This is per scanline
    void float_planar_to_interleaved(span<char> data, size_t width,
                                     size_t height);

    // All the compression modes known to photoshop
    bool decompress_packbits(const char* src, char* dst, uint32_t packed_length,
                             uint32_t unpacked_length);
    bool decompress_zip(span<char> src, span<char> dest);
    bool decompress_zip_prediction(span<char> src, span<char> dest,
                                   const uint32_t width, const uint32_t height);

    // These are AdditionalInfo entries that, for PSBs, have an 8-byte length
    static const char* additional_info_psb[];
    static const unsigned int additional_info_psb_count;
    bool is_additional_info_psb(const char* key);

    // Channel names and counts for each color mode
    static const char* mode_channel_names[][4];
    static const unsigned int mode_channel_count[];

    // Some attributes may apply to only the merged composite.
    // Others may apply to all subimages.
    // These functions are intended to be used by image resource loaders.
    //
    // Add an attribute to the composite image spec
    template<typename T>
    void composite_attribute(const std::string& name, const T& value)
    {
        m_composite_attribs.attribute(name, value);
    }

    // Add an attribute to the composite image spec
    template<typename T>
    void composite_attribute(const std::string& name, const TypeDesc& type,
                             const T& value)
    {
        m_composite_attribs.attribute(name, type, value);
    }

    // Add an attribute to the composite image spec and common image spec
    template<typename T>
    void common_attribute(const std::string& name, const T& value)
    {
        m_composite_attribs.attribute(name, value);
        m_common_attribs.attribute(name, value);
    }

    // Add an attribute to the composite image spec and common image spec
    template<typename T>
    void common_attribute(const std::string& name, const TypeDesc& type,
                          const T& value)
    {
        m_composite_attribs.attribute(name, type, value);
        m_common_attribs.attribute(name, type, value);
    }
};



// Image resource loaders
// To add an image resource loader, do the following:
// 1) Add ADD_LOADER(<ResourceID>) below
// 2) Add a method in PSDInput:
//    bool load_resource_<ResourceID> (uint32_t length);
#define ADD_LOADER(id)                                                      \
    {                                                                       \
        id, std::bind(&PSDInput::load_resource_##id, std::placeholders::_1, \
                      std::placeholders::_2)                                \
    }
const PSDInput::ResourceLoader PSDInput::resource_loaders[]
    = { ADD_LOADER(1005), ADD_LOADER(1006), ADD_LOADER(1010), ADD_LOADER(1033),
        ADD_LOADER(1036), ADD_LOADER(1039), ADD_LOADER(1047), ADD_LOADER(1058),
        ADD_LOADER(1059), ADD_LOADER(1060), ADD_LOADER(1064) };
#undef ADD_LOADER



const char* PSDInput::additional_info_psb[]
    = { "LMsk", "Lr16", "Lr32", "Layr", "Mt16", "Mt32", "Mtrn",
        "Alph", "FMsk", "Ink2", "FEid", "FXid", "PxSD", "cinf" };

const unsigned int PSDInput::additional_info_psb_count
    = sizeof(additional_info_psb) / sizeof(additional_info_psb[0]);

const char* PSDInput::mode_channel_names[][4] = {
    { "A" }, { "I" }, { "I" }, { "R", "G", "B" }, { "C", "M", "Y", "K" }, {},
    {},      {},      {},      { "L", "a", "b" }
};

const unsigned int PSDInput::mode_channel_count[] = { 1, 1, 1, 3, 4,
                                                      0, 0, 0, 0, 3 };



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput*
psd_input_imageio_create()
{
    return new PSDInput;
}

OIIO_EXPORT int psd_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
psd_imageio_library_version()
{
    return nullptr;
}

OIIO_EXPORT const char* psd_input_extensions[] = { "psd", "pdd", "psb",
                                                   nullptr };

OIIO_PLUGIN_EXPORTS_END



PSDInput::PSDInput() { init(); }


bool
PSDInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Mode::Read)
        return false;

    char signature[4] {};
    const size_t numRead = ioproxy->pread(signature, sizeof(signature), 0);
    return numRead == sizeof(signature) && validate_signature(signature);
}



bool
PSDInput::open(const std::string& name, ImageSpec& newspec)
{
    m_filename = name;
    if (!ioproxy_use_or_open(name))
        return false;
    ioseek(0);

    // File Header
    if (!load_header()) {
        errorfmt("failed to open \"{}\": failed load_header", name);
        return false;
    }

    // Color Mode Data
    if (!load_color_data()) {
        errorfmt("failed to open \"{}\": failed load_color_data", name);
        return false;
    }

    // Image Resources
    if (!load_resources()) {
        errorfmt("failed to open \"{}\": failed load_resources", name);
        return false;
    }

    // Layers
    if (!load_layers()) {
        errorfmt("failed to open \"{}\": failed load_layers", name);
        return false;
    }

    // Global Mask Info
    if (!load_global_mask_info()) {
        errorfmt("failed to open \"{}\": failed load_global_mask_info", name);
        return false;
    }

    // Global Additional Layer Info
    if (!load_global_additional()) {
        errorfmt("failed to open \"{}\": failed load_global_additional", name);
        return false;
    }

    // Image Data
    if (!load_image_data()) {
        errorfmt("failed to open \"{}\": failed load_image_data", name);
        return false;
    }

    // Layer count + 1 for merged composite (Image Data Section)
    m_subimage_count = m_layers.size() + 1;
    // Set m_type_desc to the appropriate TypeDesc
    set_type_desc();
    // Setup ImageSpecs and m_channels
    setup();

    bool ok = seek_subimage(0, 0);
    if (ok)
        newspec = spec();
    else
        close();
    return ok;
}



bool
PSDInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    m_WantRaw = config.get_int_attribute("psd:RawData")
                || config.get_int_attribute("oiio:RawColor");

    if (config.get_int_attribute("oiio:UnassociatedAlpha", 0) == 1)
        m_keep_unassociated_alpha = true;

    ioproxy_retrieve_from_config(config);

    return open(name, newspec);
}



bool
PSDInput::close()
{
    init();
    return true;
}



bool
PSDInput::seek_subimage(int subimage, int miplevel)
{
    if (miplevel != 0)
        return false;
    if (subimage == m_subimage)
        return true;  // Early return when not changing subimages
    if (subimage < 0 || subimage >= m_subimage_count)
        return false;

    m_subimage = subimage;
    m_spec     = m_specs[subimage];
    return true;
}



void
PSDInput::background_to_assocalpha(int n, void* data, int nchannels,
                                   int alpha_channel, TypeDesc format) const
{
    switch (format.basetype) {
    case TypeDesc::UINT8:
        removeBackground((unsigned char*)data, n, nchannels, alpha_channel,
                         m_background_color);
        break;
    case TypeDesc::UINT16:
        removeBackground((unsigned short*)data, n, nchannels, alpha_channel,
                         m_background_color);
        break;
    case TypeDesc::UINT32:
        removeBackground((unsigned long*)data, n, nchannels, alpha_channel,
                         m_background_color);
        break;
    case TypeDesc::FLOAT:
        removeBackground((float*)data, n, nchannels, alpha_channel,
                         m_background_color);
        break;
    default: break;
    }
}



void
PSDInput::background_to_unassalpha(int n, void* data, int nchannels,
                                   int alpha_channel, TypeDesc format) const
{
    switch (format.basetype) {
    case TypeDesc::UINT8:
        unassociateAlpha((unsigned char*)data, n, nchannels, alpha_channel,
                         m_background_color);
        break;
    case TypeDesc::UINT16:
        unassociateAlpha((unsigned short*)data, n, nchannels, alpha_channel,
                         m_background_color);
        break;
    case TypeDesc::UINT32:
        unassociateAlpha((unsigned long*)data, n, nchannels, alpha_channel,
                         m_background_color);
        break;
    case TypeDesc::FLOAT:
        unassociateAlpha((float*)data, n, nchannels, alpha_channel,
                         m_background_color);
        break;
    default: break;
    }
}



void
PSDInput::unassalpha_to_assocalpha(int n, void* data, int nchannels,
                                   int alpha_channel, TypeDesc format) const
{
    switch (format.basetype) {
    case TypeDesc::UINT8:
        associateAlpha((unsigned char*)data, n, nchannels, alpha_channel);
        break;
    case TypeDesc::UINT16:
        associateAlpha((unsigned short*)data, n, nchannels, alpha_channel);
        break;
    case TypeDesc::UINT32:
        associateAlpha((unsigned long*)data, n, nchannels, alpha_channel);
        break;
    case TypeDesc::FLOAT:
        associateAlpha((float*)data, n, nchannels, alpha_channel);
        break;
    default: break;
    }
}



bool
PSDInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    if (subimage < 0 || subimage >= m_subimage_count || miplevel != 0)
        return false;
#if 0
    // FIXME: is this lock or the seek_subimage necessary at all?
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
#endif
    // The subimage specs were fully read by open(), so we can access them
    // here safely.
    const ImageSpec& spec = m_specs[subimage];
    y -= spec.y;
    if (y < 0 || y > spec.height) {
        errorfmt("Requested scanline {} out of range [0-{}]", y,
                 spec.height - 1);
        return false;
    }

    // Buffers for channel data, one per channel
    std::vector<std::vector<unsigned char>> channel_buffers;
    channel_buffers.resize(m_channels[subimage].size());

    int bps = (m_header.depth + 7) / 8;  // bytes per sample
    OIIO_DASSERT(bps == 1 || bps == 2 || bps == 4);
    std::vector<ChannelInfo*>& channels = m_channels[subimage];
    int channel_count                   = (int)channels.size();
    for (int c = 0; c < channel_count; ++c) {
        ChannelInfo& channel_info = *channels[c];
        channel_buffers[c].resize(channel_info.row_length);
        if (!read_channel_row(channel_info, y, (char*)channel_buffers[c].data()))
            return false;
    }
    // OIIO_ASSERT(m_channels[subimage].size() == size_t(spec.nchannels));
    char* dst = (char*)data;
    if (m_WantRaw || m_header.color_mode == ColorMode_RGB
        || m_header.color_mode == ColorMode_Multichannel
        || m_header.color_mode == ColorMode_Grayscale) {
        switch (bps) {
        case 4:
            interleave_row((float*)dst, channel_buffers, spec.width,
                           spec.nchannels);
            break;
        case 2:
            interleave_row((unsigned short*)dst, channel_buffers, spec.width,
                           spec.nchannels);
            break;
        default:
            interleave_row((unsigned char*)dst, channel_buffers, spec.width,
                           spec.nchannels);
            break;
        }
    } else if (m_header.color_mode == ColorMode_CMYK) {
        span_size_t cmyklen = channel_count * spec.width;
        switch (bps) {
        case 4: {
            std::unique_ptr<float[]> cmyk(new float[cmyklen]);
            interleave_row(cmyk.get(), channel_buffers, spec.width,
                           channel_count);
            cmyk_to_rgb(spec.width, make_cspan(cmyk.get(), cmyklen),
                        channel_count,
                        make_span((float*)dst, spec.width * spec.nchannels),
                        spec.nchannels);
            break;
        }
        case 2: {
            std::unique_ptr<unsigned short[]> cmyk(new unsigned short[cmyklen]);
            interleave_row(cmyk.get(), channel_buffers, spec.width,
                           channel_count);
            cmyk_to_rgb(spec.width, make_cspan(cmyk.get(), cmyklen),
                        channel_count,
                        make_span((uint16_t*)dst, spec.width * spec.nchannels),
                        spec.nchannels);
            break;
        }
        default: {
            std::unique_ptr<unsigned char[]> cmyk(new unsigned char[cmyklen]);
            interleave_row(cmyk.get(), channel_buffers, spec.width,
                           channel_count);
            cmyk_to_rgb(spec.width, make_cspan(cmyk.get(), cmyklen),
                        channel_count,
                        make_span((uint8_t*)dst, spec.width * spec.nchannels),
                        spec.nchannels);
            break;
        }
        }
    } else if (m_header.color_mode == ColorMode_Indexed) {
        if (!indexed_to_rgb({ (unsigned char*)dst,
                              span_size_t(spec.width * spec.nchannels) },
                            channel_buffers[0], spec.width))
            return false;
    } else if (m_header.color_mode == ColorMode_Bitmap) {
        if (!bitmap_to_rgb({ (unsigned char*)dst,
                             span_size_t(spec.width * spec.nchannels) },
                           channel_buffers[0], spec.width))
            return false;
    } else {
        errorfmt("Unknown color mode: {:d}", m_header.color_mode);
        OIIO_ASSERT(0 && "unknown color mode");
        return false;
    }

    // PSD specifically dictates unassociated (un-"premultiplied") alpha.
    // Convert to associated unless we were requested not to do so.
    //
    // Composite layer (subimage 0) is mixed with background, which
    // affects the alpha (aka white borders if background not removed).
    //
    // Composite:
    // m_keep_unassociated_alpha true: remove background and convert to unassociated
    // m_keep_unassociated_alpha false: remove background only
    //
    // Other Layers:
    // m_keep_unassociated_alpha true: do nothing
    // m_keep_unassociated_alpha false: convert to associated
    //
    //
    if (spec.alpha_channel != -1) {
        if (subimage == 0) {
            if (m_keep_unassociated_alpha) {
                background_to_unassalpha(spec.width, data, spec.nchannels,
                                         spec.alpha_channel, spec.format);
            } else {
                background_to_assocalpha(spec.width, data, spec.nchannels,
                                         spec.alpha_channel, spec.format);
            }
        } else {
            if (m_keep_unassociated_alpha) {
                // do nothing - leave as it is
            } else {
                unassalpha_to_assocalpha(spec.width, data, spec.nchannels,
                                         spec.alpha_channel, spec.format);
            }
        }
    }

    return true;
#undef DEB
}



void
PSDInput::init()
{
    m_filename.clear();
    m_subimage       = -1;
    m_subimage_count = 0;
    m_specs.clear();
    m_WantRaw = false;
    m_layers.clear();
    m_image_data.channel_info.clear();
    m_image_data.transparency = false;
    m_channels.clear();
    m_alpha_names.clear();
    m_transparency_index      = -1;
    m_keep_unassociated_alpha = false;
    m_background_color[0]     = 1.0;
    m_background_color[1]     = 1.0;
    m_background_color[2]     = 1.0;
    m_background_color[3]     = 1.0;
    m_thumbnail.clear();
    ioproxy_clear();
}



bool
PSDInput::load_header()
{
    if (!read_header() || !validate_header())
        return false;

    return true;
}



bool
PSDInput::read_header()
{
    return ioread(m_header.signature, 4)
           && read_bige<uint16_t>(m_header.version) && ioseek(6, SEEK_CUR)
           && read_bige<uint16_t>(m_header.channel_count)
           && read_bige<uint32_t>(m_header.height)
           && read_bige<uint32_t>(m_header.width)
           && read_bige<uint16_t>(m_header.depth)
           && read_bige<uint16_t>(m_header.color_mode);
}



bool
PSDInput::validate_signature(const char signature[4])
{
    return std::memcmp(signature, "8BPS", 4) == 0;
}



bool
PSDInput::validate_header()
{
    if (!validate_signature(m_header.signature)) {
        errorfmt("[Header] invalid signature");
        return false;
    }
    if (m_header.version != 1 && m_header.version != 2) {
        errorfmt("[Header] invalid version");
        return false;
    }
    if (m_header.channel_count < 1 || m_header.channel_count > 56) {
        errorfmt("[Header] invalid channel count");
        return false;
    }
    switch (m_header.version) {
    case 1:
        // PSD
        // width/height range: [1,30000]
        if (m_header.height < 1 || m_header.height > 30000) {
            errorfmt("[Header] invalid image height");
            return false;
        }
        if (m_header.width < 1 || m_header.width > 30000) {
            errorfmt("[Header] invalid image width");
            return false;
        }
        break;
    case 2:
        // PSB (Large Document Format)
        // width/height range: [1,300000]
        if (m_header.height < 1 || m_header.height > 300000) {
            errorfmt("[Header] invalid image height {}", m_header.height);
            return false;
        }
        if (m_header.width < 1 || m_header.width > 300000) {
            errorfmt("[Header] invalid image width {}", m_header.width);
            return false;
        }
        break;
    }
    // Valid depths are 1,8,16,32
    if (m_header.depth != 1 && m_header.depth != 8 && m_header.depth != 16
        && m_header.depth != 32) {
        errorfmt("[Header] invalid depth {}", m_header.depth);
        return false;
    }
    if (m_WantRaw)
        return true;

    //There are other (undocumented) color modes not listed here
    switch (m_header.color_mode) {
    case ColorMode_Bitmap:
    case ColorMode_Indexed:
    case ColorMode_RGB:
    case ColorMode_Grayscale:
    case ColorMode_CMYK:
    case ColorMode_Multichannel: break;
    case ColorMode_Duotone:
    case ColorMode_Lab:
        errorfmt("[Header] unsupported color mode {:d}", m_header.color_mode);
        return false;
    default:
        errorfmt("[Header] unrecognized color mode {:d}", m_header.color_mode);
        return false;
    }
    return true;
}



bool
PSDInput::load_color_data()
{
    if (!read_bige<uint32_t>(m_color_data.length))
        return false;

    if (!validate_color_data())
        return false;

    if (m_color_data.length) {
        m_color_data.data.reset(new uint8_t[m_color_data.length]);
        return ioread(m_color_data.data.get(), m_color_data.length);
    }
    return true;
}



bool
PSDInput::validate_color_data()
{
    if (m_header.color_mode == ColorMode_Duotone && m_color_data.length == 0) {
        errorfmt(
            "[Color Mode Data] color mode data should be present for duotone image");
        return false;
    }
    if (m_header.color_mode == ColorMode_Indexed
        && m_color_data.length != 768) {
        errorfmt(
            "[Color Mode Data] length should be 768 for indexed color mode");
        return false;
    }
    return true;
}



bool
PSDInput::load_resources()
{
    uint32_t length;
    if (!read_bige<uint32_t>(length))
        return false;

    ImageResourceBlock block;
    ImageResourceMap resources;
    int64_t begin = iotell();
    int64_t end   = begin + int64_t(length);
    while (iotell() < end) {
        if (!read_resource(block) || !validate_resource(block))
            return false;

        resources.insert(std::make_pair(block.id, block));
    }

    if (!handle_resources(resources))
        return false;

    return ioseek(end);
}



bool
PSDInput::read_resource(ImageResourceBlock& block)
{
    bool ok = ioread(block.signature, 4) && read_bige<uint16_t>(block.id)
              && read_pascal_string(block.name, 2)
              && read_bige<uint32_t>(block.length);
    // Save the file position of the image resource data
    block.pos = iotell();
    // Skip the image resource data
    ok &= ioseek(block.length, SEEK_CUR);
    // Image resource blocks are supposed to be padded to an even size.
    // I'm not sure if the padding is included in the length field
    if (block.length % 2 != 0)
        ok &= ioseek(1, SEEK_CUR);
    return ok;
}



bool
PSDInput::validate_resource(ImageResourceBlock& block)
{
    if (std::memcmp(block.signature, "8BIM", 4) != 0) {
        errorfmt("[Image Resource] invalid signature");
        return false;
    }
    return true;
}



bool
PSDInput::handle_resources(ImageResourceMap& resources)
{
    // Loop through each of our resource loaders
    const ImageResourceMap::const_iterator end(resources.end());
    for (const ResourceLoader& loader : resource_loaders) {
        ImageResourceMap::const_iterator it(resources.find(loader.resource_id));
        // If a resource with that ID exists in the file, call the loader
        if (it != end) {
            if (!ioseek(it->second.pos)
                || !loader.load(this, it->second.length))
                return false;
        }
    }
    return true;
}



bool
PSDInput::load_resource_1005(uint32_t /*length*/)
{
    ResolutionInfo resinfo;
    // Fixed 16.16
    bool ok = read_bige<uint32_t>(resinfo.hRes);
    resinfo.hRes /= 65536.0f;
    ok &= read_bige<int16_t>(resinfo.hResUnit);
    ok &= read_bige<int16_t>(resinfo.widthUnit);
    // Fixed 16.16
    ok &= read_bige<uint32_t>(resinfo.vRes);
    resinfo.vRes /= 65536.0f;
    ok &= read_bige<int16_t>(resinfo.vResUnit);
    ok &= read_bige<int16_t>(resinfo.heightUnit);
    if (!ok)
        return false;

    // Make sure the same unit is used both horizontally and vertically
    // FIXME(dewyatt): I don't know for sure that the unit can differ. However,
    // if it can, perhaps we should be using ResolutionUnitH/ResolutionUnitV or
    // something similar.
    if (resinfo.hResUnit != resinfo.vResUnit) {
        errorfmt(
            "[Image Resource] [ResolutionInfo] Resolutions must have the same unit");
        return false;
    }
    // Make sure the unit is supported
    // Note: This relies on the above check that the units are the same.
    if (resinfo.hResUnit != ResolutionInfo::PixelsPerInch
        && resinfo.hResUnit != ResolutionInfo::PixelsPerCentimeter) {
        errorfmt(
            "[Image Resource] [ResolutionInfo] Unrecognized resolution unit");
        return false;
    }
    common_attribute("XResolution", resinfo.hRes);
    common_attribute("YResolution", resinfo.vRes);
    switch (resinfo.hResUnit) {
    case ResolutionInfo::PixelsPerInch:
        common_attribute("ResolutionUnit", "in");
        break;
    case ResolutionInfo::PixelsPerCentimeter:
        common_attribute("ResolutionUnit", "cm");
        break;
    };
    return true;
}



bool
PSDInput::load_resource_1006(uint32_t length)
{
    int32_t bytes_remaining = length;
    std::string name;
    while (bytes_remaining >= 2) {
        bytes_remaining -= read_pascal_string(name, 1);
        m_alpha_names.push_back(name);
    }
    return true;
}



bool
PSDInput::load_resource_1010(uint32_t /*length*/)
{
    int8_t color_id = 0;
    int32_t color   = 0;

    bool ok = read_bige<int8_t>(color_id) && read_bige<int32_t>(color);

    m_background_color[0] = convert_type<uint8_t, float>(color & 0xFF);
    m_background_color[1] = convert_type<uint8_t, float>((color >> 8) & 0xFF);
    m_background_color[2] = convert_type<uint8_t, float>((color >> 16) & 0xFF);
    m_background_color[3] = convert_type<uint8_t, float>((color >> 24) & 0xFF);

    return ok;
}



bool
PSDInput::load_resource_1033(uint32_t length)
{
    return load_resource_thumbnail(length, true);
}



bool
PSDInput::load_resource_1036(uint32_t length)
{
    return load_resource_thumbnail(length, false);
}



bool
PSDInput::load_resource_1039(uint32_t length)
{
    std::unique_ptr<uint8_t[]> icc_buf(new uint8_t[length]);
    if (!ioread(icc_buf.get(), length))
        return false;

    TypeDesc type(TypeDesc::UINT8, length);
    common_attribute("ICCProfile", type, icc_buf.get());
    std::string errormsg;
    bool ok = decode_icc_profile(cspan<uint8_t>(icc_buf.get(), length),
                                 m_common_attribs, errormsg)
              && decode_icc_profile(cspan<uint8_t>(icc_buf.get(), length),
                                    m_composite_attribs, errormsg);
    if (!ok && OIIO::get_int_attribute("imageinput:strict")) {
        errorfmt("Possible corrupt file, could not decode ICC profile: {}\n",
                 errormsg);
        return false;
    }
    return true;
}



bool
PSDInput::load_resource_1047(uint32_t /*length*/)
{
    if (!read_bige<int16_t>(m_transparency_index))
        return false;
    if (m_transparency_index < 0 || m_transparency_index >= 768) {
        errorfmt("[Image Resource] Transparency index {} is out of range",
                 m_transparency_index);
        return false;
    }
    return true;
}



bool
PSDInput::load_resource_1058(uint32_t length)
{
    std::string data(length, 0);
    if (!ioread(&data[0], length))
        return false;

    if (!decode_exif(data, m_composite_attribs)
        || !decode_exif(data, m_common_attribs)) {
        errorfmt("Failed to decode Exif data");
        return false;
    }
    return true;
}



bool
PSDInput::load_resource_1059(uint32_t length)
{
    //FIXME(dewyatt): untested, I don't have any images with this resource
    return load_resource_1058(length);
}



bool
PSDInput::load_resource_1060(uint32_t length)
{
    std::string data(length, 0);
    if (!ioread(&data[0], length))
        return false;

    // Store the XMP data for the composite and all other subimages
    if (!decode_xmp(data, m_composite_attribs)
        || !decode_xmp(data, m_common_attribs)) {
        errorfmt("Failed to decode XMP data");
        return false;
    }
    return true;
}



bool
PSDInput::load_resource_1064(uint32_t /*length*/)
{
    uint32_t version;
    if (!read_bige<uint32_t>(version))
        return false;

    if (version != 1 && version != 2) {
        errorfmt("[Image Resource] [Pixel Aspect Ratio] Unrecognized version");
        return false;
    }
    double aspect_ratio;
    if (!read_bige<double>(aspect_ratio))
        return false;

    // FIXME(dewyatt): loss of precision?
    common_attribute("PixelAspectRatio", (float)aspect_ratio);
    return true;
}



bool
PSDInput::load_resource_thumbnail(uint32_t length, bool isBGR)
{
    enum ThumbnailFormat { kJpegRGB = 1, kRawRGB = 0 };

    uint32_t format;
    uint32_t width, height;
    uint32_t widthbytes;
    uint32_t total_size;
    uint32_t compressed_size;
    uint16_t bpp;
    uint16_t planes;
    uint32_t jpeg_length = length - 28;

    bool ok = read_bige<uint32_t>(format) && read_bige<uint32_t>(width)
              && read_bige<uint32_t>(height) && read_bige<uint32_t>(widthbytes)
              && read_bige<uint32_t>(total_size)
              && read_bige<uint32_t>(compressed_size)
              && read_bige<uint16_t>(bpp) && read_bige<uint16_t>(planes);
    if (!ok)
        return false;

    // Sanity checks
    // Strutil::print("thumb h {} w {} bpp {} planes {} format {} widthbytes {} total_size {}\n",
    //                height, width, bpp, planes, format, widthbytes, total_size);
    if (bpp != 8 && bpp != 24) {
        errorfmt(
            "Thumbnail JPEG is {} bpp, not supported or possibly corrupt file",
            bpp);
        return false;
    }
    if ((bpp / 8) * width > widthbytes || (bpp / 8) * width + 3 < widthbytes) {
        errorfmt("Corrupt thumbnail: {}w * {}bpp does not match {} width bytes",
                 width, bpp, widthbytes);
        return false;
    }
    if (widthbytes * height * planes != total_size) {
        errorfmt(
            "Corrupt thumbnail: {}w * {}h * {}bpp does not match {} total_size",
            width, height, bpp, total_size);
        return false;
    }

    // We only support kJpegRGB since I don't have any test images with
    // kRawRGB
    if (format != kJpegRGB || bpp != 24 || planes != 1) {
        errorfmt(
            "[Image Resource] [JPEG Thumbnail] invalid or unsupported format");
        return false;
    }

    std::string jpeg_data(jpeg_length, '\0');
    if (!ioread(&jpeg_data[0], jpeg_length))
        return false;

    // Create an IOMemReader that references the thumbnail JPEG blob and read
    // it with an ImageInput, into the memory owned by an ImageBuf.
    Filesystem::IOMemReader thumbblob(jpeg_data.data(), jpeg_length);
    m_thumbnail.clear();
    auto imgin = ImageInput::open("thumbnail.jpg", nullptr, &thumbblob);
    if (imgin) {
        ImageSpec spec = imgin->spec(0);
        m_thumbnail.reset(spec, InitializePixels::No);
        ok = imgin->read_image(0, 0, 0, m_thumbnail.spec().nchannels,
                               m_thumbnail.spec().format,
                               m_thumbnail.localpixels());
        imgin.reset();
    } else {
        errorfmt("Failed to open thumbnail");
        return false;
    }
    if (!ok) {
        errorfmt("Failed to read thumbnail: {}", m_thumbnail.geterror());
        m_thumbnail.clear();
        return false;
    }

    // Set these attributes for the merged composite only (subimage 0)
    composite_attribute("thumbnail_width", (int)m_thumbnail.spec().width);
    composite_attribute("thumbnail_height", (int)m_thumbnail.spec().height);
    composite_attribute("thumbnail_nchannels",
                        (int)m_thumbnail.spec().nchannels);
    if (isBGR)
        m_thumbnail = ImageBufAlgo::channels(m_thumbnail, 3, { 2, 1, 0 });
    return true;
}



bool
PSDInput::load_layers()
{
    bool ok = true;
    if (m_header.version == 1)
        ok &= read_bige<uint32_t>(m_layer_mask_info.length);
    else
        ok &= read_bige<uint64_t>(m_layer_mask_info.length);

    m_layer_mask_info.begin = iotell();
    m_layer_mask_info.end = m_layer_mask_info.begin + m_layer_mask_info.length;
    if (!ok)
        return false;

    if (!m_layer_mask_info.length)
        return true;

    LayerMaskInfo::LayerInfo& layer_info = m_layer_mask_info.layer_info;
    if (m_header.version == 1)
        ok &= read_bige<uint32_t>(layer_info.length);
    else
        ok &= read_bige<uint64_t>(layer_info.length);

    layer_info.begin = iotell();
    layer_info.end   = layer_info.begin + layer_info.length;
    if (!ok)
        return false;

    // There is 3 cases where this could be empty:
    // - 16-bit files store this data in the Lr16 tagged block
    // - 32-bit files store this data in the Lr32 tagged block
    // - Single layer files may optimize this section away and instead
    //   store the data on the merged image data section
    if (!layer_info.length)
        return true;

    ok &= read_bige<int16_t>(layer_info.layer_count);
    if (layer_info.layer_count < 0) {
        m_image_data.transparency = true;
        layer_info.layer_count    = -layer_info.layer_count;
    }
    m_layers.resize(layer_info.layer_count);
    for (int16_t layer_nbr = 0; layer_nbr < layer_info.layer_count;
         ++layer_nbr) {
        Layer& layer = m_layers[layer_nbr];
        if (!load_layer(layer))
            return false;
    }
    for (int16_t layer_nbr = 0; layer_nbr < layer_info.layer_count;
         ++layer_nbr) {
        Layer& layer = m_layers[layer_nbr];
        if (!load_layer_channels(layer))
            return false;
    }
    return ok;
}



bool
PSDInput::load_layer(Layer& layer)
{
    bool ok = true;
    ok &= read_bige<uint32_t>(layer.top);
    ok &= read_bige<uint32_t>(layer.left);
    ok &= read_bige<uint32_t>(layer.bottom);
    ok &= read_bige<uint32_t>(layer.right);
    ok &= read_bige<uint16_t>(layer.channel_count);
    if (!ok)
        return false;

    layer.width  = std::abs((int)layer.right - (int)layer.left);
    layer.height = std::abs((int)layer.bottom - (int)layer.top);
    layer.channel_info.resize(layer.channel_count);
    for (uint16_t channel = 0; channel < layer.channel_count; channel++) {
        ChannelInfo& channel_info = layer.channel_info[channel];
        ok &= read_bige<int16_t>(channel_info.channel_id);
        if (m_header.version == 1)
            ok &= read_bige<uint32_t>(channel_info.data_length);
        else
            ok &= read_bige<uint64_t>(channel_info.data_length);

        layer.channel_id_map[channel_info.channel_id] = &channel_info;
    }
    char bm_signature[4];
    ok &= ioread(bm_signature, 4);
    if (!ok)
        return false;

    if (std::memcmp(bm_signature, "8BIM", 4) != 0) {
        errorfmt("[Layer Record] Invalid blend mode signature");
        return false;
    }
    ok &= ioread(layer.bm_key, 4);
    ok &= read_bige<uint8_t>(layer.opacity);
    ok &= read_bige<uint8_t>(layer.clipping);
    ok &= read_bige<uint8_t>(layer.flags);
    // skip filler
    ok &= ioseek(1, SEEK_CUR);
    ok &= read_bige<uint32_t>(layer.extra_length);
    uint32_t extra_remaining = layer.extra_length;
    // layer mask data length
    uint32_t lmd_length;
    ok &= read_bige<uint32_t>(lmd_length);
    if (!ok)
        return false;

    if (lmd_length > 0) {
        auto lmd_start = iotell();
        auto lmd_end   = lmd_start + lmd_length;

        if (lmd_length >= 4 * 4 + 1 * 2) {
            ok &= read_bige<uint32_t>(layer.mask_data.top);
            ok &= read_bige<uint32_t>(layer.mask_data.left);
            ok &= read_bige<uint32_t>(layer.mask_data.bottom);
            ok &= read_bige<uint32_t>(layer.mask_data.right);
            ok &= read_bige<uint8_t>(layer.mask_data.default_color);
            ok &= read_bige<uint8_t>(layer.mask_data.flags);
        }

        // skip mask parameters
        // skip "real" fields

        ok &= ioseek(lmd_end);
        if (!ok)
            return false;
    }
    extra_remaining -= (lmd_length + 4);

    // layer blending ranges length
    uint32_t lbr_length = 0;
    ok &= read_bige<uint32_t>(lbr_length);
    // skip block
    ok &= ioseek(lbr_length, SEEK_CUR);
    extra_remaining -= (lbr_length + 4);
    if (!ok)
        return false;

    extra_remaining -= read_pascal_string(layer.name, 4);
    while (ok && extra_remaining >= 12) {
        layer.additional_info.emplace_back();
        Layer::AdditionalInfo& info = layer.additional_info.back();

        char signature[4];
        ok &= ioread(signature, 4);
        ok &= ioread(info.key, 4);
        if (std::memcmp(signature, "8BIM", 4) != 0
            && std::memcmp(signature, "8B64", 4) != 0) {
            errorfmt("[Additional Layer Info] invalid signature");
            return false;
        }
        extra_remaining -= 8;
        if (m_header.version == 2 && is_additional_info_psb(info.key)) {
            ok &= read_bige<uint64_t>(info.length);
            extra_remaining -= 8;
        } else {
            ok &= read_bige<uint32_t>(info.length);
            extra_remaining -= 4;
        }
        ok &= ioseek(info.length, SEEK_CUR);
        extra_remaining -= info.length;
    }
    return ok;
}



bool
PSDInput::load_layer_channels(Layer& layer)
{
    for (uint16_t channel = 0; channel < layer.channel_count; ++channel) {
        ChannelInfo& channel_info = layer.channel_info[channel];
        if (!load_layer_channel(layer, channel_info))
            return false;
    }
    return true;
}



bool
PSDInput::load_layer_channel(Layer& layer, ChannelInfo& channel_info)
{
    int64_t start_pos = iotell();
    if (channel_info.data_length >= 2) {
        if (!read_bige<uint16_t>(channel_info.compression))
            return false;
    }
    // No data at all or just compression
    if (channel_info.data_length <= 2)
        return true;

    // Use mask_data size when channel_id is -2
    uint32_t width, height;
    if (channel_info.channel_id == ChannelID_LayerMask) {
        width  = (uint32_t)std::abs((int)layer.mask_data.right
                                    - (int)layer.mask_data.left);
        height = (uint32_t)std::abs((int)layer.mask_data.bottom
                                    - (int)layer.mask_data.top);
    } else {
        width  = layer.width;
        height = layer.height;
    }
    channel_info.width  = width;
    channel_info.height = height;

    channel_info.data_pos = iotell();
    channel_info.row_pos.resize(height);
    channel_info.row_length = (width * m_header.depth + 7) / 8;

    switch (channel_info.compression) {
    case Compression_Raw:
        if (height) {
            channel_info.row_pos[0] = channel_info.data_pos;
            for (uint32_t i = 1; i < height; ++i)
                channel_info.row_pos[i] = channel_info.row_pos[i - 1]
                                          + channel_info.row_length;
        }
        channel_info.data_length = channel_info.row_length * height;

        if (!ioseek(channel_info.data_length, SEEK_CUR))
            return false;
        break;
    case Compression_RLE:
        // RLE lengths are stored before the channel data
        if (!read_rle_lengths(height, channel_info.rle_lengths))
            return false;

        // channel data is located after the RLE lengths
        channel_info.data_pos = iotell();
        // subtract the RLE lengths read above
        channel_info.data_length = channel_info.data_length
                                   - (channel_info.data_pos - start_pos);
        if (height) {
            channel_info.row_pos[0] = channel_info.data_pos;
            for (uint32_t i = 1; i < height; ++i)
                channel_info.row_pos[i] = channel_info.row_pos[i - 1]
                                          + channel_info.rle_lengths[i - 1];
        }

        if (!ioseek(channel_info.data_length, SEEK_CUR))
            return false;
        break;
    case Compression_ZIP: {
        // We subtract the compression marker from the data length
        channel_info.data_length -= 2;

        // Unlike with raw and rle compression we cannot access each scanline
        // randomly so we parse the data up-front and store it
        std::vector<char> compressed_data(channel_info.data_length);
        channel_info.decompressed_data = std::vector<char>(
            width * height * (m_header.depth / 8));

        if (!ioseek(channel_info.data_pos))
            return false;
        if (!ioread(compressed_data.data(), channel_info.data_length))
            return false;

        decompress_zip(compressed_data, channel_info.decompressed_data);
    } break;
    case Compression_ZIP_Predict: {
        // We subtract the compression marker from the data length
        channel_info.data_length -= 2;

        // Unlike with raw and rle compression we cannot access each scanline
        // randomly so we parse the data up-front and store it
        std::vector<char> compressed_data(channel_info.data_length);
        channel_info.decompressed_data = std::vector<char>(
            width * height * (m_header.depth / 8));

        if (!ioseek(channel_info.data_pos))
            return false;
        if (!ioread(compressed_data.data(), channel_info.data_length))
            return false;

        decompress_zip_prediction(compressed_data,
                                  channel_info.decompressed_data, width,
                                  height);
    } break;
    default:
        errorfmt("[Layer Channel] unsupported compression {}",
                 channel_info.compression);
        return false;
    }
    return true;
}



bool
PSDInput::read_rle_lengths(uint32_t height, std::vector<uint32_t>& rle_lengths)
{
    rle_lengths.resize(height);
    bool ok = true;
    for (uint32_t row = 0; row < height && ok; ++row) {
        if (m_header.version == 1)
            ok &= read_bige<uint16_t>(rle_lengths[row]);
        else
            ok &= read_bige<uint32_t>(rle_lengths[row]);
    }
    return ok;
}



bool
PSDInput::load_global_mask_info()
{
    if (!m_layer_mask_info.length)
        return true;

    bool ok            = ioseek(m_layer_mask_info.layer_info.end);
    uint64_t remaining = m_layer_mask_info.end - iotell();
    uint32_t length;

    // This section should be at least 17 bytes, but some files lack
    // global mask info and additional layer info, not covered in the spec.
    // More modern photoshop files appear to omit this section entirely.
    // We leave this code here though in case we want to deal with older photoshop files
    // although it is not currently used anywhere else
    if (remaining < 17) {
        return ioseek(m_layer_mask_info.end);
    }

    ok &= read_bige<uint32_t>(length);
    int64_t start = iotell();
    int64_t end   = start + length;  // NOSONAR
    if (!ok)
        return false;

    // this can be empty
    if (!length)
        return true;

    ok &= read_bige<uint16_t>(m_global_mask_info.overlay_color_space);
    for (int i = 0; i < 4; ++i)
        ok &= read_bige<uint16_t>(m_global_mask_info.color_components[i]);

    ok &= read_bige<uint16_t>(m_global_mask_info.opacity);
    ok &= read_bige<int16_t>(m_global_mask_info.kind);
    ok &= ioseek(end);
    return ok;
}



bool
PSDInput::load_global_additional()
{
    if (!m_layer_mask_info.length)
        return true;

    char signature[4];
    char key[4];
    uint64_t length    = 0;
    uint64_t remaining = m_layer_mask_info.length
                         - (iotell() - m_layer_mask_info.begin);
    bool ok = true;
    while (ok && remaining >= 12) {
        if (!ioread(signature, 4))
            return false;

        // the spec supports 8BIM, and 8B64 (presumably for psb support)
        if (std::memcmp(signature, "8BIM", 4) != 0
            && std::memcmp(signature, "8B64", 4) != 0) {
            errorfmt("[Global Additional Layer Info] invalid signature");
            return false;
        }
        if (!ioread(key, 4))
            return false;

        remaining -= 8;
        if (m_header.version == 2 && is_additional_info_psb(key)) {
            ok &= read_bige<uint64_t>(length);
            remaining -= 8;
        } else {
            ok &= read_bige<uint32_t>(length);
            remaining -= 4;
        }
        // Long story short these are aligned to 4 bytes but that is not
        // included in the stored length and the specs do not mention it.

        // Load 16 and 32-bit layer data
        if (std::memcmp(key, "Lr16", 4) == 0
            || std::memcmp(key, "Lr32", 4) == 0) {
            uint64_t begin_offset = iotell();
            ok &= load_layers_16_32(length);
            uint64_t size = iotell() - begin_offset;
            remaining -= size;
        } else {
            // round up to multiple of 4
            length = (length + 3) & ~3;
            remaining -= length;
            // skip it for now
            ok &= ioseek(length, SEEK_CUR);
        }
    }
    // finished with the layer and mask information section, seek to the end
    ok &= ioseek(m_layer_mask_info.end);
    return ok;
}


bool
PSDInput::load_layers_16_32(uint64_t length)
{
    // Notice that, bar the reading of the length marker, reading this section is identical to
    // the normal layer info section
    bool ok = true;

    if (length == 0)
        return false;

    LayerMaskInfo::LayerInfo& layer_info = m_layer_mask_info.layer_info;
    // The layer info length must have been 0 in the actual layer info section
    OIIO_ASSERT(layer_info.length == 0);
    layer_info.length = length;

    uint64_t begin = iotell();

    // We read the layer info as we would usually since the section is exactly the same
    ok &= read_bige<int16_t>(layer_info.layer_count);
    if (layer_info.layer_count < 0) {
        m_image_data.transparency = true;
        layer_info.layer_count    = -layer_info.layer_count;
    }
    m_layers.resize(layer_info.layer_count);
    for (int16_t layer_nbr = 0; layer_nbr < layer_info.layer_count;
         ++layer_nbr) {
        Layer& layer = m_layers[layer_nbr];
        if (!load_layer(layer))
            return false;
    }
    for (int16_t layer_nbr = 0; layer_nbr < layer_info.layer_count;
         ++layer_nbr) {
        Layer& layer = m_layers[layer_nbr];
        if (!load_layer_channels(layer))
            return false;
    }

    // This section, like the other tagged blocks are padded to 4 bytes
    uint64_t length_read = iotell() - begin;
    int64_t remaining    = (((length_read + 3) / 4) * 4) - length_read;
    OIIO_ASSERT(remaining >= 0);
    OIIO_ASSERT(remaining < 4);
    ioseek(remaining, SEEK_CUR);

    return ok;
}


bool
PSDInput::load_image_data()
{
    uint16_t compression;
    uint32_t row_length = (m_header.width * m_header.depth + 7) / 8;
    int16_t id          = 0;
    if (!read_bige<uint16_t>(compression))
        return false;

    if (compression != Compression_Raw && compression != Compression_RLE) {
        errorfmt("[Image Data Section] unsupported compression {:d}",
                 compression);
        return false;
    }
    m_image_data.channel_info.resize(m_header.channel_count);
    // setup some generic properties and read any RLE lengths
    // Image Data Section has RLE lengths for all channels stored first
    for (ChannelInfo& channel_info : m_image_data.channel_info) {
        channel_info.width       = m_header.width;
        channel_info.height      = m_header.height;
        channel_info.compression = compression;
        channel_info.channel_id  = id++;
        channel_info.data_length = row_length * m_header.height;
        if (compression == Compression_RLE) {
            if (!read_rle_lengths(m_header.height, channel_info.rle_lengths))
                return false;
        }
    }
    bool ok = true;
    for (ChannelInfo& channel_info : m_image_data.channel_info) {
        channel_info.row_pos.resize(m_header.height);
        channel_info.data_pos   = iotell();
        channel_info.row_length = (m_header.width * m_header.depth + 7) / 8;
        switch (compression) {
        case Compression_Raw:
            channel_info.row_pos[0] = channel_info.data_pos;
            for (uint32_t i = 1; i < m_header.height; ++i)
                channel_info.row_pos[i] = channel_info.row_pos[i - 1]
                                          + row_length;

            ok &= ioseek(channel_info.row_pos.back() + row_length);
            break;
        case Compression_RLE:
            channel_info.row_pos[0] = channel_info.data_pos;
            for (uint32_t i = 1; i < m_header.height; ++i)
                channel_info.row_pos[i] = channel_info.row_pos[i - 1]
                                          + channel_info.rle_lengths[i - 1];

            ok &= ioseek(channel_info.row_pos.back()
                         + channel_info.rle_lengths.back());
            break;
        }
    }
    return ok;
}



void
PSDInput::setup()
{
    // raw_channel_count is the number of channels in the file
    // spec_channel_count is what we will report to OIIO client
    int raw_channel_count, spec_channel_count;
    if (m_header.color_mode == ColorMode_Multichannel) {
        spec_channel_count = raw_channel_count = m_header.channel_count;
    } else {
        raw_channel_count = mode_channel_count[m_header.color_mode];
        spec_channel_count
            = m_WantRaw ? raw_channel_count
                        : (m_header.color_mode == ColorMode_Grayscale ? 1 : 3);
        if (m_image_data.transparency) {
            spec_channel_count++;
            raw_channel_count++;
        } else if (m_header.color_mode == ColorMode_Indexed
                   && m_transparency_index) {
            spec_channel_count++;
        }
    }

    // Composite spec
    m_specs.emplace_back(m_header.width, m_header.height, spec_channel_count,
                         m_type_desc);
    ImageSpec& spec    = m_specs.back();
    spec.extra_attribs = m_composite_attribs.extra_attribs;
    if (m_WantRaw)
        fill_channel_names(spec, m_image_data.transparency);
    if (spec.alpha_channel != -1)
        if (m_keep_unassociated_alpha)
            spec.attribute("oiio:UnassociatedAlpha", 1);

    // Composite channels
    m_channels.reserve(m_subimage_count);
    m_channels.resize(1);
    m_channels[0].reserve(raw_channel_count);
    for (int i = 0; i < raw_channel_count; ++i)
        m_channels[0].push_back(&m_image_data.channel_info[i]);

    for (Layer& layer : m_layers) {
        spec_channel_count = m_WantRaw ? mode_channel_count[m_header.color_mode]
                                       : 3;
        raw_channel_count  = mode_channel_count[m_header.color_mode];
        bool transparency  = (bool)layer.channel_id_map.count(
            ChannelID_Transparency);
        if (transparency) {
            spec_channel_count++;
            raw_channel_count++;
        }
        m_specs.emplace_back(layer.width, layer.height, spec_channel_count,
                             m_type_desc);
        ImageSpec& spec    = m_specs.back();
        spec.x             = layer.left;
        spec.y             = layer.top;
        spec.extra_attribs = m_common_attribs.extra_attribs;
        if (m_WantRaw)
            fill_channel_names(spec, transparency);
        if (spec.alpha_channel != -1)
            if (m_keep_unassociated_alpha)
                spec.attribute("oiio:UnassociatedAlpha", 1);

        m_channels.resize(m_channels.size() + 1);
        std::vector<ChannelInfo*>& channels = m_channels.back();
        channels.reserve(raw_channel_count);
        for (unsigned int i = 0; i < mode_channel_count[m_header.color_mode];
             ++i)
            channels.push_back(layer.channel_id_map[i]);

        if (transparency)
            channels.push_back(layer.channel_id_map[ChannelID_Transparency]);
        if (layer.name.size())
            spec.attribute("oiio:subimagename", layer.name);
    }
}



void
PSDInput::fill_channel_names(ImageSpec& spec, bool transparency)
{
    spec.channelnames.clear();
    if (m_header.color_mode == ColorMode_Multichannel) {
        spec.default_channel_names();
    } else {
        for (unsigned int i = 0; i < mode_channel_count[m_header.color_mode];
             ++i)
            spec.channelnames.emplace_back(
                mode_channel_names[m_header.color_mode][i]);
        if (transparency)
            spec.channelnames.emplace_back("A");
    }
}



bool
PSDInput::read_channel_row(ChannelInfo& channel_info, uint32_t row, char* data)
{
    if (row >= channel_info.row_pos.size()) {
        errorfmt("Reading channel row out of range ({}, should be < {})", row,
                 channel_info.row_pos.size());
        return false;
    }

    switch (channel_info.compression) {
    case Compression_Raw:
        if (!ioseek(channel_info.row_pos[row]))
            return false;
        if (!ioread(data, channel_info.row_length))
            return false;

        if (!bigendian()) {
            switch (m_header.depth) {
            case 16: swap_endian((uint16_t*)data, channel_info.width); break;
            case 32: swap_endian((uint32_t*)data, channel_info.width); break;
            }
        }
        break;
    case Compression_RLE: {
        if (!ioseek(channel_info.row_pos[row]))
            return false;
        uint32_t rle_length = channel_info.rle_lengths[row];
        char* rle_buffer;
        OIIO_ALLOCATE_STACK_OR_HEAP(rle_buffer, char, rle_length);
        if (!ioread(rle_buffer, rle_length)
            || !decompress_packbits(rle_buffer, data, rle_length,
                                    channel_info.row_length))
            return false;
    } break;
    case Compression_ZIP: {
        OIIO_ASSERT(channel_info.decompressed_data.size()
                    == static_cast<uint64_t>(channel_info.width)
                           * channel_info.height * (m_header.depth / 8));
        // We simply copy over the row into destination
        uint64_t row_index = static_cast<uint64_t>(row) * channel_info.width
                             * (m_header.depth / 8);
        std::memcpy(data, channel_info.decompressed_data.data() + row_index,
                    channel_info.row_length);
    } break;
    case Compression_ZIP_Predict: {
        OIIO_ASSERT(channel_info.decompressed_data.size()
                    == static_cast<uint64_t>(channel_info.width)
                           * channel_info.height * (m_header.depth / 8));
        // We simply copy over the row into destination
        uint64_t row_index = static_cast<uint64_t>(row) * channel_info.width
                             * (m_header.depth / 8);
        std::memcpy(data, channel_info.decompressed_data.data() + row_index,
                    channel_info.row_length);
    } break;
    }

    return true;
}



template<typename T>
void
PSDInput::interleave_row(T* dst,
                         cspan<std::vector<unsigned char>> channel_buffers,
                         int width, int nchans)
{
    for (int c = 0; c < nchans; ++c) {
        const T* cbuf = reinterpret_cast<const T*>(channel_buffers[c].data());
        for (int x = 0; x < width; ++x)
            dst[nchans * x + c] = cbuf[x];
    }
}



bool
PSDInput::indexed_to_rgb(span<unsigned char> dst, cspan<unsigned char> src,
                         int width) const
{
    OIIO_ASSERT(src.size() && dst.size());
    // The color table is 768 bytes which is 256 * 3 channels (always RGB)
    const auto& table(m_color_data.data);
    if (m_transparency_index >= 0) {
        for (int i = 0; i < width; ++i) {
            int index = src[i];
            if (index == m_transparency_index) {
                dst[4 * i + 0] = 0;
                dst[4 * i + 1] = 0;
                dst[4 * i + 2] = 0;
                dst[4 * i + 3] = 0;
            } else {
                dst[4 * i + 0] = table[index];        // R
                dst[4 * i + 1] = table[index + 256];  // G
                dst[4 * i + 2] = table[index + 512];  // B
                dst[4 * i + 3] = 0xff;                // A
            }
        }
    } else {
        for (int i = 0; i < width; ++i) {
            int index      = src[i];
            dst[3 * i + 0] = table[index];        // R
            dst[3 * i + 1] = table[index + 256];  // G
            dst[3 * i + 2] = table[index + 512];  // B
        }
    }
    return true;
}



bool
PSDInput::bitmap_to_rgb(span<unsigned char> dst, cspan<unsigned char> src,
                        int width) const
{
    for (int i = 0; i < width; ++i) {
        int byte             = i / 8;
        int bit              = 7 - i % 8;
        unsigned char result = (src[byte] & (1 << bit)) ? 0 : 0xff;
        dst[i * 3 + 0]       = result;
        dst[i * 3 + 1]       = result;
        dst[i * 3 + 2]       = result;
    }
    return true;
}



void
PSDInput::set_type_desc()
{
    switch (m_header.depth) {
    case 1:
    case 8: m_type_desc = TypeDesc::UINT8; break;
    case 16: m_type_desc = TypeDesc::UINT16; break;
    case 32: m_type_desc = TypeDesc::FLOAT; break;
    };
}



int
PSDInput::read_pascal_string(std::string& s, uint16_t mod_padding)
{
    s.clear();
    uint8_t length;
    int bytes = 0;
    if (ioread((char*)&length, 1)) {
        bytes = 1;
        if (length == 0) {
            if (ioseek(mod_padding - 1, SEEK_CUR))
                bytes += mod_padding - 1;
        } else {
            s.resize(length);
            if (ioread(&s[0], length)) {
                bytes += length;
                if (mod_padding > 0) {
                    for (int padded_length = length + 1;
                         padded_length % mod_padding != 0; padded_length++) {
                        if (!ioseek(1, SEEK_CUR))
                            break;
                        bytes++;
                    }
                }
            }
        }
    }
    return bytes;
}



void
PSDInput::float_planar_to_interleaved(span<char> data, size_t width,
                                      size_t height)
{
    std::vector<char> buffer(data.size());

    // Shuffle from planar 1111... 2222... 3333... 4444... byte order to 1234 1234 1234 1234...
    for (uint64_t y = 0; y < height; ++y) {
        for (uint64_t x = 0; x < width; ++x) {
            uint64_t rowIndex = y * width * sizeof(float);

            buffer[rowIndex + x * sizeof(float) + 0] = data[rowIndex + x];
            buffer[rowIndex + x * sizeof(float) + 1]
                = data[rowIndex + width + x];
            buffer[rowIndex + x * sizeof(float) + 2]
                = data[rowIndex + width * 2 + x];
            buffer[rowIndex + x * sizeof(float) + 3]
                = data[rowIndex + width * 3 + x];
        }
    }
    std::memcpy(data.data(), buffer.data(), buffer.size());
}



bool
PSDInput::decompress_packbits(const char* src, char* dst,
                              uint32_t packed_length, uint32_t unpacked_length)
{
    int32_t src_remaining = packed_length;
    int32_t dst_remaining = unpacked_length;
    int16_t header;
    int length;

    char* dst_start = dst;
    while (src_remaining > 0 && dst_remaining > 0) {
        header = *reinterpret_cast<const signed char*>(src);
        src++;
        src_remaining--;

        if (header == 128)
            continue;
        else if (header >= 0) {
            // (1 + n) literal bytes
            length = 1 + header;
            src_remaining -= length;
            dst_remaining -= length;
            if (src_remaining < 0 || dst_remaining < 0) {
                errorfmt(
                    "unable to decode packbits (case 1, literal bytes: src_rem={}, dst_rem={}, len={})",
                    src_remaining, dst_remaining, length);
                return false;
            }

            std::memcpy(dst, src, length);
            src += length;
            dst += length;
        } else {
            // repeat byte (1 - n) times
            length = 1 - header;
            src_remaining--;
            dst_remaining -= length;
            if (src_remaining < 0 || dst_remaining < 0) {
                errorfmt(
                    "unable to decode packbits (case 2, repeating byte: src_rem={}, dst_rem={}, len={})",
                    src_remaining, dst_remaining, length);
                return false;
            }

            std::memset(dst, *src, length);
            src++;
            dst += length;
        }
    }

    if (!bigendian()) {
        switch (m_header.depth) {
        case 16: swap_endian((uint16_t*)dst_start, m_spec.width); break;
        case 32: swap_endian((uint32_t*)dst_start, m_spec.width); break;
        }
    }

    return true;
}



bool
PSDInput::decompress_zip(span<char> src, span<char> dest)
{
    z_stream stream {};
    stream.zfree     = Z_NULL;
    stream.opaque    = Z_NULL;
    stream.avail_in  = src.size();
    stream.next_in   = (Bytef*)src.data();
    stream.avail_out = dest.size();
    stream.next_out  = (Bytef*)dest.data();

    if (inflateInit(&stream) != Z_OK) {
        errorfmt(
            "zip compression inflate init failed with: src_size={}, dst_size={}",
            src.size(), dest.size());
        return false;
    }

    if (inflate(&stream, Z_FINISH) != Z_STREAM_END) {
        errorfmt(
            "unable to decode zip compressed data: src_size={}, dst_size={}",
            src.size(), dest.size());
        return false;
    }

    if (inflateEnd(&stream) != Z_OK) {
        errorfmt(
            "zip compression inflate cleanup failed with: src_size={}, dst_size={}",
            src.size(), dest.size());
        return false;
    }

    return true;
}



bool
PSDInput::decompress_zip_prediction(span<char> src, span<char> dest,
                                    const uint32_t width, const uint32_t height)
{
    OIIO_ASSERT(width * height * (m_header.depth / 8) == dest.size());
    bool ok = true;
    // Decompress into dest first and then apply the prediction decoding
    // on dest
    ok &= decompress_zip(src, dest);

    switch (m_header.depth) {
    case 8:
        for (uint64_t y = 0; y < height; ++y) {
            // Index x beginning at one since we look behind to calculate
            // the offset
            for (uint64_t x = 1; x < width; ++x) {
                dest[y * width + x] += dest[y * width + x - 1];
            };
        };
        break;
    case 16: {
        // 16-bit data requires endian swapping at this point already for the
        // prediction decoding to work correctly
        span<uint16_t> destView(reinterpret_cast<uint16_t*>(dest.data()),
                                dest.size() / 2);
        if (!bigendian())
            byteswap_span(destView);

        for (uint64_t y = 0; y < height; ++y) {
            // Index x beginning at one since we look behind to calculate
            // the offset
            for (uint64_t x = 1; x < width; ++x) {
                destView[y * width + x] += destView[y * width + x - 1];
            };
        };
    } break;
    case 32: {
        // 32-bit files actually have the float bytes stored in planar fashion on disk
        // which are then prediction encoded. Thus we first decode the bytes itself
        uint64_t index = 0;
        for (uint64_t y = 0; y < height; ++y) {
            ++index;
            for (uint64_t x = 1; x < (width * sizeof(float)); ++x) {
                uint8_t value = dest[index] + dest[index - 1];
                dest[index]   = value;
                ++index;
            }
        }

        // We now shuffle the byte order back into place from planar to interleaved
        float_planar_to_interleaved(dest, width, height);

        // Finally we byteswap if necessary
        if (!bigendian())
            byteswap_span(
                span<uint32_t>(reinterpret_cast<uint32_t*>(dest.data()),
                               dest.size() / 4));
    } break;
    default:
        errorfmt("Unknown bitdepth: {} encountered", m_header.depth);
        return false;
    }

    return ok;
};



bool
PSDInput::is_additional_info_psb(const char* key)
{
    for (unsigned int i = 0; i < additional_info_psb_count; ++i)
        if (std::memcmp(additional_info_psb[i], key, 4) == 0)
            return true;

    return false;
}

OIIO_PLUGIN_NAMESPACE_END

/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#include <fstream>
#include "psd_pvt.h"
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "jpeg_memory_src.h"
#include <setjmp.h>

#include "imageio.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace psd_pvt;

class PSDInput : public ImageInput {
public:
    PSDInput ();
    virtual ~PSDInput () { close(); }
    virtual const char * format_name (void) const { return "psd"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
	virtual int current_subimage () const { return m_subimage; }
	virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    struct LayerMaskInfo {
        uint64_t length;
        std::streampos pos;

        struct LayerInfo {
            uint64_t length;
            int16_t layer_count;
            std::streampos pos;
        };

        LayerInfo layer_info;
    };

    enum ColorMode {
        ColorMode_Bitmap = 0,
        ColorMode_Grayscale = 1,
        ColorMode_Indexed = 2,
        ColorMode_RGB = 3,
        ColorMode_CMYK = 4,
        ColorMode_Multichannel = 7,
        ColorMode_Duotone = 8,
        ColorMode_Lab = 9
    };

    enum ChannelID {
        Channel_Transparency = -1,
        Channel_LayerMask = -2,
        Channel_UserMask = -3
    };

    std::string m_filename;           ///< Stash the filename
    std::ifstream m_file;             ///< Open image handle
	int m_subimage;
	int m_subimage_count;
	FileHeader m_header;
	ColorModeData m_color_data;
	LayerMaskInfo m_layer_mask_info;
	std::vector<Layer> m_layers;
	GlobalMaskInfo m_global_mask_info;
	//We hold all ImageSpecs here. Index 0 always exists and is the composite
	//image. Index 1 is the first layer (if any), etc.
	std::vector<ImageSpec> m_specs;
	//This spec holds the extra_attribs for the merged composite
	ImageSpec m_merged_attribs;
	//This holds the common attributes that should be includedi in all subimages
	ImageSpec m_common_attribs;
	//merged composite image
    ImageDataSection m_image_data;
    //Names of the channels for our color mode + alpha names
    std::vector<std::string> m_mode_channels;
    //Names of the alpha channels (form image resource 1006)
    std::vector<std::string> m_alpha_channels;
    //Buffer for uncompressing RLE data
    std::string m_rle_buffer;
    //Buffers used to store channel data before interleaving
    std::vector<std::string> m_channel_buffers;
    //These are pointers to the actual channels to read for each subimage (including the merged composite)
    std::vector<std::vector<ChannelInfo *> > m_channels;
    /// Reset everything to initial state
    ///
    void init ();

    //File Header
    bool load_header ();
    bool read_header ();
    bool validate_header ();

    //Color Mode Data
    bool load_color_data ();
    bool read_color_data ();
    bool validate_color_data ();

    //Image Resources
    bool load_resources ();
    bool read_resource (ImageResourceBlock &block);
    bool validate_resource (ImageResourceBlock &block);

    //Image resource loaders to handle loading certain image resources into ImageSpec
    struct ResourceLoader {
        uint16_t resource_id;
        boost::function<bool (PSDInput *, uint32_t)> load;
    };
    static const ResourceLoader resource_loaders[];

    //Map image resource ID to image resource block
    typedef std::map<uint16_t, ImageResourceBlock> ImageResourceMap;

    //JPEG thumbnail
    bool load_resource_1036 (uint32_t length);
    bool load_resource_1033 (uint32_t length);
    bool load_resource_thumbnail (uint32_t length, bool isBGR);

    //ResolutionInfo
    bool load_resource_1005 (uint32_t length);

    //Alpha channel names
    bool load_resource_1006 (uint32_t length);

    //Pixel aspect ratio
    bool load_resource_1064 (uint32_t length);

    //For thumbnail loading
    struct thumbnail_error_mgr {
        jpeg_error_mgr pub;
        jmp_buf setjmp_buffer;
    };

    METHODDEF (void)
    thumbnail_error_exit (j_common_ptr cinfo);

    //Layers
    bool load_layers ();
    bool load_layer (Layer &layer);
    bool load_layer_image (Layer &layer);
    bool load_layer_channel (Layer &layer, ChannelInfo &channel_info);

    bool read_rle_lengths (uint32_t height, std::vector<uint32_t> &rle_lengths);

    //Global layer mask info
    bool load_global_mask_info ();

    //Global additional layer info
    bool load_global_additional ();

    //Merged image data
    bool load_merged_image ();

    //These are AdditionalInfo entries that, for PSBs, have an 8-byte length
    static const char *additional_info_psb[];
    static const unsigned int additional_info_psb_count;
    bool is_additional_info_psb (const char *key);

    //Pascal string has length stored first, then bytes of the string
    int read_pascal_string (std::string &s, uint16_t mod_padding);

    TypeDesc get_type_desc ();

    static const char *mode_channel_names[][4];
    static const unsigned int mode_channel_count[];

    //Sets up m_mode_channels with correct channel names for our color mode
    void set_mode_channels ();

    std::string get_channel_name (int16_t channel_id);

    void setup_specs ();

    template <typename TStorage, typename TVariable>
    bool read_bige (TVariable &value)
    {
        TStorage buffer;
        m_file.read ((char *)&buffer, sizeof(buffer));
        if (!bigendian ())
            swap_endian (&buffer);

        //value = boost::numeric_cast<TVariable>(buffer);
        value = buffer;
        if (!m_file)
            return false;

        return true;
    }

    bool read_channel_row (const ChannelInfo &channel_info, uint32_t row, char *data);
    static bool decompress_packbits (const char *src, char *dst, uint16_t packed_length, uint16_t unpacked_length);

    //Add an attribute to the current spec (m_spec)
    template <typename T>
    void attribute (const std::string &name, const T &value)
    {
        m_merged_attribs.attribute (name, value);
    }

    //Add an attribute to the current spec (m_spec)
    template <typename T>
    void attribute (const std::string &name, const TypeDesc &type, const T &value)
    {
        m_merged_attribs.attribute (name, type, value);
    }

    //Add an attribute to the current spec (m_spec) and common spec (m_common_spec)
    template <typename T>
    void common_attribute (const std::string &name, const T &value)
    {
        m_merged_attribs.attribute (name, value);
        m_common_attribs.attribute (name, value);
    }

    //Add an attribute to the current spec (m_spec) and common spec (m_common_spec)
    template <typename T>
    void common_attribute (const std::string &name, const TypeDesc &type, const T &value)
    {
        m_merged_attribs.attribute (name, type, value);
        m_common_attribs.attribute (name, type, value);
    }

};

// Image resource loaders
// To add an image resource loader, do the following:
// 1) Add ADD_LOADER(<ResourceID>) below
// 2) Add a method in PSDInput:
//    bool load_resource_<ResourceID> (uint32_t length, ImageSpec &spec);
// Note that currently, failure to load a resource is ignored
#define ADD_LOADER(id) {id, boost::bind (&PSDInput::load_resource_##id, _1, _2)}
const PSDInput::ResourceLoader PSDInput::resource_loaders[] = {
    ADD_LOADER(1033),
    ADD_LOADER(1036),
    ADD_LOADER(1005),
    ADD_LOADER(1006),
    ADD_LOADER(1064)
};
#undef ADD_LOADER

const char *
PSDInput::additional_info_psb[] =
{
    "LMsk",
    "Lr16",
    "Lr32",
    "Layr",
    "Mt16",
    "Mt32",
    "Mtrn",
    "Alph",
    "FMsk",
    "Ink2",
    "FEid",
    "FXid",
    "PxSD"
};
const unsigned int
PSDInput::additional_info_psb_count = sizeof(additional_info_psb) / sizeof(additional_info_psb[0]);

const char *
PSDInput::mode_channel_names[][4] =
{
    {"A"},
    {"I"},
    {"I"},
    {"R", "G", "B"},
    {"C", "M", "Y", "K"},
    {},
    {},
    {},
    {},
    {"L", "a", "b"}
};

const unsigned int
PSDInput::mode_channel_count[] =
{
    1, 1, 1, 3, 4, 0, 0, 0, 0, 3
};

// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageInput *psd_input_imageio_create () { return new PSDInput; }

DLLEXPORT int psd_imageio_version = OIIO_PLUGIN_VERSION;

DLLEXPORT const char * psd_input_extensions[] = {
    "psd", "pdd", "8bps", "psb", "8bpd", NULL
};

OIIO_PLUGIN_EXPORTS_END



PSDInput::PSDInput ()
{
    init();
}



bool
PSDInput::open (const std::string &name, ImageSpec &newspec)
{
    m_filename = name;
    m_file.open (name.c_str(), std::ios::binary);
    if (!m_file.is_open ()) {
        error ("\"%s\": failed to open file", name.c_str());
        return false;
    }
    //File Header
    if (!load_header ())
        return false;

    //Color Mode Data
    if (!load_color_data ())
        return false;

    //Image Resources
    if (!load_resources ())
        return false;

    set_mode_channels ();

    if (!load_layers ())
        return false;

    if (!load_global_mask_info ())
        return false;

    if (!load_global_additional ())
        return false;

    if (!load_merged_image ())
        return false;

    setup_specs ();

    if (!seek_subimage (0, 0, newspec))
        return false;

    return true;
}



bool
PSDInput::close ()
{
    init();  // Reset to initial state
    return true;
}



bool
PSDInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    if (miplevel != 0)
        return false;

    if (subimage < 0 || subimage >= m_subimage_count)
        return false;

    m_subimage = subimage;
    newspec = m_spec = m_specs[subimage];
	return true;
}



bool
PSDInput::read_native_scanline (int y, int z, void *data)
{
    if (y < 0 || y > m_spec.height)
        return false;

    if (m_channel_buffers.size () < (std::size_t)m_spec.nchannels)
        m_channel_buffers.resize (m_spec.nchannels);
    std::vector<ChannelInfo *> &channels = m_channels[m_subimage];
    for (int c = 0; c < (int)channels.size (); ++c) {
        std::string &buffer = m_channel_buffers[c];
        ChannelInfo &channel_info = *channels[c];
        if (buffer.size () < channel_info.row_length)
            buffer.resize (channel_info.row_length);

        if (!read_channel_row (channel_info, y, &buffer[0]))
            return false;
    }
    char *dst = (char *)data;
    for (int i = 0; i < m_spec.width; ++i) {
        for (int c = 0; c < (int)channels.size (); ++c) {
            std::string &buffer = m_channel_buffers[c];
            *dst++ = buffer[i];
        }
    }
    return true;
}



void
PSDInput::init ()
{
    m_filename.clear ();
    m_file.close ();
	m_subimage = -1;
	m_subimage_count = 0;
    std::memset (&m_header, 0, sizeof (m_header));
    std::memset (&m_color_data, 0, sizeof (m_color_data));
    std::memset (&m_layer_mask_info, 0, sizeof (m_layer_mask_info));
    m_layers.clear ();
    std::memset (&m_global_mask_info, 0, sizeof (m_global_mask_info));
    m_specs.clear ();
    m_merged_attribs = ImageSpec ();
    m_common_attribs = ImageSpec ();
    std::memset (&m_image_data, 0, sizeof (m_image_data));
    m_mode_channels.clear ();
    m_alpha_channels.clear ();
    m_rle_buffer.clear ();
    m_channel_buffers.clear ();
    m_channels.clear ();
}



bool
PSDInput::load_header ()
{
    if (!read_header () || !validate_header ())
        return false;

    return true;
}



bool
PSDInput::read_header ()
{
    m_file.read (m_header.signature, 4);
    read_bige<uint16_t> (m_header.version);
    //skip reserved bytes (the specs say they should be zeroed, maybe we should check)
    m_file.seekg(6, std::ios::cur);
    read_bige<uint16_t> (m_header.channel_count);
    read_bige<uint32_t> (m_header.height);
    read_bige<uint32_t> (m_header.width);
    read_bige<uint16_t> (m_header.depth);
    read_bige<uint16_t> (m_header.color_mode);
    return m_file;
}



bool
PSDInput::validate_header ()
{
    if (std::memcmp (m_header.signature, "8BPS", 4) != 0) {
        error ("[Header] invalid signature");
        return false;
    }
    if (m_header.version != 1 && m_header.version != 2) {
        error ("[Header] invalid version");
        return false;
    }
    if (m_header.channel_count < 1 || m_header.channel_count > 56) {
        error ("[Header] invalid channel count");
        return false;
    }
    switch (m_header.version) {
        case 1:
            //PSD
            // width/height range: [1,30000]
            if (m_header.height < 1 || m_header.height > 30000) {
                error ("[Header] invalid image height");
                return false;
            }
            if (m_header.width < 1 || m_header.width > 30000) {
                error ("[Header] invalid image width");
                return false;
            }
            break;
        case 2:
            //PSB (Large Document Format)
            // width/height range: [1,300000]
            if (m_header.height < 1 || m_header.height > 300000) {
                error ("[Header] invalid image height");
                return false;
            }
            if (m_header.width < 1 || m_header.width > 300000) {
                error ("[Header] invalid image width");
                return false;
            }
            break;
    }
    //Valid depths are 1,8,16,32
    if (m_header.depth != 1 && m_header.depth != 8 && m_header.depth != 16 && m_header.depth != 32) {
        error ("[Header] invalid depth");
        return false;
    }
    //There are other (undocumented) color modes not listed here
    switch (m_header.color_mode) {
        case ColorMode_RGB :
            break;
        case ColorMode_Bitmap :
        case ColorMode_Grayscale :
        case ColorMode_Indexed :
        case ColorMode_CMYK :
        case ColorMode_Multichannel :
        case ColorMode_Duotone :
        case ColorMode_Lab :
            error ("[Header] unsupported color mode");
            return false;
            break;
        default:
            error ("[Header] unrecognized color mode");
            return false;
    }
    return true;
}



bool
PSDInput::load_color_data ()
{
    return read_color_data () && validate_color_data ();
}



bool
PSDInput::read_color_data ()
{
    read_bige<uint32_t> (m_color_data.length);
    //remember the file position of the actual color mode data (if any)
    m_color_data.pos = m_file.tellg();
    m_file.seekg (m_color_data.length, std::ios::cur);
    return m_file;
}



bool
PSDInput::validate_color_data ()
{
    if (m_header.color_mode == ColorMode_Duotone && m_color_data.length == 0) {
        error ("[Color Mode Data] color mode data should be present for duotone image");
        return false;
    }
    if (m_header.color_mode == ColorMode_Indexed && m_color_data.length != 768) {
        error ("[Color Mode Data] length should be 768 for indexed color mode");
        return false;
    }
    return true;
}



bool
PSDInput::load_resources ()
{
    //Length of Image Resources section
    uint32_t length;
    if (!read_bige<uint32_t> (length))
        return false;

    ImageResourceBlock block;
    ImageResourceMap resources;
    std::streampos section_start = m_file.tellg();
    std::streampos section_end = section_start + (std::streampos)length;
    while (m_file && m_file.tellg () < section_end) {
        if (!read_resource (block) || !validate_resource (block))
            return false;

        resources[block.id] = block;
    }
    if (!m_file)
        return false;

    const ImageResourceMap::const_iterator end (resources.end ());
    BOOST_FOREACH (const ResourceLoader &loader, resource_loaders) {
        ImageResourceMap::const_iterator it (resources.find (loader.resource_id));
        //If we have an ImageResourceLoader for that resource ID, call it
        if (it != end) {
            m_file.seekg (it->second.pos);
            loader.load (this, it->second.length);
        }
    }
    m_file.seekg (section_end);
    return m_file;
}



bool
PSDInput::read_resource (ImageResourceBlock &block)
{
    m_file.read (block.signature, 4);
    read_bige<uint16_t> (block.id);
    read_pascal_string (block.name, 2);
    read_bige<uint32_t> (block.length);
    //Save the file position of the image resource data
    block.pos = m_file.tellg();
    //Skip the image resource data
    m_file.seekg (block.length, std::ios::cur);
    // image resource blocks are padded to an even size, so skip padding
    if (block.length % 2 != 0)
        m_file.seekg(1, std::ios::cur);

    return m_file;
}



bool
PSDInput::validate_resource (ImageResourceBlock &block)
{
    if (std::memcmp (block.signature, "8BIM", 4) != 0) {
        error ("[Image Resource] invalid signature");
        return false;
    }
    return true;
}



bool
PSDInput::load_resource_1033 (uint32_t length)
{
    return load_resource_thumbnail (length, true);
}



bool
PSDInput::load_resource_1036 (uint32_t length)
{
    return load_resource_thumbnail (length, false);
}



bool
PSDInput::load_resource_thumbnail (uint32_t length, bool isBGR)
{
    uint32_t format;
    uint32_t width, height;
    uint32_t widthbytes;
    uint32_t total_size;
    uint32_t compressed_size;
    uint16_t bpp;
    uint16_t planes;
    int stride;
    jpeg_decompress_struct cinfo;
    thumbnail_error_mgr jerr;
    uint32_t jpeg_length = length - 28;

    read_bige<uint32_t> (format);
    read_bige<uint32_t> (width);
    read_bige<uint32_t> (height);
    read_bige<uint32_t> (widthbytes);
    read_bige<uint32_t> (total_size);
    read_bige<uint32_t> (compressed_size);
    read_bige<uint16_t> (bpp);
    read_bige<uint16_t> (planes);
    if (!m_file)
        return false;

    //This is the only format, according to the specs
    if (format != kJpegRGB || bpp != 24 || planes != 1)
        return false;

    cinfo.err = jpeg_std_error (&jerr.pub);
    jerr.pub.error_exit = thumbnail_error_exit;
    if (setjmp (jerr.setjmp_buffer)) {
        jpeg_destroy_decompress (&cinfo);
        return false;
    }
    std::string jpeg_data (jpeg_length, '\0');
    if (!m_file.read (&jpeg_data[0], jpeg_length))
        return false;

    jpeg_create_decompress (&cinfo);
    jpeg_memory_src (&cinfo, (unsigned char *)&jpeg_data[0], jpeg_length);
    jpeg_read_header (&cinfo, TRUE);
    jpeg_start_decompress (&cinfo);
    stride = cinfo.output_width * cinfo.output_components;
    unsigned int thumbnail_bytes = cinfo.output_width * cinfo.output_height * cinfo.output_components;
    std::string thumbnail_image (thumbnail_bytes, '\0');
    //jpeg_destroy_decompress will deallocate this
    JSAMPLE **buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, stride, 1);
    while (cinfo.output_scanline < cinfo.output_height) {
        if (jpeg_read_scanlines (&cinfo, buffer, 1) != 1) {
            jpeg_finish_decompress (&cinfo);
            jpeg_destroy_decompress (&cinfo);
            return false;
        }
        std::memcpy (&thumbnail_image[(cinfo.output_scanline - 1) * stride],
                    (char *)buffer[0],
                    stride);
    }
    jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);
    attribute ("thumbnail_width", (int)width);
    attribute ("thumbnail_height", (int)height);
    attribute ("thumbnail_nchannels", 3);
    if (isBGR) {
        for (unsigned int i = 0; i < thumbnail_bytes - 2; i += 3)
            std::swap (thumbnail_image[i], thumbnail_image[i + 2]);
    }
    attribute ("thumbnail_image",
                   TypeDesc (TypeDesc::UINT8, thumbnail_image.size ()),
                   &thumbnail_image[0]);
    return true;
}



bool
PSDInput::load_resource_1005 (uint32_t length)
{
    ResolutionInfo resinfo;
    //Fixed 16.16
    read_bige<uint32_t> (resinfo.hRes);
    resinfo.hRes /= 65536.0f;
    read_bige<int16_t> (resinfo.hResUnit);
    read_bige<int16_t> (resinfo.widthUnit);
    //Fixed 16.16
    read_bige<uint32_t> (resinfo.vRes);
    resinfo.vRes /= 65536.0f;
    read_bige<int16_t> (resinfo.vResUnit);
    read_bige<int16_t> (resinfo.heightUnit);
    if (!m_file)
        return false;

    if (!(resinfo.hResUnit == resinfo.vResUnit
          && (resinfo.hResUnit == ResolutionInfo::PixelsPerInch
          || resinfo.hResUnit == ResolutionInfo::PixelsPerCentimeter))) {
          error ("[Image Resource] [ResolutionInfo] Unrecognized resolution unit");
          return false;
    }
    common_attribute ("XResolution", resinfo.hRes);
    common_attribute ("XResolution", resinfo.hRes);
    common_attribute ("YResolution", resinfo.vRes);
    switch (resinfo.hResUnit) {
        case ResolutionInfo::PixelsPerInch:
            common_attribute ("ResolutionUnit", "in");
            break;
        case ResolutionInfo::PixelsPerCentimeter:
            common_attribute ("ResolutionUnit", "cm");
            break;
    };
    return true;
}



bool
PSDInput::load_resource_1006 (uint32_t length)
{
    int32_t bytes_remaining = length;
    std::string name;
    while (m_file && bytes_remaining >= 2) {
        bytes_remaining -= read_pascal_string (name, 1);
        m_alpha_channels.push_back (name);
    }
    return m_file;
}



bool
PSDInput::load_resource_1064 (uint32_t length)
{
    uint32_t version;
    read_bige<uint32_t> (version);
    if (version != 1 && version != 2) {
        error ("[Image Resource] [Pixel Aspect Ratio] Unrecognized version");
        return false;
    }
    double aspect_ratio;
    read_bige<double> (aspect_ratio);
    //FIXME warn on loss of precision?
    common_attribute ("PixelAspectRatio", (float)aspect_ratio);
    return m_file;
}



void
PSDInput::thumbnail_error_exit (j_common_ptr cinfo)
{
    thumbnail_error_mgr *mgr = (thumbnail_error_mgr *)cinfo->err;
    longjmp (mgr->setjmp_buffer, 1);
}

bool
PSDInput::load_layers ()
{
    if (m_header.version == 1)
        read_bige<uint32_t> (m_layer_mask_info.length);
    else
        read_bige<uint64_t> (m_layer_mask_info.length);

    m_layer_mask_info.pos = m_file.tellg ();
    if (!m_file)
        return false;

    if (!m_layer_mask_info.length)
        return true;

    LayerMaskInfo::LayerInfo &layer_info = m_layer_mask_info.layer_info;
    if (m_header.version == 1)
        read_bige<uint32_t> (layer_info.length);
    else
        read_bige<uint64_t> (layer_info.length);

    layer_info.pos = m_file.tellg ();
    if (!m_file)
        return false;

    if (!layer_info.length)
        return true;

    read_bige<int16_t> (layer_info.layer_count);
    m_image_data.merged_transparency = false;
    if (layer_info.layer_count < 0) {
        m_image_data.merged_transparency = true;
        layer_info.layer_count = -layer_info.layer_count;
    }
    m_layers.resize (layer_info.layer_count);
    for (int16_t layer_nbr = 0; layer_nbr < layer_info.layer_count; ++layer_nbr) {
        Layer &layer = m_layers[layer_nbr];
        if (!load_layer (layer))
            return false;
    }
    for (int16_t layer_nbr = 0; layer_nbr < layer_info.layer_count; ++layer_nbr) {
        Layer &layer = m_layers[layer_nbr];
        if (!load_layer_image (layer))
            return false;
    }
    return true;
}



bool
PSDInput::load_layer (Layer &layer)
{
    read_bige<uint32_t> (layer.top);
    read_bige<uint32_t> (layer.left);
    read_bige<uint32_t> (layer.bottom);
    read_bige<uint32_t> (layer.right);
    read_bige<uint16_t> (layer.channel_count);
    if (!m_file)
        return false;

    layer.width = std::abs(layer.right - layer.left);
    layer.height = std::abs(layer.bottom - layer.top);
    layer.channel_info.resize (layer.channel_count);
    for(uint16_t channel = 0; channel < layer.channel_count; channel++) {
        ChannelInfo &channel_info = layer.channel_info[channel];
        read_bige<int16_t> (channel_info.channel_id);
        if (m_header.version == 1)
            read_bige<uint32_t> (channel_info.data_length);
        else
            read_bige<uint64_t> (channel_info.data_length);

        layer.channel_id_map[channel_info.channel_id] = &channel_info;
        channel_info.name = get_channel_name (channel_info.channel_id);
    }
    char bm_signature[4];
    m_file.read (bm_signature, 4);
    if (!m_file)
        return false;

    if (std::memcmp (bm_signature, "8BIM", 4) != 0) {
        error ("[Layer Record] Invalid blend mode signature");
        return false;
    }
    m_file.read (layer.bm_key, 4);
    read_bige<uint8_t> (layer.opacity);
    read_bige<uint8_t> (layer.clipping);
    read_bige<uint8_t> (layer.flags);
    //skip filler
    m_file.seekg(1, std::ios::cur);
    read_bige<uint32_t> (layer.extra_length);
    uint32_t extra_remaining = layer.extra_length;
    //layer mask data length
    uint32_t lmd_length;
    read_bige<uint32_t> (lmd_length);
    if (!m_file)
        return false;

    switch (lmd_length) {
        case 0:
            break;
        case 20:
            read_bige<uint32_t> (layer.mask_data.top);
            read_bige<uint32_t> (layer.mask_data.left);
            read_bige<uint32_t> (layer.mask_data.bottom);
            read_bige<uint32_t> (layer.mask_data.right);
            read_bige<uint8_t> (layer.mask_data.default_color);
            read_bige<uint8_t> (layer.mask_data.flags);
            //skip padding
            m_file.seekg(2, std::ios::cur);
            break;
        case 36:
            m_file.seekg (18, std::ios::cur);
            read_bige<uint8_t> (layer.mask_data.flags);
            read_bige<uint8_t> (layer.mask_data.default_color);
            read_bige<uint32_t> (layer.mask_data.top);
            read_bige<uint32_t> (layer.mask_data.left);
            read_bige<uint32_t> (layer.mask_data.bottom);
            read_bige<uint32_t> (layer.mask_data.right);
            break;
        default:
            error ("[Layer Mask Data] invalid size");
            //Actually, we could just skip this block
            return false;
            break;
    };
    extra_remaining -= (lmd_length + 4);

    //layer blending ranges length
    uint32_t lbr_length;
    read_bige<uint32_t> (lbr_length);
    //skip block
    m_file.seekg (lbr_length, std::ios::cur);
    extra_remaining -= (lbr_length + 4);
    if (!m_file)
        return false;

    extra_remaining -= read_pascal_string(layer.name, 4);
    while (m_file && extra_remaining >= 12) {
        layer.additional_info.push_back (Layer::AdditionalInfo());
        Layer::AdditionalInfo &info = layer.additional_info.back();

        char signature[4];
        m_file.read (signature, 4);
        m_file.read (info.key, 4);
        if (std::memcmp (signature, "8BIM", 4) != 0
            && std::memcmp (signature, "8B64", 4) != 0) {
            error ("[Additional Layer Info] invalid signature");
            return false;
        }
        extra_remaining -= 8;
        if (m_header.version == 2 && is_additional_info_psb (info.key)) {
            read_bige<uint64_t> (info.length);
            extra_remaining -= 8;
        } else {
            read_bige<uint32_t> (info.length);
            extra_remaining -= 4;
        }
        m_file.seekg (info.length, std::ios::cur);
        extra_remaining -= info.length;
    }
    return m_file;
}



bool
PSDInput::load_layer_image (Layer &layer)
{
    for (uint16_t channel = 0; channel < layer.channel_count; ++channel) {
        ChannelInfo &channel_info = layer.channel_info[channel];
        if (!load_layer_channel (layer, channel_info))
            return false;
    }
    return true;
}



bool
PSDInput::load_layer_channel (Layer &layer, ChannelInfo &channel_info)
{
    std::streampos start_pos = m_file.tellg ();
    if (channel_info.data_length >= 2) {
        if (!read_bige<uint16_t> (channel_info.compression))
            return false;
    }
    //No data at all or just compression
    if (channel_info.data_length <= 2)
        return true;

    channel_info.data_pos = m_file.tellg ();
    channel_info.row_pos.resize (layer.height);
    channel_info.row_length = (layer.width * m_header.depth + 7) / 8;
    switch (channel_info.compression) {
        case Compression_Raw:
            channel_info.row_pos[0] = channel_info.data_pos;
            for (uint32_t i = 1; i < layer.height; ++i)
                channel_info.row_pos[i] = channel_info.row_pos[i - 1] + (std::streampos)channel_info.row_length;

            channel_info.data_length = channel_info.row_length * layer.height;
            break;
        case Compression_RLE:
            //RLE lengths are stored before the channel data
            if (!read_rle_lengths (layer.height, channel_info.rle_lengths))
                return false;

            //channel data is located after the RLE lengths
            channel_info.data_pos = m_file.tellg ();
            //subtract the RLE lengths read above
            channel_info.data_length = channel_info.data_length - (channel_info.data_pos - start_pos);
            channel_info.row_pos[0] = channel_info.data_pos;
            for (uint32_t i = 1; i < layer.height; ++i)
                channel_info.row_pos[i] = channel_info.row_pos[i - 1] + (std::streampos)channel_info.rle_lengths[i - 1];

            break;
        case Compression_ZIP:
            return false;
            break;
        case Compression_ZIP_Predict:
            return false;
            break;
        default:
            return false;
            break;
    }
    m_file.seekg (channel_info.data_length, std::ios::cur);
    return m_file;

}





bool
PSDInput::read_rle_lengths (uint32_t height, std::vector<uint32_t> &rle_lengths)
{
    rle_lengths.resize (height);
    for (uint32_t row = 0; row < height && m_file; ++row) {
        if (m_header.version == 1)
            read_bige<uint16_t> (rle_lengths[row]);
        else
            read_bige<uint32_t> (rle_lengths[row]);
    }
    return m_file;
}



bool
PSDInput::load_global_mask_info ()
{
    m_file.seekg (m_layer_mask_info.layer_info.pos + (std::streampos)m_layer_mask_info.layer_info.length);
    uint32_t length;
    read_bige<uint32_t> (length);
    std::streampos start = m_file.tellg ();
    std::streampos end = start + (std::streampos)length;
    if (!m_file)
        return false;

    //this can be empty
    if (!length)
        return true;

    read_bige<uint16_t> (m_global_mask_info.overlay_color_space);
    for (int i = 0; i < 4; ++i)
        read_bige<uint16_t> (m_global_mask_info.color_components[i]);

    read_bige<uint16_t> (m_global_mask_info.opacity);
    read_bige<int16_t> (m_global_mask_info.kind);
    m_file.seekg (end);
    return m_file;
}



bool
PSDInput::load_global_additional ()
{
    char signature[4];
    char key[4];
    uint64_t length;
    uint64_t remaining = m_layer_mask_info.length - (m_file.tellg () - m_layer_mask_info.pos);
    while (m_file && remaining >= 12) {
        m_file.read (signature, 4);
        if (std::memcmp (signature, "8BIM", 4) != 0) {
            error ("[Global Additional Layer Info] invalid signature");
            return false;
        }
        m_file.read (key, 4);
        if (!m_file)
            return false;

        remaining -= 8;
        if (m_header.version == 2 && is_additional_info_psb (key)) {
            read_bige<uint64_t> (length);
            remaining -= 8;
        } else {
            read_bige<uint32_t> (length);
            remaining -= 4;
        }
        //This is irritating and I was puzzled until I saw comments in
        //the code of XeePhotoshopLoader.m.
        //
        //Long story short these are aligned to 4 bytes but that is not
        //included in the stored length and the specs do not mention it.

        //round up to multiple of 4
        length = (length + 3) & ~3;
        remaining -= length;
        //skip it for now
        m_file.seekg (length, std::ios::cur);
    }
    if (!m_file)
        return false;

    return true;
}



bool
PSDInput::load_merged_image ()
{
    uint16_t compression;
    uint32_t row_length = (m_header.width * m_header.depth + 7) / 8;
    int16_t id = 0;
    read_bige<uint16_t> (compression);
    if (!m_file)
        return false;

    if (compression != Compression_Raw && compression != Compression_RLE) {
        error ("[Image Data Section] unsupported compression");
        return false;
    }
    m_image_data.channel_info.resize (m_header.channel_count);
    //setup some generic properties and read any RLE lengths
    //Image Data Section has RLE lengths for all channels stored first
    BOOST_FOREACH (ChannelInfo &channel_info, m_image_data.channel_info) {
        channel_info.compression = compression;
        channel_info.channel_id = id++;
        channel_info.name = get_channel_name (channel_info.channel_id);
        channel_info.data_length = row_length * m_header.height;
        if (compression == Compression_RLE) {
            if (!read_rle_lengths (m_header.height, channel_info.rle_lengths))
                return false;
        }
    }
    BOOST_FOREACH (ChannelInfo &channel_info, m_image_data.channel_info) {
        channel_info.row_pos.resize (m_header.height);
        channel_info.data_pos = m_file.tellg ();
        channel_info.row_length = (m_header.width * m_header.depth + 7) / 8;
        assert (m_file);
        switch (compression) {
            case Compression_Raw:
                channel_info.row_pos[0] = channel_info.data_pos;
                for (uint32_t i = 1; i < m_header.height; ++i)
                    channel_info.row_pos[i] = channel_info.row_pos[i - 1] + (std::streampos)row_length;

                m_file.seekg (channel_info.row_pos.back () + (std::streampos)row_length);
                break;
            case Compression_RLE:
                channel_info.row_pos[0] = channel_info.data_pos;
                for (uint32_t i = 1; i < m_header.height; ++i)
                    channel_info.row_pos[i] = channel_info.row_pos[i - 1] + (std::streampos)channel_info.rle_lengths[i - 1];

                m_file.seekg (channel_info.row_pos.back () + (std::streampos)channel_info.rle_lengths.back ());
                break;
        }
    }
    return true;
}



bool
PSDInput::read_channel_row (const ChannelInfo &channel_info, uint32_t row, char *data)
{
    if (row >= channel_info.row_pos.size ())
        return false;

    uint32_t rle_length;
    m_file.seekg (channel_info.row_pos[row]);
    switch (channel_info.compression) {
        case Compression_Raw:
            m_file.read (data, channel_info.row_length);
            break;
        case Compression_RLE:
            rle_length = channel_info.rle_lengths[row];
            if (m_rle_buffer.size () < rle_length)
                m_rle_buffer.resize (rle_length);

            m_file.read (&m_rle_buffer[0], rle_length);
            if (!m_file)
                return false;

            if (!decompress_packbits (&m_rle_buffer[0], data, rle_length, channel_info.row_length))
                return false;

            break;
    }
    return m_file;
}



bool
PSDInput::is_additional_info_psb (const char *key)
{
    for (unsigned int i = 0; i < additional_info_psb_count; ++i)
        if (std::memcmp (additional_info_psb[i], key, 4) == 0)
            return true;

    return false;
}



int
PSDInput::read_pascal_string (std::string &s, uint16_t mod_padding)
{
    s.clear();
    uint8_t length;
    int bytes = 0;
    if (m_file.read ((char *)&length, 1)) {
        bytes = 1;
        if (length == 0) {
            if (m_file.seekg (mod_padding - 1, std::ios::cur))
                bytes += mod_padding - 1;
        } else {
            s.resize (length);
            if (m_file.read (&s[0], length)) {
                bytes += length;
                if (mod_padding > 0) {
                    for (int padded_length = length + 1; padded_length % mod_padding != 0; padded_length++) {
                        if (!m_file.seekg(1, std::ios::cur))
                            break;

                        bytes++;
                    }
                }
            }
        }
    }
    return bytes;
}



TypeDesc
PSDInput::get_type_desc ()
{
    switch (m_header.depth) {
        case 1:
        case 8:
            return TypeDesc::UINT8;
            break;
        case 16:
            return TypeDesc::UINT16;
            break;
        case 32:
            return TypeDesc::UINT32;
            break;
    };
    //not reached
    return TypeDesc::UINT8;
}



void
PSDInput::set_mode_channels ()
{
    for (unsigned int i = 0; i < mode_channel_count[m_header.color_mode]; ++i)
        m_mode_channels.push_back (mode_channel_names[m_header.color_mode][i]);

    m_mode_channels.insert (m_mode_channels.end (), m_alpha_channels.begin (), m_alpha_channels.end ());
}



std::string
PSDInput::get_channel_name (int16_t channel_id)
{
    switch (channel_id) {
        case Channel_Transparency:
            return "Transparency";
        case Channel_LayerMask:
            return "Layer Mask";
        case Channel_UserMask:
            return "User Mask";
    }
    if (channel_id >= 0 && channel_id < (int16_t)m_mode_channels.size ())
        return m_mode_channels[channel_id];

    return NULL;
}



bool
PSDInput::decompress_packbits (const char *src, char *dst, uint16_t packed_length, uint16_t unpacked_length)
{
    int32_t src_remaining = packed_length;
    int32_t dst_remaining = unpacked_length;
    int16_t header;
    int length;

    while (src_remaining > 0 && dst_remaining > 0) {
        header = *src++;
        src_remaining--;

        if (header == 128)
            continue;
        else if (header >= 0) {
            // (1 + n) literal bytes
            length = 1 + header;
            src_remaining -= length;
            dst_remaining -= length;
            if (src_remaining < 0 || dst_remaining < 0)
                return false;

            std::memcpy (dst, src, length);
            src += length;
            dst += length;
        } else {
            // repeat byte (1 - n) times
            length = 1 - header;
            src_remaining--;
            dst_remaining -= length;
            if (src_remaining < 0 || dst_remaining < 0)
                return false;

            std::memset (dst, *src, length);
            src++;
            dst += length;
        }
    }
    return true;
}



void
PSDInput::setup_specs ()
{
    m_subimage_count = m_layers.size () + 1;

    int channel_count = m_image_data.merged_transparency ? 4 : 3;
    m_channels.reserve (m_subimage_count);
    m_channels.resize (1);
    m_channels[0].reserve (channel_count);
    for (int i = 0; i < 3; ++i)
        m_channels[0].push_back (&m_image_data.channel_info[i]);

    if (m_image_data.merged_transparency && m_header.channel_count > mode_channel_count[m_header.color_mode])
        m_channels[0].push_back (&m_image_data.channel_info[mode_channel_count[m_header.color_mode]]);

    m_specs.reserve (m_subimage_count);
    m_specs.push_back (ImageSpec (m_header.width, m_header.height, channel_count, get_type_desc ()));
    m_specs.back ().extra_attribs = m_merged_attribs.extra_attribs;

    BOOST_FOREACH (Layer &layer, m_layers) {
        channel_count = layer.channel_id_map.count(Channel_Transparency) ? 4 : 3;
        m_specs.push_back (ImageSpec (layer.width, layer.height, channel_count, get_type_desc ()));
        m_specs.back ().extra_attribs = m_common_attribs.extra_attribs;
        m_channels.resize (m_channels.size () + 1);
        std::vector<ChannelInfo *> &channels = m_channels.back ();
        for (int i = 0; i < 3; ++i)
            channels.push_back (layer.channel_id_map[i]);

        if (channel_count == 4)
            channels.push_back (layer.channel_id_map[Channel_Transparency]);
    }
}

OIIO_PLUGIN_NAMESPACE_END

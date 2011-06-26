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
#include <vector>
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
	virtual int current_subimage () { return m_subimage; }
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

    std::string m_filename;           ///< Stash the filename
    std::ifstream m_file;             ///< Open image handle
	int m_subimage;
	int m_subimage_count;
	FileHeader m_header;
	ColorModeData m_color_data;
	LayerMaskInfo m_layer_mask_info;
	std::vector<Layer> m_layers;
	GlobalMaskInfo m_global_mask_info;
    /// Reset everything to initial state
    ///
    void init ();

    //File Header
    bool load_header (ImageSpec &spec);
    bool read_header ();
    bool validate_header ();

    //Color Mode Data
    bool load_color_data (ImageSpec &spec);
    bool read_color_data ();
    bool validate_color_data ();

    //Image Resources
    bool load_resources (ImageSpec &spec);
    bool read_resource (ImageResourceBlock &block);
    bool validate_resource (ImageResourceBlock &block);

    //Image resource loaders to handle loading certain image resources into ImageSpec
    struct ResourceLoader {
        uint16_t resource_id;
        boost::function<bool (PSDInput *, uint32_t, ImageSpec &)> load;
    };
    static const ResourceLoader resource_loaders[];

    //Map image resource ID to image resource block
    typedef std::map<uint16_t, ImageResourceBlock> ImageResourceMap;

    //JPEG thumbnail
    bool load_resource_1036 (uint32_t length, ImageSpec &spec);
    bool load_resource_1033 (uint32_t length, ImageSpec &spec);
    bool load_resource_thumbnail (uint32_t length, ImageSpec &spec, bool isBGR);

    //ResolutionInfo
    bool load_resource_1005 (uint32_t length, ImageSpec &spec);

    //Alpha channel names
    bool load_resource_1006 (uint32_t length, ImageSpec &spec);

    //For thumbnail loading
    struct thumbnail_error_mgr {
        jpeg_error_mgr pub;
        jmp_buf setjmp_buffer;
    };

    METHODDEF (void)
    thumbnail_error_exit (j_common_ptr cinfo);

    //Layers
    bool load_layers (ImageSpec &spec);
    bool load_layer (Layer &layer);
    bool load_layer_image (Layer &layer);
    bool load_layer_channel (Layer &layer, Layer::ChannelInfo &channel_info);
    bool read_rle_lengths (Layer &layer, Layer::ChannelInfo &channel_info);

    //Global layer mask info
    bool load_global_mask_info (ImageSpec &spec);

    //Global additional layer info
    bool load_global_additional (ImageSpec &spec);

    //Merged image data
    bool load_merged_image (ImageSpec &spec);

    //These are AdditionalInfo entries that, for PSBs, have an 8-byte length
    static const char *additional_info_psb[];
    static const std::size_t additional_info_psb_count;
    bool is_additional_info_psb (const char *key);

    //Pascal string has length stored first, then bytes of the string
    int read_pascal_string (std::string &s, uint16_t mod_padding);

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

};

// Image resource loaders
// To add an image resource loader, do the following:
// 1) Add ADD_LOADER(<ResourceID>) below
// 2) Add a method in PSDInput:
//    bool load_resource_<ResourceID> (uint32_t length, ImageSpec &spec);
// Note that currently, failure to load a resource is ignored
#define ADD_LOADER(id) {id, boost::bind (&PSDInput::load_resource_##id, _1, _2, _3)}
const PSDInput::ResourceLoader PSDInput::resource_loaders[] = {
    ADD_LOADER(1033),
    ADD_LOADER(1036),
    ADD_LOADER(1005),
    ADD_LOADER(1006)
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
const std::size_t
PSDInput::additional_info_psb_count = sizeof(additional_info_psb) / sizeof(additional_info_psb[0]);

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
    if (!load_header (newspec))
        return false;

    //Color Mode Data
    if (!load_color_data (newspec))
        return false;

    //Image Resources
    if (!load_resources (newspec))
        return false;

    if (newspec.channelnames.empty ())
        newspec.default_channel_names ();

    if (!load_layers (newspec))
        return false;

    if (!load_global_mask_info (newspec))
        return false;

    if (!load_global_additional (newspec))
        return false;

    if (!load_merged_image (newspec))
        return false;

    return true;
}



bool
PSDInput::close ()
{
    init();  // Reset to initial state
    return false;
}



bool
PSDInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    if (miplevel != 0)
        return false;

    if (subimage < 0 || subimage > m_subimage_count)
        return false;

    if (subimage == current_subimage ()) {
        //TODO
    }
	return true;
}



bool
PSDInput::read_native_scanline (int y, int z, void *data)
{
    return false;
}



void
PSDInput::init ()
{
    m_file.close ();
	m_subimage = -1;
	m_subimage_count = 0;
}



bool
PSDInput::load_header (ImageSpec &spec)
{
    if (!read_header () || !validate_header ())
        return false;

    TypeDesc typedesc;
    switch (m_header.depth) {
        case 1:
        case 8:
            typedesc = TypeDesc::UINT8;
            break;
        case 16:
            typedesc = TypeDesc::UINT16;
            break;
        case 32:
            typedesc = TypeDesc::UINT32;
            break;
    };
    spec = ImageSpec (m_header.width, m_header.height, m_header.channel_count, typedesc);
    spec.channelnames.clear ();
    switch (m_header.color_mode) {
        case ColorMode_Bitmap :
            spec.channelnames.push_back ("A");
            break;
        case ColorMode_Grayscale :
            spec.channelnames.push_back ("I");
            break;
        case ColorMode_Indexed :
            spec.channelnames.push_back ("I");
            break;
        case ColorMode_RGB :
            spec.channelnames.push_back ("R");
            spec.channelnames.push_back ("G");
            spec.channelnames.push_back ("B");
            break;
        case ColorMode_CMYK :
            spec.channelnames.push_back ("C");
            spec.channelnames.push_back ("M");
            spec.channelnames.push_back ("Y");
            spec.channelnames.push_back ("K");
            break;
        case ColorMode_Lab :
            spec.channelnames.push_back ("L");
            spec.channelnames.push_back ("a");
            spec.channelnames.push_back ("b");
            break;
    };
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
    if (!m_file) {
        error ("[File Header] I/O error");
        return false;
    }
    return true;
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
        case ColorMode_Bitmap :
        case ColorMode_Grayscale :
        case ColorMode_Indexed :
        case ColorMode_RGB :
        case ColorMode_CMYK :
        case ColorMode_Multichannel :
        case ColorMode_Duotone :
        case ColorMode_Lab :
            break;
        default:
            error ("[Header] unrecognized color mode");
            return false;
    }
    return true;
}



bool
PSDInput::load_color_data (ImageSpec &spec)
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
    if (!m_file) {
        error ("[Color Mode Data] I/O error");
        return false;
    }
    return true;
}



bool
PSDInput::validate_color_data ()
{
    if (m_header.color_mode == ColorMode_Duotone && m_color_data.length == 0) {
        error ("[Color Mode Data] color mode data should be present for duotone image");
        return false;
    }
    if (m_header.color_mode == ColorMode_Indexed && m_color_data.length != 768) {
        return ("[Color Mode Data] length should be 768 for indexed color mode");
        return false;
    }
    return true;
}



bool
PSDInput::load_resources (ImageSpec &spec)
{
    //Length of Image Resources section
    uint32_t length;
    if (!read_bige<uint32_t> (length)) {
        error ("[Image Resources Section] I/O error");
        return false;
    }
    ImageResourceBlock block;
    ImageResourceMap resources;
    std::streampos section_start = m_file.tellg();
    std::streampos section_end = section_start + (std::streampos)length;
    while (m_file && m_file.tellg () < section_end) {
        if (!read_resource (block) || !validate_resource (block))
            return false;

        resources[block.id] = block;
    }
    if (!m_file) {
        error ("[Image Resources Section] I/O error");
        return false;
    }
    const ImageResourceMap::const_iterator end (resources.end ());
    BOOST_FOREACH (const ResourceLoader &loader, resource_loaders) {
        ImageResourceMap::const_iterator it (resources.find (loader.resource_id));
        //If we have an ImageResourceLoader for that resource ID, call it
        if (it != end) {
            m_file.seekg (it->second.pos);
            loader.load (this, it->second.length, spec);
        }
    }
    m_file.seekg (section_end);
    return true;
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

    if (!m_file) {
        error ("[Image Resource] I/O error");
        return false;
    }
    return true;
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
PSDInput::load_resource_1033 (uint32_t length, ImageSpec &spec)
{
    return load_resource_thumbnail (length, spec, true);
}



bool
PSDInput::load_resource_1036 (uint32_t length, ImageSpec &spec)
{
    return load_resource_thumbnail (length, spec, false);
}



bool
PSDInput::load_resource_thumbnail (uint32_t length, ImageSpec &spec, bool isBGR)
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
    //jpeg_destroy_decompress will deallocate this
    JSAMPLE **buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, stride, 1);
    while (cinfo.output_scanline < cinfo.output_height) {
        if (jpeg_read_scanlines (&cinfo, buffer, 1) != 1) {
            jpeg_finish_decompress (&cinfo);
            jpeg_destroy_decompress (&cinfo);
            return false;
        }
        if (isBGR) {
            //TODO BGR->RGB
        }
        //TODO fill thumbnail_image attribute
    }
    jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);
    spec.attribute ("thumbnail_width", (int)width);
    spec.attribute ("thumbnail_height", (int)height);
    spec.attribute ("thumbnail_nchannels", 3);
    return true;
}



bool
PSDInput::load_resource_1005 (uint32_t length, ImageSpec &spec)
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
    if (!(resinfo.hResUnit == resinfo.vResUnit
          && (resinfo.hResUnit == ResolutionInfo::PixelsPerInch
          || resinfo.hResUnit == ResolutionInfo::PixelsPerCentimeter))) {
          error ("[Image Resource] [ResolutionInfo] Unrecognized resolution unit");
          return false;
    }
    spec.attribute ("XResolution", resinfo.hRes);
    spec.attribute ("YResolution", resinfo.vRes);
    switch (resinfo.hResUnit) {
        case ResolutionInfo::PixelsPerInch:
            spec.attribute ("ResolutionUnit", "in");
            break;
        case ResolutionInfo::PixelsPerCentimeter:
            spec.attribute ("ResolutionUnit", "cm");
            break;
    };
    return true;
}



bool
PSDInput::load_resource_1006 (uint32_t length, ImageSpec &spec)
{
    int32_t bytes_remaining = length;
    std::string name;
    while (m_file && bytes_remaining >= 2) {
        bytes_remaining -= read_pascal_string (name, 1);
        spec.channelnames.push_back (name);
    }
    return true;
}



void
PSDInput::thumbnail_error_exit (j_common_ptr cinfo)
{
    thumbnail_error_mgr *mgr = (thumbnail_error_mgr *)cinfo->err;
    //nothing here so far

    longjmp (mgr->setjmp_buffer, 1);
}



bool
PSDInput::load_layers (ImageSpec &spec)
{
    if (m_header.version == 1)
        read_bige<uint32_t> (m_layer_mask_info.length);
    else
        read_bige<uint64_t> (m_layer_mask_info.length);

    m_layer_mask_info.pos = m_file.tellg ();
    if (!m_file) {
        error ("[Layer Mask Info] I/O error");
        return false;
    }
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
    //FIXME we will need to save this elsewhere
    bool transparency = false;
    if (layer_info.layer_count < 0) {
        transparency = true;
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

    layer.channel_info.resize (layer.channel_count);
    for(uint16_t channel = 0; channel < layer.channel_count; channel++) {
        Layer::ChannelInfo &channel_info = layer.channel_info[channel];
        read_bige<int16_t> (channel_info.channel_id);
        if (m_header.version == 1)
            read_bige<uint32_t> (channel_info.data_length);
        else
            read_bige<uint64_t> (channel_info.data_length);
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
    return true;
}



bool
PSDInput::load_layer_image (Layer &layer)
{
    for (uint16_t channel = 0; channel < layer.channel_count; ++channel) {
        Layer::ChannelInfo &channel_info = layer.channel_info[channel];
        if (!load_layer_channel (layer, channel_info))
            return false;
    }
    return true;
}



bool
PSDInput::load_layer_channel (Layer &layer, Layer::ChannelInfo &channel_info)
{
    std::streampos start_pos = m_file.tellg ();
    if (channel_info.data_length >= 2) {
        if (!read_bige<uint16_t> (channel_info.compression))
            return false;
    }
    //No data at all or just compression
    if (channel_info.data_length <= 2)
        return true;

    switch (channel_info.compression) {
        case Compression_Raw:
            //TODO
            return false;
            break;
        case Compression_RLE:
            if (!read_rle_lengths (layer, channel_info))
                return false;
            break;
        case Compression_ZIP:
            //TODO
            return false;
            break;
        case Compression_ZIP_Predict:
            //TODO
            return false;
            break;
    }
    if (!m_file)
        return false;

    channel_info.data_pos = m_file.tellg ();
    //FIXME check this over
    channel_info.data_length = channel_info.data_length - (channel_info.data_pos - start_pos);
    m_file.seekg (channel_info.data_length, std::ios::cur);
    return true;

}



bool
PSDInput::read_rle_lengths (Layer &layer, Layer::ChannelInfo &channel_info)
{
    std::vector<uint32_t> &rle_lengths = channel_info.rle_lengths;
    uint32_t row_count = (layer.bottom - layer.top);
    rle_lengths.resize (row_count);
    for (uint32_t row = 0; row < row_count; ++row) {
        if (m_header.version == 1)
            read_bige<uint16_t> (rle_lengths[row]);
        else
            read_bige<uint32_t> (rle_lengths[row]);
    }
    return true;
}



bool
PSDInput::load_global_mask_info (ImageSpec &spec)
{
    m_file.seekg (m_layer_mask_info.layer_info.pos + (std::streampos)m_layer_mask_info.layer_info.length);
    uint32_t length;
    read_bige<uint32_t> (length);
    std::streampos start = m_file.tellg ();
    std::streampos end = start + (std::streampos)length;
    if (!m_file)
        return false;

    if (!length)
        return true;

    read_bige<uint16_t> (m_global_mask_info.overlay_color_space);
    for (int i = 0; i < 4; ++i)
        read_bige<uint16_t> (m_global_mask_info.color_components[i]);

    read_bige<uint16_t> (m_global_mask_info.opacity);
    read_bige<int16_t> (m_global_mask_info.kind);
    m_file.seekg (end);
    if (!m_file)
        return false;

    return true;
}



bool
PSDInput::load_global_additional (ImageSpec &spec)
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
        if (!m_file)
            return false;
    }
    if (!m_file)
        return false;

    return true;
}



bool
PSDInput::load_merged_image (ImageSpec &spec)
{
    return true;
}



bool
PSDInput::is_additional_info_psb (const char *key)
{
    for (std::size_t i = 0; i < additional_info_psb_count; ++i)
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



OIIO_PLUGIN_NAMESPACE_END


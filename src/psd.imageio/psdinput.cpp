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

#include "psd_pvt.h"
#include <fstream>
#include <vector>
#include <map>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
#include "jpeg_memory_src.h"
#include <setjmp.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace psd_pvt;

class PSDInput : public ImageInput {
public:
    PSDInput ();
    virtual ~PSDInput () { close(); }
    virtual const char * format_name (void) const { return "psd"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool open (const std::string &name, ImageSpec &newspec, const ImageSpec &config);
    virtual bool close ();
	virtual int current_subimage () const { return m_subimage; }
	virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);

private:
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

    //Image resource loaders to handle loading certain image resources into ImageSpec
    struct ResourceLoader {
        uint16_t resource_id;
        boost::function<bool (PSDInput *, uint32_t)> load;
    };

    //Map image resource ID to image resource block
    typedef std::map<uint16_t, ImageResourceBlock> ImageResourceMap;

    struct ImageResourcesSection {
        uint32_t length;
        ImageResourceMap resources;
    };

    struct ResolutionInfo {
        float hRes;
        int16_t hResUnit;
        int16_t widthUnit;
        float vRes;
        int16_t vResUnit;
        int16_t heightUnit;

        enum ResolutionUnit {
            PixelsPerInch = 1,
            PixelsPerCentimeter = 2
        };

        enum Unit {
            Inches = 1,
            Centimeters = 2,
            Points = 3,
            Picas = 4,
            Columns = 5
        };

    };

    std::string m_filename;
    std::ifstream m_file;
    //Current subimage
	int m_subimage;
	//Subimage count (1 + layer count)
	int m_subimage_count;
	std::vector<ImageSpec> m_specs;
    static const ResourceLoader resource_loaders[];

	FileHeader m_header;
	ColorModeData m_color_data;
    ImageResourcesSection m_image_resources;

    //Reset to initial state
    void init ();

    //File Header
    bool load_header ();
    bool read_header ();
    bool validate_header ();

    //Color Mode Data
    bool load_color_data ();
    bool validate_color_data ();

    //Image Resources
    bool load_resources ();
    bool read_resource (ImageResourceBlock &block);
    bool validate_resource (ImageResourceBlock &block);
    //Call the resource_loaders to load the resources into an ImageSpec
    //m_specs should be resized to m_subimage_count first
    bool handle_resources ();
    //Pixel Aspect Ratio
    bool load_resource_1064 (uint32_t length);
    //ResolutionInfo
    bool load_resource_1005 (uint32_t length);
    //JPEG thumbnail
    bool load_resource_1033 (uint32_t length);
    bool load_resource_1036 (uint32_t length);
    bool load_resource_thumbnail (uint32_t length, bool isBGR);
    //For thumbnail loading
    struct thumbnail_error_mgr {
        jpeg_error_mgr pub;
        jmp_buf setjmp_buffer;
    };
    METHODDEF (void)
    thumbnail_error_exit (j_common_ptr cinfo);

    //Check if m_file is good. If not, set error message and return false.
    bool check_io ();

    //This may be a bit inefficient but I think it's worth the convenience.
    //This takes care of things like reading a 32-bit BE into a 64-bit LE.
    template <typename TStorage, typename TVariable>
    bool read_bige (TVariable &value)
    {
        TStorage buffer;
        m_file.read ((char *)&buffer, sizeof(buffer));
        if (!bigendian ())
            swap_endian (&buffer);

        //For debugging, numeric_cast will throw if precision is lost:
        //value = boost::numeric_cast<TVariable>(buffer);
        value = buffer;
        return m_file;
    }

    int read_pascal_string (std::string &s, uint16_t mod_padding);

    //Some attributes may apply to only the merged composite.
    //Others may apply to all subimages.
    //These functions are intended to be used by image resource loaders.
    //
    //Add an attribute to the composite image spec
    template <typename T>
    void composite_attribute (const std::string &name, const T &value)
    {
        m_specs[0].attribute (name, value);
    }

    //Add an attribute to the composite image spec
    template <typename T>
    void composite_attribute (const std::string &name, const TypeDesc &type, const T &value)
    {
        m_specs[0].attribute (name, type, value);
    }

    //Add an attribute to the composite image spec and common image spec
    template <typename T>
    void common_attribute (const std::string &name, const T &value)
    {
        BOOST_FOREACH (ImageSpec &spec, m_specs)
            spec.attribute (name, value);
    }

    //Add an attribute to the composite image spec and common image spec
    template <typename T>
    void common_attribute (const std::string &name, const TypeDesc &type, const T &value)
    {
        BOOST_FOREACH (ImageSpec &spec, m_specs)
            spec.attribute (name, type, value);
    }

};

// Image resource loaders
// To add an image resource loader, do the following:
// 1) Add ADD_LOADER(<ResourceID>) below
// 2) Add a method in PSDInput:
//    bool load_resource_<ResourceID> (uint32_t length);
#define ADD_LOADER(id) {id, boost::bind (&PSDInput::load_resource_##id, _1, _2)}
const PSDInput::ResourceLoader PSDInput::resource_loaders[] =
{
    ADD_LOADER(1064),
    ADD_LOADER(1005),
    ADD_LOADER(1033),
    ADD_LOADER(1036)
};
#undef ADD_LOADER

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

    return false;
}



bool
PSDInput::open (const std::string &name, ImageSpec &newspec, const ImageSpec &config)
{
    return open (name, newspec);
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
	return false;
}



bool
PSDInput::read_native_scanline (int y, int z, void *data)
{
    return false;
}



void
PSDInput::init ()
{
    m_filename.clear ();
    m_file.close ();
	m_subimage = -1;
	m_subimage_count = 0;
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
    m_file.seekg(6, std::ios::cur);
    read_bige<uint16_t> (m_header.channel_count);
    read_bige<uint32_t> (m_header.width);
    read_bige<uint32_t> (m_header.height);
    read_bige<uint16_t> (m_header.depth);
    read_bige<uint16_t> (m_header.color_mode);
    if (!check_io ())
        return false;

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
            //FIXME For testing purposes, we'll pretend we support these
            //error ("[Header] unsupported color mode");
            return true;
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
    read_bige<uint32_t> (m_color_data.length);
    if (!check_io ())
        return false;

    if (!validate_color_data ())
        return false;

    m_color_data.data.resize (m_color_data.length);
    m_file.read (&m_color_data.data[0], m_color_data.length);
    if (!check_io ())
        return false;

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
        error ("[Color Mode Data] length should be 768 for indexed color mode");
        return false;
    }
    return true;
}



bool
PSDInput::load_resources ()
{
    read_bige<uint32_t> (m_image_resources.length);

    if (!check_io ())
        return false;

    ImageResourceBlock block;
    ImageResourceMap &resources = m_image_resources.resources;
    std::streampos begin = m_file.tellg ();
    std::streampos end = begin + (std::streampos)m_image_resources.length;
    while (m_file && m_file.tellg () < end) {
        if (!read_resource (block) || !validate_resource (block))
            return false;

        resources.insert (std::make_pair (block.id, block));
    }
    m_file.seekg (end);
    if (!check_io ())
        return false;

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
    //Image resource blocks are supposed to be padded to an even size.
    //I'm not sure if the padding is included in the length field
    if (block.length % 2 != 0)
        m_file.seekg(1, std::ios::cur);

    if (!check_io ())
        return false;

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
PSDInput::handle_resources ()
{
    ImageResourceMap &resources = m_image_resources.resources;
    //Loop through each of our resource loaders
    const ImageResourceMap::const_iterator end (resources.end ());
    BOOST_FOREACH (const ResourceLoader &loader, resource_loaders) {
        ImageResourceMap::const_iterator it (resources.find (loader.resource_id));
        //If a resource with that ID exists in the file, call the loader
        if (it != end) {
            m_file.seekg (it->second.pos);
            if (!check_io ())
                return false;

            loader.load (this, it->second.length);
            if (!check_io ())
                return false;
        }
    }
    return true;
}



bool
PSDInput::load_resource_1064 (uint32_t length)
{
    uint32_t version;
    if (!read_bige<uint32_t> (version))
        return false;

    if (version != 1 && version != 2) {
        error ("[Image Resource] [Pixel Aspect Ratio] Unrecognized version");
        return false;
    }
    double aspect_ratio;
    if (!read_bige<double> (aspect_ratio))
        return false;

    //FIXME(dewyatt): loss of precision?
    common_attribute ("PixelAspectRatio", (float)aspect_ratio);
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

    //Make sure the same unit is used both horizontally and vertically
    //FIXME(dewyatt): I don't know for sure that the unit can differ. However,
    //if it can, perhaps we should be using ResolutionUnitH/ResolutionUnitV or
    //something similar.
    if (resinfo.hResUnit != resinfo.vResUnit) {
        error ("[Image Resource] [ResolutionInfo] Resolutions must have the same unit");
        return false;
    }
    //Make sure the unit is supported
    //Note: This relies on the above check that the units are the same.
    if (resinfo.hResUnit != ResolutionInfo::PixelsPerInch
        && resinfo.hResUnit != ResolutionInfo::PixelsPerCentimeter) {
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
    enum ThumbnailFormat {
        kJpegRGB = 1,
        kRawRGB = 0
    };

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

    //We only support kJpegRGB since I don't have any test images with kRawRGB
    if (format != kJpegRGB || bpp != 24 || planes != 1) {
        error ("[Image Resource] [JPEG Thumbnail] invalid or unsupported format");
        return false;
    }

    cinfo.err = jpeg_std_error (&jerr.pub);
    jerr.pub.error_exit = thumbnail_error_exit;
    if (setjmp (jerr.setjmp_buffer)) {
        jpeg_destroy_decompress (&cinfo);
        error ("[Image Resource] [JPEG Thumbnail] libjpeg error");
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
            error ("[Image Resource] [JPEG Thumbnail] libjpeg error");
            return false;
        }
        std::memcpy (&thumbnail_image[(cinfo.output_scanline - 1) * stride],
                    (char *)buffer[0],
                    stride);
    }
    jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);
    //Set these attributes for the merged composite only (subimage 0)
    composite_attribute ("thumbnail_width", (int)width);
    composite_attribute ("thumbnail_height", (int)height);
    composite_attribute ("thumbnail_nchannels", 3);
    if (isBGR) {
        for (unsigned int i = 0; i < thumbnail_bytes - 2; i += 3)
            std::swap (thumbnail_image[i], thumbnail_image[i + 2]);
    }
    composite_attribute ("thumbnail_image",
                   TypeDesc (TypeDesc::UINT8, thumbnail_image.size ()),
                   &thumbnail_image[0]);
    return true;
}



void
PSDInput::thumbnail_error_exit (j_common_ptr cinfo)
{
    thumbnail_error_mgr *mgr = (thumbnail_error_mgr *)cinfo->err;
    longjmp (mgr->setjmp_buffer, 1);
}



bool
PSDInput::check_io ()
{
    if (!m_file) {
        error ("\"%s\": I/O error", m_filename.c_str ());
        return false;
    }
    return true;
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


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
    std::string m_filename;           ///< Stash the filename
    std::ifstream m_file;             ///< Open image handle
	int m_subimage;
	int m_subimage_count;
	FileHeader m_header;
	ColorModeData m_color_data;
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

    //For thumbnail loading
    struct thumbnail_error_mgr {
        jpeg_error_mgr pub;
        jmp_buf setjmp_buffer;
    };

    METHODDEF (void)
    thumbnail_error_exit (j_common_ptr cinfo);

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
    ADD_LOADER(1005)
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



void
PSDInput::thumbnail_error_exit (j_common_ptr cinfo)
{
    thumbnail_error_mgr *mgr = (thumbnail_error_mgr *)cinfo->err;
    //nothing here so far

    longjmp (mgr->setjmp_buffer, 1);
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


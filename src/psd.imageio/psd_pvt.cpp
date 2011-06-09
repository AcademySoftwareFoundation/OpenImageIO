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
#include <boost/foreach.hpp>

#include <setjmp.h>
#include "jpeg_memory_src.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace OIIO = OIIO_NAMESPACE;

namespace psd_pvt {

//A couple read/write template functions
//These are here for a few reasons:
//1 - Eliminate repetitive (char *) casting for iostreams
//2 - Take care of endian swapping automatically
//3 - Deal with issues like reading a 32bitBE into a 64bitLE
//
//TStorage - type stored in stream (on disk)
//TVariable - type of variable we are reading into to
template <typename TStorage, typename TVariable>
std::istream &read_bige (std::istream &is, TVariable &value)
{
    TStorage buffer;
    is.read ((char *)&buffer, sizeof(buffer));
    if (!bigendian ())
        swap_endian (&buffer);

    //value = boost::numeric_cast<TVariable>(buffer);
    value = buffer;
    return is;
}



//TStorage - type to store in stream (on disk)
//TVariable - type of variable data is stored in
template <typename TStorage, typename TVariable>
std::istream &write_bige (std::ostream &os, TVariable &value)
{
    //TStorage buffer = boost::numeric_cast<TStorage>(value);
    TStorage buffer = value;
    if (!bigendian ())
        swap_endian (&buffer);

    os.write ((char *)&buffer, sizeof(buffer));
    return os;
}



int
read_pascal_string (std::ifstream &inf, std::string &s, uint16_t mod_padding)
{
    s.clear();
    uint8_t length;
    int bytes = 0;
    if (inf.read ((char *)&length, 1)) {
        bytes = 1;
        if (length == 0) {
            if (inf.seekg (mod_padding - 1, std::ios::cur))
                bytes += mod_padding - 1;
        } else {
            s.resize (length);
            if (inf.read (&s[0], length)) {
                bytes += length;
                if (mod_padding > 0) {
                    for (int padded_length = length + 1; padded_length % mod_padding != 0; padded_length++) {
                        if (!inf.seekg(1, std::ios::cur))
                            break;

                        bytes++;
                    }
                }
            }
        }
    }
    return bytes;
}



bool
decompress_packbits (const char *src, char *dst, uint16_t packed_length, uint16_t unpacked_length)
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



std::string
PSDFileHeader::read (std::ifstream &inf)
{
    inf.read (signature, 4);
    read_bige<uint16_t> (inf, version);
    // skip reserved bytes
    inf.seekg (6, std::ios::cur);
    read_bige<uint16_t> (inf, channels);
    read_bige<uint32_t> (inf, height);
    read_bige<uint32_t> (inf, width);
    read_bige<uint16_t> (inf, depth);
    read_bige<uint16_t> (inf, color_mode);
    if (!inf)
        return "read error";

    return validate();
}



std::string
PSDFileHeader::validate () const
{
    if (std::memcmp (signature, "8BPS", 4) != 0)
        return "[header] invalid signature";

    if (version != 1 && version != 2)
        return "[header] invalid version";

    if (channels < 1 || channels > 56)
        return "[header] invalid channel count";

    if (height < 1 || ((version == 1 && height > 30000) || (version == 2 && height > 300000)))
        return "[header] invalid image height";

    if (width < 1 || ((version == 1 && width > 30000) || (version == 2 && width > 300000)))
        return "[header] invalid image width";

    if (depth != 1 && depth != 8 && depth != 16 && depth != 32)
        return "[header] invalid depth";

    switch (color_mode) {
        case COLOR_MODE_BITMAP :
        case COLOR_MODE_GRAYSCALE :
        case COLOR_MODE_INDEXED :
        case COLOR_MODE_RGB :
        case COLOR_MODE_CMYK :
        case COLOR_MODE_MULTICHANNEL :
        case COLOR_MODE_DUOTONE :
        case COLOR_MODE_LAB :
            break;
        default:
            return "[header] invalid color mode";
    }
    return "";
}



PSDColorModeData::PSDColorModeData (const PSDFileHeader &header) : m_header (header)
{
}



std::string
PSDColorModeData::read (std::ifstream &inf)
{
    read_bige<uint32_t> (inf, length);
    pos = inf.tellg();
    inf.seekg (pos, std::ios::cur);
    if (!inf)
        return "read error";

    return validate();
}



std::string
PSDColorModeData::validate () const
{
    if (m_header.color_mode == COLOR_MODE_DUOTONE && length == 0)
        return "[color mode data] color mode data should be present for duotone image";

    if (m_header.color_mode == COLOR_MODE_INDEXED && length != 768)
        return "[color mode data] length should be 768 for indexed color mode";

    return "";
}



std::string
PSDImageResourceBlock::read (std::ifstream &inf)
{
    inf.read (signature, 4);
    read_bige<uint16_t> (inf, id);
    read_pascal_string (inf, name, 2);
    read_bige<uint32_t> (inf, length);
    pos = inf.tellg();
    inf.seekg (length, std::ios::cur);
    // image resource blocks are padded to an even size, so skip padding
    if (length % 2 != 0)
        inf.seekg(1, std::ios::cur);

    if (!inf)
        return "read error";

    return validate ();
}



std::string
PSDImageResourceBlock::validate () const
{
    if (std::memcmp (signature, "8BIM", 4) != 0)
        return "[image resource block] invalid signature";

    return "";
}



std::string
PSDImageResourceSection::read (std::ifstream &inf)
{
    resources.clear();
    if (read_bige<uint32_t> (inf, length)) {
        std::streampos section_start = inf.tellg();
        std::streampos section_end = section_start + (std::streampos)length;
        PSDImageResourceBlock block;
        std::string err;
        while (inf && inf.tellg () < section_end) {
            err = block.read (inf);
            if (!err.empty ())
                return err;

            resources[block.id] = block;
        }
    }
    if (!inf)
        return "read error";

    return "";
}



struct thumbnail_error_mgr {
    jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};



METHODDEF (void)
thumbnail_error_exit (j_common_ptr cinfo)
{
    thumbnail_error_mgr *mgr = (thumbnail_error_mgr *)cinfo->err;
    //nothing here so far

    longjmp (mgr->setjmp_buffer, 1);
}



bool load_resource_1036 (std::ifstream &inf, const PSDImageResourceBlock &resource, ImageSpec &spec)
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
    uint32_t jpeg_length = resource.length - 28;
    std::string data (jpeg_length, 0);

    inf.seekg (resource.pos);
    read_bige<uint32_t> (inf, format);
    read_bige<uint32_t> (inf, width);
    read_bige<uint32_t> (inf, height);
    read_bige<uint32_t> (inf, widthbytes);
    read_bige<uint32_t> (inf, total_size);
    read_bige<uint32_t> (inf, compressed_size);
    read_bige<uint16_t> (inf, bpp);
    read_bige<uint16_t> (inf, planes);
    inf.read (&data[0], jpeg_length);
    spec.attribute ("thumbnail_width", (int)width);
    spec.attribute ("thumbnail_height", (int)height);
    spec.attribute ("thumbnail_nchannels", 3);
    if (!inf)
        return false;

    if (format != kJpegRGB || bpp != 24 || planes != 1)
        return false;

    cinfo.err = jpeg_std_error (&jerr.pub);
    jerr.pub.error_exit = thumbnail_error_exit;
    if (setjmp (jerr.setjmp_buffer)) {
        jpeg_destroy_decompress (&cinfo);
        return false;
    }
    jpeg_create_decompress (&cinfo);
    jpeg_memory_src (&cinfo, (unsigned char *)&data[0], jpeg_length);
    jpeg_read_header (&cinfo, TRUE);
    jpeg_start_decompress (&cinfo);
    stride = cinfo.output_width * cinfo.output_components;
    JSAMPLE **buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, stride, 1);
    while (cinfo.output_scanline < cinfo.output_height) {
        if (jpeg_read_scanlines (&cinfo, buffer, 1) != 1) {
            jpeg_finish_decompress (&cinfo);
            jpeg_destroy_decompress (&cinfo);
            return false;
        }
        if (resource.id == Resource_Thumbnail_V4) {
            //TODO BGR->RGB
        }
        //TODO fill thumbnail_image attribute
    }
    jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);
    return true;
}



bool load_resource_1033 (std::ifstream &inf, const PSDImageResourceBlock &resource, ImageSpec &spec)
{
    return load_resource_1036 (inf, resource, spec);
}



#define ADD_HANDLER(id) {id, load_resource_##id}
const ImageResourceHandler resource_handlers[] =
{
    ADD_HANDLER(1033),
    ADD_HANDLER(1036)
};
#undef ADD_HANDLER

const std::size_t resource_handlers_count = sizeof(resource_handlers) / sizeof(resource_handlers[0]);



} // psd_pvt namespace


OIIO_PLUGIN_NAMESPACE_END


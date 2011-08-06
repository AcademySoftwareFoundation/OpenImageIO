/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;

#include "dds_pvt.h"
#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

#include "squish/squish.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace DDS_pvt;

class DDSOutput : public ImageOutput {
public:
    DDSOutput ();
    virtual ~DDSOutput ();
    virtual const char * format_name (void) const { return "dds"; }
    virtual bool supports (const std::string &feature) const {
        return iequals (feature, "mipmap") || iequals (feature, "volumes")
            // FIXME: reenable when we figure out how to solve the cube map conflict!
            /*|| iequals (feature, "tiles")*/;
    }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z,
                             TypeDesc format, const void *data,
                             stride_t xstride, stride_t ystride, stride_t zstride);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    std::vector<unsigned char> m_scratch;
    dds_header m_dds;
    long m_startofs;                  ///< File offset to MIP level start
    bool m_1x6;                       ///< If true, it's a 1x6 layout cube map

    // Initialize private members to pre-opened state
    void init (void) {
        m_file = NULL;
    }
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageOutput *dds_output_imageio_create () { return new DDSOutput; }

// DLLEXPORT int dds_imageio_version = OIIO_PLUGIN_VERSION;   // it's in tgainput.cpp

DLLEXPORT const char * dds_output_extensions[] = {
    "dds", NULL
};

OIIO_PLUGIN_EXPORTS_END



DDSOutput::DDSOutput ()
{
    init ();
}



DDSOutput::~DDSOutput ()
{
    // Close, if not already done.
    close ();
}



bool
DDSOutput::open (const std::string &name, const ImageSpec &userspec,
                 OpenMode mode)
{
    if (mode == Create)
        close ();  // Close any already-opened file

    m_spec = userspec;  // Stash the spec

    switch (mode) {
        case Create:
        {
            m_file = fopen (name.c_str(), "wb");
            if (! m_file) {
                error ("Could not open file \"%s\"", name.c_str());
                return false;
            }
            
            // set up the DDS header struct
            memset (&m_dds, 0, sizeof (m_dds));
            m_dds.fourCC = DDS_MAKE4CC('D', 'D', 'S', ' ');
            m_dds.size = 124;
            m_dds.pitch = m_spec.scanline_bytes (true);
            m_dds.fmt.size = 32;
            m_dds.flags |= DDS_CAPS | DDS_PIXELFORMAT | DDS_WIDTH | DDS_HEIGHT
                | DDS_PITCH;
            m_dds.caps.flags1 |= DDS_CAPS1_TEXTURE;
            
            m_dds.fmt.bpp = m_spec.get_int_attribute ("oiio:BitsPerSample",
                m_spec.format.size() * 8) * m_spec.nchannels;
            
            // FIXME: this mask determination algo is not very portable, it'll
            // give wrong results on big-endian machines
            switch (m_spec.nchannels) {
                case 1:
                    m_dds.fmt.flags |= DDS_PF_LUMINANCE;
                    break;
                case 2:
                    m_dds.fmt.flags |= DDS_PF_LUMINANCE | DDS_PF_ALPHA;
                    m_dds.fmt.amask = (1 << (m_dds.fmt.bpp / 2)) - 1;
                    break;
                case 3:
                    m_dds.fmt.flags |= DDS_PF_RGB;
                    m_dds.fmt.rmask = (1 << (m_dds.fmt.bpp / 3)) - 1;
                    m_dds.fmt.gmask = m_dds.fmt.rmask << (m_dds.fmt.bpp / 3);
                    m_dds.fmt.bmask = m_dds.fmt.gmask << (m_dds.fmt.bpp / 3);
                    break;
                case 4:
                    m_dds.fmt.flags |= DDS_PF_RGB | DDS_PF_ALPHA;
                    m_dds.fmt.rmask = (1 << (m_dds.fmt.bpp / 4)) - 1;
                    m_dds.fmt.gmask = m_dds.fmt.rmask << (m_dds.fmt.bpp / 4);
                    m_dds.fmt.bmask = m_dds.fmt.gmask << (m_dds.fmt.bpp / 4);
                    m_dds.fmt.amask = m_dds.fmt.bmask << (m_dds.fmt.bpp / 4);
                    break;
                default:
                    error ("Unsupported number of channels: %d", m_spec.nchannels);
                    return false;
            }
            
            std::string textype = m_spec.get_string_attribute ("texturetype", "");
            std::string texfmt = m_spec.get_string_attribute ("textureformat", "");
            if (iequals (textype, "Volume Texture")
                || iequals (texfmt, "Volume Texture")
                || m_spec.depth > 1) {
                m_dds.caps.flags1 |= DDS_CAPS1_COMPLEX;
                m_dds.caps.flags2 |= DDS_CAPS2_VOLUME;
                m_dds.flags |= DDS_DEPTH;
                m_dds.width = m_spec.width;
                m_dds.height = m_spec.height;
                m_dds.depth = m_spec.depth;
                m_startofs = ftell (m_file);
            } else if (iequals (textype, "Environment")
                || iequals (texfmt, "CubeFace Environment")) {
                m_dds.caps.flags1 |= DDS_CAPS1_COMPLEX;
                m_dds.caps.flags2 |= DDS_CAPS2_CUBEMAP;
                m_dds.width = m_spec.full_width;
                m_dds.height = m_spec.full_height;
                
                // find out which cube faces we have here
                texfmt = m_spec.get_string_attribute ("dds:CubeSides", "");
                char s = 127; // start out in state of error
                if (texfmt.length ()) {
                    s = 0;      // clear error state and parse the string
                    for (std::string::iterator it = texfmt.begin ();
                        it != texfmt.end (); ++it) {
                        if (*it == ' ')
                            continue;
                        switch (s) {
                            case 0:
                                if (*it == '+')
                                    s = 1;
                                else if (*it == '-')
                                    s = -1;
                                else
                                    s = 127; // error
                                break;
                            case 1:
                                if (*it == 'x' || *it == 'X') {
                                    m_dds.caps.flags2 |= DDS_CAPS2_CUBEMAP_POSITIVEX;
                                    s = 0;
                                } else if (*it == 'y' || *it == 'Y') {
                                    m_dds.caps.flags2 |= DDS_CAPS2_CUBEMAP_POSITIVEY;
                                    s = 0;
                                } else if (*it == 'z' || *it == 'Z') {
                                    m_dds.caps.flags2 |= DDS_CAPS2_CUBEMAP_POSITIVEZ;
                                    s = 0;
                                } else
                                    s = 127; // error
                                break;
                            case -1:
                                if (*it == 'x' || *it == 'X') {
                                    m_dds.caps.flags2 |= DDS_CAPS2_CUBEMAP_NEGATIVEX;
                                    s = 0;
                                } else if (*it == 'y' || *it == 'Y') {
                                    m_dds.caps.flags2 |= DDS_CAPS2_CUBEMAP_NEGATIVEY;
                                    s = 0;
                                } else if (*it == 'z' || *it == 'Z') {
                                    m_dds.caps.flags2 |= DDS_CAPS2_CUBEMAP_NEGATIVEZ;
                                    s = 0;
                                } else
                                    s = 127; // error
                                break;
                        }
                        if (s == 127) // error
                            break;
                    }
                    // we had a parse error, disregard the flags we found
                    if (s == 127)
                        m_dds.caps.flags2 &= ~(DDS_CAPS2_CUBEMAP_POSITIVEX
                            | DDS_CAPS2_CUBEMAP_POSITIVEY
                            | DDS_CAPS2_CUBEMAP_POSITIVEZ
                            | DDS_CAPS2_CUBEMAP_NEGATIVEX
                            | DDS_CAPS2_CUBEMAP_NEGATIVEY
                            | DDS_CAPS2_CUBEMAP_NEGATIVEZ);
                }
                // detect tile layout
                int htiles = m_spec.width / m_spec.full_width;
                int vtiles = m_spec.height / m_spec.full_height;
                // check for the 1x6 layout
                if (htiles == 1) {
                    if (vtiles > 6) {
                        error ("Invalid tile layout for cube map - 1x%d",
                                vtiles);
                        return false;
                    }
                    m_1x6 = true;
                // else check for the 3x2 layout
                } else if (htiles <= 3 && vtiles == 2)
                    m_1x6 = false;
                else {
                    error ("Invalid tile layout for cube map - %dx%d",
                            htiles, vtiles);
                    return false;
                }
                if (s == 127) {
                    // either side availability not specified by client or a
                    // parsing error, assume first x sides
                    for (int i = 0; i < htiles * vtiles; ++i)
                        m_dds.caps.flags2 |= DDS_CAPS2_CUBEMAP_POSITIVEX << i;
                }
            } else {    // plain texture
                m_dds.width = m_spec.width;
                m_dds.height = m_spec.height;
                m_startofs = ftell (m_file);
            }
            
            std::string cmp = m_spec.get_string_attribute ("compression", "");
            if (iequals (cmp, "DXT1")) {
                m_dds.fmt.flags |= DDS_PF_FOURCC;
                m_dds.fmt.fourCC = DDS_4CC_DXT1;
            } else if (iequals (cmp, "DXT2")) {
                m_dds.fmt.flags |= DDS_PF_FOURCC;
                m_dds.fmt.fourCC = DDS_4CC_DXT2;
            } else if (iequals (cmp, "DXT3")) {
                m_dds.fmt.flags |= DDS_PF_FOURCC;
                m_dds.fmt.fourCC = DDS_4CC_DXT3;
            } else if (iequals (cmp, "DXT4")) {
                m_dds.fmt.flags |= DDS_PF_FOURCC;
                m_dds.fmt.fourCC = DDS_4CC_DXT4;
            } else if (iequals (cmp, "DXT5")) {
                m_dds.fmt.flags |= DDS_PF_FOURCC;
                m_dds.fmt.fourCC = DDS_4CC_DXT5;
            }
            
            // skip the header (128 bytes) for now, we'll write it upon closing
            // we don't know everything until the image data comes
            int32_t zero = 0;
            for (uint i = 0; i < 128 / sizeof(zero); ++i)
                fwrite (&zero, sizeof(zero), 1, m_file);
            break;
        }
        case AppendMIPLevel:
            // if this is the first MIP level, mark it in the header
            if (++m_dds.mipmaps == 1) {
                // miplevel 1 is the original size image, so increase again
                ++m_dds.mipmaps;
                m_dds.flags |= DDS_MIPMAPCOUNT;
                m_dds.caps.flags1 |= DDS_CAPS1_COMPLEX | DDS_CAPS1_MIPMAP;
            }
            unsigned int w, h, d;
            // calculate new start offset
            if (!(m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP)) {
                dds_internal_seek (m_dds, m_file, 0, m_dds.mipmaps,
                                   m_spec.pixel_bytes (true), w, h, d);
                m_startofs = ftell (m_file);
            }
            // check if the mipmap isn't out of order
            w = m_dds.width >> (m_dds.mipmaps - 1);
            if (w < 1)
                w = 1;
            h = m_dds.height >> (m_dds.mipmaps - 1);
            if (h < 1)
                h = 1;
            d = m_dds.depth >> (m_dds.mipmaps - 1);
            if (d < 1)
                d = 1;
            if (m_spec.width != (int)w || m_spec.height != (int)h
                || m_spec.depth != (int)d) {
                error ("Out of order MIP map %dx%dx%d, expected %dx%dx%d",
                    m_spec.width, m_spec.height, m_spec.depth, w, h, d);
                return false;
            }
            break;
        case AppendSubimage:
            error ("%s does not support subimages", format_name());
            return false;
    }
    
    return true;
}



bool
DDSOutput::close ()
{
    if (m_file) {
        // dump header to file
        fseek (m_file, 0, SEEK_SET);
        if (bigendian()) {
            // DDS files are little-endian
            // only swap values which are not flags or bitmasks
            swap_endian (&m_dds.size);
            swap_endian (&m_dds.height);
            swap_endian (&m_dds.width);
            swap_endian (&m_dds.pitch);
            swap_endian (&m_dds.depth);
            swap_endian (&m_dds.mipmaps);

            swap_endian (&m_dds.fmt.size);
            swap_endian (&m_dds.fmt.bpp);
        }

// due to struct packing, we may get a corrupt header if we just load the
// struct from file; to adress that, read every member individually
// save some typing
#define WH(memb)  fwrite (&m_dds.memb, sizeof (m_dds.memb), 1, m_file)
        WH(fourCC);
        WH(size);
        WH(flags);
        WH(height);
        WH(width);
        WH(pitch);
        WH(depth);
        WH(mipmaps);

        // advance the file pointer by 44 bytes (reserved fields)
        fseek (m_file, 44, SEEK_CUR);

        // pixel format struct
        WH(fmt.size);
        WH(fmt.flags);
        WH(fmt.fourCC);
        WH(fmt.bpp);
        WH(fmt.rmask);
        WH(fmt.gmask);
        WH(fmt.bmask);
        WH(fmt.amask);

        // caps
        WH(caps.flags1);
        WH(caps.flags2);
#undef WH
        
        // close the stream
        fclose (m_file);
        m_file = NULL;
    }

    init ();      // re-initialize
    return true;  // How can we fail?
                  // Epicly. -- IneQuation
}



bool
DDSOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    // scanline writing is only used for non-cube maps, bail out if the client
    // attempts to do that for something else
    if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP) {
        error ("Cube maps in %s must be written as tiles", format_name ());
        return false;
    }
    
    m_spec.auto_stride (xstride, format, spec().nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
        data = &m_scratch[0];
    }
    
    fseek (m_file, m_startofs + z * m_dds.height * m_dds.pitch
        + y * m_dds.pitch, SEEK_SET);
    
    if (m_dds.fmt.flags & DDS_PF_FOURCC) {
        error ("Compression not yet supported");
        return false;
    } else      // uncompressed data
        fwrite (data, 1, m_dds.pitch, m_file);

    return true;
}



bool
DDSOutput::write_tile (int x, int y, int z,
                       TypeDesc format, const void *data,
                       stride_t xstride, stride_t ystride, stride_t zstride)
{
    // tile writing is only used for cube maps, bail out if the client
    // attempts to do that for something else
    if (!(m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP)) {
        error ("Only cube maps in %s may be written as tiles", format_name ());
        return false;
    }
    
    m_spec.auto_stride (xstride, ystride, zstride, format, spec().nchannels,
                        spec().tile_width, spec().tile_height);
    
    const void *origdata = data;   // Stash original pointer
    data = to_native_tile (format, data, xstride, ystride, zstride, m_scratch);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                            (unsigned char *)data + m_spec.tile_bytes());
        data = &m_scratch[0];
    }
    
    // figure out tile index
    int tx = x / m_spec.full_width;
    int ty = y / m_spec.full_height;
    int side;
    if (m_1x6)
        side = ty;
    else
        side = tx * 2 + ty;
    
    unsigned int w, h, d;
    dds_internal_seek (m_dds, m_file, side, m_dds.mipmaps,
                       m_spec.pixel_bytes (true), w, h, d);
    if (w * h * d > 0) {
        if (m_dds.fmt.flags & DDS_PF_FOURCC) {
            error ("Compression not yet supported");
            return false;
        } else      // uncompressed data
            fwrite (data, 1, m_spec.tile_bytes (true), m_file);
    }
    
    return false;
}

OIIO_PLUGIN_NAMESPACE_END


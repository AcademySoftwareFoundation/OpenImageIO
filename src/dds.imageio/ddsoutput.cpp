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
        // Support nothing nonstandard
        return false;
    }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    std::vector<unsigned char> m_scratch;
    dds_header m_dds;

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

    m_file = fopen (name.c_str(), "wb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    if (mode == Create) {
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
        
        m_dds.width = m_spec.width;
        m_dds.height = m_spec.height;
        
        std::string textype = m_spec.get_string_attribute ("texturetype", "");
        std::string texfmt = m_spec.get_string_attribute ("textureformat", "");
        if (iequals (textype, "Volume Texture")
            || iequals (texfmt, "Volume Texture")
            || m_spec.depth > 1) {
            m_dds.caps.flags1 |= DDS_CAPS1_COMPLEX;
            m_dds.caps.flags2 |= DDS_CAPS2_VOLUME;
            m_dds.flags |= DDS_DEPTH;
        } else if (iequals (textype, "Environment")
            || iequals (texfmt, "CubeFace Environment")) {
            m_dds.caps.flags1 |= DDS_CAPS1_COMPLEX;
            m_dds.caps.flags2 |= DDS_CAPS2_CUBEMAP;
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
    }
    
    // skip the header (128 bytes) for now, we'll write it upon closing as we
    // don't know everything until the image data is about to get written
    int32_t zero = 0;
    for (int i = 0; i < 32; ++i)
        fwrite (&zero, sizeof(zero), 1, m_file);
    
    
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
    m_spec.auto_stride (xstride, format, spec().nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
        data = &m_scratch[0];
    }
    
    if (m_dds.fmt.flags & DDS_PF_FOURCC) {
        error ("Compression not yet supported");
        return false;
    }
    
    fseek (m_file, 128 + y * m_dds.pitch, SEEK_SET);
    fwrite (data, 1, m_dds.pitch, m_file);

    return true;
}

OIIO_PLUGIN_NAMESPACE_END


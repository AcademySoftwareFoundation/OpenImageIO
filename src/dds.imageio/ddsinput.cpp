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

#include "dds_pvt.h"
using namespace DDS_pvt;

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "fmath.h"

#include "squish/squish.h"

using namespace OpenImageIO;



class DDSInput : public ImageInput {
public:
    DDSInput () { init(); }
    virtual ~DDSInput () { close(); }
    virtual const char * format_name (void) const { return "dds"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int index, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    std::vector<unsigned char> m_buf; ///< Buffer the image pixels
    int m_subimage;
    int m_nchans;                     ///< Number of colour channels in image
    int m_Bpp;                        ///< Number of bytes per pixel
    int m_redL, m_redR;               ///< Bit shifts to extract red channel
    int m_greenL, m_greenR;           ///< Bit shifts to extract green channel
    int m_blueL, m_blueR;             ///< Bit shifts to extract blue channel
    int m_alphaL, m_alphaR;           ///< Bit shifts to extract alpha channel

    dds_header m_dds;                 ///< DDS header

    /// Reset everything to initial state
    ///
    void init () {
        m_file = NULL;
        m_subimage = -1;
        m_buf.clear ();
    }

    /// Helper function: read the image.
    ///
    bool readimg ();

    /// Helper function: calculate bit shifts to properly extract channel data
    ///
    inline void calc_shifts (int mask, int& left, int& right);
};



// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT ImageInput *dds_input_imageio_create () { return new DDSInput; }

DLLEXPORT int dds_imageio_version = OPENIMAGEIO_PLUGIN_VERSION;

DLLEXPORT const char * dds_input_extensions[] = {
    "dds", NULL
};

};



bool
DDSInput::open (const std::string &name, ImageSpec &newspec)
{
    m_filename = name;

    m_file = fopen (name.c_str(), "rb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

// due to struct packing, we may get a corrupt header if we just load the
// struct from file; to adress that, read every member individually
// save some typing
#define RH(memb)    fread (&m_dds.memb, sizeof (m_dds.memb), 1, m_file)
    RH(fourCC);
    RH(size);
    RH(flags);
    RH(height);
    RH(width);
    RH(pitch);
    RH(depth);
    RH(mipmaps);

    // advance the file pointer by 44 bytes (reserved fields)
    fseek (m_file, 44, SEEK_CUR);

    // pixel format struct
    RH(fmt.size);
    RH(fmt.flags);
    RH(fmt.fourCC);
    RH(fmt.bpp);
    RH(fmt.rmask);
    RH(fmt.gmask);
    RH(fmt.bmask);
    RH(fmt.amask);

    // caps
    RH(caps.flags1);
    RH(caps.flags2);

    // advance the file pointer by 8 bytes (reserved fields)
    fseek (m_file, 8, SEEK_CUR);
#undef RH
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

    /*std::cerr << "[dds] fourCC: " << ((char *)&m_dds.fourCC)[0]
                                  << ((char *)&m_dds.fourCC)[1]
                                  << ((char *)&m_dds.fourCC)[2]
                                  << ((char *)&m_dds.fourCC)[3]
                                  << " (" << m_dds.fourCC << ")\n";
    std::cerr << "[dds] size: " << m_dds.size << "\n";
    std::cerr << "[dds] flags: " << m_dds.flags << "\n";
    std::cerr << "[dds] pitch: " << m_dds.pitch << "\n";
    std::cerr << "[dds] width: " << m_dds.width << "\n";
    std::cerr << "[dds] height: " << m_dds.height << "\n";
    std::cerr << "[dds] depth: " << m_dds.depth << "\n";
    std::cerr << "[dds] mipmaps: " << m_dds.mipmaps << "\n";
    std::cerr << "[dds] fmt.size: " << m_dds.fmt.size << "\n";
    std::cerr << "[dds] fmt.flags: " << m_dds.fmt.flags << "\n";
    
    std::cerr << "[dds] fmt.fourCC: " << ((char *)&m_dds.fmt.fourCC)[0]
                                      << ((char *)&m_dds.fmt.fourCC)[1]
                                      << ((char *)&m_dds.fmt.fourCC)[2]
                                      << ((char *)&m_dds.fmt.fourCC)[3]
                                      << " (" << m_dds.fmt.fourCC << ")\n";
    std::cerr << "[dds] fmt.bpp: " << m_dds.fmt.bpp << "\n";
    std::cerr << "[dds] caps.flags1: " << m_dds.caps.flags1 << "\n";
    std::cerr << "[dds] caps.flags2: " << m_dds.caps.flags2 << "\n";*/

    // sanity checks - valid 4CC, correct struct sizes and flags which should
    // be always present, regardless of the image type, size etc., also check
    // for impossible flag combinations
    if (m_dds.fourCC != DDS_MAKE4CC('D', 'D', 'S', ' ')
        || m_dds.size != 124 || m_dds.fmt.size != 32
        || !(m_dds.caps.flags1 & DDS_CAPS1_TEXTURE)
        || !(m_dds.flags & DDS_CAPS)
        || !(m_dds.flags & DDS_PIXELFORMAT)
        || (m_dds.caps.flags2 & DDS_CAPS2_VOLUME
            && !(m_dds.caps.flags1 & DDS_CAPS1_COMPLEX
                && m_dds.flags & DDS_DEPTH))
        || (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP
            && !(m_dds.caps.flags1 & DDS_CAPS1_COMPLEX))){
        error ("Invalid DDS header, possibly corrupt file");
        return false;
    }

    // FIXME: don't try to load stuff we can't handle *yet*
    if (m_dds.caps.flags2 & (DDS_CAPS2_VOLUME | DDS_CAPS2_CUBEMAP)) {
        error ("Volume textures and cube maps are not supported yet, please "
            "poke Leszek in the mailing list");
        return false;
    }

    // make sure all dimensions are > 0 and that we have at least one channel
    // (for uncompressed images)
    if (!(m_dds.flags & DDS_WIDTH) || !m_dds.width
        || !(m_dds.flags & DDS_HEIGHT) || !m_dds.height
        || ((m_dds.flags & DDS_DEPTH) && !m_dds.depth)
        || (!(m_dds.fmt.flags & DDS_PF_FOURCC)
            && !((m_dds.fmt.flags & DDS_PF_RGB)
            | (m_dds.fmt.flags & DDS_PF_LUMINANCE)
            | (m_dds.fmt.flags & DDS_PF_ALPHA)))) {
        error ("Image with no data");
        return false;
    }

    // validate the pixel format
    // TODO: support DXGI and the "wackier" uncompressed formats
    if (m_dds.fmt.flags & DDS_PF_FOURCC
        && m_dds.fmt.fourCC != DDS_4CC_DXT1
        && m_dds.fmt.fourCC != DDS_4CC_DXT2
        && m_dds.fmt.fourCC != DDS_4CC_DXT3
        && m_dds.fmt.fourCC != DDS_4CC_DXT4
        && m_dds.fmt.fourCC != DDS_4CC_DXT5) {
        error ("Unsupported compression type");
        return false;
    }

    // determine the number of channels we have
    if (m_dds.fmt.flags & DDS_PF_FOURCC) {
        // squish decompresses everything to RGBA anyway
        /*if (m_dds.fmt.fourCC == DDS_4CC_DXT1)
            m_nchans = 3; // no alpha in DXT1
        else*/
            m_nchans = 4;
    } else {
        m_nchans = ((m_dds.fmt.flags & DDS_PF_LUMINANCE) ? 1 : 3)
                + ((m_dds.fmt.flags & DDS_PF_ALPHA) ? 1 : 0);
        // also calculate bytes per pixel and the bit shifts
        m_Bpp = (m_dds.fmt.bpp + 7) >> 3;
        if (!(m_dds.fmt.flags & DDS_PF_LUMINANCE)) {
            calc_shifts (m_dds.fmt.rmask, m_redL, m_redR);
            calc_shifts (m_dds.fmt.gmask, m_greenL, m_greenR);
            calc_shifts (m_dds.fmt.bmask, m_blueL, m_blueR);
            calc_shifts (m_dds.fmt.amask, m_alphaL, m_alphaR);
        }
    }

    seek_subimage(0, m_spec);

    newspec = spec ();
    return true;
}



inline void
DDSInput::calc_shifts (int mask, int& left, int& right)
{
    if (mask == 0) {
        left = right = 0;
        return;
    }

    int i, tmp = mask;
    for (i = 0; i < 32; i++, tmp >>= 1) {
        if (tmp & 1)
            break;
    }
    right = i;

    for (i = 0; i < 8; i++, tmp >>= 1) {
        if (!(tmp & 1))
            break;
    }
    left = 8 - i;
}



bool
DDSInput::seek_subimage (int index, ImageSpec &newspec)
{
    // don't seek if the image doesn't contain mipmaps, isn't 3D or a cube map,
    // and don't seek out of bounds
    if ((index < 0 || (!(m_dds.caps.flags1 & DDS_CAPS1_COMPLEX) && index != 0))
        || (m_dds.flags & DDS_MIPMAPCOUNT && m_dds.mipmaps > 0
            && index >= (int)m_dds.mipmaps)
        || (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP && index >= 6))
        return false;

    // early out
    if (index == m_subimage)
        return true;

    // clear buffer so that readimage is called
    m_buf.clear();

    // we can easily calculate the offsets because both compressed and
    // uncompressed images have predictable length
    unsigned int len, w = m_dds.width, h = m_dds.height, d = m_dds.depth;
    if (m_dds.fmt.flags & DDS_PF_FOURCC)
        len = m_dds.pitch;
    else
        len = m_dds.pitch * m_dds.height *
            (m_dds.caps.flags2 & DDS_CAPS2_VOLUME ? m_dds.depth : 1);
    // calculate the offset; start with after the header
    int ofs = 128;
    // skip subimages preceding the one we're seeking to
    for (int i = 1; i <= index; i++) {
        ofs += len;
        w >>= 1;
        if (!w)
            w = 1;
        h >>= 1;
        if (!h)
            h = 1;
        d >>= 1;
        if (!d)
            d = 1;
        if (m_dds.fmt.flags & DDS_PF_FOURCC)
            // only check for DXT1 - all other formats have same block size
            len = squish::GetStorageRequirements(w, h,
                m_dds.fmt.fourCC == DDS_4CC_DXT1 ? squish::kDxt1
                    : squish::kDxt5);
        else
            // mipmaps of uncompressed images are 1/4th the previous size
            //len >>= 2;
            len = w * h * d * m_Bpp;
    }
    // seek to the offset we've found
    fseek (m_file, ofs, SEEK_SET);

    // create imagespec
    m_spec = ImageSpec (w, h, m_nchans, TypeDesc::UINT8);
    m_spec.depth = (m_dds.flags & DDS_DEPTH) ? m_dds.depth : 1;
    if (m_dds.fmt.flags & DDS_PF_FOURCC) {
        std::string tempstr = "";
        tempstr += ((char *)&m_dds.fmt.fourCC)[0];
        tempstr += ((char *)&m_dds.fmt.fourCC)[1];
        tempstr += ((char *)&m_dds.fmt.fourCC)[2];
        tempstr += ((char *)&m_dds.fmt.fourCC)[3];
        m_spec.attribute ("compression", tempstr);
    }
    m_spec.attribute ("BitsPerSample", m_dds.fmt.bpp);
    m_spec.default_channel_names ();
    m_spec.linearity = ImageSpec::UnknownLinearity;

    m_subimage = index;
    newspec = spec ();
    return true;
}



bool
DDSInput::readimg ()
{
    // resize destination buffer
    m_buf.resize (m_spec.scanline_bytes() * m_spec.height * m_spec.depth
        / (1 << m_subimage));
    if (m_dds.fmt.flags & DDS_PF_FOURCC) {
        // compressed image
        int flags = 0;
        switch (m_dds.fmt.fourCC) {
            case DDS_4CC_DXT1:
                flags = squish::kDxt1;
                break;
            // DXT2 and 3 are the same, only 2 has pre-multiplied alpha
            case DDS_4CC_DXT2:
            case DDS_4CC_DXT3:
                flags = squish::kDxt3;
                break;
            // DXT4 and 5 are the same, only 4 has pre-multiplied alpha
            case DDS_4CC_DXT4:
            case DDS_4CC_DXT5:
                flags = squish::kDxt5;
                break;
        }
        // create source buffer
        std::vector<squish::u8> tmp(squish::GetStorageRequirements
            (m_spec.width, m_spec.height, flags));
        // load image into buffer
        fread (&tmp[0], tmp.size(), 1, m_file);
        // decompress image
        squish::DecompressImage (&m_buf[0], m_spec.width, m_spec.height,
            &tmp[0], flags);
        tmp.clear();
        // correct pre-multiplied alpha, if necessary
        if (m_dds.fmt.fourCC == DDS_4CC_DXT2
            || m_dds.fmt.fourCC == DDS_4CC_DXT4) {
            int k;
            for (int y = 0; y < m_spec.height; y++) {
                for (int x = 0; x < m_spec.width; x++) {
                    k = (y * m_spec.width + x) * 4;
                    m_buf[k + 0] =
                        (unsigned char)(m_buf[k + 0] * 255 / m_buf[k + 3]);
                    m_buf[k + 1] =
                        (unsigned char)(m_buf[k + 1] * 255 / m_buf[k + 3]);
                    m_buf[k + 2] =
                        (unsigned char)(m_buf[k + 2] * 255 / m_buf[k + 3]);
                }
            }
        }
    } else {
        // uncompressed image

        // HACK: shortcut for luminance
        if (m_dds.fmt.flags & DDS_PF_LUMINANCE) {
            fread (&m_buf[0], m_spec.scanline_bytes(), m_spec.height, m_file);
            return true;
        }
        
        int k, pixel = 0;
        for (int y = 0; y < m_spec.height; y++) {
            for (int x = 0; x < m_spec.width; x++) {
                fread (&pixel, 1, m_Bpp, m_file);
                k = (y * m_spec.width + x) * m_spec.nchannels;
                m_buf[k + 0] = ((pixel & m_dds.fmt.rmask) >> m_redR)
                    << m_redL;
                m_buf[k + 1] = ((pixel & m_dds.fmt.gmask) >> m_greenR)
                    << m_greenL;
                m_buf[k + 2] = ((pixel & m_dds.fmt.bmask) >> m_blueR)
                    << m_blueL;
                if (m_dds.fmt.flags & DDS_PF_ALPHA)
                    m_buf[k + 3] = ((pixel & m_dds.fmt.amask) >> m_alphaR)
                        << m_alphaL;
            }
        }
    }
    return true;
}



bool
DDSInput::close ()
{
    if (m_file) {
        fclose (m_file);
        m_file = NULL;
    }

    init();  // Reset to initial state
    return true;
}



bool
DDSInput::read_native_scanline (int y, int z, void *data)
{
    if (m_buf.empty ())
        readimg ();

    size_t size = spec().scanline_bytes();
    memcpy (data, &m_buf[0] + y * size, size);
    return true;
}

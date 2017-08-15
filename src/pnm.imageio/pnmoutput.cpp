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

#include <fstream>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

OIIO_PLUGIN_NAMESPACE_BEGIN


class PNMOutput final : public ImageOutput {
public:
    virtual ~PNMOutput ();
    virtual const char * format_name (void) const { return "pnm"; }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride,
                             stride_t ystride, stride_t zstride);

private:
    std::string m_filename;           ///< Stash the filename
    OIIO::ofstream m_file;
    unsigned int m_max_val, m_pnm_type;
    unsigned int m_dither;
    std::vector<unsigned char> m_scratch;
    std::vector<unsigned char> m_tilebuffer;
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT ImageOutput *pnm_output_imageio_create () { return new PNMOutput; }

    OIIO_EXPORT const char * pnm_output_extensions[] = {
        "ppm","pgm","pbm","pnm", NULL
    };

OIIO_PLUGIN_EXPORTS_END


inline void
write_ascii_binary (std::ostream & file, const unsigned char * data, 
                    const stride_t stride, const ImageSpec & spec)
{
    for (int x = 0; x < spec.width; x++) 
        file << (data[x * stride] ? '1' : '0') << "\n"; 
}



inline void
write_raw_binary (std::ostream & file, const unsigned char * data, 
                  const stride_t stride, const ImageSpec & spec)
{
    unsigned char val;
    for (int x = 0; x < spec.width;) {
        val=0;
        for (int bit=7; bit >= 0 && x < spec.width; x++, bit--) 
            val += (data[x * stride] ? (1 << bit) : 0);
        file.write ((char*)&val, sizeof (char));
    }
}



template <class T>
inline void
write_ascii (std::ostream &file, const T *data, const stride_t stride, 
             const ImageSpec &spec, unsigned int max_val)
{
    unsigned int pixel, val;
    for (int x = 0; x < spec.width; x++){
        pixel = x * stride;
        for (int c = 0; c < spec.nchannels; c++) {
            val = data [pixel + c];
            val = val * max_val / std::numeric_limits<T>::max();
            file << val << "\n"; 
        }
    }
}



template <class T>
inline void
write_raw (std::ostream &file, const T *data, const stride_t stride, 
           const ImageSpec &spec, unsigned int max_val)
{
    unsigned char byte;
    unsigned int pixel, val;
    for (int x = 0; x < spec.width; x++) {
        pixel = x * stride;
        for (int c = 0; c < spec.nchannels; c++) {
            val = data[pixel + c];
            val = val * max_val / std::numeric_limits<T>::max();
            if (sizeof(T) == 2)
            {
                // Writing a 16bit ppm file
                // I'll adopt the practice of Netpbm and write the MSB first
                byte = static_cast<unsigned char>(val >> 8);
                file.write ((char*)&byte, 1);
                byte = static_cast<unsigned char>(val & 0xff);
                file.write ((char*)&byte, 1);
            }
            else
            {
                // This must be an 8bit ppm file
                byte = static_cast<unsigned char>(val);
                file.write ((char*)&byte, 1);
            }
        }
    }
}



PNMOutput::~PNMOutput ()
{
    close ();
}



bool
PNMOutput::open (const std::string &name, const ImageSpec &userspec,
                 OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    close ();  // Close any already-opened file
    m_spec = userspec;  // Stash the spec
    m_spec.set_format (TypeDesc::UINT8);  // Force 8 bit output
    int bits_per_sample = m_spec.get_int_attribute ("oiio:BitsPerSample", 8);
    m_dither = (m_spec.format == TypeDesc::UINT8) ?
                    m_spec.get_int_attribute ("oiio:dither", 0) : 0;

    if (m_spec.nchannels != 1 && m_spec.nchannels != 3) {
        error ("%s does not support %d-channel images\n",
               format_name(), m_spec.nchannels);
        return false;
    }

    if (bits_per_sample == 1)
        m_pnm_type = 4;
    else if (m_spec.nchannels == 1)
        m_pnm_type = 5;
    else 
        m_pnm_type = 6;
    if (!m_spec.get_int_attribute ("pnm:binary", 1)) 
    {
        m_pnm_type -= 3;
        Filesystem::open (m_file, name);
        
    }
    else {
        Filesystem::open (m_file, name, std::ios::out|std::ios::binary);
 
    }
    
    if (!m_file)
        return false;

    m_max_val = (1 << bits_per_sample) - 1;
    // Write header
    m_file << "P" << m_pnm_type << std::endl;
    m_file << m_spec.width << " " << m_spec.height << std::endl;
    if (m_pnm_type != 1 && m_pnm_type != 4)  // only non-monochrome 
        m_file << m_max_val << std::endl;

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize (m_spec.image_bytes());

    return m_file.good();
}



bool
PNMOutput::close ()
{
    if (!m_file) {   // already closed
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // Handle tile emulation -- output the buffered pixels
        ASSERT (m_tilebuffer.size());
        ok &= write_scanlines (m_spec.y, m_spec.y+m_spec.height, 0,
                               m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap (m_tilebuffer);
    }

    m_file.close();
    return true;  
}



bool
PNMOutput::write_scanline (int y, int z, TypeDesc format,
        const void *data, stride_t xstride)
{
    if (!m_file)
        return false;
    if (z)
        return false;

    m_spec.auto_stride (xstride, format, spec().nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch,
                               m_dither, y, z);
    if (data != origdata) // a conversion happened...
        xstride = spec().nchannels;

    switch (m_pnm_type){
        case 1:
            write_ascii_binary (m_file, (unsigned char *) data, xstride, m_spec);
            break;
        case 2:
        case 3:
            if (m_max_val > std::numeric_limits<unsigned char>::max())
                write_ascii (m_file, (unsigned short *) data, xstride, m_spec, m_max_val);
            else 
                write_ascii (m_file, (unsigned char *) data, xstride, m_spec, m_max_val);
            break;
        case 4:
            write_raw_binary (m_file, (unsigned char *) data, xstride, m_spec);
            break;
        case 5:
        case 6:
            if (m_max_val > std::numeric_limits<unsigned char>::max())
                write_raw (m_file, (unsigned short *) data, xstride, m_spec, m_max_val);
            else 
                write_raw (m_file, (unsigned char *) data, xstride, m_spec, m_max_val);
            break;
        default:
            return false;
    } 

    return m_file.good();
}



bool
PNMOutput::write_tile (int x, int y, int z, TypeDesc format,
                       const void *data, stride_t xstride,
                       stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer (x, y, z, format, data, xstride,
                                      ystride, zstride, &m_tilebuffer[0]);
}


OIIO_PLUGIN_NAMESPACE_END


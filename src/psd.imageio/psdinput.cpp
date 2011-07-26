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

    std::string m_filename;
    std::ifstream m_file;
    //Current subimage
	int m_subimage;
	//Subimage count (1 + layer count)
	int m_subimage_count;

    //File header structure
	FileHeader m_header;

    //Reset to initial state
    void init ();

    //File Header
    bool load_header ();
    bool read_header ();
    bool validate_header ();

    //Check if m_file is good. If not, set error message and return false.
    bool check_io ();

    //This may be a bit inefficient but I think it's worth the convenience.
    //This takes care of reading a 32-bit BE into a 64-bit LE.
    template <typename TStorage, typename TVariable>
    bool read_bige (TVariable &value)
    {
        TStorage buffer;
        m_file.read ((char *)&buffer, sizeof(buffer));
        if (!bigendian ())
            swap_endian (&buffer);

        //value = boost::numeric_cast<TVariable>(buffer);
        value = buffer;
        return m_file;
    }

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
PSDInput::check_io ()
{
    if (!m_file) {
        error ("\"%s\": I/O error", m_filename.c_str ());
        return false;
    }
    return true;
}

OIIO_PLUGIN_NAMESPACE_END


/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
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

#include "zlib.h"

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>

OIIO_PLUGIN_NAMESPACE_BEGIN


namespace {  // anon namespace

struct ZfileHeader {
    int magic;
    short width;
    short height;
    float worldtoscreen[16];
    float worldtocamera[16];
};

static const int zfile_magic = 0x2f0867ab;
static const int zfile_magic_endian = 0xab67082f;  // other endianness

}  // end anon namespace



class ZfileInput final : public ImageInput {
public:
    ZfileInput () { init(); }
    virtual ~ZfileInput () { close(); }
    virtual const char * format_name (void) const { return "zfile"; }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    std::string m_filename;       ///< Stash the filename
    gzFile m_gz;                  ///< Handle for compressed files
    bool m_swab;                  ///< swap bytes for other endianness?
    int m_next_scanline;          ///< Which scanline is the next to be read?

    // Reset everything to initial state
    void init () {
        m_gz = 0;
        m_swab = false;
        m_next_scanline = 0;
    }
};



class ZfileOutput final : public ImageOutput {
public:
    ZfileOutput () { init(); }
    virtual ~ZfileOutput () { close(); }
    virtual const char * format_name (void) const { return "zfile"; }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride,
                             stride_t ystride, stride_t zstride);

private:
    std::string m_filename;       ///< Stash the filename
    FILE *m_file;                 ///< Open image handle for not compresed
    gzFile m_gz;                  ///< Handle for compressed files
    std::vector<unsigned char> m_scratch;
    std::vector<unsigned char> m_tilebuffer;

    // Initialize private members to pre-opened state
    void init (void) {
        m_file = NULL;
        m_gz = 0;
    }
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int zfile_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char* zfile_imageio_library_version () { return NULL; }

OIIO_EXPORT ImageInput *zfile_input_imageio_create () { return new ZfileInput; }

OIIO_EXPORT const char * zfile_input_extensions[] = {
    "zfile", NULL
};

OIIO_EXPORT ImageOutput *zfile_output_imageio_create () { return new ZfileOutput; }

OIIO_EXPORT const char * zfile_output_extensions[] = {
    "zfile", NULL
};

OIIO_PLUGIN_EXPORTS_END

bool
ZfileInput::valid_file (const std::string &filename) const
{
#ifdef _WIN32
    std::wstring wpath = Strutil::utf8_to_utf16(filename);
    gzFile gz = gzopen_w (wpath.c_str(), "rb");
#else
    gzFile gz = gzopen (filename.c_str(), "rb");
#endif
    if (! gz)
        return false;

    ZfileHeader header;
    gzread (gz, &header, sizeof(header));

    bool ok = (header.magic == zfile_magic || header.magic == zfile_magic_endian);
    gzclose (gz);
    return ok;
}



bool
ZfileInput::open (const std::string &name, ImageSpec &newspec)
{
    m_filename = name;

    FILE *fd = Filesystem::fopen (name, "rb");
    m_gz = (fd) ? gzdopen (fileno (fd), "rb") : NULL;
    if (! m_gz) {
        if (fd)
            fclose (fd);
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    ZfileHeader header;
    ASSERT (sizeof(header) == 136);
    gzread (m_gz, &header, sizeof(header));

    if (header.magic != zfile_magic && header.magic != zfile_magic_endian) {
        error ("Not a valid Zfile");
        return false;
    }

    m_swab = (header.magic == zfile_magic_endian);
    if (m_swab) {
        swap_endian (&header.width);
        swap_endian (&header.height);
        swap_endian ((float *)&header.worldtoscreen, 16);
        swap_endian ((float *)&header.worldtocamera, 16);
    }

    m_spec = ImageSpec (header.width, header.height, 1, TypeDesc::FLOAT);
    if (m_spec.channelnames.size() == 0)
        m_spec.channelnames.emplace_back("z");
    else
        m_spec.channelnames[0] = "z";
    m_spec.z_channel = 0;

    m_spec.attribute ("worldtoscreen", TypeMatrix,
                      (float *)&header.worldtoscreen);
    m_spec.attribute ("worldtocamera", TypeMatrix,
                      (float *)&header.worldtocamera);

    newspec = spec ();
    return true;
}



bool
ZfileInput::close ()
{
    if (m_gz) {
        gzclose (m_gz);
        m_gz = 0;
    }

    init();  // Reset to initial state
    return true;
}



bool
ZfileInput::read_native_scanline (int y, int z, void *data)
{
    if (m_next_scanline > y) {
        // User is trying to read an earlier scanline than the one we're
        // up to.  Easy fix: close the file and re-open.
        ImageSpec dummyspec;
        int subimage = current_subimage();
        if (! close ()  ||
            ! open (m_filename, dummyspec)  ||
            ! seek_subimage (subimage, dummyspec))
            return false;    // Somehow, the re-open failed
        ASSERT (m_next_scanline == 0 && current_subimage() == subimage);
    }
    while (m_next_scanline <= y) {
        // Keep reading until we're read the scanline we really need
        gzread (m_gz, data, m_spec.width*sizeof(float));
        ++m_next_scanline;
    }
    if (m_swab)
        swap_endian ((float *)data, m_spec.width);
    return true;
}




bool
ZfileOutput::open (const std::string &name, const ImageSpec &userspec,
                   OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    close ();  // Close any already-opened file
    m_gz = 0;
    m_file = NULL;
    m_spec = userspec;  // Stash the spec

    // Check for things this format doesn't support
    if (m_spec.width < 1 || m_spec.height < 1) {
        error ("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }
    if (m_spec.depth < 1)
        m_spec.depth = 1;
    if (m_spec.depth > 1) {
        error ("%s does not support volume images (depth > 1)", format_name());
        return false;
    }

    if (m_spec.nchannels != 1) {
        error ("Zfile only supports 1 channel, not %d", m_spec.nchannels);
        return false;
    }

    // Force float
    if (m_spec.format != TypeDesc::FLOAT)
        m_spec.format = TypeDesc::FLOAT;

    ZfileHeader header;
    header.magic = zfile_magic;
    header.width = (int)m_spec.width;
    header.height = (int)m_spec.height;

    ParamValue *p;
    static float ident[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    if ((p = m_spec.find_attribute ("worldtocamera", TypeMatrix)))
        memcpy (header.worldtocamera, p->data(), 16*sizeof(float));
    else
        memcpy (header.worldtocamera, ident, 16*sizeof(float));
    if ((p = m_spec.find_attribute ("worldtoscreen", TypeMatrix)))
        memcpy (header.worldtoscreen, p->data(), 16*sizeof(float));
    else
        memcpy (header.worldtoscreen, ident, 16*sizeof(float));

    if (m_spec.get_string_attribute ("compression", "none") != std::string("none")) {
        FILE *fd = Filesystem::fopen (name, "wb");
        if (fd) {
            m_gz = gzdopen (fileno (fd), "wb");
            if (!m_gz)
                fclose (fd);
        }
    }
    else
        m_file = Filesystem::fopen (name, "wb");
    if (! m_file  &&  ! m_gz) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    if (m_gz)
        gzwrite (m_gz, &header, sizeof(header));
    else {
    	size_t b = fwrite (&header, sizeof(header), 1, m_file);
    	if (b != 1) {
            error ("Failed write zfile::open (err: %d)", b);
            return false;
    	}
    }

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.this form
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize (m_spec.image_bytes());

    return true;
}



bool
ZfileOutput::close ()
{
    bool ok = true;
    if (m_spec.tile_width) {
        // We've been emulating tiles; now dump as scanlines.
        ASSERT (m_tilebuffer.size());
        ok &= write_scanlines (m_spec.y, m_spec.y+m_spec.height, 0,
                               m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap (m_tilebuffer);
    }

    if (m_gz) {
        gzclose (m_gz);
        m_gz = 0;
    }
    if (m_file) {
        fclose (m_file);
        m_file = NULL;
    }

    init ();      // re-initialize
    return ok;
}



bool
ZfileOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    y -= m_spec.y;
    m_spec.auto_stride (xstride, format, spec().nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    if (m_gz)
        gzwrite (m_gz, data, m_spec.width*sizeof(float));
    else {
    	size_t b = fwrite (data, sizeof(float), m_spec.width, m_file);
        if (b != (size_t)m_spec.width) {
            error ("Failed write zfile::open (err: %d)", b);
            return false;
        }
    }

    return true;
}



bool
ZfileOutput::write_tile (int x, int y, int z, TypeDesc format,
                       const void *data, stride_t xstride,
                       stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer (x, y, z, format, data, xstride,
                                      ystride, zstride, &m_tilebuffer[0]);
}


OIIO_PLUGIN_NAMESPACE_END


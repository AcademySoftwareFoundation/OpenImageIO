/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
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

#include <boost/scoped_array.hpp>
#include <vector>
#include <gif_lib.h>

#include "imageio.h"
#include "thread.h"

// GIFLIB:
// http://giflib.sourceforge.net/
// Format description:
// http://giflib.sourceforge.net/whatsinagif/index.html

// for older giflib versions
#ifndef GIFLIB_MAJOR
#define GIFLIB_MAJOR 4
#endif

OIIO_PLUGIN_NAMESPACE_BEGIN

class GIFInput : public ImageInput {
public:
    GIFInput () { init (); }
    virtual ~GIFInput () { close (); }
    virtual const char *format_name (void) const { return "gif"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close (void);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);

    virtual int current_subimage (void) const { return m_subimage; }
    
    virtual int current_miplevel (void) const {
        // No mipmap support
        return 0;
    }

 private:
    std::string m_filename;          ///< Stash the filename
    GifFileType *m_gif_file;         ///< GIFLIB handle
    GifColorType *m_global_colormap; ///< Global colormap. Used if there's
                                     ///  no local colormap provided.
    GifColorType *m_local_colormap;  ///< Colormap for current subimage
    int m_background_color;          ///< Background (transparent) color index
    int m_subimage;                  ///< Current subimage index
    int m_next_scanline;             ///< Next scanline to read
    std::vector<unsigned char> m_cached_data; ///< Cached scanlines in native
                                              ///  format
    
    /// Reset everything to initial state
    ///
    void init (void);

    /// Read current subimage metadata.
    ///
    bool read_subimage_metadata (ImageSpec &newspec,
                                 GifColorType **local_colormap);

    /// Helper: read gif extension.
    ///
    void read_gif_extension (int ext_code, GifByteType *ext, ImageSpec &spec);

    /// Decode and return a real scanline index in the interlaced image.
    ///
    int decode_line_number (int line_number);

    /// Translate scanline data from a native format.
    ///
    void translate_scanline (unsigned char *gif_scanline, void *data);

    /// Print error message.
    ///
    void report_last_error (void);
};



// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int gif_imageio_version = OIIO_PLUGIN_VERSION;
OIIO_EXPORT ImageInput *gif_input_imageio_create () { return new GIFInput; }
OIIO_EXPORT const char *gif_input_extensions[] = { "gif", NULL };

OIIO_PLUGIN_EXPORTS_END



void
GIFInput::init (void)
{
    m_gif_file = NULL;
}



bool
GIFInput::open (const std::string &name, ImageSpec &newspec)
{
    m_filename = name;
    m_subimage = -1;
    m_next_scanline = -1;
    m_cached_data.clear ();
    
    return seek_subimage (0, 0, newspec);
}



inline int
GIFInput::decode_line_number (int line_number) {
    if ((line_number + 1) % 2 == 0) // 4th tile 1/2 sized
        return (m_spec.height + line_number) / 2;
    if ((line_number + 2) % 4 == 0) // 3rd tile 1/4 sized
        return (m_spec.height + line_number - 2) / 4;
    if ((line_number + 4) % 8 == 0) // 2nd tile 1/8 sized
        return (m_spec.height + line_number) / 8;

    // line_number % 8 == 0  (1st tile 1/8 sized)
    return line_number / 8;
}



void
GIFInput::translate_scanline (unsigned char *gif_scanline, void *data)
{
    unsigned char *outscanline = (unsigned char *)data;
    GifColorType *colormap = m_local_colormap != NULL ?
        m_local_colormap : m_global_colormap;
    if (m_spec.nchannels == 3) {
        for (int i = 0; i < m_spec.width; i++) {
            outscanline[3 * i] =
                (unsigned char)colormap[gif_scanline[i]].Red;
            outscanline[3 * i + 1] =
                (unsigned char)colormap[gif_scanline[i]].Green;
            outscanline[3 * i + 2] =
                (unsigned char)colormap[gif_scanline[i]].Blue;
        }
    } else {
        for (int i = 0; i < m_spec.width; i++) {
            outscanline[4 * i] =
                (unsigned char)colormap[gif_scanline[i]].Red;
            outscanline[4 * i + 1] =
                (unsigned char)colormap[gif_scanline[i]].Green;
            outscanline[4 * i + 2] =
                (unsigned char)colormap[gif_scanline[i]].Blue;
            outscanline[4 * i + 3] =
                (m_background_color == gif_scanline[i] ? 0x00 : 0xff);
        }
    }
}



bool
GIFInput::read_native_scanline (int _y, int z, void *data)
{
    if (_y < 0 || _y > m_spec.height)
        return false;

    bool interlacing = m_spec.get_int_attribute ("gif:Interlacing") != 0;
    
    // decode scanline index if image is interlaced
    int gif_y = interlacing ? decode_line_number (_y) : _y;

    if (interlacing) { // gif is interlaced so cache the scanlines
        int scanlines_cached = m_cached_data.size() / m_spec.width;
        if (gif_y >= scanlines_cached) {
            // scanline is not cached yet, read the scanline and preceding ones
            m_cached_data.resize (m_spec.width * (gif_y + 1));
            int delta_size = m_spec.width * (gif_y - scanlines_cached + 1);
            if (DGifGetLine (m_gif_file,
                             &m_cached_data[scanlines_cached * m_spec.width],
                             delta_size) == GIF_ERROR) {
                report_last_error ();
                return false;
            }
        }
        
        translate_scanline (&m_cached_data[gif_y * m_spec.width], data);
        
    } else { // no interlacing, thus no scanlines caching
        if (m_next_scanline > gif_y) {
            // requested scanline is located before the one to read next
            // random access is not supported, so reopen the file, find
            // current subimage and skip preceding image data
            ImageSpec dummyspec;
            if (! close () ||
                ! open (m_filename, dummyspec) ||
                ! seek_subimage (m_subimage, 0, dummyspec)) {
                return false;
            }
        
            int remaining_size = m_spec.width * gif_y;
            boost::scoped_array<unsigned char> buffer
                (new unsigned char[remaining_size]);
            if (DGifGetLine (m_gif_file, buffer.get(), remaining_size)
                == GIF_ERROR) {
                report_last_error ();
                return false;
            }
        
        } else if (m_next_scanline < gif_y) {
            // requested scanline is located after the one to read next
            // skip the lines in between
            int delta_size = m_spec.width * (gif_y - m_next_scanline);
            boost::scoped_array<unsigned char> buffer
                (new unsigned char[delta_size]);
            if (DGifGetLine (m_gif_file,
                             buffer.get(),
                             delta_size) == GIF_ERROR) {
                report_last_error ();
                return false;
            }
        }

        // read the requested scanline
        boost::scoped_array<unsigned char> fscanline
                (new unsigned char[m_spec.width]);
        if (DGifGetLine (m_gif_file,
                         fscanline.get(),
                         m_spec.width) == GIF_ERROR) {
            report_last_error ();
            return false;
        }
        translate_scanline (fscanline.get(), data);
    }

    m_next_scanline = gif_y + 1;
    
    return true;
}



void
GIFInput::read_gif_extension (int ext_code, GifByteType *ext,
                              ImageSpec &newspec)
{
    if (ext_code == GRAPHICS_EXT_FUNC_CODE) {
        // read background color index and delay time between frames
        // http://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html#graphics_control_extension_block
        
        if (ext[1] & 0x01) {
            newspec.nchannels = 4;
            m_background_color = (int) ext[4];
        }

        int delay = (ext[3] << 8) | ext[2];

        if (delay) {
            newspec.attribute ("gif:DelayMs", delay * 10);
        }
        
    } else if (ext_code == COMMENT_EXT_FUNC_CODE) {
        // read comment data
        // http://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html#comment_extension_block
        std::string comment = std::string ((const char *)&ext[1], int (ext[0]));
        newspec.attribute("ImageDescription", comment);
    } 
}



bool
GIFInput::read_subimage_metadata (ImageSpec &newspec,
                                  GifColorType **local_colormap)
{
    newspec = ImageSpec (TypeDesc::UINT8);
    newspec.nchannels = 3;
    
    GifRecordType m_gif_rec_type;
    do {
        if (DGifGetRecordType (m_gif_file, &m_gif_rec_type) == GIF_ERROR) {
            report_last_error ();
            return false;
        }

        switch (m_gif_rec_type) {
        case IMAGE_DESC_RECORD_TYPE:
            if (DGifGetImageDesc (m_gif_file) == GIF_ERROR) {
                report_last_error ();
                return false;
            }

            newspec.width = m_gif_file->Image.Width;
            newspec.height = m_gif_file->Image.Height;

            // read local colormap
            if (m_gif_file->Image.ColorMap != NULL) {
                *local_colormap = m_gif_file->Image.ColorMap->Colors;
            } else {
                *local_colormap = NULL;
            }
            
            break;

        case EXTENSION_RECORD_TYPE:
            int ext_code;
            GifByteType *ext;
            if (DGifGetExtension(m_gif_file, &ext_code, &ext) == GIF_ERROR) 
            {
                report_last_error ();
                return false;
            }
            read_gif_extension (ext_code, ext, newspec);
            
            while (ext != NULL)
            {
                if (DGifGetExtensionNext(m_gif_file, &ext) == GIF_ERROR) 
                {
                    report_last_error ();
                    return false;
                }
                
                if (ext != NULL) {
                    read_gif_extension (ext_code, ext, newspec);
                }
            }

            break;

        case TERMINATE_RECORD_TYPE:
            return false;
            break;

        default:
            break;
        }
    } while (m_gif_rec_type != IMAGE_DESC_RECORD_TYPE);

    newspec.attribute ("gif:Interlacing", m_gif_file->Image.Interlace);

    newspec.default_channel_names ();    
    if (newspec.nchannels == 4) {
        newspec.alpha_channel = 4;
    }

    return true;
}



bool
GIFInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    GifColorType *local_colormap = NULL;

    if (subimage < 0 || miplevel != 0)
        return false;
    
    if (m_subimage == subimage) {
        // We're already pointing to the right subimage
        newspec = m_spec;
        return true;
    }

    if (m_subimage > subimage) {
        // requested subimage is located before the current one
        // file needs to be reopened
        if (m_gif_file && ! close()) {
            return false;
        }
    }

    if (! m_gif_file) {
        
#if GIFLIB_MAJOR >= 5
        int giflib_error;
        if (! (m_gif_file = DGifOpenFileName (m_filename.c_str(),
                                              &giflib_error))) {
            error (GifErrorString (giflib_error));
            return false; 
        }
#else
        if (! (m_gif_file = DGifOpenFileName (m_filename.c_str()))) {
            error ("Error trying to open the file.");
            return false; 
        }
#endif
        
        m_subimage = -1;
        
        // read global color table
        if (m_gif_file->SColorMap) {
            m_global_colormap = m_gif_file->SColorMap->Colors;
        } else {
            m_global_colormap = NULL;
        }
    }

    if (m_subimage < subimage) {
        // requested subimage is located after the current one
        // skip the preceding part of current image..
        if (m_subimage != -1 && m_next_scanline < m_spec.height) {
            int remaining_size =
                m_spec.width * (m_spec.height - m_next_scanline);
            boost::scoped_array<unsigned char> buffer
                (new unsigned char[remaining_size]);
            if (DGifGetLine (m_gif_file,
                             buffer.get(),
                             remaining_size) == GIF_ERROR) {
                report_last_error ();
                return false;
            }
        }

        // ..and skip the rest of preceding subimages
        for (m_subimage += 1; m_subimage < subimage; m_subimage ++) {
            if (! read_subimage_metadata (newspec, &local_colormap)) {
                return false;
            }
            int image_size = newspec.width * newspec.height;
            boost::scoped_array<unsigned char> buffer
                (new unsigned char[image_size]);
            if (DGifGetLine (m_gif_file,
                             buffer.get(),
                             image_size) == GIF_ERROR) {
                report_last_error ();
                return false;
            }
        }
    }

    // read metadata of current subimage
    if (! read_subimage_metadata (newspec, &local_colormap)) {
        return false;
    }

    m_spec = newspec;
    m_subimage = subimage;
    m_next_scanline = 0;
    m_cached_data.clear ();
    m_local_colormap = local_colormap;
    
    return true;
}



static spin_mutex gif_error_mutex;


void
GIFInput::report_last_error (void)
{
    // N.B. Only GIFLIB_MAJOR >= 5 looks properly thread-safe, in that the
    // error is guaranteed to be specific to this open file.  We use a  spin
    // mutex to prevent a thread clash for older versions, but it still
    // appears to be a global call, so we can't be absolutely sure that the
    // error was for *this* file.  So if you're using giflib prior to
    // version 5, beware.
#if GIFLIB_MAJOR >= 5
    error ("%s", GifErrorString (m_gif_file->Error));
#elif GIFLIB_MAJOR == 4 && GIFLIB_MINOR >= 2
    spin_lock lock (gif_error_mutex);
    error ("%s", GifErrorString());
#else
    spin_lock lock (gif_error_mutex);
    error ("GIF error %d", GifLastError());
#endif
}



inline bool
GIFInput::close (void)
{
    if (m_gif_file) {
        if (DGifCloseFile (m_gif_file) == GIF_ERROR) {
            error ("Error trying to close the file.");
            return false;
        }
        m_gif_file = NULL;
    }
    m_cached_data.clear ();
    
    return true;
}

OIIO_PLUGIN_NAMESPACE_END


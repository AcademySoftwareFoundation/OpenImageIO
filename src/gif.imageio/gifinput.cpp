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

#include <vector>
#include <memory>
#include <gif_lib.h>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/thread.h>

// GIFLIB:
// http://giflib.sourceforge.net/
// Format description:
// http://giflib.sourceforge.net/whatsinagif/index.html

// for older giflib versions
#ifndef GIFLIB_MAJOR
#define GIFLIB_MAJOR 4
#endif

#ifndef DISPOSAL_UNSPECIFIED
#define DISPOSAL_UNSPECIFIED 0
#endif

#ifndef DISPOSE_BACKGROUND
#define DISPOSE_BACKGROUND 2
#endif

OIIO_PLUGIN_NAMESPACE_BEGIN

class GIFInput final : public ImageInput {
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
    int m_transparent_color;         ///< Transparent color index
    int m_subimage;                  ///< Current subimage index
    int m_disposal_method;           ///< Disposal method of current subimage.
                                     ///  Indicates what to do with canvas
                                     ///  before drawing the _next_ subimage.
    int m_previous_disposal_method;  ///< Disposal method of previous subimage.
                                     ///  Indicates what to do with canvas
                                     ///  before drawing _current_ subimage.
    std::vector<unsigned char> m_canvas; ///< Image canvas in output format, on
                                         ///  which subimages are sequentially
                                         ///  drawn.

    /// Reset everything to initial state
    ///
    void init (void);

    /// Read current subimage metadata.
    ///
    bool read_subimage_metadata (ImageSpec &newspec);

    /// Read current subimage data (ie. draw it on canvas).
    ///
    bool read_subimage_data (void);

    /// Helper: read gif extension.
    ///
    void read_gif_extension (int ext_code, GifByteType *ext, ImageSpec &spec);

    /// Decode and return a real scanline index in the interlaced image.
    ///
    int decode_line_number (int line_number, int height);

    /// Print error message.
    ///
    void report_last_error (void);
};



// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int gif_imageio_version = OIIO_PLUGIN_VERSION;
OIIO_EXPORT ImageInput *gif_input_imageio_create () { return new GIFInput; }
OIIO_EXPORT const char *gif_input_extensions[] = { "gif", NULL };

OIIO_EXPORT const char* gif_imageio_library_version () {
#define STRINGIZE2(a) #a
#define STRINGIZE(a) STRINGIZE2(a)
#if defined(GIFLIB_MAJOR) && defined(GIFLIB_MINOR) && defined(GIFLIB_RELEASE)
    return "gif_lib " STRINGIZE(GIFLIB_MAJOR) "." STRINGIZE(GIFLIB_MINOR) "." STRINGIZE(GIFLIB_RELEASE);
#else
    return "gif_lib unknown version";
#endif
}

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
    m_canvas.clear ();
    
    return seek_subimage (0, 0, newspec);
}



inline int
GIFInput::decode_line_number (int line_number, int height)
{
    if (1 < height && (height + 1) / 2 <= line_number) // 4th tile 1/2 sized
        return 2 * (line_number - (height + 1) / 2) + 1;
    if (2 < height && (height + 3) / 4 <= line_number) // 3rd tile 1/4 sized
        return 4 * (line_number - (height + 3) / 4) + 2;
    if (4 < height && (height + 7) / 8 <= line_number) // 2nd tile 1/8 sized
        return 8 * (line_number - (height + 7) / 8) + 4;

    // 1st tile 1/8 sized
    return line_number * 8;
}



bool
GIFInput::read_native_scanline (int y, int z, void *data)
{
    if (y < 0 || y > m_spec.height || ! m_canvas.size())
        return false;

    memcpy (data, &m_canvas[y * m_spec.width * m_spec.nchannels],
            m_spec.width * m_spec.nchannels);

    return true;
}



void
GIFInput::read_gif_extension (int ext_code, GifByteType *ext,
                              ImageSpec &newspec)
{
    if (ext_code == GRAPHICS_EXT_FUNC_CODE) {
        // read background color index, disposal method and delay time between frames
        // http://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html#graphics_control_extension_block
        
        if (ext[1] & 0x01) {
            m_transparent_color = (int) ext[4];
        }

        m_disposal_method = (ext[1] & 0x1c) >> 2;

        int delay = (ext[3] << 8) | ext[2];
        if (delay) {
            int rat[2] = { 100, delay };
            newspec.attribute ("FramesPerSecond", TypeRational, &rat);
            newspec.attribute ("oiio:Movie", 1);
        }
        
    } else if (ext_code == COMMENT_EXT_FUNC_CODE) {
        // read comment data
        // http://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html#comment_extension_block
        std::string comment = std::string ((const char *)&ext[1], int (ext[0]));
        newspec.attribute("ImageDescription", comment);

    } else if (ext_code == APPLICATION_EXT_FUNC_CODE) {
        // read loop count
        // http://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html#application_extension_block
        if (ext[0] == 3) {
            newspec.attribute ("gif:LoopCount", (ext[3] << 8) | ext[2]);
        }
    }
}



bool
GIFInput::read_subimage_metadata (ImageSpec &newspec)
{
    newspec = ImageSpec (TypeDesc::UINT8);
    newspec.nchannels = 4;
    newspec.default_channel_names ();
    newspec.alpha_channel = 4;
    newspec.attribute ("oiio:ColorSpace", "sRGB");

    m_previous_disposal_method = m_disposal_method;
    m_disposal_method = DISPOSAL_UNSPECIFIED;

    m_transparent_color = -1;
    
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

    newspec.attribute ("gif:Interlacing", m_gif_file->Image.Interlace ? 1 : 0);

    return true;
}



bool
GIFInput::read_subimage_data()
{
    GifColorType *colormap = NULL;
    if (m_gif_file->Image.ColorMap) { // local colormap
        colormap = m_gif_file->Image.ColorMap->Colors;
    } else if (m_gif_file->SColorMap) { // global colormap
        colormap = m_gif_file->SColorMap->Colors;
    } else {
        error ("Neither local nor global colormap present.");
        return false;
    }

    if (m_subimage == 0 || m_previous_disposal_method == DISPOSE_BACKGROUND) {
        // make whole canvas transparent
        std::fill (m_canvas.begin(), m_canvas.end(), 0x00);
    }

    // decode scanline index if image is interlaced
    bool interlacing = m_spec.get_int_attribute ("gif:Interlacing") != 0;

    // get subimage dimensions and draw it on canvas
    int window_height = m_gif_file->Image.Height;
    int window_width  = m_gif_file->Image.Width;
    int window_top    = m_gif_file->Image.Top;
    int window_left   = m_gif_file->Image.Left;
    std::unique_ptr<unsigned char[]> fscanline (new unsigned char [window_width]);
    for (int wy = 0; wy < window_height; wy++) {
        if (DGifGetLine (m_gif_file, &fscanline[0], window_width) == GIF_ERROR) {
            report_last_error ();
            return false;
        }
        int y = window_top + (interlacing ?
                                  decode_line_number(wy, window_height) : wy);
        if (0 <= y && y < m_spec.height) {
            for (int wx = 0; wx < window_width; wx++) {
                int x = window_left + wx;
                int idx = m_spec.nchannels * (y * m_spec.width + x);
                if (0 <= x && x < m_spec.width &&
                        fscanline[wx] != m_transparent_color) {
                    m_canvas[idx]     = colormap[fscanline[wx]].Red;
                    m_canvas[idx + 1] = colormap[fscanline[wx]].Green;
                    m_canvas[idx + 2] = colormap[fscanline[wx]].Blue;
                    m_canvas[idx + 3] = 0xff;
                }
            }
        }
    }

    return true;
}



bool
GIFInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
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
        m_canvas.resize (m_gif_file->SWidth * m_gif_file->SHeight * 4);
    }

    // skip subimages preceding the requested one
    if (m_subimage < subimage) {
        for (m_subimage += 1; m_subimage < subimage; m_subimage ++) {
            if (! read_subimage_metadata (newspec) ||
                ! read_subimage_data ()) {
                return false;
            }
        }
    }

    // read metadata of current subimage
    if (! read_subimage_metadata (newspec)) {
        return false;
    }

    newspec.width = m_gif_file->SWidth;
    newspec.height = m_gif_file->SHeight;
    newspec.depth = 1;
    newspec.full_height = newspec.height;
    newspec.full_width = newspec.width;
    newspec.full_depth = newspec.depth;

    m_spec = newspec;
    m_subimage = subimage;

    // draw subimage on canvas
    if (! read_subimage_data ()) {
        return false;
    }

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
#if GIFLIB_MAJOR > 5 || (GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 1)
        if (DGifCloseFile (m_gif_file, NULL) == GIF_ERROR) {
#else
        if (DGifCloseFile (m_gif_file) == GIF_ERROR) {
#endif
            error ("Error trying to close the file.");
            return false;
        }
        m_gif_file = NULL;
    }
    m_canvas.clear();
    
    return true;
}

OIIO_PLUGIN_NAMESPACE_END


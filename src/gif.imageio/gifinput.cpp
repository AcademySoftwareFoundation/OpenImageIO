// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <fcntl.h>
#include <memory>
#include <vector>

#include <gif_lib.h>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/thread.h>

// GIFLIB:
// http://giflib.sourceforge.net/
// Format description:
// http://giflib.sourceforge.net/whatsinagif/index.html

// for older giflib versions
#ifndef GIFLIB_MAJOR
#    error "GIFLIB 5.0 minimum required"
#endif

#ifndef DISPOSAL_UNSPECIFIED
#    define DISPOSAL_UNSPECIFIED 0
#endif

#ifndef DISPOSE_BACKGROUND
#    define DISPOSE_BACKGROUND 2
#endif



OIIO_PLUGIN_NAMESPACE_BEGIN

class GIFInput final : public ImageInput {
public:
    GIFInput() { init(); }
    ~GIFInput() override { close(); }
    const char* format_name(void) const override { return "gif"; }
    int supports(string_view feature) const override
    {
        return (feature == "ioproxy" || feature == "multiimage");
    }
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool close(void) override;
    bool seek_subimage(int subimage, int miplevel) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;

    int current_subimage(void) const override
    {
        lock_guard lock(*this);
        return m_subimage;
    }
    int current_miplevel(void) const override
    {
        // No mipmap support
        return 0;
    }

private:
    std::string m_filename;          ///< Stash the filename
    GifFileType* m_gif_file;         ///< GIFLIB handle
    int m_transparent_color;         ///< Transparent color index
    int m_subimage;                  ///< Current subimage index
    int m_disposal_method;           ///< Disposal method of current subimage.
                                     ///  Indicates what to do with canvas
                                     ///  before drawing the _next_ subimage.
    int m_previous_disposal_method;  ///< Disposal method of previous subimage.
                                     ///  Indicates what to do with canvas
                                     ///  before drawing _current_ subimage.
    std::vector<unsigned char> m_canvas;  ///< Image canvas in output format, on
                                          ///  which subimages are sequentially
                                          ///  drawn.

    /// Reset everything to initial state
    ///
    void init(void);

    /// Read current subimage metadata.
    ///
    bool read_subimage_metadata(ImageSpec& newspec);

    /// Read current subimage data (ie. draw it on canvas).
    ///
    bool read_subimage_data(void);

    /// Helper: read gif extension.
    ///
    void read_gif_extension(int ext_code, GifByteType* ext, ImageSpec& spec);

    /// Decode and return a real scanline index in the interlaced image.
    ///
    int decode_line_number(int line_number, int height);

    /// Print error message.
    ///
    void report_last_error(void);

    // Wrapper for GIF library to call that inputs from our IOProxy
    static int readFunc(GifFileType* gif, GifByteType* data, int bytes)
    {
        size_t size(bytes);
        auto inp    = reinterpret_cast<GIFInput*>(gif->UserData);
        auto io     = inp->ioproxy();
        auto result = io->read(data, size);
        if (result < size)
            inp->errorfmt(
                "GIF read error at position {}, asked for {} bytes, got {} (total size {})",
                io->tell() - result, size, result, io->size());
        return static_cast<int>(result);
    }
};



// Obligatory material to make this a recognizable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int gif_imageio_version = OIIO_PLUGIN_VERSION;
OIIO_EXPORT ImageInput*
gif_input_imageio_create()
{
    return new GIFInput;
}
OIIO_EXPORT const char* gif_input_extensions[] = { "gif", NULL };

OIIO_EXPORT const char*
gif_imageio_library_version()
{
    return "gif_lib " OIIO_STRINGIZE(GIFLIB_MAJOR) "." OIIO_STRINGIZE(
        GIFLIB_MINOR) "." OIIO_STRINGIZE(GIFLIB_RELEASE);
}

OIIO_PLUGIN_EXPORTS_END



void
GIFInput::init(void)
{
    m_gif_file = nullptr;
    ioproxy_clear();
}



bool
GIFInput::open(const std::string& name, ImageSpec& newspec)
{
    m_filename = name;
    m_subimage = -1;
    m_canvas.clear();

    if (seek_subimage(0, 0)) {
        newspec = spec();
        return true;
    } else {
        close();
        return false;
    }
}



bool
GIFInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    // Check 'config' for any special requests
    ioproxy_retrieve_from_config(config);
    ioseek(0);
    return open(name, newspec);
}



inline int
GIFInput::decode_line_number(int line_number, int height)
{
    if (1 < height && (height + 1) / 2 <= line_number)  // 4th tile 1/2 sized
        return 2 * (line_number - (height + 1) / 2) + 1;
    if (2 < height && (height + 3) / 4 <= line_number)  // 3rd tile 1/4 sized
        return 4 * (line_number - (height + 3) / 4) + 2;
    if (4 < height && (height + 7) / 8 <= line_number)  // 2nd tile 1/8 sized
        return 8 * (line_number - (height + 7) / 8) + 4;

    // 1st tile 1/8 sized
    return line_number * 8;
}



bool
GIFInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    if (y < 0 || y > m_spec.height || !m_canvas.size())
        return false;

    memcpy(data, &m_canvas[y * m_spec.width * m_spec.nchannels],
           m_spec.width * m_spec.nchannels);

    return true;
}



void
GIFInput::read_gif_extension(int ext_code, GifByteType* ext, ImageSpec& newspec)
{
    if (ext_code == GRAPHICS_EXT_FUNC_CODE) {
        // read background color index, disposal method and delay time between frames
        // http://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html#graphics_control_extension_block

        if (ext[1] & 0x01) {
            m_transparent_color = (int)ext[4];
        }

        m_disposal_method = (ext[1] & 0x1c) >> 2;

        int delay = (ext[3] << 8) | ext[2];
        if (delay) {
            int rat[2] = { 100, delay };
            newspec.attribute("FramesPerSecond", TypeRational, &rat);
            newspec.attribute("oiio:Movie", 1);
        }

    } else if (ext_code == COMMENT_EXT_FUNC_CODE) {
        // read comment data
        // http://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html#comment_extension_block
        std::string comment = std::string((const char*)&ext[1], int(ext[0]));
        newspec.attribute("ImageDescription", comment);

    } else if (ext_code == APPLICATION_EXT_FUNC_CODE) {
        // read loop count
        // http://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html#application_extension_block
        if (ext[0] == 3) {
            newspec.attribute("gif:LoopCount", (ext[3] << 8) | ext[2]);
            newspec.attribute("oiio:LoopCount", (ext[3] << 8) | ext[2]);
        }
    }
}



bool
GIFInput::read_subimage_metadata(ImageSpec& newspec)
{
    newspec           = ImageSpec(TypeDesc::UINT8);
    newspec.nchannels = 4;
    newspec.default_channel_names();
    newspec.alpha_channel = 4;
    newspec.set_colorspace("sRGB");

    m_previous_disposal_method = m_disposal_method;
    m_disposal_method          = DISPOSAL_UNSPECIFIED;

    m_transparent_color = -1;

    GifRecordType m_gif_rec_type;
    do {
        if (DGifGetRecordType(m_gif_file, &m_gif_rec_type) == GIF_ERROR) {
            report_last_error();
            return false;
        }

        switch (m_gif_rec_type) {
        case IMAGE_DESC_RECORD_TYPE:
            if (DGifGetImageDesc(m_gif_file) == GIF_ERROR) {
                report_last_error();
                return false;
            }

            break;

        case EXTENSION_RECORD_TYPE:
            int ext_code;
            GifByteType* ext;
            if (DGifGetExtension(m_gif_file, &ext_code, &ext) == GIF_ERROR) {
                report_last_error();
                return false;
            }
            if (ext != NULL) {
                read_gif_extension(ext_code, ext, newspec);
            }

            while (ext != NULL) {
                if (DGifGetExtensionNext(m_gif_file, &ext) == GIF_ERROR) {
                    report_last_error();
                    return false;
                }

                if (ext != NULL) {
                    read_gif_extension(ext_code, ext, newspec);
                }
            }

            break;

        case TERMINATE_RECORD_TYPE: return false; break;

        default: break;
        }
    } while (m_gif_rec_type != IMAGE_DESC_RECORD_TYPE);

    newspec.attribute("gif:Interlacing", m_gif_file->Image.Interlace ? 1 : 0);

    return true;
}



bool
GIFInput::read_subimage_data()
{
    GifColorType* colormap = NULL;
    int colormap_count;
    if (m_gif_file->Image.ColorMap) {  // local colormap
        colormap       = m_gif_file->Image.ColorMap->Colors;
        colormap_count = m_gif_file->Image.ColorMap->ColorCount;
    } else if (m_gif_file->SColorMap) {  // global colormap
        colormap       = m_gif_file->SColorMap->Colors;
        colormap_count = m_gif_file->SColorMap->ColorCount;
    } else {
        errorfmt("Neither local nor global colormap present.");
        return false;
    }

    if (m_subimage == 0 || m_previous_disposal_method == DISPOSE_BACKGROUND) {
        // make whole canvas transparent
        std::fill(m_canvas.begin(), m_canvas.end(), 0x00);
    }

    // decode scanline index if image is interlaced
    bool interlacing = m_spec.get_int_attribute("gif:Interlacing") != 0;

    // get subimage dimensions and draw it on canvas
    int window_height = m_gif_file->Image.Height;
    int window_width  = m_gif_file->Image.Width;
    int window_top    = m_gif_file->Image.Top;
    int window_left   = m_gif_file->Image.Left;
    std::unique_ptr<unsigned char[]> fscanline(new unsigned char[window_width]);
    for (int wy = 0; wy < window_height; wy++) {
        if (DGifGetLine(m_gif_file, &fscanline[0], window_width) == GIF_ERROR) {
            report_last_error();
            return false;
        }
        int y = window_top
                + (interlacing ? decode_line_number(wy, window_height) : wy);
        if (0 <= y && y < m_spec.height) {
            for (int wx = 0; wx < window_width; wx++) {
                if (fscanline[wx] >= colormap_count) {
                    errorfmt(
                        "Possible corruption: Encoded value {:d} @ ({},{}) exceeds palette size {}\n",
                        fscanline[wx], wx, y, colormap_count);
                    return false;
                }
                int x   = window_left + wx;
                int idx = m_spec.nchannels * (y * m_spec.width + x);
                if (0 <= x && x < m_spec.width
                    && fscanline[wx] != m_transparent_color) {
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
GIFInput::seek_subimage(int subimage, int miplevel)
{
    if (subimage < 0 || miplevel != 0)
        return false;

    if (m_subimage == subimage) {
        // We're already pointing to the right subimage
        return true;
    }

    if (m_subimage > subimage) {
        // requested subimage is located before the current one
        // file needs to be reopened
        if (m_gif_file && !close()) {
            return false;
        }
    }

    if (!m_gif_file) {
        if (!ioproxy_use_or_open(m_filename))
            return false;
        int giflib_error;
        m_gif_file = DGifOpen(this, readFunc, &giflib_error);
        if (!m_gif_file) {
            errorfmt("{}", GifErrorString(giflib_error));
            return false;
        }
        m_subimage = -1;
        m_canvas.resize(m_gif_file->SWidth * m_gif_file->SHeight * 4);
    }

    // skip subimages preceding the requested one
    if (m_subimage < subimage) {
        for (m_subimage += 1; m_subimage < subimage; m_subimage++) {
            if (!read_subimage_metadata(m_spec) || !read_subimage_data()) {
                return false;
            }
        }
    }

    // read metadata of current subimage
    if (!read_subimage_metadata(m_spec)) {
        return false;
    }

    m_spec.width       = m_gif_file->SWidth;
    m_spec.height      = m_gif_file->SHeight;
    m_spec.depth       = 1;
    m_spec.full_height = m_spec.height;
    m_spec.full_width  = m_spec.width;
    m_spec.full_depth  = m_spec.depth;

    m_subimage = subimage;

    // draw subimage on canvas
    if (!read_subimage_data()) {
        return false;
    }

    return true;
}



void
GIFInput::report_last_error(void)
{
    // GIFLIB_MAJOR >= 5 looks properly thread-safe, in that the error is
    // guaranteed to be specific to this open file.
    errorfmt("{}", GifErrorString(m_gif_file->Error));
}



inline bool
GIFInput::close(void)
{
    bool ok = true;
    if (m_gif_file) {
#if GIFLIB_MAJOR > 5 || (GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 1)
        if (DGifCloseFile(m_gif_file, NULL) == GIF_ERROR) {
#else
        if (DGifCloseFile(m_gif_file) == GIF_ERROR) {
#endif
            errorfmt("Error trying to close the file.");
            ok = false;
        }
        m_gif_file = nullptr;
    }
    m_canvas.clear();
    ioproxy_clear();
    return ok;
}

OIIO_PLUGIN_NAMESPACE_END

/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////



#ifndef IMAGEBUF_H
#define IMAGEBUF_H

#include "imageio.h"

namespace OpenImageIO {


class ImageBuf {
public:
    ImageBuf (const std::string &name);
    ImageBuf (const std::string &name, ImageIOFormatSpec &spec);
    virtual ~ImageBuf ();

    /// Allocate space the right size for an image described by the
    /// format spec.
    virtual bool alloc (ImageIOFormatSpec &spec);

    /// Read the file from disk.  Generally will skip the read if we've
    /// already got a current version of the image in memory, unless
    /// force==true.
    virtual bool read (int subimage=0, bool force=false,
                       OpenImageIO::ProgressCallback progress_callback=NULL,
                       void *progress_callback_data=NULL);

    /// Initialize this ImageBuf with the named image file, and read its
    /// header to fill out the spec correctly.  Return true if this
    /// succeeded, false if the file could not be read.  But don't
    /// allocate or read the pixels.
    virtual bool init_spec (const std::string &filename);

    /// Save the image or a subset thereof, with override for filename (""
    /// means use the original filename) and file format ("" indicates 
    /// to infer it from the filename).
    virtual bool save (const std::string &filename = std::string(),
                       const std::string &fileformat = std::string(),
                       OpenImageIO::ProgressCallback progress_callback=NULL,
                       void *progress_callback_data=NULL);

    /// Return info on the last error that occurred since error_message()
    /// was called.  This also clears the error message for next time.
    std::string error_message (void) {
        std::string e = m_err;
        m_err.clear();
        return e;
    }

    /// Return a reference to the image spec;
    ///
    const ImageIOFormatSpec & spec () const { return m_spec; }

    /// Return a pointer to the start of scanline #y.
    ///
    void *scanline (int y) {
        return (void *) (&m_pixels[y * m_spec.scanline_bytes()]);
    }

    const std::string & name (void) const { return m_name; }

    /// Return the index of the subimage are we currently viewing
    ///
    int subimage () const { return m_current_subimage; }

    /// Return the number of subimages in the file.
    ///
    int nsubimages () const { return m_nsubimages; }

    int nchannels () const { return m_spec.nchannels; }

    /// Retrieve a single channel of one pixel.
    ///
    float getchannel (int x, int y, int c) const;
    
    /// Retrieve the pixel value by x and y coordintes (on [0,res-1]),
    /// storing the floating point version in pixel[].  Retrieve at most
    /// maxchannels (will be clamped to the actual number of channels).
    void getpixel (int x, int y, float *pixel, int maxchannels=1000) const;

    /// Linearly interpolate at pixel coordinates (x,y), where (0,0) is
    /// the upper left corner, (xres,yres) the lower right corner of
    /// the pixel data.
    void interppixel (float x, float y, float *pixel) const;

    /// Linearly interpolate at pixel coordinates (x,y), where (0,0) is
    /// the upper left corner, (1,1) the lower right corner of the pixel
    /// data.
    void interppixel_NDC (float x, float y, float *pixel) const {
        interppixel (spec().x + x * spec().width,
                     spec().y + y * spec().height, pixel);
    }

    /// Set the pixel value by x and y coordintes (on [0,res-1]),
    /// from floating-point values in pixel[].  Set at most
    /// maxchannels (will be clamped to the actual number of channels).
    void setpixel (int x, int y, const float *pixel, int maxchannels=1000);

    int oriented_width () const;
    int oriented_height () const;
    int orientation () const { return m_orientation; }

    int xmin () const { return spec().x; }
    int xmax () const { return spec().x + spec().width - 1; }
    int ymin () const { return spec().y; }
    int ymax () const { return spec().y + spec().height - 1; }

    const void *pixeladdr (int x, int y) const {
        x -= spec().x;
        y -= spec().y;
        size_t p = y * m_spec.scanline_bytes() + x * m_spec.pixel_bytes();
        return &(m_pixels[p]);
    }

    void *pixeladdr (int x, int y) {
        x -= spec().x;
        y -= spec().y;
        size_t p = y * m_spec.scanline_bytes() + x * m_spec.pixel_bytes();
        return &(m_pixels[p]);
    }

protected:
    std::string m_name;        ///< Filename of the image
    int m_nsubimages;          ///< How many subimages are there?
    int m_current_subimage;    ///< Current subimage we're viewing
    ImageIOFormatSpec m_spec;  ///< Describes the image (size, etc)
    std::vector<char> m_pixels;  ///< Pixel data
//    char *m_thumbnail;         ///< Thumbnail image
    bool m_spec_valid;         ///< Is the spec valid
    bool m_pixels_valid;       ///< Image is valid
//    bool m_thumbnail_valid;    ///< Thumbnail is valid
    bool m_badfile;            ///< File not found
    std::string m_err;         ///< Last error message
//    float m_gamma;             ///< Gamma correction of this image
//    float m_exposure;          ///< Exposure gain of this image, in stops
    int m_orientation;         ///< Orientation of the image
//    mutable std::string m_shortinfo;
//    mutable std::string m_longinfo;

    // An ImageBuf can be in one of several states:
    //   * Uninitialized
    //         (m_name.empty())
    //   * Broken -- couldn't ever open the file
    //         (m_badfile == true)
    //   * Non-resident, ignorant -- know the name, nothing else
    //         (! m_name.empty() && ! m_badfile && ! m_spec_valid)
    //   * Non-resident, know spec, but the spec is valid
    //         (m_spec_valid && ! m_pixels)
    //   * Pixels loaded from disk, currently accurate
    //         (m_pixels && m_pixels_valid)

};


};  // namespace OpenImageIO


#endif // IMAGEBUF_H

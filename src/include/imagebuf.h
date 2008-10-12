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


/// \file
/// Classes for in-memory storage and simple manipulation of whole images,
/// which uses ImageInput and ImageOutput underneath for the file access.


#ifndef IMAGEBUF_H
#define IMAGEBUF_H

#include "imageio.h"

namespace OpenImageIO {


/// An ImageBuf is a simple in-memory representation of a 2D image.  It
/// uses ImageInput and ImageOutput underneath for its file I/O, and has
/// simple routines for setting and getting individual pixels, that
/// hides most of the details of memory layout and data representation
/// (translating to/from float automatically).
class ImageBuf {
public:
    /// Construct an named ImageBuf but allocated pixels.
    ///
    ImageBuf (const std::string &name);

    /// Construct an Imagebuf given both a name and a proposed spec
    /// describing the image size and type, and allocate storage for
    /// the pixels of the image (whose values will be undefined).
    ImageBuf (const std::string &name, const ImageSpec &spec);

    /// Destructor for an ImageBuf.
    ///
    virtual ~ImageBuf ();

    /// Allocate space the right size for an image described by the
    /// format spec.  If the ImageBuf already has allocated pixels,
    /// their values will not be preserved if the new spec does not
    /// describe an image of the same size and data type as it used
    /// to be.
    virtual void alloc (const ImageSpec &spec);

    /// Read the file from disk.  Generally will skip the read if we've
    /// already got a current version of the image in memory, unless
    /// force==true.  This uses ImageInput underneath, so will read any
    /// file format for which an appropriate imageio plugin can be found.
    virtual bool read (int subimage=0, bool force=false,
                       TypeDesc convert=TypeDesc::UNKNOWN,
                       OpenImageIO::ProgressCallback progress_callback=NULL,
                       void *progress_callback_data=NULL);

    /// Initialize this ImageBuf with the named image file, and read its
    /// header to fill out the spec correctly.  Return true if this
    /// succeeded, false if the file could not be read.  But don't
    /// allocate or read the pixels.
    virtual bool init_spec (const std::string &filename);

    /// Save the image or a subset thereof, with override for filename
    /// ("" means use the original filename) and file format ("" indicates
    /// to infer it from the filename).  This uses ImageOutput
    /// underneath, so will write any file format for which an
    /// appropriate imageio plugin can be found.
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

    /// Return a reference to the image spec.
    ///
    const ImageSpec & spec () const { return m_spec; }

    /// Return a pointer to the start of scanline #y.
    ///
    void *scanline (int y) {
        return (void *) (&m_pixels[y * m_spec.scanline_bytes()]);
    }

    /// Return the name of this image.
    ///
    const std::string & name (void) const { return m_name; }

    /// Return the name of the image file format of the disk file we
    /// read into this image.  Returns an empty string if this image
    /// was not the result of a read().
    const std::string & file_format_name (void) const { return m_fileformat; }

    /// Return the index of the subimage are we currently viewing
    ///
    int subimage () const { return m_current_subimage; }

    /// Return the number of subimages in the file.
    ///
    int nsubimages () const { return m_nsubimages; }

    /// Return the number of color channels in the image.
    ///
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

    int orientation () const { return m_orientation; }

    int oriented_width () const;
    int oriented_height () const;
    int oriented_x () const;
    int oriented_y () const;
    int oriented_full_width () const;
    int oriented_full_height () const;
    int oriented_full_x () const;
    int oriented_full_y () const;

    /// Return the minimum x coordinate of the defined image.
    ///
    int xmin () const { return spec().x; }

    /// Return the maximum x coordinate of the defined image.
    ///
    int xmax () const { return spec().x + spec().width - 1; }

    /// Return the minimum y coordinate of the defined image.
    ///
    int ymin () const { return spec().y; }

    /// Return the maximum y coordinate of the defined image.
    ///
    int ymax () const { return spec().y + spec().height - 1; }

    /// Return the address where pixel (x,y) is stored in the image buffer.
    /// Use with extreme caution!
    ///
    const void *pixeladdr (int x, int y) const {
        x -= spec().x;
        y -= spec().y;
        size_t p = y * m_spec.scanline_bytes() + x * m_spec.pixel_bytes();
        return &(m_pixels[p]);
    }

    /// Return the address where pixel (x,y) is stored in the image buffer.
    /// Use with extreme caution!
    ///
    void *pixeladdr (int x, int y) {
        x -= spec().x;
        y -= spec().y;
        size_t p = y * m_spec.scanline_bytes() + x * m_spec.pixel_bytes();
        return &(m_pixels[p]);
    }

    /// Zero out (set to 0, black) the entire image.
    ///
    void zero ();

protected:
    std::string m_name;          ///< Filename of the image
    std::string m_fileformat;    ///< File format name
    int m_nsubimages;            ///< How many subimages are there?
    int m_current_subimage;      ///< Current subimage we're viewing
    ImageSpec m_spec;            ///< Describes the image (size, etc)
    std::vector<char> m_pixels;  ///< Pixel data
    bool m_spec_valid;           ///< Is the spec valid
    bool m_pixels_valid;         ///< Image is valid
    bool m_badfile;              ///< File not found
    std::string m_err;           ///< Last error message
    int m_orientation;           ///< Orientation of the image

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

    void realloc ();
};


};  // namespace OpenImageIO


#endif // IMAGEBUF_H

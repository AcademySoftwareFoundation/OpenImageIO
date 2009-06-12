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
#include "fmath.h"

namespace OpenImageIO {


/// An ImageBuf is a simple in-memory representation of a 2D image.  It
/// uses ImageInput and ImageOutput underneath for its file I/O, and has
/// simple routines for setting and getting individual pixels, that
/// hides most of the details of memory layout and data representation
/// (translating to/from float automatically).
class ImageBuf {
public:
    /// Construct an ImageBuf without allocated pixels.
    ///
    ImageBuf (const std::string &name = std::string());

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
    /// Return value is true if all is ok, otherwise false.
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
                       void *progress_callback_data=NULL) const;

    /// Write the image to the open ImageOutput 'out'.  Return true if
    /// all went ok, false if there were errors writing.  It does NOT
    /// close the file when it's done (and so may be called in a loop to
    /// write a multi-image file).
    virtual bool write (ImageOutput *out,
                        OpenImageIO::ProgressCallback progress_callback=NULL,
                        void *progress_callback_data=NULL) const;

    /// Return info on the last error that occurred since error_message()
    /// was called.  This also clears the error message for next time.
    std::string error_message (void) {
        std::string e = m_err;
        m_err.clear();
        return e;
    }

    /// Return a read-only (const) reference to the image spec.
    ///
    const ImageSpec & spec () const { return m_spec; }

    /// Return the name of this image.
    ///
    const std::string & name (void) const { return m_name.string(); }

    /// Return the name of the image file format of the disk file we
    /// read into this image.  Returns an empty string if this image
    /// was not the result of a read().
    const std::string & file_format_name (void) const { return m_fileformat.string(); }

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

    /// Retrieve the i-th pixel of the image (out of width*height*depth),
    /// storing the floating point version in pixel[].  Retrieve at most
    /// maxchannels (will be clamped to the actual number of channels).
    void getpixel (int i, float *pixel, int maxchannels=1000) const;

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

    /// Set the i-th pixel value of the image (out of width*height*depth),
    /// from floating-point values in pixel[].  Set at most
    /// maxchannels (will be clamped to the actual number of channels).
    void setpixel (int i, const float *pixel, int maxchannels=1000);

    /// Retrieve the rectangle of pixels spanning [xbegin..xend) X
    /// [ybegin..yend) (with exclusive 'end'), specified as integer
    /// pixel coordinates, at the current MIP-map level, storing the
    /// pixel values beginning at the address specified by result.  It
    /// is up to the caller to ensure that result points to an area of
    /// memory big enough to accommodate the requested rectangle.
    /// Return true if the operation could be completed, otherwise
    /// return false.
    bool copy_pixels (int xbegin, int xend, int ybegin, int yend,
                      TypeDesc format, void *result) const;

    /// Even safer version of copy_pixels: Retrieve the rectangle of
    /// pixels spanning [xbegin..xend) X [ybegin..yend) (with exclusive
    /// 'end'), specified as integer pixel coordinates, at the current
    /// MIP-map level, storing the pixel values in the 'result' vector
    /// (even allocating the right size).  Return true if the operation
    /// could be completed, otherwise return false.
    template<typename T>
    bool copy_pixels (int xbegin, int xend, int ybegin, int yend,
                      std::vector<T> &result) const
    {
        result.resize (nchannels() * ((yend-ybegin)*(xend-xbegin)));
        return _copy_pixels (xbegin, xend, ybegin, yend, &result[0]);
    }

    int orientation () const { return m_orientation; }

    int oriented_width () const;
    int oriented_height () const;
    int oriented_x () const;
    int oriented_y () const;
    int oriented_full_width () const;
    int oriented_full_height () const;
    int oriented_full_x () const;
    int oriented_full_y () const;

    /// Return the beginning (minimum) x coordinate of the defined image.
    ///
    int xbegin () const { return spec().x; }

    /// Return the end (one past maximum) x coordinate of the defined image.
    ///
    int xend () const { return spec().x + spec().width; }

    /// Return the beginning (minimum) y coordinate of the defined image.
    ///
    int ybegin () const { return spec().y; }

    /// Return the end (one past maximum) y coordinate of the defined image.
    ///
    int yend () const { return spec().y + spec().height; }

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

    /// Zero out (set to 0, black) the entire image.
    ///
    void zero ();

    bool pixels_valid (void) const { return m_pixels_valid; }

    /// Templated class for referring to an individual pixel in an
    /// ImageBuf, iterating over the pixels of an ImageBuf, or iterating
    /// over the pixels of a specified region of the ImageBuf
    /// [xbegin..xend) X [ybegin..yend).  It is templated on BUFT, the
    /// type known to be in the internal representation of the ImageBuf,
    /// and USERT, the type that the user wants to retrieve or set the
    /// data (defaulting to float).  the whole idea is to allow this:
    /// \code
    ///   ImageBuf img (...);
    ///   ImageBuf::Iterator<float> pixel (img, 0, 512, 0, 512);
    ///   for (  ;  pixel.valid();  ++pixel) {
    ///       for (int c = 0;  c < img.nchannels();  ++c) {
    ///           float x = (*pixel)[c];
    ///           (*pixel)[c] = ...;
    ///       }
    ///   }
    /// \endcode
    ///
    template<typename BUFT, typename USERT=float>
    class Iterator {
    public:
        /// Construct from just an ImageBuf -- iterate over the whole
        /// region, starting with the upper left pixel of the region.
        Iterator (ImageBuf &ib)
            : m_ib(&ib), m_xbegin(ib.xbegin()), m_ybegin(ib.ybegin()),
              m_xend(ib.xend()), m_yend(ib.yend())
          { pos (m_xbegin,m_ybegin); }
        /// Construct from an ImageBuf and a specific pixel index..
        ///
        Iterator (ImageBuf &ib, int x, int y)
            : m_ib(&ib), m_xbegin(ib.xbegin()), m_ybegin(ib.ybegin()),
              m_xend(ib.xend()), m_yend(ib.yend())
          { pos (x, y); }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel.
        Iterator (ImageBuf &ib, int xbegin, int ybegin, int xend, int yend)
            : m_ib(&ib), m_xbegin(std::max(xbegin,ib.xbegin())), 
              m_ybegin(std::max(ybegin,ib.ybegin())),
              m_xend(std::min(xend,ib.xend())),
              m_yend(std::min(yend,ib.yend()))
          { pos (m_xbegin, m_ybegin); }

        /// Explicitly point the iterator.  This results in an invalid
        /// iterator if outside the previously-designated region.
        void pos (int x, int y) {
            m_x = x;  m_y = y;
            m_proxy.set (valid() ? (BUFT *)m_ib->pixeladdr (m_x, m_y) : NULL);
            // FIXME -- this is ok for now, but when ImageBuf is backed by
            // ImageCache, this should point to the current tile!
        }

        /// Increment to the next pixel in the region.
        ///
        void operator++ () {
            if (++m_x >= m_xend) {
                m_x = m_xbegin;
                ++m_y;
            }
            pos (m_x, m_y);
            // FIXME -- we could do the traversal in an even more coherent
            // pattern, and this may help when we're backed by ImageCache.
        }
        /// Increment to the next pixel in the region.
        ///
        void operator++ (int) {
            (*this)++;
        }

        /// Retrieve the current x location of the iterator.
        ///
        int x () const { return m_x; }
        /// Retrieve the current y location of the iterator.
        ///
        int y () const { return m_y; }

        /// Is the current location valid?  Locations outside the
        /// designated region are invalid, as is an iterator that has
        /// completed iterating over the whole region.
        bool valid () const {
            return (m_x >= m_xbegin && m_x < m_xend &&
                    m_y >= m_ybegin && m_y < m_yend);
        }

        /// Dereferencing the iterator gives us a proxy for the pixel,
        /// which we can index for reading or assignment.
        DataArrayProxy<BUFT,USERT>& operator* () { return m_proxy; }

        /// Array indexing retrieves the value of the i-th channel of
        /// the current pixel.
        USERT operator[] (int i) const { return m_proxy[i]; } 

        /// Array referencing retrieve a proxy (which may be "assigned
        /// to") of i-th channel of the current pixel, so that this
        /// works: me[i] = val;
        DataProxy<BUFT,USERT> operator[] (int i) { return m_proxy[i]; } 

        void * rawptr () const { return m_proxy.get(); }

    private:
        ImageBuf *m_ib;
        int m_xbegin, m_xend, m_ybegin, m_yend;
        int m_x, m_y;
        DataArrayProxy<BUFT,USERT> m_proxy;
        // FIXME -- this is ok for now, but when ImageBuf is backed by
        // ImageCache, this should hold and keep track of the current tile.
    };


    /// Just like an ImageBuf::Iterator, except that it refers to a
    /// const ImageBuf.
    template<typename BUFT, typename USERT=float>
    class ConstIterator {
    public:
        /// Construct from just an ImageBuf -- iterate over the whole
        /// region, starting with the upper left pixel of the region.
        ConstIterator (const ImageBuf &ib)
            : m_ib(&ib), m_xbegin(ib.xbegin()), m_xend(ib.xend()), 
              m_ybegin(ib.ybegin()), m_yend(ib.yend())
          { pos (m_xbegin,m_ybegin); }
        /// Construct from an ImageBuf and a specific pixel index..
        ///
        ConstIterator (const ImageBuf &ib, int x, int y)
            : m_ib(&ib), m_xbegin(ib.xbegin()), m_xend(ib.xend()),
              m_ybegin(ib.ybegin()), m_yend(ib.yend())
          { pos (x, y); }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel.
        ConstIterator (const ImageBuf &ib, int xbegin, int xend,
                       int ybegin, int yend)
            : m_ib(&ib), m_xbegin(std::max(xbegin,ib.xbegin())), 
              m_xend(std::min(xend,ib.xend())),
              m_ybegin(std::max(ybegin,ib.ybegin())),
              m_yend(std::min(yend,ib.yend()))
          { pos (m_xbegin, m_ybegin); }

        /// Explicitly point the iterator.  This results in an invalid
        /// iterator if outside the previously-designated region.
        void pos (int x, int y) {
            m_x = x;  m_y = y;
            m_proxy.set (valid() ? (BUFT *)m_ib->pixeladdr (m_x, m_y) : NULL);
            // FIXME -- this is ok for now, but when ImageBuf is backed by
            // ImageCache, this should point to the current tile!
        }

        /// Increment to the next pixel in the region.
        ///
        void operator++ () {
            if (++m_x >= m_xend) {
                m_x = m_xbegin;
                ++m_y;
            }
            pos (m_x, m_y);
            // FIXME -- we could do the traversal in an even more coherent
            // pattern, and this may help when we're backed by ImageCache.
        }
        /// Increment to the next pixel in the region.
        ///
        void operator++ (int) {
            (*this)++;
        }

        /// Retrieve the current x location of the iterator.
        ///
        int x () const { return m_x; }
        /// Retrieve the current y location of the iterator.
        ///
        int y () const { return m_y; }

        /// Is the current location valid?  Locations outside the
        /// designated region are invalid, as is an iterator that has
        /// completed iterating over the whole region.
        bool valid () const {
            return (m_x >= m_xbegin && m_x < m_xend &&
                    m_y >= m_ybegin && m_y < m_yend);
        }

        /// Dereferencing the iterator gives us a proxy for the pixel,
        /// which we can index for reading or assignment.
        ConstDataArrayProxy<BUFT,USERT>& operator* () const { return m_proxy; }

        /// Array indexing retrieves the value of the i-th channel of
        /// the current pixel.
        USERT operator[] (int i) const { return m_proxy[i]; } 

        const void * rawptr () const { return m_proxy.get(); }

    private:
        const ImageBuf *m_ib;
        int m_xbegin, m_xend, m_ybegin, m_yend;
        int m_x, m_y;
        ConstDataArrayProxy<BUFT,USERT> m_proxy;
        // FIXME -- this is ok for now, but when ImageBuf is backed by
        // ImageCache, this should hold and keep track of the current tile.
    };


protected:
    ustring m_name;              ///< Filename of the image
    ustring m_fileformat;        ///< File format name
    int m_nsubimages;            ///< How many subimages are there?
    int m_current_subimage;      ///< Current subimage we're viewing
    ImageSpec m_spec;            ///< Describes the image (size, etc)
    std::vector<char> m_pixels;  ///< Pixel data
    bool m_spec_valid;           ///< Is the spec valid
    bool m_pixels_valid;         ///< Image is valid
    bool m_badfile;              ///< File not found
    mutable std::string m_err;   ///< Last error message
    int m_orientation;           ///< Orientation of the image
    float m_pixelaspect;         ///< Pixel aspect ratio of the image

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

    // This is the specialization of _copy_pixels, fully specialized by
    // both source (ImageBuffer contents) and destination (user) types.
    template<typename S, typename D>
    void _copy_pixels2 (int xbegin, int xend, int ybegin, int yend, D *r) const
    {
        int w = (xend-xbegin);
        for (ImageBuf::ConstIterator<S,D> p (*this, xbegin, xend, ybegin, yend);
             p.valid(); ++p) {
            imagesize_t offset = ((p.y()-ybegin)*w + (p.x()-xbegin)) * nchannels();
            for (int c = 0;  c < nchannels();  ++c)
                r[offset+c] = p[c];
        }
    }

    // This is the specialization of copy_pixels, already specialized
    // for the user's destination type.  Now do more template magic to
    // further specialize by the data type of the buffer we're reading
    // from.
    template<typename D>
    bool _copy_pixels (int xbegin, int xend, int ybegin, int yend, D *r) const
    {
        // Caveat: serious hack here.  To avoid duplicating code, use a
        // #define.  Furthermore, exploit the CType<> template to construct
        // the right C data type for the given BASETYPE.
#define TYPECASE(B) \
        case B : _copy_pixels2<CType<B>::type,D>(xbegin, xend, ybegin, yend, (D *)r); return true

        switch (spec().format.basetype) {
            TYPECASE (TypeDesc::UINT8);
            TYPECASE (TypeDesc::INT8);
            TYPECASE (TypeDesc::UINT16);
            TYPECASE (TypeDesc::INT16);
            TYPECASE (TypeDesc::UINT);
            TYPECASE (TypeDesc::INT);
#ifdef _HALF_H_
            TYPECASE (TypeDesc::HALF);
#endif
            TYPECASE (TypeDesc::FLOAT);
            TYPECASE (TypeDesc::DOUBLE);
        }
        return false;
#undef TYPECASE
    }

    // Return the address where pixel (x,y) is stored in the image buffer.
    // Use with extreme caution!
    const void *pixeladdr (int x, int y) const;

    // Return the address where pixel (x,y) is stored in the image buffer.
    // Use with extreme caution!
    void *pixeladdr (int x, int y);
};



//////////////////////////////////////////////////////////////////////////
// Implementation of ImageBuf internal methods, including 
// ImageBuf::Iterator
//



};  // namespace OpenImageIO


#endif // IMAGEBUF_H

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


#ifndef OPENIMAGEIO_IMAGEBUF_H
#define OPENIMAGEIO_IMAGEBUF_H

#include "imageio.h"
#include "fmath.h"
#include "imagecache.h"
#include "colortransfer.h"

#ifdef OPENIMAGEIO_NAMESPACE
namespace OPENIMAGEIO_NAMESPACE {
#endif

namespace OpenImageIO {


/// An ImageBuf is a simple in-memory representation of a 2D image.  It
/// uses ImageInput and ImageOutput underneath for its file I/O, and has
/// simple routines for setting and getting individual pixels, that
/// hides most of the details of memory layout and data representation
/// (translating to/from float automatically).
class DLLPUBLIC ImageBuf {
public:
    /// Construct an ImageBuf to read the named image.  
    /// If name is the empty string (the default), it's a completely
    /// uninitialized ImageBuf.
    ImageBuf (const std::string &name = std::string(),
              ImageCache *imagecache = NULL);

    /// Construct an Imagebuf given both a name and a proposed spec
    /// describing the image size and type, and allocate storage for
    /// the pixels of the image (whose values will be undefined).
    ImageBuf (const std::string &name, const ImageSpec &spec);

    /// Destructor for an ImageBuf.
    ///
    virtual ~ImageBuf ();

    /// Restore the ImageBuf to an uninitialized state.
    ///
    virtual void clear ();

    /// Forget all previous info, reset this ImageBuf to a new image.
    ///
    virtual void reset (const std::string &name = std::string(),
                        ImageCache *imagecache = NULL);

    /// Forget all previous info, reset this ImageBuf to a blank
    /// image of the given name and dimensions.
    virtual void reset (const std::string &name, const ImageSpec &spec);

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

    /// Return info on the last error that occurred since geterror()
    /// was called.  This also clears the error message for next time.
    std::string geterror (void) const {
        std::string e = m_err;
        m_err.clear();
        return e;
    }
    /// Deprecated
    ///
    std::string error_message () const { return geterror (); }

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

    /// Linearly interpolate at pixel coordinates (x,y), where (0,0) is
    /// the upper left corner, (xres,yres) the lower right corner of
    /// the pixel data.
    void interppixel (float x, float y, float *pixel) const;

    /// Linearly interpolate at pixel coordinates (x,y), where (0,0) is
    /// the upper left corner of the pixel data window, (1,1) the lower
    /// right corner of the pixel data.
    void interppixel_NDC (float x, float y, float *pixel) const {
        interppixel (spec().x + x * spec().width,
                     spec().y + y * spec().height, pixel);
    }

    /// Linearly interpolate at pixel coordinates (x,y), where (0,0) is
    /// the upper left corner of the display window, (1,1) the lower
    /// right corner of the display window.
    void interppixel_NDC_full (float x, float y, float *pixel) const {
        interppixel (spec().full_x + x * spec().full_width,
                     spec().full_y + y * spec().full_height, pixel);
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

    /// Retrieve the rectangle of pixels spanning [xbegin..xend) X
    /// [ybegin..yend) (with exclusive 'end'), specified as integer
    /// pixel coordinates, at the current MIP-map level, storing the
    /// pixel values beginning at the address specified by result,
    /// converting to the type <T> in the process.  It is up to the
    /// caller to ensure that result points to an area of memory big
    /// enough to accommodate the requested rectangle.  Return true if
    /// the operation could be completed, otherwise return false.
    template<typename T>
    bool copy_pixels (int xbegin, int xend, int ybegin, int yend,
                      T *result) const;

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

    /// Apply a color transfer function to the pixels (in place).
    ///
    void transfer_pixels (ColorTransfer *tfunc);

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

    /// Fill the entire image with the given pixel value.
    ///
    void fill (const float *pixel);

    /// Fill a subregion of the image with the given pixel value.  The
    /// subregion is bounded by [xbegin..xend) X [ybegin..yend).
    void fill (const float *pixel, int xbegin, int xend, int ybegin, int yend);
 
    bool pixels_valid (void) const { return m_pixels_valid; }

    bool localpixels () const { return m_localpixels; }
    ImageCache *imagecache () const { return m_imagecache; }

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
              m_xend(ib.xend()), m_yend(ib.yend()), m_tile(NULL)
          { pos (m_xbegin,m_ybegin); }
        /// Construct from an ImageBuf and a specific pixel index..
        ///
        Iterator (ImageBuf &ib, int x, int y)
            : m_ib(&ib), m_xbegin(ib.xbegin()), m_ybegin(ib.ybegin()),
              m_xend(ib.xend()), m_yend(ib.yend()), m_tile(NULL)
          { pos (x, y); }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel.
        Iterator (ImageBuf &ib, int xbegin, int ybegin, int xend, int yend)
            : m_ib(&ib), m_xbegin(std::max(xbegin,ib.xbegin())), 
              m_ybegin(std::max(ybegin,ib.ybegin())),
              m_xend(std::min(xend,ib.xend())),
              m_yend(std::min(yend,ib.yend())), m_tile(NULL)
          { pos (m_xbegin, m_ybegin); }
        Iterator (const Iterator &i)
            : m_ib (i.m_ib), m_xbegin(i.m_xbegin), m_xend(i.m_xend), 
              m_ybegin(i.m_ybegin), m_yend(i.m_yend), m_tile(NULL)
        {
            pos (i.m_x, i.m_y);
        }
        ~Iterator () {
            if (m_tile)
                m_ib->imagecache()->release_tile (m_tile);
        }

        /// Explicitly point the iterator.  This results in an invalid
        /// iterator if outside the previously-designated region.
        void pos (int x, int y) {
            if (! valid(x,y))
                m_proxy.set (NULL);
            else if (m_ib->localpixels())
                m_proxy.set ((BUFT *)m_ib->pixeladdr (x, y));
            else
                m_proxy.set ((BUFT *)m_ib->retile (m_ib->subimage(), x, y,
                                         m_tile, m_tilexbegin, m_tileybegin));
            m_x = x;  m_y = y;
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

        /// Assign one Iterator to another
        ///
        const Iterator & operator= (const Iterator &i) {
            if (m_tile)
                m_ib->imagecache()->release_tile (m_tile);
            m_tile = NULL;
            m_ib = i.m_ib;
            m_xbegin = i.m_xbegin;  m_xend = i.m_xend;
            m_ybegin = i.m_ybegin;  m_yend = i.m_yend;
            pos (i.m_x, i.m_y);
            return *this;
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

        /// Is the location (x,y) valid?  Locations outside the
        /// designated region are invalid, as is an iterator that has
        /// completed iterating over the whole region.
        bool valid (int x, int y) const {
            return (x >= m_xbegin && x < m_xend &&
                    y >= m_ybegin && y < m_yend);
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
        int m_xbegin, m_ybegin, m_xend, m_yend;
        int m_x, m_y;
        DataArrayProxy<BUFT,USERT> m_proxy;
        ImageCache::Tile *m_tile;
        int m_tilexbegin, m_tileybegin;
    };


    /// Just like an ImageBuf::Iterator, except that it refers to a
    /// const ImageBuf.  If BUFT == void, 
    template<typename BUFT, typename USERT=float>
    class ConstIterator {
    public:
        /// Construct from just an ImageBuf -- iterate over the whole
        /// region, starting with the upper left pixel of the region.
        ConstIterator (const ImageBuf &ib)
            : m_ib(&ib), m_xbegin(ib.xbegin()), m_xend(ib.xend()), 
              m_ybegin(ib.ybegin()), m_yend(ib.yend()), m_tile(NULL)
          { pos (m_xbegin,m_ybegin); }
        /// Construct from an ImageBuf and a specific pixel index..
        ///
        ConstIterator (const ImageBuf &ib, int x, int y)
            : m_ib(&ib), m_xbegin(ib.xbegin()), m_xend(ib.xend()),
              m_ybegin(ib.ybegin()), m_yend(ib.yend()), m_tile(NULL)
          { pos (x, y); }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel.
        ConstIterator (const ImageBuf &ib, int xbegin, int xend,
                       int ybegin, int yend)
            : m_ib(&ib), m_xbegin(std::max(xbegin,ib.xbegin())), 
              m_xend(std::min(xend,ib.xend())),
              m_ybegin(std::max(ybegin,ib.ybegin())),
              m_yend(std::min(yend,ib.yend())), m_tile(NULL)
          { pos (m_xbegin, m_ybegin); }
        ConstIterator (const ConstIterator &i)
            : m_ib (i.m_ib), m_xbegin(i.m_xbegin), m_xend(i.m_xend), 
              m_ybegin(i.m_ybegin), m_yend(i.m_yend), m_tile(NULL)
        {
            pos (i.m_x, i.m_y);
        }

        ~ConstIterator () {
            if (m_tile)
                m_ib->imagecache()->release_tile (m_tile);
        }

        /// Explicitly point the iterator.  This results in an invalid
        /// iterator if outside the previously-designated region.
        void pos (int x, int y) {
            if (! valid(x,y))
                m_proxy.set (NULL);
            else if (m_ib->localpixels())
                m_proxy.set ((BUFT *)m_ib->pixeladdr (x, y));
            else
                m_proxy.set ((BUFT *)m_ib->retile (m_ib->subimage(), x, y,
                                         m_tile, m_tilexbegin, m_tileybegin));
            m_x = x;  m_y = y;
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

        /// Assign one ConstIterator to another
        ///
        const ConstIterator & operator= (const ConstIterator &i) {
            if (m_tile)
                m_ib->imagecache()->release_tile (m_tile);
            m_tile = NULL;
            m_ib = i.m_ib;
            m_xbegin = i.m_xbegin;  m_xend = i.m_xend;
            m_ybegin = i.m_ybegin;  m_yend = i.m_yend;
            pos (i.m_x, i.m_y);
            return *this;
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

        /// Is the location (x,y) valid?  Locations outside the
        /// designated region are invalid, as is an iterator that has
        /// completed iterating over the whole region.
        bool valid (int x, int y) const {
            return (x >= m_xbegin && x < m_xend &&
                    y >= m_ybegin && y < m_yend);
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
        ImageCache::Tile *m_tile;
        int m_tilexbegin, m_tileybegin;
    };


protected:
    ustring m_name;              ///< Filename of the image
    ustring m_fileformat;        ///< File format name
    int m_nsubimages;            ///< How many subimages are there?
    int m_current_subimage;      ///< Current subimage we're viewing
    ImageSpec m_spec;            ///< Describes the image (size, etc)
    std::vector<char> m_pixels;  ///< Pixel data
    bool m_localpixels;          ///< Pixels are local, in m_pixels
    bool m_spec_valid;           ///< Is the spec valid
    bool m_pixels_valid;         ///< Image is valid
    bool m_badfile;              ///< File not found
    mutable std::string m_err;   ///< Last error message
    int m_orientation;           ///< Orientation of the image
    float m_pixelaspect;         ///< Pixel aspect ratio of the image
    ImageCache *m_imagecache;    ///< ImageCache to use
    TypeDesc m_cachedpixeltype;  ///< Data type stored in the cache

    void realloc ();

    // Return the address where pixel (x,y) is stored in the image buffer.
    // Use with extreme caution!
    const void *pixeladdr (int x, int y) const;

    // Return the address where pixel (x,y) is stored in the image buffer.
    // Use with extreme caution!
    void *pixeladdr (int x, int y);

    // Reset the ImageCache::Tile * to reserve and point to the correct
    // tile for the given pixel, and return the ptr to the actual pixel
    // within the tile.
    const void * retile (int subimage, int x, int y, ImageCache::Tile* &tile,
                         int &tilexbegin, int &tileybegin) const;
};



namespace ImageBufAlgo {

/// Add the pixels of two images A and B, putting the sum in dst.
/// The 'options' flag controls behaviors, particular of what happens
/// when A, B, and dst have differing data windows.  Note that dst must
/// not be the same image as A or B, and all three images must have the
/// same number of channels.  A and B *must* be float images.

bool DLLPUBLIC add (ImageBuf &dst, const ImageBuf &A, const ImageBuf &B, int options=0);

/// Enum describing options to be passed to ImageBufAlgo::add.
/// Multiple options are allowed simultaneously by "or'ing" together.
enum DLLPUBLIC AddOptions
{
    ADD_DEFAULT = 0,
    ADD_RETAIN_DST = 1,     ///< Retain dst pixels outside the region
    ADD_CLEAR_DST = 0,      ///< Default: clear all the dst pixels first
    ADD_RETAIN_WINDOWS = 2, ///< Honor the existing windows
    ADD_ALIGN_WINDOWS = 0,  ///< Default: align the windows before adding
};



/// Copy a crop window of src to dst.  The crop region is bounded by
/// [xbegin..xend) X [ybegin..yend), with the pixels affected including
/// begin but not including the end pixel (just like STL ranges).  The
/// cropping can be done one of several ways, specified by the options
/// parameter, one of: CROP_CUT, CROP_WINDOW, CROP_BLACK, CROP_WHITE,
/// CROP_TRANS.
bool DLLPUBLIC crop (ImageBuf &dst, const ImageBuf &src,
           int xbegin, int xend, int ybegin, int yend, int options);

enum DLLPUBLIC CropOptions 
{
    CROP_CUT, 	  ///< cut out a pixel region to make a new image at the origin
    CROP_WINDOW,  ///< reduce the pixel data window, keep in the same position
    CROP_BLACK,	  ///< color to black all the pixels outside of the bounds
    CROP_WHITE,	  ///< color to white all the pixels outside of the bounds
    CROP_TRANS	  ///< make all pixels out of bounds transparent (zero)
};



/// Apply a transfer function to the pixel values.
///
bool DLLPUBLIC colortransfer (ImageBuf &dst, const ImageBuf &src,
                              ColorTransfer *tfunc);

};  // end namespace ImageBufAlgo


};  // namespace OpenImageIO

#ifdef OPENIMAGEIO_NAMESPACE
}; // end namespace OPENIMAGEIO_NAMESPACE
using namespace OPENIMAGEIO_NAMESPACE;
#endif

#endif // OPENIMAGEIO_IMAGEBUF_H

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

#if defined(_MSC_VER)
// Ignore warnings about DLL exported classes with member variables that are template classes.
// This happens with the std::vector and std::string protected members of ImageBuf below.
#  pragma warning (disable : 4251)
#endif

#include "imageio.h"
#include "fmath.h"
#include "imagecache.h"
#include "dassert.h"


OIIO_NAMESPACE_ENTER
{

class ImageBuf;


/// Helper struct describing a region of interest in an image.
/// The region is [xbegin,xend) x [begin,yend) x [zbegin,zend),
/// with the "end" designators signifying one past the last pixel,
/// a la STL style.
struct ROI {
    int xbegin, xend, ybegin, yend, zbegin, zend;
    int chbegin, chend;

    /// Default constructor is an undefined region.
    ///
    ROI () : xbegin(0), xend(-1), chbegin(0), chend(1000) { }

    /// Constructor with an explicitly defined region.
    ///
    ROI (int xbegin, int xend, int ybegin, int yend,
         int zbegin=0, int zend=1, int chbegin=0, int chend=10000)
        : xbegin(xbegin), xend(xend), ybegin(ybegin), yend(yend),
          zbegin(zbegin), zend(zend), chbegin(chbegin), chend(chend)
    { }

    /// Is a region defined?
    bool defined () const { return (xbegin <= xend); }

    // Region dimensions.
    int width () const { return xend - xbegin; }
    int height () const { return yend - ybegin; }
    int depth () const { return zend - zbegin; }
    int nchannels () const { return chend - chbegin; }

    /// Total number of pixels in the region.
    imagesize_t npixels () const {
        if (! defined())
            return 0;
        imagesize_t w = width(), h = height(), d = depth();
        return w*h*d;
    }

};


/// Union of two regions, the smallest region containing both.
OIIO_API ROI roi_union (const ROI &A, const ROI &B);

/// Intersection of two regions.
OIIO_API ROI roi_intersection (const ROI &A, const ROI &B);

/// Return pixel data window for this ImageSpec as a ROI.
OIIO_API ROI get_roi (const ImageSpec &spec);

/// Return full/display window for this ImageSpec as a ROI.
OIIO_API ROI get_roi_full (const ImageSpec &spec);

/// Set pixel data window for this ImageSpec to a ROI.
/// Does NOT change the channels of the spec, regardless of newroi.
OIIO_API void set_roi (ImageSpec &spec, const ROI &newroi);

/// Set full/display window for this ImageSpec to a ROI.
/// Does NOT change the channels of the spec, regardless of newroi.
OIIO_API void set_roi_full (ImageSpec &spec, const ROI &newroi);



class ImageBufImpl;   // Opaque pointer



/// An ImageBuf is a simple in-memory representation of a 2D image.  It
/// uses ImageInput and ImageOutput underneath for its file I/O, and has
/// simple routines for setting and getting individual pixels, that
/// hides most of the details of memory layout and data representation
/// (translating to/from float automatically).
class OIIO_API ImageBuf {
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

    /// Construct an ImageBuf that "wraps" a memory buffer owned by the
    /// calling application.  It can write pixels to this buffer, but
    /// can't change its resolution or data type.
    ImageBuf (const std::string &name, const ImageSpec &spec, void *buffer);

    /// Construct a copy of an ImageBuf.
    ///
    ImageBuf (const ImageBuf &src);

    /// Destructor for an ImageBuf.
    ///
    ~ImageBuf ();

    /// Restore the ImageBuf to an uninitialized state.
    ///
    void clear ();

    /// Forget all previous info, reset this ImageBuf to a new image
    /// that is uninitialized (no pixel values, no size or spec).
    void reset (const std::string &name, ImageCache *imagecache = NULL);

    /// Forget all previous info, reset this ImageBuf to a blank
    /// image of the given name and dimensions.
    void reset (const std::string &name, const ImageSpec &spec);

    /// Copy spec to *this, and then allocate enough space the right
    /// size for an image described by the format spec.  If the ImageBuf
    /// already has allocated pixels, their values will not be preserved
    /// if the new spec does not describe an image of the same size and
    /// data type as it used to be.
    void alloc (const ImageSpec &spec);

    /// Read the file from disk.  Generally will skip the read if we've
    /// already got a current version of the image in memory, unless
    /// force==true.  This uses ImageInput underneath, so will read any
    /// file format for which an appropriate imageio plugin can be found.
    /// Return value is true if all is ok, otherwise false.
    bool read (int subimage=0, int miplevel=0, bool force=false,
               TypeDesc convert=TypeDesc::UNKNOWN,
               ProgressCallback progress_callback=NULL,
               void *progress_callback_data=NULL);

    /// Initialize this ImageBuf with the named image file, and read its
    /// header to fill out the spec correctly.  Return true if this
    /// succeeded, false if the file could not be read.  But don't
    /// allocate or read the pixels.
    bool init_spec (const std::string &filename,
                    int subimage, int miplevel);

    /// Save the image or a subset thereof, with override for filename
    /// ("" means use the original filename) and file format ("" indicates
    /// to infer it from the filename).  This uses ImageOutput
    /// underneath, so will write any file format for which an
    /// appropriate imageio plugin can be found.
    bool save (const std::string &filename = std::string(),
               const std::string &fileformat = std::string(),
               ProgressCallback progress_callback=NULL,
               void *progress_callback_data=NULL) const;

    /// Write the image to the open ImageOutput 'out'.  Return true if
    /// all went ok, false if there were errors writing.  It does NOT
    /// close the file when it's done (and so may be called in a loop to
    /// write a multi-image file).
    bool write (ImageOutput *out,
                ProgressCallback progress_callback=NULL,
                void *progress_callback_data=NULL) const;

    /// Copy all the metadata from src to *this (except for pixel data
    /// resolution, channel information, and data format).
    void copy_metadata (const ImageBuf &src);

    /// Copy the pixel data from src to *this, automatically converting
    /// to the existing data format of *this.  It only copies pixels in
    /// the overlap regions (and channels) of the two images; pixel data
    /// in *this that do exist in src will be set to 0, and pixel data
    /// in src that do not exist in *this will not be copied.
    bool copy_pixels (const ImageBuf &src);

    /// Try to copy the pixels and metadata from src to *this, returning
    /// true upon success and false upon error/failure.
    /// 
    /// If the previous state of *this was uninitialized, owning its own
    /// local pixel memory, or referring to a read-only image backed by
    /// ImageCache, then local pixel memory will be allocated to hold
    /// the new pixels and the call always succeeds unless the memory
    /// cannot be allocated.
    ///
    /// If *this previously referred to an app-owned memory buffer, the
    /// memory cannot be re-allocated, so the call will only succeed if
    /// the app-owned buffer is already the correct resolution and
    /// number of channels.  The data type of the pixels will be
    /// converted automatically to the data type of the app buffer.
    bool copy (const ImageBuf &src);

    /// Error reporting for ImageBuf: call this with printf-like
    /// arguments.  Note however that this is fully typesafe!
    /// void error (const char *format, ...)
    TINYFORMAT_WRAP_FORMAT (void, error, const,
        std::ostringstream msg;, msg, append_error(msg.str());)

    /// Return true if the IB has had an error and has an error message
    /// to retrieve via geterror().
    bool has_error (void) const;

    /// Return info on the last error that occurred since geterror() was
    /// called (or an empty string if no errors are pending).  This also
    /// clears the error message for next time.
    std::string geterror (void) const;

    /// Return a read-only (const) reference to the image spec that
    /// describes the buffer.
    const ImageSpec & spec () const;

    /// Return a writable reference to the image spec that describes the
    /// buffer.  Use with extreme caution!  If you use this for anything
    /// other than adding attribute metadata, you are really taking your
    /// chances!
    ImageSpec & specmod ();

    /// Return a read-only (const) reference to the "native" image spec
    /// (that describes the file, which may be slightly different than
    /// the spec of the ImageBuf, particularly if the IB is backed by an
    /// ImageCache that is imposing some particular data format or tile
    /// size).
    const ImageSpec & nativespec () const;

    /// Return the name of this image.
    ///
    const std::string & name (void) const;

    /// Return the name of the image file format of the disk file we
    /// read into this image.  Returns an empty string if this image
    /// was not the result of a read().
    const std::string & file_format_name (void) const;

    /// Return the index of the subimage are we currently viewing
    ///
    int subimage () const;

    /// Return the number of subimages in the file.
    ///
    int nsubimages () const;

    /// Return the index of the miplevel are we currently viewing
    ///
    int miplevel () const;

    /// Return the number of miplevels of the current subimage.
    ///
    int nmiplevels () const;

    /// Return the number of color channels in the image.
    ///
    int nchannels () const;

    /// Retrieve a single channel of one pixel.
    ///
    float getchannel (int x, int y, int c) const;

    /// Retrieve the pixel value by x and y coordintes (on [0,res-1]),
    /// storing the floating point version in pixel[].  Retrieve at most
    /// maxchannels (will be clamped to the actual number of channels).
    void getpixel (int x, int y, float *pixel, int maxchannels=1000) const {
        getpixel (x, y, 0, pixel, maxchannels);
    }

    /// Retrieve the pixel value by x, y, z coordintes (on [0,res-1]),
    /// storing the floating point version in pixel[].  Retrieve at most
    /// maxchannels (will be clamped to the actual number of channels).
    void getpixel (int x, int y, int z, float *pixel, int maxchannels=1000) const;

    /// Linearly interpolate at pixel coordinates (x,y), where (0,0) is
    /// the upper left corner, (xres,yres) the lower right corner of
    /// the pixel data.
    void interppixel (float x, float y, float *pixel) const;

    /// Linearly interpolate at image data NDC coordinates (x,y), where (0,0) is
    /// the upper left corner of the pixel data window, (1,1) the lower
    /// right corner of the pixel data.
    /// FIXME -- lg thinks that this is stupid, and the only useful NDC
    /// space is the one used by interppixel_NDC_full.  We should deprecate
    /// this in the future.
    void interppixel_NDC (float x, float y, float *pixel) const;

    /// Linearly interpolate at NDC (image) coordinates (x,y), where (0,0) is
    /// the upper left corner of the display window, (1,1) the lower
    /// right corner of the display window.
    void interppixel_NDC_full (float x, float y, float *pixel) const;

    /// Set the pixel value by x and y coordintes (on [0,res-1]),
    /// from floating-point values in pixel[].  Set at most
    /// maxchannels (will be clamped to the actual number of channels).
    void setpixel (int x, int y, const float *pixel, int maxchannels=1000) {
        setpixel (x, y, 0, pixel, maxchannels);
    }

    /// Set the pixel value by x, y, z coordintes (on [0,res-1]),
    /// from floating-point values in pixel[].  Set at most
    /// maxchannels (will be clamped to the actual number of channels).
    void setpixel (int x, int y, int z,
                   const float *pixel, int maxchannels=1000);

    /// Set the i-th pixel value of the image (out of width*height*depth),
    /// from floating-point values in pixel[].  Set at most
    /// maxchannels (will be clamped to the actual number of channels).
    void setpixel (int i, const float *pixel, int maxchannels=1000);

    /// Retrieve the rectangle of pixels spanning [xbegin..xend) X
    /// [ybegin..yend), channels [chbegin,chend) (all with exclusive
    /// 'end'), specified as integer pixel coordinates, at the current
    /// MIP-map level, storing the pixel values beginning at the address
    /// specified by result and with the given strides (by default,
    /// AutoStride means the usual contiguous packing of pixels).  It is
    /// up to the caller to ensure that result points to an area of
    /// memory big enough to accommodate the requested rectangle.
    /// Return true if the operation could be completed, otherwise
    /// return false.
    bool get_pixel_channels (int xbegin, int xend, int ybegin, int yend,
                             int zbegin, int zend, int chbegin, int chend,
                             TypeDesc format, void *result,
                             stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride) const;

    /// Retrieve the rectangle of pixels spanning [xbegin..xend) X
    /// [ybegin..yend) (with exclusive 'end'), specified as integer
    /// pixel coordinates, at the current MIP-map level, storing the
    /// pixel values beginning at the address specified by result and
    /// with the given strides (by default, AutoStride means the usual
    /// contiguous packing of pixels).  It is up to the caller to ensure
    /// that result points to an area of memory big enough to
    /// accommodate the requested rectangle.  Return true if the
    /// operation could be completed, otherwise return false.
    bool get_pixels (int xbegin, int xend, int ybegin, int yend,
                     int zbegin, int zend, TypeDesc format,
                     void *result, stride_t xstride=AutoStride,
                     stride_t ystride=AutoStride,
                     stride_t zstride=AutoStride) const;

    /// Retrieve the rectangle of pixels spanning [xbegin..xend) X
    /// [ybegin..yend) (with exclusive 'end'), specified as integer
    /// pixel coordinates, at the current MIP-map level, storing the
    /// pixel values beginning at the address specified by result and
    /// with the given strides (by default, AutoStride means the usual
    /// contiguous packing of pixels), converting to the type <T> in the
    /// process.  It is up to the caller to ensure that result points to
    /// an area of memory big enough to accommodate the requested
    /// rectangle.  Return true if the operation could be completed,
    /// otherwise return false.
    template<typename T>
    bool get_pixel_channels (int xbegin, int xend, int ybegin, int yend,
                             int zbegin, int zend, int chbegin, int chend,
                             T *result, stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride) const;

    template<typename T>
    bool get_pixels (int xbegin, int xend, int ybegin, int yend,
                     int zbegin, int zend, T *result,
                     stride_t xstride=AutoStride, stride_t ystride=AutoStride,
                     stride_t zstride=AutoStride) const
    {
        return get_pixel_channels (xbegin, xend, ybegin, yend, zbegin, zend,
                                   0, nchannels(), result,
                                   xstride, ystride, zstride);
    }

    /// Even safer version of get_pixels: Retrieve the rectangle of
    /// pixels spanning [xbegin..xend) X [ybegin..yend) (with exclusive
    /// 'end'), specified as integer pixel coordinates, at the current
    /// MIP-map level, storing the pixel values in the 'result' vector
    /// (even allocating the right size).  Return true if the operation
    /// could be completed, otherwise return false.
    template<typename T>
    bool get_pixels (int xbegin_, int xend_, int ybegin_, int yend_,
                      int zbegin_, int zend_,
                      std::vector<T> &result) const
    {
        result.resize (nchannels() * ((zend_-zbegin_)*(yend_-ybegin_)*(xend_-xbegin_)));
        return get_pixels (xbegin_, xend_, ybegin_, yend_, zbegin_, zend_,
                           &result[0]);
    }

    /// Retrieve the number of deep data samples corresponding to pixel
    /// (x,y,z).  Return 0 not a deep image or if the pixel is out of
    /// range or has no deep samples.
    int deep_samples (int x, int y, int z=0) const;

    /// Return a pointer to the raw array of deep data samples for
    /// channel c of pixel (x,y,z).  Return NULL if not a deep image or
    /// if the pixel is out of range or has no deep samples.
    const void *deep_pixel_ptr (int x, int y, int z, int c) const;

    /// Return the value (as a float) of sample s of channel c of pixel
    /// (x,y,z).  Return 0.0 if not a deep image or if the pixel
    /// coordinates or channel number are out of range or if it has no
    /// deep samples.
    float deep_value (int x, int y, int z, int c, int s) const;

    int orientation () const;

    int oriented_width () const;
    int oriented_height () const;
    int oriented_x () const;
    int oriented_y () const;
    int oriented_full_width () const;
    int oriented_full_height () const;
    int oriented_full_x () const;
    int oriented_full_y () const;

    /// Return the beginning (minimum) x coordinate of the defined image.
    int xbegin () const;

    /// Return the end (one past maximum) x coordinate of the defined image.
    int xend () const;

    /// Return the beginning (minimum) y coordinate of the defined image.
    int ybegin () const;

    /// Return the end (one past maximum) y coordinate of the defined image.
    int yend () const;

    /// Return the beginning (minimum) z coordinate of the defined image.
    int zbegin () const;

    /// Return the end (one past maximum) z coordinate of the defined image.
    int zend () const;

    /// Return the minimum x coordinate of the defined image.
    int xmin () const;

    /// Return the maximum x coordinate of the defined image.
    int xmax () const;

    /// Return the minimum y coordinate of the defined image.
    int ymin () const;

    /// Return the maximum y coordinate of the defined image.
    int ymax () const;

    /// Return the minimum z coordinate of the defined image.
    int zmin () const;

    /// Return the maximum z coordinate of the defined image.
    int zmax () const;

    /// Set the "full" (a.k.a. display) window to [xbegin,xend) x
    /// [ybegin,yend) x [zbegin,zend).  If bordercolor is not NULL, also
    /// set the spec's "oiio:bordercolor" attribute.
    void set_full (int xbegin, int xend, int ybegin, int yend,
                   int zbegin, int zend, const float *bordercolor=NULL);

    bool pixels_valid (void) const;

    TypeDesc pixeltype () const;

    /// A pointer to "local" pixels, if they are fully in RAM and not
    /// backed by an ImageCache, or NULL otherwise.  (You can also test
    /// it like a bool to find out if pixels are local.)
    void *localpixels ();
    const void *localpixels () const;

    /// Are the pixels backed by an ImageCache, rather than the whole
    /// image being in RAM somewhere?
    bool cachedpixels () const;

    ImageCache *imagecache () const;

    /// Return the address where pixel (x,y) is stored in the image buffer.
    /// Use with extreme caution!  Will return NULL if the pixel values
    /// aren't local.
    const void *pixeladdr (int x, int y) const { return pixeladdr (x, y, 0); }

    /// Return the address where pixel (x,y,z) is stored in the image buffer.
    /// Use with extreme caution!  Will return NULL if the pixel values
    /// aren't local.
    const void *pixeladdr (int x, int y, int z) const;

    /// Return the address where pixel (x,y) is stored in the image buffer.
    /// Use with extreme caution!  Will return NULL if the pixel values
    /// aren't local.
    void *pixeladdr (int x, int y) { return pixeladdr (x, y, 0); }

    /// Return the address where pixel (x,y,z) is stored in the image buffer.
    /// Use with extreme caution!  Will return NULL if the pixel values
    /// aren't local.
    void *pixeladdr (int x, int y, int z);

    /// Does this ImageBuf store deep data?
    bool deep () const;

    /// Retrieve the "deep" data.
    DeepData *deepdata ();
    const DeepData *deepdata () const;

    /// Is this ImageBuf object initialized?
    bool initialized () const;

    friend class IteratorBase;

    class IteratorBase {
    public:
        IteratorBase (const ImageBuf &ib)
            : m_ib(&ib), m_tile(NULL), m_proxydata(NULL)
        {
            init_ib ();
            range_is_image ();
        }

        IteratorBase (const ImageBuf &ib, int xbegin, int xend,
                      int ybegin, int yend, int zbegin=0, int zend=1)
            : m_ib(&ib), m_tile(NULL), m_proxydata(NULL)
        {
            init_ib ();
            m_rng_xbegin = std::max (xbegin, m_img_xbegin); 
            m_rng_xend   = std::min (xend,   m_img_xend);
            m_rng_ybegin = std::max (ybegin, m_img_ybegin);
            m_rng_yend   = std::min (yend,   m_img_yend);
            m_rng_zbegin = std::max (zbegin, m_img_zbegin);
            m_rng_zend   = std::min (zend,   m_img_zend);
        }

        /// Construct read-write clamped valid iteration region from
        /// ImageBuf and ROI.
        IteratorBase (const ImageBuf &ib, const ROI &roi)
            : m_ib(&ib), m_tile(NULL), m_proxydata(NULL)
        {
            init_ib ();
            if (roi.defined()) {
                m_rng_xbegin = std::max (roi.xbegin, m_img_xbegin);
                m_rng_xend   = std::min (roi.xend,   m_img_xend);
                m_rng_ybegin = std::max (roi.ybegin, m_img_ybegin);
                m_rng_yend   = std::min (roi.yend,   m_img_yend);
                m_rng_zbegin = std::max (roi.zbegin, m_img_zbegin);
                m_rng_zend   = std::min (roi.zend,   m_img_zend);
            } else {
                range_is_image ();
            }
        }

        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel, and do NOT
        /// clamp the region to the valid image pixels.  If "unclamped"
        /// is true, the iteration region will NOT be clamped to the
        /// image boundary, so you must use done() to test whether the
        /// iteration is complete, versus valid() to test whether it's
        /// pointing to a valid image pixel.
        IteratorBase (const ImageBuf &ib, int xbegin, int xend,
                      int ybegin, int yend, int zbegin, int zend,
                      bool unclamped)
            : m_ib(&ib), m_tile(NULL), m_proxydata(NULL)
        {
            init_ib ();
            if (unclamped) {
                m_rng_xbegin = xbegin;
                m_rng_xend = xend;
                m_rng_ybegin = ybegin;
                m_rng_yend = yend;
                m_rng_zbegin = zbegin;
                m_rng_zend = zend;
            } else {
                m_rng_xbegin = std::max(xbegin,m_img_xbegin);
                m_rng_xend = std::min(xend,m_img_xend);
                m_rng_ybegin = std::max(ybegin,m_img_ybegin);
                m_rng_yend = std::min(yend,m_img_yend);
                m_rng_zbegin = std::max(zbegin,m_img_zbegin);
                m_rng_zend = std::min(zend,m_img_zend);
            }
        }

        IteratorBase (const IteratorBase &i)
            : m_ib (i.m_ib),
              m_rng_xbegin(i.m_rng_xbegin), m_rng_xend(i.m_rng_xend), 
              m_rng_ybegin(i.m_rng_ybegin), m_rng_yend(i.m_rng_yend),
              m_rng_zbegin(i.m_rng_zbegin), m_rng_zend(i.m_rng_zend),
              m_tile(NULL), m_proxydata(i.m_proxydata)
        {
            init_ib ();
        }

        ~IteratorBase () {
            if (m_tile)
                m_ib->imagecache()->release_tile (m_tile);
        }

        /// Assign one IteratorBase to another
        ///
        const IteratorBase & assign_base (const IteratorBase &i) {
            if (m_tile)
                m_ib->imagecache()->release_tile (m_tile);
            m_tile = NULL;
            m_proxydata = i.m_proxydata;
            m_ib = i.m_ib;
            init_ib ();
            m_rng_xbegin = i.m_rng_xbegin;  m_rng_xend = i.m_rng_xend;
            m_rng_ybegin = i.m_rng_ybegin;  m_rng_yend = i.m_rng_yend;
            m_rng_zbegin = i.m_rng_zbegin;  m_rng_zend = i.m_rng_zend;
            return *this;
        }

        /// Retrieve the current x location of the iterator.
        ///
        int x () const { return m_x; }
        /// Retrieve the current y location of the iterator.
        ///
        int y () const { return m_y; }
        /// Retrieve the current z location of the iterator.
        ///
        int z () const { return m_z; }

        /// Is the current location valid?  Locations outside the
        /// designated region are invalid, as is an iterator that has
        /// completed iterating over the whole region.
        bool valid () const {
            return m_valid;
        }

        /// Is the location (x,y[,z]) within the designated iteration
        /// range?
        bool valid (int x_, int y_, int z_=0) const {
            return (x_ >= m_rng_xbegin && x_ < m_rng_xend &&
                    y_ >= m_rng_ybegin && y_ < m_rng_yend &&
                    z_ >= m_rng_zbegin && z_ < m_rng_zend);
        }

        /// Is the location (x,y[,z]) within the region of the ImageBuf
        /// that contains pixel values (sometimes called the "data window")?
        bool exists (int x_, int y_, int z_=0) const {
            return (x_ >= m_img_xbegin && x_ < m_img_xend &&
                    y_ >= m_img_ybegin && y_ < m_img_yend &&
                    z_ >= m_img_zbegin && z_ < m_img_zend);
        }
        /// Does the current location exist within the ImageBuf's 
        /// data window?
        bool exists () const {
            return m_exists;
        }

        /// Are we finished iterating over the region?
        //
        bool done () const {
            // We're "done" if we are both invalid and in exactly the
            // spot that we would end up after iterating off of the last
            // pixel in the range.  (The m_valid test is just a quick
            // early-out for when we're in the correct pixel range.)
            return (m_valid == false && m_x == m_rng_xbegin &&
                    m_y == m_rng_ybegin && m_z == m_rng_zend);
        }

        /// Retrieve the number of deep data samples at this pixel.
        int deep_samples () { return m_ib->deep_samples (m_x, m_y, m_z); }

        /// Explicitly point the iterator.  This results in an invalid
        /// iterator if outside the previously-designated region.
        void pos (int x_, int y_, int z_=0) {
            if (x_ == m_x+1 && x_ < m_rng_xend && y_ == m_y && z_ == m_z &&
                m_valid && m_exists) {
                // Special case for what is in effect just incrementing x
                // within the iteration region.
                m_x = x_;
                pos_xincr ();
                m_exists = (x_ < m_img_xend);
                return;
            }
            bool v = valid(x_,y_,z_);
            bool e = exists(x_,y_,z_);
            if (! e || m_deep)
                m_proxydata = NULL;
            else if (m_localpixels)
                m_proxydata = (char *)m_ib->pixeladdr (x_, y_, z_);
            else
                m_proxydata = (char *)m_ib->retile (x_, y_, z_, m_tile,
                                                    m_tilexbegin, m_tileybegin,
                                                    m_tilezbegin, m_tilexend);
            m_x = x_;  m_y = y_;  m_z = z_;
            m_valid = v;
            m_exists = e;
        }

        /// Increment to the next pixel in the region.
        ///
        void operator++ () {
            if (++m_x <  m_rng_xend) {
                // Special case: we only incremented x, didn't change y
                // or z, and the previous position was within the data
                // window.  Call a shortcut version of pos.
                if (m_exists) {
                    pos_xincr ();
                    return;
                }
            } else {
                // Wrap to the next scanline
                m_x = m_rng_xbegin;
                if (++m_y >= m_rng_yend) {
                    m_y = m_rng_ybegin;
                    if (++m_z >= m_rng_zend) {
                        m_valid = false;  // shortcut -- finished iterating
                        return;
                    }
                }
            }
            pos (m_x, m_y, m_z);
        }
        /// Increment to the next pixel in the region.
        ///
        void operator++ (int) {
            ++(*this);
        }

    protected:
        friend class ImageBuf;
        friend class ImageBufImpl;
        const ImageBuf *m_ib;
        bool m_valid, m_exists;
        bool m_deep;
        bool m_localpixels;
        // Image boundaries
        int m_img_xbegin, m_img_xend, m_img_ybegin, m_img_yend,
            m_img_zbegin, m_img_zend;
        // Iteration range
        int m_rng_xbegin, m_rng_xend, m_rng_ybegin, m_rng_yend,
            m_rng_zbegin, m_rng_zend;
        int m_x, m_y, m_z;
        ImageCache::Tile *m_tile;
        int m_tilexbegin, m_tileybegin, m_tilezbegin;
        int m_tilexend;
        int m_nchannels, m_pixel_bytes;
        char *m_proxydata;

        // Helper called by ctrs -- set up some locally cached values
        // that are copied or derived from the ImageBuf.
        void init_ib () {
            const ImageSpec &spec (m_ib->spec());
            m_deep = spec.deep;
            m_localpixels = m_ib->localpixels();
            m_img_xbegin = spec.x; m_img_xend = spec.x+spec.width;
            m_img_ybegin = spec.y; m_img_yend = spec.y+spec.height;
            m_img_zbegin = spec.z; m_img_zend = spec.z+spec.depth;
            m_nchannels = spec.nchannels;
//            m_tilewidth = spec.tile_width;
            m_pixel_bytes = spec.pixel_bytes();
            m_x = 0xffffffff;
            m_y = 0xffffffff;
            m_z = 0xffffffff;
        }

        // Helper called by ctrs -- make the iteration range the full
        // image data window.
        void range_is_image () {
            m_rng_xbegin = m_img_xbegin;  m_rng_xend = m_img_xend; 
            m_rng_ybegin = m_img_ybegin;  m_rng_yend = m_img_yend;
            m_rng_zbegin = m_img_zbegin;  m_rng_zend = m_img_zend;
        }

        // Helper called by pos(), but ONLY for the case where we are
        // moving from an existing pixel to the next spot in +x.
        // Note: called *after* m_x was incremented!
        void pos_xincr () {
            DASSERT (m_exists && m_valid);   // precondition
            DASSERT (valid(m_x,m_y,m_z));    // should be true by definition
            if (m_x >= m_img_xend /*same as !exists() for this case*/) {
                m_proxydata = NULL;
                m_exists = false;
            } else if (m_deep) {
                m_proxydata = NULL;
            } else if (m_localpixels) {
                m_proxydata += m_pixel_bytes;
            } else if (m_x < m_tilexend) {
                // Haven't crossed a tile boundary, don't retile!
                m_proxydata += m_pixel_bytes;
            } else {
                m_proxydata = (char *)m_ib->retile (m_x, m_y, m_z, m_tile,
                                    m_tilexbegin, m_tileybegin, m_tilezbegin,
                                    m_tilexend);
            }
        }
    };

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
    ///   for (  ;  ! pixel.done();  ++pixel) {
    ///       for (int c = 0;  c < img.nchannels();  ++c) {
    ///           float x = pixel[c];
    ///           pixel[c] = ...;
    ///       }
    ///   }
    /// \endcode
    ///
    template<typename BUFT, typename USERT=float>
    class Iterator : public IteratorBase {
    public:
        /// Construct from just an ImageBuf -- iterate over the whole
        /// region, starting with the upper left pixel of the region.
        Iterator (ImageBuf &ib)
            : IteratorBase(ib)
        {
            pos (m_rng_xbegin,m_rng_ybegin,m_rng_zbegin);
        }
        /// Construct from an ImageBuf and a specific pixel index.
        /// The iteration range is the full image.
        Iterator (ImageBuf &ib, int x_, int y_, int z_=0)
            : IteratorBase(ib)
        {
            pos (x_, y_, z_);
        }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel.  The
        /// iteration region will be clamped to the valid image range.
        Iterator (ImageBuf &ib, int xbegin, int xend,
                  int ybegin, int yend, int zbegin=0, int zend=1)
            : IteratorBase(ib, xbegin, xend, ybegin, yend, zbegin, zend)
        {
            pos (m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
        }
        /// Construct read-write clamped valid iteration region from
        /// ImageBuf and ROI.
        Iterator (ImageBuf &ib, const ROI &roi)
            : IteratorBase (ib, roi)
        {
            pos (m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
        }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel, and do NOT
        /// clamp the region to the valid image pixels.  If "unclamped"
        /// is true, the iteration region will NOT be clamped to the
        /// image boundary, so you must use done() to test whether the
        /// iteration is complete, versus valid() to test whether it's
        /// pointing to a valid image pixel.
        Iterator (ImageBuf &ib, int xbegin, int xend,
                  int ybegin, int yend, int zbegin, int zend,
                  bool unclamped)
            : IteratorBase(ib, xbegin, xend, ybegin, yend,
                           zbegin, zend, unclamped)
        {
            pos (m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
        }
        /// Copy constructor.
        ///
        Iterator (Iterator &i)
            : IteratorBase (i.m_ib)
        {
            pos (i.m_x, i.m_y, i.m_z);
        }

        ~Iterator () { }

        /// Assign one Iterator to another
        ///
        const Iterator & operator= (const Iterator &i) {
            assign_base (i);
            pos (i.m_x, i.m_y, i.m_z);
            return *this;
        }

        /// Dereferencing the iterator gives us a proxy for the pixel,
        /// which we can index for reading or assignment.
        DataArrayProxy<BUFT,USERT>& operator* () {
            return *(DataArrayProxy<BUFT,USERT> *)(void *)&m_proxydata;
        }

        /// Array indexing retrieves the value of the i-th channel of
        /// the current pixel.
        USERT operator[] (int i) const {
            DataArrayProxy<BUFT,USERT> proxy((BUFT*)m_proxydata);
            return proxy[i];
        } 

        /// Array referencing retrieve a proxy (which may be "assigned
        /// to") of i-th channel of the current pixel, so that this
        /// works: me[i] = val;
        DataProxy<BUFT,USERT> operator[] (int i) {
            DataArrayProxy<BUFT,USERT> proxy((BUFT*)m_proxydata);
            return proxy[i];
        } 

        void * rawptr () const { return m_proxydata; }

        /// Retrieve the deep data value of sample s of channel c.
        USERT deep_value (int c, int s) const {
            return convert_type<float,USERT>(m_ib->deep_value (m_x, m_y, m_z, c, s));
        }
    };


    /// Just like an ImageBuf::Iterator, except that it refers to a
    /// const ImageBuf.
    template<typename BUFT, typename USERT=float>
    class ConstIterator : public IteratorBase {
    public:
        /// Construct from just an ImageBuf -- iterate over the whole
        /// region, starting with the upper left pixel of the region.
        ConstIterator (const ImageBuf &ib)
            : IteratorBase(ib)
        {
            pos (m_rng_xbegin,m_rng_ybegin,m_rng_zbegin);
        }
        /// Construct from an ImageBuf and a specific pixel index.
        /// The iteration range is the full image.
        ConstIterator (const ImageBuf &ib, int x_, int y_, int z_=0)
            : IteratorBase(ib)
        {
            pos (x_, y_, z_);
        }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel.  The
        /// iteration region will be clamped to the valid image range.
        ConstIterator (const ImageBuf &ib, int xbegin, int xend,
                       int ybegin, int yend, int zbegin=0, int zend=1)
            : IteratorBase(ib, xbegin, xend, ybegin, yend, zbegin, zend)
        {
            pos (m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
        }
        /// Construct read-only clamped valid iteration region
        /// from ImageBuf and ROI.
        ConstIterator (const ImageBuf &ib, const ROI &roi)
            : IteratorBase (ib, roi)
        {
            pos (m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
        }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel, and do NOT
        /// clamp the region to the valid image pixels.  If "unclamped"
        /// is true, the iteration region will NOT be clamped to the
        /// image boundary, so you must use done() to test whether the
        /// iteration is complete, versus valid() to test whether it's
        /// pointing to a valid image pixel.
        ConstIterator (const ImageBuf &ib, int xbegin, int xend,
                       int ybegin, int yend, int zbegin, int zend,
                       bool unclamped)
            : IteratorBase(ib, xbegin, xend, ybegin, yend,
                           zbegin, zend, unclamped)
        {
            pos (m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
        }
        /// Copy constructor.
        ///
        ConstIterator (const ConstIterator &i)
            : IteratorBase (i)
        {
            pos (i.m_x, i.m_y, i.m_z);
        }

        ~ConstIterator () { }

        /// Assign one ConstIterator to another
        ///
        const ConstIterator & operator= (const ConstIterator &i) {
            assign_base (i);
            pos (i.m_x, i.m_y, i.m_z);
            return *this;
        }

        /// Dereferencing the iterator gives us a proxy for the pixel,
        /// which we can index for reading or assignment.
        ConstDataArrayProxy<BUFT,USERT>& operator* () const {
            return *(ConstDataArrayProxy<BUFT,USERT> *)&m_proxydata;
        }

        /// Array indexing retrieves the value of the i-th channel of
        /// the current pixel.
        USERT operator[] (int i) const {
            ConstDataArrayProxy<BUFT,USERT> proxy ((BUFT*)m_proxydata);
            return proxy[i];
        } 

        const void * rawptr () const { return m_proxydata; }

        /// Retrieve the deep data value of sample s of channel c.
        USERT deep_value (int c, int s) const {
            return convert_type<float,USERT>(m_ib->deep_value (m_x, m_y, m_z, c, s));
        }
    };


protected:
    ImageBufImpl *m_impl;    //< PIMPL idiom

    ImageBufImpl * impl () { return m_impl; }
    const ImageBufImpl * impl () const { return m_impl; }

    // Copy src's pixels into *this.  Pixels must already be local
    // (either owned or wrapped) and the resolution and number of
    // channels must match src.  Data type is allowed to be different,
    // however, with automatic conversion upon copy.
    void copy_from (const ImageBuf &src);

    // Reset the ImageCache::Tile * to reserve and point to the correct
    // tile for the given pixel, and return the ptr to the actual pixel
    // within the tile.
    const void * retile (int x, int y, int z,
                         ImageCache::Tile* &tile, int &tilexbegin,
                         int &tileybegin, int &tilezbegin,
                         int &tilexend) const;

    /// Private and unimplemented.
    const ImageBuf& operator= (const ImageBuf &src);

    /// Add to the error message list for this IB.
    void append_error (const std::string& message) const;

};


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_IMAGEBUF_H

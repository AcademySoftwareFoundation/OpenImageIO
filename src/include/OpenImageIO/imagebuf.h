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

#include <limits>


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
    ROI () : xbegin(std::numeric_limits<int>::min()), xend(0),
             ybegin(0), yend(0), zbegin(0), zend(0), chbegin(0), chend(0)
    { }

    /// Constructor with an explicitly defined region.
    ///
    ROI (int xbegin, int xend, int ybegin, int yend,
         int zbegin=0, int zend=1, int chbegin=0, int chend=10000)
        : xbegin(xbegin), xend(xend), ybegin(ybegin), yend(yend),
          zbegin(zbegin), zend(zend), chbegin(chbegin), chend(chend)
    { }

    /// Is a region defined?
    bool defined () const { return (xbegin != std::numeric_limits<int>::min()); }

    // Region dimensions.
    int width () const { return xend - xbegin; }
    int height () const { return yend - ybegin; }
    int depth () const { return zend - zbegin; }

    /// Number of channels in the region.  Beware -- this defaults to a
    /// huge number, and to be meaningful you must consider
    /// std::min (imagebuf.nchannels(), roi.nchannels()).
    int nchannels () const { return chend - chbegin; }

    /// Total number of pixels in the region.
    imagesize_t npixels () const {
        if (! defined())
            return 0;
        imagesize_t w = width(), h = height(), d = depth();
        return w*h*d;
    }

    /// Documentary sugar -- although the static ROI::All() function
    /// simply returns the results of the default ROI constructor, it
    /// makes it very clear when using as a default function argument
    /// that it means "all" of the image.  For example,
    ///     float myfunc (ImageBuf &buf, ROI roi = ROI::All());
    /// Doesn't that make it abundantly clear?
    static ROI All () { return ROI(); }

    /// Test equality of two ROIs
    friend bool operator== (const ROI &a, const ROI &b) {
        return (a.xbegin == b.xbegin && a.xend == b.xend &&
                a.ybegin == b.ybegin && a.yend == b.yend &&
                a.zbegin == b.zbegin && a.zend == b.zend &&
                a.chbegin == b.chbegin && a.chend == b.chend);
    }
    /// Test inequality of two ROIs
    friend bool operator!= (const ROI &a, const ROI &b) {
        return (a.xbegin != b.xbegin || a.xend != b.xend ||
                a.ybegin != b.ybegin || a.yend != b.yend ||
                a.zbegin != b.zbegin || a.zend != b.zend ||
                a.chbegin != b.chbegin || a.chend != b.chend);
    }

    /// Stream output of the range
    friend std::ostream & operator<< (std::ostream &out, const ROI &roi) {
        out << roi.xbegin << ' ' << roi.xend << ' ' << roi.ybegin << ' '
            << roi.yend << ' ' << roi.zbegin << ' ' << roi.zend << ' '
            << roi.chbegin << ' ' << roi.chend;
        return out;
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
    /// Construct an empty/uninitialized ImageBuf.  This is relatively
    /// useless until you call reset().
    ImageBuf ();

    /// Construct an ImageBuf to read the named image (at the designated
    /// subimage/MIPlevel -- but don't actually read it yet!   The image
    /// will actually be read when other methods need to access the spec
    /// and/or pixels, or when an explicit call to init_spec() or read() is
    /// made, whichever comes first. If a non-NULL imagecache is supplied,
    /// it will specifiy a custom ImageCache to use; if otherwise, the
    /// global/shared ImageCache will be used.
    ImageBuf (const std::string &name, int subimage=0, int miplevel=0,
              ImageCache *imagecache = NULL);

    /// Construct an ImageBuf to read the named image -- but don't actually
    /// read it yet!  The image will actually be read when other methods
    /// need to access the spec and/or pixels, or when an explicit call to
    /// init_spec() or read() is made, whichever comes first. If a non-NULL
    /// imagecache is supplied, it will specifiy a custom ImageCache to use;
    /// if otherwise, the global/shared ImageCache will be used.
    ImageBuf (const std::string &name, ImageCache *imagecache);

    /// Construct an Imagebuf given a proposed spec describing the image
    /// size and type, and allocate storage for the pixels of the image
    /// (whose values will be uninitialized).
    ImageBuf (const ImageSpec &spec);

    /// Construct an Imagebuf given both a name and a proposed spec
    /// describing the image size and type, and allocate storage for
    /// the pixels of the image (whose values will be undefined).
    ImageBuf (const std::string &name, const ImageSpec &spec);

    /// Construct an ImageBuf that "wraps" a memory buffer owned by the
    /// calling application.  It can write pixels to this buffer, but
    /// can't change its resolution or data type.
    ImageBuf (const ImageSpec &spec, void *buffer);

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

    /// Description of where the pixels live for this ImageBuf.
    enum IBStorage { UNINITIALIZED,   // no pixel memory
                     LOCALBUFFER,     // The IB owns the memory
                     APPBUFFER,       // The IB wraps app's memory
                     IMAGECACHE       // Backed by ImageCache
                   };

    /// Restore the ImageBuf to an uninitialized state.
    ///
    void clear ();

    /// Forget all previous info, reset this ImageBuf to a new image
    /// that is uninitialized (no pixel values, no size or spec).
    void reset (const std::string &name, int subimage, int miplevel,
                ImageCache *imagecache = NULL);

    /// Forget all previous info, reset this ImageBuf to a new image
    /// that is uninitialized (no pixel values, no size or spec).
    void reset (const std::string &name, ImageCache *imagecache=NULL);

    /// Forget all previous info, reset this ImageBuf to a blank
    /// image of the given dimensions.
    void reset (const ImageSpec &spec);

    /// Forget all previous info, reset this ImageBuf to a blank
    /// image of the given name and dimensions.
    void reset (const std::string &name, const ImageSpec &spec);

    /// Copy spec to *this, and then allocate enough space the right
    /// size for an image described by the format spec.  If the ImageBuf
    /// already has allocated pixels, their values will not be preserved
    /// if the new spec does not describe an image of the same size and
    /// data type as it used to be.
    /// DEPRECATED (1.3) -- I don't see any reason why this should be
    /// favored over reset(spec).
    void alloc (const ImageSpec &spec);

    /// Which type of storage is being used for the pixels?
    IBStorage storage () const;

    /// Is this ImageBuf object initialized?
    bool initialized () const;

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

    /// Write the image to the named file and file format ("" means to infer
    /// the type from the filename extension). Return true if all went ok,
    /// false if there were errors writing.
    bool write (const std::string &filename,
                const std::string &fileformat = std::string(),
                ProgressCallback progress_callback=NULL,
                void *progress_callback_data=NULL) const;

    /// Inform the ImageBuf what data format you'd like for any subsequent
    /// write().
    void set_write_format (TypeDesc format);

    /// Inform the ImageBuf what tile size (or no tiling, for 0) for
    /// any subsequent write().
    void set_write_tiles (int width=0, int height=0, int depth=0);

    /// DEPRECATED (1.3) synonym for write().  Kept for now for backward
    /// compatibility.
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

    /// Swap with another ImageBuf
    void swap (ImageBuf &other) { std::swap (m_impl, other.m_impl); }

    /// Error reporting for ImageBuf: call this with printf-like
    /// arguments.  Note however that this is fully typesafe!
    /// void error (const char *format, ...)
    TINYFORMAT_WRAP_FORMAT (void, error, const,
        std::ostringstream msg;, msg, append_error(msg.str());)

    /// Return true if the IB has had an error and has an error message
    /// to retrieve via geterror().
    bool has_error (void) const;

    /// Return the text of all error messages issued since geterror()
    /// was called (or an empty string if no errors are pending).  This
    /// also clears the error message for next time.
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

    /// Wrap mode describes what happens when an iterator points to
    /// a value outside the usual data range of an image.
    enum WrapMode { WrapDefault, WrapBlack, WrapClamp, WrapPeriodic,
                    WrapMirror, _WrapLast };

    /// Retrieve a single channel of one pixel.
    ///
    float getchannel (int x, int y, int z, int c, WrapMode wrap=WrapBlack) const;

    /// Retrieve the pixel value by x and y pixel indices,
    /// storing the floating point version in pixel[].  Retrieve at most
    /// maxchannels (will be clamped to the actual number of channels).
    void getpixel (int x, int y, float *pixel, int maxchannels=1000) const {
        getpixel (x, y, 0, pixel, maxchannels);
    }

    /// Retrieve the pixel value by x, y, z pixel indices,
    /// storing the floating point version in pixel[].  Retrieve at most
    /// maxchannels (will be clamped to the actual number of channels).
    void getpixel (int x, int y, int z, float *pixel, int maxchannels=1000,
                   WrapMode wrap=WrapBlack) const;

    /// Sample the image plane at coordinates (x,y), using linear
    /// interpolation between pixels, placing the result in pixel[0..n-1]
    /// where n is the smaller of maxchannels or the actual number of
    /// channels stored in the buffer.  It is up to the application to
    /// ensure that pixel points to enough memory to hold the required
    /// number of channels. Note that pixel data values themselves are at
    /// the pixel centers, so pixel (i,j) is at image plane coordinate
    /// (i+0.5, j+0.5).
    void interppixel (float x, float y, float *pixel,
                      WrapMode wrap=WrapBlack) const;

    /// Linearly interpolate at image data NDC coordinates (s,t), where
    /// (0,0) is the upper left corner of the pixel data window, (1,1)
    /// the lower right corner of the pixel data.
    /// FIXME -- lg thinks that this is stupid, and the only useful NDC
    /// space is the one used by interppixel_NDC_full.  We should deprecate
    /// this in the future.
    void interppixel_NDC (float s, float t, float *pixel,
                          WrapMode wrap=WrapBlack) const;

    /// Linearly interpolate at space coordinates (s,t), where (0,0) is
    /// the upper left corner of the display window, (1,1) the lower
    /// right corner of the display window.
    void interppixel_NDC_full (float s, float t, float *pixel,
                               WrapMode wrap=WrapBlack) const;

    /// Set the pixel with coordinates (x,y,0) to have the values in
    /// pixel[0..n-1].  The number of channels copied, n, is the minimum
    /// of maxchannels and the actual number of channels in the image.
    void setpixel (int x, int y, const float *pixel, int maxchannels=1000) {
        setpixel (x, y, 0, pixel, maxchannels);
    }

    /// Set the pixel with coordinates (x,y,z) to have the values in
    /// pixel[0..n-1].  The number of channels copied, n, is the minimum
    /// of maxchannels and the actual number of channels in the image.
    void setpixel (int x, int y, int z,
                   const float *pixel, int maxchannels=1000);

    /// Set the i-th pixel value of the image (out of width*height*depth),
    /// from floating-point values in pixel[].  Set at most
    /// maxchannels (will be clamped to the actual number of channels).
    void setpixel (int i, const float *pixel, int maxchannels=1000);

    /// Retrieve the rectangle of pixels spanning [xbegin..xend) X
    /// [ybegin..yend) X [zbegin..zend), channels [chbegin,chend) (all
    /// with exclusive 'end'), specified as integer pixel coordinates,
    /// at the current subimage and MIP-map level, storing the pixel
    /// values beginning at the address specified by result and with the
    /// given strides (by default, AutoStride means the usual contiguous
    /// packing of pixels) and converting into the data type described
    /// by 'format'.  It is up to the caller to ensure that result
    /// points to an area of memory big enough to accommodate the
    /// requested rectangle.  Return true if the operation could be
    /// completed, otherwise return false.
    bool get_pixel_channels (int xbegin, int xend, int ybegin, int yend,
                             int zbegin, int zend, int chbegin, int chend,
                             TypeDesc format, void *result,
                             stride_t xstride=AutoStride,
                             stride_t ystride=AutoStride,
                             stride_t zstride=AutoStride) const;

    /// Retrieve the rectangle of pixels spanning [xbegin..xend) X
    /// [ybegin..yend) X [zbegin..zend) (with exclusive 'end'),
    /// specified as integer pixel coordinates, at the current MIP-map
    /// level, storing the pixel values beginning at the address
    /// specified by result and with the given strides (by default,
    /// AutoStride means the usual contiguous packing of pixels) and
    /// converting into the data type described by 'format'.  It is up
    /// to the caller to ensure that result points to an area of memory
    /// big enough to accommodate the requested rectangle.  Return true
    /// if the operation could be completed, otherwise return false.
    bool get_pixels (int xbegin, int xend, int ybegin, int yend,
                     int zbegin, int zend,
                     TypeDesc format, void *result,
                     stride_t xstride=AutoStride,
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

    int orientation () const;
    void set_orientation (int orient);

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
    /// [ybegin,yend) x [zbegin,zend).
    void set_full (int xbegin, int xend, int ybegin, int yend,
                   int zbegin, int zend);

    /// Set the "full" (a.k.a. display) window to [xbegin,xend) x
    /// [ybegin,yend) x [zbegin,zend).  If bordercolor is not NULL, also
    /// set the spec's "oiio:bordercolor" attribute.
    /// DEPRECATED (1.3) -- we don't use the 'bordercolor' parameter, and
    /// it seems strange, so let's phase it out.
    void set_full (int xbegin, int xend, int ybegin, int yend,
                   int zbegin, int zend, const float *bordercolor);

    /// Return pixel data window for this ImageBuf as a ROI.
    ROI roi () const;

    /// Return full/display window for this ImageBuf as a ROI.
    ROI roi_full () const;

    /// Set full/display window for this ImageBuf to a ROI.
    /// Does NOT change the channels of the spec, regardless of newroi.
    void set_roi_full (const ROI &newroi);

    bool pixels_valid (void) const;

    TypeDesc pixeltype () const;

    /// A raw pointer to "local" pixel memory, if they are fully in RAM
    /// and not backed by an ImageCache, or NULL otherwise.  You can
    /// also test it like a bool to find out if pixels are local.
    void *localpixels ();
    const void *localpixels () const;

    /// Are the pixels backed by an ImageCache, rather than the whole
    /// image being in RAM somewhere?
    bool cachedpixels () const;

    ImageCache *imagecache () const;

    /// Return the address where pixel (x,y,z) is stored in the image buffer.
    /// Use with extreme caution!  Will return NULL if the pixel values
    /// aren't local.
    const void *pixeladdr (int x, int y, int z=0) const;

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

    /// Retrieve the number of deep data samples corresponding to pixel
    /// (x,y,z).  Return 0 if not a deep image or if the pixel is out of
    /// range or has no deep samples.
    int deep_samples (int x, int y, int z=0) const;

    /// Return a pointer to the raw array of deep data samples for
    /// channel c of pixel (x,y,z).  Return NULL if the pixel
    /// coordinates or channel number are out of range, if the
    /// pixel/channel has no deep samples, or if the image is not deep.
    const void *deep_pixel_ptr (int x, int y, int z, int c) const;

    /// Return the value (as a float) of sample s of channel c of pixel
    /// (x,y,z).  Return 0.0 if not a deep image or if the pixel
    /// coordinates or channel number are out of range or if it has no
    /// deep samples.
    float deep_value (int x, int y, int z, int c, int s) const;

    /// Retrieve the "deep" data.
    DeepData *deepdata ();
    const DeepData *deepdata () const;

    friend class IteratorBase;

    class IteratorBase {
    public:
        IteratorBase (const ImageBuf &ib, WrapMode wrap)
            : m_ib(&ib), m_tile(NULL), m_proxydata(NULL)
        {
            init_ib (wrap);
            range_is_image ();
        }

        /// Construct valid iteration region from ImageBuf and ROI.
        IteratorBase (const ImageBuf &ib, const ROI &roi, WrapMode wrap)
            : m_ib(&ib), m_tile(NULL), m_proxydata(NULL)
        {
            init_ib (wrap);
            if (roi.defined()) {
                m_rng_xbegin = roi.xbegin;
                m_rng_xend   = roi.xend;
                m_rng_ybegin = roi.ybegin;
                m_rng_yend   = roi.yend;
                m_rng_zbegin = roi.zbegin;
                m_rng_zend   = roi.zend;
            } else {
                range_is_image ();
            }
        }

        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel.
        IteratorBase (const ImageBuf &ib, int xbegin, int xend,
                      int ybegin, int yend, int zbegin, int zend,
                      WrapMode wrap)
            : m_ib(&ib), m_tile(NULL), m_proxydata(NULL)
        {
            init_ib (wrap);
            m_rng_xbegin = xbegin;
            m_rng_xend   = xend;
            m_rng_ybegin = ybegin;
            m_rng_yend   = yend;
            m_rng_zbegin = zbegin;
            m_rng_zend   = zend;
        }

        IteratorBase (const IteratorBase &i)
            : m_ib (i.m_ib),
              m_rng_xbegin(i.m_rng_xbegin), m_rng_xend(i.m_rng_xend), 
              m_rng_ybegin(i.m_rng_ybegin), m_rng_yend(i.m_rng_yend),
              m_rng_zbegin(i.m_rng_zbegin), m_rng_zend(i.m_rng_zend),
              m_tile(NULL), m_proxydata(i.m_proxydata)
        {
            init_ib (i.m_wrap);
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
            init_ib (i.m_wrap);
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

        /// Is the current location within the designated iteration range?
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

        /// Return the wrap mode
        WrapMode wrap () const { return m_wrap; }

        /// Explicitly point the iterator.  This results in an invalid
        /// iterator if outside the previously-designated region.
        void pos (int x_, int y_, int z_=0) {
            if (x_ == m_x+1 && x_ < m_rng_xend && y_ == m_y && z_ == m_z &&
                m_valid && m_exists) {
                // Special case for what is in effect just incrementing x
                // within the iteration region.
                m_x = x_;
                pos_xincr ();
                // Not necessary? m_exists = (x_ < m_img_xend);
                DASSERT ((x_ < m_img_xend) == m_exists);
                return;
            }
            bool v = valid(x_,y_,z_);
            bool e = exists(x_,y_,z_);
            if (m_localpixels) {
                if (e)
                    m_proxydata = (char *)m_ib->pixeladdr (x_, y_, z_);
                else {  // pixel not in data window
                    m_x = x_;  m_y = y_;  m_z = z_;
                    if (m_wrap == WrapBlack) {
                        m_proxydata = (char *)m_ib->blackpixel();
                    } else {
                        if (m_ib->do_wrap (x_, y_, z_, m_wrap))
                            m_proxydata = (char *)m_ib->pixeladdr (x_, y_, z_);
                        else
                            m_proxydata = (char *)m_ib->blackpixel();
                    }
                    m_valid = v;
                    m_exists = e;
                    return;
                }
            }
            else if (! m_deep)
                m_proxydata = (char *)m_ib->retile (x_, y_, z_, m_tile,
                                                    m_tilexbegin, m_tileybegin,
                                                    m_tilezbegin, m_tilexend,
                                                    e, m_wrap);
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

        /// Return the iteration range
        ROI range () const {
            return ROI (m_rng_xbegin, m_rng_xend, m_rng_ybegin, m_rng_yend,
                        m_rng_zbegin, m_rng_zend, 0, m_ib->nchannels());
        }

        /// Reset the iteration range for this iterator and reposition to
        /// the beginning of the range, but keep referring to the same
        /// image.
        void rerange (int xbegin, int xend, int ybegin, int yend,
                      int zbegin, int zend, WrapMode wrap=WrapDefault)
        {
            m_x = 1<<31;
            m_y = 1<<31;
            m_z = 1<<31;
            m_wrap = (wrap == WrapDefault ? WrapBlack : wrap);
            m_rng_xbegin = xbegin;
            m_rng_xend   = xend;
            m_rng_ybegin = ybegin;
            m_rng_yend   = yend;
            m_rng_zbegin = zbegin;
            m_rng_zend   = zend;
            pos (xbegin, ybegin, zbegin);
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
        int m_nchannels;
        size_t m_pixel_bytes;
        char *m_proxydata;
        WrapMode m_wrap;

        // Helper called by ctrs -- set up some locally cached values
        // that are copied or derived from the ImageBuf.
        void init_ib (WrapMode wrap) {
            const ImageSpec &spec (m_ib->spec());
            m_deep = spec.deep;
            m_localpixels = (m_ib->localpixels() != NULL);
            m_img_xbegin = spec.x; m_img_xend = spec.x+spec.width;
            m_img_ybegin = spec.y; m_img_yend = spec.y+spec.height;
            m_img_zbegin = spec.z; m_img_zend = spec.z+spec.depth;
            m_nchannels = spec.nchannels;
//            m_tilewidth = spec.tile_width;
            m_pixel_bytes = spec.pixel_bytes();
            m_x = 1<<31;
            m_y = 1<<31;
            m_z = 1<<31;
            m_wrap = (wrap == WrapDefault ? WrapBlack : wrap);
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
            bool e = (m_x < m_img_xend);
            if (m_localpixels) {
                if (e)
                    // Haven't run off the end, just increment
                    m_proxydata += m_pixel_bytes;
                else {
                    m_exists = false;
                    if (m_wrap == WrapBlack) {
                        m_proxydata = (char *)m_ib->blackpixel();
                    } else {
                        int x = m_x, y = m_y, z = m_z;
                        if (m_ib->do_wrap (x, y, z, m_wrap))
                            m_proxydata = (char *)m_ib->pixeladdr (x, y, z);
                        else
                            m_proxydata = (char *)m_ib->blackpixel();
                    }
                }
            } else if (m_deep) {
                m_proxydata = NULL;
            } else {
                // Cached image
                if (e && m_x < m_tilexend && m_tile)
                    // Haven't crossed a tile boundary, don't retile!
                    m_proxydata += m_pixel_bytes;
                else {
                    m_proxydata = (char *)m_ib->retile (m_x, m_y, m_z, m_tile,
                                    m_tilexbegin, m_tileybegin, m_tilezbegin,
                                    m_tilexend, e, m_wrap);
                    m_exists = e;
                }
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
        Iterator (ImageBuf &ib, WrapMode wrap=WrapDefault)
            : IteratorBase(ib,wrap)
        {
            pos (m_rng_xbegin,m_rng_ybegin,m_rng_zbegin);
        }
        /// Construct from an ImageBuf and a specific pixel index.
        /// The iteration range is the full image.
        Iterator (ImageBuf &ib, int x, int y, int z=0,
                  WrapMode wrap=WrapDefault)
            : IteratorBase(ib,wrap)
        {
            pos (x, y, z);
        }
        /// Construct read-write iteration region from ImageBuf and ROI.
        Iterator (ImageBuf &ib, const ROI &roi, WrapMode wrap=WrapDefault)
            : IteratorBase (ib, roi, wrap)
        {
            pos (m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
        }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel.
        Iterator (ImageBuf &ib, int xbegin, int xend,
                  int ybegin, int yend, int zbegin=0, int zend=1,
                  WrapMode wrap=WrapDefault)
            : IteratorBase(ib, xbegin, xend, ybegin, yend, zbegin, zend, wrap)
        {
            pos (m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
        }
        /// Copy constructor.
        ///
        Iterator (Iterator &i)
            : IteratorBase (i.m_ib, i.m_wrap)
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
        ConstIterator (const ImageBuf &ib, WrapMode wrap=WrapDefault)
            : IteratorBase(ib,wrap)
        {
            pos (m_rng_xbegin,m_rng_ybegin,m_rng_zbegin);
        }
        /// Construct from an ImageBuf and a specific pixel index.
        /// The iteration range is the full image.
        ConstIterator (const ImageBuf &ib, int x_, int y_, int z_=0,
                       WrapMode wrap=WrapDefault)
            : IteratorBase(ib,wrap)
        {
            pos (x_, y_, z_);
        }
        /// Construct read-only iteration region from ImageBuf and ROI.
        ConstIterator (const ImageBuf &ib, const ROI &roi,
                       WrapMode wrap=WrapDefault)
            : IteratorBase (ib, roi, wrap)
        {
            pos (m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
        }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel.
        ConstIterator (const ImageBuf &ib, int xbegin, int xend,
                       int ybegin, int yend, int zbegin=0, int zend=1,
                       WrapMode wrap=WrapDefault)
            : IteratorBase(ib, xbegin, xend, ybegin, yend, zbegin, zend, wrap)
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
                         int &tilexend, bool exists,
                         WrapMode wrap=WrapDefault) const;

    const void *blackpixel () const;

    // Given x,y,z known to be outside the pixel data range, and a wrap
    // mode, alter xyz to implement the wrap. Return true if the resulting
    // x,y,z is within the valid pixel data window, false if it still is
    // not.
    bool do_wrap (int &x, int &y, int &z, WrapMode wrap) const;

    /// Private and unimplemented.
    const ImageBuf& operator= (const ImageBuf &src);

    /// Add to the error message list for this IB.
    void append_error (const std::string& message) const;

};


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_IMAGEBUF_H

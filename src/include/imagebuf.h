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
#include "colortransfer.h"
#include "dassert.h"


OIIO_NAMESPACE_ENTER
{

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
    virtual bool read (int subimage=0, int miplevel=0, bool force=false,
                       TypeDesc convert=TypeDesc::UNKNOWN,
                       ProgressCallback progress_callback=NULL,
                       void *progress_callback_data=NULL);

    /// Initialize this ImageBuf with the named image file, and read its
    /// header to fill out the spec correctly.  Return true if this
    /// succeeded, false if the file could not be read.  But don't
    /// allocate or read the pixels.
    virtual bool init_spec (const std::string &filename,
                            int subimage, int miplevel);

    /// Save the image or a subset thereof, with override for filename
    /// ("" means use the original filename) and file format ("" indicates
    /// to infer it from the filename).  This uses ImageOutput
    /// underneath, so will write any file format for which an
    /// appropriate imageio plugin can be found.
    virtual bool save (const std::string &filename = std::string(),
                       const std::string &fileformat = std::string(),
                       ProgressCallback progress_callback=NULL,
                       void *progress_callback_data=NULL) const;

    /// Write the image to the open ImageOutput 'out'.  Return true if
    /// all went ok, false if there were errors writing.  It does NOT
    /// close the file when it's done (and so may be called in a loop to
    /// write a multi-image file).
    virtual bool write (ImageOutput *out,
                        ProgressCallback progress_callback=NULL,
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

    /// Return a read-only (const) reference to the image spec that
    /// describes the buffer.
    const ImageSpec & spec () const { return m_spec; }

    /// Return a read-only (const) reference to the "native" image spec
    /// (that describes the file, which may be slightly different than
    /// the spec of the ImageBuf, particularly if the IB is backed by an
    /// ImageCache that is imposing some particular data format or tile
    /// size).
    const ImageSpec & nativespec () const { return m_nativespec; }

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

    /// Return the index of the miplevel are we currently viewing
    ///
    int miplevel () const { return m_current_miplevel; }

    /// Return the number of miplevels of the current subimage.
    ///
    int nmiplevels () const { return m_nmiplevels; }

    /// Return the number of color channels in the image.
    ///
    int nchannels () const { return m_spec.nchannels; }

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
    void interppixel_NDC (float x, float y, float *pixel) const {
        interppixel (static_cast<float>(spec().x) + x * static_cast<float>(spec().width),
                     static_cast<float>(spec().y) + y * static_cast<float>(spec().height),
                     pixel);
    }

    /// Linearly interpolate at NDC (image) coordinates (x,y), where (0,0) is
    /// the upper left corner of the display window, (1,1) the lower
    /// right corner of the display window.
    void interppixel_NDC_full (float x, float y, float *pixel) const {
        interppixel (static_cast<float>(spec().full_x) + x * static_cast<float>(spec().full_width),
                     static_cast<float>(spec().full_y) + y * static_cast<float>(spec().full_height),
                     pixel);
    }

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
    bool copy_pixels (int xbegin_, int xend_, int ybegin_, int yend_,
                      std::vector<T> &result) const
    {
        result.resize (nchannels() * ((yend_-ybegin_)*(xend_-xbegin_)));
        return _copy_pixels (xbegin_, xend_, ybegin_, yend_, &result[0]);
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

    /// Return the beginning (minimum) z coordinate of the defined image.
    ///
    int zbegin () const { return spec().z; }

    /// Return the end (one past maximum) z coordinate of the defined image.
    ///
    int zend () const { return spec().z + std::max(spec().depth,1); }

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

    /// Return the minimum z coordinate of the defined image.
    ///
    int zmin () const { return spec().z; }

    /// Return the maximum z coordinate of the defined image.
    ///
    int zmax () const { return spec().z + std::max(spec().depth,1) - 1; }

    /// Set the "full" (a.k.a. display) window to [xbegin,xend) x
    /// [ybegin,yend) x [zbegin,zend).  If bordercolor is not NULL, also
    /// set the spec's "oiio:bordercolor" attribute.
    void set_full (int xbegin, int xend, int ybegin, int yend,
                   int zbegin, int zend, const float *bordercolor);

    bool pixels_valid (void) const { return m_pixels_valid; }

    TypeDesc pixeltype () const {
        return m_localpixels ? m_spec.format : m_cachedpixeltype;
    }

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
    ///   for (  ;  ! pixel.done();  ++pixel) {
    ///       for (int c = 0;  c < img.nchannels();  ++c) {
    ///           float x = pixel[c];
    ///           pixel[c] = ...;
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
            : m_ib(&ib), m_tile(NULL)
        {
            init_ib ();
            range_is_image ();
            pos (m_rng_xbegin,m_rng_ybegin,m_rng_zbegin);
        }
        /// Construct from an ImageBuf and a specific pixel index.
        /// The iteration range is the full image.
        Iterator (ImageBuf &ib, int x_, int y_, int z_=0)
            : m_ib(&ib), m_tile(NULL)
        {
            init_ib ();
            range_is_image ();
            pos (x_, y_, z_);
        }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel.  The
        /// iteration region will be clamped to the valid image range.
        Iterator (ImageBuf &ib, int xbegin, int xend,
                  int ybegin, int yend, int zbegin=0, int zend=1)
            : m_ib(&ib), m_tile(NULL)
        {
            init_ib ();
            m_rng_xbegin = std::max (xbegin, m_img_xbegin); 
            m_rng_xend   = std::min (xend,   m_img_xend);
            m_rng_ybegin = std::max (ybegin, m_img_ybegin);
            m_rng_yend   = std::min (yend,   m_img_yend);
            m_rng_zbegin = std::max (zbegin, m_img_zbegin);
            m_rng_zend   = std::min (zend,   m_img_zend);
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
            : m_ib(&ib), m_tile(NULL)
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
            pos (m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
        }
        /// Copy constructor.
        ///
        Iterator (Iterator &i)
            : m_ib (i.m_ib),
              m_rng_xbegin(i.m_rng_xbegin), m_rng_xend(i.m_rng_xend), 
              m_rng_ybegin(i.m_rng_ybegin), m_rng_yend(i.m_rng_yend),
              m_rng_zbegin(i.m_rng_zbegin), m_rng_zend(i.m_rng_zend),
              m_tile(NULL)
        {
            init_ib ();
            pos (i.m_x, i.m_y, i.m_z);
        }

        ~Iterator () {
            if (m_tile)
                m_ib->imagecache()->release_tile (m_tile);
        }

        /// Explicitly point the iterator.  This results in an invalid
        /// iterator if outside the previously-designated region.
        void pos (int x_, int y_, int z_=0) {
            bool v = valid(x_,y_,z_);
            bool e = exists(x_,y_,z_);
            if (! e)
                m_proxy.set (NULL);
            else if (m_ib->localpixels())
                m_proxy.set ((BUFT *)m_ib->pixeladdr (x_, y_, z_));
            else
                m_proxy.set ((BUFT *)m_ib->retile (x_, y_, z_,
                                         m_tile, m_tilexbegin,
                                         m_tileybegin, m_tilezbegin));
            m_x = x_;  m_y = y_;  m_z = z_;
            m_valid = v;
            m_exists = e;
        }

        /// Increment to the next pixel in the region.
        ///
        void operator++ () {
            if (++m_x >= m_rng_xend) {
                m_x = m_rng_xbegin;
                if (++m_y >= m_rng_yend) {
                    m_y = m_rng_ybegin;
                    ++m_z;
                }
            } else {
                // Special case: we only incremented x, didn't change y
                // or z, and the previous position was within the data
                // window.  Call a shortcut version of pos.
                if (m_exists) {
                    pos_xincr ();
                    return;
                }
            }
            pos (m_x, m_y, m_z);
        }
        /// Increment to the next pixel in the region.
        ///
        void operator++ (int) {
            ++(*this);
        }

        /// Assign one Iterator to another
        ///
        const Iterator & operator= (const Iterator &i) {
            if (m_tile)
                m_ib->imagecache()->release_tile (m_tile);
            m_tile = NULL;
            m_ib = i.m_ib;
            init_ib ();
            m_rng_xbegin = i.m_rng_xbegin;  m_rng_xend = i.m_rng_xend;
            m_rng_ybegin = i.m_rng_ybegin;  m_rng_yend = i.m_rng_yend;
            m_rng_zbegin = i.m_rng_zbegin;  m_rng_zend = i.m_rng_zend;
            pos (i.m_x, i.m_y, i.m_z);
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
        bool m_valid, m_exists;
        // Image boundaries
        int m_img_xbegin, m_img_xend, m_img_ybegin, m_img_yend,
            m_img_zbegin, m_img_zend;
        // Iteration range
        int m_rng_xbegin, m_rng_xend, m_rng_ybegin, m_rng_yend,
            m_rng_zbegin, m_rng_zend;
        int m_x, m_y, m_z;
        DataArrayProxy<BUFT,USERT> m_proxy;
        ImageCache::Tile *m_tile;
        int m_tilexbegin, m_tileybegin, m_tilezbegin;
        int m_nchannels, m_tilewidth;

        // Helper called by ctrs -- set up some locally cached values
        // that are copied or derived from the ImageBuf.
        void init_ib () {
            m_img_xbegin = m_ib->xbegin(); m_img_xend = m_ib->xend();
            m_img_ybegin = m_ib->ybegin(); m_img_yend = m_ib->yend();
            m_img_zbegin = m_ib->zbegin(); m_img_zend = m_ib->zend();
            m_nchannels = m_ib->spec().nchannels;
            m_tilewidth = m_ib->spec().tile_width;
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
                m_proxy.set (NULL);
                m_exists = false;
            } else if (m_ib->localpixels()) {
                m_proxy += m_nchannels;
            } else if (m_x < m_tilexbegin+m_tilewidth) {
                // Haven't crossed a tile boundary, don't retile!
                m_proxy += m_nchannels;
            } else {
                m_proxy.set ((BUFT *)m_ib->retile (m_x, m_y, m_z, m_tile,
                                    m_tilexbegin, m_tileybegin, m_tilezbegin));
            }
        }
    };


    /// Just like an ImageBuf::Iterator, except that it refers to a
    /// const ImageBuf.
    template<typename BUFT, typename USERT=float>
    class ConstIterator {
    public:
        /// Construct from just an ImageBuf -- iterate over the whole
        /// region, starting with the upper left pixel of the region.
        ConstIterator (const ImageBuf &ib)
            : m_ib(&ib), m_tile(NULL)
        {
            init_ib ();
            range_is_image ();
            pos (m_rng_xbegin,m_rng_ybegin,m_rng_zbegin);
        }
        /// Construct from an ImageBuf and a specific pixel index.
        /// The iteration range is the full image.
        ConstIterator (const ImageBuf &ib, int x_, int y_, int z_=0)
            : m_ib(&ib), m_tile(NULL)
        {
            init_ib ();
            range_is_image ();
            pos (x_, y_, z_);
        }
        /// Construct from an ImageBuf and designated region -- iterate
        /// over region, starting with the upper left pixel.  The
        /// iteration region will be clamped to the valid image range.
        ConstIterator (const ImageBuf &ib, int xbegin, int xend,
                       int ybegin, int yend, int zbegin=0, int zend=1)
            : m_ib(&ib), m_tile(NULL)
        {
            init_ib ();
            m_rng_xbegin = std::max (xbegin, m_img_xbegin); 
            m_rng_xend   = std::min (xend,   m_img_xend);
            m_rng_ybegin = std::max (ybegin, m_img_ybegin);
            m_rng_yend   = std::min (yend,   m_img_yend);
            m_rng_zbegin = std::max (zbegin, m_img_zbegin);
            m_rng_zend   = std::min (zend,   m_img_zend);
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
            : m_ib(&ib), m_tile(NULL)
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
            pos (m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
        }
        /// Copy constructor.
        ///
        ConstIterator (const ConstIterator &i)
            : m_ib (i.m_ib),
              m_rng_xbegin(i.m_rng_xbegin), m_rng_xend(i.m_rng_xend), 
              m_rng_ybegin(i.m_rng_ybegin), m_rng_yend(i.m_rng_yend),
              m_rng_zbegin(i.m_rng_zbegin), m_rng_zend(i.m_rng_zend),
              m_tile(NULL)
        {
            init_ib ();
            pos (i.m_x, i.m_y, i.m_z);
        }

        ~ConstIterator () {
            if (m_tile)
                m_ib->imagecache()->release_tile (m_tile);
        }

        /// Explicitly point the iterator.  This results in an invalid
        /// iterator if outside the previously-designated region.
        void pos (int x_, int y_, int z_=0) {
            bool v = valid(x_,y_,z_);
            bool e = exists(x_,y_,z_);
            if (! e)
                m_proxy.set (NULL);
            else if (m_ib->localpixels())
                m_proxy.set ((BUFT *)m_ib->pixeladdr (x_, y_, z_));
            else
                m_proxy.set ((BUFT *)m_ib->retile (x_, y_, z_,
                                         m_tile, m_tilexbegin,
                                         m_tileybegin, m_tilezbegin));
            m_x = x_;  m_y = y_;  m_z = z_;
            m_valid = v;
            m_exists = e;
        }

        /// Increment to the next pixel in the region.
        ///
        void operator++ () {
            if (++m_x >= m_rng_xend) {
                m_x = m_rng_xbegin;
                if (++m_y >= m_rng_yend) {
                    m_y = m_rng_ybegin;
                    ++m_z;
                }
            } else {
                // Special case: we only incremented x, didn't change y
                // or z, and the previous position was within the data
                // window.  Call a shortcut version of pos.
                if (m_exists) {
                    pos_xincr ();
                    return;
                }
            }
            pos (m_x, m_y, m_z);
        }
        /// Increment to the next pixel in the region.
        ///
        void operator++ (int) {
            ++(*this);
        }

        /// Assign one ConstIterator to another
        ///
        const ConstIterator & operator= (const ConstIterator &i) {
            if (m_tile)
                m_ib->imagecache()->release_tile (m_tile);
            m_tile = NULL;
            m_ib = i.m_ib;
            init_ib ();
            m_rng_xbegin = i.m_rng_xbegin;  m_rng_xend = i.m_rng_xend;
            m_rng_ybegin = i.m_rng_ybegin;  m_rng_yend = i.m_rng_yend;
            m_rng_zbegin = i.m_rng_zbegin;  m_rng_zend = i.m_rng_zend;
            pos (i.m_x, i.m_y, i.m_z);
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
        ///
        bool done () const {
            // We're "done" if we are both invalid and in exactly the
            // spot that we would end up after iterating off of the last
            // pixel in the range.  (The m_valid test is just a quick
            // early-out for when we're in the correct pixel range.)
            return (m_valid == false && m_x == m_rng_xbegin &&
                    m_y == m_rng_ybegin && m_z == m_rng_zend);
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
        bool m_valid, m_exists;
        // Image boundaries
        int m_img_xbegin, m_img_xend, m_img_ybegin, m_img_yend,
            m_img_zbegin, m_img_zend;
        // Iteration range
        int m_rng_xbegin, m_rng_xend, m_rng_ybegin, m_rng_yend,
            m_rng_zbegin, m_rng_zend;
        int m_x, m_y, m_z;
        ConstDataArrayProxy<BUFT,USERT> m_proxy;
        ImageCache::Tile *m_tile;
        int m_tilexbegin, m_tileybegin, m_tilezbegin;
        int m_nchannels, m_tilewidth;

        // Helper called by ctrs -- set up some locally cached values
        // that are copied or derived from the ImageBuf.
        void init_ib () {
            m_img_xbegin = m_ib->xbegin(); m_img_xend = m_ib->xend();
            m_img_ybegin = m_ib->ybegin(); m_img_yend = m_ib->yend();
            m_img_zbegin = m_ib->zbegin(); m_img_zend = m_ib->zend();
            m_nchannels = m_ib->spec().nchannels;
            m_tilewidth = m_ib->spec().tile_width;
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
                m_proxy.set (NULL);
                m_exists = false;
            } else if (m_ib->localpixels()) {
                m_proxy += m_nchannels;
            } else if (m_x < m_tilexbegin+m_tilewidth) {
                // Haven't crossed a tile boundary, don't retile!
                m_proxy += m_nchannels;
            } else {
                m_proxy.set ((BUFT *)m_ib->retile (m_x, m_y, m_z, m_tile,
                                    m_tilexbegin, m_tileybegin, m_tilezbegin));
            }
        }
    };


protected:
    ustring m_name;              ///< Filename of the image
    ustring m_fileformat;        ///< File format name
    int m_nsubimages;            ///< How many subimages are there?
    int m_current_subimage;      ///< Current subimage we're viewing
    int m_current_miplevel;      ///< Current miplevel we're viewing
    int m_nmiplevels;            ///< # of MIP levels in the current subimage
    ImageSpec m_spec;            ///< Describes the image (size, etc)
    ImageSpec m_nativespec;      ///< Describes the true native image
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
    const void *pixeladdr (int x, int y) const { return pixeladdr (x, y, 0); }

    // Return the address where pixel (x,y,z) is stored in the image buffer.
    // Use with extreme caution!
    const void *pixeladdr (int x, int y, int z) const;

    // Return the address where pixel (x,y) is stored in the image buffer.
    // Use with extreme caution!
    void *pixeladdr (int x, int y) { return pixeladdr (x, y, 0); }

    // Return the address where pixel (x,y,z) is stored in the image buffer.
    // Use with extreme caution!
    void *pixeladdr (int x, int y, int z);

    // Reset the ImageCache::Tile * to reserve and point to the correct
    // tile for the given pixel, and return the ptr to the actual pixel
    // within the tile.
    const void * retile (int x, int y, int z,
                         ImageCache::Tile* &tile, int &tilexbegin,
                         int &tileybegin, int &tilezbegin) const;
};


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_IMAGEBUF_H

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
/// Implementation of ImageBuf class.


#include <iostream>

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>
#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>

#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "imagebufalgo_util.h"
#include "imagecache.h"
#include "dassert.h"
#include "strutil.h"
#include "fmath.h"
#include "thread.h"

OIIO_NAMESPACE_ENTER
{


static atomic_ll IB_local_mem_current;



ROI
get_roi (const ImageSpec &spec)
{
    return ROI (spec.x, spec.x + spec.width,
                spec.y, spec.y + spec.height,
                spec.z, spec.z + spec.depth,
                0, spec.nchannels);
}



ROI
get_roi_full (const ImageSpec &spec)
{
    return ROI (spec.full_x, spec.full_x + spec.full_width,
                spec.full_y, spec.full_y + spec.full_height,
                spec.full_z, spec.full_z + spec.full_depth,
                0, spec.nchannels);
}



void
set_roi (ImageSpec &spec, const ROI &newroi)
{
    spec.x = newroi.xbegin;
    spec.y = newroi.ybegin;
    spec.z = newroi.zbegin;
    spec.width = newroi.width();
    spec.height = newroi.height();
    spec.depth = newroi.depth();
}



void
set_roi_full (ImageSpec &spec, const ROI &newroi)
{
    spec.full_x = newroi.xbegin;
    spec.full_y = newroi.ybegin;
    spec.full_z = newroi.zbegin;
    spec.full_width = newroi.width();
    spec.full_height = newroi.height();
    spec.full_depth = newroi.depth();
}



ROI
roi_union (const ROI &A, const ROI &B)
{
    return ROI (std::min (A.xbegin, B.xbegin), std::max (A.xend, B.xend),
                std::min (A.ybegin, B.ybegin), std::max (A.yend, B.yend),
                std::min (A.zbegin, B.zbegin), std::max (A.zend, B.zend),
                std::min (A.chbegin, B.chbegin), std::max (A.chend, B.chend));
}



ROI
roi_intersection (const ROI &A, const ROI &B)
{
    return ROI (std::max (A.xbegin, B.xbegin), std::min (A.xend, B.xend),
                std::max (A.ybegin, B.ybegin), std::min (A.yend, B.yend),
                std::max (A.zbegin, B.zbegin), std::min (A.zend, B.zend),
                std::max (A.chbegin, B.chbegin), std::min (A.chend, B.chend));
}




// Expansion of the opaque type that hides all the ImageBuf implementation
// detail.
class ImageBufImpl {
public:
    ImageBufImpl (const std::string &filename, int subimage, int miplevel,
                  ImageCache *imagecache=NULL, const ImageSpec *spec=NULL,
                  void *buffer=NULL);
    ImageBufImpl (const ImageBufImpl &src);
    ~ImageBufImpl ();

    void clear ();
    void reset (const std::string &name, int subimage, int miplevel,
                ImageCache *imagecache);
    void reset (const std::string &name, const ImageSpec &spec);
    void alloc (const ImageSpec &spec);
    void realloc ();
    bool init_spec (const std::string &filename, int subimage, int miplevel);
    bool read (int subimage=0, int miplevel=0, bool force=false,
               TypeDesc convert=TypeDesc::UNKNOWN,
               ProgressCallback progress_callback=NULL,
               void *progress_callback_data=NULL);
    void copy_metadata (const ImageBufImpl &src);

    // Error reporting for ImageBuf: call this with printf-like
    // arguments.  Note however that this is fully typesafe!
    // void error (const char *format, ...)
    TINYFORMAT_WRAP_FORMAT (void, error, const,
        std::ostringstream msg;, msg, append_error(msg.str());)

    void append_error (const std::string& message) const;

    ImageBuf::IBStorage storage () const { return m_storage; }

    TypeDesc pixeltype () const {
        validate_spec ();
        return m_localpixels ? m_spec.format : m_cachedpixeltype;
    }

    DeepData *deepdata () {
        validate_pixels();
        return m_spec.deep ? &m_deepdata : NULL;
    }
    const DeepData *deepdata () const {
        validate_pixels();
        return m_spec.deep ? &m_deepdata : NULL;
    }
    bool initialized () const {
        return m_spec_valid && m_storage != ImageBuf::UNINITIALIZED;
    }
    bool cachedpixels () const { return m_storage == ImageBuf::IMAGECACHE; }

    const void *pixeladdr (int x, int y, int z) const;
    void *pixeladdr (int x, int y, int z);

    const void *retile (int x, int y, int z, ImageCache::Tile* &tile,
                    int &tilexbegin, int &tileybegin, int &tilezbegin,
                    int &tilexend, bool exists, ImageBuf::WrapMode wrap) const;

    bool do_wrap (int &x, int &y, int &z, ImageBuf::WrapMode wrap) const;

    const void *blackpixel () const {
        validate_spec ();
        return &m_blackpixel[0];
    }

    bool validate_spec () const {
        if (m_spec_valid)
            return true;
        if (! m_name.size())
            return false;
        spin_lock lock (m_valid_mutex); // prevent multiple init_spec
        if (m_spec_valid)
            return true;
        ImageBufImpl *imp = const_cast<ImageBufImpl *>(this);
        if (imp->m_current_subimage < 0)
            imp->m_current_subimage = 0;
        if (imp->m_current_miplevel < 0)
            imp->m_current_miplevel = 0;
        return imp->init_spec(m_name.string(),
                              m_current_subimage, m_current_miplevel);
    }

    bool validate_pixels () const {
        if (m_pixels_valid)
            return true;
        if (! m_name.size())
            return true;
        spin_lock lock (m_valid_mutex); // prevent multiple read()
        if (m_pixels_valid)
            return true;
        ImageBufImpl *imp = const_cast<ImageBufImpl *>(this);
        if (imp->m_current_subimage < 0)
            imp->m_current_subimage = 0;
        if (imp->m_current_miplevel < 0)
            imp->m_current_miplevel = 0;
        return imp->read (m_current_subimage, m_current_miplevel);
    }

    const ImageSpec & spec () const {
        validate_spec ();
        return m_spec;
    }
    const ImageSpec & nativespec () const {
        validate_spec ();
        return m_nativespec;
    }
    ImageSpec & specmod () {
        validate_spec ();
        return m_spec;
    }

private:
    ImageBuf::IBStorage m_storage; ///< Pixel storage class
    ustring m_name;              ///< Filename of the image
    ustring m_fileformat;        ///< File format name
    int m_nsubimages;            ///< How many subimages are there?
    int m_current_subimage;      ///< Current subimage we're viewing
    int m_current_miplevel;      ///< Current miplevel we're viewing
    int m_nmiplevels;            ///< # of MIP levels in the current subimage
    ImageSpec m_spec;            ///< Describes the image (size, etc)
    ImageSpec m_nativespec;      ///< Describes the true native image
    boost::scoped_array<char> m_pixels; ///< Pixel data, if local and we own it
    char *m_localpixels;         ///< Pointer to local pixels
    mutable spin_mutex m_valid_mutex;
    mutable bool m_spec_valid;   ///< Is the spec valid
    mutable bool m_pixels_valid; ///< Image is valid
    bool m_badfile;              ///< File not found
    int m_orientation;           ///< Orientation of the image
    float m_pixelaspect;         ///< Pixel aspect ratio of the image
    size_t m_pixel_bytes;
    size_t m_scanline_bytes;
    size_t m_plane_bytes;
    ImageCache *m_imagecache;    ///< ImageCache to use
    TypeDesc m_cachedpixeltype;  ///< Data type stored in the cache
    DeepData m_deepdata;         ///< Deep data
    size_t m_allocated_size;     ///< How much memory we've allocated
    std::vector<char> m_blackpixel; ///< Pixel-sized zero bytes
    TypeDesc m_write_format;     /// Format to use for write()
    int m_write_tile_width;
    int m_write_tile_height;
    int m_write_tile_depth;
    mutable std::string m_err;   ///< Last error message

    const ImageBufImpl operator= (const ImageBufImpl &src); // unimplemented
    friend class ImageBuf;
};



ImageBufImpl::ImageBufImpl (const std::string &filename,
                            int subimage, int miplevel,
                            ImageCache *imagecache,
                            const ImageSpec *spec, void *buffer)
    : m_storage(ImageBuf::UNINITIALIZED),
      m_name(filename), m_nsubimages(0),
      m_current_subimage(subimage), m_current_miplevel(miplevel),
      m_localpixels(NULL),
      m_spec_valid(false), m_pixels_valid(false),
      m_badfile(false), m_orientation(1), m_pixelaspect(1), 
      m_pixel_bytes(0), m_scanline_bytes(0), m_plane_bytes(0),
      m_imagecache(imagecache), m_allocated_size(0),
      m_write_format(TypeDesc::UNKNOWN), m_write_tile_width(0),
      m_write_tile_height(0), m_write_tile_depth(1)
{
    if (spec) {
        m_spec = *spec;
        m_nativespec = *spec;
        m_pixel_bytes = spec->pixel_bytes();
        m_scanline_bytes = spec->scanline_bytes();
        m_plane_bytes = clamped_mult64 (m_scanline_bytes, (imagesize_t)m_spec.height);
        m_blackpixel.resize (m_pixel_bytes, 0);
        if (buffer) {
            m_localpixels = (char *)buffer;
            m_storage = ImageBuf::APPBUFFER;
            m_pixels_valid = true;
        } else {
            m_storage = ImageBuf::LOCALBUFFER;
        }
        m_spec_valid = true;
    } else if (filename.length() > 0) {
        ASSERT (buffer == NULL);
        // If a filename was given, read the spec and set it up as an
        // ImageCache-backed image.  Reallocate later if an explicit read()
        // is called to force read into a local buffer.
        read (subimage, miplevel);
    } else {
        ASSERT (buffer == NULL);
    }
}



ImageBufImpl::ImageBufImpl (const ImageBufImpl &src)
    : m_storage(src.m_storage),
      m_name(src.m_name), m_fileformat(src.m_fileformat),
      m_nsubimages(src.m_nsubimages),
      m_current_subimage(src.m_current_subimage),
      m_current_miplevel(src.m_current_miplevel),
      m_nmiplevels(src.m_nmiplevels),
      m_spec(src.m_spec), m_nativespec(src.m_nativespec),
      m_pixels(src.m_localpixels ? new char [src.m_spec.image_bytes()] : NULL),
      m_localpixels(m_pixels.get()),
      m_badfile(src.m_badfile),
      m_orientation(src.m_orientation),
      m_pixelaspect(src.m_pixelaspect),
      m_pixel_bytes(src.m_pixel_bytes),
      m_scanline_bytes(src.m_scanline_bytes),
      m_plane_bytes(src.m_plane_bytes),
      m_imagecache(src.m_imagecache),
      m_cachedpixeltype(src.m_cachedpixeltype),
      m_deepdata(src.m_deepdata),
      m_blackpixel(src.m_blackpixel),
      m_write_format(src.m_write_format),
      m_write_tile_width(src.m_write_tile_width),
      m_write_tile_height(src.m_write_tile_height),
      m_write_tile_depth(src.m_write_tile_depth)
{
    m_spec_valid = src.m_spec_valid;
    m_pixels_valid = src.m_pixels_valid;
    m_allocated_size = src.m_localpixels ? src.spec().image_bytes() : 0;
    IB_local_mem_current += m_allocated_size;
    if (src.m_localpixels) {
        // Source had the image fully in memory (no cache)
        if (m_storage == ImageBuf::APPBUFFER) {
            // Source just wrapped the client app's pixels
            ASSERT (0 && "ImageBuf wrapping client buffer not yet supported");
        } else {
            // We own our pixels -- copy from source
            memcpy (m_pixels.get(), src.m_pixels.get(), m_spec.image_bytes());
        }
    } else {
        // Source was cache-based or deep
        // nothing else to do
    }
}



ImageBufImpl::~ImageBufImpl ()
{
    // Do NOT destroy m_imagecache here -- either it was created
    // externally and passed to the ImageBuf ctr or reset() method, or
    // else init_spec requested the system-wide shared cache, which
    // does not need to be destroyed.
    IB_local_mem_current -= m_allocated_size;
}



ImageBuf::ImageBuf ()
    : m_impl (new ImageBufImpl (std::string(), -1, -1, NULL))
{
}



ImageBuf::ImageBuf (const std::string &filename, int subimage, int miplevel,
                    ImageCache *imagecache)
    : m_impl (new ImageBufImpl (filename, subimage, miplevel, imagecache))
{
}



ImageBuf::ImageBuf (const std::string &filename, ImageCache *imagecache)
    : m_impl (new ImageBufImpl (filename, 0, 0, imagecache))
{
}



ImageBuf::ImageBuf (const ImageSpec &spec)
    : m_impl (new ImageBufImpl (std::string(), 0, 0, NULL, &spec))
{
    alloc (spec);
}



ImageBuf::ImageBuf (const std::string &filename, const ImageSpec &spec)
    : m_impl (new ImageBufImpl (filename, 0, 0, NULL, &spec))
{
    alloc (spec);
}



ImageBuf::ImageBuf (const std::string &filename, const ImageSpec &spec,
                    void *buffer)
    : m_impl (new ImageBufImpl (filename, 0, 0, NULL, &spec, buffer))
{
}



ImageBuf::ImageBuf (const ImageSpec &spec, void *buffer)
    : m_impl (new ImageBufImpl (std::string(), 0, 0, NULL, &spec, buffer))
{
}



ImageBuf::ImageBuf (const ImageBuf &src)
    : m_impl (new ImageBufImpl (*src.impl()))
{
}



ImageBuf::~ImageBuf ()
{
    delete m_impl; m_impl = NULL;
}



static spin_mutex err_mutex;      ///< Protect m_err fields


bool
ImageBuf::has_error () const
{
    spin_lock lock (err_mutex);
    return ! impl()->m_err.empty();
}



std::string
ImageBuf::geterror (void) const
{
    spin_lock lock (err_mutex);
    std::string e = impl()->m_err;
    impl()->m_err.clear();
    return e;
}



void
ImageBufImpl::append_error (const std::string &message) const
{
    spin_lock lock (err_mutex);
    ASSERT (m_err.size() < 1024*1024*16 &&
            "Accumulated error messages > 16MB. Try checking return codes!");
    if (m_err.size() && m_err[m_err.size()-1] != '\n')
        m_err += '\n';
    m_err += message;
}



void
ImageBuf::append_error (const std::string &message) const
{
    impl()->append_error (message);
}



ImageBuf::IBStorage
ImageBuf::storage () const
{
    return impl()->storage ();
}



void
ImageBufImpl::clear ()
{
    m_storage = ImageBuf::UNINITIALIZED;
    m_name.clear ();
    m_fileformat.clear ();
    m_nsubimages = 0;
    m_current_subimage = -1;
    m_current_miplevel = -1;
    m_spec = ImageSpec ();
    m_nativespec = ImageSpec ();
    m_pixels.reset ();
    m_localpixels = NULL;
    m_spec_valid = false;
    m_pixels_valid = false;
    m_badfile = false;
    m_orientation = 1;
    m_pixelaspect = 1;
    m_pixel_bytes = 0;
    m_scanline_bytes = 0;
    m_plane_bytes = 0;
    m_imagecache = NULL;
    m_deepdata.free ();
    m_blackpixel.clear ();
    m_write_format = TypeDesc::UNKNOWN;
    m_write_tile_width = 0;
    m_write_tile_height = 0;
    m_write_tile_depth = 0;
}



void
ImageBuf::clear ()
{
    impl()->clear ();
}



void
ImageBufImpl::reset (const std::string &filename, int subimage,
                     int miplevel, ImageCache *imagecache)
{
    clear ();
    m_name = ustring (filename);
    m_current_subimage = subimage;
    m_current_miplevel = miplevel;
    if (imagecache)
        m_imagecache = imagecache;

    if (m_name.length() > 0) {
        // If a filename was given, read the spec and set it up as an
        // ImageCache-backed image.  Reallocate later if an explicit read()
        // is called to force read into a local buffer.
        read (subimage, miplevel);
    }
}



void
ImageBuf::reset (const std::string &filename, int subimage, int miplevel,
                 ImageCache *imagecache)
{
    impl()->reset (filename, subimage, miplevel, imagecache);
}



void
ImageBuf::reset (const std::string &filename, ImageCache *imagecache)
{
    impl()->reset (filename, 0, 0, imagecache);
}



void
ImageBufImpl::reset (const std::string &filename, const ImageSpec &spec)
{
    clear ();
    m_name = ustring (filename);
    m_current_subimage = 0;
    m_current_miplevel = 0;
    alloc (spec);
}



void
ImageBuf::reset (const std::string &filename, const ImageSpec &spec)
{
    impl()->reset (filename, spec);
}



void
ImageBuf::reset (const ImageSpec &spec)
{
    impl()->reset (std::string(), spec);
}



void
ImageBufImpl::realloc ()
{
    IB_local_mem_current -= m_allocated_size;
    m_allocated_size = m_spec.deep ? size_t(0) : m_spec.image_bytes ();
    IB_local_mem_current += m_allocated_size;
    m_pixels.reset (m_allocated_size ? new char [m_allocated_size] : NULL);
    m_localpixels = m_pixels.get();
    m_storage = m_allocated_size ? ImageBuf::LOCALBUFFER : ImageBuf::UNINITIALIZED;
    m_pixel_bytes = m_spec.pixel_bytes();
    m_scanline_bytes = m_spec.scanline_bytes();
    m_plane_bytes = clamped_mult64 (m_scanline_bytes, (imagesize_t)m_spec.height);
    m_blackpixel.resize (m_pixel_bytes, 0);
    if (m_allocated_size)
        m_pixels_valid = true;
#if 0
    std::cerr << "ImageBuf " << m_name << " local allocation: " << m_allocated_size << "\n";
#endif
}



void
ImageBufImpl::alloc (const ImageSpec &spec)
{
    m_spec = spec;

    // Preclude a nonsensical size
    m_spec.width = std::max (1, m_spec.width);
    m_spec.height = std::max (1, m_spec.height);
    m_spec.depth = std::max (1, m_spec.depth);
    m_spec.nchannels = std::max (1, m_spec.nchannels);

    m_nativespec = spec;
    realloc ();
    m_spec_valid = true;
}



void
ImageBuf::alloc (const ImageSpec &spec)
{
    impl()->alloc (spec);
}



void
ImageBuf::copy_from (const ImageBuf &src)
{
    if (this == &src)
        return;
    const ImageBufImpl *srcimpl (src.impl());
    srcimpl->validate_pixels ();
    const ImageSpec &srcspec (srcimpl->spec());
    ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    ASSERT (spec.width == srcspec.width &&
            spec.height == srcspec.height &&
            spec.depth == srcspec.depth &&
            spec.nchannels == srcspec.nchannels);
    impl->realloc ();
    if (spec.deep)
        impl->m_deepdata = srcimpl->m_deepdata;
    else
        src.get_pixels (src.xbegin(), src.xend(), src.ybegin(), src.yend(),
                        src.zbegin(), src.zend(), spec.format,
                        impl->m_localpixels);
}



bool
ImageBufImpl::init_spec (const std::string &filename, int subimage, int miplevel)
{
    if (!m_badfile && m_spec_valid
            && m_current_subimage >= 0 && m_current_miplevel >= 0
            && m_name == filename && m_current_subimage == subimage
            && m_current_miplevel == miplevel)
        return true;   // Already done

    if (! m_imagecache) {
        m_imagecache = ImageCache::create (true /* shared cache */);
    }

    m_pixels_valid = false;
    m_name = filename;
    m_nsubimages = 0;
    m_nmiplevels = 0;
    static ustring s_subimages("subimages"), s_miplevels("miplevels");
    static ustring s_fileformat("fileformat");
    m_imagecache->get_image_info (m_name, subimage, miplevel, s_subimages,
                                  TypeDesc::TypeInt, &m_nsubimages);
    m_imagecache->get_image_info (m_name, subimage, miplevel, s_miplevels,
                                  TypeDesc::TypeInt, &m_nmiplevels);
    const char *fmt = NULL;
    m_imagecache->get_image_info (m_name, subimage, miplevel,
                                  s_fileformat, TypeDesc::TypeString, &fmt);
    m_fileformat = ustring(fmt);
    m_imagecache->get_imagespec (m_name, m_spec, subimage, miplevel);
    m_imagecache->get_imagespec (m_name, m_nativespec, subimage, miplevel, true);
    m_pixel_bytes = m_spec.pixel_bytes();
    m_scanline_bytes = m_spec.scanline_bytes();
    m_plane_bytes = clamped_mult64 (m_scanline_bytes, (imagesize_t)m_spec.height);
    m_blackpixel.resize (m_pixel_bytes, 0);

    // Subtlety: m_nativespec will have the true formats of the file, but
    // we rig m_spec to reflect what it will look like in the cache.
    // This may make m_spec appear to change if there's a subsequent read()
    // that forces a full read into local memory, but what else can we do?
    // It causes havoc for it to suddenly change in the other direction
    // when the file is lazily read.
    int peltype = TypeDesc::UNKNOWN;
    m_imagecache->get_image_info (m_name, subimage, miplevel,
                                  ustring("cachedpixeltype"),
                                  TypeDesc::TypeInt, &peltype);
    if (peltype != TypeDesc::UNKNOWN) {
        m_spec.format = (TypeDesc::BASETYPE)peltype;
        m_spec.channelformats.clear();
    }

    if (m_nsubimages) {
        m_badfile = false;
        m_orientation = m_spec.get_int_attribute ("orientation", 1);
        m_pixelaspect = m_spec.get_float_attribute ("pixelaspectratio", 1.0f);
        m_current_subimage = subimage;
        m_current_miplevel = miplevel;
        m_spec_valid = true;
    } else {
        m_badfile = true;
        m_current_subimage = -1;
        m_current_miplevel = -1;
        m_err = m_imagecache->geterror ();
        m_spec_valid = false;
        // std::cerr << "ImageBuf ERROR: " << m_err << "\n";
    }

    return !m_badfile;
}



bool
ImageBuf::init_spec (const std::string &filename, int subimage, int miplevel)
{
    return impl()->init_spec (filename, subimage, miplevel);
}



bool
ImageBufImpl::read (int subimage, int miplevel, bool force, TypeDesc convert,
                    ProgressCallback progress_callback,
                    void *progress_callback_data)
{
    if (! m_name.length())
        return true;

    if (m_pixels_valid && !force &&
            subimage == m_current_subimage && miplevel == m_current_miplevel)
        return true;

    if (! init_spec (m_name.string(), subimage, miplevel)) {
        m_badfile = true;
        m_spec_valid = false;
        return false;
    }

    m_current_subimage = subimage;
    m_current_miplevel = miplevel;

    if (m_spec.deep) {
        boost::scoped_ptr<ImageInput> input (ImageInput::open (m_name.string()));
        if (! input) {
            error ("%s", OIIO::geterror());
            return false;
        }
        ImageSpec dummyspec;
        if (! input->seek_subimage (subimage, miplevel, dummyspec)) {
            error ("%s", input->geterror());
            return false;
        }
        if (! input->read_native_deep_image (m_deepdata)) {
            error ("%s", input->geterror());
            return false;
        }
        m_spec = m_nativespec;   // Deep images always use native data
        m_pixels_valid = true;
        m_storage = ImageBuf::LOCALBUFFER;
        return true;
    }

    // If we don't already have "local" pixels, and we aren't asking to
    // convert the pixels to a specific (and different) type, then take an
    // early out by relying on the cache.
    int peltype = TypeDesc::UNKNOWN;
    m_imagecache->get_image_info (m_name, subimage, miplevel,
                                  ustring("cachedpixeltype"),
                                  TypeDesc::TypeInt, &peltype);
    m_cachedpixeltype = TypeDesc ((TypeDesc::BASETYPE)peltype);
    if (! m_localpixels && ! force &&
        (convert == m_cachedpixeltype || convert == TypeDesc::UNKNOWN)) {
        m_spec.format = m_cachedpixeltype;
        m_pixel_bytes = m_spec.pixel_bytes();
        m_scanline_bytes = m_spec.scanline_bytes();
        m_plane_bytes = clamped_mult64 (m_scanline_bytes, (imagesize_t)m_spec.height);
        m_blackpixel.resize (m_pixel_bytes, 0);
        m_pixels_valid = true;
        m_storage = ImageBuf::IMAGECACHE;
#ifndef NDEBUG
        std::cerr << "read was not necessary -- using cache\n";
#endif
        return true;
    } else {
#ifndef NDEBUG
        std::cerr << "going to have to read " << m_name << ": "
                  << m_spec.format.c_str() << " vs " << convert.c_str() << "\n";
#endif
        // FIXME/N.B. - is it really best to go through the ImageCache
        // for forced IB reads?  Are there circumstances in which we
        // should just to a straight read_image() to avoid the extra
        // copies or the memory use of having bytes both in the cache
        // and in the IB?
    }

    if (convert != TypeDesc::UNKNOWN)
        m_spec.format = convert;
    else
        m_spec.format = m_nativespec.format;
    m_orientation = m_spec.get_int_attribute ("orientation", 1);
    m_pixelaspect = m_spec.get_float_attribute ("pixelaspectratio", 1.0f);
    realloc ();
    if (m_imagecache->get_pixels (m_name, subimage, miplevel,
                                  m_spec.x, m_spec.x+m_spec.width,
                                  m_spec.y, m_spec.y+m_spec.height,
                                  m_spec.z, m_spec.z+m_spec.depth,
                                  m_spec.format, m_localpixels)) {
        m_pixels_valid = true;
        // If forcing a full read, make sure the spec reflects the
        // nativespec's tile sizes, rather than that imposed by the
        // ImageCache.
        m_spec.tile_width = m_nativespec.tile_width;
        m_spec.tile_height = m_nativespec.tile_height;
        m_spec.tile_depth = m_nativespec.tile_depth;
    } else {
        m_pixels_valid = false;
        error ("%s", m_imagecache->geterror ());
    }

    return m_pixels_valid;
}



bool
ImageBuf::read (int subimage, int miplevel, bool force, TypeDesc convert,
                ProgressCallback progress_callback,
                void *progress_callback_data)
{
    return impl()->read (subimage, miplevel, force, convert,
                         progress_callback, progress_callback_data);
}



void
ImageBuf::set_write_format (TypeDesc format)
{
    impl()->m_write_format = format;
}


void
ImageBuf::set_write_tiles (int width, int height, int depth)
{
    impl()->m_write_tile_width = width;
    impl()->m_write_tile_height = height;
    impl()->m_write_tile_depth = std::max (1, depth);
}



bool
ImageBuf::write (ImageOutput *out,
                 ProgressCallback progress_callback,
                 void *progress_callback_data) const
{
    stride_t as = AutoStride;
    bool ok = true;
    const ImageBufImpl *impl = this->impl();
    impl->validate_pixels ();
    const ImageSpec &m_spec (impl->m_spec);
    if (impl->m_localpixels) {
        // In-core pixel buffer for the whole image
        ok = out->write_image (m_spec.format, impl->m_localpixels, as, as, as,
                               progress_callback, progress_callback_data);
    } else if (deep()) {
        // Deep image record
        ok = out->write_deep_image (impl->m_deepdata);
    } else {
        // Backed by ImageCache
        boost::scoped_array<char> tmp (new char [m_spec.image_bytes()]);
        get_pixels (xbegin(), xend(), ybegin(), yend(), zbegin(), zend(),
                    m_spec.format, &tmp[0]);
        ok = out->write_image (m_spec.format, &tmp[0], as, as, as,
                               progress_callback, progress_callback_data);
        // FIXME -- not good for huge images.  Instead, we should read
        // little bits at a time (scanline or tile blocks).
    }
    if (! ok)
        error ("%s", out->geterror ());
    return ok;
}



bool
ImageBuf::save (const std::string &filename, const std::string &fileformat,
                ProgressCallback progress_callback,
                void *progress_callback_data) const
{
    return write (filename, fileformat,
                  progress_callback, progress_callback_data);
}



bool
ImageBuf::write (const std::string &_filename, const std::string &_fileformat,
                 ProgressCallback progress_callback,
                 void *progress_callback_data) const
{
    std::string filename = _filename.size() ? _filename : name();
    std::string fileformat = _fileformat.size() ? _fileformat : filename;
    if (filename.size() == 0) {
        error ("ImageBuf::write() called with no filename");
        return false;
    }
    impl()->validate_pixels ();
    boost::scoped_ptr<ImageOutput> out (ImageOutput::create (fileformat.c_str(), "" /* searchpath */));
    if (! out) {
        error ("%s", geterror());
        return false;
    }

    // Write scanline files by default, but if the file type allows tiles,
    // user can override via ImageBuf::set_write_tiles(), or by using the
    // variety of IB::write() that takes the open ImageOutput* directly.
    ImageSpec newspec = spec();
    if (out->supports("tiles") && impl()->m_write_tile_width > 0) {
        newspec.tile_width  = impl()->m_write_tile_width;
        newspec.tile_height = impl()->m_write_tile_height;
        newspec.tile_depth  = std::max (1, impl()->m_write_tile_depth);
    } else {
        newspec.tile_width  = 0;
        newspec.tile_height = 0;
        newspec.tile_depth  = 0;
    }
    // Allow for format override via ImageBuf::set_write_format()
    if (impl()->m_write_format != TypeDesc::UNKNOWN) {
        newspec.set_format (impl()->m_write_format);
        newspec.channelformats.clear();
    } else {
        newspec.set_format (nativespec().format);
        newspec.channelformats = nativespec().channelformats;
    }
    if (! out->open (filename.c_str(), newspec)) {
        error ("%s", out->geterror());
        return false;
    }
    if (! write (out.get(), progress_callback, progress_callback_data))
        return false;
    out->close ();
    if (progress_callback)
        progress_callback (progress_callback_data, 0);
    return true;
}



void
ImageBufImpl::copy_metadata (const ImageBufImpl &src)
{
    const ImageSpec &srcspec (src.spec());
    ImageSpec &m_spec (this->specmod());
    m_spec.full_x = srcspec.full_x;
    m_spec.full_y = srcspec.full_y;
    m_spec.full_z = srcspec.full_z;
    m_spec.full_width = srcspec.full_width;
    m_spec.full_height = srcspec.full_height;
    m_spec.full_depth = srcspec.full_depth;
    if (src.storage() == ImageBuf::IMAGECACHE) {
        // If we're copying metadata from a cached image, be sure to
        // get the file's tile size, not the cache's tile size.
        m_spec.tile_width = src.nativespec().tile_width;
        m_spec.tile_height = src.nativespec().tile_height;
        m_spec.tile_depth = src.nativespec().tile_depth;
    } else {
        m_spec.tile_width = srcspec.tile_width;
        m_spec.tile_height = srcspec.tile_height;
        m_spec.tile_depth = srcspec.tile_depth;
    }
    m_spec.extra_attribs = srcspec.extra_attribs;
}



void
ImageBuf::copy_metadata (const ImageBuf &src)
{
    impl()->copy_metadata (*src.impl());
}




const ImageSpec &
ImageBuf::spec () const
{
    return impl()->spec();
}



ImageSpec &
ImageBuf::specmod ()
{
    return impl()->specmod();
}



const ImageSpec &
ImageBuf::nativespec () const
{
    return impl()->nativespec();
}



const std::string &
ImageBuf::name (void) const
{
    return impl()->m_name.string();
}


const std::string &
ImageBuf::file_format_name (void) const
{
    impl()->validate_spec ();
    return impl()->m_fileformat.string();
}


int
ImageBuf::subimage () const
{
    return impl()->m_current_subimage;
}


int
ImageBuf::nsubimages () const
{
    impl()->validate_spec();
    return impl()->m_nsubimages;
}


int
ImageBuf::miplevel () const
{
    return impl()->m_current_miplevel;
}


int
ImageBuf::nmiplevels () const
{
    impl()->validate_spec();
    return impl()->m_nmiplevels;
}


int
ImageBuf::nchannels () const
{
    return impl()->spec().nchannels;
}



int
ImageBuf::orientation () const
{
    impl()->validate_spec();
    return impl()->m_orientation;
}



bool
ImageBuf::pixels_valid (void) const
{
    return impl()->m_pixels_valid;
}



TypeDesc
ImageBuf::pixeltype () const
{
    return impl()->pixeltype();
}



void *
ImageBuf::localpixels ()
{
    impl()->validate_pixels ();
    return impl()->m_localpixels;
}



const void *
ImageBuf::localpixels () const
{
    impl()->validate_pixels ();
    return impl()->m_localpixels;
}



bool
ImageBuf::cachedpixels () const
{
    return impl()->cachedpixels();
}



ImageCache *
ImageBuf::imagecache () const
{
    return impl()->m_imagecache;
}



bool
ImageBuf::deep () const
{
    return spec().deep;
}


DeepData *
ImageBuf::deepdata ()
{
    return impl()->deepdata ();
}


const DeepData *
ImageBuf::deepdata () const
{
    return impl()->deepdata ();
}


bool
ImageBuf::initialized () const
{
    return impl()->initialized ();
}




namespace {

// Pixel-by-pixel copy fully templated by both data types.
// The roi is guaranteed to exist in both images.
template<class D, class S>
bool copy_pixels_2 (ImageBuf &dst, const ImageBuf &src, const ROI &roi)
{
    int nchannels = roi.nchannels();
    if (is_same<D,S>::value) {
        // If both bufs are the same type, just directly copy the values
        ImageBuf::Iterator<D,D> d (dst, roi);
        ImageBuf::ConstIterator<D,D> s (src, roi);
        for ( ; ! d.done();  ++d, ++s) {
            for (int c = 0;  c < nchannels;  ++c)
                d[c] = s[c];
        }
    } else {
        // If the two bufs are different types, convert through float
        ImageBuf::Iterator<D,float> d (dst, roi);
        ImageBuf::ConstIterator<S,float> s (src, roi);
        for ( ; ! d.done();  ++d, ++s) {
            for (int c = 0;  c < nchannels;  ++c)
                d[c] = s[c];
        }
    }
    return true;
}

} // anon namespace



bool
ImageBuf::copy_pixels (const ImageBuf &src)
{
    // compute overlap
    ROI myroi = get_roi(spec());
    ROI roi = roi_intersection (myroi, get_roi(src.spec()));

    // If we aren't copying over all our pixels, zero out the pixels
    if (roi != myroi)
        ImageBufAlgo::zero (*this);

    OIIO_DISPATCH_TYPES2 ("copy_pixels", copy_pixels_2,
                          spec().format, src.spec().format, *this, src, roi);
    return true;
}



bool
ImageBuf::copy (const ImageBuf &src)
{
    src.impl()->validate_pixels ();
    if (this == &src)     // self-assignment
        return true;
    if (src.storage() == UNINITIALIZED) {    // buf = uninitialized
        clear();
        return true;
    }
    reset (src.name(), src.spec());
    return this->copy_pixels (src);
}



template<typename T>
static inline float getchannel_ (const ImageBuf &buf, int x, int y, int z,
                                 int c, ImageBuf::WrapMode wrap)
{
    ImageBuf::ConstIterator<T> pixel (buf, x, y, z);
    return pixel[c];
}



float
ImageBuf::getchannel (int x, int y, int z, int c, WrapMode wrap) const
{
    if (c < 0 || c >= spec().nchannels)
        return 0.0f;
    OIIO_DISPATCH_TYPES ("getchannel", getchannel_, spec().format,
                         *this, x, y, z, c, wrap);
}



template<typename T>
static bool
getpixel_ (const ImageBuf &buf, int x, int y, int z, float *result, int chans,
           ImageBuf::WrapMode wrap)
{
    ImageBuf::ConstIterator<T> pixel (buf, x, y, z, wrap);
    for (int i = 0;  i < chans;  ++i)
        result[i] = pixel[i];
    return true;
}



inline bool
getpixel_wrapper (int x, int y, int z, float *pixel, int nchans,
                  ImageBuf::WrapMode wrap, const ImageBuf &ib)
{
    OIIO_DISPATCH_TYPES ("getpixel", getpixel_, ib.spec().format,
                         ib, x, y, z, pixel, nchans, wrap);
}



void
ImageBuf::getpixel (int x, int y, int z, float *pixel, int maxchannels,
                    WrapMode wrap) const
{
    int nchans = std::min (spec().nchannels, maxchannels);
    getpixel_wrapper (x, y, z, pixel, nchans, wrap, *this);
}



template <class T>
static bool
interppixel_ (const ImageBuf &img, float x, float y, float *pixel,
              ImageBuf::WrapMode wrap)
{
    int n = img.spec().nchannels;
    float *localpixel = ALLOCA (float, n*4);
    float *p[4] = { localpixel, localpixel+n, localpixel+2*n, localpixel+3*n };
    x -= 0.5f;
    y -= 0.5f;
    int xtexel, ytexel;
    float xfrac, yfrac;
    xfrac = floorfrac (x, &xtexel);
    yfrac = floorfrac (y, &ytexel);
    ImageBuf::ConstIterator<T> it (img, xtexel, xtexel+2, ytexel, ytexel+2,
                                   0, 1, wrap);
    for (int i = 0;  i < 4;  ++i, ++it)
        for (int c = 0; c < n; ++c)
            p[i][c] = it[c];
    bilerp (p[0], p[1], p[2], p[3], xfrac, yfrac, n, pixel);
    return true;
}



inline bool
interppixel_wrapper (float x, float y, float *pixel,
                     ImageBuf::WrapMode wrap, const ImageBuf &img)
{
    OIIO_DISPATCH_TYPES ("interppixel", interppixel_, img.spec().format,
                         img, x, y, pixel, wrap);
}



void
ImageBuf::interppixel (float x, float y, float *pixel, WrapMode wrap) const
{
    interppixel_wrapper (x, y, pixel, wrap, *this);
}



void
ImageBuf::interppixel_NDC (float x, float y, float *pixel, WrapMode wrap) const
{
    const ImageSpec &spec (impl()->spec());
    interppixel (static_cast<float>(spec.x) + x * static_cast<float>(spec.width),
                 static_cast<float>(spec.y) + y * static_cast<float>(spec.height),
                 pixel, wrap);
}



void
ImageBuf::interppixel_NDC_full (float x, float y, float *pixel, WrapMode wrap) const
{
    const ImageSpec &spec (impl()->spec());
    interppixel (static_cast<float>(spec.full_x) + x * static_cast<float>(spec.full_width),
                 static_cast<float>(spec.full_y) + y * static_cast<float>(spec.full_height),
                 pixel, wrap);
}



template<typename T>
inline void
setpixel_ (ImageBuf &buf, int x, int y, int z, const float *data, int chans)
{
    ImageBuf::Iterator<T> pixel (buf, x, y, z);
    if (pixel.exists()) {
        for (int i = 0;  i < chans;  ++i)
            pixel[i] = data[i];
    }
}



void
ImageBuf::setpixel (int x, int y, int z, const float *pixel, int maxchannels)
{
    int n = std::min (spec().nchannels, maxchannels);
    switch (spec().format.basetype) {
    case TypeDesc::FLOAT : setpixel_<float> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT8 : setpixel_<unsigned char> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT8  : setpixel_<char> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT16: setpixel_<unsigned short> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT16 : setpixel_<short> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT  : setpixel_<unsigned int> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT   : setpixel_<int> (*this, x, y, z, pixel, n); break;
    case TypeDesc::HALF  : setpixel_<half> (*this, x, y, z, pixel, n); break;
    case TypeDesc::DOUBLE: setpixel_<double> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT64: setpixel_<unsigned long long> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT64 : setpixel_<long long> (*this, x, y, z, pixel, n); break;
    default:
        ASSERT (0);
    }
}



void
ImageBuf::setpixel (int i, const float *pixel, int maxchannels)
{
    setpixel (spec().x + (i % spec().width), spec().y + (i / spec().width),
              pixel, maxchannels);
}



template<typename D, typename S>
static inline bool 
get_pixel_channels_ (const ImageBuf &buf, int xbegin, int xend,
                     int ybegin, int yend, int zbegin, int zend, 
                     int chbegin, int chend, void *r_,
                     stride_t xstride, stride_t ystride, stride_t zstride)
{
    D *r = (D *)r_;
    int w = (xend-xbegin), h = (yend-ybegin);
    int nchans = chend - chbegin;
    ImageSpec::auto_stride (xstride, ystride, zstride, sizeof(D), nchans, w, h);
    for (ImageBuf::ConstIterator<S,D> p (buf, xbegin, xend, ybegin, yend, zbegin, zend);
         !p.done(); ++p) {
        imagesize_t offset = (p.z()-zbegin)*zstride + (p.y()-ybegin)*ystride
                           + (p.x()-xbegin)*xstride;
        D *rc = (D *)((char *)r + offset);
        for (int c = 0;  c < nchans;  ++c)
            rc[c] = p[c+chbegin];
    }
    return true;
}



template<typename D>
bool
ImageBuf::get_pixel_channels (int xbegin, int xend, int ybegin, int yend,
                              int zbegin, int zend,
                              int chbegin, int chend, D *r,
                              stride_t xstride, stride_t ystride,
                              stride_t zstride) const
{
    OIIO_DISPATCH_TYPES2_HELP ("get_pixel_channels", get_pixel_channels_,
                               D, spec().format, *this,
                               xbegin, xend, ybegin, yend, zbegin, zend,
                               chbegin, chend, r, xstride, ystride, zstride);
}



bool
ImageBuf::get_pixel_channels (int xbegin, int xend, int ybegin, int yend,
                              int zbegin, int zend, int chbegin, int chend,
                              TypeDesc format, void *result,
                              stride_t xstride, stride_t ystride,
                              stride_t zstride) const
{
    OIIO_DISPATCH_TYPES2 ("get_pixel_channels", get_pixel_channels_,
                          format, spec().format, *this,
                          xbegin, xend, ybegin, yend, zbegin, zend,
                          chbegin, chend, result, xstride, ystride, zstride);
}



bool
ImageBuf::get_pixels (int xbegin, int xend, int ybegin, int yend,
                      int zbegin, int zend, TypeDesc format, void *result,
                      stride_t xstride, stride_t ystride,
                      stride_t zstride) const
{
    return get_pixel_channels (xbegin, xend, ybegin, yend, zbegin, zend,
                               0, nchannels(), format, result,
                               xstride, ystride, zstride);
}



int
ImageBuf::deep_samples (int x, int y, int z) const
{
    impl()->validate_pixels();
    if (! deep())
        return 0;
    const ImageSpec &m_spec (spec());
    if (x < m_spec.x || y < m_spec.y || z < m_spec.z)
        return 0;
    x -= m_spec.x;  y -= m_spec.y;  z -= m_spec.z;
    if (x >= m_spec.width || y >= m_spec.height || z >= m_spec.depth)
        return 0;
    int p = (z * m_spec.height + y) * m_spec.width  + x;
    return deepdata()->nsamples[p];
}



const void *
ImageBuf::deep_pixel_ptr (int x, int y, int z, int c) const
{
    impl()->validate_pixels();
    if (! deep())
        return NULL;
    const ImageSpec &m_spec (spec());
    if (x < m_spec.x || y < m_spec.y || z < m_spec.z)
        return NULL;
    x -= m_spec.x;  y -= m_spec.y;  z -= m_spec.z;
    if (x >= m_spec.width || y >= m_spec.height || z >= m_spec.depth ||
        c < 0 || c >= m_spec.nchannels)
        return NULL;
    int p = (z * m_spec.height + y) * m_spec.width  + x;
    return deepdata()->nsamples[p] ? deepdata()->pointers[p*m_spec.nchannels] : NULL;
}



float
ImageBuf::deep_value (int x, int y, int z, int c, int s) const
{
    impl()->validate_pixels();
    if (! deep())
        return 0.0f;
    const ImageSpec &m_spec (spec());
    if (x < m_spec.x || y < m_spec.y || z < m_spec.z)
        return 0.0f;
    x -= m_spec.x;  y -= m_spec.y;  z -= m_spec.z;
    if (x >= m_spec.width || y >= m_spec.height || z >= m_spec.depth ||
        c < 0 || c >= m_spec.nchannels)
        return 0.0f;
    int p = (z * m_spec.height + y) * m_spec.width + x;
    int nsamps = impl()->m_deepdata.nsamples[p];
    if (s >= nsamps)
        return 0.0f;
    const void *ptr = impl()->m_deepdata.pointers[p*m_spec.nchannels+c];
    TypeDesc t = impl()->m_deepdata.channeltypes[c];
    switch (t.basetype) {
    case TypeDesc::FLOAT :
        return ((const float *)ptr)[s];
    case TypeDesc::HALF  :
        return ((const half *)ptr)[s];
    case TypeDesc::UINT8 :
        return ConstDataArrayProxy<unsigned char,float>((const unsigned char *)ptr)[s];
    case TypeDesc::INT8  :
        return ConstDataArrayProxy<char,float>((const char *)ptr)[s];
    case TypeDesc::UINT16:
        return ConstDataArrayProxy<unsigned short,float>((const unsigned short *)ptr)[s];
    case TypeDesc::INT16 :
        return ConstDataArrayProxy<short,float>((const short *)ptr)[s];
    case TypeDesc::UINT  :
        return ConstDataArrayProxy<unsigned int,float>((const unsigned int *)ptr)[s];
    case TypeDesc::INT   :
        return ConstDataArrayProxy<int,float>((const int *)ptr)[s];
    case TypeDesc::UINT64:
        return ConstDataArrayProxy<unsigned long long,float>((const unsigned long long *)ptr)[s];
    case TypeDesc::INT64 :
        return ConstDataArrayProxy<long long,float>((const long long *)ptr)[s];
    default:
        ASSERT (0);
        return 0.0f;
    }
}



int
ImageBuf::xbegin () const
{
    return spec().x;
}



int
ImageBuf::xend () const
{
    return spec().x + spec().width;
}



int
ImageBuf::ybegin () const
{
    return spec().y;
}



int
ImageBuf::yend () const
{
    return spec().y + spec().height;
}



int
ImageBuf::zbegin () const
{
    return spec().z;
}



int
ImageBuf::zend () const
{
    return spec().z + std::max(spec().depth,1);
}



int
ImageBuf::xmin () const
{
    return spec().x;
}



int
ImageBuf::xmax () const
{
    return spec().x + spec().width - 1;
}



int
ImageBuf::ymin () const
{
    return spec().y;
}



int
ImageBuf::ymax () const
{
    return spec().y + spec().height - 1;
}



int
ImageBuf::zmin () const
{
    return spec().z;
}



int
ImageBuf::zmax () const
{
    return spec().z + std::max(spec().depth,1) - 1;
}


int
ImageBuf::oriented_width () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return impl->m_orientation <= 4 ? spec.width : spec.height;
}



int
ImageBuf::oriented_height () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return impl->m_orientation <= 4 ? spec.height : spec.width;
}



int
ImageBuf::oriented_x () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return impl->m_orientation <= 4 ? spec.x : spec.y;
}



int
ImageBuf::oriented_y () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return impl->m_orientation <= 4 ? spec.y : spec.x;
}



int
ImageBuf::oriented_full_width () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return impl->m_orientation <= 4 ? spec.full_width : spec.full_height;
}



int
ImageBuf::oriented_full_height () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return impl->m_orientation <= 4 ? spec.full_height : spec.full_width;
}



int
ImageBuf::oriented_full_x () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return impl->m_orientation <= 4 ? spec.full_x : spec.full_y;
}



int
ImageBuf::oriented_full_y () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return impl->m_orientation <= 4 ? spec.full_y : spec.full_x;
}



void
ImageBuf::set_full (int xbegin, int xend, int ybegin, int yend,
                    int zbegin, int zend)
{
    ImageSpec &m_spec (impl()->specmod());
    m_spec.full_x = xbegin;
    m_spec.full_y = ybegin;
    m_spec.full_z = zbegin;
    m_spec.full_width  = xend - xbegin;
    m_spec.full_height = yend - ybegin;
    m_spec.full_depth  = zend - zbegin;
}



void
ImageBuf::set_full (int xbegin, int xend, int ybegin, int yend,
                    int zbegin, int zend, const float *bordercolor)
{
    ImageSpec &m_spec (impl()->specmod());
    m_spec.full_x = xbegin;
    m_spec.full_y = ybegin;
    m_spec.full_z = zbegin;
    m_spec.full_width  = xend - xbegin;
    m_spec.full_height = yend - ybegin;
    m_spec.full_depth  = zend - zbegin;
    if (bordercolor)
        m_spec.attribute ("oiio:bordercolor",
                          TypeDesc(TypeDesc::FLOAT,m_spec.nchannels),
                          bordercolor);
}



ROI
ImageBuf::roi () const
{
    return get_roi(spec());
}


ROI
ImageBuf::roi_full () const
{
    return get_roi_full(spec());
}


void
ImageBuf::set_roi_full (const ROI &newroi)
{
    OIIO::set_roi_full (specmod(), newroi);
}



const void *
ImageBufImpl::pixeladdr (int x, int y, int z) const
{
    if (cachedpixels())
        return NULL;
    validate_pixels ();
    x -= m_spec.x;
    y -= m_spec.y;
    z -= m_spec.z;
    size_t p = y * m_scanline_bytes + x * m_pixel_bytes
             + z * m_plane_bytes;
    return &(m_localpixels[p]);
}



void *
ImageBufImpl::pixeladdr (int x, int y, int z)
{
    validate_pixels ();
    if (cachedpixels())
        return NULL;
    x -= m_spec.x;
    y -= m_spec.y;
    z -= m_spec.z;
    size_t p = y * m_scanline_bytes + x * m_pixel_bytes
             + z * m_plane_bytes;
    return &(m_localpixels[p]);
}



const void *
ImageBuf::pixeladdr (int x, int y, int z) const
{
    return impl()->pixeladdr (x, y, z);
}



void *
ImageBuf::pixeladdr (int x, int y, int z)
{
    return impl()->pixeladdr (x, y, z);
}



const void *
ImageBuf::blackpixel () const
{
    return impl()->blackpixel();
}



bool
ImageBufImpl::do_wrap (int &x, int &y, int &z, ImageBuf::WrapMode wrap) const
{
    const ImageSpec &m_spec (this->spec());

    // Double check that we're outside the data window -- supposedly a
    // precondition of calling this method.
    DASSERT (! (x >= m_spec.x && x < m_spec.x+m_spec.width &&
                y >= m_spec.y && y < m_spec.y+m_spec.height &&
                z >= m_spec.z && z < m_spec.z+m_spec.depth));

    // Wrap based on the display window
    if (wrap == ImageBuf::WrapBlack) {
        // no remapping to do
        return false;  // still outside the data window
    }
    else if (wrap == ImageBuf::WrapClamp) {
        x = OIIO::clamp (x, m_spec.full_x, m_spec.full_x+m_spec.full_width-1);
        y = OIIO::clamp (y, m_spec.full_y, m_spec.full_y+m_spec.full_height-1);
        z = OIIO::clamp (z, m_spec.full_z, m_spec.full_z+m_spec.full_depth-1);
    }
    else if (wrap == ImageBuf::WrapPeriodic) {
        wrap_periodic (x, m_spec.full_x, m_spec.full_width);
        wrap_periodic (y, m_spec.full_y, m_spec.full_height);
        wrap_periodic (z, m_spec.full_z, m_spec.full_depth);
    }
    else if (wrap == ImageBuf::WrapMirror) {
        wrap_mirror (x, m_spec.full_x, m_spec.full_width);
        wrap_mirror (y, m_spec.full_y, m_spec.full_height);
        wrap_mirror (z, m_spec.full_z, m_spec.full_depth);
    }
    else {
        ASSERT_MSG (0, "unknown wrap mode %d", (int)wrap);
    }

    // Now determine if the new position is within the data window
    return (x >= m_spec.x && x < m_spec.x+m_spec.width &&
            y >= m_spec.y && y < m_spec.y+m_spec.height &&
            z >= m_spec.z && z < m_spec.z+m_spec.depth);
}



bool
ImageBuf::do_wrap (int &x, int &y, int &z, WrapMode wrap) const
{
    return m_impl->do_wrap (x, y, z, wrap);
}



const void *
ImageBufImpl::retile (int x, int y, int z, ImageCache::Tile* &tile,
                      int &tilexbegin, int &tileybegin, int &tilezbegin,
                      int &tilexend, bool exists,
                      ImageBuf::WrapMode wrap) const
{
    if (! exists) {
        // Special case -- (x,y,z) describes a location outside the data
        // window.  Use the wrap mode to possibly give a meaningful data
        // proxy to point to.  
        if (! do_wrap (x, y, z, wrap)) {
            // After wrapping, the new xyz point outside the data window.
            // So return the black pixel.
            return &m_blackpixel[0];
        }
        // We've adjusted x,y,z, and know the wrapped coordinates are in the
        // pixel data window, so now fall through below to get the right
        // tile.
    }

    DASSERT (x >= m_spec.x && x < m_spec.x+m_spec.width &&
             y >= m_spec.y && y < m_spec.y+m_spec.height &&
             z >= m_spec.z && z < m_spec.z+m_spec.depth);

    int tw = m_spec.tile_width, th = m_spec.tile_height;
    int td = m_spec.tile_depth;  DASSERT(m_spec.tile_depth >= 1);
    DASSERT (tile == NULL || tilexend == (tilexbegin+tw));
    if (tile == NULL || x < tilexbegin || x >= tilexend ||
                        y < tileybegin || y >= (tileybegin+th) ||
                        z < tilezbegin || z >= (tilezbegin+td)) {
        // not the same tile as before
        if (tile)
            m_imagecache->release_tile (tile);
        int xtile = (x-m_spec.x) / tw;
        int ytile = (y-m_spec.y) / th;
        int ztile = (z-m_spec.z) / td;
        tilexbegin = m_spec.x + xtile*tw;
        tileybegin = m_spec.y + ytile*th;
        tilezbegin = m_spec.z + ztile*td;
        tilexend = tilexbegin + tw;
        tile = m_imagecache->get_tile (m_name, m_current_subimage,
                                       m_current_miplevel, x, y, z);
        if (! tile)
            return NULL;
    }

    size_t offset = ((z - tilezbegin) * (size_t) th + (y - tileybegin)) * (size_t) tw
                    + (x - tilexbegin);
    offset *= m_spec.pixel_bytes();
    DASSERTMSG (m_spec.pixel_bytes() == m_pixel_bytes,
                "%d vs %d", (int)m_spec.pixel_bytes(), (int)m_pixel_bytes);

    TypeDesc format;
    return (const char *)m_imagecache->tile_pixels (tile, format) + offset;
}



const void *
ImageBuf::retile (int x, int y, int z, ImageCache::Tile* &tile,
                  int &tilexbegin, int &tileybegin, int &tilezbegin,
                  int &tilexend, bool exists,
                  WrapMode wrap) const
{
    return impl()->retile (x, y, z, tile, tilexbegin, tileybegin, tilezbegin,
                           tilexend, exists, wrap);
}



}
OIIO_NAMESPACE_EXIT

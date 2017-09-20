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
#include <memory>

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/simd.h>

OIIO_NAMESPACE_BEGIN


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
    ROI R (A.defined() ? A : B);
    if (A.defined() && B.defined())
        R = ROI (std::min (R.xbegin,  B.xbegin),  std::max (R.xend,  B.xend),
                 std::min (R.ybegin,  B.ybegin),  std::max (R.yend,  B.yend),
                 std::min (R.zbegin,  B.zbegin),  std::max (R.zend,  B.zend),
                 std::min (R.chbegin, B.chbegin), std::max (R.chend, B.chend));
    return R;
}



ROI
roi_intersection (const ROI &A, const ROI &B)
{
    ROI R (A.defined() ? A : B);
    if (A.defined() && B.defined())
        R = ROI (std::max (R.xbegin,  B.xbegin),  std::min (R.xend,  B.xend),
                 std::max (R.ybegin,  B.ybegin),  std::min (R.yend,  B.yend),
                 std::max (R.zbegin,  B.zbegin),  std::min (R.zend,  B.zend),
                 std::max (R.chbegin, B.chbegin), std::min (R.chend, B.chend));
    return R;
}




// Expansion of the opaque type that hides all the ImageBuf implementation
// detail.
class ImageBufImpl {
public:
    ImageBufImpl (string_view filename, int subimage, int miplevel,
                  ImageCache *imagecache=NULL, const ImageSpec *spec=NULL,
                  void *buffer=NULL, const ImageSpec *config = NULL);
    ImageBufImpl (const ImageBufImpl &src);
    ~ImageBufImpl ();

    void clear ();
    void reset (string_view name, int subimage, int miplevel,
                ImageCache *imagecache, const ImageSpec *config);
    // Reset the buf to blank, given the spec. If nativespec is also
    // supplied, use it for the "native" spec, otherwise make the nativespec
    // just copy the regular spec.
    void reset (string_view name, const ImageSpec &spec,
                const ImageSpec *nativespec=nullptr);
    void alloc (const ImageSpec &spec, const ImageSpec *nativespec=nullptr);
    void realloc ();
    bool init_spec (string_view filename, int subimage, int miplevel);
    bool read (int subimage, int miplevel, int chbegin=0, int chend=-1,
               bool force=false, TypeDesc convert=TypeDesc::UNKNOWN,
               ProgressCallback progress_callback=NULL,
               void *progress_callback_data=NULL);
    void copy_metadata (const ImageBufImpl &src);

    // Error reporting for ImageBuf: call this with printf-like
    // arguments.  Note however that this is fully typesafe!
    template<typename... Args>
    void error (string_view fmt, const Args&... args) const {
        append_error(Strutil::format (fmt, args...));
    }

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

    void threads (int n) const { m_threads = n; }
    int threads () const { return m_threads; }

    // Allocate m_configspec if not already done
    void add_configspec (const ImageSpec *config=NULL) {
        if (! m_configspec)
            m_configspec.reset (config ? new ImageSpec (*config) : new ImageSpec);
    }

    // Return the index of pixel (x,y,z). If check_range is true, return
    // -1 for an invalid coordinate that is not within the data window.
    int pixelindex (int x, int y, int z, bool check_range=false) const {
        x -= m_spec.x;  y -= m_spec.y;  z -= m_spec.z;
        if (check_range && (x < 0 || x >= m_spec.width ||
                            y < 0 || y >= m_spec.height ||
                            z < 0 || z >= m_spec.depth))
            return -1;
        return (z * m_spec.height + y) * m_spec.width + x;
    }

private:
    ImageBuf::IBStorage m_storage; ///< Pixel storage class
    ustring m_name;              ///< Filename of the image
    ustring m_fileformat;        ///< File format name
    int m_nsubimages;            ///< How many subimages are there?
    int m_current_subimage;      ///< Current subimage we're viewing
    int m_current_miplevel;      ///< Current miplevel we're viewing
    int m_nmiplevels;            ///< # of MIP levels in the current subimage
    mutable int m_threads;       ///< thread policy for this image
    ImageSpec m_spec;            ///< Describes the image (size, etc)
    ImageSpec m_nativespec;      ///< Describes the true native image
    std::unique_ptr<char[]> m_pixels; ///< Pixel data, if local and we own it
    char *m_localpixels;         ///< Pointer to local pixels
    mutable spin_mutex m_valid_mutex;
    mutable bool m_spec_valid;   ///< Is the spec valid
    mutable bool m_pixels_valid; ///< Image is valid
    bool m_badfile;              ///< File not found
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
    std::unique_ptr<ImageSpec> m_configspec; // Configuration spec
    mutable std::string m_err;   ///< Last error message

    const ImageBufImpl operator= (const ImageBufImpl &src); // unimplemented
    friend class ImageBuf;
};



ImageBufImpl::ImageBufImpl (string_view filename,
                            int subimage, int miplevel,
                            ImageCache *imagecache,
                            const ImageSpec *spec, void *buffer,
                            const ImageSpec *config)
    : m_storage(ImageBuf::UNINITIALIZED),
      m_name(filename), m_nsubimages(0),
      m_current_subimage(subimage), m_current_miplevel(miplevel),
      m_nmiplevels(0),
      m_threads(0),
      m_localpixels(NULL),
      m_spec_valid(false), m_pixels_valid(false),
      m_badfile(false), m_pixelaspect(1),
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
        m_blackpixel.resize (round_to_multiple (m_pixel_bytes, OIIO_SIMD_MAX_SIZE_BYTES), 0);
        // NB make it big enough for SSE
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
        m_configspec.reset (config ? new ImageSpec (*config) : NULL);
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
      m_threads(src.m_threads),
      m_spec(src.m_spec), m_nativespec(src.m_nativespec),
      m_badfile(src.m_badfile),
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
            // Source just wrapped the client app's pixels, we do the same
            m_localpixels = src.m_localpixels;
        } else {
            // We own our pixels -- copy from source
            m_pixels.reset (new char [src.m_spec.image_bytes()]);
            memcpy (m_pixels.get(), src.m_pixels.get(), m_spec.image_bytes());
            m_localpixels = m_pixels.get();
        }
    } else {
        // Source was cache-based or deep
        // nothing else to do
        m_localpixels = NULL;
    }
    if (src.m_configspec)
        m_configspec.reset (new ImageSpec(*src.m_configspec));
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



ImageBuf::ImageBuf (string_view filename, int subimage, int miplevel,
                    ImageCache *imagecache, const ImageSpec *config)
    : m_impl (new ImageBufImpl (filename, subimage, miplevel, imagecache,
              NULL /*spec*/, NULL /*buffer*/, config))
{
}



ImageBuf::ImageBuf (string_view filename, ImageCache *imagecache)
    : m_impl (new ImageBufImpl (filename, 0, 0, imagecache))
{
}



ImageBuf::ImageBuf (const ImageSpec &spec)
    : m_impl (new ImageBufImpl ("", 0, 0, NULL, &spec))
{
    m_impl->alloc (spec);
}



ImageBuf::ImageBuf (string_view filename, const ImageSpec &spec)
    : m_impl (new ImageBufImpl (filename, 0, 0, NULL, &spec))
{
    m_impl->alloc (spec);
}



ImageBuf::ImageBuf (string_view filename, const ImageSpec &spec,
                    void *buffer)
    : m_impl (new ImageBufImpl (filename, 0, 0, NULL, &spec, buffer))
{
}



ImageBuf::ImageBuf (const ImageSpec &spec, void *buffer)
    : m_impl (new ImageBufImpl ("", 0, 0, NULL, &spec, buffer))
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
    m_configspec.reset (NULL);
}



void
ImageBuf::clear ()
{
    impl()->clear ();
}



void
ImageBufImpl::reset (string_view filename, int subimage,
                     int miplevel, ImageCache *imagecache,
                     const ImageSpec *config)
{
    clear ();
    m_name = ustring (filename);
    m_current_subimage = subimage;
    m_current_miplevel = miplevel;
    if (imagecache)
        m_imagecache = imagecache;
    if (config)
        m_configspec.reset (new ImageSpec(*config));

    if (m_name.length() > 0) {
        // If a filename was given, read the spec and set it up as an
        // ImageCache-backed image.  Reallocate later if an explicit read()
        // is called to force read into a local buffer.
        read (subimage, miplevel);
    }
}



void
ImageBuf::reset (string_view filename, int subimage, int miplevel,
                 ImageCache *imagecache, const ImageSpec *config)
{
    impl()->reset (filename, subimage, miplevel, imagecache, config);
}



void
ImageBuf::reset (string_view filename, ImageCache *imagecache)
{
    impl()->reset (filename, 0, 0, imagecache, NULL);
}



void
ImageBufImpl::reset (string_view filename, const ImageSpec &spec,
                     const ImageSpec* nativespec)
{
    clear ();
    m_name = ustring (filename);
    m_current_subimage = 0;
    m_current_miplevel = 0;
    alloc (spec);
    if (nativespec)
        m_nativespec = *nativespec;
}



void
ImageBuf::reset (string_view filename, const ImageSpec &spec)
{
    impl()->reset (filename, spec);
}



void
ImageBuf::reset (const ImageSpec &spec)
{
    impl()->reset ("", spec);
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
    m_blackpixel.resize (round_to_multiple (m_pixel_bytes, OIIO_SIMD_MAX_SIZE_BYTES), 0);
    // NB make it big enough for SSE
    if (m_allocated_size)
        m_pixels_valid = true;
    if (m_spec.deep) {
        m_deepdata.init (m_spec);
        m_storage = ImageBuf::LOCALBUFFER;
    }
#if 0
    std::cerr << "ImageBuf " << m_name << " local allocation: " << m_allocated_size << "\n";
#endif
}



void
ImageBufImpl::alloc (const ImageSpec &spec, const ImageSpec *nativespec)
{
    m_spec = spec;

    // Preclude a nonsensical size
    m_spec.width = std::max (1, m_spec.width);
    m_spec.height = std::max (1, m_spec.height);
    m_spec.depth = std::max (1, m_spec.depth);
    m_spec.nchannels = std::max (1, m_spec.nchannels);

    m_nativespec = nativespec ? *nativespec : spec;
    realloc ();
    m_spec_valid = true;
}



bool
ImageBufImpl::init_spec (string_view filename, int subimage, int miplevel)
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
    if (m_configspec)  // Pass configuration options to cache
        m_imagecache->add_file (m_name, NULL, m_configspec.get());
    m_imagecache->get_image_info (m_name, subimage, miplevel, s_subimages,
                                  TypeInt, &m_nsubimages);
    m_imagecache->get_image_info (m_name, subimage, miplevel, s_miplevels,
                                  TypeInt, &m_nmiplevels);
    const char *fmt = NULL;
    m_imagecache->get_image_info (m_name, subimage, miplevel,
                                  s_fileformat, TypeString, &fmt);
    m_fileformat = ustring(fmt);
    m_imagecache->get_imagespec (m_name, m_spec, subimage, miplevel);
    m_imagecache->get_imagespec (m_name, m_nativespec, subimage, miplevel, true);
    m_pixel_bytes = m_spec.pixel_bytes();
    m_scanline_bytes = m_spec.scanline_bytes();
    m_plane_bytes = clamped_mult64 (m_scanline_bytes, (imagesize_t)m_spec.height);
    m_blackpixel.resize (round_to_multiple (m_pixel_bytes, OIIO_SIMD_MAX_SIZE_BYTES), 0);
    // NB make it big enough for SSE

    // Subtlety: m_nativespec will have the true formats of the file, but
    // we rig m_spec to reflect what it will look like in the cache.
    // This may make m_spec appear to change if there's a subsequent read()
    // that forces a full read into local memory, but what else can we do?
    // It causes havoc for it to suddenly change in the other direction
    // when the file is lazily read.
    int peltype = TypeDesc::UNKNOWN;
    m_imagecache->get_image_info (m_name, subimage, miplevel,
                                  ustring("cachedpixeltype"),
                                  TypeInt, &peltype);
    if (peltype != TypeDesc::UNKNOWN) {
        m_spec.format = (TypeDesc::BASETYPE)peltype;
        m_spec.channelformats.clear();
    }

    if (m_nsubimages) {
        m_badfile = false;
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
ImageBuf::init_spec (string_view filename, int subimage, int miplevel)
{
    return impl()->init_spec (filename, subimage, miplevel);
}



bool
ImageBufImpl::read (int subimage, int miplevel, int chbegin, int chend,
                    bool force, TypeDesc convert,
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
    if (chend < 0 || chend > nativespec().nchannels)
        chend = nativespec().nchannels;
    bool use_channel_subset = (chbegin != 0 || chend != nativespec().nchannels);

    if (m_spec.deep) {
        std::unique_ptr<ImageInput> input (
                    ImageInput::open (m_name.string(), m_configspec.get()));
        if (! input) {
            error ("%s", OIIO::geterror());
            return false;
        }
        input->threads (threads());  // Pass on our thread policy
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

    m_pixelaspect = m_spec.get_float_attribute ("pixelaspectratio", 1.0f);

    // If we don't already have "local" pixels, and we aren't asking to
    // convert the pixels to a specific (and different) type, then take an
    // early out by relying on the cache.
    int peltype = TypeDesc::UNKNOWN;
    m_imagecache->get_image_info (m_name, subimage, miplevel,
                                  ustring("cachedpixeltype"),
                                  TypeInt, &peltype);
    m_cachedpixeltype = TypeDesc ((TypeDesc::BASETYPE)peltype);
    if (! m_localpixels && ! force && ! use_channel_subset &&
        (convert == m_cachedpixeltype || convert == TypeDesc::UNKNOWN)) {
        m_spec.format = m_cachedpixeltype;
        m_pixel_bytes = m_spec.pixel_bytes();
        m_scanline_bytes = m_spec.scanline_bytes();
        m_plane_bytes = clamped_mult64 (m_scanline_bytes, (imagesize_t)m_spec.height);
        m_blackpixel.resize (round_to_multiple (m_pixel_bytes, OIIO_SIMD_MAX_SIZE_BYTES), 0);
        // NB make it big enough for SSE
        m_pixels_valid = true;
        m_storage = ImageBuf::IMAGECACHE;
#ifndef NDEBUG
        // std::cerr << "read was not necessary -- using cache\n";
#endif
        return true;
    }

    if (use_channel_subset) {
        // Some adjustments because we are reading a channel subset
        force = true;
        m_spec.nchannels = chend-chbegin;
        m_spec.channelnames.resize (m_spec.nchannels);
        for (int c = 0; c < m_spec.nchannels; ++c)
            m_spec.channelnames[c] = m_nativespec.channelnames[c+chbegin];
        if (m_nativespec.channelformats.size()) {
            m_spec.channelformats.resize (m_spec.nchannels);
            for (int c = 0; c < m_spec.nchannels; ++c)
                m_spec.channelformats[c] = m_nativespec.channelformats[c+chbegin];
        }
    }

    if (convert != TypeDesc::UNKNOWN)
        m_spec.format = convert;
    else
        m_spec.format = m_nativespec.format;
    realloc ();

    // If forcing a full read, make sure the spec reflects the nativespec's
    // tile sizes, rather than that imposed by the ImageCache.
    m_spec.tile_width = m_nativespec.tile_width;
    m_spec.tile_height = m_nativespec.tile_height;
    m_spec.tile_depth = m_nativespec.tile_depth;

    if (force || (convert != TypeDesc::UNKNOWN &&
                  convert != m_cachedpixeltype &&
                  convert.size() >= m_cachedpixeltype.size() &&
                  convert.size() >= m_nativespec.format.size())) {
        // A specific conversion type was requested which is not the cached
        // type and whose bit depth is as much or more than the cached type.
        // Bypass the cache and read directly so that there is no possible
        // loss of range or precision resulting from going through the
        // cache.
        int unassoc = 0;
        if (m_imagecache->getattribute ("unassociatedalpha", unassoc)) {
            // Since IB needs to act as if it's backed by an ImageCache,
            // even though in this case we're bypassing the IC, we need to
            // honor the IC's "unassociatedalpha" flag.
            add_configspec ();
            m_configspec->attribute ("oiio:UnassociatedAlpha", unassoc);
        }
        ImageInput *in = ImageInput::open (m_name.string(), m_configspec.get());
        bool ok = true;
        if (in) {
            in->threads (threads());  // Pass on our thread policy
            if (subimage || miplevel) {
                ImageSpec newspec;
                ok &= in->seek_subimage (subimage, miplevel, newspec);
            }
            if (ok)
                ok &= in->read_image (chbegin, chend, convert, m_localpixels);
            in->close ();
            if (ok) {
                m_pixels_valid = true;
            } else {
                m_pixels_valid = false;
                error ("%s", in->geterror());
            }
            delete in;
        } else {
            m_pixels_valid = false;
            error ("%s", OIIO::geterror());
        }
        return m_pixels_valid;
    }

    // All other cases, no loss of precision is expected, so even a forced
    // read should go through the image cache.
    if (m_imagecache->get_pixels (m_name, subimage, miplevel,
                                  m_spec.x, m_spec.x+m_spec.width,
                                  m_spec.y, m_spec.y+m_spec.height,
                                  m_spec.z, m_spec.z+m_spec.depth,
                                  chbegin, chend,
                                  m_spec.format, m_localpixels)) {
        m_pixels_valid = true;
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
    return impl()->read (subimage, miplevel, 0, -1, force, convert,
                         progress_callback, progress_callback_data);
}



bool
ImageBuf::read (int subimage, int miplevel, int chbegin, int chend,
                bool force, TypeDesc convert,
                ProgressCallback progress_callback,
                void *progress_callback_data)
{
    return impl()->read (subimage, miplevel, chbegin, chend, force, convert,
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
    ok &= impl->validate_pixels ();
    const ImageSpec &bufspec (impl->m_spec);
    const ImageSpec &outspec (out->spec());
    TypeDesc bufformat = spec().format;
    if (impl->m_localpixels) {
        // In-core pixel buffer for the whole image
        ok = out->write_image (bufformat, impl->m_localpixels, as, as, as,
                               progress_callback, progress_callback_data);
    } else if (deep()) {
        // Deep image record
        ok = out->write_deep_image (impl->m_deepdata);
    } else {
        // The image we want to write is backed by ImageCache -- we must be
        // immediately writing out a file from disk, possibly with file
        // format or data format conversion, but without any ImageBufAlgo
        // functions having been applied.
        const imagesize_t budget = 1024*1024*64; // 64 MB
        imagesize_t imagesize = bufspec.image_bytes();
        if (imagesize <= budget) {
            // whole image can fit within our budget
            std::unique_ptr<char[]> tmp (new char [imagesize]);
            ok &= get_pixels (roi(), bufformat, &tmp[0]);
            ok &= out->write_image (bufformat, &tmp[0], as, as, as,
                                    progress_callback, progress_callback_data);
        } else if (outspec.tile_width) {
            // Big tiled image: break up into tile strips
            size_t pixelsize = bufspec.pixel_bytes();
            size_t chunksize = pixelsize * outspec.width * outspec.tile_height * outspec.tile_depth;
            std::unique_ptr<char[]> tmp (new char [chunksize]);
            for (int z = 0;  z < outspec.depth;  z += outspec.tile_depth) {
                int zend = std::min (z+outspec.z+outspec.tile_depth,
                                     outspec.z+outspec.depth);
                for (int y = 0;  y < outspec.height && ok;  y += outspec.tile_height) {
                    int yend = std::min (y+outspec.y+outspec.tile_height,
                                         outspec.y+outspec.height);
                    ok &= get_pixels (ROI(outspec.x, outspec.x+outspec.width,
                                          outspec.y+y, yend,
                                          outspec.z+z, zend),
                                      bufformat, &tmp[0]);
                    ok &= out->write_tiles (outspec.x, outspec.x+outspec.width,
                                            y+outspec.y, yend, z+outspec.z, zend,
                                            bufformat, &tmp[0]);
                    if (progress_callback &&
                        progress_callback (progress_callback_data,
                                           (float)(z*outspec.height+y)/(outspec.height*outspec.depth)))
                        return ok;
                }
            }
        } else {
            // Big scanline image: break up into scanline strips
            imagesize_t slsize = bufspec.scanline_bytes();
            int chunk = clamp (round_to_multiple(int(budget/slsize), 64), 1, 1024);
            std::unique_ptr<char[]> tmp (new char [chunk*slsize]);
            for (int z = 0;  z < outspec.depth;  ++z) {
                for (int y = 0;  y < outspec.height && ok;  y += chunk) {
                    int yend = std::min (y+outspec.y+chunk, outspec.y+outspec.height);
                    ok &= get_pixels (ROI(outspec.x, outspec.x+outspec.width,
                                          outspec.y+y, yend,
                                          outspec.z, outspec.z+outspec.depth),
                                      bufformat, &tmp[0]);
                    ok &= out->write_scanlines (y+outspec.y, yend, z+outspec.z,
                                                bufformat, &tmp[0]);
                    if (progress_callback &&
                        progress_callback (progress_callback_data,
                                           (float)(z*outspec.height+y)/(outspec.height*outspec.depth)))
                        return ok;
                }
            }
        }
    }
    if (! ok)
        error ("%s", out->geterror ());
    return ok;
}



bool
ImageBuf::write (string_view _filename, string_view _fileformat,
                 ProgressCallback progress_callback,
                 void *progress_callback_data) const
{
    string_view filename = _filename.size() ? _filename : name();
    string_view fileformat = _fileformat.size() ? _fileformat : filename;
    if (filename.size() == 0) {
        error ("ImageBuf::write() called with no filename");
        return false;
    }
    impl()->validate_pixels ();
    std::unique_ptr<ImageOutput> out (ImageOutput::create (fileformat.c_str(), "" /* searchpath */));
    if (! out) {
        error ("%s", geterror());
        return false;
    }
    out->threads (threads());  // Pass on our thread policy

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



bool
ImageBuf::make_writeable (bool keep_cache_type)
{
    if (storage() == IMAGECACHE) {
        return read (subimage(), miplevel(), 0, -1, true /*force*/,
                     keep_cache_type ? impl()->m_cachedpixeltype : TypeDesc());
    }
    return true;
}



void
ImageBufImpl::copy_metadata (const ImageBufImpl &src)
{
    if (this == &src)
        return;
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



string_view
ImageBuf::name (void) const
{
    return impl()->m_name;
}


string_view
ImageBuf::file_format_name (void) const
{
    impl()->validate_spec ();
    return impl()->m_fileformat;
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
    return impl()->spec().get_int_attribute ("Orientation", 1);
}



void
ImageBuf::set_orientation (int orient)
{
    impl()->specmod().attribute ("Orientation", orient);
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



void
ImageBuf::threads (int n) const
{
    impl()->threads(n);
}



int
ImageBuf::threads () const
{
    return impl()->threads();
}




namespace {

// Pixel-by-pixel copy fully templated by both data types.
// The roi is guaranteed to exist in both images.
template<class D, class S>
static bool
copy_pixels_impl (ImageBuf &dst, const ImageBuf &src, ROI roi, int nthreads=0)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [&](ROI roi){
        int nchannels = roi.nchannels();
        if (is_same<D,S>::value) {
            // If both bufs are the same type, just directly copy the values
            if (src.localpixels() && roi.chbegin == 0 &&
                roi.chend == dst.nchannels() && roi.chend == src.nchannels()) {
                // Extra shortcut -- totally local pixels for src, copying all
                // channels, so we can copy memory around line by line, rather
                // than value by value.
                int nxvalues = roi.width() * dst.nchannels();
                for (int z = roi.zbegin; z < roi.zend; ++z)
                    for (int y = roi.ybegin; y < roi.yend; ++y) {
                        D *draw = (D *) dst.pixeladdr (roi.xbegin, y, z);
                        const S *sraw = (const S *) src.pixeladdr (roi.xbegin, y, z);
                        DASSERT (draw && sraw);
                        for (int x = 0; x < nxvalues; ++x)
                            draw[x] = sraw[x];
                    }
            } else {
                ImageBuf::Iterator<D,D> d (dst, roi);
                ImageBuf::ConstIterator<D,D> s (src, roi);
                for ( ; ! d.done();  ++d, ++s) {
                    for (int c = 0;  c < nchannels;  ++c)
                        d[c] = s[c];
                }
            }
        } else {
            // If the two bufs are different types, convert through float
            ImageBuf::Iterator<D> d (dst, roi);
            ImageBuf::ConstIterator<S> s (src, roi);
            for ( ; ! d.done();  ++d, ++s) {
                for (int c = 0;  c < nchannels;  ++c)
                    d[c] = s[c];
            }
        }
    });
    return true;
}

} // anon namespace



bool
ImageBuf::copy_pixels (const ImageBuf &src)
{
    if (this == &src)
        return true;

    if (deep() || src.deep())
        return false;   // This operation is not supported for deep images

    // compute overlap
    ROI myroi = get_roi(spec());
    ROI roi = roi_intersection (myroi, get_roi(src.spec()));

    // If we aren't copying over all our pixels, zero out the pixels
    if (roi != myroi)
        ImageBufAlgo::zero (*this);

    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "copy_pixels", copy_pixels_impl,
                          spec().format, src.spec().format, *this, src, roi);
    return ok;
}



bool
ImageBuf::copy (const ImageBuf &src, TypeDesc format)
{
    src.impl()->validate_pixels ();
    if (this == &src)     // self-assignment
        return true;
    if (src.storage() == UNINITIALIZED) {    // buf = uninitialized
        clear();
        return true;
    }
    if (src.deep()) {
        impl()->reset (src.name(), src.spec(), &src.nativespec());
        impl()->m_deepdata = src.impl()->m_deepdata;
        return true;
    }
    if (format.basetype == TypeDesc::UNKNOWN || src.deep())
        impl()->reset (src.name(), src.spec(), &src.nativespec());
    else {
        ImageSpec newspec (src.spec());
        newspec.set_format (format);
        newspec.channelformats.clear ();
        reset (src.name(), newspec);
    }
    return this->copy_pixels (src);
}



bool
ImageBuf::copy (const ImageBuf &src)
{
    return copy (src, TypeDesc::UNKNOWN);
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
    float ret;
    OIIO_DISPATCH_TYPES (ret, "getchannel", getchannel_, spec().format,
                         *this, x, y, z, c, wrap);
    return ret;
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
    bool ok;
    OIIO_DISPATCH_TYPES (ok, "getpixel", getpixel_, ib.spec().format,
                         ib, x, y, z, pixel, nchans, wrap);
    return ok;
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
    bool ok;
    OIIO_DISPATCH_TYPES (ok, "interppixel", interppixel_, img.spec().format,
                         img, x, y, pixel, wrap);
    return ok;
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
    interppixel (static_cast<float>(spec.full_x) + x * static_cast<float>(spec.full_width),
                 static_cast<float>(spec.full_y) + y * static_cast<float>(spec.full_height),
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



template <class T>
static bool
interppixel_bicubic_ (const ImageBuf &img, float x, float y, float *pixel,
                      ImageBuf::WrapMode wrap)
{
    int n = img.spec().nchannels;
    x -= 0.5f;
    y -= 0.5f;
    int xtexel, ytexel;
    float xfrac, yfrac;
    xfrac = floorfrac (x, &xtexel);
    yfrac = floorfrac (y, &ytexel);

    float wx[4]; evalBSplineWeights (wx, xfrac);
    float wy[4]; evalBSplineWeights (wy, yfrac);
    for (int c = 0;  c < n;  ++c)
        pixel[c] = 0.0f;
    ImageBuf::ConstIterator<T> it (img, xtexel-1, xtexel+3, ytexel-1, ytexel+3,
                                   0, 1, wrap);
    for (int j = 0;  j < 4;  ++j) {
        for (int i = 0;  i < 4;  ++i, ++it) {
            float w = wx[i] * wy[j];
            for (int c = 0;  c < n;  ++c)
                pixel[c] += w * it[c];
        }
    }
    return true;
}



inline bool
interppixel_bicubic_wrapper (float x, float y, float *pixel,
                     ImageBuf::WrapMode wrap, const ImageBuf &img)
{
    bool ok;
    OIIO_DISPATCH_TYPES (ok, "interppixel_bicubic", interppixel_bicubic_,
                         img.spec().format, img, x, y, pixel, wrap);
    return ok;
}



void
ImageBuf::interppixel_bicubic (float x, float y, float *pixel, WrapMode wrap) const
{
    interppixel_bicubic_wrapper (x, y, pixel, wrap, *this);
}



void
ImageBuf::interppixel_bicubic_NDC (float x, float y, float *pixel, WrapMode wrap) const
{
    const ImageSpec &spec (impl()->spec());
    interppixel_bicubic (static_cast<float>(spec.full_x) + x * static_cast<float>(spec.full_width),
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
        ASSERTMSG (0, "Unknown/unsupported data type %d", spec().format.basetype);
    }
}



void
ImageBuf::setpixel (int i, const float *pixel, int maxchannels)
{
    setpixel (spec().x + (i % spec().width), spec().y + (i / spec().width),
              pixel, maxchannels);
}



template<typename D, typename S>
static bool
get_pixels_ (const ImageBuf &buf, ROI whole_roi, ROI roi, void *r_,
             stride_t xstride, stride_t ystride, stride_t zstride,
             int nthreads=0)
{
    ImageBufAlgo::parallel_image (roi, nthreads, [=,&buf](ROI roi){
        D *r = (D *)r_;
        int nchans = roi.nchannels();
        for (ImageBuf::ConstIterator<S,D> p (buf, roi); !p.done(); ++p) {
            imagesize_t offset = (p.z()-whole_roi.zbegin)*zstride
                               + (p.y()-whole_roi.ybegin)*ystride
                               + (p.x()-whole_roi.xbegin)*xstride;
            D *rc = (D *)((char *)r + offset);
            for (int c = 0;  c < nchans;  ++c)
                rc[c] = p[c+roi.chbegin];
        }
    });
    return true;
}



bool
ImageBuf::get_pixels (ROI roi, TypeDesc format, void *result,
                      stride_t xstride, stride_t ystride,
                      stride_t zstride) const
{
    if (! roi.defined())
        roi = this->roi();
    roi.chend = std::min (roi.chend, nchannels());
    ImageSpec::auto_stride (xstride, ystride, zstride, format.size(),
                            roi.nchannels(), roi.width(), roi.height());
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "get_pixels", get_pixels_,
                          format, spec().format, *this, roi, roi,
                          result, xstride, ystride, zstride, threads());
    return ok;
}



// DEPRECATED
template<typename D>
bool
ImageBuf::get_pixel_channels (int xbegin, int xend, int ybegin, int yend,
                              int zbegin, int zend,
                              int chbegin, int chend, D *r,
                              stride_t xstride, stride_t ystride,
                              stride_t zstride) const
{
    ROI roi (xbegin, xend, ybegin, yend, zbegin, zend, chbegin, chend);
    ImageSpec::auto_stride (xstride, ystride, zstride, sizeof(D),
                            roi.nchannels(), roi.width(), roi.height());
    bool ok;
    OIIO_DISPATCH_TYPES2_HELP (ok, "get_pixel_channels", get_pixels_,
                               D, spec().format, *this, roi, roi,
                               r, xstride, ystride, zstride);
    return ok;
}



// DEPRECATED
bool
ImageBuf::get_pixel_channels (int xbegin, int xend, int ybegin, int yend,
                              int zbegin, int zend, int chbegin, int chend,
                              TypeDesc format, void *result,
                              stride_t xstride, stride_t ystride,
                              stride_t zstride) const
{
    ROI roi (xbegin, xend, ybegin, yend, zbegin, zend, chbegin, chend);
    return get_pixels (roi, format, result, xstride, ystride, zstride);
}



// DEPRECATED
bool
ImageBuf::get_pixels (int xbegin, int xend, int ybegin, int yend,
                      int zbegin, int zend, TypeDesc format, void *result,
                      stride_t xstride, stride_t ystride,
                      stride_t zstride) const
{
    ROI roi (xbegin, xend, ybegin, yend, zbegin, zend, 0, nchannels());
    return get_pixels (roi, format, result, xstride, ystride, zstride);
}



template<typename D, typename S>
static bool
set_pixels_ (ImageBuf &buf, ROI roi, const void *data_,
             stride_t xstride, stride_t ystride, stride_t zstride)
{
    const D *data = (const D *)data_;
    int w = roi.width(), h = roi.height(), nchans = roi.nchannels();
    ImageSpec::auto_stride (xstride, ystride, zstride, sizeof(S), nchans, w, h);
    for (ImageBuf::Iterator<D,S> p (buf, roi);  !p.done();  ++p) {
        if (! p.exists())
            continue;
        imagesize_t offset = (p.z()-roi.zbegin)*zstride
                           + (p.y()-roi.ybegin)*ystride
                           + (p.x()-roi.xbegin)*xstride;
        const S *src = (const S *)((const char *)data + offset);
        for (int c = 0;  c < nchans;  ++c)
            p[c+roi.chbegin] = src[c];
    }
    return true;
}



bool
ImageBuf::set_pixels (ROI roi, TypeDesc format, const void *data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    bool ok;
    if (! roi.defined())
        roi = this->roi();
    roi.chend = std::min (roi.chend, nchannels());
    OIIO_DISPATCH_TYPES2 (ok, "set_pixels", set_pixels_,
                          spec().format, format, *this, roi,
                          data, xstride, ystride, zstride);
    return ok;

}



int
ImageBuf::deep_samples (int x, int y, int z) const
{
    impl()->validate_pixels();
    if (! deep())
        return 0;
    int p = impl()->pixelindex (x, y, z, true);
    return p >= 0 ? deepdata()->samples(p) : 0;
}



const void *
ImageBuf::deep_pixel_ptr (int x, int y, int z, int c, int s) const
{
    impl()->validate_pixels();
    if (! deep())
        return NULL;
    const ImageSpec &m_spec (spec());
    int p = impl()->pixelindex (x, y, z, true);
    if (p < 0 || c < 0 || c >= m_spec.nchannels)
        return NULL;
    return (s < deepdata()->samples(p)) ? deepdata()->data_ptr (p, c, s) : NULL;
}



float
ImageBuf::deep_value (int x, int y, int z, int c, int s) const
{
    impl()->validate_pixels();
    if (! deep())
        return 0.0f;
    int p = impl()->pixelindex (x, y, z);
    return impl()->m_deepdata.deep_value (p, c, s);
}



uint32_t
ImageBuf::deep_value_uint (int x, int y, int z, int c, int s) const
{
    impl()->validate_pixels();
    if (! deep())
        return 0;
    int p = impl()->pixelindex (x, y, z);
    return impl()->m_deepdata.deep_value_uint (p, c, s);
}



void
ImageBuf::set_deep_samples (int x, int y, int z, int samps)
{
    if (! deep())
        return ;
    int p = impl()->pixelindex (x, y, z);
    impl()->m_deepdata.set_samples (p, samps);
}



void
ImageBuf::deep_insert_samples (int x, int y, int z,
                               int samplepos, int nsamples)
{
    if (! deep())
        return ;
    int p = impl()->pixelindex (x, y, z);
    impl()->m_deepdata.insert_samples (p, samplepos, nsamples);
}



void
ImageBuf::deep_erase_samples (int x, int y, int z,
                              int samplepos, int nsamples)
{
    if (! deep())
        return ;
    int p = impl()->pixelindex (x, y, z);
    impl()->m_deepdata.erase_samples (p, samplepos, nsamples);
}



void
ImageBuf::set_deep_value (int x, int y, int z, int c, int s, float value)
{
    impl()->validate_pixels();
    if (! deep())
        return ;
    int p = impl()->pixelindex (x, y, z);
    return impl()->m_deepdata.set_deep_value (p, c, s, value);
}



void
ImageBuf::set_deep_value (int x, int y, int z, int c, int s, uint32_t value)
{
    impl()->validate_pixels();
    if (! deep())
        return;
    int p = impl()->pixelindex (x, y, z);
    return impl()->m_deepdata.set_deep_value (p, c, s, value);
}



// DEPRECATED (1.7): old name
void
ImageBuf::set_deep_value_uint (int x, int y, int z, int c, int s, uint32_t value)
{
    return set_deep_value (x, y, z, c, s, value);
}



// DEPRECATED (1.7)
void
ImageBuf::deep_alloc ()
{
    ASSERT (m_impl->m_storage == ImageBuf::LOCALBUFFER);
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
    return orientation() <= 4 ? spec.width : spec.height;
}



int
ImageBuf::oriented_height () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return orientation() <= 4 ? spec.height : spec.width;
}



int
ImageBuf::oriented_x () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return orientation() <= 4 ? spec.x : spec.y;
}



int
ImageBuf::oriented_y () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return orientation() <= 4 ? spec.y : spec.x;
}



int
ImageBuf::oriented_full_width () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return orientation() <= 4 ? spec.full_width : spec.full_height;
}



int
ImageBuf::oriented_full_height () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return orientation() <= 4 ? spec.full_height : spec.full_width;
}



int
ImageBuf::oriented_full_x () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return orientation() <= 4 ? spec.full_x : spec.full_y;
}



int
ImageBuf::oriented_full_y () const
{
    const ImageBufImpl *impl (this->impl());
    const ImageSpec &spec (impl->spec());
    return orientation() <= 4 ? spec.full_y : spec.full_x;
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



bool
ImageBuf::contains_roi (ROI roi) const
{
    ROI myroi = roi;
    return (roi.defined() && myroi.defined() &&
            roi.xbegin >= myroi.xbegin && roi.xend <= myroi.xend &&
            roi.ybegin >= myroi.ybegin && roi.yend <= myroi.yend &&
            roi.zbegin >= myroi.zbegin && roi.zend <= myroi.zend &&
            roi.chbegin >= myroi.chbegin && roi.chend <= myroi.chend);
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



int
ImageBuf::pixelindex (int x, int y, int z, bool check_range) const
{
    return impl()->pixelindex (x, y, z, check_range);
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



ImageBuf::WrapMode
ImageBuf::WrapMode_from_string (string_view name)
{
    static const char* names[] = {
        "default", "black", "clamp", "periodic", "mirror", NULL
    };
    for (int i = 0; names[i]; ++i)
        if (name == names[i])
            return WrapMode(i);
    return WrapDefault;  // name not found
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
        if (! tile) {
            // Even though tile is NULL, ensure valid black pixel data
            std::string e = m_imagecache->geterror();
            error ("%s", e.size() ? e : "unspecified ImageCache error");
            return &m_blackpixel[0];
        }
    }

    size_t offset = ((z - tilezbegin) * (size_t) th + (y - tileybegin)) * (size_t) tw
                    + (x - tilexbegin);
    offset *= m_spec.pixel_bytes();
    DASSERTMSG (m_spec.pixel_bytes() == m_pixel_bytes,
                "%d vs %d", (int)m_spec.pixel_bytes(), (int)m_pixel_bytes);

    TypeDesc format;
    const void* pixeldata = m_imagecache->tile_pixels (tile, format);
    return pixeldata ? (const char *)pixeldata + offset : NULL;
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



OIIO_NAMESPACE_END

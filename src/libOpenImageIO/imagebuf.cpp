// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <iostream>
#include <memory>

#include <OpenImageIO/half.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/strongparam.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/thread.h>

#include "imageio_pvt.h"

OIIO_NAMESPACE_BEGIN


OIIO_STRONG_PARAM_TYPE(DoLock, bool);

namespace pvt {
int imagebuf_print_uncaught_errors(1);
int imagebuf_use_imagecache(0);
atomic_ll IB_local_mem_current;
atomic_ll IB_local_mem_peak;
std::atomic<float> IB_total_open_time(0.0f);
std::atomic<float> IB_total_image_read_time(0.0f);
}  // namespace pvt



ROI
get_roi(const ImageSpec& spec)
{
    return ROI(spec.x, spec.x + spec.width, spec.y, spec.y + spec.height,
               spec.z, spec.z + spec.depth, 0, spec.nchannels);
}



ROI
get_roi_full(const ImageSpec& spec)
{
    return ROI(spec.full_x, spec.full_x + spec.full_width, spec.full_y,
               spec.full_y + spec.full_height, spec.full_z,
               spec.full_z + spec.full_depth, 0, spec.nchannels);
}



void
set_roi(ImageSpec& spec, const ROI& newroi)
{
    spec.x      = newroi.xbegin;
    spec.y      = newroi.ybegin;
    spec.z      = newroi.zbegin;
    spec.width  = newroi.width();
    spec.height = newroi.height();
    spec.depth  = newroi.depth();
}



void
set_roi_full(ImageSpec& spec, const ROI& newroi)
{
    spec.full_x      = newroi.xbegin;
    spec.full_y      = newroi.ybegin;
    spec.full_z      = newroi.zbegin;
    spec.full_width  = newroi.width();
    spec.full_height = newroi.height();
    spec.full_depth  = newroi.depth();
}



span<std::byte>
span_from_buffer(void* data, TypeDesc format, int nchannels, int width,
                 int height, int depth, stride_t xstride, stride_t ystride,
                 stride_t zstride)
{
    ImageSpec::auto_stride(xstride, ystride, zstride, format.size(), nchannels,
                           width, height);
    // Need to figure out the span based on the origin and strides.
    // Start with the span range of one pixel.
    std::byte* bufstart = (std::byte*)data;
    std::byte* bufend   = bufstart + format.size() * nchannels;
    // Expand to the span range for one row. Remember negative strides!
    if (xstride >= 0) {
        bufend += xstride * (width - 1);
    } else {
        bufstart += xstride * (width - 1);
    }
    // Expand to the span range for a whole image plane.
    if (ystride >= 0) {
        bufend += ystride * (height - 1);
    } else {
        bufstart += ystride * (height - 1);
    }
    // Expand to the span range for a whole volume.
    if (depth > 1 && zstride != 0) {
        if (zstride >= 0) {
            bufend += zstride * (depth - 1);
        } else {
            bufstart += zstride * (depth - 1);
        }
    }
    return { bufstart, size_t(bufend - bufstart) };
}



cspan<std::byte>
cspan_from_buffer(const void* data, TypeDesc format, int nchannels, int width,
                  int height, int depth, stride_t xstride, stride_t ystride,
                  stride_t zstride)
{
    auto s = span_from_buffer(const_cast<void*>(data), format, nchannels, width,
                              height, depth, xstride, ystride, zstride);
    return { s.data(), s.size() };
}



// Expansion of the opaque type that hides all the ImageBuf implementation
// detail.
class ImageBufImpl {
public:
    ImageBufImpl(string_view filename, int subimage, int miplevel,
                 std::shared_ptr<ImageCache> imagecache = {},
                 const ImageSpec* spec = nullptr, span<std::byte> bufspan = {},
                 const void* buforigin = nullptr, bool readonly = false,
                 const ImageSpec* config      = nullptr,
                 Filesystem::IOProxy* ioproxy = nullptr,
                 stride_t xstride = AutoStride, stride_t ystride = AutoStride,
                 stride_t zstride = AutoStride);
    ImageBufImpl(const ImageBufImpl& src);
    ~ImageBufImpl();

    void clear();
    void reset(string_view name, int subimage, int miplevel,
               std::shared_ptr<ImageCache> imagecache, const ImageSpec* config,
               Filesystem::IOProxy* ioproxy);
    // Reset the buf to blank, given the spec. If nativespec is also
    // supplied, use it for the "native" spec, otherwise make the nativespec
    // just copy the regular spec.
    void reset(string_view name, const ImageSpec& spec,
               const ImageSpec* nativespec = nullptr,
               span<std::byte> bufspan = {}, const void* buforigin = {},
               bool readonly = false, stride_t xstride = AutoStride,
               stride_t ystride = AutoStride, stride_t zstride = AutoStride);
    void alloc(const ImageSpec& spec, const ImageSpec* nativespec = nullptr);
    void realloc();
    bool init_spec(string_view filename, int subimage, int miplevel,
                   DoLock do_lock);
    bool read(int subimage, int miplevel, int chbegin = 0, int chend = -1,
              bool force = false, TypeDesc convert = TypeDesc::UNKNOWN,
              ProgressCallback progress_callback = nullptr,
              void* progress_callback_data       = nullptr,
              DoLock do_lock                     = DoLock(true));
    void copy_metadata(const ImageBufImpl& src);

    // At least one of bufspan or buforigin is supplied. Set this->m_bufspan
    // and this->m_localpixels appropriately.
    void set_bufspan_localpixels(span<std::byte> bufspan,
                                 const void* buforigin);

    // Note: Uses std::format syntax
    template<typename... Args>
    void error(const char* fmt, const Args&... args) const
    {
        error(Strutil::fmt::format(fmt, args...));
    }

    void error(string_view message) const;

    ImageBuf::IBStorage storage() const { return m_storage; }

    TypeDesc pixeltype() const
    {
        validate_spec();
        return cachedpixels() ? m_cachedpixeltype : m_spec.format;
    }

    DeepData* deepdata()
    {
        validate_pixels();
        return m_spec.deep ? &m_deepdata : NULL;
    }
    const DeepData* deepdata() const
    {
        validate_pixels();
        return m_spec.deep ? &m_deepdata : NULL;
    }
    bool initialized() const
    {
        return m_spec_valid && m_storage != ImageBuf::UNINITIALIZED;
    }
    bool cachedpixels() const { return m_storage == ImageBuf::IMAGECACHE; }

    const void* pixeladdr(int x, int y, int z, int ch) const;
    void* pixeladdr(int x, int y, int z, int ch);

    const void* retile(int x, int y, int z, ImageCache::Tile*& tile,
                       int& tilexbegin, int& tileybegin, int& tilezbegin,
                       int& tilexend, bool& haderror, bool exists,
                       ImageBuf::WrapMode wrap) const;

    bool do_wrap(int& x, int& y, int& z, ImageBuf::WrapMode wrap) const;

    const void* blackpixel() const
    {
        validate_spec();
        return &m_blackpixel[0];
    }

    bool validate_spec(DoLock do_lock = DoLock(true)) const
    {
        if (m_spec_valid)
            return true;
        if (!m_name.size())
            return false;
        lock_t lock(m_mutex, std::defer_lock_t());
        if (do_lock)
            lock.lock();
        if (m_spec_valid)
            return true;
        ImageBufImpl* imp = const_cast<ImageBufImpl*>(this);
        if (imp->m_current_subimage < 0)
            imp->m_current_subimage = 0;
        if (imp->m_current_miplevel < 0)
            imp->m_current_miplevel = 0;
        return imp->init_spec(m_name, m_current_subimage, m_current_miplevel,
                              DoLock(false) /* we already hold the lock */);
    }

    bool validate_pixels(DoLock do_lock = DoLock(true)) const
    {
        if (m_pixels_valid)
            return true;
        if (!m_name.size())
            return true;
        lock_t lock(m_mutex, std::defer_lock_t());
        if (do_lock)
            lock.lock();
        if (m_pixels_valid)
            return true;
        ImageBufImpl* imp = const_cast<ImageBufImpl*>(this);
        if (imp->m_current_subimage < 0)
            imp->m_current_subimage = 0;
        if (imp->m_current_miplevel < 0)
            imp->m_current_miplevel = 0;
        return imp->read(m_current_subimage, m_current_miplevel,
                         DoLock(false) /* we already hold the lock */);
    }

    const ImageSpec& spec() const
    {
        validate_spec();
        return m_spec;
    }
    const ImageSpec& nativespec() const
    {
        validate_spec();
        return m_nativespec;
    }
    ImageSpec& specmod()
    {
        validate_spec();
        return m_spec;
    }

    void threads(int n) const { m_threads = n; }
    int threads() const { return m_threads; }

    // Allocate m_configspec if not already done
    void add_configspec(const ImageSpec* config = NULL)
    {
        if (!m_configspec)
            m_configspec.reset(config ? new ImageSpec(*config) : new ImageSpec);
    }

    // Return the index of pixel (x,y,z). If check_range is true, return
    // -1 for an invalid coordinate that is not within the data window.
    int pixelindex(int x, int y, int z, bool check_range = false) const
    {
        x -= m_spec.x;
        y -= m_spec.y;
        z -= m_spec.z;
        if (check_range
            && (x < 0 || x >= m_spec.width || y < 0 || y >= m_spec.height
                || z < 0 || z >= m_spec.depth))
            return -1;
        return (z * m_spec.height + y) * m_spec.width + x;
    }

    // Invalidate the file in our imagecache and the shared one
    void invalidate(ustring filename, bool force)
    {
        auto shared_imagecache = ImageCache::create(true);
        OIIO_DASSERT(shared_imagecache);
        if (m_imagecache)
            m_imagecache->invalidate(filename, force);  // *our* IC
        if (m_imagecache != shared_imagecache)
            shared_imagecache->invalidate(filename, force);  // the shared IC
    }

    void eval_contiguous()
    {
        m_contiguous = m_localpixels
                       && (m_storage == ImageBuf::LOCALBUFFER
                           || m_storage == ImageBuf::APPBUFFER)
                       && m_xstride == m_spec.nchannels * m_channel_stride
                       && m_ystride == m_xstride * m_spec.width
                       && m_zstride == m_ystride * m_spec.height;
    }

    bool has_thumbnail(DoLock do_lock = DoLock(true)) const;
    void clear_thumbnail(DoLock do_lock = DoLock(true));
    void set_thumbnail(const ImageBuf& thumb, DoLock do_lock = DoLock(true));
    std::shared_ptr<ImageBuf> get_thumbnail(DoLock do_lock = DoLock(true)) const;

private:
    ImageBuf::IBStorage m_storage;  ///< Pixel storage class
    ustring m_name;                 ///< Filename of the image
    ustring m_fileformat;           ///< File format name
    int m_nsubimages;               ///< How many subimages are there?
    int m_current_subimage;         ///< Current subimage we're viewing
    int m_current_miplevel;         ///< Current miplevel we're viewing
    int m_nmiplevels;               ///< # of MIP levels in the current subimage
    mutable int m_threads;          ///< thread policy for this image
    ImageSpec m_spec;               ///< Describes the image (size, etc)
    ImageSpec m_nativespec;         ///< Describes the true native image
    std::unique_ptr<char[]> m_pixels;  ///< Pixel data, if local and we own it
    char* m_localpixels;               ///< Pointer to local pixels
    span<std::byte> m_bufspan;         ///< Bounded buffer for local pixels
    typedef std::recursive_mutex mutex_t;
    typedef std::unique_lock<mutex_t> lock_t;
    mutable mutex_t m_mutex;      ///< Thread safety for this ImageBuf
    mutable bool m_spec_valid;    ///< Is the spec valid
    mutable bool m_pixels_valid;  ///< Image is valid
    mutable bool m_pixels_read;   ///< Is file already in the local pixels?
    bool m_readonly;              ///< The bufspan is read-only
    bool m_badfile;               ///< File not found
    float m_pixelaspect;          ///< Pixel aspect ratio of the image
    stride_t m_xstride;
    stride_t m_ystride;
    stride_t m_zstride;
    stride_t m_channel_stride;
    bool m_contiguous;
    std::shared_ptr<ImageCache> m_imagecache;  ///< ImageCache to use
    TypeDesc m_cachedpixeltype;            ///< Data type stored in the cache
    DeepData m_deepdata;                   ///< Deep data
    size_t m_allocated_size;               ///< How much memory we've allocated
    std::vector<char> m_blackpixel;        ///< Pixel-sized zero bytes
    std::vector<TypeDesc> m_write_format;  /// Pixel data format to use for write()
    int m_write_tile_width;
    int m_write_tile_height;
    int m_write_tile_depth;
    std::unique_ptr<ImageSpec> m_configspec;  // Configuration spec
    Filesystem::IOProxy* m_rioproxy = nullptr;
    Filesystem::IOProxy* m_wioproxy = nullptr;
    mutable std::string m_err;  ///< Last error message
    bool m_has_thumbnail = false;
    std::shared_ptr<ImageBuf> m_thumbnail;

    // Private reset m_pixels to new allocation of new size, copy if
    // data is not nullptr. Return nullptr if an allocation of that size
    // was not possible.
    char* new_pixels(size_t size, const void* data = nullptr);
    // Private release of m_pixels.
    void free_pixels();

    TypeDesc write_format(int channel = 0) const
    {
        if (size_t(channel) < m_write_format.size())
            return m_write_format[channel];
        if (m_write_format.size() == 1)
            return m_write_format[0];
        return m_nativespec.format;
    }

    void lock() const { m_mutex.lock(); }
    void unlock() const { m_mutex.unlock(); }

    const ImageBufImpl operator=(const ImageBufImpl& src);  // unimplemented
    friend class ImageBuf;
};



void
ImageBuf::impl_deleter(ImageBufImpl* todel)
{
    delete todel;
}



ImageBufImpl::ImageBufImpl(string_view filename, int subimage, int miplevel,
                           std::shared_ptr<ImageCache> imagecache,
                           const ImageSpec* spec, span<std::byte> bufspan,
                           const void* buforigin, bool readonly,
                           const ImageSpec* config,
                           Filesystem::IOProxy* ioproxy, stride_t xstride,
                           stride_t ystride, stride_t zstride)
    : m_storage(ImageBuf::UNINITIALIZED)
    , m_name(filename)
    , m_nsubimages(0)
    , m_current_subimage(subimage)
    , m_current_miplevel(miplevel)
    , m_nmiplevels(0)
    , m_threads(0)
    , m_localpixels(NULL)
    , m_spec_valid(false)
    , m_pixels_valid(false)
    , m_pixels_read(false)
    , m_readonly(readonly)
    , m_badfile(false)
    , m_pixelaspect(1)
    , m_xstride(0)
    , m_ystride(0)
    , m_zstride(0)
    , m_channel_stride(0)
    , m_contiguous(false)
    , m_imagecache(imagecache)
    , m_allocated_size(0)
    , m_write_tile_width(0)
    , m_write_tile_height(0)
    , m_write_tile_depth(1)
{
    if (spec) {
        // spec != nullptr means we're constructing an ImageBuf that either
        // wraps a buffer or owns its own memory.
        m_spec           = *spec;
        m_nativespec     = *spec;
        m_channel_stride = stride_t(spec->format.size());
        m_xstride        = xstride;
        m_ystride        = ystride;
        m_zstride        = zstride;
        ImageSpec::auto_stride(m_xstride, m_ystride, m_zstride, m_spec.format,
                               m_spec.nchannels, m_spec.width, m_spec.height);
        m_blackpixel.resize(round_to_multiple(spec->pixel_bytes(),
                                              OIIO_SIMD_MAX_SIZE_BYTES),
                            0);
        // NB make it big enough for SSE
        if (buforigin || bufspan.size()) {
            set_bufspan_localpixels(bufspan, buforigin);
            m_storage      = ImageBuf::APPBUFFER;
            m_pixels_valid = true;
        } else {
            m_storage = ImageBuf::LOCALBUFFER;
        }
        m_spec_valid = true;
    } else if (filename.length() > 0) {
        // filename being nonempty means this ImageBuf refers to a file.
        OIIO_DASSERT(buforigin == nullptr);
        OIIO_DASSERT(bufspan.empty());
        reset(filename, subimage, miplevel, std::move(imagecache), config,
              ioproxy);
    } else {
        OIIO_DASSERT(buforigin == nullptr);
        OIIO_DASSERT(bufspan.empty());
    }
    eval_contiguous();
}



ImageBufImpl::ImageBufImpl(const ImageBufImpl& src)
    : m_storage(src.m_storage)
    , m_name(src.m_name)
    , m_fileformat(src.m_fileformat)
    , m_nsubimages(src.m_nsubimages)
    , m_current_subimage(src.m_current_subimage)
    , m_current_miplevel(src.m_current_miplevel)
    , m_nmiplevels(src.m_nmiplevels)
    , m_threads(src.m_threads)
    , m_spec(src.m_spec)
    , m_nativespec(src.m_nativespec)
    , m_readonly(src.m_readonly)
    , m_badfile(src.m_badfile)
    , m_pixelaspect(src.m_pixelaspect)
    , m_xstride(src.m_xstride)
    , m_ystride(src.m_ystride)
    , m_zstride(src.m_zstride)
    , m_channel_stride(src.m_channel_stride)
    , m_contiguous(src.m_contiguous)
    , m_imagecache(src.m_imagecache)
    , m_cachedpixeltype(src.m_cachedpixeltype)
    , m_deepdata(src.m_deepdata)
    , m_allocated_size(0)
    , m_blackpixel(src.m_blackpixel)  // gets fixed up in the body vvv
    , m_write_format(src.m_write_format)
    , m_write_tile_width(src.m_write_tile_width)
    , m_write_tile_height(src.m_write_tile_height)
    , m_write_tile_depth(src.m_write_tile_depth)
// NO -- copy ctr does not transfer proxy   , m_rioproxy(src.m_rioproxy)
// NO -- copy ctr does not transfer proxy   , m_wioproxy(src.m_wioproxy)
{
    m_spec_valid   = src.m_spec_valid;
    m_pixels_valid = src.m_pixels_valid;
    m_pixels_read  = src.m_pixels_read;
    if (src.m_localpixels) {
        // Source had the image fully in memory (no cache)
        if (m_storage == ImageBuf::APPBUFFER) {
            // Source just wrapped the client app's pixels, we do the same
            m_localpixels = src.m_localpixels;
            m_bufspan     = src.m_bufspan;
        } else {
            // We own our pixels -- copy from source
            new_pixels(src.m_spec.image_bytes(), src.m_pixels.get());
            // N.B. new_pixels will set m_bufspan
        }
    } else {
        // Source was cache-based or deep
        // nothing else to do
        m_localpixels = nullptr;
        m_bufspan     = span<std::byte>();
    }
    if (m_localpixels || m_spec.deep) {
        // A copied ImageBuf is no longer a direct file reference, so clear
        // some of the fields that are only meaningful for file references.
        m_fileformat.clear();
        m_nsubimages       = 1;
        m_current_subimage = 0;
        m_current_miplevel = 0;
        m_nmiplevels       = 0;
        m_spec.erase_attribute("oiio:subimages");
        m_nativespec.erase_attribute("oiio:subimages");
        m_pixels_read = true;
    }
    if (src.m_configspec)
        m_configspec.reset(new ImageSpec(*src.m_configspec));
    eval_contiguous();
}



ImageBufImpl::~ImageBufImpl()
{
    // Do NOT destroy m_imagecache here -- either it was created
    // externally and passed to the ImageBuf ctr or reset() method, or
    // else init_spec requested the system-wide shared cache, which
    // does not need to be destroyed.
    clear();

    // Upon destruction, print uncaught errors to help users who don't know
    // how to properly check for errors.
    if (!m_err.empty() /* Note: safe becausethis is the dtr */
        && pvt::imagebuf_print_uncaught_errors) {
        OIIO::print(
            "An ImageBuf was destroyed with a pending error message that was never\n"
            "retrieved via ImageBuf::geterror(). This was the error message:\n{}\n",
            m_err);
    }
}



ImageBuf::ImageBuf()
    : m_impl(new ImageBufImpl(std::string(), -1, -1, NULL), &impl_deleter)
{
}



ImageBuf::ImageBuf(string_view filename, int subimage, int miplevel,
                   std::shared_ptr<ImageCache> imagecache,
                   const ImageSpec* config, Filesystem::IOProxy* ioproxy)
    : m_impl(new ImageBufImpl(filename, subimage, miplevel,
                              std::move(imagecache), nullptr /*spec*/,
                              {} /*bufspan*/, nullptr /*buforigin*/, true,
                              config, ioproxy),
             &impl_deleter)
{
}



ImageBuf::ImageBuf(const ImageSpec& spec, InitializePixels zero)
    : m_impl(new ImageBufImpl("", 0, 0, NULL, &spec), &impl_deleter)
{
    m_impl->alloc(spec);
    // N.B. alloc will set m_bufspan
    if (zero == InitializePixels::Yes && !deep())
        ImageBufAlgo::zero(*this);
}



ImageBuf::ImageBuf(const ImageSpec& spec, void* buffer, stride_t xstride,
                   stride_t ystride, stride_t zstride)
    : m_impl(new ImageBufImpl("", 0, 0, nullptr /*imagecache*/, &spec,
                              {} /*bufspan*/, buffer /*buforigin*/,
                              false /*readonly*/, nullptr /*config*/,
                              nullptr /*ioproxy*/, xstride, ystride, zstride),
             &impl_deleter)
{
}



ImageBuf::ImageBuf(const ImageSpec& spec, cspan<std::byte> buffer,
                   void* buforigin, stride_t xstride, stride_t ystride,
                   stride_t zstride)
    : m_impl(new ImageBufImpl("", 0, 0, nullptr /*imagecache*/, &spec,
                              make_span((std::byte*)buffer.data(),
                                        buffer.size()),
                              buforigin, true /*readonly*/, nullptr /*config*/,
                              nullptr /*ioproxy*/, xstride, ystride, zstride),
             &impl_deleter)
{
}



ImageBuf::ImageBuf(const ImageSpec& spec, span<std::byte> buffer,
                   void* buforigin, stride_t xstride, stride_t ystride,
                   stride_t zstride)
    : m_impl(new ImageBufImpl("", 0, 0, NULL, &spec, buffer, buforigin,
                              false /*readonly*/, nullptr /*config*/,
                              nullptr /*ioproxy*/, xstride, ystride, zstride),
             &impl_deleter)
{
}



ImageBuf::ImageBuf(const ImageBuf& src)
    : m_impl(new ImageBufImpl(*src.m_impl), &impl_deleter)
{
}



ImageBuf::ImageBuf(ImageBuf&& src)
    : m_impl(std::move(src.m_impl))
{
}



ImageBuf::~ImageBuf() {}



const ImageBuf&
ImageBuf::operator=(const ImageBuf& src)
{
    copy(src);
    return *this;
}



const ImageBuf&
ImageBuf::operator=(ImageBuf&& src)
{
    m_impl = std::move(src.m_impl);
    return *this;
}



char*
ImageBufImpl::new_pixels(size_t size, const void* data)
{
    if (m_allocated_size)
        free_pixels();
    try {
        m_pixels.reset(size ? new char[size] : nullptr);
        // Set bufspan to the allocated memory
        m_bufspan = { reinterpret_cast<std::byte*>(m_pixels.get()), size };
    } catch (const std::exception& e) {
        // Could not allocate enough memory. So don't allocate anything,
        // consider this an uninitialized ImageBuf, issue an error, and hope
        // it's handled well downstream.
        m_pixels.reset();
        m_bufspan = make_span<std::byte>(nullptr, 0);
        OIIO::debugfmt("ImageBuf unable to allocate {} bytes ({})\n", size,
                       e.what());
        error("ImageBuf unable to allocate {} bytes ({})\n", size, e.what());
        size      = 0;
        m_bufspan = {};
    }
    m_allocated_size = size;
    pvt::IB_local_mem_current += m_allocated_size;
    atomic_max(pvt::IB_local_mem_peak, (long long)pvt::IB_local_mem_current);
    if (data && size)
        memcpy(m_pixels.get(), data, size);
    m_localpixels = m_pixels.get();
    m_storage     = size ? ImageBuf::LOCALBUFFER : ImageBuf::UNINITIALIZED;
    if (pvt::oiio_print_debug > 1)
        OIIO::debugfmt("IB allocated {} MB, global IB memory now {} MB\n",
                       size >> 20, pvt::IB_local_mem_current >> 20);
    eval_contiguous();
    return m_localpixels;
}


void
ImageBufImpl::free_pixels()
{
    if (m_allocated_size) {
        if (pvt::oiio_print_debug > 1)
            OIIO::debugfmt("IB freed {} MB, global IB memory now {} MB\n",
                           m_allocated_size >> 20,
                           pvt::IB_local_mem_current >> 20);
        pvt::IB_local_mem_current -= m_allocated_size;
        m_allocated_size = 0;
    }
    m_pixels.reset();
    // print("IB Freed pixels of length {}\n", m_bufspan.size());
    m_bufspan = make_span<std::byte>(nullptr, 0);
    m_deepdata.free();
    m_storage = ImageBuf::UNINITIALIZED;
    m_blackpixel.clear();
}



static spin_mutex err_mutex;  ///< Protect m_err fields


bool
ImageBuf::has_error() const
{
    spin_lock lock(err_mutex);
    return !m_impl->m_err.empty();
}



std::string
ImageBuf::geterror(bool clear) const
{
    spin_lock lock(err_mutex);
    std::string e = m_impl->m_err;
    if (clear)
        m_impl->m_err.clear();
    return e;
}



void
ImageBufImpl::error(string_view message) const
{
    // Remove a single trailing newline
    if (message.size() && message.back() == '\n')
        message.remove_suffix(1);
    spin_lock lock(err_mutex);
    OIIO_ASSERT(
        m_err.size() < 1024 * 1024 * 16
        && "Accumulated error messages > 16MB. Try checking return codes!");
    // If we are appending to existing error messages, separate them with
    // a single newline.
    if (m_err.size() && m_err.back() != '\n')
        m_err += '\n';
    m_err += std::string(message);
}



void
ImageBuf::error(string_view message) const
{
    m_impl->error(message);
}



ImageBuf::IBStorage
ImageBuf::storage() const
{
    return m_impl->storage();
}



void
ImageBufImpl::clear()
{
    if (m_imagecache && !m_name.empty()
        && (storage() == ImageBuf::IMAGECACHE || m_rioproxy)) {
        // If we were backed by an ImageCache, invalidate any IC entries we
        // might have made. Also do so if we were using an IOProxy, because
        // the proxy may not survive long after the ImageBuf is destroyed.
        m_imagecache->close(m_name);
        m_imagecache->invalidate(m_name, false);
    }
    free_pixels();
    m_name.clear();
    m_fileformat.clear();
    m_nsubimages       = 0;
    m_current_subimage = -1;
    m_current_miplevel = -1;
    m_spec             = ImageSpec();
    m_nativespec       = ImageSpec();
    m_pixels.reset();
    m_bufspan        = make_span<std::byte>(nullptr, 0);
    m_localpixels    = nullptr;
    m_spec_valid     = false;
    m_pixels_valid   = false;
    m_badfile        = false;
    m_pixels_read    = false;
    m_pixelaspect    = 1;
    m_xstride        = 0;
    m_ystride        = 0;
    m_zstride        = 0;
    m_channel_stride = 0;
    m_contiguous     = false;
    m_imagecache.reset();
    m_deepdata.free();
    m_blackpixel.clear();
    m_write_format.clear();
    m_write_tile_width  = 0;
    m_write_tile_height = 0;
    m_write_tile_depth  = 0;
    m_rioproxy          = nullptr;
    m_wioproxy          = nullptr;
    m_configspec.reset();
    m_thumbnail.reset();
}



void
ImageBuf::clear()
{
    m_impl->clear();
}



void
ImageBufImpl::reset(string_view filename, int subimage, int miplevel,
                    std::shared_ptr<ImageCache> imagecache,
                    const ImageSpec* config, Filesystem::IOProxy* ioproxy)
{
    clear();
    m_name = ustring(filename);
    if (m_imagecache || pvt::imagebuf_use_imagecache) {
        // Invalidate the image in cache. Do so unconditionally if there's a
        // chance that configuration hints may have changed.
        invalidate(m_name, config || m_configspec);
    }
    m_current_subimage = subimage;
    m_current_miplevel = miplevel;
    m_imagecache       = std::move(imagecache);
    if (config)
        m_configspec.reset(new ImageSpec(*config));
    m_rioproxy = ioproxy;
    if (m_rioproxy) {
        add_configspec();
        m_configspec->attribute("oiio:ioproxy", TypeDesc::PTR, &m_rioproxy);
    }
    m_bufspan = {};
    m_storage = ImageBuf::LOCALBUFFER;
    if (m_name.length() > 0) {
        // For IC-backed file ImageBuf's, call read now. For other file-based
        // images, just init the spec.
        if (m_imagecache)
            read(subimage, miplevel);
        else
            init_spec(m_name, subimage, miplevel, DoLock(true));
    }
}



void
ImageBuf::reset(string_view filename, int subimage, int miplevel,
                std::shared_ptr<ImageCache> imagecache, const ImageSpec* config,
                Filesystem::IOProxy* ioproxy)
{
    m_impl->reset(filename, subimage, miplevel, std::move(imagecache), config,
                  ioproxy);
}



void
ImageBufImpl::set_bufspan_localpixels(span<std::byte> bufspan,
                                      const void* buforigin)
{
    if (bufspan.size() && !buforigin) {
        buforigin = bufspan.data();
    } else if (buforigin && (!bufspan.data() || bufspan.empty())) {
        bufspan = span_from_buffer(const_cast<void*>(buforigin), m_spec.format,
                                   m_spec.nchannels, m_spec.width,
                                   m_spec.height, m_spec.depth, m_xstride,
                                   m_ystride, m_zstride);
    }
    m_bufspan     = bufspan;
    m_localpixels = (char*)buforigin;
    OIIO_ASSERT(check_span(m_bufspan, m_localpixels, spec().format));
}



void
ImageBufImpl::reset(string_view filename, const ImageSpec& spec,
                    const ImageSpec* nativespec, span<std::byte> bufspan,
                    const void* buforigin, bool readonly, stride_t xstride,
                    stride_t ystride, stride_t zstride)
{
    clear();
    if (!spec.image_bytes()) {
        m_storage = ImageBuf::UNINITIALIZED;
        error(
            "Could not initialize ImageBuf: the provided ImageSpec needs a valid width, height, depth, nchannels, format.");
        return;
    }
    m_name             = ustring(filename);
    m_current_subimage = 0;
    m_current_miplevel = 0;
    if (buforigin || bufspan.size()) {
        m_spec           = spec;
        m_nativespec     = nativespec ? *nativespec : spec;
        m_channel_stride = stride_t(spec.format.size());
        m_xstride        = xstride;
        m_ystride        = ystride;
        m_zstride        = zstride;
        m_readonly       = readonly;
        ImageSpec::auto_stride(m_xstride, m_ystride, m_zstride, m_spec.format,
                               m_spec.nchannels, m_spec.width, m_spec.height);
        m_blackpixel.resize(round_to_multiple(spec.pixel_bytes(),
                                              OIIO_SIMD_MAX_SIZE_BYTES),
                            0);
        set_bufspan_localpixels(bufspan, buforigin);
        m_storage      = ImageBuf::APPBUFFER;
        m_pixels_valid = true;
    } else {
        m_storage  = ImageBuf::LOCALBUFFER;
        m_readonly = false;
        alloc(spec);
        // N.B. alloc sets m_bufspan
    }
    if (nativespec)
        m_nativespec = *nativespec;
}



void
ImageBuf::reset(const ImageSpec& spec, InitializePixels zero)
{
    m_impl->reset("", spec);
    if (zero == InitializePixels::Yes && !deep())
        ImageBufAlgo::zero(*this);
}



void
ImageBuf::reset(const ImageSpec& spec, void* buffer, stride_t xstride,
                stride_t ystride, stride_t zstride)
{
    m_impl->reset("", spec, nullptr, {}, buffer, xstride, ystride, zstride);
}



void
ImageBuf::reset(const ImageSpec& spec, cspan<std::byte> buffer,
                const void* buforigin, stride_t xstride, stride_t ystride,
                stride_t zstride)
{
    m_impl->reset("", spec, nullptr,
                  { const_cast<std::byte*>(buffer.data()), buffer.size() },
                  buforigin, true /*readonly*/, xstride, ystride, zstride);
}



void
ImageBuf::reset(const ImageSpec& spec, span<std::byte> buffer,
                const void* buforigin, stride_t xstride, stride_t ystride,
                stride_t zstride)
{
    m_impl->reset("", spec, nullptr, buffer, buforigin, false /*readonly*/,
                  xstride, ystride, zstride);
}



void
ImageBufImpl::realloc()
{
    new_pixels(m_spec.deep ? size_t(0) : m_spec.image_bytes());
    // N.B. new_pixels will set m_bufspan
    m_channel_stride = m_spec.format.size();
    m_xstride        = AutoStride;
    m_ystride        = AutoStride;
    m_zstride        = AutoStride;
    ImageSpec::auto_stride(m_xstride, m_ystride, m_zstride, m_spec.format,
                           m_spec.nchannels, m_spec.width, m_spec.height);
    m_blackpixel.resize(round_to_multiple(m_xstride, OIIO_SIMD_MAX_SIZE_BYTES),
                        0);
    // NB make it big enough for SSE
    if (m_allocated_size) {
        m_pixels_valid = true;
        m_storage      = ImageBuf::LOCALBUFFER;
    }
    if (m_spec.deep) {
        m_deepdata.init(m_spec);
        m_storage = ImageBuf::LOCALBUFFER;
    }
    m_readonly    = false;
    m_pixels_read = false;
    eval_contiguous();
#if 0
    std::cerr << "ImageBuf " << m_name << " local allocation: " << m_allocated_size << "\n";
#endif
}



void
ImageBufImpl::alloc(const ImageSpec& spec, const ImageSpec* nativespec)
{
    m_spec = spec;

    // Preclude a nonsensical size
    m_spec.width     = std::max(1, m_spec.width);
    m_spec.height    = std::max(1, m_spec.height);
    m_spec.depth     = std::max(1, m_spec.depth);
    m_spec.nchannels = std::max(1, m_spec.nchannels);

    m_nativespec = nativespec ? *nativespec : spec;
    realloc();
    // N.B. realloc sets m_bufspan
    m_spec_valid = true;
}



bool
ImageBufImpl::init_spec(string_view filename, int subimage, int miplevel,
                        DoLock do_lock)
{
    lock_t lock(m_mutex, std::defer_lock_t());
    if (do_lock)
        lock.lock();

    if (!m_badfile && m_spec_valid && m_current_subimage >= 0
        && m_current_miplevel >= 0 && m_name == filename
        && m_current_subimage == subimage && m_current_miplevel == miplevel)
        return true;  // Already done

    pvt::LoggedTimer logtime("IB::init_spec");

    m_name = filename;

    // If we weren't given an imagecache but "imagebuf:use_imagecache"
    // attribute was set, use a shared IC.
    if (!m_imagecache && pvt::imagebuf_use_imagecache)
        m_imagecache = ImageCache::create(true);

    if (m_imagecache) {
        m_pixels_valid = false;
        m_nsubimages   = 0;
        m_nmiplevels   = 0;
        static ustring s_subimages("subimages"), s_miplevels("miplevels");
        static ustring s_fileformat("fileformat");
        if (m_configspec) {  // Pass configuration options to cache
            // Invalidate the file in the cache, and add with replacement
            // because it might have a different config than last time.
            m_imagecache->invalidate(m_name, true);
            m_imagecache->add_file(m_name, nullptr, m_configspec.get(),
                                   /*replace=*/true);
        } else {
            // If no configspec, just do a regular soft invalidate
            invalidate(m_name, false);
        }
        m_imagecache->get_image_info(m_name, subimage, miplevel, s_subimages,
                                     TypeInt, &m_nsubimages);
        m_imagecache->get_image_info(m_name, subimage, miplevel, s_miplevels,
                                     TypeInt, &m_nmiplevels);
        const char* fmt = NULL;
        m_imagecache->get_image_info(m_name, subimage, miplevel, s_fileformat,
                                     TypeString, &fmt);
        m_fileformat = ustring(fmt);

        m_imagecache->get_imagespec(m_name, m_nativespec, subimage);
        m_spec = m_nativespec;
        m_imagecache->get_cache_dimensions(m_name, m_spec, subimage, miplevel);

        m_xstride = m_spec.pixel_bytes();
        m_ystride = m_spec.scanline_bytes();
        m_zstride = clamped_mult64(m_ystride, (imagesize_t)m_spec.height);
        m_channel_stride = m_spec.format.size();
        m_blackpixel.resize(round_to_multiple(m_xstride,
                                              OIIO_SIMD_MAX_SIZE_BYTES),
                            0);
        // ^^^ NB make it big enough for SIMD

        // Go ahead and read any thumbnail that exists. Is that bad?
        if (m_spec["thumbnail_width"].get<int>()
            && m_spec["thumbnail_height"].get<int>()) {
            m_thumbnail.reset(new ImageBuf);
            m_imagecache->get_thumbnail(m_name, *m_thumbnail, subimage);
            m_has_thumbnail = true;
        }

        // Subtlety: m_nativespec will have the true formats of the file, but
        // we rig m_spec to reflect what it will look like in the cache.
        // This may make m_spec appear to change if there's a subsequent read()
        // that forces a full read into local memory, but what else can we do?
        // It causes havoc for it to suddenly change in the other direction
        // when the file is lazily read.
        int peltype = TypeDesc::UNKNOWN;
        m_imagecache->get_image_info(m_name, subimage, miplevel,
                                     ustring("cachedpixeltype"), TypeInt,
                                     &peltype);
        if (peltype != TypeDesc::UNKNOWN) {
            m_spec.format = (TypeDesc::BASETYPE)peltype;
            m_spec.channelformats.clear();
            m_cachedpixeltype = m_spec.format;
        }

        if (m_nsubimages) {
            m_badfile          = false;
            m_pixelaspect      = m_spec.get_float_attribute("pixelaspectratio",
                                                            1.0f);
            m_current_subimage = subimage;
            m_current_miplevel = miplevel;
            m_spec_valid       = true;
        } else {
            m_badfile          = true;
            m_current_subimage = -1;
            m_current_miplevel = -1;
            m_err              = m_imagecache->geterror();
            m_spec_valid       = false;
            // std::cerr << "ImageBuf ERROR: " << m_err << "\n";
        }
    } else {
        //
        // No imagecache supplied, we will use ImageInput directly
        //
        Timer timer;
        m_badfile          = true;
        m_pixels_valid     = false;
        m_spec_valid       = false;
        m_pixels_read      = false;
        m_nsubimages       = 0;
        m_nmiplevels       = 0;
        m_badfile          = false;
        m_current_subimage = -1;
        m_current_miplevel = -1;
        auto input = ImageInput::open(filename, m_configspec.get(), m_rioproxy);
        if (!input) {
            m_err = OIIO::geterror();
            atomic_fetch_add(pvt::IB_total_open_time, float(timer()));
            return false;
        }
        m_spec = input->spec(subimage, miplevel);
        if (input->has_error()) {
            m_err = input->geterror();
            atomic_fetch_add(pvt::IB_total_open_time, float(timer()));
            return false;
        }
        m_badfile    = false;
        m_spec_valid = true;
        m_fileformat = ustring(input->format_name());
        m_nativespec = m_spec;
        m_xstride    = m_spec.pixel_bytes();
        m_ystride    = m_spec.scanline_bytes();
        m_zstride    = clamped_mult64(m_ystride, (imagesize_t)m_spec.height);
        m_channel_stride = m_spec.format.size();
        m_blackpixel.resize(
            round_to_multiple(m_xstride, OIIO_SIMD_MAX_SIZE_BYTES));
        // ^^^ NB make it big enough for SIMD
        m_nsubimages = input->supports("multiimage")
                           ? m_spec.get_int_attribute("oiio:subimages")
                           : 1;

        // Go ahead and read any thumbnail that exists. Is that bad?
        if (m_spec["thumbnail_width"].get<int>()
            && m_spec["thumbnail_height"].get<int>()) {
            m_thumbnail.reset(new ImageBuf);
            m_has_thumbnail = input->get_thumbnail(*m_thumbnail.get(),
                                                   subimage);
        }

        m_current_subimage = subimage;
        m_current_miplevel = miplevel;
        m_pixelaspect = m_spec.get_float_attribute("pixelaspectratio", 1.0f);
        atomic_fetch_add(pvt::IB_total_open_time, float(timer()));
    }
    return !m_badfile;
}



bool
ImageBuf::init_spec(string_view filename, int subimage, int miplevel)
{
    return m_impl->init_spec(filename, subimage, miplevel,
                             DoLock(true) /* acquire the lock */);
}



bool
ImageBufImpl::read(int subimage, int miplevel, int chbegin, int chend,
                   bool force, TypeDesc convert,
                   ProgressCallback progress_callback,
                   void* progress_callback_data, DoLock do_lock)
{
    lock_t lock(m_mutex, std::defer_lock_t());
    if (do_lock)
        lock.lock();

    // If this doesn't reference a file in any way, nothing to do here.
    if (!m_name.length())
        return true;

    // If the pixels have already been read and we aren't switching
    // subimage/miplevel or being force to read (for example, turning a cached
    // image into an in-memory image), then there is nothing to do.
    if (m_pixels_valid && !force && subimage == m_current_subimage
        && miplevel == m_current_miplevel)
        return true;

    // If it's a local buffer from a file and we've already read the pixels
    // into memory, we're done, provided that we aren't asking it to force
    // a read with a different data type conversion or different number of
    // channels.
    if (m_storage == ImageBuf::LOCALBUFFER && m_pixels_valid && m_pixels_read
        && (convert == TypeUnknown || convert == m_spec.format)
        && subimage == m_current_subimage && miplevel == m_current_miplevel
        && ((chend - chbegin) == m_spec.nchannels || (chend <= chbegin)))
        return true;

    if (!init_spec(m_name.string(), subimage, miplevel,
                   DoLock(false) /* we already hold the lock */)) {
        m_badfile    = true;
        m_spec_valid = false;
        return false;
    }

    pvt::LoggedTimer logtime("IB::read");
    m_current_subimage = subimage;
    m_current_miplevel = miplevel;
    if (chend < 0 || chend > nativespec().nchannels)
        chend = nativespec().nchannels;
    bool use_channel_subset = (chbegin != 0 || chend != nativespec().nchannels);

    if (m_spec.deep) {
        Timer timer;
        auto input = ImageInput::open(m_name.string(), m_configspec.get(),
                                      m_rioproxy);
        if (!input) {
            error(OIIO::geterror());
            return false;
        }
        input->threads(threads());  // Pass on our thread policy
        bool ok = input->read_native_deep_image(subimage, miplevel, m_deepdata);
        if (ok) {
            m_spec = m_nativespec;  // Deep images always use native data
            m_pixels_valid = true;
            m_pixels_read  = true;
            m_storage      = ImageBuf::LOCALBUFFER;
        } else {
            error(input->geterror());
        }
        atomic_fetch_add(pvt::IB_total_image_read_time, float(timer()));
        return ok;
    }

    m_pixelaspect = m_spec.get_float_attribute("pixelaspectratio", 1.0f);

    if (m_imagecache) {
        // If we don't already have "local" pixels, and we aren't asking to
        // convert the pixels to a specific (and different) type, then take an
        // early out by relying on the cache.
        int peltype = TypeDesc::UNKNOWN;
        m_imagecache->get_image_info(m_name, subimage, miplevel,
                                     ustring("cachedpixeltype"), TypeInt,
                                     &peltype);
        m_cachedpixeltype = TypeDesc((TypeDesc::BASETYPE)peltype);
        if (!m_localpixels && !force && !use_channel_subset
            && (convert == m_cachedpixeltype || convert == TypeDesc::UNKNOWN)) {
            m_spec.format = m_cachedpixeltype;
            m_xstride     = m_spec.pixel_bytes();
            m_ystride     = m_spec.scanline_bytes();
            m_zstride = clamped_mult64(m_ystride, (imagesize_t)m_spec.height);
            m_blackpixel.resize(round_to_multiple(m_xstride,
                                                  OIIO_SIMD_MAX_SIZE_BYTES),
                                0);
            // NB make it big enough for SSE
            m_pixels_valid = true;
            m_storage      = ImageBuf::IMAGECACHE;
#ifndef NDEBUG
            // std::cerr << "read was not necessary -- using cache\n";
#endif
            return true;
        }

    } else {
        // No cache should take the "forced read now" route.
        force             = true;
        m_cachedpixeltype = m_nativespec.format;
    }

    if (use_channel_subset) {
        // Some adjustments because we are reading a channel subset
        force            = true;
        m_spec.nchannels = chend - chbegin;
        m_spec.channelnames.resize(m_spec.nchannels);
        for (int c = 0; c < m_spec.nchannels; ++c)
            m_spec.channelnames[c] = m_nativespec.channelnames[c + chbegin];
        if (m_nativespec.channelformats.size()) {
            m_spec.channelformats.resize(m_spec.nchannels);
            for (int c = 0; c < m_spec.nchannels; ++c)
                m_spec.channelformats[c]
                    = m_nativespec.channelformats[c + chbegin];
        }
    }

    if (convert != TypeDesc::UNKNOWN)
        m_spec.format = convert;
    else
        m_spec.format = m_nativespec.format;
    realloc();
    // N.B. realloc sets m_bufspan

    // If forcing a full read, make sure the spec reflects the nativespec's
    // tile sizes, rather than that imposed by the ImageCache.
    m_spec.tile_width  = m_nativespec.tile_width;
    m_spec.tile_height = m_nativespec.tile_height;
    m_spec.tile_depth  = m_nativespec.tile_depth;

    if (force || !m_imagecache || m_rioproxy
        || (convert != TypeDesc::UNKNOWN && convert != m_cachedpixeltype
            && convert.size() >= m_cachedpixeltype.size()
            && convert.size() >= m_nativespec.format.size())) {
        // A specific conversion type was requested which is not the cached
        // type and whose bit depth is as much or more than the cached type.
        // Bypass the cache and read directly so that there is no possible
        // loss of range or precision resulting from going through the
        // cache. Or the caller requested a forced read, for that case we
        // also do a direct read now.
        if (m_imagecache
            && (!m_configspec
                || !m_configspec->find_attribute("oiio:UnassociatedAlpha"))) {
            int unassoc = 0;
            if (m_imagecache->getattribute("unassociatedalpha", unassoc)) {
                // Since IB needs to act as if it's backed by an ImageCache,
                // even though in this case we're bypassing the IC, we need
                // to honor the IC's "unassociatedalpha" flag. But only if
                // this IB wasn't already given a config spec that dictated
                // a specific unassociated alpha behavior.
                add_configspec();
                m_configspec->attribute("oiio:UnassociatedAlpha", unassoc);
            }
        }
        Timer timer;
        auto in = ImageInput::open(m_name.string(), m_configspec.get(),
                                   m_rioproxy);
        if (in) {
            in->threads(threads());  // Pass on our thread policy
            bool ok = in->read_image(subimage, miplevel, chbegin, chend,
                                     m_spec.format, m_localpixels, AutoStride,
                                     AutoStride, AutoStride, progress_callback,
                                     progress_callback_data);
            in->close();
            if (ok) {
                m_pixels_valid = true;
                m_pixels_read  = true;
            } else {
                m_pixels_valid = false;
                error(in->geterror());
            }
        } else {
            m_pixels_valid = false;
            error(OIIO::geterror());
        }
        atomic_fetch_add(pvt::IB_total_image_read_time, float(timer()));
        // Since we have read in the entire image now, if we are using an
        // IOProxy, we invalidate any cache entry to avoid lifetime issues
        // related to the IOProxy. This helps to eliminate trouble emerging
        // from the following idiom that looks totally reasonable to the user
        // but is actually a recipe for disaster:
        //      IOProxy proxy(...);   // temporary proxy
        //      ImageBuf A ("foo.exr", 0, 0, proxy);
        //          // ^^ now theres an IC entry that knows the proxy.
        //      A.read (0, 0, true);
        //          // ^^ looks like a forced immediate read, user thinks
        //          //    they are done with the ImageBuf, but there's
        //          //    STILL a cache entry that knows the proxy.
        //      proxy.close();
        //          // ^^ now the proxy is gone, which seemed safe because
        //          //    the user thinks the forced immediate read was the
        //          //    last it'll be needed. But the cache entry still
        //          //    has a pointer to it! Oh no!
        if (m_imagecache && m_rioproxy)
            m_imagecache->invalidate(m_name);
        return m_pixels_valid;
    }

    // All other cases, no loss of precision is expected, so even a forced
    // read should go through the image cache.
    OIIO_ASSERT(m_imagecache);
    if (m_imagecache->get_pixels(m_name, subimage, miplevel, m_spec.x,
                                 m_spec.x + m_spec.width, m_spec.y,
                                 m_spec.y + m_spec.height, m_spec.z,
                                 m_spec.z + m_spec.depth, chbegin, chend,
                                 m_spec.format, m_localpixels)) {
        m_imagecache->close(m_name);
        m_pixels_valid = true;
    } else {
        m_pixels_valid = false;
        error(m_imagecache->geterror());
    }

    return m_pixels_valid;
}



bool
ImageBuf::read(int subimage, int miplevel, bool force, TypeDesc convert,
               ProgressCallback progress_callback, void* progress_callback_data)
{
    return m_impl->read(subimage, miplevel, 0, -1, force, convert,
                        progress_callback, progress_callback_data,
                        DoLock(true) /* acquire the lock */);
}



bool
ImageBuf::read(int subimage, int miplevel, int chbegin, int chend, bool force,
               TypeDesc convert, ProgressCallback progress_callback,
               void* progress_callback_data)
{
    return m_impl->read(subimage, miplevel, chbegin, chend, force, convert,
                        progress_callback, progress_callback_data,
                        DoLock(true) /* acquire the lock */);
}



void
ImageBuf::set_write_format(cspan<TypeDesc> format)
{
    m_impl->m_write_format.clear();
    if (format.size() > 0)
        m_impl->m_write_format.assign(format.data(),
                                      format.data() + format.size());
}


void
ImageBuf::set_write_format(TypeDesc format)
{
    set_write_format(cspan<TypeDesc>(format));
}



void
ImageBuf::set_write_tiles(int width, int height, int depth)
{
    m_impl->m_write_tile_width  = width;
    m_impl->m_write_tile_height = height;
    m_impl->m_write_tile_depth  = std::max(1, depth);
}



void
ImageBuf::set_write_ioproxy(Filesystem::IOProxy* ioproxy)
{
    m_impl->m_wioproxy = ioproxy;
}



bool
ImageBuf::write(ImageOutput* out, ProgressCallback progress_callback,
                void* progress_callback_data) const
{
    if (!out) {
        errorfmt("Empty ImageOutput passed to ImageBuf::write()");
        return false;
    }
    bool ok = true;
    ok &= m_impl->validate_pixels();
    pvt::LoggedTimer logtime("IB::write inner");
    if (out->supports("thumbnail") && has_thumbnail()) {
        auto thumb = get_thumbnail();
        // Strutil::print("IB::write: has thumbnail ROI {}\n", thumb->roi());
        out->set_thumbnail(*thumb);
    }
    const ImageSpec& bufspec(m_impl->m_spec);
    const ImageSpec& outspec(out->spec());
    TypeDesc bufformat = spec().format;
    if (m_impl->m_localpixels) {
        // In-core pixel buffer for the whole image
        ok = out->write_image(bufformat, m_impl->m_localpixels, pixel_stride(),
                              scanline_stride(), z_stride(), progress_callback,
                              progress_callback_data);
    } else if (deep()) {
        // Deep image record
        ok = out->write_deep_image(m_impl->m_deepdata);
    } else {
        // The image we want to write is backed by ImageCache -- we must be
        // immediately writing out a file from disk, possibly with file
        // format or data format conversion, but without any ImageBufAlgo
        // functions having been applied.
        const imagesize_t budget = 1024 * 1024 * 64;  // 64 MB
        imagesize_t imagesize    = bufspec.image_bytes();
        if (imagesize <= budget) {
            // whole image can fit within our budget
            std::unique_ptr<std::byte[]> tmp(new std::byte[imagesize]);
            ok &= get_pixels(roi(), bufformat, make_span(tmp.get(), imagesize));
            ok &= out->write_image(bufformat, tmp.get(), AutoStride, AutoStride,
                                   AutoStride, progress_callback,
                                   progress_callback_data);
        } else if (outspec.tile_width) {
            // Big tiled image: break up into tile strips
            size_t pixelsize = bufspec.pixel_bytes();
            size_t chunksize = pixelsize * outspec.width * outspec.tile_height
                               * outspec.tile_depth;
            std::unique_ptr<std::byte[]> tmp(new std::byte[chunksize]);
            auto tmpspan = make_span(tmp.get(), chunksize);
            for (int z = 0; z < outspec.depth; z += outspec.tile_depth) {
                int zend = std::min(z + outspec.z + outspec.tile_depth,
                                    outspec.z + outspec.depth);
                for (int y = 0; y < outspec.height && ok;
                     y += outspec.tile_height) {
                    int yend = std::min(y + outspec.y + outspec.tile_height,
                                        outspec.y + outspec.height);
                    ok &= get_pixels(ROI(outspec.x, outspec.x + outspec.width,
                                         outspec.y + y, yend, outspec.z + z,
                                         zend),
                                     bufformat, tmpspan);
                    ok &= out->write_tiles(outspec.x, outspec.x + outspec.width,
                                           y + outspec.y, yend, z + outspec.z,
                                           zend, bufformat, &tmp[0]);
                    if (progress_callback
                        && progress_callback(progress_callback_data,
                                             (float)(z * outspec.height + y)
                                                 / (outspec.height
                                                    * outspec.depth)))
                        return ok;
                }
            }
        } else {
            // Big scanline image: break up into scanline strips
            imagesize_t slsize = bufspec.scanline_bytes();
            int chunk = clamp(round_to_multiple(int(budget / slsize), 64), 1,
                              1024);
            std::unique_ptr<std::byte[]> tmp(new std::byte[chunk * slsize]);
            auto tmpspan = make_span(tmp.get(), chunk * slsize);

            // Special handling for flipped vertical scanline order. Right now, OpenEXR
            // is the only format that allows it, so we special case it by name. For
            // just one format, trying to be more general just seems even more awkward.
            const bool isDecreasingY = !strcmp(out->format_name(), "openexr")
                                       && outspec.get_string_attribute(
                                              "openexr:lineOrder")
                                              == "decreasingY";
            const int numChunks  = outspec.height > 0
                                       ? 1 + ((outspec.height - 1) / chunk)
                                       : 0;
            const int yLoopStart = isDecreasingY ? (numChunks - 1) * chunk : 0;
            const int yDelta     = isDecreasingY ? -chunk : chunk;
            const int yLoopEnd   = yLoopStart + numChunks * yDelta;

            for (int z = 0; z < outspec.depth; ++z) {
                for (int y = yLoopStart; y != yLoopEnd && ok; y += yDelta) {
                    int yend = std::min(y + outspec.y + chunk,
                                        outspec.y + outspec.height);
                    ok &= get_pixels(ROI(outspec.x, outspec.x + outspec.width,
                                         outspec.y + y, yend, outspec.z,
                                         outspec.z + outspec.depth),
                                     bufformat, tmpspan);
                    ok &= out->write_scanlines(y + outspec.y, yend,
                                               z + outspec.z, bufformat,
                                               &tmp[0]);
                    if (progress_callback
                        && progress_callback(
                            progress_callback_data,
                            (float)(z * outspec.height
                                    + (isDecreasingY ? (outspec.height - 1 - y)
                                                     : y))
                                / (outspec.height * outspec.depth)))
                        return ok;
                }
            }
        }
    }
    if (!ok)
        error(out->geterror());
    return ok;
}



bool
ImageBuf::write(string_view _filename, TypeDesc dtype, string_view _fileformat,
                ProgressCallback progress_callback,
                void* progress_callback_data) const
{
    pvt::LoggedTimer logtime("IB::write");
    string_view filename   = _filename.size() ? _filename : string_view(name());
    string_view fileformat = _fileformat.size() ? _fileformat : filename;
    if (filename.size() == 0) {
        errorfmt("ImageBuf::write() called with no filename");
        return false;
    }
    m_impl->validate_pixels();

    // Two complications related to our reliance on ImageCache, as we are
    // writing this image:
    // First, if we are writing over the file "in place" and this is an IC-
    // backed IB, be sure we have completely read the file into memory so we
    // don't clobber the file before we've fully read it.
    if (filename == name() && storage() == IMAGECACHE) {
        m_impl->read(subimage(), miplevel(), 0, -1, true /*force*/,
                     spec().format, nullptr, nullptr);
        if (storage() != LOCALBUFFER) {
            errorfmt("ImageBuf overwriting {} but could not force read",
                     name());
            return false;
        }
    }
    // Second, be sure to tell the ImageCache to invalidate the file we're
    // about to write. This is because (a) since we're overwriting it, any
    // pixels in the cache will then be likely wrong; (b) on Windows, if the
    // cache holds an open file handle for reading, we will not be able to
    // open the same file for writing.
    m_impl->invalidate(ustring(filename), true);

    auto out = ImageOutput::create(fileformat);
    if (!out) {
        error(geterror());
        return false;
    }
    out->threads(threads());  // Pass on our thread policy

    // Write scanline files by default, but if the file type allows tiles,
    // user can override via ImageBuf::set_write_tiles(), or by using the
    // variety of IB::write() that takes the open ImageOutput* directly.
    ImageSpec newspec = spec();
    if (out->supports("tiles") && m_impl->m_write_tile_width > 0) {
        newspec.tile_width  = m_impl->m_write_tile_width;
        newspec.tile_height = m_impl->m_write_tile_height;
        newspec.tile_depth  = std::max(1, m_impl->m_write_tile_depth);
    } else {
        newspec.tile_width  = 0;
        newspec.tile_height = 0;
        newspec.tile_depth  = 0;
    }

    // Process pixel data type overrides
    if (dtype != TypeUnknown) {
        // This call's dtype param, if set, overrides everything else
        newspec.set_format(dtype);
        newspec.channelformats.clear();
    } else if (m_impl->m_write_format.size() != 0) {
        // If set_write_format was called for the ImageBuf, it overrides
        TypeDesc biggest;  // starts as Unknown
        // Figure out the "biggest" of the channel formats, make that the
        // presumed default format.
        for (auto& f : m_impl->m_write_format)
            biggest = TypeDesc::basetype_merge(biggest, f);
        newspec.set_format(biggest);
        // Copy the channel formats, change any 'unknown' to the default
        newspec.channelformats = m_impl->m_write_format;
        newspec.channelformats.resize(newspec.nchannels, newspec.format);
        bool alldefault = true;
        for (auto& f : newspec.channelformats) {
            if (f == TypeUnknown)
                f = newspec.format;
            alldefault &= (f == newspec.format);
        }
        // If all channel formats are the same, get rid of them, the default
        // captures all the info we need.
        if (alldefault)
            newspec.channelformats.clear();
    } else {
        // No override on the ImageBuf, nor on this call to write(), so
        // we just use what is known from the imagespec.
        newspec.set_format(nativespec().format);
        newspec.channelformats = nativespec().channelformats;
    }

    if (m_impl->m_wioproxy) {
        if (!out->supports("ioproxy")
            || !out->set_ioproxy(m_impl->m_wioproxy)) {
            errorfmt("Format {} does not support writing via IOProxy",
                     out->format_name());
            return false;
        }
    }

    if (!out->open(filename, newspec)) {
        error(out->geterror());
        return false;
    }
    if (!write(out.get(), progress_callback, progress_callback_data))
        return false;
    out->close();
    if (progress_callback)
        progress_callback(progress_callback_data, 0);
    return true;
}



bool
ImageBuf::make_writable(bool keep_cache_type)
{
    if (storage() == IMAGECACHE) {
        return read(subimage(), miplevel(), 0, -1, true /*force*/,
                    keep_cache_type ? m_impl->m_cachedpixeltype : TypeDesc());
    }
    return true;
}



void
ImageBufImpl::copy_metadata(const ImageBufImpl& src)
{
    if (this == &src)
        return;
    const ImageSpec& srcspec(src.spec());
    ImageSpec& m_spec(this->specmod());
    m_spec.full_x      = srcspec.full_x;
    m_spec.full_y      = srcspec.full_y;
    m_spec.full_z      = srcspec.full_z;
    m_spec.full_width  = srcspec.full_width;
    m_spec.full_height = srcspec.full_height;
    m_spec.full_depth  = srcspec.full_depth;
    if (src.storage() == ImageBuf::IMAGECACHE) {
        // If we're copying metadata from a cached image, be sure to
        // get the file's tile size, not the cache's tile size.
        m_spec.tile_width  = src.nativespec().tile_width;
        m_spec.tile_height = src.nativespec().tile_height;
        m_spec.tile_depth  = src.nativespec().tile_depth;
    } else {
        m_spec.tile_width  = srcspec.tile_width;
        m_spec.tile_height = srcspec.tile_height;
        m_spec.tile_depth  = srcspec.tile_depth;
    }
    m_spec.extra_attribs = srcspec.extra_attribs;
}



void
ImageBuf::copy_metadata(const ImageBuf& src)
{
    m_impl->copy_metadata(*src.m_impl);
}



const ImageSpec&
ImageBuf::spec() const
{
    return m_impl->spec();
}



ImageSpec&
ImageBuf::specmod()
{
    return m_impl->specmod();
}



const ImageSpec&
ImageBuf::nativespec() const
{
    return m_impl->nativespec();
}



bool
ImageBufImpl::has_thumbnail(DoLock do_lock) const
{
    validate_spec(do_lock);
    return m_has_thumbnail;
}



void
ImageBufImpl::clear_thumbnail(DoLock do_lock)
{
    lock_t lock(m_mutex, std::defer_lock_t());
    if (do_lock)
        lock.lock();
    validate_spec(DoLock(false) /* we already hold the lock */);
    m_thumbnail.reset();
    m_spec.erase_attribute("thumbnail_width");
    m_spec.erase_attribute("thumbnail_height");
    m_spec.erase_attribute("thumbnail_nchannels");
    m_spec.erase_attribute("thumbnail_image");
    m_has_thumbnail = false;
}



void
ImageBufImpl::set_thumbnail(const ImageBuf& thumb, DoLock do_lock)
{
    lock_t lock(m_mutex, std::defer_lock_t());
    if (do_lock)
        lock.lock();
    clear_thumbnail(DoLock(false) /* we already hold the lock */);
    if (thumb.initialized()) {
        m_thumbnail.reset(new ImageBuf(thumb));
    }
}



std::shared_ptr<ImageBuf>
ImageBufImpl::get_thumbnail(DoLock do_lock) const
{
    lock_t lock(m_mutex, std::defer_lock_t());
    if (do_lock)
        lock.lock();
    validate_spec(DoLock(false) /* we already hold the lock */);
    return m_thumbnail;
}



bool
ImageBuf::has_thumbnail() const
{
    return m_impl->has_thumbnail();
}



void
ImageBuf::set_thumbnail(const ImageBuf& thumb)
{
    m_impl->set_thumbnail(thumb);
}



void
ImageBuf::clear_thumbnail()
{
    m_impl->clear_thumbnail();
}



std::shared_ptr<ImageBuf>
ImageBuf::get_thumbnail() const
{
    return m_impl->get_thumbnail();
}



string_view
ImageBuf::name(void) const
{
    return m_impl->m_name;
}


ustring
ImageBuf::uname(void) const
{
    return m_impl->m_name;
}



void
ImageBuf::set_name(string_view name)
{
    m_impl->m_name = name;
}



string_view
ImageBuf::file_format_name(void) const
{
    m_impl->validate_spec();
    return m_impl->m_fileformat;
}


int
ImageBuf::subimage() const
{
    return m_impl->m_current_subimage;
}


int
ImageBuf::nsubimages() const
{
    m_impl->validate_spec();
    return m_impl->m_nsubimages;
}


int
ImageBuf::miplevel() const
{
    return m_impl->m_current_miplevel;
}


int
ImageBuf::nmiplevels() const
{
    m_impl->validate_spec();
    return m_impl->m_nmiplevels;
}


int
ImageBuf::nchannels() const
{
    return m_impl->spec().nchannels;
}



int
ImageBuf::orientation() const
{
    m_impl->validate_spec();
    return m_impl->spec().get_int_attribute("Orientation", 1);
}



void
ImageBuf::set_orientation(int orient)
{
    m_impl->specmod().attribute("Orientation", orient);
}



bool
ImageBuf::pixels_valid(void) const
{
    return m_impl->m_pixels_valid;
}



bool
ImageBuf::pixels_read(void) const
{
    return m_impl->m_pixels_read;
}



TypeDesc
ImageBuf::pixeltype() const
{
    return m_impl->pixeltype();
}



void*
ImageBuf::localpixels()
{
    m_impl->validate_pixels();
    return m_impl->m_localpixels;
}



const void*
ImageBuf::localpixels() const
{
    m_impl->validate_pixels();
    return m_impl->m_localpixels;
}



stride_t
ImageBuf::pixel_stride() const
{
    return m_impl->m_xstride;
}



stride_t
ImageBuf::scanline_stride() const
{
    return m_impl->m_ystride;
}



stride_t
ImageBuf::z_stride() const
{
    return m_impl->m_zstride;
}



bool
ImageBuf::contiguous() const
{
    return m_impl->m_contiguous;
}



bool
ImageBuf::cachedpixels() const
{
    return m_impl->cachedpixels();
}



std::shared_ptr<ImageCache>
ImageBuf::imagecache() const
{
    return m_impl->m_imagecache;
}



bool
ImageBuf::deep() const
{
    return spec().deep;
}


DeepData*
ImageBuf::deepdata()
{
    return m_impl->deepdata();
}


const DeepData*
ImageBuf::deepdata() const
{
    return m_impl->deepdata();
}


bool
ImageBuf::initialized() const
{
    return m_impl->initialized();
}



void
ImageBuf::threads(int n) const
{
    m_impl->threads(n);
}



int
ImageBuf::threads() const
{
    return m_impl->threads();
}



namespace {

// Pixel-by-pixel copy fully templated by both data types.
// The roi is guaranteed to exist in both images.
template<class D, class S>
static bool
copy_pixels_impl(ImageBuf& dst, const ImageBuf& src, ROI roi, int nthreads = 0)
{
    std::atomic<bool> ok(true);
    ImageBufAlgo::parallel_image(roi, { "copy_pixels", nthreads }, [&](ROI roi) {
        int nchannels = roi.nchannels();
        if (std::is_same<D, S>::value) {
            // If both bufs are the same type, just directly copy the values
            if (src.localpixels() && roi.chbegin == 0
                && roi.chend == dst.nchannels()
                && roi.chend == src.nchannels()) {
                // Extra shortcut -- totally local pixels for src, copying all
                // channels, so we can copy memory around line by line, rather
                // than value by value.
                int nxvalues = roi.width() * dst.nchannels();
                for (int z = roi.zbegin; z < roi.zend; ++z)
                    for (int y = roi.ybegin; y < roi.yend; ++y) {
                        D* draw       = (D*)dst.pixeladdr(roi.xbegin, y, z);
                        const S* sraw = (const S*)src.pixeladdr(roi.xbegin, y,
                                                                z);
                        OIIO_DASSERT(draw && sraw);
                        for (int x = 0; x < nxvalues; ++x)
                            draw[x] = sraw[x];
                    }
            } else {
                ImageBuf::Iterator<D, D> d(dst, roi);
                ImageBuf::ConstIterator<D, D> s(src, roi);
                for (; !d.done(); ++d, ++s) {
                    for (int c = 0; c < nchannels; ++c)
                        d[c] = s[c];
                }
                if (s.has_error())
                    ok = false;
            }
        } else {
            // If the two bufs are different types, convert through float
            ImageBuf::Iterator<D> d(dst, roi);
            ImageBuf::ConstIterator<S> s(src, roi);
            for (; !d.done(); ++d, ++s) {
                for (int c = 0; c < nchannels; ++c)
                    d[c] = s[c];
            }
            if (s.has_error())
                ok = false;
        }
    });
    return ok;
}

}  // namespace



bool
ImageBuf::copy_pixels(const ImageBuf& src)
{
    if (this == &src)
        return true;

    if (deep() || src.deep())
        return false;  // This operation is not supported for deep images

    // compute overlap
    ROI myroi = get_roi(spec());
    ROI roi   = roi_intersection(myroi, get_roi(src.spec()));

    // If we aren't copying over all our pixels, zero out the pixels
    if (roi != myroi)
        ImageBufAlgo::zero(*this);

    bool ok;
    OIIO_DISPATCH_TYPES2(ok, "copy_pixels", copy_pixels_impl, spec().format,
                         src.spec().format, *this, src, roi);
    // N.B.: it's tempting to change this to OIIO_DISPATCH_COMMON_TYPES2,
    // but don't! Because the DISPATCH_COMMON macros themselves depend
    // on copy() to convert from rare types to common types, eventually
    // we need to bottom out with something that handles all types, and
    // this is the place where that happens.

    // A copied ImageBuf is no longer a direct file reference, so clear some
    // of the fields that are only meaningful for file references.
    m_impl->m_fileformat.clear();
    m_impl->m_nsubimages       = 1;
    m_impl->m_current_subimage = 0;
    m_impl->m_current_miplevel = 0;
    m_impl->m_nmiplevels       = 0;
    m_impl->m_spec.erase_attribute("oiio:subimages");
    m_impl->m_nativespec.erase_attribute("oiio:subimages");

    return ok;
}



bool
ImageBuf::copy(const ImageBuf& src, TypeDesc format)
{
    src.m_impl->validate_pixels();
    if (this == &src)  // self-assignment
        return true;
    if (src.storage() == UNINITIALIZED) {  // buf = uninitialized
        clear();
        return true;
    }
    if (src.deep()) {
        m_impl->reset(src.name(), src.spec(), &src.nativespec());
        m_impl->m_deepdata = src.m_impl->m_deepdata;
        return true;
    }
    if (format.basetype == TypeDesc::UNKNOWN || src.deep())
        m_impl->reset(src.name(), src.spec(), &src.nativespec());
    else {
        ImageSpec newspec(src.spec());
        newspec.set_format(format);
        newspec.channelformats.clear();
        reset(newspec);
    }
    return this->copy_pixels(src);
}



ImageBuf
ImageBuf::copy(TypeDesc format) const
{
    ImageBuf result;
    result.copy(*this, format);
    return result;
}



template<typename T>
static inline float
getchannel_(const ImageBuf& buf, int x, int y, int z, int c,
            ImageBuf::WrapMode wrap)
{
    ImageBuf::ConstIterator<T> pixel(buf, x, y, z, wrap);
    return pixel[c];
}



float
ImageBuf::getchannel(int x, int y, int z, int c, WrapMode wrap) const
{
    if (c < 0 || c >= spec().nchannels)
        return 0.0f;
    float ret;
    OIIO_DISPATCH_TYPES(ret, "getchannel", getchannel_, spec().format, *this, x,
                        y, z, c, wrap);
    return ret;
}



template<typename T>
static bool
getpixel_(const ImageBuf& buf, int x, int y, int z, span<float> result,
          ImageBuf::WrapMode wrap)
{
    OIIO_DASSERT(result.size() <= size_t(buf.spec().nchannels));
    ImageBuf::ConstIterator<T> pixel(buf, x, y, z, wrap);
    for (size_t i = 0, e = result.size(); i < e; ++i)
        result[i] = pixel[i];
    return true;
}



inline bool
getpixel_wrapper(int x, int y, int z, span<float> pixel,
                 ImageBuf::WrapMode wrap, const ImageBuf& ib)
{
    bool ok;
    OIIO_DISPATCH_TYPES(ok, "getpixel", getpixel_, ib.spec().format, ib, x, y,
                        z, pixel, wrap);
    return ok;
}



void
ImageBuf::getpixel(int x, int y, int z, span<float> pixel, WrapMode wrap) const
{
    pixel = pixel.subspan(0, std::min(size_t(spec().nchannels), pixel.size()));
    getpixel_wrapper(x, y, z, pixel, wrap, *this);
}



template<class T>
static bool
interppixel_(const ImageBuf& img, float x, float y, span<float> pixel,
             ImageBuf::WrapMode wrap)
{
    int n             = std::min(int(pixel.size()), img.spec().nchannels);
    float* localpixel = OIIO_ALLOCA(float, n * 4);
    float* p[4]       = { localpixel, localpixel + n, localpixel + 2 * n,
                          localpixel + 3 * n };
    x -= 0.5f;
    y -= 0.5f;
    int xtexel, ytexel;
    float xfrac, yfrac;
    xfrac = floorfrac(x, &xtexel);
    yfrac = floorfrac(y, &ytexel);
    ImageBuf::ConstIterator<T> it(img, xtexel, xtexel + 2, ytexel, ytexel + 2,
                                  0, 1, wrap);
    for (int i = 0; i < 4; ++i, ++it)
        for (int c = 0; c < n; ++c)
            p[i][c] = it[c];  //NOSONAR
    bilerp(p[0], p[1], p[2], p[3], xfrac, yfrac, n, pixel.data());
    return true;
}



inline bool
interppixel_wrapper(float x, float y, span<float> pixel,
                    ImageBuf::WrapMode wrap, const ImageBuf& img)
{
    bool ok;
    OIIO_DISPATCH_TYPES(ok, "interppixel", interppixel_, img.spec().format, img,
                        x, y, pixel, wrap);
    return ok;
}



void
ImageBuf::interppixel(float x, float y, span<float> pixel, WrapMode wrap) const
{
    interppixel_wrapper(x, y, pixel, wrap, *this);
}



void
ImageBuf::interppixel_NDC(float x, float y, span<float> pixel,
                          WrapMode wrap) const
{
    const ImageSpec& spec(m_impl->spec());
    interppixel(static_cast<float>(spec.full_x)
                    + x * static_cast<float>(spec.full_width),
                static_cast<float>(spec.full_y)
                    + y * static_cast<float>(spec.full_height),
                pixel, wrap);
}



template<class T>
static bool
interppixel_bicubic_(const ImageBuf& img, float x, float y, span<float> pixel,
                     ImageBuf::WrapMode wrap)
{
    int n = std::min(img.spec().nchannels, int(pixel.size()));
    x -= 0.5f;
    y -= 0.5f;
    int xtexel, ytexel;
    float xfrac, yfrac;
    xfrac = floorfrac(x, &xtexel);
    yfrac = floorfrac(y, &ytexel);

    float wx[4];
    evalBSplineWeights(wx, xfrac);
    float wy[4];
    evalBSplineWeights(wy, yfrac);
    for (int c = 0; c < n; ++c)
        pixel[c] = 0.0f;
    ImageBuf::ConstIterator<T> it(img, xtexel - 1, xtexel + 3, ytexel - 1,
                                  ytexel + 3, 0, 1, wrap);
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i, ++it) {
            float w = wx[i] * wy[j];
            for (int c = 0; c < n; ++c)
                pixel[c] += w * it[c];
        }
    }
    return true;
}



inline bool
interppixel_bicubic_wrapper(float x, float y, span<float> pixel,
                            ImageBuf::WrapMode wrap, const ImageBuf& img)
{
    bool ok;
    OIIO_DISPATCH_TYPES(ok, "interppixel_bicubic", interppixel_bicubic_,
                        img.spec().format, img, x, y, pixel, wrap);
    return ok;
}



void
ImageBuf::interppixel_bicubic(float x, float y, span<float> pixel,
                              WrapMode wrap) const
{
    interppixel_bicubic_wrapper(x, y, pixel, wrap, *this);
}



void
ImageBuf::interppixel_bicubic_NDC(float x, float y, span<float> pixel,
                                  WrapMode wrap) const
{
    const ImageSpec& spec(m_impl->spec());
    interppixel_bicubic(static_cast<float>(spec.full_x)
                            + x * static_cast<float>(spec.full_width),
                        static_cast<float>(spec.full_y)
                            + y * static_cast<float>(spec.full_height),
                        pixel, wrap);
}



template<typename T>
inline void
setpixel_(ImageBuf& buf, int x, int y, int z, const float* data, int chans)
{
    ImageBuf::Iterator<T> pixel(buf, x, y, z);
    if (pixel.exists()) {
        for (int i = 0; i < chans; ++i)
            pixel[i] = data[i];
    }
}



void
ImageBuf::setpixel(int x, int y, int z, cspan<float> pixelspan)
{
    const float* pixel = pixelspan.data();
    int n              = std::min(spec().nchannels, int(pixelspan.size()));
    switch (spec().format.basetype) {
    case TypeDesc::FLOAT: setpixel_<float>(*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT8:
        setpixel_<unsigned char>(*this, x, y, z, pixel, n);
        break;
    case TypeDesc::INT8: setpixel_<char>(*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT16:
        setpixel_<unsigned short>(*this, x, y, z, pixel, n);
        break;
    case TypeDesc::INT16: setpixel_<short>(*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT:
        setpixel_<unsigned int>(*this, x, y, z, pixel, n);
        break;
    case TypeDesc::INT: setpixel_<int>(*this, x, y, z, pixel, n); break;
    case TypeDesc::HALF: setpixel_<half>(*this, x, y, z, pixel, n); break;
    case TypeDesc::DOUBLE: setpixel_<double>(*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT64:
        setpixel_<unsigned long long>(*this, x, y, z, pixel, n);
        break;
    case TypeDesc::INT64: setpixel_<long long>(*this, x, y, z, pixel, n); break;
    default:
        OIIO_ASSERT_MSG(0, "Unknown/unsupported data type %d",
                        spec().format.basetype);
    }
}



template<typename D, typename S>
static bool
get_pixels_(const ImageBuf& buf, const ImageBuf& /*dummy*/, ROI whole_roi,
            ROI roi, void* r_, stride_t xstride, stride_t ystride,
            stride_t zstride, int nthreads = 0)
{
    std::atomic<bool> ok(true);
    ImageBufAlgo::parallel_image(
        roi, { "get_pixels", nthreads }, [=, &buf, &ok](ROI roi) {
            D* r       = (D*)r_;
            int nchans = roi.nchannels();
            ImageBuf::ConstIterator<S, D> p(buf, roi);
            for (; !p.done(); ++p) {
                imagesize_t offset = (p.z() - whole_roi.zbegin) * zstride
                                     + (p.y() - whole_roi.ybegin) * ystride
                                     + (p.x() - whole_roi.xbegin) * xstride;
                D* rc = (D*)((char*)r + offset);
                for (int c = 0; c < nchans; ++c)
                    rc[c] = p[c + roi.chbegin];
            }
            if (p.has_error())
                ok = false;
        });
    return ok;
}



bool
ImageBuf::get_pixels(ROI roi, TypeDesc format, span<std::byte> buffer,
                     void* buforigin, stride_t xstride, stride_t ystride,
                     stride_t zstride) const
{
    if (!roi.defined())
        roi = this->roi();
    roi.chend = std::min(roi.chend, nchannels());
    ImageSpec::auto_stride(xstride, ystride, zstride, format.size(),
                           roi.nchannels(), roi.width(), roi.height());
    void* result = buforigin ? buforigin : buffer.data();
    auto range = span_from_buffer(result, format, roi.nchannels(), roi.width(),
                                  roi.height(), roi.depth(), xstride, ystride,
                                  zstride);
    if (!span_within(buffer, range)) {
        errorfmt("get_pixels: buffer span does not contain the ROI dimensions");
        return false;
    }
    if (localpixels() && this->roi().contains(roi)) {
        // Easy case -- if the buffer is already fully in memory and the roi
        // is completely contained in the pixel window, this reduces to a
        // parallel_convert_image, which is both threaded and already
        // handles many special cases.
        return parallel_convert_image(
            roi.nchannels(), roi.width(), roi.height(), roi.depth(),
            pixeladdr(roi.xbegin, roi.ybegin, roi.zbegin, roi.chbegin),
            spec().format, pixel_stride(), scanline_stride(), z_stride(),
            result, format, xstride, ystride, zstride, threads());
    }

    // General case -- can handle IC-backed images.
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2_CONST(ok, "get_pixels", get_pixels_, format,
                                      spec().format, *this, *this, roi, roi,
                                      result, xstride, ystride, zstride,
                                      threads());
    return ok;
}



bool
ImageBuf::get_pixels(ROI roi, TypeDesc format, void* result, stride_t xstride,
                     stride_t ystride, stride_t zstride) const
{
    if (!roi.defined())
        roi = this->roi();
    roi.chend = std::min(roi.chend, nchannels());
    ImageSpec::auto_stride(xstride, ystride, zstride, format.size(),
                           roi.nchannels(), roi.width(), roi.height());
    auto range = span_from_buffer(result, format, roi.nchannels(), roi.width(),
                                  roi.height(), roi.depth(), xstride, ystride,
                                  zstride);
    return get_pixels(roi, format, range, result, xstride, ystride, zstride);
}



template<typename D, typename S>
static bool
set_pixels_(ImageBuf& buf, ROI roi, const void* data_, stride_t xstride,
            stride_t ystride, stride_t zstride)
{
    const D* data = (const D*)data_;
    int nchans    = roi.nchannels();
    for (ImageBuf::Iterator<D, S> p(buf, roi); !p.done(); ++p) {
        if (!p.exists())
            continue;
        imagesize_t offset = (p.z() - roi.zbegin) * zstride
                             + (p.y() - roi.ybegin) * ystride
                             + (p.x() - roi.xbegin) * xstride;
        const S* src = (const S*)((const char*)data + offset);
        for (int c = 0; c < nchans; ++c)
            p[c + roi.chbegin] = src[c];
    }
    return true;
}



bool
ImageBuf::set_pixels(ROI roi, TypeDesc format, const void* data,
                     stride_t xstride, stride_t ystride, stride_t zstride)
{
    if (!initialized()) {
        errorfmt("Cannot set_pixels() on an uninitialized ImageBuf");
        return false;
    }
    if (!roi.defined())
        roi = this->roi();
    roi.chend = std::min(roi.chend, nchannels());

    ImageSpec::auto_stride(xstride, ystride, zstride, format.size(),
                           roi.nchannels(), roi.width(), roi.height());

    bool ok;
    OIIO_DISPATCH_TYPES2(ok, "set_pixels", set_pixels_, spec().format, format,
                         *this, roi, data, xstride, ystride, zstride);
    return ok;
}



bool
ImageBuf::set_pixels(ROI roi, TypeDesc format, cspan<std::byte> buffer,
                     const void* buforigin, stride_t xstride, stride_t ystride,
                     stride_t zstride)
{
    if (!initialized()) {
        errorfmt("Cannot set_pixels() on an uninitialized ImageBuf");
        return false;
    }
    bool ok;
    if (!roi.defined())
        roi = this->roi();
    roi.chend = std::min(roi.chend, nchannels());

    ImageSpec::auto_stride(xstride, ystride, zstride, format.size(),
                           roi.nchannels(), roi.width(), roi.height());
    const void* result = buforigin ? buforigin : buffer.data();
    auto range = cspan_from_buffer(result, format, roi.nchannels(), roi.width(),
                                   roi.height(), roi.depth(), xstride, ystride,
                                   zstride);
    if (!span_within(buffer, range)) {
        errorfmt("set_pixels: buffer span does not contain the ROI dimensions");
        return false;
    }

    OIIO_DISPATCH_TYPES2(ok, "set_pixels", set_pixels_, spec().format, format,
                         *this, roi, result, xstride, ystride, zstride);
    return ok;
}



int
ImageBuf::deep_samples(int x, int y, int z) const
{
    m_impl->validate_pixels();
    if (!deep())
        return 0;
    int p = m_impl->pixelindex(x, y, z, true);
    return p >= 0 ? deepdata()->samples(p) : 0;
}



const void*
ImageBuf::deep_pixel_ptr(int x, int y, int z, int c, int s) const
{
    m_impl->validate_pixels();
    if (!deep())
        return NULL;
    const ImageSpec& m_spec(spec());
    int p = m_impl->pixelindex(x, y, z, true);
    if (p < 0 || c < 0 || c >= m_spec.nchannels)
        return NULL;
    return (s < deepdata()->samples(p)) ? deepdata()->data_ptr(p, c, s) : NULL;
}



float
ImageBuf::deep_value(int x, int y, int z, int c, int s) const
{
    m_impl->validate_pixels();
    if (!deep())
        return 0.0f;
    int p = m_impl->pixelindex(x, y, z);
    return m_impl->m_deepdata.deep_value(p, c, s);
}



uint32_t
ImageBuf::deep_value_uint(int x, int y, int z, int c, int s) const
{
    m_impl->validate_pixels();
    if (!deep())
        return 0;
    int p = m_impl->pixelindex(x, y, z);
    return m_impl->m_deepdata.deep_value_uint(p, c, s);
}



void
ImageBuf::set_deep_samples(int x, int y, int z, int samps)
{
    if (!deep())
        return;
    int p = m_impl->pixelindex(x, y, z);
    m_impl->m_deepdata.set_samples(p, samps);
}



void
ImageBuf::deep_insert_samples(int x, int y, int z, int samplepos, int nsamples)
{
    if (!deep())
        return;
    int p = m_impl->pixelindex(x, y, z);
    m_impl->m_deepdata.insert_samples(p, samplepos, nsamples);
}



void
ImageBuf::deep_erase_samples(int x, int y, int z, int samplepos, int nsamples)
{
    if (!deep())
        return;
    int p = m_impl->pixelindex(x, y, z);
    m_impl->m_deepdata.erase_samples(p, samplepos, nsamples);
}



void
ImageBuf::set_deep_value(int x, int y, int z, int c, int s, float value)
{
    m_impl->validate_pixels();
    if (!deep())
        return;
    int p = m_impl->pixelindex(x, y, z);
    return m_impl->m_deepdata.set_deep_value(p, c, s, value);
}



void
ImageBuf::set_deep_value(int x, int y, int z, int c, int s, uint32_t value)
{
    m_impl->validate_pixels();
    if (!deep())
        return;
    int p = m_impl->pixelindex(x, y, z);
    return m_impl->m_deepdata.set_deep_value(p, c, s, value);
}



bool
ImageBuf::copy_deep_pixel(int x, int y, int z, const ImageBuf& src, int srcx,
                          int srcy, int srcz)
{
    m_impl->validate_pixels();
    src.m_impl->validate_pixels();
    if (!deep() || !src.deep())
        return false;
    int p    = pixelindex(x, y, z);
    int srcp = src.pixelindex(srcx, srcy, srcz);
    return m_impl->m_deepdata.copy_deep_pixel(p, *src.deepdata(), srcp);
}



int
ImageBuf::xbegin() const
{
    return spec().x;
}



int
ImageBuf::xend() const
{
    return spec().x + spec().width;
}



int
ImageBuf::ybegin() const
{
    return spec().y;
}



int
ImageBuf::yend() const
{
    return spec().y + spec().height;
}



int
ImageBuf::zbegin() const
{
    return spec().z;
}



int
ImageBuf::zend() const
{
    return spec().z + std::max(spec().depth, 1);
}



int
ImageBuf::xmin() const
{
    return spec().x;
}



int
ImageBuf::xmax() const
{
    return spec().x + spec().width - 1;
}



int
ImageBuf::ymin() const
{
    return spec().y;
}



int
ImageBuf::ymax() const
{
    return spec().y + spec().height - 1;
}



int
ImageBuf::zmin() const
{
    return spec().z;
}



int
ImageBuf::zmax() const
{
    return spec().z + std::max(spec().depth, 1) - 1;
}


int
ImageBuf::oriented_width() const
{
    const ImageSpec& spec(m_impl->spec());
    return orientation() <= 4 ? spec.width : spec.height;
}



int
ImageBuf::oriented_height() const
{
    const ImageSpec& spec(m_impl->spec());
    return orientation() <= 4 ? spec.height : spec.width;
}



int
ImageBuf::oriented_x() const
{
    const ImageSpec& spec(m_impl->spec());
    return orientation() <= 4 ? spec.x : spec.y;
}



int
ImageBuf::oriented_y() const
{
    const ImageSpec& spec(m_impl->spec());
    return orientation() <= 4 ? spec.y : spec.x;
}



int
ImageBuf::oriented_full_width() const
{
    const ImageSpec& spec(m_impl->spec());
    return orientation() <= 4 ? spec.full_width : spec.full_height;
}



int
ImageBuf::oriented_full_height() const
{
    const ImageSpec& spec(m_impl->spec());
    return orientation() <= 4 ? spec.full_height : spec.full_width;
}



int
ImageBuf::oriented_full_x() const
{
    const ImageSpec& spec(m_impl->spec());
    return orientation() <= 4 ? spec.full_x : spec.full_y;
}



int
ImageBuf::oriented_full_y() const
{
    const ImageSpec& spec(m_impl->spec());
    return orientation() <= 4 ? spec.full_y : spec.full_x;
}



void
ImageBuf::set_origin(int x, int y, int z)
{
    ImageSpec& spec(m_impl->specmod());
    spec.x = x;
    spec.y = y;
    spec.z = z;
}



void
ImageBuf::set_full(int xbegin, int xend, int ybegin, int yend, int zbegin,
                   int zend)
{
    ImageSpec& m_spec(m_impl->specmod());
    m_spec.full_x      = xbegin;
    m_spec.full_y      = ybegin;
    m_spec.full_z      = zbegin;
    m_spec.full_width  = xend - xbegin;
    m_spec.full_height = yend - ybegin;
    m_spec.full_depth  = zend - zbegin;
}



ROI
ImageBuf::roi() const
{
    return get_roi(spec());
}


ROI
ImageBuf::roi_full() const
{
    return get_roi_full(spec());
}


void
ImageBuf::set_roi_full(const ROI& newroi)
{
    OIIO::set_roi_full(specmod(), newroi);
}



bool
ImageBuf::contains_roi(const ROI& roi) const
{
    ROI myroi = this->roi();
    return (roi.defined() && myroi.defined() && roi.xbegin >= myroi.xbegin
            && roi.xend <= myroi.xend && roi.ybegin >= myroi.ybegin
            && roi.yend <= myroi.yend && roi.zbegin >= myroi.zbegin
            && roi.zend <= myroi.zend && roi.chbegin >= myroi.chbegin
            && roi.chend <= myroi.chend);
}



const void*
ImageBufImpl::pixeladdr(int x, int y, int z, int ch) const
{
    if (cachedpixels())
        return nullptr;
    validate_pixels();
    x -= m_spec.x;
    y -= m_spec.y;
    z -= m_spec.z;
    stride_t p = y * m_ystride + x * m_xstride + z * m_zstride
                 + ch * m_channel_stride;
    return &(m_localpixels[p]);
}



void*
ImageBufImpl::pixeladdr(int x, int y, int z, int ch)
{
    validate_pixels();
    if (cachedpixels())
        return nullptr;
    x -= m_spec.x;
    y -= m_spec.y;
    z -= m_spec.z;
    size_t p = y * m_ystride + x * m_xstride + z * m_zstride
               + ch * m_channel_stride;
    return &(m_localpixels[p]);
}



const void*
ImageBuf::pixeladdr(int x, int y, int z, int ch) const
{
    return m_impl->pixeladdr(x, y, z, ch);
}



void*
ImageBuf::pixeladdr(int x, int y, int z, int ch)
{
    return m_impl->pixeladdr(x, y, z, ch);
}



int
ImageBuf::pixelindex(int x, int y, int z, bool check_range) const
{
    return m_impl->pixelindex(x, y, z, check_range);
}



const void*
ImageBuf::blackpixel() const
{
    return m_impl->blackpixel();
}



bool
ImageBufImpl::do_wrap(int& x, int& y, int& z, ImageBuf::WrapMode wrap) const
{
    const ImageSpec& m_spec(this->spec());

    // Double check that we're outside the data window -- supposedly a
    // precondition of calling this method.
    OIIO_DASSERT(!(x >= m_spec.x && x < m_spec.x + m_spec.width && y >= m_spec.y
                   && y < m_spec.y + m_spec.height && z >= m_spec.z
                   && z < m_spec.z + m_spec.depth));

    // Wrap based on the display window
    if (wrap == ImageBuf::WrapBlack) {
        // no remapping to do
        return false;  // still outside the data window
    } else if (wrap == ImageBuf::WrapClamp) {
        x = OIIO::clamp(x, m_spec.full_x,
                        m_spec.full_x + m_spec.full_width - 1);
        y = OIIO::clamp(y, m_spec.full_y,
                        m_spec.full_y + m_spec.full_height - 1);
        z = OIIO::clamp(z, m_spec.full_z,
                        m_spec.full_z + m_spec.full_depth - 1);
    } else if (wrap == ImageBuf::WrapPeriodic) {
        wrap_periodic(x, m_spec.full_x, m_spec.full_width);
        wrap_periodic(y, m_spec.full_y, m_spec.full_height);
        wrap_periodic(z, m_spec.full_z, m_spec.full_depth);
    } else if (wrap == ImageBuf::WrapMirror) {
        wrap_mirror(x, m_spec.full_x, m_spec.full_width);
        wrap_mirror(y, m_spec.full_y, m_spec.full_height);
        wrap_mirror(z, m_spec.full_z, m_spec.full_depth);
    } else {
        OIIO_ASSERT_MSG(0, "unknown wrap mode %d", (int)wrap);
    }

    // Now determine if the new position is within the data window
    return (x >= m_spec.x && x < m_spec.x + m_spec.width && y >= m_spec.y
            && y < m_spec.y + m_spec.height && z >= m_spec.z
            && z < m_spec.z + m_spec.depth);
}



bool
ImageBuf::do_wrap(int& x, int& y, int& z, WrapMode wrap) const
{
    return m_impl->do_wrap(x, y, z, wrap);
}



static const ustring wrapnames[] = { ustring("default"), ustring("black"),
                                     ustring("clamp"), ustring("periodic"),
                                     ustring("mirror") };


ImageBuf::WrapMode
ImageBuf::WrapMode_from_string(string_view name)
{
    int i = 0;
    for (auto w : wrapnames) {
        if (name == w)
            return WrapMode(i);
        ++i;
    }
    return WrapDefault;  // name not found
}


ustring
ImageBuf::wrapmode_name(WrapMode wrap)
{
    unsigned int w(wrap);
    return w <= 4 ? wrapnames[w] : wrapnames[0];
}



void
ImageBuf::IteratorBase::release_tile()
{
    auto ic = m_ib->imagecache();
    OIIO_DASSERT(ic);
    ic->release_tile(m_tile);
}



const void*
ImageBufImpl::retile(int x, int y, int z, ImageCache::Tile*& tile,
                     int& tilexbegin, int& tileybegin, int& tilezbegin,
                     int& tilexend, bool& haderror, bool exists,
                     ImageBuf::WrapMode wrap) const
{
    OIIO_DASSERT(m_imagecache);
    if (!exists) {
        // Special case -- (x,y,z) describes a location outside the data
        // window.  Use the wrap mode to possibly give a meaningful data
        // proxy to point to.
        if (!do_wrap(x, y, z, wrap)) {
            // After wrapping, the new xyz point outside the data window.
            // So return the black pixel.
            return &m_blackpixel[0];
        }
        // We've adjusted x,y,z, and know the wrapped coordinates are in the
        // pixel data window, so now fall through below to get the right
        // tile.
    }

    OIIO_DASSERT(x >= m_spec.x && x < m_spec.x + m_spec.width && y >= m_spec.y
                 && y < m_spec.y + m_spec.height && z >= m_spec.z
                 && z < m_spec.z + m_spec.depth);

    int tw = m_spec.tile_width, th = m_spec.tile_height;
    int td = m_spec.tile_depth;
    OIIO_DASSERT(m_spec.tile_depth >= 1);
    OIIO_DASSERT(tile == NULL || tilexend == (tilexbegin + tw));
    if (tile == NULL || x < tilexbegin || x >= tilexend || y < tileybegin
        || y >= (tileybegin + th) || z < tilezbegin || z >= (tilezbegin + td)) {
        // not the same tile as before
        if (tile)
            m_imagecache->release_tile(tile);
        int xtile  = (x - m_spec.x) / tw;
        int ytile  = (y - m_spec.y) / th;
        int ztile  = (z - m_spec.z) / td;
        tilexbegin = m_spec.x + xtile * tw;
        tileybegin = m_spec.y + ytile * th;
        tilezbegin = m_spec.z + ztile * td;
        tilexend   = tilexbegin + tw;
        tile       = m_imagecache->get_tile(m_name, m_current_subimage,
                                            m_current_miplevel, x, y, z);
        if (!tile) {
            // Even though tile is NULL, ensure valid black pixel data
            std::string e = m_imagecache->geterror();
            if (e.size())
                error("{}", e);
            haderror = true;
            return &m_blackpixel[0];
        }
    }

    size_t offset = ((z - tilezbegin) * (size_t)th + (y - tileybegin))
                        * (size_t)tw
                    + (x - tilexbegin);
    offset *= m_spec.pixel_bytes();
    OIIO_DASSERT_MSG(m_spec.pixel_bytes() == size_t(m_xstride), "%d vs %d",
                     (int)m_spec.pixel_bytes(), (int)m_xstride);

    TypeDesc format;
    const void* pixeldata = m_imagecache->tile_pixels(tile, format);
    return pixeldata ? (const char*)pixeldata + offset : NULL;
}



const void*
ImageBuf::retile(int x, int y, int z, ImageCache::Tile*& tile, int& tilexbegin,
                 int& tileybegin, int& tilezbegin, int& tilexend,
                 bool& haderror, bool exists, WrapMode wrap) const
{
    return m_impl->retile(x, y, z, tile, tilexbegin, tileybegin, tilezbegin,
                          tilexend, haderror, exists, wrap);
}



ImageBuf::IteratorBase::IteratorBase(const ImageBuf& ib, WrapMode wrap,
                                     bool write)
    : m_ib(&ib)
{
    init_ib(wrap, write);
    range_is_image();
    pos(m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
    if (m_rng_xbegin == m_rng_xend || m_rng_ybegin == m_rng_yend
        || m_rng_zbegin == m_rng_zend)
        pos_done();  // make empty range look "done"
}



ImageBuf::IteratorBase::IteratorBase(const ImageBuf& ib, int x, int y, int z,
                                     WrapMode wrap, bool write)
    : m_ib(&ib)
{
    init_ib(wrap, write);
    range_is_image();
    pos(x, y, z);
}



ImageBuf::IteratorBase::IteratorBase(const ImageBuf& ib, const ROI& roi,
                                     WrapMode wrap, bool write)
    : m_ib(&ib)
{
    init_ib(wrap, write);
    if (roi.defined()) {
        m_rng_xbegin = roi.xbegin;
        m_rng_xend   = roi.xend;
        m_rng_ybegin = roi.ybegin;
        m_rng_yend   = roi.yend;
        m_rng_zbegin = roi.zbegin;
        m_rng_zend   = roi.zend;
    } else {
        range_is_image();
    }
    pos(m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
    if (m_rng_xbegin == m_rng_xend || m_rng_ybegin == m_rng_yend
        || m_rng_zbegin == m_rng_zend)
        pos_done();  // make empty range look "done"
}


ImageBuf::IteratorBase::IteratorBase(const ImageBuf& ib, int xbegin, int xend,
                                     int ybegin, int yend, int zbegin, int zend,
                                     WrapMode wrap, bool write)
    : m_ib(&ib)
{
    init_ib(wrap, write);
    m_rng_xbegin = xbegin;
    m_rng_xend   = xend;
    m_rng_ybegin = ybegin;
    m_rng_yend   = yend;
    m_rng_zbegin = zbegin;
    m_rng_zend   = zend;
    pos(m_rng_xbegin, m_rng_ybegin, m_rng_zbegin);
    if (m_rng_xbegin == m_rng_xend || m_rng_ybegin == m_rng_yend
        || m_rng_zbegin == m_rng_zend)
        pos_done();  // make empty range look "done"
}



ImageBuf::IteratorBase::IteratorBase(const IteratorBase& i)
    : m_ib(i.m_ib)
    , m_rng_xbegin(i.m_rng_xbegin)
    , m_rng_xend(i.m_rng_xend)
    , m_rng_ybegin(i.m_rng_ybegin)
    , m_rng_yend(i.m_rng_yend)
    , m_rng_zbegin(i.m_rng_zbegin)
    , m_rng_zend(i.m_rng_zend)
    , m_proxydata(i.m_proxydata)
{
    init_ib(i.m_wrap, false);
    pos(i.m_x, i.m_y, i.m_z);
}



inline void
ImageBuf::IteratorBase::pos_done()
{
    m_valid = false;
    m_x     = m_rng_xbegin;
    m_y     = m_rng_ybegin;
    m_z     = m_rng_zend;
}



inline void
ImageBuf::IteratorBase::range_is_image()
{
    m_rng_xbegin = m_img_xbegin;
    m_rng_xend   = m_img_xend;
    m_rng_ybegin = m_img_ybegin;
    m_rng_yend   = m_img_yend;
    m_rng_zbegin = m_img_zbegin;
    m_rng_zend   = m_img_zend;
}



void
ImageBuf::IteratorBase::make_writable()
{
    std::lock_guard<const ImageBuf> lock(*m_ib);
    if (m_ib->storage() != IMAGECACHE)
        return;  // already done
    const_cast<ImageBuf*>(m_ib)->make_writable(true);
    OIIO_DASSERT(m_ib->storage() != IMAGECACHE);
    if (m_tile)
        release_tile();
    m_tile        = nullptr;
    m_proxydata   = nullptr;
    m_localpixels = !m_deep;
    pos(m_x, m_y, m_z);
}



void
ImageBuf::IteratorBase::init_ib(WrapMode wrap, bool write)
{
    ImageBufImpl::lock_t lock(m_ib->m_impl->m_mutex);
    const ImageSpec& spec(m_ib->spec());
    m_deep        = spec.deep;
    m_localpixels = (m_ib->localpixels() != nullptr);
    // if (write)
    //      ensure_writable();  // Not here; do it lazily
    m_img_xbegin   = spec.x;
    m_img_xend     = spec.x + spec.width;
    m_img_ybegin   = spec.y;
    m_img_yend     = spec.y + spec.height;
    m_img_zbegin   = spec.z;
    m_img_zend     = spec.z + spec.depth;
    m_nchannels    = spec.nchannels;
    m_pixel_stride = m_ib->pixel_stride();
    m_x            = 1 << 31;
    m_y            = 1 << 31;
    m_z            = 1 << 31;
    m_wrap         = (wrap == WrapDefault ? WrapBlack : wrap);
    m_pixeltype    = spec.format.basetype;
}



const ImageBuf::IteratorBase&
ImageBuf::IteratorBase::operator=(const IteratorBase& i)
{
    if (m_tile)
        release_tile();
    m_tile      = nullptr;
    m_proxydata = i.m_proxydata;
    m_ib        = i.m_ib;
    init_ib(i.m_wrap, false);
    m_rng_xbegin = i.m_rng_xbegin;
    m_rng_xend   = i.m_rng_xend;
    m_rng_ybegin = i.m_rng_ybegin;
    m_rng_yend   = i.m_rng_yend;
    m_rng_zbegin = i.m_rng_zbegin;
    m_rng_zend   = i.m_rng_zend;
    m_x          = i.m_x;
    m_y          = i.m_y;
    m_z          = i.m_z;
    return *this;
}



void
ImageBuf::IteratorBase::pos(int x_, int y_, int z_)
{
    if (x_ == m_x + 1 && x_ < m_rng_xend && y_ == m_y && z_ == m_z && m_valid
        && m_exists) {
        // Special case for what is in effect just incrementing x
        // within the iteration region.
        m_x = x_;
        pos_xincr();
        // Not necessary? m_exists = (x_ < m_img_xend);
        OIIO_DASSERT((x_ < m_img_xend) == m_exists);
        return;
    }
    bool v = valid(x_, y_, z_);
    bool e = exists(x_, y_, z_);
    if (m_localpixels) {
        if (e)
            m_proxydata = (char*)m_ib->pixeladdr(x_, y_, z_);
        else {  // pixel not in data window
            m_x = x_;
            m_y = y_;
            m_z = z_;
            if (m_wrap == WrapBlack) {
                m_proxydata = (char*)m_ib->blackpixel();
            } else {
                if (m_ib->do_wrap(x_, y_, z_, m_wrap))
                    m_proxydata = (char*)m_ib->pixeladdr(x_, y_, z_);
                else
                    m_proxydata = (char*)m_ib->blackpixel();
            }
            m_valid  = v;
            m_exists = e;
            return;
        }
    } else if (!m_deep)
        m_proxydata = (char*)m_ib->retile(x_, y_, z_, m_tile, m_tilexbegin,
                                          m_tileybegin, m_tilezbegin,
                                          m_tilexend, m_readerror, e, m_wrap);
    m_x      = x_;
    m_y      = y_;
    m_z      = z_;
    m_valid  = v;
    m_exists = e;
}



void
ImageBuf::IteratorBase::pos_xincr_local_past_end()
{
    m_exists = false;
    if (m_wrap == WrapBlack) {
        m_proxydata = (char*)m_ib->blackpixel();
    } else {
        int x = m_x, y = m_y, z = m_z;
        if (m_ib->do_wrap(x, y, z, m_wrap))
            m_proxydata = (char*)m_ib->pixeladdr(x, y, z);
        else
            m_proxydata = (char*)m_ib->blackpixel();
    }
}



void
ImageBuf::IteratorBase::rerange(int xbegin, int xend, int ybegin, int yend,
                                int zbegin, int zend, WrapMode wrap)
{
    m_x          = 1 << 31;
    m_y          = 1 << 31;
    m_z          = 1 << 31;
    m_wrap       = (wrap == WrapDefault ? WrapBlack : wrap);
    m_rng_xbegin = xbegin;
    m_rng_xend   = xend;
    m_rng_ybegin = ybegin;
    m_rng_yend   = yend;
    m_rng_zbegin = zbegin;
    m_rng_zend   = zend;
    pos(xbegin, ybegin, zbegin);
}



void
ImageBuf::lock() const
{
    m_impl->lock();
}


void
ImageBuf::unlock() const
{
    m_impl->unlock();
}


OIIO_NAMESPACE_END

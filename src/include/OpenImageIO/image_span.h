// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#pragma once

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/strided_ptr.h>


OIIO_NAMESPACE_BEGIN

#ifndef OIIO_STRIDE_T_DEFINED
#    define OIIO_STRIDE_T_DEFINED
/// Type we use to express how many pixels (or bytes) constitute an image,
/// tile, or scanline.
using imagesize_t = uint64_t;

/// Type we use for stride lengths between pixels, scanlines, or image
/// planes.
using stride_t = int64_t;

/// Special value to indicate a stride length that should be
/// auto-computed.
inline constexpr stride_t AutoStride = std::numeric_limits<stride_t>::min();
#endif



/// image_span : a non-owning reference to an image-like n-D array having
/// between 2 and 4 dimensions representing channel, x, y, z with each
/// dimension having known size and optionally non-default strides (expressed
/// in bytes) through the data.  An `image_span<T>` is mutable (the values in
/// the image may be modified), whereas an `image_span<const T>` is not
/// mutable.
///
/// Note that the optional template parameter `Rank` includes the channels as
/// the first dimension.  When no Rank template parameter is provided, it
/// defaults to 4, meaning it's an image span that can describe any choice of
/// a scanline (Rank 2), 2D image (Rank 3), or volume (Rank 4).
template<typename T, size_t Rank = 4> class image_span {
    static_assert(Rank >= 2 && Rank <= 4, "Rank must be between 2 and 4");

public:
    using value_type      = T;
    using reference       = T&;
    using const_reference = const T&;
    using stride_t        = int64_t;
    using stride_type     = int64_t;
    using size_type       = uint32_t;

    /// Default ctr -- points to nothing
    image_span() = default;

    /// Copy constructor
    image_span(const image_span& copy) = default;

    /// Construct from T*, dimensions, and (possibly default) strides (in
    /// bytes).
    image_span(T* data, uint32_t nchannels, uint32_t width, uint32_t height,
               uint32_t depth = 1, stride_t chanstride = AutoStride,
               stride_t xstride = AutoStride, stride_t ystride = AutoStride,
               stride_t zstride = AutoStride, uint32_t chansize = sizeof(T))
        : m_data(data)
        , m_chansize(chansize)
    {
        // Validations:
        // - an image_span<byte> can have any chansize, but any other T must
        //   have the chansize equal to the data type size.
        OIIO_DASSERT((std::is_same<std::remove_const_t<T>, std::byte>::value)
                     || chansize == sizeof(T));

        m_sizes[0] = nchannels;
        m_sizes[1] = width;
        if constexpr (Rank >= 3)
            m_sizes[2] = height;
        if constexpr (Rank >= 4)
            m_sizes[3] = depth;

        chanstride   = chanstride != AutoStride ? chanstride : chansize;
        xstride      = xstride != AutoStride ? xstride : nchannels * chanstride;
        m_strides[0] = chanstride;
        m_strides[1] = xstride;
        if constexpr (Rank >= 3) {
            ystride      = ystride != AutoStride ? ystride : width * xstride;
            m_strides[2] = ystride;
        }
        if constexpr (Rank >= 4) {
            zstride      = zstride != AutoStride ? zstride : height * ystride;
            m_strides[3] = zstride;
        }
    }

    // clang-format off
    /* clang_format gets confused by this */

    /// Copy constructor from `image_span<T>` to `image_span<const T>`.
    template<typename U, size_t R,
             OIIO_ENABLE_IF((std::is_same_v<std::remove_const_t<T>, U>)
                            && std::is_const_v<T> && !std::is_const_v<U>
                            && R <= Rank)>
    image_span(const image_span<U, R>& other)
        : image_span(other.data(), other.nchannels(), other.width(),
                     other.height(), other.depth(), other.chanstride(),
                     other.xstride(), other.ystride(), other.zstride(),
                     other.chansize())
    {
    }
    // clang-format on

    /// Construct from `span<T>` and dimensions, assume contiguous strides.
    image_span(span<T> data, uint32_t nchannels, uint32_t width,
               uint32_t height, uint32_t depth = 1)
        : image_span(data.data(), nchannels, width, height, depth)
    {
        // Validations:
        // - The full layout must fit within the original span size.
        OIIO_DASSERT(nvalues() <= data.size());
    }

    /// assignments -- not a deep copy, just make this image_span point to the
    /// same strided data as the operand.
    image_span& operator=(const image_span& copy) = default;

    /// image_span(x,y,z) returns a `strided_ptr<T,1>` for the pixel (x,y,z).
    /// The z can be omitted for 2D images.  Note that the resulting
    /// strided_ptr can then have individual channels accessed with
    /// operator[]. This particular strided pointer has stride multiplier 1,
    /// because this class uses bytes as strides, not sizeof(T).
    strided_ptr<T, 1> operator()(uint32_t x, uint32_t y, uint32_t z = 0)
    {
        return strided_ptr<T, 1>(getptr(0, x, y, z), chanstride());
    }

    /// Return the number of dimensions of the image_span. Remember that a
    /// Rank 2 image_span is one scanline, a Rank 3 image_span can be a 2D
    /// image, and a Rank 4 image_span can be a volume.
    static constexpr size_t rank() noexcept { return Rank; }

    /// Return the number of channels.
    constexpr size_type nchannels() const { return m_sizes[0]; }
    /// Return the stride, in bytes, between channels within a pixel.
    constexpr stride_type chanstride() const { return m_strides[0]; }

    /// Return the width -- the number of pixels in the x dimension.
    constexpr size_type width() const { return m_sizes[1]; }
    /// Return the stride, in bytes, between pixels in the x dimension.
    constexpr stride_type xstride() const { return m_strides[1]; }

    /// Return the height -- the number of pixels in the y dimension. This
    /// will be 1 for a Rank 2 (single scanline) `image_span<T,2>`.
    constexpr size_type height() const
    {
        if constexpr (Rank >= 3)
            return m_sizes[2];
        else
            return 1;
    }
    /// Return the stride, in bytes, between pixels in the y dimension. This
    /// will be 0 for a Rank 2 (single scanline) `image_span<T,2>`.
    constexpr stride_type ystride() const
    {
        if constexpr (Rank >= 3)
            return m_strides[2];
        else
            return 0;
    }

    /// Return the depth -- the number of pixels in the z dimension. This will
    /// be 1 if there are fewer than 3 spatial dimensions (i.e. a scanline
    /// `image_span<T,2>` or 2D image `image_span<T,3>`).
    constexpr size_type depth() const
    {
        if constexpr (Rank >= 4)
            return m_sizes[3];
        return 1;
    }
    /// Return the stride, in bytes, between pixels in the z dimension. This
    /// will be 0 if there are fewer than 3 spatial dimensions (i.e. a
    /// scanline `image_span<T,2>` or 2D image `image_span<T,3>`).
    constexpr stride_type zstride() const
    {
        if constexpr (Rank >= 4)
            return m_strides[3];
        return 0;
    }

    /// Return the size of a channel, in bytes. For all element types except
    /// for std::byte, this should always be the same as sizeof(T). For
    /// std::byte, this may be different if the channels are another data type
    /// but for the sake of this span, we are treating it as untyped memory.
    /// The channel size is set by the constructor.
    constexpr size_type chansize() const { return m_chansize; }

    /// Return a raw pointer to the start of the data: channel=0, x=0, y=0,
    /// z=0.
    T* data() const { return m_data; }

    /// Convert an `image_span<T>` to an `image_span<const std::byte>`
    /// representing the same sized and strided memory pattern represented
    /// un-typed memory.
    image_span<const std::byte> as_bytes_image_span() const noexcept
    {
        return image_span<const std::byte>(reinterpret_cast<const std::byte*>(
                                               m_data),
                                           nchannels(), width(), height(),
                                           depth(), chanstride(), xstride(),
                                           ystride(), zstride(), m_chansize);
    }

    /// Convert an `image_span<T>` to an image_span<std::byte> representing
    /// the same sized and strided memory pattern represented un-typed memory.
    /// Note that this will not work (be a compiler error) if T a const type.
    image_span<std::byte> as_writable_bytes_image_span() const noexcept
    {
        return image_span<std::byte>(reinterpret_cast<std::byte*>(m_data),
                                     nchannels(), width(), height(), depth(),
                                     chanstride(), xstride(), ystride(),
                                     zstride(), m_chansize);
    }

    /// Does this image_span represent contiguous pixels -- i.e. within each
    /// pixel, the channels directly follow each other in memory?
    constexpr bool is_contiguous_pixel() const noexcept
    {
        return chanstride() == m_chansize;
    }

    /// Does this image_span represent contiguous scanlines -- i.e. channels
    /// contiguous within each pixel and pixels contiguous within each
    /// scanline?
    constexpr bool is_contiguous_scanline() const noexcept
    {
        return is_contiguous_pixel() && xstride() == chanstride() * nchannels();
    }

    /// Does this image_span represent contiguous 2D image planes -- i.e.
    /// channels contiguous within each pixel, pixels contiguous within each
    /// scanline, scanlines contiguous within each 2D image plane?
    constexpr bool is_contiguous_plane() const noexcept
    {
        return is_contiguous_scanline()
               && (Rank < 3 || ystride() == xstride() * width());
    }

    /// Does this image_span represent fully contiguous data in all
    /// dimensions, i.e., each channel, pixel, scanline, and image plane
    /// directly abuts its neighbour, with no gaps?
    constexpr bool is_contiguous() const noexcept
    {
        return is_contiguous_scanline()
               /* image plane is contiguous scanlines */
               && (Rank < 3 || ystride() == xstride() * width())
               /* volume is contiguous planes */
               && (Rank < 4 || zstride() == ystride() * height());
    }

    /// Return the total number of pixels in the span: `w * h * d`.
    constexpr size_t npixels() const
    {
        return size_t(width()) * size_t(height()) * size_t(depth());
    }

    /// Return the total number of values in the span: `c * w * h * d`.
    constexpr size_t nvalues() const
    {
        return size_t(nchannels()) * size_t(width()) * size_t(height())
               * size_t(depth());
    }

    /// Return the total number of bytes of (c*w*h*d) values of the given type
    /// (but not counting space in any gaps).
    constexpr size_t size_bytes() const { return nvalues() * chansize(); }

    /// Return a reference to the value at channel c, pixel (x,y,z).
    inline T& get(int c, int x, int y = 0, int z = 0) const
    {
        // Bounds check in done in getptr
        return *getptr(c, x, y, z);
    }

    /// Return a pointer to the value at channel c, pixel (x,y,z).
    inline T* getptr(int c, int x, int y, int z = 0) const
    {
        // Bounds check in debug mode
        OIIO_DASSERT(unsigned(c) < unsigned(nchannels())
                     && unsigned(x) < unsigned(width())
                     && unsigned(y) < unsigned(height())
                     && unsigned(z) < unsigned(depth()));
        if constexpr (Rank == 2) {
            OIIO_DASSERT(y == 0 && z == 0);
        } else if constexpr (Rank == 3) {
            OIIO_DASSERT(z == 0);
        }
        return (T*)((char*)data() + c * chanstride() + x * xstride()
                    + y * ystride() + z * zstride());
    }

    /// Return a pointer to the value at channel 0, pixel (x,y,z).
    inline T* getpixelptr(int x, int y = 0, int z = 0) const
    {
        return getptr(0, x, y, z);
    }

    /// Return a subspan in x, y, and z (but assume all channels are
    /// included).
    image_span subspan(uint32_t xbegin, uint32_t xend, uint32_t ybegin,
                       uint32_t yend, uint32_t zbegin = 0,
                       uint32_t zend = 1) const
    {
        // Bounds check in debug mode
        OIIO_DASSERT(xbegin <= xend && xend <= width() && ybegin <= yend
                     && yend <= height() && zbegin <= zend && zend <= depth());
        return image_span(data() + xbegin * xstride() + ybegin * ystride()
                              + zbegin * zstride(),
                          nchannels(), xend - xbegin, yend - ybegin,
                          zend - zbegin, chanstride(), xstride(), ystride(),
                          zstride(), chansize());
    }

    /// Return a subspan in all dimensions: channel, x, y, and z.
    image_span chansubspan(uint32_t chbegin, uint32_t chend, uint32_t xbegin,
                           uint32_t xend, uint32_t ybegin, uint32_t yend,
                           uint32_t zbegin = 0, uint32_t zend = 1) const
    {
        // Bounds check in debug mode
        OIIO_DASSERT(chbegin <= chend && chend <= nchannels() && xbegin <= xend
                     && xend <= width() && ybegin <= yend && yend <= height()
                     && zbegin <= zend && zend <= depth());
        return image_span(data() + chbegin * chanstride() + xbegin * xstride()
                              + ybegin * ystride() + zbegin * zstride(),
                          chend - chbegin, xend - xbegin, yend - ybegin,
                          zend - zbegin, chanstride(), xstride(), ystride(),
                          zstride(), chansize());
    }

private:
    T* m_data { nullptr };  // pointer to the start of the data
    std::array<stride_type, Rank> m_strides;  // byte strides for each dim
    std::array<size_type, Rank> m_sizes;      // size for each dim
    uint32_t m_chansize { sizeof(T) };  // size of a channel value, in bytes
};



/// Type alias for an image_span that can describe a 3D volumetric image with
/// channels.
template<typename T> using image3d_span = image_span<T, 4>;

/// Type alias for an image_span that can describe a 2D image with channels,
/// but cannot describe a 3D volume.
template<typename T> using image2d_span = image_span<T, 3>;

/// Type alias for an image_span that can describe a single scanline with
/// channels, but cannot describe a full 2D image or a 3D volume.
template<typename T> using image1d_span = image_span<T, 2>;



/// Convert an image_span of any type to a non-mutable span of const bytes
/// covering the same range of memory.
template<typename T, size_t Rank>
image_span<const std::byte>
as_image_span_bytes(const image_span<T, Rank>& src) noexcept
{
    return image_span<const std::byte>(
        reinterpret_cast<const std::byte*>(src.data()), src.nchannels(),
        src.width(), src.height(), src.depth(), src.chanstride(), src.xstride(),
        src.ystride(), src.zstride(), src.chansize());
}


/// Convert an image_span of any nonconst type to a mutable span of bytes
/// covering the same range of memory.
template<typename T, size_t Rank>
image_span<std::byte>
as_image_span_writable_bytes(const image_span<T, Rank>& src) noexcept
{
    return image_span<std::byte>(reinterpret_cast<std::byte*>(src.data()),
                                 src.nchannels(), src.width(), src.height(),
                                 src.depth(), src.chanstride(), src.xstride(),
                                 src.ystride(), src.zstride(), src.chansize());
}

/// Verify that the image_span has all its contents lying within the
/// contiguous span.
OIIO_API bool
image_span_within_span(const image_span<const std::byte>& ispan,
                       span<const std::byte> contiguous) noexcept;

/// image_span_within_span() for generic span types. Just reduce to
/// const byte versions.
template<typename T, size_t Trank, typename S>
bool
image_span_within_span(const image_span<T, Trank>& ispan,
                       span<S> contiguous) noexcept
{
    return image_span_within_span(as_image_span_bytes(ispan),
                                  as_bytes(contiguous));
}

OIIO_NAMESPACE_END

// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/// \file
/// Declarations for things that are used privately by OpenImageIO.


#ifndef OPENIMAGEIO_IMAGEIO_PVT_H
#define OPENIMAGEIO_IMAGEIO_PVT_H

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/timer.h>



OIIO_NAMESPACE_BEGIN
// Note: Everything in pvt namespace is expected to be local to the library
// and does not appear in exported headers that client software will see.
// Therefore, it should all stay in the current namespace except where
// specifically noted.

namespace pvt {

// Mutex allowing thread safety of the ImageIO internals below
extern std::recursive_mutex imageio_mutex;

extern atomic_int oiio_threads;
extern atomic_int oiio_read_chunk;
extern atomic_int oiio_try_all_readers;
extern ustring font_searchpath;
extern ustring plugin_searchpath;
extern std::string format_list;
extern std::string input_format_list;
extern std::string output_format_list;
extern std::string extension_list;
extern std::string library_list;
extern OIIO_UTIL_API int oiio_print_debug;
extern OIIO_UTIL_API int oiio_print_uncaught_errors;
extern int oiio_log_times;
extern int openexr_core;
extern int jpeg_com_attributes;
extern int png_linear_premult;
extern int limit_channels;
extern int limit_imagesize_MB;
extern int imagebuf_print_uncaught_errors;
extern int imagebuf_use_imagecache;
extern int imageinput_strict;
extern atomic_ll IB_local_mem_current;
extern atomic_ll IB_local_mem_peak;
extern std::atomic<float> IB_total_open_time;
extern std::atomic<float> IB_total_image_read_time;
extern OIIO_UTIL_API int oiio_use_tbb;  // This lives in libOpenImageIO_Util
OIIO_API const std::vector<std::string>&
font_dirs();
OIIO_API const std::vector<std::string>&
font_file_list();
OIIO_API const std::vector<std::string>&
font_list();
OIIO_API const std::vector<std::string>&
font_family_list();
OIIO_API const std::vector<std::string>
font_style_list(string_view family);
OIIO_API const std::string
font_filename(string_view family, string_view style = "");


// Make sure all plugins are inventoried. For internal use only.
void
catalog_all_plugins(std::string searchpath);

// Inexpensive check if a file extension or format name corresponds to a
// procedural input plugin.
bool
is_procedural_plugin(const std::string& name);

/// Given the format, set the default quantization range.
void
get_default_quantize(TypeDesc format, long long& quant_min,
                     long long& quant_max) noexcept;

/// Turn potentially non-contiguous-stride data (e.g. "RGBxRGBx") into
/// contiguous-stride ("RGBRGB"), for any format or stride values (measured in
/// bytes).  Caller must pass in a dst pointing to enough memory to hold the
/// contiguous rectangle.  Return span where the contiguous data ended up,
/// which is either dst or src (if the strides indicated that data were
/// already contiguous).
OIIO_API span<const std::byte>
contiguize(const image_span<const std::byte>& src, span<std::byte> dst);

/// Turn potentially non-contiguous-stride data (e.g. "RGBxRGBx") into
/// contiguous-stride ("RGBRGB"), for any format or stride values
/// (measured in bytes).  Caller must pass in a dst pointing to enough
/// memory to hold the contiguous rectangle.  Return a ptr to where the
/// contiguous data ended up, which is either dst or src (if the strides
/// indicated that data were already contiguous).
OIIO_API const void*
contiguize(const void* src, int nchannels, stride_t xstride, stride_t ystride,
           stride_t zstride, void* dst, int width, int height, int depth,
           TypeDesc format);

/// Turn contiguous data from any format into float data.  Return a
/// pointer to the converted data (which may still point to src if no
/// conversion was necessary).
const float*
convert_to_float(const void* src, float* dst, int nvals, TypeDesc format);

/// Turn contiguous float data into any format.  Return a pointer to the
/// converted data (which may still point to src if no conversion was
/// necessary).
const void*
convert_from_float(const float* src, void* dst, size_t nvals, TypeDesc format);

/// A version of convert_from_float that will break up big jobs with
/// multiple threads.
const void*
parallel_convert_from_float(const float* src, void* dst, size_t nvals,
                            TypeDesc format);

/// Internal utility: Error checking on the spec -- if it contains texture-
/// specific metadata but there are clues it's not actually a texture file
/// written by maketx or `oiiotool -otex`, then assume these metadata are
/// wrong and delete them. Return true if we think it's one of these
/// incorrect files and it was fixed.
OIIO_API bool
check_texture_metadata_sanity(ImageSpec& spec);

/// Set oiio:ColorSpace to the default sRGB colorspace, which may be display
/// or scene referred depending on configuration.
///
/// If image_state_default is "scene" or "display", a colorspace with that
/// image state will be preferred over otherwise equivalent color spaces.
///
/// If erase_other_attributes is true, other potentially conflicting attributes
/// are erased.
OIIO_API void
set_colorspace_srgb(ImageSpec& spec,
                    string_view image_state_default = "display",
                    bool erase_other_attributes     = true);

/// Set oiio:ColorSpace in the spec's metadata based on the gamma value.
///
/// If image_state_default is "scene" or "display" a colrospace with that
/// image state will be prefered over otherwise equivalent color spaces.
OIIO_API void
set_colorspace_rec709_gamma(ImageSpec& spec, float gamma,
                            string_view image_state_default = "display");

// Set CICP attribute in the spec's metadata, and set oiio:ColorSpace
// along with it if there is a corresponding known colorspace.
//
/// If image_state_default is "scene" or "display" a colrospace with that
/// image state will be prefered over otherwise equivalent color spaces.
OIIO_API void
set_colorspace_cicp(ImageSpec& spec, const int cicp[4],
                    string_view image_state_default = "display");

/// Returns true if for the purpose of interop, the spec's metadata
/// specifies a color space that should be encoded as sRGB.
///
/// If default_to_srgb is true, the colorspace will be assumed to
/// be sRGB if no colorspace was specified in the spec.
OIIO_API bool
is_colorspace_srgb(const ImageSpec& spec, bool default_to_srgb = true);

/// If the spec's metadata specifies a color space with Rec709 primaries and
/// gamma transfer function, return the gamma value. If not, return zero.
OIIO_API float
get_colorspace_rec709_gamma(const ImageSpec& spec);

// Returns ICC profile from the spec's metadata, either from an ICCProfile
// attribute or from the colorspace if from_colorspace is true.
// Returns an empty vector if not found.
OIIO_API std::vector<uint8_t>
get_colorspace_icc_profile(const ImageSpec& spec, bool from_colorspace = true);

// Returns CICP from the spec's metadata, either from a CICP attribute
// or from the colorspace if from_colorspace is true.
// Returns an empty span if not found.
OIIO_API cspan<int>
get_colorspace_cicp(const ImageSpec& spec, bool from_colorspace = true);

/// Get the timing report from log_time entries.
OIIO_API std::string
timing_report();

/// An object that, if oiio_log_times is nonzero, logs time until its
/// destruction. If oiio_log_times is 0, it does nothing.
class LoggedTimer {
public:
    LoggedTimer(string_view name)
        : m_timer(oiio_log_times)
    {
        if (oiio_log_times)
            m_name = name;
    }
    ~LoggedTimer()
    {
        if (oiio_log_times)
            log_time(m_name, m_timer, m_count);
    }
    // Stop the timer. An optional count_offset will be added to the
    // "invocations count" of the underlying timer, if a single invocation
    // does not correctly describe the thing being timed.
    void stop(int count_offset = 0)
    {
        m_timer.stop();
        m_count += count_offset;
    }
    void start() { m_timer.start(); }
    void rename(string_view name) { m_name = name; }

private:
    Timer m_timer;
    std::string m_name;
    int m_count = 1;
};


// Access to an internal periodic blue noise table.
inline constexpr int bntable_res = 256;
extern float bluenoise_table[bntable_res][bntable_res][4];

// 1-channel value lookup of periodic blue noise of a 2D coordinate.
inline float
bluenoise_1chan(int x, int y)
{
    x &= bntable_res - 1;
    y &= bntable_res - 1;
    return bluenoise_table[y][x][0];
}

// 4-channel pointer lookup of periodic blue noise of a 2D coordinate.
inline const float*
bluenoise_4chan_ptr(int x, int y)
{
    x &= bntable_res - 1;
    y &= bntable_res - 1;
    return bluenoise_table[y][x];
}

// 4-channel pointer lookup of periodic blue noise of 3D coordinate + seed +
// channel channel number. The pointer is to the 4 floats of the mod 4 group
// of channels, i.e. if ch=5, the pointer will be to the 4 floats representing
// channels 4..7.
inline const float*
bluenoise_4chan_ptr(int x, int y, int z, int ch = 0, int seed = 0)
{
    if (z | (ch & ~3) | seed) {
        x += bjhash::bjfinal(z, ch, seed);
        y += bjhash::bjfinal(z, ch, seed + 83533);
    }
    x &= bntable_res - 1;
    y &= bntable_res - 1;
    return bluenoise_table[y][x];
}



struct print_info_options {
    bool verbose            = false;
    bool filenameprefix     = false;
    bool sum                = false;
    bool subimages          = false;
    bool compute_sha1       = false;
    bool compute_stats      = false;
    bool dumpdata           = false;
    bool dumpdata_showempty = true;
    bool dumpdata_C         = false;
    bool native             = false;
    std::string dumpdata_C_name;
    std::string metamatch;
    std::string nometamatch;
    std::string infoformat;
    size_t namefieldlength = 20;
    ROI roi;
};

OIIO_API std::string
compute_sha1(ImageInput* input, int subimage, int miplevel, std::string& err);
OIIO_API bool
print_stats(std::ostream& out, string_view indent, const ImageBuf& input,
            const ImageSpec& spec, ROI roi, std::string& err);



enum class ComputeDevice : int {
    CPU  = 0,
    CUDA = 1,
    // Might expand later...
};

// Which compute device is currently active, and should be used by any
// OIIO facilities that know how to use it.
OIIO_API ComputeDevice
compute_device();

#if 0
/// Return true if CUDA is available to OpenImageIO at this time -- support
/// enabled at build time, and has already been turned on with enable_cuda()
/// or with OIIO::attribute("cuda", 1), and hardware is present and was
/// successfully initialized.
OIIO_API bool
openimageio_cuda();
#endif

// Set an attribute related to OIIO's use of GPUs/compute devices. This is a
// strictly internal function. User code should just call OIIO::attribute()
// and GPU-related attributes will be directed here automatically.
OIIO_API bool
gpu_attribute(string_view name, TypeDesc type, const void* val);

// Retrieve an attribute related to OIIO's use of GPUs/compute devices. This
// is a strictly internal function. User code should just call
// OIIO::getattribute() and GPU-related attributes will be directed here
// automatically.
OIIO_API bool
gpu_getattribute(string_view name, TypeDesc type, void* val);


/// Allocate compute device memory
OIIO_API void*
device_malloc(size_t size);

/// Allocate unified compute device memory -- visible on both CPU & GPU
OIIO_API void*
device_unified_malloc(size_t size);

/// Free compute device memory
OIIO_API void
device_free(void* mem);

}  // namespace pvt



/// Allocator adaptor that interposes construct() calls to convert value
/// initialization into default initialization.
///
/// This is a way to achieve a std::vector whose resize does not force a value
/// initialization of every allocated element. Put in more plain terms, the
/// following:
///
///     std::vector<int> v(Nlarge);
///
/// will zero-initialize the Nlarge elements, which may be a cost we do not
/// wish to pay, particularly when allocating POD types that we are going to
/// write over anyway. Sometimes we do the following instead:
///
///     std::unique_ptr<int[]> v (new int[Nlarge]);
///
/// which does not zero-initialize the elements. But it's more awkward, and
/// lacks the methods you get automatically with vectors.
///
/// But you will get a lack of forced value initialization if you use a
/// std::vector with a special allocator that does default initialization.
/// This is such an allocator, so the following:
///
///    std::vector<T, default_init_allocator<T>> v(Nlarge);
///
/// will have the same performance characteristics as the new[] version.
///
/// For details:
/// https://stackoverflow.com/questions/21028299/is-this-behavior-of-vectorresizesize-type-n-under-c11-and-boost-container/21028912#21028912
///
template<typename T, typename A = std::allocator<T>>
class default_init_allocator : public A {
    typedef std::allocator_traits<A> a_t;

public:
    template<typename U> struct rebind {
        using other
            = default_init_allocator<U, typename a_t::template rebind_alloc<U>>;
    };

    using A::A;

    template<typename U>
    void
    construct(U* ptr) noexcept(std::is_nothrow_default_constructible<U>::value)
    {
        ::new (static_cast<void*>(ptr)) U;
    }
    template<typename U, typename... Args>
    void construct(U* ptr, Args&&... args)
    {
        a_t::construct(static_cast<A&>(*this), ptr,
                       std::forward<Args>(args)...);
    }
};


/// Type alias for a std::vector that uses the default_init_allocator.
///
/// Consider using a `default_init_vector<T>` instead of `std::vector<T>` when
/// all of the following are true:
///
/// * The use is entirely internal to OIIO (since at present, this type is
///   not defined in any public header files).
/// * The type T is POD (plain old data) or trivially constructible.
/// * The vector is likely to be large enough that the cost of default
///   initialization is worth trying to avoid.
/// * After allocation, the vector will be filled with data before any reads
///   are attempted, so the default initialization is not needed.
///
template<typename T>
using default_init_vector = std::vector<T, default_init_allocator<T>>;



OIIO_NAMESPACE_END

#endif  // OPENIMAGEIO_IMAGEIO_PVT_H

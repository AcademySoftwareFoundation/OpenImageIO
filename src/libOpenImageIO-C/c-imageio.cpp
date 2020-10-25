// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#include <limits>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/deepdata.h>

#include <OpenImageIO/c-imageio.h>
#include <OpenImageIO/c-typedesc.h>

#include "util.h"

using OIIO::bit_cast;
using OIIO::Strutil::safe_strcpy;

namespace {

// Some small utility functions to make casting between the C++ and C types
// less obtuse
DEFINE_POINTER_CASTS(ImageSpec)
DEFINE_POINTER_CASTS(ImageInput)
DEFINE_POINTER_CASTS(ImageOutput)
DEFINE_POINTER_CASTS(ParamValue)
DEFINE_POINTER_CASTS(DeepData)

#undef DEFINE_POINTER_CASTS

}  // namespace

extern "C" {

stride_t OIIO_AutoStride = std::numeric_limits<stride_t>::min();

// Check ROI is bit-equivalent
OIIO_STATIC_ASSERT(sizeof(OIIO_ROI) == sizeof(OIIO::ROI));
OIIO_STATIC_ASSERT(alignof(OIIO_ROI) == alignof(OIIO::ROI));
OIIO_STATIC_ASSERT(offsetof(OIIO_ROI, xbegin) == offsetof(OIIO::ROI, xbegin));
OIIO_STATIC_ASSERT(offsetof(OIIO_ROI, xend) == offsetof(OIIO::ROI, xend));
OIIO_STATIC_ASSERT(offsetof(OIIO_ROI, ybegin) == offsetof(OIIO::ROI, ybegin));
OIIO_STATIC_ASSERT(offsetof(OIIO_ROI, yend) == offsetof(OIIO::ROI, yend));
OIIO_STATIC_ASSERT(offsetof(OIIO_ROI, zbegin) == offsetof(OIIO::ROI, zbegin));
OIIO_STATIC_ASSERT(offsetof(OIIO_ROI, zend) == offsetof(OIIO::ROI, zend));
OIIO_STATIC_ASSERT(offsetof(OIIO_ROI, chbegin) == offsetof(OIIO::ROI, chbegin));
OIIO_STATIC_ASSERT(offsetof(OIIO_ROI, chend) == offsetof(OIIO::ROI, chend));



OIIO_ROI
OIIO_ROI_All()
{
    return OIIO_ROI { std::numeric_limits<int>::min(), 0, 0, 0, 0, 0, 0, 0 };
}



bool
OIIO_ROI_defined(const OIIO_ROI* roi)
{
    return roi->xbegin != std::numeric_limits<int>::min();
}



int
OIIO_ROI_width(const OIIO_ROI* roi)
{
    return roi->xend - roi->xbegin;
}



int
OIIO_ROI_height(const OIIO_ROI* roi)
{
    return roi->yend - roi->ybegin;
}



int
OIIO_ROI_depth(const OIIO_ROI* roi)
{
    return roi->zend - roi->zbegin;
}



int
OIIO_ROI_nchannels(const OIIO_ROI* roi)
{
    return roi->chend - roi->chbegin;
}



imagesize_t
OIIO_ROI_npixels(const OIIO_ROI* roi)
{
    if (roi->xbegin != std::numeric_limits<int>::min()) {
        return OIIO_ROI_width(roi) * OIIO_ROI_height(roi) * OIIO_ROI_depth(roi);
    } else {
        return 0;
    }
}



bool
OIIO_ROI_contains(const OIIO_ROI* roi, int x, int y, int z, int ch)
{
    return x >= roi->xbegin && x < roi->xend && y >= roi->ybegin
           && y < roi->yend && z >= roi->zbegin && z < roi->zend
           && ch >= roi->chbegin && ch < roi->chend;
}



bool
OIIO_ROI_contains_roi(const OIIO_ROI* a, OIIO_ROI* b)
{
    return b->xbegin >= a->xbegin && b->xend <= a->xend
           && b->ybegin >= a->ybegin && b->yend <= a->yend
           && b->zbegin >= a->zbegin && b->zend <= a->zend
           && b->chbegin >= a->chbegin && b->chend <= a->chend;
}



OIIO_ImageSpec*
OIIO_ImageSpec_new()
{
    return to_c(new OIIO::ImageSpec);
}



void
OIIO_ImageSpec_delete(const OIIO_ImageSpec* is)
{
    delete to_cpp(is);
}



OIIO_ImageSpec*
OIIO_ImageSpec_new_with_dimensions(int xres, int yres, int nchans,
                                   OIIO_TypeDesc fmt)
{
    return to_c(
        new OIIO::ImageSpec(xres, yres, nchans,
                            bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(fmt)));
}



OIIO_ImageSpec*
OIIO_ImageSpec_copy(const OIIO_ImageSpec* ii)
{
    const OIIO::ImageSpec* oii = to_cpp(ii);
    return to_c(new OIIO::ImageSpec(*oii));
}



void
OIIO_ImageSpec_attribute(OIIO_ImageSpec* is, const char* name,
                         OIIO_TypeDesc fmt, const void* value)
{
    to_cpp(is)->attribute(name, bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(fmt),
                          value);
}



int
OIIO_ImageSpec_x(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->x;
}



void
OIIO_ImageSpec_set_x(OIIO_ImageSpec* is, int x)
{
    to_cpp(is)->x = x;
}



int
OIIO_ImageSpec_y(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->y;
}



void
OIIO_ImageSpec_set_y(OIIO_ImageSpec* is, int y)
{
    to_cpp(is)->y = y;
}



int
OIIO_ImageSpec_z(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->z;
}



void
OIIO_ImageSpec_set_z(OIIO_ImageSpec* is, int z)
{
    to_cpp(is)->z = z;
}



int
OIIO_ImageSpec_width(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->width;
}



void
OIIO_ImageSpec_set_width(OIIO_ImageSpec* is, int width)
{
    to_cpp(is)->width = width;
}



int
OIIO_ImageSpec_height(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->height;
}



void
OIIO_ImageSpec_set_height(OIIO_ImageSpec* is, int height)
{
    to_cpp(is)->height = height;
}



int
OIIO_ImageSpec_depth(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->depth;
}



void
OIIO_ImageSpec_set_depth(OIIO_ImageSpec* is, int depth)
{
    to_cpp(is)->depth = depth;
}



int
OIIO_ImageSpec_full_x(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->full_x;
}



void
OIIO_ImageSpec_set_full_x(OIIO_ImageSpec* is, int full_x)
{
    to_cpp(is)->full_x = full_x;
}



int
OIIO_ImageSpec_full_y(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->full_y;
}



void
OIIO_ImageSpec_set_full_y(OIIO_ImageSpec* is, int full_y)
{
    to_cpp(is)->full_y = full_y;
}



int
OIIO_ImageSpec_full_z(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->full_z;
}



void
OIIO_ImageSpec_set_full_z(OIIO_ImageSpec* is, int full_z)
{
    to_cpp(is)->full_z = full_z;
}



int
OIIO_ImageSpec_full_width(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->full_width;
}



void
OIIO_ImageSpec_set_full_width(OIIO_ImageSpec* is, int full_width)
{
    to_cpp(is)->full_width = full_width;
}



int
OIIO_ImageSpec_full_height(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->full_height;
}



void
OIIO_ImageSpec_set_full_height(OIIO_ImageSpec* is, int full_height)
{
    to_cpp(is)->full_height = full_height;
}



int
OIIO_ImageSpec_full_depth(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->full_depth;
}



void
OIIO_ImageSpec_set_full_depth(OIIO_ImageSpec* is, int full_depth)
{
    to_cpp(is)->full_depth = full_depth;
}



int
OIIO_ImageSpec_tile_width(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->tile_width;
}



void
OIIO_ImageSpec_set_tile_width(OIIO_ImageSpec* is, int tile_width)
{
    to_cpp(is)->tile_width = tile_width;
}



int
OIIO_ImageSpec_tile_height(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->tile_height;
}



void
OIIO_ImageSpec_set_tile_height(OIIO_ImageSpec* is, int tile_height)
{
    to_cpp(is)->tile_height = tile_height;
}



int
OIIO_ImageSpec_tile_depth(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->tile_depth;
}



void
OIIO_ImageSpec_set_tile_depth(OIIO_ImageSpec* is, int tile_depth)
{
    to_cpp(is)->tile_depth = tile_depth;
}



OIIO_TypeDesc
OIIO_ImageSpec_format(const OIIO_ImageSpec* is)
{
    return bit_cast<OIIO::TypeDesc, OIIO_TypeDesc>(to_cpp(is)->format);
}



void
OIIO_ImageSpec_set_format(OIIO_ImageSpec* is, OIIO_TypeDesc fmt)
{
    to_cpp(is)->set_format(bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(fmt));
}


int
OIIO_ImageSpec_alpha_channel(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->alpha_channel;
}



void
OIIO_ImageSpec_set_alpha_channel(OIIO_ImageSpec* is, int alpha_channel)
{
    to_cpp(is)->alpha_channel = alpha_channel;
}



int
OIIO_ImageSpec_z_channel(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->z_channel;
}



void
OIIO_ImageSpec_set_z_channel(OIIO_ImageSpec* is, int z_channel)
{
    to_cpp(is)->z_channel = z_channel;
}



bool
OIIO_ImageSpec_deep(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->deep;
}



void
OIIO_ImageSpec_set_deep(OIIO_ImageSpec* is, bool deep)
{
    to_cpp(is)->deep = deep;
}



void
OIIO_ImageSpec_default_channel_names(OIIO_ImageSpec* is)
{
    to_cpp(is)->default_channel_names();
}



size_t
OIIO_ImageSpec_channel_bytes(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->channel_bytes();
}



size_t
OIIO_ImageSpec_channel_bytes_at(const OIIO_ImageSpec* is, int chan, bool native)
{
    return to_cpp(is)->channel_bytes(chan, native);
}



size_t
OIIO_ImageSpec_pixel_bytes(const OIIO_ImageSpec* is, bool native)
{
    return to_cpp(is)->pixel_bytes(native);
}



size_t
OIIO_ImageSpec_pixel_bytes_for_channels(const OIIO_ImageSpec* is, int chbegin,
                                        int chend, bool native)
{
    return to_cpp(is)->pixel_bytes(chbegin, chend, native);
}



imagesize_t
OIIO_ImageSpec_scanline_bytes(const OIIO_ImageSpec* is, bool native)
{
    return to_cpp(is)->scanline_bytes(native);
}



imagesize_t
OIIO_ImageSpec_tile_pixels(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->tile_pixels();
}



imagesize_t
OIIO_ImageSpec_tile_bytes(const OIIO_ImageSpec* is, bool native)
{
    return to_cpp(is)->tile_bytes(native);
}



imagesize_t
OIIO_ImageSpec_image_pixels(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->image_pixels();
}



imagesize_t
OIIO_ImageSpec_image_bytes(const OIIO_ImageSpec* is, bool native)
{
    return to_cpp(is)->image_bytes(native);
}



void
OIIO_ImageSpec_auto_stride_xyz(stride_t* xstride, stride_t* ystride,
                               stride_t* zstride, OIIO_TypeDesc format,
                               int nchannels, int width, int height)
{
    OIIO::ImageSpec::auto_stride(*xstride, *ystride, *zstride,
                                 bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(format),
                                 nchannels, width, height);
}



OIIO_API void
OIIO_ImageSpec_auto_stride(stride_t* xstride, OIIO_TypeDesc format,
                           int nchannels)
{
    OIIO::ImageSpec::auto_stride(
        *xstride, bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(format), nchannels);
}



void
OIIO_ImageSpec_erase_attribute(OIIO_ImageSpec* is, const char* name,
                               OIIO_TypeDesc searchtype, bool casesensitive)
{
    to_cpp(is)->erase_attribute(name,
                                bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(
                                    searchtype),
                                casesensitive);
}



OIIO_ParamValue*
OIIO_ImageSpec_find_attribute(OIIO_ImageSpec* is, const char* name,
                              OIIO_TypeDesc searchtype, bool casesensitive)
{
    OIIO::ParamValue* pv = to_cpp(is)->find_attribute(
        name, bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(searchtype),
        casesensitive);
    return to_c(pv);
}



void
OIIO_ImageSpec_metadata_val(const OIIO_ImageSpec* is, const OIIO_ParamValue* p,
                            bool human, char* string_buffer, int buffer_length)
{
    std::string s = to_cpp(is)->metadata_val(*to_cpp(p), human);
    safe_strcpy(string_buffer, s, buffer_length);
}



OIIO_API void
OIIO_ImageSpec_serialize(const OIIO_ImageSpec* is, int format, int verbose,
                         char* string_buffer, int buffer_length)
{
    std::string s
        = to_cpp(is)->serialize((OIIO::ImageSpec::SerialFormat)format,
                                (OIIO::ImageSpec::SerialVerbose)verbose);
    safe_strcpy(string_buffer, s, buffer_length);
}



OIIO_API void
OIIO_ImageSpec_to_xml(const OIIO_ImageSpec* is, char* string_buffer,
                      int buffer_length)
{
    std::string s = to_cpp(is)->to_xml();
    safe_strcpy(string_buffer, s, buffer_length);
}



void
OIIO_ImageSpec_from_xml(OIIO_ImageSpec* is, const char* xml)
{
    to_cpp(is)->from_xml(xml);
}



void
OIIO_ImageSpec_decode_compression_metadata(OIIO_ImageSpec* is,
                                           const char* default_comp, char* comp,
                                           int comp_length, int* qual)
{
    auto p = to_cpp(is)->decode_compression_metadata(default_comp, *qual);
    safe_strcpy(comp, p.first.c_str(), comp_length);
    *qual = p.second;
}

bool
OIIO_ImageSpec_valid_tile_range(OIIO_ImageSpec* is, int xbegin, int xend,
                                int ybegin, int yend, int zbegin, int zend)
{
    return to_cpp(is)->valid_tile_range(xbegin, xend, ybegin, yend, zbegin,
                                        zend);
}



int
OIIO_ImageSpec_nchannels(const OIIO_ImageSpec* is)
{
    return to_cpp(is)->nchannels;
}



OIIO_TypeDesc
OIIO_ImageSpec_channelformat(const OIIO_ImageSpec* is, int chan)
{
    return bit_cast<OIIO::TypeDesc, OIIO_TypeDesc>(
        to_cpp(is)->channelformat(chan));
}



OIIO_API void
OIIO_ImageSpec_get_channelformats(const OIIO_ImageSpec* is,
                                  OIIO_TypeDesc* formats)
{
    const OIIO::ImageSpec* ois = to_cpp(is);
    memcpy(formats, ois->channelformats.data(),
           sizeof(OIIO_TypeDesc) * ois->channelformats.size());
    for (int i = ois->channelformats.size(); i < ois->nchannels; ++i) {
        formats[i] = bit_cast<OIIO::TypeDesc, OIIO_TypeDesc>(ois->format);
    }
}



int
OIIO_ImageSpec_channelindex(const OIIO_ImageSpec* is, const char* name)
{
    return to_cpp(is)->channelindex(name);
}



OIIO_ROI
OIIO_ImageSpec_roi(const OIIO_ImageSpec* is)
{
    return bit_cast<OIIO::ROI, OIIO_ROI>(to_cpp(is)->roi());
}



OIIO_ROI
OIIO_ImageSpec_roi_full(const OIIO_ImageSpec* is)
{
    return bit_cast<OIIO::ROI, OIIO_ROI>(to_cpp(is)->roi_full());
}



const char*
OIIO_ImageSpec_channel_name(const OIIO_ImageSpec* is, int chan)
{
    return to_cpp(is)->channel_name(chan).c_str();
}



bool
OIIO_ImageSpec_getattribute(const OIIO_ImageSpec* is, const char* name,
                            OIIO_TypeDesc type, void* value, bool casesensitive)
{
    return to_cpp(is)->getattribute(name,
                                    bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(
                                        type),
                                    value, casesensitive);
}



// FIXME: add Filesystem::IOProxy
OIIO_ImageInput*
OIIO_ImageInput_open(const char* filename, const OIIO_ImageSpec* config,
                     OIIO_Filesystem_IOProxy* ioproxy)
{
    auto ii = OIIO::ImageInput::open(
        filename, to_cpp(config),
        reinterpret_cast<OIIO::Filesystem::IOProxy*>(ioproxy));
    return to_c(ii.release());
}



void
OIIO_ImageInput_delete(OIIO_ImageInput* ii)
{
    delete to_cpp(ii);
}



const OIIO_ImageSpec*
OIIO_ImageInput_spec(OIIO_ImageInput* ii)
{
    const OIIO::ImageSpec& spec = to_cpp(ii)->spec();
    return to_c(&spec);
}



OIIO_ImageSpec*
OIIO_ImageInput_spec_copy(OIIO_ImageInput* ii, int subimage, int miplevel)
{
    return to_c(new OIIO::ImageSpec(to_cpp(ii)->spec(subimage, miplevel)));
}



OIIO_ImageSpec*
OIIO_ImageInput_spec_dimensions(OIIO_ImageInput* ii, int subimage, int miplevel)
{
    return to_c(
        new OIIO::ImageSpec(to_cpp(ii)->spec_dimensions(subimage, miplevel)));
}



bool
OIIO_ImageInput_close(OIIO_ImageInput* ii)
{
    return to_cpp(ii)->close();
}



int
OIIO_ImageInput_current_subimage(OIIO_ImageInput* ii)
{
    return to_cpp(ii)->current_subimage();
}



int
OIIO_ImageInput_current_miplevel(OIIO_ImageInput* ii)
{
    return to_cpp(ii)->current_miplevel();
}



OIIO_API bool
OIIO_ImageInput_seek_subimage(OIIO_ImageInput* ii, int subimage, int miplevel)
{
    return to_cpp(ii)->seek_subimage(subimage, miplevel);
}



OIIO_API bool
OIIO_ImageInput_read_scanline(OIIO_ImageInput* ii, int y, int z,
                              OIIO_TypeDesc format, void* data,
                              stride_t xstride)
{
    return to_cpp(ii)->read_scanline(
        y, z, bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(format), data, xstride);
}



OIIO_API bool
OIIO_ImageInput_read_scanlines(OIIO_ImageInput* ii, int subimage, int miplevel,
                               int ybegin, int yend, int z, int chbegin,
                               int chend, OIIO_TypeDesc format, void* data,
                               stride_t xstride, stride_t ystride)
{
    return to_cpp(ii)->read_scanlines(subimage, miplevel, ybegin, yend, z,
                                      chbegin, chend,
                                      bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(
                                          format),
                                      data, xstride, ystride);
}



OIIO_API bool
OIIO_ImageInput_read_tile(OIIO_ImageInput* ii, int x, int y, int z,
                          OIIO_TypeDesc format, void* data, stride_t xstride,
                          stride_t ystride, stride_t zstride)
{
    return to_cpp(ii)->read_tile(x, y, z,
                                 bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(format),
                                 data, xstride, ystride, zstride);
}



OIIO_API bool
OIIO_ImageInput_read_tiles(OIIO_ImageInput* ii, int subimage, int miplevel,
                           int xbegin, int xend, int ybegin, int yend,
                           int zbegin, int zend, int chbegin, int chend,
                           OIIO_TypeDesc format, void* data, stride_t xstride,
                           stride_t ystride, stride_t zstride)
{
    return to_cpp(ii)->read_tiles(subimage, miplevel, xbegin, xend, ybegin,
                                  yend, zbegin, zend, chbegin, chend,
                                  bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(
                                      format),
                                  data, xstride, ystride, zstride);
}



bool
OIIO_ImageInput_read_image(OIIO_ImageInput* ii, int subimage, int miplevel,
                           int chbegin, int chend, OIIO_TypeDesc format,
                           void* data, stride_t xstride, stride_t ystride,
                           stride_t zstride,
                           OIIO_ProgressCallback progress_callback,
                           void* progress_callback_data)
{
    return to_cpp(ii)->read_image(subimage, miplevel, chbegin, chend,
                                  bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(
                                      format),
                                  data, xstride, ystride, zstride,
                                  progress_callback, progress_callback_data);
}



bool
OIIO_ImageInput_read_native_deep_scanlines(OIIO_ImageInput* ii, int subimage,
                                           int miplevel, int ybegin, int yend,
                                           int z, int chbegin, int chend,
                                           OIIO_DeepData* deepdata)
{
    return to_cpp(ii)->read_native_deep_scanlines(subimage, miplevel, ybegin,
                                                  yend, z, chbegin, chend,
                                                  *to_cpp(deepdata));
}



bool
OIIO_ImageInput_read_native_deep_tiles(OIIO_ImageInput* ii, int subimage,
                                       int miplevel, int xbegin, int xend,
                                       int ybegin, int yend, int zbegin,
                                       int zend, int chbegin, int chend,
                                       OIIO_DeepData* deepdata)
{
    return to_cpp(ii)->read_native_deep_tiles(subimage, miplevel, xbegin, xend,
                                              ybegin, yend, zbegin, zend,
                                              chbegin, chend,
                                              *to_cpp(deepdata));
}



bool
OIIO_ImageInput_read_native_deep_image(OIIO_ImageInput* ii, int subimage,
                                       int miplevel, OIIO_DeepData* deepdata)
{
    return to_cpp(ii)->read_native_deep_image(subimage, miplevel,
                                              *to_cpp(deepdata));
}



bool
OIIO_ImageInput_has_error(const OIIO_ImageInput* ii)
{
    return to_cpp(ii)->has_error();
}



void
OIIO_ImageInput_geterror(const OIIO_ImageInput* ii, char* msg,
                         int buffer_length, bool clear)
{
    std::string errorstring = to_cpp(ii)->geterror(clear);
    safe_strcpy(msg, errorstring, buffer_length);
}



OIIO_ImageOutput*
OIIO_ImageOutput_create(const char* filename, OIIO_Filesystem_IOProxy* ioproxy,
                        const char* plugin_search_path)
{
    return to_c(OIIO::ImageOutput::create(
                    filename,
                    reinterpret_cast<OIIO::Filesystem::IOProxy*>(ioproxy),
                    plugin_search_path)
                    .release());
}



void
OIIO_ImageOutput_delete(OIIO_ImageOutput* io)
{
    delete to_cpp(io);
}



const char*
OIIO_ImageOutput_format_name(OIIO_ImageOutput* io)
{
    return to_cpp(io)->format_name();
}



int
OIIO_ImageOutput_supports(OIIO_ImageOutput* io, const char* feature)
{
    return to_cpp(io)->supports(feature);
}



bool
OIIO_ImageOutput_open(OIIO_ImageOutput* io, const char* name,
                      const OIIO_ImageSpec* newspec, int mode)
{
    return to_cpp(io)->open(std::string(name), *to_cpp(newspec),
                            (OIIO::ImageOutput::OpenMode)mode);
}



bool
OIIO_ImageOutput_open_multiimage(OIIO_ImageOutput* io, const char* name,
                                 int subimages, const OIIO_ImageSpec* specs)
{
    std::string sname(name);
    return to_cpp(io)->open(sname, subimages, to_cpp(specs));
}



const OIIO_ImageSpec*
OIIO_ImageOutput_spec(const OIIO_ImageOutput* io)
{
    return to_c(&to_cpp(io)->spec());
}



OIIO_API bool
OIIO_ImageOutput_close(OIIO_ImageOutput* io);



OIIO_API bool
OIIO_ImageOutput_write_scanline(OIIO_ImageOutput* io, int y, int z,
                                OIIO_TypeDesc format, const void* data,
                                stride_t xstride)
{
    return to_cpp(io)->write_scanline(
        y, z, bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(format), data, xstride);
}



OIIO_API bool
OIIO_ImageOutput_write_scanlines(OIIO_ImageOutput* io, int ybegin, int yend,
                                 int z, OIIO_TypeDesc format, const void* data,
                                 stride_t xstride, stride_t ystride)
{
    return to_cpp(io)->write_scanlines(ybegin, yend, z,
                                       bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(
                                           format),
                                       data, xstride, ystride);
}



OIIO_API bool
OIIO_ImageOuptut_write_tile(OIIO_ImageOutput* io, int x, int y, int z,
                            OIIO_TypeDesc format, const void* data,
                            stride_t xstride, stride_t ystride,
                            stride_t zstride)
{
    return to_cpp(io)->write_tile(x, y, z,
                                  bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(
                                      format),
                                  data, xstride, ystride, zstride);
}



OIIO_API bool
OIIO_ImageOutput_write_tiles(OIIO_ImageOutput* io, int xbegin, int xend,
                             int ybegin, int yend, int zbegin, int zend,
                             OIIO_TypeDesc format, const void* data,
                             stride_t xstride, stride_t ystride,
                             stride_t zstride)
{
    return to_cpp(io)->write_tiles(xbegin, xend, ybegin, yend, zbegin, zend,
                                   bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(
                                       format),
                                   data, xstride, ystride, zstride);
}



OIIO_API bool
OIIO_ImageOutput_write_rectangle(OIIO_ImageOutput* io, int xbegin, int xend,
                                 int ybegin, int yend, int zbegin, int zend,
                                 OIIO_TypeDesc format, const void* data,
                                 stride_t xstride, stride_t ystride,
                                 stride_t zstride)
{
    return to_cpp(io)->write_rectangle(xbegin, xend, ybegin, yend, zbegin, zend,
                                       bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(
                                           format),
                                       data, xstride, ystride, zstride);
}



OIIO_API bool
OIIO_ImageOutput_write_deep_scanlines(OIIO_ImageOutput* io, int ybegin,
                                      int yend, int z,
                                      const OIIO_DeepData* deepdata)
{
    return to_cpp(io)->write_deep_scanlines(ybegin, yend, z, *to_cpp(deepdata));
}



OIIO_API bool
OIIO_ImageOutput_write_deep_tiles(OIIO_ImageOutput* io, int xbegin, int xend,
                                  int ybegin, int yend, int zbegin, int zend,
                                  const OIIO_DeepData* deepdata)
{
    return to_cpp(io)->write_deep_tiles(xbegin, xend, ybegin, yend, zbegin,
                                        zend, *to_cpp(deepdata));
}



OIIO_API bool
OIIO_ImageOutput_write_deep_image(OIIO_ImageOutput* io,
                                  const OIIO_DeepData* deepdata)
{
    return to_cpp(io)->write_deep_image(*to_cpp(deepdata));
}



OIIO_API bool
OIIO_ImageOutput_copy_image(OIIO_ImageOutput* io, OIIO_ImageInput* in)
{
    return to_cpp(io)->copy_image(to_cpp(in));
}



OIIO_API bool
OIIO_ImageOutput_set_ioproxy(OIIO_ImageOutput* io,
                             OIIO_Filesystem_IOProxy* ioproxy)
{
    return to_cpp(io)->set_ioproxy(
        reinterpret_cast<OIIO::Filesystem::IOProxy*>(ioproxy));
}



OIIO_API void
OIIO_ImageOutput_set_threads(OIIO_ImageOutput* io, int n)
{
    to_cpp(io)->threads(n);
}



OIIO_API int
OIIO_ImageOutput_threads(const OIIO_ImageOutput* io)
{
    return to_cpp(io)->threads();
}



bool
OIIO_ImageOutput_has_error(const OIIO_ImageOutput* io)
{
    return to_cpp(io)->has_error();
}



void
OIIO_ImageOutput_geterror(const OIIO_ImageOutput* io, char* msg,
                          int buffer_length, bool clear)
{
    std::string errorstring = to_cpp(io)->geterror(clear);
    safe_strcpy(msg, errorstring, buffer_length);
}



bool
OIIO_ImageOutput_write_image(OIIO_ImageOutput* io, OIIO_TypeDesc format,
                             const void* data, stride_t xstride,
                             stride_t ystride, stride_t zstride,
                             OIIO_ProgressCallback progress_callback,
                             void* progress_callback_data)
{
    return to_cpp(io)->write_image(bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(
                                       format),
                                   data, xstride, ystride, zstride,
                                   progress_callback, progress_callback_data);
}



int
OIIO_openimageio_version()
{
    return OIIO::openimageio_version();
}



bool
OIIO_haserror()
{
    return OIIO::has_error();
}



void
OIIO_geterror(char* msg, int buffer_length, bool clear)
{
    std::string errorstring = OIIO::geterror(clear);
    safe_strcpy(msg, errorstring, buffer_length);
}



OIIO_API bool
OIIO_attribute(const char* name, OIIO_TypeDesc type, const void* val)
{
    return OIIO::attribute(name, bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(type),
                           val);
}



OIIO_API bool
OIIO_getattribute(const char* name, OIIO_TypeDesc type, void* val)
{
    return OIIO::getattribute(name,
                              bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(type),
                              val);
}



OIIO_API bool
OIIO_convert_pixel_values(OIIO_TypeDesc src_type, const void* src,
                          OIIO_TypeDesc dst_type, void* dst, int n)
{
    return OIIO::convert_pixel_values(
        bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(src_type), src,
        bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(dst_type), dst, n);
}



OIIO_API
bool
OIIO_convert_image(int nchannels, int width, int height, int depth,
                   const void* src, OIIO_TypeDesc src_type,
                   stride_t src_xstride, stride_t src_ystride,
                   stride_t src_zstride, void* dst, OIIO_TypeDesc dst_type,
                   stride_t dst_xstride, stride_t dst_ystride,
                   stride_t dst_zstride)
{
    return OIIO::convert_image(nchannels, width, height, depth, src,
                               bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(src_type),
                               src_xstride, src_ystride, src_zstride, dst,
                               bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(dst_type),
                               dst_xstride, dst_ystride, dst_zstride);
}



OIIO_API bool
OIIO_parallel_convert_image(int nchannels, int width, int height, int depth,
                            const void* src, OIIO_TypeDesc src_type,
                            stride_t src_xstride, stride_t src_ystride,
                            stride_t src_zstride, void* dst,
                            OIIO_TypeDesc dst_type, stride_t dst_xstride,
                            stride_t dst_ystride, stride_t dst_zstride,
                            int nthreads)
{
    return OIIO::parallel_convert_image(
        nchannels, width, height, depth, src,
        bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(src_type), src_xstride,
        src_ystride, src_zstride, dst,
        bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(dst_type), dst_xstride,
        dst_ystride, dst_zstride, nthreads);
}



OIIO_API void
OIIO_add_dither(int nchannels, int width, int height, int depth, float* data,
                stride_t xstride, stride_t ystride, stride_t zstride,
                float ditheramplitude, int alpha_channel, int z_channel,
                unsigned int ditherseed, int chorigin, int xorigin, int yorigin,
                int zorigin)
{
    return OIIO::add_dither(nchannels, width, height, depth, data, xstride,
                            ystride, zstride, ditheramplitude, alpha_channel,
                            z_channel, ditherseed, chorigin, xorigin, yorigin,
                            zorigin);
}



OIIO_API void
OIIO_premult(int nchannels, int width, int height, int depth, int chbegin,
             int chend, OIIO_TypeDesc datatype, void* data, stride_t xstride,
             stride_t ystride, stride_t zstride, int alpha_channel,
             int z_channel)
{
    return OIIO::premult(nchannels, width, height, depth, chbegin, chend,
                         bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(datatype),
                         data, xstride, ystride, zstride, alpha_channel,
                         z_channel);
}



OIIO_API bool
OIIO_copy_image(int nchannels, int width, int height, int depth,
                const void* src, stride_t pixelsize, stride_t src_xstride,
                stride_t src_ystride, stride_t src_zstride, void* dst,
                stride_t dst_xstride, stride_t dst_ystride,
                stride_t dst_zstride)
{
    return OIIO::copy_image(nchannels, width, height, depth, src, pixelsize,
                            src_xstride, src_ystride, src_zstride, dst,
                            dst_xstride, dst_ystride, dst_zstride);
}


OIIO_API bool
OIIO_wrap_black(int* coord, int origin, int width)
{
    return OIIO::wrap_black(*coord, origin, width);
}



OIIO_API bool
OIIO_wrap_clamp(int* coord, int origin, int width)
{
    return OIIO::wrap_clamp(*coord, origin, width);
}



OIIO_API bool
OIIO_wrap_periodic(int* coord, int origin, int width)
{
    return OIIO::wrap_periodic(*coord, origin, width);
}



OIIO_API bool
OIIO_wrap_periodic_pow2(int* coord, int origin, int width)
{
    return OIIO::wrap_periodic_pow2(*coord, origin, width);
}



OIIO_API bool
OIIO_wrap_mirror(int* coord, int origin, int width)
{
    return OIIO::wrap_mirror(*coord, origin, width);
}
}

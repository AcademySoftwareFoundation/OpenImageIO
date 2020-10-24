// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio


#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <OpenImageIO/export.h>

#include "c-paramlist.h"
#include "c-typedesc.h"

typedef int64_t stride_t;
typedef uint64_t imagesize_t;

#ifdef __cplusplus
extern "C" {
#endif

extern stride_t OIIO_AutoStride;

/// Pointer to a function called periodically by read_image and
/// write_image.  This can be used to implement progress feedback, etc.
/// It takes an opaque data pointer (passed to read_image/write_image)
/// and a float giving the portion of work done so far.  It returns a
/// bool, which if 'true' will STOP the read or write.
typedef bool (*OIIO_ProgressCallback)(void* opaque_data, float portion_done);


// Forward declarations
typedef struct OIIO_Filesystem_IOProxy OIIO_Filesystem_IOProxy;
typedef struct OIIO_DeepData OIIO_DeepData;

/// ROI is a small helper struct describing a rectangular region of interest
/// in an image. The region is [xbegin,xend) x [begin,yend) x [zbegin,zend),
/// with the "end" designators signifying one past the last pixel in each
/// dimension, a la STL style.
///
typedef struct {
    ///@{
    /// @name ROI data members
    /// The data members are:
    ///
    ///     int xbegin, xend, ybegin, yend, zbegin, zend;
    ///     int chbegin, chend;
    ///
    /// These describe the spatial extent
    ///     [xbegin,xend) x [ybegin,yend) x [zbegin,zend)
    /// And the channel extent:
    ///     [chbegin,chend)]
    int xbegin, xend;
    int ybegin, yend;
    int zbegin, zend;
    int chbegin, chend;
    ///@}
} OIIO_ROI;

/// Constructs an ROI representing the entire image.
///
/// Equivalent C++: `ROI::All() or ROI()`
///
OIIO_API OIIO_ROI
OIIO_ROI_All();

/// Is a region defined?
OIIO_API bool
OIIO_ROI_defined(const OIIO_ROI* roi);

///@{
/// @name Spatial size functions.
/// The width, height, and depth of the region.
OIIO_API int
OIIO_ROI_width(const OIIO_ROI* roi);

OIIO_API int
OIIO_ROI_height(const OIIO_ROI* roi);

OIIO_API int
OIIO_ROI_depth(const OIIO_ROI* roi);
///@}

/// Number of channels in the region.  Beware -- this defaults to a
/// huge number, and to be meaningful you must consider
/// MIN(OIIO_ImageBuf_nchannels(), OIIO_ROI_nchannels())
OIIO_API int
OIIO_ROI_nchannels(const OIIO_ROI* roi);

/// Total number of pixels in the region.
OIIO_API imagesize_t
OIIO_ROI_npixels(const OIIO_ROI* roi);

/// Test if the given coordinate is within the ROI
OIIO_API bool
OIIO_ROI_contains(const OIIO_ROI* roi, int x, int y, int z, int ch);

/// Test if one ROI is entirely within another
OIIO_API bool
OIIO_ROI_contains_roi(const OIIO_ROI* container, OIIO_ROI* containee);

/// ImageSpec describes the data format of an image -- dimensions, layout,
/// number and meanings of image channels.
///
/// It is equivalent to the C++ ImageSpec, but is exposed to C as an opaque
/// pointer.
///
typedef struct OIIO_ImageSpec OIIO_ImageSpec;

/// Create a new ImageSpec. The caller takes ownership and is responsible for
/// calling ImageSpec_delete.
///
/// Equivalent C++: `new ImageSpec`
///
OIIO_API OIIO_ImageSpec*
OIIO_ImageSpec_new();

/// Delete an ImageSpec
///
/// Equivalent C++: `delete is;`
///
OIIO_API void
OIIO_ImageSpec_delete(const OIIO_ImageSpec* is);

/// Create a new ImageSpec with explicit dimensions
///
/// Equivalent C++: `new ImageSpec(xres, yres, nchans, fmt)`
///
OIIO_API OIIO_ImageSpec*
OIIO_ImageSpec_new_with_dimensions(int xres, int yres, int nchans,
                                   OIIO_TypeDesc fmt);

/// Create a new ImageSpec whose dimensions (both data and display) and
/// number of channels are given by `roi`, pixel data type by `fmt`.
///
/// Equivalent C++: `new ImageSpec(roi, fmt)`
///
OIIO_API OIIO_ImageSpec*
OIIO_ImageSpec_new_with_roi(const OIIO_ROI* roi, OIIO_TypeDesc fmt);

/// Create a copy of this ImageSpec with the default copy constructor
///
/// Equivalent C++: `new ImageSpec(*is)`
///
OIIO_API OIIO_ImageSpec*
OIIO_ImageSpec_copy(const OIIO_ImageSpec* is);

/// Origin (upper-left corner) of the pixel data
///
/// Equivalent C++: `is->x`
///
OIIO_API int
OIIO_ImageSpec_x(const OIIO_ImageSpec* is);

/// Origin (upper-left corner) of the pixel data
///
/// Equivalent C++: `is->x = x`
///
OIIO_API void
OIIO_ImageSpec_set_x(OIIO_ImageSpec* is, int x);

/// Origin (upper-left corner of the pixel data)
///
/// Equivalent C++: `is->y`
///
OIIO_API int
OIIO_ImageSpec_y(const OIIO_ImageSpec* is);

/// Origin (upper-left corner) of the piyel data
///
/// Equivalent C++: `is->y = y`
///
OIIO_API void
OIIO_ImageSpec_set_y(OIIO_ImageSpec* is, int y);

/// Origin (upper-left corner of the pixel data)
///
/// Equivalent C++: `is->z`
///
OIIO_API int
OIIO_ImageSpec_z(const OIIO_ImageSpec* is);

/// Origin (upper-left corner) of the pizel data
///
/// Equivalent C++: `is->z = z`
///
OIIO_API void
OIIO_ImageSpec_set_z(OIIO_ImageSpec* is, int z);

/// Width of the pixel data window
///
/// Equivalent C++: `is->width`
///
OIIO_API int
OIIO_ImageSpec_width(const OIIO_ImageSpec* is);

/// Width of the pixel data window
///
/// Equivalent C++: `is->width = width`
///
OIIO_API void
OIIO_ImageSpec_set_width(OIIO_ImageSpec* is, int width);

/// Height of the pixel data window
///
/// Equivalent C++: `is->height`
///
OIIO_API int
OIIO_ImageSpec_height(const OIIO_ImageSpec* is);

/// Height of the pixel data window
///
/// Equivalent C++: `is->height = height`
///
OIIO_API void
OIIO_ImageSpec_set_height(OIIO_ImageSpec* is, int height);

/// Depth of the pixel data window
///
/// Equivalent C++: `is->depth`
///
OIIO_API int
OIIO_ImageSpec_depth(const OIIO_ImageSpec* is);

/// Depth of the pixel data window
///
/// Equivalent C++: `is->depth = depth`
///
OIIO_API void
OIIO_ImageSpec_set_depth(OIIO_ImageSpec* is, int depth);

/// Origin of the full (display) window
///
/// Equivalent C++: `is->full_x`
///
OIIO_API int
OIIO_ImageSpec_full_x(const OIIO_ImageSpec* is);

/// Origin of the full (display) window
///
/// Equivalent C++: `is->full_x = full_x`
///
OIIO_API void
OIIO_ImageSpec_set_full_x(OIIO_ImageSpec* is, int full_x);

/// Origin of the full (display) window
///
/// Equivalent C++: `is->full_y`
///
OIIO_API int
OIIO_ImageSpec_full_y(const OIIO_ImageSpec* is);

/// Origin of the full (display) window
///
/// Equivalent C++: `is->full_y = full_y
///
OIIO_API void
OIIO_ImageSpec_set_full_y(OIIO_ImageSpec* is, int full_y);

/// Origin of the full (display) window
///
/// Equivalent C++: `is->full_z`
///
OIIO_API int
OIIO_ImageSpec_full_z(const OIIO_ImageSpec* is);

/// Origin of the full (display) window
///
/// Equivalent C++: `is->full_z = full_z`
///
OIIO_API void
OIIO_ImageSpec_set_full_z(OIIO_ImageSpec* is, int full_z);

/// Width of the full (display) window
///
/// Equivalent C++: `is->full_width`
///
OIIO_API int
OIIO_ImageSpec_full_width(const OIIO_ImageSpec* is);

/// Width of the full (display) window
///
/// Equivalent C++: `is->full_width = full_width`
///
OIIO_API void
OIIO_ImageSpec_set_full_width(OIIO_ImageSpec* is, int full_width);

/// Height of the full (display) window
///
/// Equivalent C++: `is->full_height`
///
OIIO_API int
OIIO_ImageSpec_full_height(const OIIO_ImageSpec* is);

/// Height of the full (display) window
///
/// Equivalent C++: `is->full_height = full_height`
///
OIIO_API void
OIIO_ImageSpec_set_full_height(OIIO_ImageSpec* is, int full_height);

/// Depth of the full (display) window
///
/// Equivalent C++: `is->full_depth`
///
OIIO_API int
OIIO_ImageSpec_full_depth(const OIIO_ImageSpec* is);

/// Depth of the full (display) window
///
/// Equivalent C++: `is->full_depth = full_depth`
///
OIIO_API void
OIIO_ImageSpec_set_full_depth(OIIO_ImageSpec* is, int full_depth);

/// Tile width (0 for a non-tiled image)
///
/// Equivalent C++: `is->tile_width`
///
OIIO_API int
OIIO_ImageSpec_tile_width(const OIIO_ImageSpec* is);

/// Tile width (0 for a non-tiled image)
///
/// Equivalent C++: `is->tile_width = tile_width`
///
OIIO_API void
OIIO_ImageSpec_set_tile_width(OIIO_ImageSpec* is, int tile_width);

/// Tile height (0 for a non-tiled image)
///
/// Equivalent C++: `is->tile_height`
///
OIIO_API int
OIIO_ImageSpec_tile_height(const OIIO_ImageSpec* is);

/// Tile height (0 for a non-tiled image)
///
/// Equivalent C++: `is->tile_height`
///
OIIO_API void
OIIO_ImageSpec_set_tile_height(OIIO_ImageSpec* is, int tile_height);

/// Tile depth (0 for a non-tiled image)
///
/// Equivalent C++: `is->tile_depth`
///
OIIO_API int
OIIO_ImageSpec_tile_depth(const OIIO_ImageSpec* is);

/// Tile depth (0 for a non-tiled image)
///
/// Equivalent C++: `is->tile_depth`
///
OIIO_API void
OIIO_ImageSpec_set_tile_depth(OIIO_ImageSpec* is, int tile_depth);

/// Number of image channels, e.g. 4 for RGBA
///
/// Equivalent C++: `is->nchannels`
///
OIIO_API int
OIIO_ImageSpec_nchannels(const OIIO_ImageSpec* is);

/// Data format of the channels
///
/// Equivalent C++: `is->format`
///
OIIO_API OIIO_TypeDesc
OIIO_ImageSpec_format(const OIIO_ImageSpec* is);

/// Set the data format and clear any per-channel format information.
///
/// Equivalent C++: `is->set_format(format)`
///
OIIO_API void
OIIO_ImageSpec_set_format(OIIO_ImageSpec* is, OIIO_TypeDesc format);

/// Index of the channel that represents *alpha* (pixel coverage and/or transparency)
///
/// Equivalent C++: `is->alpha_channel`
///
OIIO_API int
OIIO_ImageSpec_alpha_channel(const OIIO_ImageSpec* is);

/// Index of the channel that represents *alpha* (pixel coverage and/or transparency)
///
/// Equivalent C++: `is->alpha_channel = alpha_channel`
///
OIIO_API void
OIIO_ImageSpec_set_alpha_channel(OIIO_ImageSpec* is, int alpha_channel);

/// Index of the channel that represents *z* or *depth* (from the camera).
///
/// Equivalent C++: `is->z_channel`
///
OIIO_API int
OIIO_ImageSpec_z_channel(const OIIO_ImageSpec* is);

/// Index of the channel that represents *z* or *depth* (from the camera).
///
/// Equivalent C++: `is->z_channel = z_channel`
///
OIIO_API void
OIIO_ImageSpec_set_z_channel(OIIO_ImageSpec* is, int z_channel);

/// If true, this indicates that the image contains "deep" data consisting of
/// multiple samples per pixel.
///
/// Equivalent C++: `is->deep`
///
OIIO_API bool
OIIO_ImageSpec_deep(const OIIO_ImageSpec* is);

/// If true, this indicates that the image contains "deep" data consisting of
/// multiple samples per pixel.
///
/// Equivalent C++: `is->deep = deep`
///
OIIO_API void
OIIO_ImageSpec_set_deep(OIIO_ImageSpec* is, bool deep);

/// Sets the `channelnames` to reasonable defaults for the number of channels.
///
/// Equivalent C++: `is->default_channel_names()`
///
OIIO_API void
OIIO_ImageSpec_default_channel_names(OIIO_ImageSpec* is);

/// Returns the number of bytes comprising each channel of each pixel.
///
/// Equivalent C++: `is->channel_bytes()`
///
OIIO_API size_t
OIIO_ImageSpec_channel_bytes(const OIIO_ImageSpec* is);

/// Returns the number of bytes comprising the single specified channel.
///
/// Equivalent C++: `is->channel_bytes(chan, native)`
///
OIIO_API size_t
OIIO_ImageSpec_channel_bytes_at(const OIIO_ImageSpec* is, int chan,
                                bool native);

/// Returns the number of bytes for each pixel, counting all channels.
///
/// Equivalent C++: `is->pixel_bytes(native)`
///
OIIO_API size_t
OIIO_ImageSpec_pixel_bytes(const OIIO_ImageSpec* is, bool native);

/// Returns the number of bytes for each pixel counting just the channels in
/// range [chbegin, chend)
///
/// Equivalent C++: `is->pixel_bytes(chbegin, chend, native)`
///
OIIO_API size_t
OIIO_ImageSpec_pixel_bytes_for_channels(const OIIO_ImageSpec* is, int chbegin,
                                        int chend, bool native);

/// Returns the number of bytes for each scanline.
///
/// Equivalent C++: `is->scanline_bytes(native)`
///
OIIO_API imagesize_t
OIIO_ImageSpec_scanline_bytes(const OIIO_ImageSpec* is, bool native);

/// Returns the number of pixels comprising an image tile.
///
/// Equivalent C++: `is->tile_pixels()`
///
OIIO_API imagesize_t
OIIO_ImageSpec_tile_pixels(const OIIO_ImageSpec* is);

/// Returns the number of bytes comprising an image tile.
///
/// Equivalent C++: `is->tile_bytes(native)`
///
OIIO_API imagesize_t
OIIO_ImageSpec_tile_bytes(const OIIO_ImageSpec* is, bool native);

/// Returns the number of pixels comprising the entire image
///
/// Equivalent C++: `is->image_pixels()`
///
OIIO_API imagesize_t
OIIO_ImageSpec_image_pixels(const OIIO_ImageSpec* is);

/// Returns the number of bytes comprising the entire image
///
/// Equivalent C++: `is->image_bytes(native)`
///
OIIO_API imagesize_t
OIIO_ImageSpec_image_bytes(const OIIO_ImageSpec* is, bool native);

/// Verify that on this platform, a `size_t` is big enough to hold the
/// number of bytes (and pixels) in a scanline, a tile, and the
/// whole image.
///
/// Equivalent C++: `is->size_t_safe()`
///
OIIO_API bool
OIIO_ImageSpec_size_t_safe(const OIIO_ImageSpec* is);

/// Adjust the stride values, if set to AutoStride, to be the right
/// sizes for contiguous data with the given channel size, channels,
/// width, height.
///
/// Equivalent C++: `ImageSpec::auto_stride(*xstride, *ystride, *zstride,
///                                         channelsize, nchannels, width, height)`
///
/// TODO: (AL) other two overloads of this - which is the most general
OIIO_API void
OIIO_ImageSpec_auto_stride(stride_t* xstride, stride_t* ystride,
                           stride_t* zstride, stride_t channelsize,
                           int nchannels, int width, int height);

/// Add a metadata attribute to `extra_attribs`, with the given name and
/// data type. The `value` pointer specifies the address of the data to
/// be copied.
///
/// Equivalent C++: `is->attribute(name, fmt, value)`
///
OIIO_API void
OIIO_ImageSpec_attribute(OIIO_ImageSpec* is, const char* name,
                         OIIO_TypeDesc fmt, const void* value);

/// If the `ImageSpec` contains the named attribute and its type matches
/// `type`, copy the attribute value into the memory pointed to by `val`
/// (it is up to the caller to ensure there is enough space) and return
/// `true`. If no such attribute is found, or if it doesn't match the
/// type, return `false` and do not modify `val`.
///
/// Note that when passing a string, you need to pass a pointer to the
/// `char*`, not a pointer to the first character.  Also, the `char*`
/// will end up pointing to characters owned by the `ImageSpec`; the
/// caller does not need to ever free the memory that contains the
/// characters.
///
/// Equivalent C++: `is->getattribute(name, type, value, casesensitive)`
///
OIIO_API bool
OIIO_ImageSpec_getattribute(const OIIO_ImageSpec* is, const char* name,
                            OIIO_TypeDesc type, void* value,
                            bool casesensitive);

/// Searches `extra_attribs` for any attributes matching `name` (as a
/// regular expression), removing them entirely from `extra_attribs`.
///
/// Equivalent C++: `is->erase_attribute(name, searchtype, casesensitive)`
///
OIIO_API void
OIIO_ImageSpec_erase_attribute(OIIO_ImageSpec* is, const char* name,
                               OIIO_TypeDesc searchtype, bool casesensitive);


/// Searches `extra_attribs` for any attributes matching `name` (as a
/// regular expression), removing them entirely from `extra_attribs`.
///
/// Equivalent C++: `is->erase_attribute(name, searchtype, casesensitive)`
///
/// TODO: (AL) - other overload that takes temp storage to get data members
/// of ImageSpec
OIIO_API OIIO_ParamValue*
OIIO_ImageSpec_find_attribute(OIIO_ImageSpec* is, const char* name,
                              OIIO_TypeDesc searchtype, bool casesensitive);

/// For a given parameter `p`, format the value nicely as a string.
///
/// @param p Parameter to get the string for
/// @param human If true, use especially human-readable explanations (units or
///              decoding of values) for certain, known metadata
/// @param string_buffer Caller-provided storage for the generated string
/// @param buffer_length Size of the provided string buffer. If the size of the
///                      provided storage is not large enough to hold the entire
///                      generated string, the string will be truncated to fit.
///
OIIO_API void
OIIO_ImageSpec_metadata_val(const OIIO_ImageSpec* is, const OIIO_ParamValue* p,
                            bool human, char* string_buffer, int buffer_length);

enum OIIO_ImageSpec_SerialFormat {
    OIIO_ImageSpec_SerialFormat_SerialText,
    OIIO_ImageSpec_SerialFormat_SerialXML,
};

enum OIIO_ImageSpec_SerialVerbose {
    OIIO_ImageSpec_SerialVerbose_SerialBrief,
    OIIO_ImageSpec_SerialVerbose_SerialDetailed,
    OIIO_ImageSpec_SerialVerbose_SerialDetailedHuman,
};

/// Serialize an OIIO_ImageSpec to a string
///
/// @param is the ImageSpec to serialize
/// @param format The format for the serialized string
/// @param verbose The verbosity level of the serialized string
/// @param string_buffer Caller-provided storage for the generated string
/// @param buffer_length Size of the provided string buffer. If the size of the
///                      provided storage is not large enough to hold the entire
///                      generated string, the string will be truncated to fit.
///
OIIO_API void
OIIO_ImageSpec_serialize(const OIIO_ImageSpec* is,
                         OIIO_ImageSpec_SerialFormat format,
                         OIIO_ImageSpec_SerialVerbose verbose,
                         char* string_buffer, int buffer_length);

/// Converts the contents of the ImageSpec to an XML string
///
/// @param is the ImageSpec to serialize
/// @param string_buffer Caller-provided storage for the generated string
/// @param buffer_length Size of the provided string buffer. If the size of the
///                      provided storage is not large enough to hold the entire
///                      generated string, the string will be truncated to fit.
///
OIIO_API void
OIIO_ImageSpec_to_xml(const OIIO_ImageSpec* is, char* string_buffer,
                      int buffer_length);


/// Populates the fields of the `ImageSpec` based on the XML passed in.
///
/// Equivalent C++: `is->from_xml(xml)`
OIIO_API void
OIIO_ImageSpec_from_xml(OIIO_ImageSpec* is, const char* xml);


/// TODO: (AL) decode_compression_metadata


/// Helper function to verify that the given pixel range exactly covers
/// a set of tiles.
///
/// Equivalent C++: `is->valid_tile_range(xbegin, xend, ybegin, yend, zbegin, zend)`
///
OIIO_API bool
OIIO_ImageSpec_valid_tile_range(OIIO_ImageSpec* is, int xbegin, int xend,
                                int ybegin, int yend, int zbegin, int zend);

/// Return the format of the given channel. This is safe even if
/// channelformats is not filled out.
///
/// Equivalent C++: `is->channelformat(chan)`
///
OIIO_API OIIO_TypeDesc
OIIO_ImageSpec_channelformat(const OIIO_ImageSpec* is, int chan);

/// Return the channel name of the given channel. This is safe even if
/// channelnames is not filled out.
///
/// Equivalent C++: `is->channel_name(chan)`
///
OIIO_API const char*
OIIO_ImageSpec_channel_name(const OIIO_ImageSpec* is, int chan);

/// Fill the provided array with OIIO_TypeDesc describing all channels in the
/// image. The array must have been allocated by the caller to hold nchannels
/// OIIO_TypeDesc structures
OIIO_API void
OIIO_ImageSpec_get_channelformats(const OIIO_ImageSpec* is,
                                  OIIO_TypeDesc* formats);

/// Return the index of the channel with the given name, or -1 if no
/// such channel is present in `channelnames`.
///
/// Equivalent C++: `is->channelindex(name)`
///
OIIO_API int
OIIO_ImageSpec_channelindex(const OIIO_ImageSpec* is, const char* name);

/// Return pixel data window for this ImageSpec expressed as an ROI
///
/// Equivalent C++: `is->roi()`
///
OIIO_API OIIO_ROI
OIIO_ImageSpec_roi(const OIIO_ImageSpec* is);

/// Return pixel display window for this ImageSpec expressed as an ROI
///
/// Equivalent C++: `is->roi_full()`
///
OIIO_API OIIO_ROI
OIIO_ImageSpec_roi_full(const OIIO_ImageSpec* is);

/// ImageInput abstracts the reading of an image file in a file
/// format-agnostic manner.
///
/// It is equivalent to the C++ ImageSpec, but is exposed to C as an opaque
/// pointer.
///
typedef struct OIIO_ImageInput OIIO_ImageInput;

/// Create an ImageInput subclass instance that is able to read the
/// given file and open it, returning a pointer to the ImageInput
/// if successful. If the open fails, returns NULL and set an error
/// that can be retrieved by `OIIO_geterror()`.
///
/// Equivalent C++: `ImageInput::open(filename, config, ioproxy)`
///
/// FIXME: add Filesystem::IOProxy
///
OIIO_API OIIO_ImageInput*
OIIO_ImageInput_open(const char* filename, const OIIO_ImageSpec* config,
                     OIIO_Filesystem_IOProxy* ioproxy);

/// Deletes an ImageInput, automatically closing the file if it's open
///
/// Equivalent C++: `delete ii`
///
OIIO_API void
OIIO_ImageInput_delete(OIIO_ImageInput* ii);

/// Return a pointer to the image specification of the current
/// subimage/MIPlevel.  Note that the contents of the spec are invalid
/// before `open()` or after `close()`, and may change with a call to
/// `seek_subimage()`. It is thus not thread-safe, since the spec may
/// change if another thread calls `seek_subimage`, or any of the
/// `read_*()` functions that take explicit subimage/miplevel.
///
/// Note that the ImageInput owns the ImageSpec and the caller must not delete
/// it.
///
/// Equivalent C++: `ii->spec()`
///
OIIO_API const OIIO_ImageSpec*
OIIO_ImageInput_spec(OIIO_ImageInput* ii);


/// Return a full copy of the ImageSpec of the designated subimage and
/// MIPlevel. This method is thread-safe, but it is potentially
/// expensive, due to the work that needs to be done to fully copy an
/// ImageSpec if there is lots of named metadata to allocate and copy. The C
/// API must also invoke an additional copy to put the copied ImageSpec on the
/// heap before returning it.
///
/// The caller is responsible for destroying this object.
///
/// Equivalent C++: `ii->spec(subimage, miplevel)`
///
OIIO_API OIIO_ImageSpec*
OIIO_ImageInput_spec_copy(OIIO_ImageInput* ii, int subimage, int miplevel);

/// Return a copy of the ImageSpec of the designated subimage and
/// miplevel, but only the dimension and type fields.
///
/// The caller is responsible for destroying this object.
///
/// Equivalent C++: `ii->spec_dimensions(subimage, miplevel)`
///
OIIO_API OIIO_ImageSpec*
OIIO_ImageInput_spec_dimensions(OIIO_ImageInput* ii, int subimage,
                                int miplevel);

/// Close an open ImageInput. The call to close() is not strictly
/// necessary if the ImageInput is destroyed immediately afterwards,
/// since it is required for the destructor to close if the file is
/// still open.
///
/// Equivalent C++: `ii->close()`
///
OIIO_API bool
OIIO_ImageInput_close(OIIO_ImageInput* ii);

/// Returns the index of the subimage that is currently being read.
/// The first subimage (or the only subimage, if there is just one)
/// is number 0.
///
/// Equivalent C++: `ii->current_subimage()`
///
OIIO_API int
OIIO_ImageInput_current_subimage(OIIO_ImageInput* ii);

/// Returns the index of the MIPmap image that is currently being read.
/// The highest-res MIP level (or the only level, if there is just
/// one) is number 0.
///
/// Equivalent C++: `ii->current_miplevel()`
///
OIIO_API int
OIIO_ImageInput_current_miplevel(OIIO_ImageInput* ii);

/// Seek to the given subimage and MIP-map level within the open image
/// file.
///
/// Equivalent C++: `ii->seek_subimage(subimage, miplevel)`
///
OIIO_API bool
OIIO_ImageInput_seek_subimage(OIIO_ImageInput* ii, int subimage, int miplevel);

/// Read the scanline that includes pixels (*,y,z) from the "current"
/// subimage and MIP level.  The `xstride` value gives the distance
/// between successive pixels (in bytes).  Strides set to `AutoStride`
/// imply "contiguous" data.
///
/// Equivalent C++: `ii->read_scanline(y, z, format, data, xstride)`
///
OIIO_API bool
OIIO_ImageInput_read_scanline(OIIO_ImageInput* ii, int y, int z,
                              OIIO_TypeDesc format, void* data,
                              stride_t xstride);

/// Read multiple scanlines that include pixels (*,y,z) for all ybegin
/// <= y < yend in the specified subimage and mip level, into `data`,
/// using the strides given and converting to the requested data
/// `format` (TypeUnknown indicates no conversion, just copy native data
/// types). Only channels [chbegin,chend) will be read/copied
/// (chbegin=0, chend=spec.nchannels reads all channels, yielding
/// equivalent behavior to the simpler variant of `read_scanlines`).
///
/// Equivalent C++: `ii->read_scanlines(subimage, miplevel, ybegin, yend, z, chbegin, chend, format, data, xstride, ystride)`
///
OIIO_API bool
OIIO_ImageInput_read_scanlines(OIIO_ImageInput* ii, int subimage, int miplevel,
                               int ybegin, int yend, int z, int chbegin,
                               int chend, OIIO_TypeDesc format, void* data,
                               stride_t xstride, stride_t ystride);

/// Read the tile whose upper-left origin is (x,y,z) into `data[]`,
/// converting if necessary from the native data format of the file into
/// the `format` specified. The stride values give the data spacing of
/// adjacent pixels, scanlines, and volumetric slices (measured in
/// bytes). Strides set to AutoStride imply 'contiguous' data in the
/// shape of a full tile.
///
/// Equivalent C++: `ii->read_tile(x, y, z format, data, xstride, ystride, zstride)`
///
OIIO_API bool
OIIO_ImageInput_read_tile(OIIO_ImageInput* ii, int x, int y, int z,
                          OIIO_TypeDesc format, void* data, stride_t xstride,
                          stride_t ystride, stride_t zstride);

/// Read the block of multiple tiles that include all pixels in
///
///     [xbegin,xend) X [ybegin,yend) X [zbegin,zend)
///
/// This is analogous to calling `read_tile(x,y,z,...)` for each tile
/// in turn (but for some file formats, reading multiple tiles may allow
/// it to read more efficiently or in parallel).
///
/// Equivalent C++: `ii->read_tiles(subimage, miplevel, xbegin, xend, ybegin,
///                                 yend, zbegin, zend, chbegin, chend, format,
///                                 data, xstride, ystride, zstride)`
///
OIIO_API bool
OIIO_ImageInput_read_tiles(OIIO_ImageInput* ii, int subimage, int miplevel,
                           int xbegin, int xend, int ybegin, int yend,
                           int zbegin, int zend, int chbegin, int chend,
                           OIIO_TypeDesc format, void* data, stride_t xstride,
                           stride_t ystride, stride_t zstride);

/// Read deep scanlines containing pixels (*,y,z), for all y in the
/// range [ybegin,yend) into `deepdata`. This will fail if it is not a
/// deep file.
///
/// Equivalent C++: `ii->read_native_deep_scanlines(subimage, miplevel,
///                         ybegin, yend, z, chbegin, chend, *deepdata)
///
OIIO_API bool
OIIO_ImageInput_read_native_deep_scanlines(OIIO_ImageInput* ii, int subimage,
                                           int miplevel, int ybegin, int yend,
                                           int z, int chbegin, int chend,
                                           OIIO_DeepData* deepdata);

/// Read deep scanlines containing pixels (*,y,z), for all y in the
/// range [ybegin,yend) into `deepdata`. This will fail if it is not a
/// deep file.
///
/// Equivalent C++: `ii->read_native_deep_tiles(subimage, miplevel,
///                         xbegin, xend, ybegin, yend, zbegin, zend,
///                         chbegin, chend, *deepdata)`
///
OIIO_API bool
OIIO_ImageInput_read_native_deep_tiles(OIIO_ImageInput* ii, int subimage,
                                       int miplevel, int xbegin, int xend,
                                       int ybegin, int yend, int zbegin,
                                       int zend, int chbegin, int chend,
                                       OIIO_DeepData* deepdata);

/// Read deep scanlines containing pixels (*,y,z), for all y in the
/// range [ybegin,yend) into `deepdata`. This will fail if it is not a
/// deep file.
///
/// Equivalent C++: `ii->read_native_deep_image(subimage, miplevel, *deepdata)`
///
OIIO_API bool
OIIO_ImageInput_read_native_deep_image(OIIO_ImageInput* ii, int subimage,
                                       int miplevel, OIIO_DeepData* deepdata);

/// Read the entire image of `spec.width x spec.height x spec.depth`
/// pixels into a buffer with the given strides and in the desired
/// data format.
///
/// Equivalent C++: `ii->read_image(subimage, miplevel, chbegin, chend, format
///                                 data, xstride, ystride, zstride,
///                                 progress_callback, progress_callbackd_data)`
///
OIIO_API bool
OIIO_ImageInput_read_image(OIIO_ImageInput* ii, int subimage, int miplevel,
                           int chbegin, int chend, OIIO_TypeDesc format,
                           void* data, stride_t xstride, stride_t ystride,
                           stride_t zstride,
                           OIIO_ProgressCallback progress_callback,
                           void* progress_callback_data);

/// Is there a pending error message waiting to be retrieved?
///
/// Equivalent C++: `ii->has_error()`
///
OIIO_API bool
OIIO_ImageInput_has_error(const OIIO_ImageInput* ii);

/// Return the text of all pending error messages issued against this ImageInput
/// that have been raised from this thread and optionally clear the pending error
/// message.
/// If no error message is pending, it will return an empty string.
///
/// @param ii               ImageInput to get the error message for.
/// @param msg              Caller-allocated storage for the error message string.
/// @param buffer_length    The size of the provided string storage, including
///                         the terminating null character. If the returned error
///                         string is longer than this, it will be truncated.
/// @param clear            If true, clear the internal error message before returning.
///
OIIO_API void
OIIO_ImageInput_geterror(const OIIO_ImageInput* ii, char* msg,
                         int buffer_length, bool clear);

/// ImageOutput abstracts the writing of an image file in a file
/// format-agnostic manner.
///
/// It is equivalent to the C++ ImageSpec, but is exposed to C as an opaque
/// pointer.
///
typedef struct OIIO_ImageOutput OIIO_ImageOutput;

/// Modes pass to the `OIIO_ImageOutput_open()` call
enum OIIO_ImageOutput_OpenMode {
    OIIO_ImageOutput_OpenMode_Create,
    OIIO_ImageOutput_OpenMode_AppendSubImage,
    OIIO_ImageOutput_OpenMode_AppendMipLevel,
};

/// Create an `ImageOutput` that can be used to write an image file.
/// The type of image file (and hence, the particular subclass of
/// `ImageOutput` returned, and the plugin that contains its methods) is
/// inferred from the name, if it appears to be a full filename, or it
/// may also name the format.
///
/// Equivalent C++: `ImageOutput::create(filename, ioproxy, plugin_search_path)`
///
OIIO_API OIIO_ImageOutput*
OIIO_ImageOutput_create(const char* filename, OIIO_Filesystem_IOProxy* ioproxy,
                        const char* plugin_search_path);

/// Call delete on the ImageOutput*, closing any open files
///
/// Equivalent C++: `delete io`
///
OIIO_API void
OIIO_ImageOutput_delete(OIIO_ImageOutput* io);

/// Open the file with given name, with resolution and other format
/// data as given in newspec.
///
/// Equivalent C++: `io->open(name, newspec, mode)`
///
OIIO_API bool
OIIO_ImageOutput_open(OIIO_ImageOutput* io, const char* name,
                      const OIIO_ImageSpec* newspec, int mode);

/// Is there a pending error message waiting to be retrieved?
///
/// Equivalent C++: `io->has_error()`
///
OIIO_API bool
OIIO_ImageOutput_has_error(const OIIO_ImageOutput* io);

/// Return the text of all pending error messages issued against this ImageOutput,
/// and optionally clear the pending error message.
/// If no error message is pending, it will return an empty string.
///
/// @param io               ImageOutput to get the error message for.
/// @param msg              Caller-allocated storage for the error message string.
/// @param buffer_length    The size of the provided string storage, including
///                         the terminating null character. If the returned error
///                         string is longer than this, it will be truncated.
/// @param clear            If true, clear the internal error message before returning.
///
OIIO_API void
OIIO_ImageOutput_geterror(const OIIO_ImageOutput* io, char* msg,
                          int buffer_length, bool clear);

/// Write the entire image of `spec.width x spec.height x spec.depth`
/// pixels, from a buffer with the given strides and in the desired
/// format.
///
/// Depending on the spec, this will write either all tiles or all
/// scanlines. Assume that data points to a layout in row-major order.
///
/// Equivalent C++: `io->write_image(format, data, xstride, ystride, zstride,
///                                     progress_callback, progress_callback_data)`
///
OIIO_API bool
OIIO_ImageOutput_write_image(OIIO_ImageOutput* io, OIIO_TypeDesc format,
                             const void* data, stride_t xstride,
                             stride_t ystride, stride_t zstride,
                             OIIO_ProgressCallback progress_callback,
                             void* progress_callback_data);

// Global utility functions

/// Returns a numeric value for the version of OpenImageIO, 10000 for each
/// major version, 100 for each minor version, 1 for each patch.  For
/// example, OpenImageIO 1.2.3 would return a value of 10203. One example of
/// how this is useful is for plugins to query the version to be sure they
/// are linked against an adequate version of the library.
OIIO_API int
openimageio_version();

/// Is there a pending global error message waiting to be retrieved?
OIIO_API bool
openimageio_haserror();

/// Returns any error string describing what went wrong if
/// `ImageInput_create()` or `ImageOutput_create()` failed (since in such
/// cases, the ImageInput or ImageOutput itself does not exist to have its
/// own `geterror()` function called). This function returns the last error
/// for this particular thread, and clear the pending error message unless
/// `clear` is false; separate threads will not clobber each other's global
/// error messages.
///
/// @param msg              Caller-allocated storage for the error message string.
/// @param buffer_length    The size of the provided string storage, including
///                         the terminating null character. If the returned error
///                         string is longer than this, it will be truncated.
/// @param clear            If true, clear the internal error message before returning.
///
OIIO_API void
openimageio_geterror(char* msg, int buffer_length, bool clear);

#ifdef __cplusplus
}
#endif

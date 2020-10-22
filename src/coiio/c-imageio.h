// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio


#pragma once

#include "c-imageio_defines.h"
#include "c-typedesc.h"

#ifdef __cplusplus
extern "C" {
#endif

/// ImageSpec describes the data format of an image -- dimensions, layout,
/// number and meanings of image channels.
///
/// It is equivalent to the C++ ImageSpec, but is exposed to C as an opaque
/// pointer.
///
typedef struct ImageSpec ImageSpec;

/// Create a new ImageSpec. The caller takes ownership and is responsible for
/// calling ImageSpec_delete.
///
/// Equivalent C++: `new ImageSpec`
///
OIIO_API ImageSpec*
ImageSpec_new();

/// Delete an ImageSpec
///
/// Equivalent C++: `delete is;`
///
OIIO_API void
ImageSpec_delete(const ImageSpec* is);

/// Create a new ImageSpec with explicit dimensions
///
/// Equivalent C++: `new ImageSpec(xres, yres, nchans, fmt)`
///
OIIO_API ImageSpec*
ImageSpec_new_with_dimensions(int xres, int yres, int nchans, TypeDesc fmt);

/// Create a copy of this ImageSpec with the default copy constructor
///
/// Equivalent C++: `new ImageSpec(*is)`
///
OIIO_API ImageSpec*
ImageSpec_copy(const ImageSpec* is);

/// Width of the pixel data window
///
/// Equivalent C++: `is->width`
///
OIIO_API int
ImageSpec_width(const ImageSpec* is);

/// Height of the pixel data window
///
/// Equivalent C++: `is->height`
///
OIIO_API int
ImageSpec_height(const ImageSpec* is);

/// Number of image channels, e.g. 4 for RGBA
///
/// Equivalent C++: `is->nchannels`
///
OIIO_API int
ImageSpec_nchannels(const ImageSpec* is);

/// Return the channel name of the given channel. This is safe even if
/// channelnames is not filled out.
///
/// Equivalent C++: `is->channel_name(chan)`
///
OIIO_API const char*
ImageSpec_channel_name(const ImageSpec* is, int chan);

/// Add a metadata attribute to `extra_attribs`, with the given name and
/// data type. The `value` pointer specifies the address of the data to
/// be copied.
///
/// Equivalent C++: `is->attribute(name, fmt, value)`
///
OIIO_API void
ImageSpec_attribute(ImageSpec* is, const char* name, TypeDesc fmt,
                    const void* value);


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
ImageSpec_getattribute(const ImageSpec* is, const char* name, TypeDesc type,
                       void* value, bool casesensitive);

/// ImageInput abstracts the reading of an image file in a file
/// format-agnostic manner.
///
/// It is equivalent to the C++ ImageSpec, but is exposed to C as an opaque
/// pointer.
///
typedef struct ImageInput ImageInput;

/// Create an ImageInput subclass instance that is able to read the
/// given file and open it, returning a pointer to the ImageInput
/// if successful. If the open fails, returns NULL and set an error
/// that can be retrieved by `OIIO_geterror()`.
///
/// Equivalent C++: `ImageInput::open(filename, config, ioproxy)`
///
/// FIXME: add Filesystem::IOProxy
///
OIIO_API ImageInput*
ImageInput_open(const char* filename, const ImageSpec* config, void* ioproxy);

/// Close an open ImageInput. The call to close() is not strictly
/// necessary if the ImageInput is destroyed immediately afterwards,
/// since it is required for the destructor to close if the file is
/// still open.
///
/// Equivalent C++: `ii->close()`
///
OIIO_API bool
ImageInput_close(ImageInput* ii);

/// Deletes an ImageInput, automatically closing the file if it's open
///
/// Equivalent C++: `delete ii`
///
OIIO_API void
ImageInput_delete(ImageInput* ii);

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
OIIO_API const ImageSpec*
ImageInput_spec(ImageInput* ii);

/// Read the entire image of `spec.width x spec.height x spec.depth`
/// pixels into a buffer with the given strides and in the desired
/// data format.
///
/// Equivalent C++: `ii->read_image(subimage, miplevel, chbegin, chend, format
///                                 data, xstride, ystride, zstride,
///                                 progress_callback, progress_callbackd_data)`
///
OIIO_API bool
ImageInput_read_image(ImageInput* ii, int subimage, int miplevel, int chbegin,
                      int chend, TypeDesc format, void* data, stride_t xstride,
                      stride_t ystride, stride_t zstride,
                      ProgressCallback progress_callback,
                      void* progress_callback_data);

/// Is there a pending error message waiting to be retrieved?
///
/// Equivalent C++: `ii->has_error()`
///
OIIO_API bool
ImageInput_has_error(const ImageInput* ii);

/// Return the text of all pending error messages issued against this ImageInput,
/// and clear the pending error message.
/// If no error message is pending, it will return an empty string.
///
OIIO_API const char*
ImageInput_geterror(const ImageInput* ii);

/// ImageOutput abstracts the writing of an image file in a file
/// format-agnostic manner.
///
/// It is equivalent to the C++ ImageSpec, but is exposed to C as an opaque
/// pointer.
///
typedef struct ImageOutput ImageOutput;

/// Create an `ImageOutput` that can be used to write an image file.
/// The type of image file (and hence, the particular subclass of
/// `ImageOutput` returned, and the plugin that contains its methods) is
/// inferred from the name, if it appears to be a full filename, or it
/// may also name the format.
///
/// Equivalent C++: `ImageOutput::create(filename, ioproxy, plugin_search_path)`
///
OIIO_API ImageOutput*
ImageOutput_create(const char* filename, void* ioproxy,
                   const char* plugin_search_path);

/// Call delete on the ImageOutput*, closing any open files
///
/// Equivalent C++: `delete io`
///
OIIO_API void
ImageOutput_delete(ImageOutput* io);

/// Open the file with given name, with resolution and other format
/// data as given in newspec.
///
/// Equivalent C++: `io->open(name, newspec, mode)`
///
OIIO_API bool
ImageOutput_open(ImageOutput* io, const char* name, const ImageSpec* newspec,
                 int mode);

/// Is there a pending error message waiting to be retrieved?
///
/// Equivalent C++: `io->has_error()`
///
OIIO_API bool
ImageOutput_has_error(const ImageOutput* io);

/// Return the text of all pending error messages issued against this ImageOutput,
/// and clear the pending error message.
/// If no error message is pending, it will return an empty string.
///
OIIO_API const char*
ImageOutput_geterror(const ImageOutput* io);

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
ImageOutput_write_image(ImageOutput* io, TypeDesc format, const void* data,
                        stride_t xstride, stride_t ystride, stride_t zstride,
                        ProgressCallback progress_callback,
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
OIIO_API const char*
openimageio_geterror(bool clear);

#ifdef __cplusplus
}
#endif

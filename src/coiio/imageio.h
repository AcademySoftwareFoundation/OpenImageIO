#pragma once

#include "imageio_defines.h"
#include "typedesc.h"

#ifdef __cplusplus
extern "C" {
#endif

/// ImageSpec describes the data format of an image -- dimensions, layout,
/// number and meanings of image channels.
///
/// The `width, height, depth` are the size of the data of this image, i.e.,
/// the number of pixels in each dimension.  A ``depth`` greater than 1
/// indicates a 3D "volumetric" image. The `x, y, z` fields indicate the
/// *origin* of the pixel data of the image. These default to (0,0,0), but
/// setting them differently may indicate that this image is offset from the
/// usual origin.
/// Therefore the pixel data are defined over pixel coordinates
///    [`x` ... `x+width-1`] horizontally,
///    [`y` ... `y+height-1`] vertically,
///    and [`z` ... `z+depth-1`] in depth.
///
/// The analogous `full_width`, `full_height`, `full_depth` and `full_x`,
/// `full_y`, `full_z` fields define a "full" or "display" image window over
/// the region [`full_x` ... `full_x+full_width-1`] horizontally, [`full_y`
/// ... `full_y+full_height-1`] vertically, and [`full_z`...
/// `full_z+full_depth-1`] in depth.
///
/// Having the full display window different from the pixel data window can
/// be helpful in cases where you want to indicate that your image is a
/// *crop window* of a larger image (if the pixel data window is a subset of
/// the full display window), or that the pixels include *overscan* (if the
/// pixel data is a superset of the full display window), or may simply
/// indicate how different non-overlapping images piece together.
///
/// For tiled images, `tile_width`, `tile_height`, and `tile_depth` specify
/// that the image is stored in a file organized into rectangular *tiles*
/// of these dimensions. The default of 0 value for these fields indicates
/// that the image is stored in scanline order, rather than as tiles.
///

struct ImageSpec;

/// Create a new ImageSpec
OIIO_API ImageSpec*
ImageSpec_new();

/// Delete an ImageSpec
OIIO_API void
ImageSpec_delete(const ImageSpec* is);

/// Create a new ImageSpec with explicit dimensions
OIIO_API ImageSpec*
ImageSpec_new_with_dimensions(int xres, int yres, int nchans, TypeDesc fmt);

/// Create a copy of this ImageSpec with the default copy constructor
OIIO_API ImageSpec*
ImageSpec_copy(const ImageSpec* is);

/// Width of the pixel data window
OIIO_API int
ImageSpec_width(const ImageSpec* is);

/// Height of the pixel data window
OIIO_API int
ImageSpec_height(const ImageSpec* is);

/// Number of image channels, e.g. 4 for RGBA
OIIO_API int
ImageSpec_nchannels(const ImageSpec* is);

/// Return the channel name of the given channel. This is safe even if
/// channelnames is not filled out.
OIIO_API const char*
ImageSpec_channel_name(const ImageSpec* is, int chan);

/// Add a metadata attribute to `extra_attribs`, with the given name and
/// data type. The `value` pointer specifies the address of the data to
/// be copied.
OIIO_API void
ImageSpec_attribute(ImageSpec* is, const char* name, TypeDesc fmt,
                    const void* value);


/// If the `ImageSpec` contains the named attribute and its type matches
/// `type`, copy the attribute value into the memory pointed to by `val`
/// (it is up to the caller to ensure there is enough space) and return
/// `true`. If no such attribute is found, or if it doesn't match the
/// type, return `false` and do not modify `val`.
///
/// EXAMPLES:
///
///     ImageSpec spec;
///     ...
///     // Retrieving an integer attribute:
///     int orientation = 0;
///     spec.getattribute ("orientation", TypeInt, &orientation);
///
///     // Retrieving a string attribute with a char*:
///     const char* compression = nullptr;
///     spec.getattribute ("compression", TypeString, &compression);
///
///     // Alternately, retrieving a string with a ustring:
///     ustring compression;
///     spec.getattribute ("compression", TypeString, &compression);
///
/// Note that when passing a string, you need to pass a pointer to the
/// `char*`, not a pointer to the first character.  Also, the `char*`
/// will end up pointing to characters owned by the `ImageSpec`; the
/// caller does not need to ever free the memory that contains the
/// characters.
OIIO_API bool
ImageSpec_getattribute(const ImageSpec* is, const char* name, TypeDesc type,
                       void* value, bool casesensitive);

/// ImageInput abstracts the reading of an image file in a file
/// format-agnostic manner.
struct ImageInput;

/// @{
/// @name Creating an ImageInput

/// Create an ImageInput subclass instance that is able to read the
/// given file and open it, returning a pointer to the ImageInput
/// if successful. If the open fails, returns NULL and set an error
/// that can be retrieved by `OIIO_geterror()`.
///
/// The `config`, if not NULL, points to an ImageSpec giving hints,
/// requests, or special instructions.  ImageInput implementations are
/// free to not respond to any such requests, so the default
/// implementation is just to ignore `config`.
///
/// `open()` will first try to make an ImageInput corresponding to
/// the format implied by the file extension (for example, `"foo.tif"`
/// will try the TIFF plugin), but if one is not found or if the
/// inferred one does not open the file, every known ImageInput type
/// will be tried until one is found that will open the file.
///
/// @param filename
///         The name of the file to open.
///
/// @param config
///         Optional pointer to an ImageSpec whose metadata contains
///         "configuration hints."
///
/// @param ioproxy
///         Optional pointer to an IOProxy to use (not supported by all
///         formats, see `supports("ioproxy")`). The caller retains
///         ownership of the proxy.
///
/// @returns
///         A `unique_ptr` that will close and free the ImageInput when
///         it exits scope or is reset. The pointer will be empty if the
///         required writer was not able to be created.
// FIXME: add Filesystem::IOProxy
// FIXME: The C++ docs say it returns a unique_ptr that has a custom deleter
// for closing the file but I cannot see that in the code, so callers are still
// responsible for closing the file as well when done?
OIIO_API ImageInput*
ImageInput_open(const char* filename, const ImageSpec* config, void* ioproxy);

/// Close an open ImageInput. The call to close() is not strictly
/// necessary if the ImageInput is destroyed immediately afterwards,
/// since it is required for the destructor to close if the file is
/// still open.
///
/// @returns
///         `true` upon success, or `false` upon failure.
OIIO_API bool
ImageInput_close(ImageInput* ii);

/// Deletes an ImageInput, automatically closing the file if it's open
OIIO_API void
ImageInput_delete(ImageInput* ii);

/// Return a pointer to the image specification of the current
/// subimage/MIPlevel.  Note that the contents of the spec are invalid
/// before `open()` or after `close()`, and may change with a call to
/// `seek_subimage()`. It is thus not thread-safe, since the spec may
/// change if another thread calls `seek_subimage`, or any of the
/// `read_*()` functions that take explicit subimage/miplevel.
OIIO_API const ImageSpec*
ImageInput_spec(ImageInput* ii);

/// Read the entire image of `spec.width x spec.height x spec.depth`
/// pixels into a buffer with the given strides and in the desired
/// data format.
///
/// Depending on the spec, this will read either all tiles or all
/// scanlines. Assume that data points to a layout in row-major order.
///
/// This version of read_image, because it passes explicit subimage and
/// miplevel, does not require a separate call to seek_subimage, and is
/// guaranteed to be thread-safe against other concurrent calls to any
/// of the read_* methods that take an explicit subimage/miplevel (but
/// not against any other ImageInput methods).
///
/// Because this may be an expensive operation, a progress callback
/// may be passed.  Periodically, it will be called as follows:
///
///     progress_callback (progress_callback_data, float done);
///
/// where `done` gives the portion of the image (between 0.0 and 1.0)
/// that has been written thus far.
///
/// @param  subimage    The subimage to read from (starting with 0).
/// @param  miplevel    The MIP level to read (0 is the highest
///                     resolution level).
/// @param  chbegin/chend
///                     The channel range to read.
/// @param  format      A TypeDesc describing the type of `data`.
/// @param  data        Pointer to the pixel data.
/// @param  xstride/ystride/zstride
///                     The distance in bytes between successive pixels,
///                     scanlines, and image planes (or `AutoStride`).
/// @param  progress_callback/progress_callback_data
///                     Optional progress callback.
/// @returns            `true` upon success, or `false` upon failure.
OIIO_API bool
ImageInput_read_image(ImageInput* ii, int subimage, int miplevel, int chbegin,
                      int chend, TypeDesc format, void* data, stride_t xstride,
                      stride_t ystride, stride_t zstride,
                      ProgressCallback progress_callback,
                      void* progress_callback_data);

/// Is there a pending error message waiting to be retrieved?
OIIO_API bool
ImageInput_has_error(const ImageInput* ii);

/// Return the text of all pending error messages issued against this ImageInput,
/// and clear the pending error message.
/// If no error message is pending, it will return an empty string.
OIIO_API const char*
ImageInput_geterror(const ImageInput* ii);

/// ImageOutput abstracts the writing of an image file in a file
/// format-agnostic manner.
///
struct ImageOutput;

OIIO_API ImageOutput*
ImageOutput_create(const char* filename, void* ioproxy,
                   const char* plugin_search_path);

/// Call delete on the ImageOutput*, closing any open files
OIIO_API void
ImageOutput_delete(ImageOutput* io);

/// Open the file with given name, with resolution and other format
/// data as given in newspec. It is legal to call open multiple times
/// on the same file without a call to `close()`, if it supports
/// multiimage and mode is AppendSubimage, or if it supports
/// MIP-maps and mode is AppendMIPLevel -- this is interpreted as
/// appending a subimage, or a MIP level to the current subimage,
/// respectively.
///
/// @param  io          The ImageOutput to call open() on
/// @param  name        The name of the image file to open.
/// @param  newspec     The ImageSpec describing the resolution, data
///                     types, etc.
/// @param  mode        Specifies whether the purpose of the `open` is
///                     to create/truncate the file (default: `Create`),
///                     append another subimage (`AppendSubimage`), or
///                     append another MIP level (`AppendMIPLevel`).
/// @returns            `true` upon success, or `false` upon failure.
OIIO_API bool
ImageOutput_open(ImageOutput* io, const char* name, const ImageSpec* newspec,
                 int mode);

/// Is there a pending error message waiting to be retrieved?
OIIO_API bool
ImageOutput_has_error(const ImageOutput* io);

/// Return the text of all pending error messages issued against this ImageOutput,
/// and clear the pending error message.
/// If no error message is pending, it will return an empty string.
OIIO_API const char*
ImageOutput_geterror(const ImageOutput* io);

/// Write the entire image of `spec.width x spec.height x spec.depth`
/// pixels, from a buffer with the given strides and in the desired
/// format.
///
/// Depending on the spec, this will write either all tiles or all
/// scanlines. Assume that data points to a layout in row-major order.
///
/// Because this may be an expensive operation, a progress callback
/// may be passed.  Periodically, it will be called as follows:
///
///     progress_callback (progress_callback_data, float done);
///
/// where `done` gives the portion of the image (between 0.0 and 1.0)
/// that has been written thus far.
///
/// @param  io          The ImageOutput to write to
/// @param  format      A TypeDesc describing the type of `data`.
/// @param  data        Pointer to the pixel data.
/// @param  xstride/ystride/zstride
///                     The distance in bytes between successive pixels,
///                     scanlines, and image planes (or `AutoStride`).
/// @param  progress_callback/progress_callback_data
///                     Optional progress callback.
/// @returns            `true` upon success, or `false` upon failure.
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
/// `ImageInput::create()` or `ImageOutput::create()` failed (since in such
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

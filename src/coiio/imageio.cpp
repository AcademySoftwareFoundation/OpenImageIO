#include <OpenImageIO/imageio.h>

#include "imageio_defines.h"
#include "typedesc.h"
#include "util.hpp"

using ImageInput  = OIIO::ImageInput;
using ImageOutput = OIIO::ImageOutput;
using ImageSpec   = OIIO::ImageSpec;

extern "C" {

ImageSpec*
ImageSpec_new()
{
    return new ImageSpec;
}

void
ImageSpec_delete(const ImageSpec* is)
{
    delete is;
}

ImageSpec*
ImageSpec_new_with_dimensions(int xres, int yres, int nchans, TypeDesc fmt)
{
    return new ImageSpec(xres, yres, nchans, pun<OIIO::TypeDesc>(fmt));
}

ImageSpec*
ImageSpec_copy(const ImageSpec* ii)
{
    return new ImageSpec(*ii);
}

void
ImageSpec_attribute(ImageSpec* is, const char* name, TypeDesc fmt,
                    const void* value)
{
    is->attribute(name, pun<OIIO::TypeDesc>(fmt), value);
}

int
ImageSpec_width(const ImageSpec* is)
{
    return is->width;
}

int
ImageSpec_height(const ImageSpec* is)
{
    return is->height;
}

int
ImageSpec_nchannels(const ImageSpec* is)
{
    return is->nchannels;
}

const char*
ImageSpec_channel_name(const ImageSpec* is, int chan)
{
    return is->channel_name(chan).c_str();
}

bool
ImageSpec_getattribute(const ImageSpec* is, const char* name, TypeDesc type,
                       void* value, bool casesensitive)
{
    return is->getattribute(name, pun<OIIO::TypeDesc>(type), value,
                            casesensitive);
}

// FIXME: add Filesystem::IOProxy
ImageInput*
ImageInput_open(const char* filename, const ImageSpec* config, void* _ioproxy)
{
    auto ii = ImageInput::open(filename, config, nullptr);
    return ii.release();
}

bool
ImageInput_close(ImageInput* ii)
{
    return ii->close();
}

void
ImageInput_delete(ImageInput* ii)
{
    delete ii;
}

const ImageSpec*
ImageInput_spec(ImageInput* ii)
{
    return &ii->spec();
}

bool
ImageInput_read_image(ImageInput* ii, int subimage, int miplevel, int chbegin,
                      int chend, TypeDesc format, void* data, stride_t xstride,
                      stride_t ystride, stride_t zstride,
                      ProgressCallback progress_callback,
                      void* progress_callback_data)
{
    return ii->read_image(subimage, miplevel, chbegin, chend,
                          pun<OIIO::TypeDesc>(format), data, xstride, ystride,
                          zstride, progress_callback, progress_callback_data);
}

OIIO_API bool
ImageInput_has_error(const ImageInput* ii)
{
    return ii->has_error();
}

const char*
ImageInput_geterror(const ImageInput* ii)
{
    // If we have multiple ImageInputs that we want to get errors for, we're
    // still passing through this one function. We have to cache the string
    // in order to return a char*, so it needs to be thread_local to avoid issues.
    // We force clear here so that multiple calls from different ImageInputs
    // don't get each others' errors.
    thread_local std::string errorstring;
    errorstring = ii->geterror(true);
    return errorstring.c_str();
}

ImageOutput*
ImageOutput_create(const char* filename, void* ioproxy,
                   const char* plugin_search_path)
{
    return ImageOutput::create(filename, nullptr, plugin_search_path).release();
}

void
ImageOutput_delete(ImageOutput* io)
{
    delete io;
}

bool
ImageOutput_open(ImageOutput* io, const char* name, const ImageSpec* newspec,
                 int mode)
{
    return io->open(std::string(name), *newspec,
                    (OIIO::ImageOutput::OpenMode)mode);
}

OIIO_API bool
ImageOutput_has_error(const ImageOutput* io)
{
    return io->has_error();
}

const char*
ImageOutput_geterror(const ImageOutput* io)
{
    // If we have multiple ImageOutputs that we want to get errors for, we're
    // still passing through this one function. We have to cache the string
    // in order to return a char*, so it needs to be thread_local to avoid issues.
    // We force clear here so that multiple calls from different ImageOutputs
    // don't get each others' errors.
    thread_local std::string errorstring;
    errorstring = io->geterror(true);
    return errorstring.c_str();
}

bool
ImageOutput_write_image(ImageOutput* io, TypeDesc format, const void* data,
                        stride_t xstride, stride_t ystride, stride_t zstride,
                        ProgressCallback progress_callback,
                        void* progress_callback_data)
{
    return io->write_image(pun<OIIO::TypeDesc>(format), data, xstride, ystride,
                           zstride, progress_callback, progress_callback_data);
}
int
openimageio_version()
{
    return OIIO::openimageio_version();
}

bool
openimageio_haserror()
{
    return OIIO::has_error();
}

const char*
openimageio_geterror(bool clear)
{
    thread_local std::string errorstring;
    errorstring = OIIO::geterror(clear);
    return errorstring.c_str();
}
}
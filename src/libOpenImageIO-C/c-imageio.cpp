// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio


#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

#include "c-imageio_defines.h"
#include "c-typedesc.h"

using ImageInput  = OIIO::ImageInput;
using ImageOutput = OIIO::ImageOutput;
using ImageSpec   = OIIO::ImageSpec;
using OIIO::bit_cast;

extern "C" {

stride_t OIIO_AutoStride = 0x8000000000000000L;



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
    return new ImageSpec(xres, yres, nchans,
                         bit_cast<TypeDesc, OIIO::TypeDesc>(fmt));
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
    is->attribute(name, bit_cast<TypeDesc, OIIO::TypeDesc>(fmt), value);
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
    return is->getattribute(name, bit_cast<TypeDesc, OIIO::TypeDesc>(type),
                            value, casesensitive);
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
                          bit_cast<TypeDesc, OIIO::TypeDesc>(format), data,
                          xstride, ystride, zstride, progress_callback,
                          progress_callback_data);
}



bool
ImageInput_has_error(const ImageInput* ii)
{
    return ii->has_error();
}



void
ImageInput_geterror(const ImageInput* ii, char* msg, int buffer_length,
                    bool clear)
{
    std::string errorstring = ii->geterror(clear);
    int length = std::min(buffer_length, (int)errorstring.size() + 1);
    memcpy(msg, errorstring.c_str(), length);
    msg[length - 1] = '\0';
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



bool
ImageOutput_has_error(const ImageOutput* io)
{
    return io->has_error();
}



void
ImageOutput_geterror(const ImageOutput* io, char* msg, int buffer_length,
                     bool clear)
{
    std::string errorstring = io->geterror(clear);
    int length = std::min(buffer_length, (int)errorstring.size() + 1);
    memcpy(msg, errorstring.c_str(), length);
    msg[length - 1] = '\0';
}



bool
ImageOutput_write_image(ImageOutput* io, TypeDesc format, const void* data,
                        stride_t xstride, stride_t ystride, stride_t zstride,
                        ProgressCallback progress_callback,
                        void* progress_callback_data)
{
    return io->write_image(bit_cast<TypeDesc, OIIO::TypeDesc>(format), data,
                           xstride, ystride, zstride, progress_callback,
                           progress_callback_data);
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



void
openimageio_geterror(char* msg, int buffer_length, bool clear)
{
    std::string errorstring = OIIO::geterror(clear);
    int length = std::min(buffer_length, (int)errorstring.size() + 1);
    memcpy(msg, errorstring.c_str(), length);
    msg[length - 1] = '\0';
}
}

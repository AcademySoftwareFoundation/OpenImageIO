/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdarg>
#include <iostream>
#include <dlfcn.h>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include "dassert.h"
#include "paramtype.h"
#include "filesystem.h"
#include "plugin.h"
#include "thread.h"
#include "strutil.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing ImageIO */
#include "imageio.h"
#include "imageio_pvt.h"
#undef DLL_EXPORT_PUBLIC

using namespace OpenImageIO;




ImageOutput *
ImageOutput::create (const char *filename, const char *plugin_searchpath)
{
    if (!filename || !filename[0]) { // Can't even guess if no filename given
        OpenImageIO::error ("ImageOutput::create() called with no filename");
        return NULL;
    }

    // Extract the file extension from the filename
    std::string format = fs::extension (filename);
    if (format.empty()) {
        OpenImageIO::error ("ImageOutput::create() could not deduce the format from '%s'", filename);
        return NULL;
    }
    if (format[0] == '.')
        format.erase (format.begin());
    std::cerr << "extension of '" << filename << "' is '" << format << "'\n";

    // Try to create a format of the given extension
    return create_format (format.c_str(), plugin_searchpath);
}



ImageOutput *
ImageOutput::create_format (const char *fmt, const char *plugin_searchpath)
{
    if (!fmt || !fmt[0]) {
        OpenImageIO::error ("ImageOutput::create_format() called with no format");
        return NULL;
    }

    std::string plugin_filename = std::string (fmt) + ".imageio";
#if defined(WINDOWS)
    plugin_filename += ".dll";
#elif defined(__APPLE__)
    plugin_filename += ".dylib";
#else
    plugin_filename += ".so";
#endif

    std::string searchpath;
    const char *imageio_library_path = getenv ("IMAGEIO_LIBRARY_PATH");
    if (imageio_library_path)
        searchpath = imageio_library_path;
    if (plugin_searchpath) {
        if (searchpath.length())
            searchpath += ':';
        searchpath += std::string(plugin_searchpath);
    }
//    std::cerr << "  searchpath = '" << searchpath << "'\n";
    std::vector<std::string> dirs;
    Filesystem::searchpath_split (searchpath, dirs, true);
    std::string plugin_fullpath = Filesystem::searchpath_find (plugin_filename, dirs);
    if (plugin_fullpath.empty()) {
        OpenImageIO::error ("Plugin \"%s\" not found in searchpath \"%s\"",
                            plugin_filename.c_str(), searchpath.c_str());
        return NULL;
    }

    // FIXME -- threadsafe
    Plugin::Handle handle = Plugin::open (plugin_fullpath);
    if (handle) {
        std::cerr << "Succeeded in opening " << plugin_fullpath << "\n";
    } else {
        std::cerr << "Open of " << plugin_fullpath << " failed:\n" 
                  << Plugin::error_message() << "\n";
    }

    int *plugin_version = (int *) Plugin::getsym (handle, "imageio_version");
    if (! plugin_version) {
        OpenImageIO::error ("Plugin \"%s\" did not have 'imageio_version' symbol",
                            plugin_filename.c_str());
        Plugin::close (handle);
        return NULL;
    }
    std::string create_name = Strutil::format ("%s_output_imageio_create", fmt);
    create_prototype create_function = 
        (create_prototype) Plugin::getsym (handle, create_name);
    if (! create_function) {
        OpenImageIO::error ("Plugin \"%s\" did not have '%s' symbol",
                            create_name.c_str());
        Plugin::close (handle);
        return NULL;
    }
    
    return (ImageOutput *) create_function();

    // FIXME: If we've already loaded the DSO we need, use it
    // otherwise find one

    return NULL;
}



int
ImageOutput::send_to_output (const char *format, ...)
{
    // FIXME -- I can't remember how this is supposed to work
    return 0;
}



int
ImageOutput::send_to_client (const char *format, ...)
{
    // FIXME -- I can't remember how this is supposed to work
    return 0;
}



void
ImageOutput::error (const char *message, ...)
{
    va_list ap;
    va_start (ap, message);
    char buf[16384];     // FIXME -- do something more robust here
    vsnprintf (buf, sizeof(buf), message, ap);
    va_end (ap);
    m_errmessage = buf;
}



const void *
ImageOutput::to_native_scanline (ParamBaseType format,
                                 const void *data, int xstride,
                                 std::vector<unsigned char> &scratch)
{
    return to_native_rectangle (0, spec.width-1, 0, 0, 0, 0, format, data,
                                xstride, xstride*spec.width, 0, scratch);
}



const void *
ImageOutput::to_native_tile (ParamBaseType format, const void *data,
                             int xstride, int ystride, int zstride,
                             std::vector<unsigned char> &scratch)
{
    return to_native_rectangle (0, spec.tile_width-1, 0, spec.tile_height-1,
                                0, std::max(0,spec.tile_depth-1), format, data,
                                xstride, ystride, zstride, scratch);
}



const void *
ImageOutput::to_native_rectangle (int xmin, int xmax, int ymin, int ymax,
                                  int zmin, int zmax, 
                                  ParamBaseType format, const void *data,
                                  int xstride, int ystride, int zstride,
                                  std::vector<unsigned char> &scratch)
{
    // Compute width and height from the rectangle extents
    int width = xmax - xmin + 1;
    int height = ymax - ymin + 1;

    // Do the strides indicate that the data are already contiguous?
    bool contiguous = (xstride == spec.nchannels &&
                       ystride == spec.nchannels*width &&
                       (zstride == spec.nchannels*width*height || !zstride));
    // Is the only conversion we are doing that of data format?
    bool data_conversion_only =  (contiguous && spec.gamma == 1.0f);

    if (format == spec.format && data_conversion_only) {
        // Data are already in the native format, contiguous, and need
        // no gamma correction -- just return a ptr to the original data.
        return data;
    }

    int depth = zmax - zmin + 1;
    int rectangle_pixels = width * height * depth;
    int rectangle_values = rectangle_pixels * spec.nchannels;
    bool contiguoussize = contiguous ? 0 
                : rectangle_values * ParamBaseTypeSize(format);
    int rectangle_bytes = rectangle_pixels * spec.pixel_bytes();
    int floatsize = rectangle_values * sizeof(float);
    scratch.resize (contiguoussize + floatsize + rectangle_bytes);

    // Force contiguity if not already present
    if (! contiguous) {
        data = contiguize (data, spec.nchannels, xstride, ystride, zstride,
                           (void *)&scratch[0], width, height, depth, format);
        // Reset strides to indicate contiguous data
        xstride = spec.nchannels;
        ystride = spec.nchannels * width;
        zstride = spec.nchannels * width * height;
    }

    // Rather than implement the entire cross-product of possible
    // conversions, use float as an intermediate format, which generally
    // will always preserve enough precision.
    const float *buf;
    if (format == PT_FLOAT && spec.gamma == 1.0f) {
        // Already in float format and no gamma correction is needed --
        // leave it as-is.
        buf = (float *)data;
    } else {
        // Convert to from 'format' to float.
        buf = convert_to_float (data, (float *)&scratch[contiguoussize],
                                rectangle_values, format);
        // Now buf points to float
        if (spec.gamma != 1) {
            float invgamma = 1.0 / spec.gamma;
            float *f = (float *)buf;
            for (int p = 0;  p < rectangle_pixels;  ++p)
                for (int c = 0;  c < spec.nchannels;  ++c, ++f)
                    if (c != spec.alpha_channel)
                        *f = powf (*f, invgamma);
            // FIXME: we should really move the gamma correction to
            // happen immediately after contiguization.  That way,
            // byte->byte with gamma can use a table shortcut instead
            // of having to go through float just for gamma.
        }
        // Now buf points to gamma-corrected float
    }
    // Convert from float to native format.
    return convert_from_float (buf, &scratch[contiguoussize+floatsize], 
                       rectangle_pixels, spec.quant_black, spec.quant_white,
                       spec.quant_min, spec.quant_max, spec.quant_dither,
                       spec.format);
}

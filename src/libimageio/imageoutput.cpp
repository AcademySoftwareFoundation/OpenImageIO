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

#include <ImathFun.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include "dassert.h"
#include "paramtype.h"
#include "filesystem.h"
#include "plugin.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing ImageIO */
#include "imageio.h"
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
    std::cerr << "create_format '" << fmt << "'\n";
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
    std::cerr << "  searchpath = '" << searchpath << "'\n";
    std::vector<std::string> dirs;
    Filesystem::searchpath_split (searchpath, dirs, true);
    std::string plugin_fullpath = Filesystem::searchpath_find (plugin_filename, dirs);
    std::cerr << "Checkpoint 1\n";
    if (plugin_fullpath.empty()) {
        OpenImageIO::error ("Plugin \"%s\" not found in searchpath \"%s\"",
                            plugin_filename.c_str(), searchpath.c_str());
        return NULL;
    }

    std::cerr << "Checkpoint 2\n";
    // FIXME -- threadsafe
    Plugin::Handle handle = Plugin::open (plugin_fullpath);
    if (handle) {
        std::cerr << "Succeeded in opening " << plugin_fullpath << "\n";
    } else {
        std::cerr << "Open of " << plugin_fullpath << " failed:\n" 
                  << Plugin::error_message() << "\n";
    }



#if 0
    // FIXME
    std::string format;
    if (fmt && fmt[0])
        format = fmt;
    else {
        fs::path filename (fname);
        if (!fname || !fname[0])    // Can't create
            return NULL;
        format = fs::extension (filename);


    std::cerr << "extension of '" << filename << "' is '" << format << "'\n";
#endif

    // If we've already loaded the DSO we need, use it
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



int
ImageOutput::quantize (float value, int quant_black, int quant_white,
                       int quant_min, int quant_max, float quant_dither)
{
    value = Imath::lerp (quant_black, quant_white, value);
#if 0
    // FIXME
    if (quant_dither)
        value += quant_dither * (2.0f * rand() - 1.0f);
#endif
    return Imath::clamp ((int)(value + 0.5f), quant_min, quant_max);
}



float
ImageOutput::exposure (float value, float gain, float invgamma)
{
    if (invgamma != 1 && value >= 0)
        return powf (gain * value, invgamma);
    // Simple case - skip the expensive pow; also fall back to this
    // case for negative values, for which gamma makes no sense.
    return gain * value;
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
}

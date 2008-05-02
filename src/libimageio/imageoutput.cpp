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

#include <ImathFun.h>

#include "dassert.h"
#include "paramtype.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing ImageIO */
#include "imageio.h"
#undef DLL_EXPORT_PUBLIC

using namespace OpenImageIO;



/// Create an ImageOutput that will write to a file in the given
/// format.  The plugin_searchpath parameter is a colon-separated
/// list of directories to search for ImageIO plugin DSO/DLL's.
/// This just creates the ImageOutput, it does not open the file.
ImageOutput *
ImageOutput::create (const char *filename, const char *format,
                     const char *plugin_searchpath)
{
    // FIXME
    return NULL;
}



/// Create an ImageOutput that will write to a file, with the format
/// inferred from the extension of the file.  The plugin_searchpath
/// parameter is a colon-separated list of directories to search for
/// ImageIO plugin DSO/DLL's.  This just creates the ImageOutput, it
/// does not open the file.
ImageOutput *
ImageOutput::create (const char *filename, 
                     const char *plugin_searchpath)
{
    // FIXME
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

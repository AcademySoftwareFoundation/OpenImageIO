/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/regex.hpp>

#include <OpenEXR/half.h>

#include <cmath>

#include "OpenImageIO/imagebuf.h"
#include "OpenImageIO/imagebufalgo.h"
#include "OpenImageIO/imagebufalgo_util.h"
#include "OpenImageIO/dassert.h"
#include "OpenImageIO/sysutil.h"
#include "OpenImageIO/filter.h"
#include "OpenImageIO/thread.h"
#include "OpenImageIO/filesystem.h"

#ifdef USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif


OIIO_NAMESPACE_ENTER {


#ifdef USE_FREETYPE
namespace { // anon
static mutex ft_mutex;
static FT_Library ft_library = NULL;
static bool ft_broken = false;
static const char * default_font_name[] = {
        "cour", "Courier New", "FreeMono", NULL
     };
} // anon namespace
#endif


bool
ImageBufAlgo::render_text (ImageBuf &R, int x, int y, string_view text,
                           int fontsize, string_view font_,
                           const float *textcolor)
{
    if (R.spec().depth > 1) {
        R.error ("ImageBufAlgo::render_text does not support volume images");
        return false;
    }

#ifdef USE_FREETYPE
    // If we know FT is broken, don't bother trying again
    if (ft_broken)
        return false;

    // Thread safety
    lock_guard ft_lock (ft_mutex);
    int error = 0;

    // If FT not yet initialized, do it now.
    if (! ft_library) {
        error = FT_Init_FreeType (&ft_library);
        if (error) {
            ft_broken = true;
            R.error ("Could not initialize FreeType for font rendering");
            return false;
        }
    }

    // A set of likely directories for fonts to live, across several systems.
    std::vector<std::string> search_dirs;
    const char *home = getenv ("HOME");
    if (home && *home) {
        std::string h (home);
        search_dirs.push_back (h + "/fonts");
        search_dirs.push_back (h + "/Fonts");
        search_dirs.push_back (h + "/Library/Fonts");
    }
    const char *systemRoot = getenv ("SystemRoot");
    if (systemRoot && *systemRoot) {
        std::string sysroot (systemRoot);
        search_dirs.push_back (sysroot + "/Fonts");
    }
    search_dirs.push_back ("/usr/share/fonts");
    search_dirs.push_back ("/Library/Fonts");
    search_dirs.push_back ("C:/Windows/Fonts");
    search_dirs.push_back ("/opt/local/share/fonts");

    // Try to find the font.  Experiment with several extensions
    std::string font = font_;
    if (font.empty()) {
        // nothing specified -- look for something to use as a default.
        for (int j = 0;  default_font_name[j] && font.empty(); ++j) {
            static const char *extensions[] = { "", ".ttf", ".pfa", ".pfb", NULL };
            for (int i = 0;  font.empty() && extensions[i];  ++i)
                font = Filesystem::searchpath_find (std::string(default_font_name[j])+extensions[i],
                                                 search_dirs, true, true);
        }
        if (font.empty()) {
            R.error ("Could not set default font face");
            return false;
        }
    } else if (Filesystem::is_regular (font)) {
        // directly specified a filename -- use it
    } else {
        // A font name was specified but it's not a full path, look for it
        std::string f;
        static const char *extensions[] = { "", ".ttf", ".pfa", ".pfb", NULL };
        for (int i = 0;  f.empty() && extensions[i];  ++i)
            f = Filesystem::searchpath_find (font+extensions[i],
                                             search_dirs, true, true);
        if (f.empty()) {
            R.error ("Could not set font face to \"%s\"", font);
            return false;
        }
        font = f;
    }

    ASSERT (! font.empty());
    if (! Filesystem::is_regular (font)) {
        R.error ("Could not find font \"%s\"", font);
        return false;
    }

    FT_Face face;      // handle to face object
    error = FT_New_Face (ft_library, font.c_str(), 0 /* face index */, &face);
    if (error) {
        R.error ("Could not set font face to \"%s\"", font);
        return false;  // couldn't open the face
    }

    error = FT_Set_Pixel_Sizes (face,        // handle to face object
                                0,           // pixel_width
                                fontsize);   // pixel_heigh
    if (error) {
        FT_Done_Face (face);
        R.error ("Could not set font size to %d", fontsize);
        return false;  // couldn't set the character size
    }

    FT_GlyphSlot slot = face->glyph;  // a small shortcut
    int nchannels = R.spec().nchannels;
    float *pixelcolor = ALLOCA (float, nchannels);
    if (! textcolor) {
        float *localtextcolor = ALLOCA (float, nchannels);
        for (int c = 0;  c < nchannels;  ++c)
            localtextcolor[c] = 1.0f;
        textcolor = localtextcolor;
    }

    for (size_t n = 0, e = text.size();  n < e;  ++n) {
        // load glyph image into the slot (erase previous one)
        error = FT_Load_Char (face, text[n], FT_LOAD_RENDER);
        if (error)
            continue;  // ignore errors
        // now, draw to our target surface
        for (int j = 0;  j < static_cast<int>(slot->bitmap.rows); ++j) {
            int ry = y + j - slot->bitmap_top;
            for (int i = 0;  i < static_cast<int>(slot->bitmap.width); ++i) {
                int rx = x + i + slot->bitmap_left;
                float b = slot->bitmap.buffer[slot->bitmap.pitch*j+i] / 255.0f;
                R.getpixel (rx, ry, pixelcolor);
                for (int c = 0;  c < nchannels;  ++c)
                    pixelcolor[c] = b*textcolor[c] + (1.0f-b) * pixelcolor[c];
                R.setpixel (rx, ry, pixelcolor);
            }
        }
        // increment pen position
        x += slot->advance.x >> 6;
    }

    FT_Done_Face (face);
    return true;

#else
    R.error ("OpenImageIO was not compiled with FreeType for font rendering");
    return false;   // Font rendering not supported
#endif
}



} OIIO_NAMESPACE_EXIT

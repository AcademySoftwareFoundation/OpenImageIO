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


#include <string>
#include <boost/scoped_ptr.hpp>
#include <boost/tr1/memory.hpp>
using namespace std::tr1;

#include <ImathVec.h>
#include <ImathMatrix.h>
#include <half.h>

#include "dassert.h"
#include "typedesc.h"
#include "varyingref.h"
#include "ustring.h"
#include "hash.h"
#include "thread.h"
#include "fmath.h"
#include "imageio.h"
using namespace OpenImageIO;

#define DLL_EXPORT_PUBLIC /* Because we are implementing TextureSystem */
#include "texture.h"
#undef DLL_EXPORT_PUBLIC

#include "texture_pvt.h"
using namespace OpenImageIO::pvt;


namespace OpenImageIO {


static float default_blur = 0;
static float default_width = 1;
static float default_bias = 0;
static float default_fill = 0;

static TextureOptions defaultTextureOptions(true);  // use special ctr



/// Special private ctr that makes a canonical default TextureOptions.
/// For use internal to libtexture.  Users, don't call this!
TextureOptions::TextureOptions (bool)
    : firstchannel(0), nchannels(1),
      swrap(WrapDefault), twrap(WrapDefault),
      sblur(default_blur), tblur(default_blur),
      swidth(default_width), twidth(default_width),
      bias(default_bias),
      fill(default_fill),
      alpha(NULL),
      stateful(false),
      swrap_func(NULL), twrap_func(NULL)
{
    
}



TextureOptions::TextureOptions ()
{
    memcpy (this, &defaultTextureOptions, sizeof(*this));
}



static const char * wrap_type_name[] = {
    // MUST match the order of TextureOptions::Wrap
    "default", "black", "clamp", "periodic", "mirror",
    ""
};


TextureOptions::Wrap
TextureOptions::decode_wrapmode (const char *name)
{
    for (int i = 0;  i < (int)TextureOptions::WrapLast;  ++i)
        if (! strcmp (name, wrap_type_name[i]))
            return (TextureOptions::Wrap) i;
    return TextureOptions::WrapDefault;
}



void
TextureOptions::parse_wrapmodes (const char *wrapmodes,
                                 TextureOptions::Wrap &swrapcode,
                                 TextureOptions::Wrap &twrapcode)
{
    char *swrap = (char *) alloca (strlen(wrapmodes)+1);
    const char *twrap;
    int i;
    for (i = 0;  wrapmodes[i] && wrapmodes[i] != ',';  ++i)
        swrap[i] = wrapmodes[i];
    swrap[i] = 0;
    if (wrapmodes[i] == ',')
        twrap = wrapmodes + i+1;
    else twrap = swrap;
    swrapcode = decode_wrapmode (swrap);
    twrapcode = decode_wrapmode (twrap);
}




};  // end namespace OpenImageIO

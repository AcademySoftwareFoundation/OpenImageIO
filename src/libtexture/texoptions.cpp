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

#include <string>
#include <boost/scoped_ptr.hpp>
#include <boost/tr1/memory.hpp>
using namespace std::tr1;

#include "dassert.h"
#include "typedesc.h"
#include "varyingref.h"
#include "ustring.h"
#include "hash.h"
#include "thread.h"
#include "fmath.h"
#include "imageio.h"

#include "texture.h"


OIIO_NAMESPACE_ENTER
{


namespace {  // anonymous

static float default_blur = 0;
static float default_width = 1;
static float default_time = 0;
static float default_bias = 0;
static float default_fill = 0;
static int   default_samples = 1;

static const ustring wrap_type_name[] = {
    // MUST match the order of TextureOptions::Wrap
    ustring("default"), ustring("black"), ustring("clamp"),
    ustring("periodic"), ustring("mirror"),
    ustring()
};

};  // end anonymous namespace



/// Special private ctr that makes a canonical default TextureOptions.
/// For use internal to libtexture.  Users, don't call this!
TextureOptions::TextureOptions ()
    : firstchannel(0), nchannels(1), subimage(0),
      swrap(TextureOptions::WrapDefault), twrap(TextureOptions::WrapDefault),
      mipmode(TextureOptions::MipModeDefault),
      interpmode(TextureOptions::InterpSmartBicubic),
      anisotropic(32), conservative_filter(true),
      sblur(default_blur), tblur(default_blur),
      swidth(default_width), twidth(default_width),
      time(default_time),
      bias(default_bias),
      fill(default_fill),
      missingcolor(NULL),
      samples(default_samples),
      dresultds(NULL), dresultdt(NULL),
      rwrap(TextureOptions::WrapDefault),
      rblur(default_blur), rwidth(default_width),
      dresultdr(NULL),
      swrap_func(NULL), twrap_func(NULL), rwrap_func(NULL)
{
}



TextureOptions::TextureOptions (const TextureOpt &opt)
    : firstchannel(opt.firstchannel), nchannels(opt.nchannels),
      subimage(opt.subimage), subimagename(opt.subimagename),
      swrap((Wrap)opt.swrap), twrap((Wrap)opt.twrap),
      mipmode((MipMode)opt.mipmode),
      interpmode((InterpMode)opt.interpmode),
      anisotropic(opt.anisotropic),
      conservative_filter(opt.conservative_filter),
      sblur((float *)&opt.sblur), tblur((float *)&opt.tblur),
      swidth((float *)&opt.swidth), twidth((float *)&opt.twidth),
      time((float *)&opt.time),
      bias((float *)&opt.bias),
      fill((float *)&opt.fill),
      missingcolor((void *)opt.missingcolor),
      samples((int *)&opt.samples),
      dresultds((float *)opt.dresultds), dresultdt((float *)opt.dresultdt),
      rwrap((Wrap)opt.rwrap), rblur((float *)&opt.rblur),
      rwidth((float *)&opt.rwidth),
      dresultdr((float *)opt.dresultdr),
      swrap_func(NULL), twrap_func(NULL), rwrap_func(NULL)
{
}



TextureOpt::TextureOpt (const TextureOptions &opt, int index)
    : nchannels(opt.nchannels), firstchannel(opt.firstchannel),
      subimage(opt.subimage), subimagename(opt.subimagename),
      swrap((Wrap)opt.swrap), twrap((Wrap)opt.twrap),
      mipmode((MipMode)opt.mipmode),
      interpmode((InterpMode)opt.interpmode),
      anisotropic(opt.anisotropic),
      conservative_filter(opt.conservative_filter),
      sblur(opt.sblur[index]), tblur(opt.tblur[index]),
      swidth(opt.swidth[index]), twidth(opt.twidth[index]),
      fill(opt.fill[index]),
      missingcolor(opt.missingcolor.ptr() ? &opt.missingcolor[index] : NULL),
      dresultds(opt.dresultds), dresultdt(opt.dresultdt),
      time(opt.time[index]),
      bias(opt.bias[index]),
      samples(opt.samples[index]),
      rwrap((Wrap)opt.rwrap),
      rblur(opt.rblur[index]), rwidth(opt.rwidth[index]),
      dresultdr(opt.dresultdr),
      swrap_func(NULL), twrap_func(NULL), rwrap_func(NULL),
      envlayout(0)
{
}



TextureOpt::Wrap
TextureOpt::decode_wrapmode (const char *name)
{
    for (int i = 0;  i < (int)WrapLast;  ++i)
        if (! strcmp (name, wrap_type_name[i].c_str()))
            return (Wrap) i;
    return TextureOpt::WrapDefault;
}



TextureOpt::Wrap
TextureOpt::decode_wrapmode (ustring name)
{
    for (int i = 0;  i < (int)WrapLast;  ++i)
        if (name == wrap_type_name[i])
            return (Wrap) i;
    return TextureOpt::WrapDefault;
}



void
TextureOpt::parse_wrapmodes (const char *wrapmodes,
                             TextureOpt::Wrap &swrapcode,
                             TextureOpt::Wrap &twrapcode)
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

}
OIIO_NAMESPACE_EXIT

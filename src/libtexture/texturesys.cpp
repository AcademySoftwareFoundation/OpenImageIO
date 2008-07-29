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
#include <boost/tr1/memory.hpp>
using namespace std::tr1;

#include <ImathVec.h>
#include <ImathMatrix.h>
#include <half.h>

#include "dassert.h"
#include "paramtype.h"
#include "varyingref.h"
#include "ustring.h"
#include "hash.h"
#include "imageio.h"
using namespace OpenImageIO;

#define DLL_EXPORT_PUBLIC /* Because we are implementing TextureSystem */
#include "texture.h"
#undef DLL_EXPORT_PUBLIC

#include "texture_pvt.h"


TextureSystem *
TextureSystem::create ()
{
    return new TexturePvt::TextureSystemImpl;
}



void
TextureSystem::destroy (TextureSystem * &x)
{
    delete x;
    x = NULL;
}



namespace TexturePvt {


void
TextureSystemImpl::init ()
{
    max_open_files (100);
    max_memory_MB (50);
}



TextureFile *
TextureSystemImpl::findtex (ustring filename)
{
    return NULL;
}



bool
TextureSystemImpl::gettextureinfo (ustring filename, ustring dataname,
                                   ParamType datatype, void *data)
{
    std::cerr << "gettextureinfo \"" << filename << "\"\n";

    TextureFile *texfile = findtex (filename);
    if (! texfile) {
        std::cerr << "   NOT FOUND\n";
        return false;
    }
    std::cerr << "    found.\n";
    return true;
}


};  // end namespace TexturePvt


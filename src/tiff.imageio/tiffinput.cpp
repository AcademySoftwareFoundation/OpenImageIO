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

#include "dassert.h"
#include "paramtype.h"
#include "imageio.h"


using namespace OpenImageIO;


class DLLPUBLIC TIFFInput : public ImageInput {
public:
    TIFFInput ();
    virtual ~TIFFInput ();

    virtual bool open (const char *name, ImageIOFormatSpec &newspec,
                       int nparams, const ImageIOParameter *param);
    virtual int current_subimage (void) const;
    virtual bool seek_subimage (int index, ImageIOFormatSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);
    virtual bool get_parameter (std::string name, ParamType t, void *val);
    virtual bool close ();

private:
};



TIFFInput::TIFFInput ()
{
}



TIFFInput::~TIFFInput ()
{
}



bool
TIFFInput::open (const char *name, ImageIOFormatSpec &newspec,
                 int nparams, const ImageIOParameter *param)
{
}




int
TIFFInput::current_subimage (void) const
{
}




bool
TIFFInput::seek_subimage (int index, ImageIOFormatSpec &newspec)
{
}




bool
TIFFInput::read_native_scanline (int y, int z, void *data)
{
}




bool
TIFFInput::read_native_tile (int x, int y, int z, void *data)
{
}




bool
TIFFInput::get_parameter (std::string name, ParamType t, void *val)
{
}




bool
TIFFInput::close ()
{
}

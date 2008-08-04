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
#include "paramtype.h"
#include "varyingref.h"
#include "ustring.h"
#include "hash.h"
#include "thread.h"
#include "imageio.h"
using namespace OpenImageIO;

#define DLL_EXPORT_PUBLIC /* Because we are implementing TextureSystem */
#include "texture.h"
#undef DLL_EXPORT_PUBLIC

#include "texture_pvt.h"


namespace OpenImageIO {


TextureSystem *
TextureSystem::create ()
{
    return new pvt::TextureSystemImpl;
}



void
TextureSystem::destroy (TextureSystem * &x)
{
    delete x;
    x = NULL;
}



namespace pvt {   // namespace TextureSystem::pvt


static const char * texture_type_name[] = {
    // MUST match the order of TexType
    "unknown", "Plain Texture", "Volume Texture",
    "Shadow", "CubeFace Shadow", "Volume Shadow",
    "LatLong Environment", "CubeFace Environment",
    ""
};



static const char * wrap_type_name[] = {
    // MUST match the order of TextureOptions::Wrap
    "default", "black", "clamp", "periodic", "mirror",
    ""
};


static TextureOptions::Wrap
decode_wrapmode (const char *name)
{
    for (int i = 0;  i < (int)TextureOptions::WrapLast;  ++i)
        if (! strcmp (name, wrap_type_name[i]))
            return (TextureOptions::Wrap) i;
    return TextureOptions::WrapDefault;
}



static void
parse_wrapmodes (const char *wrapmodes, TextureOptions::Wrap &m_swrap,
                 TextureOptions::Wrap &m_twrap)
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
    m_swrap = decode_wrapmode (swrap);
    m_twrap = decode_wrapmode (twrap);
}



TextureFile::TextureFile (TextureSystemImpl &texsys, ustring filename)
    : m_filename(filename), m_used(true), m_broken(false),
      m_textype(TexTypeTexture), 
      m_swrap(TextureOptions::WrapBlack), m_twrap(TextureOptions::WrapBlack),
      m_cubelayout(CubeUnknown), m_y_up(false),
      m_texsys(texsys)
{
    m_input.reset (ImageInput::create (filename.c_str(),
                                       m_texsys.searchpath().c_str()));
    if (! m_input) {
        m_broken = true;
        return;
    }
    m_spec.reserve (16);
    ImageIOFormatSpec tempspec;
    if (! m_input->open (filename.c_str(), tempspec)) {
        m_broken = true;
        m_input.reset ();
        return;
    }

    int nsubimages = 0;
    do {
        ++nsubimages;
        m_spec.push_back (tempspec);
    } while (m_input->seek_subimage (nsubimages, tempspec));
    std::cerr << filename << " has " << m_spec.size() << " subimages\n";
    ASSERT (nsubimages = m_spec.size());

    const ImageIOFormatSpec &spec (m_spec[0]);
    const ImageIOParameter *p;

    m_textype = TexTypeTexture;
    p = spec.find_attribute ("textureformat");
    if (p && p->type == PT_STRING && p->nvalues == 1) {
        const char *textureformat = (const char *)p->data();
        for (int i = 0;  i < TexTypeLast;  ++i)
            if (! strcmp (textureformat, texture_type_name[i])) {
                m_textype = (TexType) i;
                break;
            }
    }

    p = spec.find_attribute ("wrapmodes");
    if (p && p->type == PT_STRING && p->nvalues == 1) {
        const char *wrapmodes = (const char *)p->data();
        parse_wrapmodes (wrapmodes, m_swrap, m_twrap);
    }

    m_y_up = false;
    if (m_textype == TexTypeCubeFaceEnv) {
        if (! strcmp (m_input->format_name(), "openexr"))
            m_y_up = true;
        int w = std::max (spec.full_width, spec.tile_width);
        int h = std::max (spec.full_height, spec.tile_height);
        if (spec.width == 3*w && spec.height == 2*h)
            m_cubelayout = CubeThreeByTwo;
        else if (spec.width == w && spec.height == 6*h)
            m_cubelayout = CubeOneBySix;
        else
            m_cubelayout = CubeLast;
    }

    // FIXME -- fill in: Mlocal, Mproj, Mtex, Mras
}



TextureFile::~TextureFile ()
{
    if (m_input) {
        m_input->close ();
        m_input.reset ();
    }
}



void
TextureSystemImpl::init ()
{
    max_open_files (100);
    max_memory_MB (50);
}



TextureFile *
TextureSystemImpl::findtex (ustring filename)
{
    lock_guard guard (m_texturefiles_mutex);
    FilenameMap::iterator found = m_texturefiles.find (filename);
    TextureFileRef tf;
    if (found == m_texturefiles.end()) {
        // We don't already have this file in the texture list.  Try to
        // open it and create a record.
        tf.reset (new TextureFile (*this, filename));
        m_texturefiles[filename] = tf;
    } else {
        tf = found->second;
    }

    return tf.get();
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
    if (texfile->broken()) {
        std::cerr << "    Invalid file\n";
        return false;
    }
    const ImageIOFormatSpec &spec (texfile->spec());
    if (dataname == "resolution" && datatype==ParamType(PT_INT,2)) {
        int *d = (int *)data;
        d[0] = spec.width;
        d[1] = spec.height;
        return true;
    }
    if (dataname == "texturetype" && datatype==ParamType(PT_STRING)) {
        const char **d = (const char **)data;
        d[0] = "unknown";  // FIXME
        return true;
    }
    if (dataname == "textureformat" && datatype==ParamType(PT_STRING)) {
        const char **d = (const char **)data;
        d[0] = "unknown";  // FIXME
        return true;
    }
    if (dataname == "channels" && datatype==ParamType(PT_INT)) {
        *(int *)data = spec.nchannels;
        return true;
    }
    if (dataname == "channels" && datatype==ParamType(PT_FLOAT)) {
        *(float *)data = spec.nchannels;
        return true;
    }
    // FIXME - "viewingmatrix"
    // FIXME - "projectionmatrix"

    // general case
    const ImageIOParameter *p = spec.find_attribute (dataname.string());
    if (p && p->nvalues == datatype.arraylen) {
        // First test for exact type match
        if (p->type == datatype.basetype) {
            memcpy (data, p->data(), datatype.datasize());
            return true;
        }
        // If the real data is int but user asks for float, translate it
        if (p->type == PT_FLOAT && datatype.basetype == PT_INT) {
            for (int i = 0;  i < p->nvalues;  ++i)
                ((float *)data)[i] = ((int *)p->data())[i];
            return true;
        }
    }

    return false;
}


};  // end namespace OpenImageIO::pvt
};  // end namespace OpenImageIO

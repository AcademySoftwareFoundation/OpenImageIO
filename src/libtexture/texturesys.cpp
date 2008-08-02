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


TextureFile::TextureFile (TextureSystemImpl &texsys, ustring filename)
    : m_texsys(texsys), m_filename(filename), m_used(true), m_broken(false)
{
    m_input.reset (ImageInput::create (filename.c_str(), m_texsys.searchpath().c_str()));
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

    // FIXME -- fill in: textype, swrap, twrap, Mlocal, Mproj, Mtex,
    // Mras, cubelayout, y_up
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
#if 0
    if (dataname == "textureformat" && datatype==ParamType(PT_STRING)) {
        const char **d = (const char **)data;
        d[0] = "unknown";  // FIXME
        return true;
    }
#endif
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

    // FIXME - general case

    
    return false;
}


};  // end namespace TexturePvt


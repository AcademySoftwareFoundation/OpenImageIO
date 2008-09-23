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
namespace pvt {   // namespace OpenImageIO::pvt



TextureFile::TextureFile (TextureSystemImpl &texsys, ustring filename)
    : m_filename(filename), m_used(true), m_broken(false),
      m_texformat(TexFormatTexture), 
      m_swrap(TextureOptions::WrapBlack), m_twrap(TextureOptions::WrapBlack),
      m_cubelayout(CubeUnknown), m_y_up(false),
      m_texsys(texsys)
{
    m_spec.clear ();
    open ();
}



TextureFile::~TextureFile ()
{
    release ();
}



void
TextureFile::open ()
{
    if (m_input)         // Already opened
        return;
    if (m_broken)        // Already failed an open -- it's broken
        return;
    
    m_input.reset (ImageInput::create (m_filename.c_str(),
                                       m_texsys.searchpath().c_str()));
    if (! m_input) {
        m_broken = true;
        return;
    }

    ImageSpec tempspec;
    if (! m_input->open (m_filename.c_str(), tempspec)) {
        m_broken = true;
        m_input.reset ();
        return;
    }
    m_texsys.incr_open_files ();
    use ();

    // If m_spec has already been filled out, we've opened this file
    // before, read the spec, and filled in all the fields.  So now that
    // we've re-opened it, we're done.
    if (m_spec.size())
        return;

    // From here on, we know that we've opened this file for the very
    // first time.  So read all the MIP levels, fill out all the fields
    // of the TextureFile.
    m_spec.reserve (16);
    int nsubimages = 0;
    do {
        ++nsubimages;
        m_spec.push_back (tempspec);
        // Sanity checks: all levels need the same num channels
        ASSERT (tempspec.nchannels == m_spec[0].nchannels);
    } while (m_input->seek_subimage (nsubimages, tempspec));
    ASSERT (nsubimages = m_spec.size());

    const ImageSpec &spec (m_spec[0]);
    const ImageIOParameter *p;

    m_texformat = TexFormatTexture;
    if (p = spec.find_attribute ("textureformat", PT_STRING)) {
        const char *textureformat = (const char *)p->data();
        for (int i = 0;  i < TexFormatLast;  ++i)
            if (! strcmp (textureformat, texture_format_name((TexFormat)i))) {
                m_texformat = (TexFormat) i;
                break;
            }
    }

    if (p = spec.find_attribute ("wrapmodes", PT_STRING)) {
        const char *wrapmodes = (const char *)p->data();
        TextureOptions::parse_wrapmodes (wrapmodes, m_swrap, m_twrap);
    }

    m_y_up = false;
    if (m_texformat == TexFormatCubeFaceEnv) {
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

    Imath::M44f c2w;
    m_texsys.get_commontoworld (c2w);
    if (p = spec.find_attribute ("worldtocamera", PT_MATRIX)) {
        const Imath::M44f *m = (const Imath::M44f *)p->data();
        m_Mlocal = c2w * (*m);
    }
    if (p = spec.find_attribute ("worldtoscreen", PT_MATRIX)) {
        const Imath::M44f *m = (const Imath::M44f *)p->data();
        m_Mproj = c2w * (*m);
    }
    // FIXME -- compute Mtex, Mras

    m_datatype = PT_FLOAT;
    // FIXME -- use 8-bit when that's native?
}



bool
TextureFile::read_tile (int level, int x, int y, int z,
                        TypeDesc format, void *data)
{
    open ();
    ImageSpec tmp;
    if (m_input->current_subimage() != level)
        m_input->seek_subimage (level, tmp);
    return m_input->read_tile (x, y, z, format, data);
}



void
TextureFile::release ()
{
    if (m_used) {
        m_used = false;
    } else if (opened()) {
        m_input->close ();
        m_input.reset ();
        m_used = false;
        m_texsys.decr_open_files ();
    }
}



TextureFileRef
TextureSystemImpl::find_texturefile (ustring filename)
{
    lock_guard guard (m_texturefiles_mutex);

    FilenameMap::iterator found = m_texturefiles.find (filename);
    TextureFileRef tf;
    if (found == m_texturefiles.end()) {
        // We don't already have this file in the texture list.  Try to
        // open it and create a record.
        check_max_files ();
        tf.reset (new TextureFile (*this, filename));
        m_texturefiles[filename] = tf;
    } else {
        tf = found->second;
    }

    tf->use ();
    return tf;
}



void
TextureSystemImpl::check_max_files ()
{
#ifdef DEBUG
    std::cerr << "open files " << m_open_files << ", max = " << m_max_open_files << "\n";
#endif
    while (m_open_files >= m_max_open_files) {
        if (m_file_sweep == m_texturefiles.end())
            m_file_sweep = m_texturefiles.begin();
        ASSERT (m_file_sweep != m_texturefiles.end());
        m_file_sweep->second->release ();  // May reduce m_open_files
        ++m_file_sweep;
    }
}



};  // end namespace OpenImageIO::pvt
};  // end namespace OpenImageIO

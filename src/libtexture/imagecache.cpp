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
      m_untiled(false), m_unmipped(false),
      m_texformat(TexFormatTexture), 
      m_swrap(TextureOptions::WrapBlack), m_twrap(TextureOptions::WrapBlack),
      m_cubelayout(CubeUnknown), m_y_up(false),
      m_texsys(texsys)
{
    m_spec.clear ();
    open ();
    ++texsys.m_stat_texfile_records_created;
    if (++texsys.m_stat_texfile_records_current > texsys.m_stat_texfile_records_peak)
        texsys.m_stat_texfile_records_peak = texsys.m_stat_texfile_records_current;
}



TextureFile::~TextureFile ()
{
    close ();
    --texsys().m_stat_texfile_records_current;
}



bool
TextureFile::open ()
{
    if (m_input)         // Already opened
        return !m_broken;
    if (m_broken)        // Already failed an open -- it's broken
        return false;

    m_input.reset (ImageInput::create (m_filename.c_str(),
                                       m_texsys.searchpath().c_str()));
    if (! m_input) {
        m_broken = true;
        return false;
    }

    ImageSpec tempspec;
    if (! m_input->open (m_filename.c_str(), tempspec)) {
        m_broken = true;
        m_input.reset ();
        return false;
    }
    m_texsys.incr_open_files ();
    use ();

    // If m_spec has already been filled out, we've opened this file
    // before, read the spec, and filled in all the fields.  So now that
    // we've re-opened it, we're done.
    if (m_spec.size())
        return true;

    // From here on, we know that we've opened this file for the very
    // first time.  So read all the MIP levels, fill out all the fields
    // of the TextureFile.
    ++texsys().m_stat_files_referenced;
    m_spec.reserve (16);
    int nsubimages = 0;
    do {
        if (nsubimages > 1 && tempspec.nchannels != m_spec[0].nchannels) {
            // No idea what to do with a subimage that doesn't have the
            // same number of channels as the others, so just skip it.
            m_input.reset ();
            m_texsys.decr_open_files ();
            m_broken = true;
            return false;
        }
        if (tempspec.tile_width == 0 || tempspec.tile_height == 0) {
            m_untiled = true;
            tempspec.tile_width = pow2roundup (tempspec.width);
            tempspec.tile_height = pow2roundup (tempspec.height);
        }
        ++nsubimages;
        m_spec.push_back (tempspec);
        texsys().m_stat_files_totalsize += (long long)tempspec.image_bytes();
    } while (m_input->seek_subimage (nsubimages, tempspec));
    ASSERT (nsubimages == m_spec.size());
    if (m_untiled && nsubimages == 1)
        m_unmipped = true;

    const ImageSpec &spec (m_spec[0]);
    const ImageIOParameter *p;

    m_texformat = TexFormatTexture;
    if (p = spec.find_attribute ("textureformat", TypeDesc::STRING)) {
        const char *textureformat = (const char *)p->data();
        for (int i = 0;  i < TexFormatLast;  ++i)
            if (! strcmp (textureformat, texture_format_name((TexFormat)i))) {
                m_texformat = (TexFormat) i;
                break;
            }
    }

    if (p = spec.find_attribute ("wrapmodes", TypeDesc::STRING)) {
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

    m_datatype = TypeDesc::FLOAT;
    // FIXME -- use 8-bit when that's native?

    if (m_untiled || m_unmipped) {
        close ();
    }

    return !m_broken;
}



bool
TextureFile::read_tile (int level, int x, int y, int z,
                        TypeDesc format, void *data)
{
    bool ok = open ();
    if (! ok)
        return false;

    ImageSpec tmp;
    if (m_input->current_subimage() != level)
        m_input->seek_subimage (level, tmp);

    // Handle untiled, unmip-mapped
    if (m_untiled || m_unmipped) {
        stride_t xstride=AutoStride, ystride=AutoStride, zstride=AutoStride;
        spec().auto_stride (xstride, ystride, zstride, format, spec().nchannels,
                            spec().tile_width, spec().tile_height);
        ok = m_input->read_image (format, data, xstride, ystride, zstride);
        texsys().m_stat_bytes_read += spec().image_bytes();
        close ();   // Done with it
        return ok;
    }

    // Ordinary tiled
    texsys().m_stat_bytes_read += spec(level).tile_bytes();
    return m_input->read_tile (x, y, z, format, data);
}



void
TextureFile::close ()
{
    if (opened()) {
        m_input->close ();
        m_input.reset ();
        m_texsys.decr_open_files ();
    }
}



void
TextureFile::release ()
{
    if (m_used)
        m_used = false;
    else
        close ();
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
    if (! (m_open_files % 16) || m_open_files >= m_max_open_files)
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

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
#include <sstream>
#include <vector>
#include <cstring>

#include <OpenEXR/ImathMatrix.h>

#include "OpenImageIO/dassert.h"
#include "OpenImageIO/typedesc.h"
#include "OpenImageIO/varyingref.h"
#include "OpenImageIO/ustring.h"
#include "OpenImageIO/filesystem.h"
#include "OpenImageIO/thread.h"
#include "OpenImageIO/fmath.h"
#include "OpenImageIO/strutil.h"
#include "OpenImageIO/sysutil.h"
#include "OpenImageIO/timer.h"
#include "OpenImageIO/optparser.h"
#include "OpenImageIO/imageio.h"
#include "OpenImageIO/imagebuf.h"
#include "OpenImageIO/imagecache.h"
#include "OpenImageIO/texture.h"
#include "imagecache_pvt.h"

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/scoped_array.hpp>


OIIO_NAMESPACE_ENTER
{
    using namespace pvt;

namespace pvt {

// The static perthread mutex needs to outlive the shared_image_cache
// instance, so must be declared first in this file to avoid static
// initialization order problems.
mutex ImageCacheImpl::m_perthread_info_mutex;

}

namespace {  // anonymous

static shared_ptr<ImageCacheImpl> shared_image_cache;
static spin_mutex shared_image_cache_mutex;

// Make some static ustring constants to avoid strcmp's
static ustring s_resolution ("resolution"), s_texturetype ("texturetype");
static ustring s_textureformat ("textureformat"), s_fileformat ("fileformat");
static ustring s_format ("format"), s_cachedformat ("cachedformat");
static ustring s_channels ("channels"), s_cachedpixeltype ("cachedpixeltype");
static ustring s_exists ("exists");
static ustring s_subimages ("subimages"), s_miplevels ("miplevels");
static ustring s_datawindow ("datawindow"), s_displaywindow ("displaywindow");


// Functor to compare filenames
static bool
filename_compare (const ImageCacheFileRef &a, const ImageCacheFileRef &b)
{
    return a->filename() < b->filename();
}


// Functor to compare read bytes, sort in descending order
static bool
bytesread_compare (const ImageCacheFileRef &a, const ImageCacheFileRef &b)
{
    return a->bytesread() > b->bytesread();
}


// Functor to compare read times, sort in descending order
static bool
iotime_compare (const ImageCacheFileRef &a, const ImageCacheFileRef &b)
{
    return a->iotime() > b->iotime();
}


// Functor to compare read rate (MB/s), sort in ascending order
static bool
iorate_compare (const ImageCacheFileRef &a, const ImageCacheFileRef &b)
{
    double arate = a->bytesread()/(1024.0*1024.0) / a->iotime();
    double brate = b->bytesread()/(1024.0*1024.0) / b->iotime();
    return arate < brate;
}


};  // end anonymous namespace


namespace pvt {   // namespace pvt



void
ImageCacheStatistics::init ()
{
    // ImageCache stats:
    find_tile_calls = 0;
    find_tile_microcache_misses = 0;
    find_tile_cache_misses = 0;
//    tiles_created = 0;
//    tiles_current = 0;
//    tiles_peak = 0;
    files_totalsize = 0;
    bytes_read = 0;
//    open_files_created = 0;
//    open_files_current = 0;
//    open_files_peak = 0;
    unique_files = 0;
    fileio_time = 0;
    fileopen_time = 0;
    file_locking_time = 0;
    tile_locking_time = 0;
    find_file_time = 0;
    find_tile_time = 0;

    // TextureSystem stats:
    texture_queries = 0;
    texture_batches = 0;
    texture3d_queries = 0;
    texture3d_batches = 0;
    shadow_queries = 0;
    shadow_batches = 0;
    environment_queries = 0;
    environment_batches = 0;
    aniso_queries = 0;
    aniso_probes = 0;
    max_aniso = 1;
    closest_interps = 0;
    bilinear_interps = 0;
    cubic_interps = 0;
    file_retry_success = 0;
    tile_retry_success = 0;
}



void
ImageCacheStatistics::merge (const ImageCacheStatistics &s)
{
    // ImageCache stats:
    find_tile_calls += s.find_tile_calls;
    find_tile_microcache_misses += s.find_tile_microcache_misses;
    find_tile_cache_misses += s.find_tile_cache_misses;
//    tiles_created += s.tiles_created;
//    tiles_current += s.tiles_current;
//    tiles_peak += s.tiles_peak;
    files_totalsize += s.files_totalsize;
    bytes_read += s.bytes_read;
//    open_files_created += s.open_files_created;
//    open_files_current += s.open_files_current;
//    open_files_peak += s.open_files_peak;
    unique_files += s.unique_files;
    fileio_time += s.fileio_time;
    fileopen_time += s.fileopen_time;
    file_locking_time += s.file_locking_time;
    tile_locking_time += s.tile_locking_time;
    find_file_time += s.find_file_time;
    find_tile_time += s.find_tile_time;

    // TextureSystem stats:
    texture_queries += s.texture_queries;
    texture_batches += s.texture_batches;
    texture3d_queries += s.texture3d_queries;
    texture3d_batches += s.texture3d_batches;
    shadow_queries += s.shadow_queries;
    shadow_batches += s.shadow_batches;
    environment_queries += s.environment_queries;
    environment_batches += s.environment_batches;
    aniso_queries += s.aniso_queries;
    aniso_probes += s.aniso_probes;
    max_aniso = std::max (max_aniso, s.max_aniso);
    closest_interps += s.closest_interps;
    bilinear_interps += s.bilinear_interps;
    cubic_interps += s.cubic_interps;
    file_retry_success += s.file_retry_success;
    tile_retry_success += s.tile_retry_success;
}



ImageCacheFile::LevelInfo::LevelInfo (const ImageSpec &spec_,
                                      const ImageSpec &nativespec_)
    : spec(spec_), nativespec(nativespec_)
{
    full_pixel_range = (spec.x == spec.full_x && spec.y == spec.full_y &&
                        spec.z == spec.full_z &&
                        spec.width == spec.full_width &&
                        spec.height == spec.full_height &&
                        spec.depth == spec.full_depth);
    onetile = (spec.width <= spec.tile_width &&
               spec.height <= spec.tile_height &&
               spec.depth <= spec.tile_depth);
    polecolorcomputed = false;
}



ImageCacheFile::ImageCacheFile (ImageCacheImpl &imagecache,
                                ImageCachePerThreadInfo *thread_info,
                                ustring filename,
                                ImageInput::Creator creator)
    : m_filename(filename), m_used(true), m_broken(false),
      m_texformat(TexFormatTexture),
      m_swrap(TextureOpt::WrapBlack), m_twrap(TextureOpt::WrapBlack),
      m_rwrap(TextureOpt::WrapBlack),
      m_envlayout(LayoutTexture), m_y_up(false), m_sample_border(false),
      m_tilesread(0), m_bytesread(0), m_timesopened(0), m_iotime(0),
      m_mipused(false), m_validspec(false), 
      m_imagecache(imagecache), m_duplicate(NULL),
      m_total_imagesize(0),
      m_inputcreator(creator)
{
    m_filename_original = m_filename;
    m_filename = imagecache.resolve_filename (m_filename_original.string());
    // N.B. the file is not opened, the ImageInput is NULL.  This is
    // reflected by the fact that m_validspec is false.
    m_Mlocal.makeIdentity();
    m_Mproj.makeIdentity();
    m_Mtex.makeIdentity();
    m_Mras.makeIdentity();
}



ImageCacheFile::~ImageCacheFile ()
{
    close ();
}



void
ImageCacheFile::SubimageInfo::init (const ImageSpec &spec, bool forcefloat)
{
    volume = (spec.depth > 1 || spec.full_depth > 1);
    full_pixel_range = (spec.x == spec.full_x &&
                           spec.y == spec.full_y &&
                           spec.z == spec.full_z &&
                           spec.width == spec.full_width &&
                           spec.height == spec.full_height &&
                           spec.depth == spec.full_depth);
    if (! full_pixel_range) {
        sscale = float(spec.full_width) / spec.width;
        soffset = float(spec.full_x-spec.x) / spec.width;
        tscale = float(spec.full_height) / spec.height;
        toffset = float(spec.full_y-spec.y) / spec.height;
    } else {
        sscale = tscale = 1.0f;
        soffset = toffset = 0.0f;
    }
    subimagename = ustring (spec.get_string_attribute("oiio:subimagename"));
    datatype = TypeDesc::FLOAT;
    if (! forcefloat) {
        // If we aren't forcing everything to be float internally, then 
        // there are a few other types we allow.
        // But at present, it's only UINT8 and FLOAT.
        if (spec.format == TypeDesc::UINT8
            /* future expansion:  || spec.format == AnotherFormat ... */)
            datatype = spec.format;
    }
    channelsize = datatype.size();
    pixelsize = channelsize * spec.nchannels;
    eightbit = (datatype == TypeDesc::UINT8);
}



bool
ImageCacheFile::open (ImageCachePerThreadInfo *thread_info)
{
    // N.B. open() does not need to lock the m_input_mutex, because open()
    // itself is only called by routines that hold the lock.
    // recursive_lock_guard_t guard (m_input_mutex);

    if (m_input)         // Already opened
        return !m_broken;
    if (m_broken)        // Already failed an open -- it's broken
        return false;

    if (m_inputcreator)
        m_input.reset (m_inputcreator());
    else
        m_input.reset (ImageInput::create (m_filename.c_str(),
                                           m_imagecache.plugin_searchpath().c_str()));
    if (! m_input) {
        imagecache().error ("%s", OIIO::geterror().c_str());
        m_broken = true;
        invalidate_spec ();
        return false;
    }

    ImageSpec configspec;
    if (imagecache().unassociatedalpha())
        configspec.attribute ("oiio:UnassociatedAlpha", 1);

    ImageSpec nativespec, tempspec;
    m_broken = false;
    bool ok = true;
    for (int tries = 0; tries <= imagecache().failure_retries(); ++tries) {
        ok = m_input->open (m_filename.c_str(), nativespec, configspec);
        if (ok) {
            tempspec = nativespec;
            if (tries)   // succeeded, but only after a failure!
                ++thread_info->m_stats.file_retry_success;
            (void) m_input->geterror ();  // Eat the errors
            break;
        }
        // We failed.  Wait a bit and try again.
        Sysutil::usleep (1000 * 100);  // 100 ms
    }
    if (! ok) {
        imagecache().error ("%s", m_input->geterror().c_str());
        m_broken = true;
        m_input.reset ();
        return false;
    }
    m_fileformat = ustring (m_input->format_name());
    ++m_timesopened;
    m_imagecache.incr_open_files ();
    use ();

    // If we are simply re-opening a closed file, and the spec is still
    // valid, we're done, no need to reread the subimage and mip headers.
    if (validspec())
        return true;

    // From here on, we know that we've opened this file for the very
    // first time.  So read all the subimages, fill out all the fields
    // of the ImageCacheFile.
    m_subimages.clear ();
    int nsubimages = 0;

    // Since each subimage can potentially have its own mipmap levels,
    // keep track of the highest level discovered
    imagesize_t old_total_imagesize = m_total_imagesize;
    m_total_imagesize = 0;
    do {
        m_subimages.resize (nsubimages+1);
        SubimageInfo &si (subimageinfo(nsubimages));
        int nmip = 0;
        do {
            tempspec = nativespec;
            if (nmip == 0) {
                // Things to do on MIP level 0, i.e. once per subimage
                si.init (tempspec, imagecache().forcefloat());
            }
            if (tempspec.tile_width == 0 || tempspec.tile_height == 0) {
                si.untiled = true;
                int autotile = imagecache().autotile();
                if (autotile) {
                    // Automatically make it appear as if it's tiled
                    if (imagecache().autoscanline()) {
                        tempspec.tile_width = tempspec.width;
                    } else {
                        tempspec.tile_width = std::min (tempspec.width, autotile);
                    }
                    tempspec.tile_height = std::min (tempspec.height, autotile);
                    tempspec.tile_depth = std::min (std::max(tempspec.depth,1), autotile);
                } else {
                    // Don't auto-tile -- which really means, make it look like
                    // a single tile that's as big as the whole image.
                    // We round to a power of 2 because the texture system
                    // currently requires power of 2 tile sizes.
                    tempspec.tile_width = tempspec.width;
                    tempspec.tile_height = tempspec.height;
                    tempspec.tile_depth = tempspec.depth;
                }
            }
//            thread_info->m_stats.files_totalsize += tempspec.image_bytes();
            m_total_imagesize += tempspec.image_bytes();
            // All MIP levels need the same number of channels
            if (nmip > 0 && tempspec.nchannels != spec(nsubimages,0).nchannels) {
                // No idea what to do with a subimage that doesn't have the
                // same number of channels as the others, so just skip it.
                close ();
                m_broken = true;
                invalidate_spec ();
                return false;
            }
            // ImageCache can't store differing formats per channel
            tempspec.channelformats.clear();
            LevelInfo levelinfo (tempspec, nativespec);
            si.levels.push_back (levelinfo);
            ++nmip;
        } while (m_input->seek_subimage (nsubimages, nmip, nativespec));

        // Special work for non-MIPmapped images -- but only if "automip"
        // is on, it's a non-mipmapped image, and it doesn't have a
        // "textureformat" attribute (because that would indicate somebody
        // constructed it as texture and specifically wants it un-mipmapped).
        // But not volume textures -- don't auto MIP them for now.
        if (nmip == 1 && !si.volume && 
            (tempspec.width > 1 || tempspec.height > 1 || tempspec.depth > 1))
            si.unmipped = true;
        if (si.unmipped && imagecache().automip() &&
            ! tempspec.find_attribute ("textureformat", TypeDesc::TypeString)) {
            int w = tempspec.full_width;
            int h = tempspec.full_height;
            int d = tempspec.full_depth;
            while (w > 1 || h > 1 || d > 1) {
                w = std::max (1, w/2);
                h = std::max (1, h/2);
                d = std::max (1, d/2);
                ImageSpec s = tempspec;
                s.width = w;
                s.height = h;
                s.depth = d;
                s.full_width = w;
                s.full_height = h;
                s.full_depth = d;
                if (imagecache().autotile()) {
                    if (imagecache().autoscanline()) {
                       s.tile_width = w;
                    } else {
                       s.tile_width = std::min (imagecache().autotile(), w);
                    }
                    s.tile_height = std::min (imagecache().autotile(), h);
                    s.tile_depth = std::min (imagecache().autotile(), d);
                } else {
                    s.tile_width = w;
                    s.tile_height = h;
                    s.tile_depth = d;
                }
                ++nmip;
                LevelInfo levelinfo (s, s);
                si.levels.push_back (levelinfo);
            }
        }
        if (si.untiled && ! imagecache().accept_untiled()) {
            imagecache().error ("%s was untiled, rejecting",
                                m_filename.c_str());
            m_broken = true;
            invalidate_spec ();
            m_input.reset ();
            return false;
        }
        if (si.unmipped && ! imagecache().accept_unmipped()) {
            imagecache().error ("%s was not MIP-mapped, rejecting",
                                m_filename.c_str());
            m_broken = true;
            invalidate_spec ();
            m_input.reset ();
            return false;
        }

        ++nsubimages;
    } while (m_input->seek_subimage (nsubimages, 0, nativespec));
    ASSERT ((size_t)nsubimages == m_subimages.size());

    thread_info->m_stats.files_totalsize -= old_total_imagesize;
    thread_info->m_stats.files_totalsize += m_total_imagesize;

    init_from_spec ();  // Fill in the rest of the fields
    return true;
}



void
ImageCacheFile::init_from_spec ()
{
    const ImageSpec &spec (this->spec(0,0));
    const ImageIOParameter *p;

    // FIXME -- this should really be per-subimage
    if (spec.depth <= 1 && spec.full_depth <= 1)
        m_texformat = TexFormatTexture;
    else
        m_texformat = TexFormatTexture3d;
    if ((p = spec.find_attribute ("textureformat", TypeDesc::STRING))) {
        const char *textureformat = *(const char **)p->data();
        for (int i = 0;  i < TexFormatLast;  ++i)
            if (Strutil::iequals (textureformat, texture_format_name((TexFormat)i))) {
                m_texformat = (TexFormat) i;
                break;
            }
        // For textures marked as such, doctor the full_width/full_height to
        // not be non-sensical.
        if (m_texformat == TexFormatTexture) {
            for (int s = 0;  s < subimages();  ++s) {
                for (int m = 0;  m < miplevels(s);  ++m) {
                    ImageSpec &spec (this->spec(s,m));
                    if (spec.full_width > spec.width)
                        spec.full_width = spec.width;
                    if (spec.full_height > spec.height)
                        spec.full_height = spec.height;
                    if (spec.full_depth > spec.depth)
                        spec.full_depth = spec.depth;
                }
            }
        }
    }

    if ((p = spec.find_attribute ("wrapmodes", TypeDesc::STRING))) {
        const char *wrapmodes = *(const char **)p->data();
        TextureOpt::parse_wrapmodes (wrapmodes, m_swrap, m_twrap);
        m_rwrap = m_swrap;
        // FIXME(volume) -- rwrap
    }

    m_y_up = m_imagecache.latlong_y_up_default();
    m_sample_border = false;
    if (m_texformat == TexFormatLatLongEnv ||
        m_texformat == TexFormatCubeFaceEnv ||
        m_texformat == TexFormatCubeFaceShadow) {
        if (spec.get_string_attribute ("oiio:updirection") == "y")
            m_y_up = true;
        else if (spec.get_string_attribute ("oiio:updirection") == "z")
            m_y_up = false;
        if (spec.get_int_attribute ("oiio:sampleborder") != 0)
            m_sample_border = true;
    }

    if (m_texformat == TexFormatCubeFaceEnv ||
        m_texformat == TexFormatCubeFaceShadow) {
        int w = std::max (spec.full_width, spec.tile_width);
        int h = std::max (spec.full_height, spec.tile_height);
        if (spec.width == 3*w && spec.height == 2*h)
            m_envlayout = LayoutCubeThreeByTwo;
        else if (spec.width == w && spec.height == 6*h)
            m_envlayout = LayoutCubeOneBySix;
        else
            m_envlayout = LayoutTexture;
    }

    Imath::M44f c2w;
    m_imagecache.get_commontoworld (c2w);
    if ((p = spec.find_attribute ("worldtocamera", TypeDesc::TypeMatrix))) {
        const Imath::M44f *m = (const Imath::M44f *)p->data();
        m_Mlocal = c2w * (*m);
    }
    if ((p = spec.find_attribute ("worldtoscreen", TypeDesc::TypeMatrix))) {
        const Imath::M44f *m = (const Imath::M44f *)p->data();
        m_Mproj = c2w * (*m);
    }
    // FIXME -- compute Mtex, Mras

    // See if there's a SHA-1 hash in the image description
    std::string fing = spec.get_string_attribute ("oiio:SHA-1");
    if (fing.length()) {
        m_fingerprint = ustring(fing);
    } else {
        // If there was no "oiio:SHA-1" attribute, search for it as
        // part of the ImageDescription.
        std::string desc = spec.get_string_attribute ("ImageDescription");
        const char *prefix = "SHA-1=";
        size_t found = desc.rfind (prefix);
        if (found != std::string::npos)
            m_fingerprint = ustring (desc, found+strlen(prefix), 40);
    }
    if (m_fingerprint) {
        // If it looks like something other than OIIO wrote the file, forget
        // the fingerprint, it probably is not accurate.
        string_view software = spec.get_string_attribute ("Software");
        if (! Strutil::istarts_with (software, "OpenImageIO") &&
            ! Strutil::istarts_with (software, "maketx"))
            m_fingerprint.clear ();
    }

    m_mod_time = Filesystem::last_write_time (m_filename.string());

    // Set all mipmap level read counts to zero
    int maxmip = 1;
    for (int s = 0, nsubimages = subimages();  s < nsubimages;  ++s)
        maxmip = std::max (maxmip, miplevels(s));
    m_mipreadcount.clear ();
    m_mipreadcount.resize(maxmip, 0);

    DASSERT (! m_broken);
    m_validspec = true;
}



bool
ImageCacheFile::read_tile (ImageCachePerThreadInfo *thread_info,
                           int subimage, int miplevel, int x, int y, int z,
                           TypeDesc format, void *data)
{
    recursive_lock_guard guard (m_input_mutex);

    if (! m_input && !m_broken) {
        // The file is already in the file cache, but the handle is
        // closed.  We will need to re-open, so we must make sure there
        // will be enough file handles.
        // But wait, it's possible that somebody else is waiting on our
        // m_input_mutex, which we locked above.  To avoid deadlock, we
        // need to release m_input_mutex while we close files.
        m_input_mutex.unlock ();
        imagecache().check_max_files (thread_info);
        // Now we're back, whew!  Grab the lock again.
        m_input_mutex.lock ();
    }

    bool ok = open (thread_info);
    if (! ok)
        return false;

    // Mark if we ever use a mip level that's not the first
    if (miplevel > 0)
        m_mipused = true;

    // count how many times this mipmap level was read
    m_mipreadcount[miplevel]++;

    SubimageInfo &subinfo (subimageinfo(subimage));

    // Special case for un-MIP-mapped
    if (subinfo.unmipped && miplevel != 0) {
        // For a non-base mip level of an unmipped file, release the
        // mutex on the ImageInput since upper levels don't need to
        // directly perform I/O.  This prevents the deadlock that could
        // occur if another thread has one of the lower-level tiles and
        // itself blocks on the mutex (it's waiting for our mutex, we're
        // waiting on its tile to get filled with pixels).
        unlock_input_mutex ();
        bool ok = read_unmipped (thread_info, subimage, miplevel,
                                 x, y, z, format, data);
        // The lock_guard at the very top will try to unlock upon
        // destruction, to to make things right, we need to re-lock.
        lock_input_mutex ();
        return ok;
    }

    // Special case for untiled images -- need to do tile emulation
    if (subinfo.untiled)
        return read_untiled (thread_info, subimage, miplevel,
                             x, y, z, format, data);

    // Ordinary tiled
    ImageSpec tmp;
    if (m_input->current_subimage() != subimage ||
        m_input->current_miplevel() != miplevel)
        ok = m_input->seek_subimage (subimage, miplevel, tmp);
    if (ok) {
        for (int tries = 0; tries <= imagecache().failure_retries(); ++tries) {
            ok = m_input->read_tile (x, y, z, format, data);
            if (ok) {
                if (tries)   // succeeded, but only after a failure!
                    ++thread_info->m_stats.tile_retry_success;
                (void) m_input->geterror ();  // Eat the errors
                break;
            }
            // We failed.  Wait a bit and try again.
            Sysutil::usleep (1000 * 100);  // 100 ms
            // TODO: should we attempt to close and re-open the file?
        }
        if (! ok)
            imagecache().error ("%s", m_input->geterror().c_str());
    }
    if (ok) {
        size_t b = spec(subimage,miplevel).tile_bytes();
        thread_info->m_stats.bytes_read += b;
        m_bytesread += b;
        ++m_tilesread;
    }
    return ok;
}



bool
ImageCacheFile::read_unmipped (ImageCachePerThreadInfo *thread_info,
                               int subimage, int miplevel, int x, int y, int z,
                               TypeDesc format, void *data)
{
    // We need a tile from an unmipmapped file, and it doesn't really
    // exist.  So generate it out of thin air by interpolating pixels
    // from the next higher-res level.  Of course, that may also not
    // exist, but it will be generated recursively, since we call
    // imagecache->get_pixels(), and it will ask for other tiles, which
    // will again call read_unmipped... eventually it will hit a subimage 0
    // tile that actually exists.

    // N.B. No need to lock the mutex, since this is only called
    // from read_tile, which already holds the lock.

    // Figure out the size and strides for a single tile, make an ImageBuf
    // to hold it temporarily.
    const ImageSpec &spec (this->spec(subimage,miplevel));
    int tw = spec.tile_width;
    int th = spec.tile_height;
    stride_t xstride=AutoStride, ystride=AutoStride, zstride=AutoStride;
    spec.auto_stride(xstride, ystride, zstride, format, spec.nchannels, tw, th);
    ImageSpec lospec (tw, th, spec.nchannels, TypeDesc::FLOAT);
    ImageBuf lores (lospec);

    // Figure out the range of texels we need for this tile
    x -= spec.x;
    y -= spec.y;
    z -= spec.z;
    int x0 = x - (x % spec.tile_width);
    int x1 = std::min (x0+spec.tile_width-1, spec.full_width-1);
    int y0 = y - (y % spec.tile_height);
    int y1 = std::min (y0+spec.tile_height-1, spec.full_height-1);
//    int z0 = z - (z % spec.tile_depth);
//    int z1 = std::min (z0+spec.tile_depth-1, spec.full_depth-1);

    // Save the contents of the per-thread microcache.  This is because
    // a caller several levels up may be retaining a reference to
    // thread_info->tile and expecting it not to suddenly point to a
    // different tile id!  It's a very reasonable assumption that if you
    // ask to read the last-found tile, it will still be the last-found
    // tile after the pixels are read.  Well, except that below our call
    // to get_pixels may recursively trigger more tiles to be read, and
    // totally change the microcache.  Simple solution: save & restore it.
    ImageCacheTileRef oldtile = thread_info->tile;
    ImageCacheTileRef oldlasttile = thread_info->lasttile;

    // Auto-mipping will totally thrash the cache if the user unwisely
    // sets it to be too small compared to the image file that needs to
    // automipped.  So we simply override bad decisions by adjusting the
    // cache size to be a minimum of twice as big as any image we automip.
    imagecache().set_min_cache_size (2 * (long long)this->spec(subimage,0).image_bytes());

    // Texel by texel, generate the values by interpolating filtered
    // lookups form the next finer subimage.
    const ImageSpec &upspec (this->spec(subimage,miplevel-1));  // next higher level
    float *bilerppels = (float *) alloca (4 * spec.nchannels * sizeof(float));
    float *resultpel = (float *) alloca (spec.nchannels * sizeof(float));
    bool ok = true;
    // FIXME(volume) -- loop over z, too
    for (int j = y0;  j <= y1;  ++j) {
        float yf = (j+0.5f) / spec.full_height;
        int ylow;
        float yfrac = floorfrac (yf * upspec.full_height - 0.5, &ylow);
        for (int i = x0;  i <= x1;  ++i) {
            float xf = (i+0.5f) / spec.full_width;
            int xlow;
            float xfrac = floorfrac (xf * upspec.full_width - 0.5, &xlow);
            ok &= imagecache().get_pixels (this, thread_info,
                                           subimage, miplevel-1,
                                           xlow, xlow+2, ylow, ylow+2,
                                           0, 1, 0, spec.nchannels,
                                           TypeDesc::FLOAT, bilerppels);
            bilerp (bilerppels+0, bilerppels+spec.nchannels,
                    bilerppels+2*spec.nchannels, bilerppels+3*spec.nchannels,
                    xfrac, yfrac, spec.nchannels, resultpel);
            lores.setpixel (i-x0, j-y0, resultpel);
        }
    }

    // Now convert and copy those values out to the caller's buffer
    lores.get_pixels (0, tw, 0, th, 0, 1, format, data);

    // Restore the microcache to the way it was before.
    thread_info->tile = oldtile;
    thread_info->lasttile = oldlasttile;

    return ok;
}



// Helper routine for read_tile that handles the rare (but tricky) case
// of reading a "tile" from a file that's scanline-oriented.
bool
ImageCacheFile::read_untiled (ImageCachePerThreadInfo *thread_info,
                              int subimage, int miplevel, int x, int y, int z,
                              TypeDesc format, void *data)
{
    // N.B. No need to lock the input mutex, since this is only called
    // from read_tile, which already holds the lock.

    if (m_input->current_subimage() != subimage ||
        m_input->current_miplevel() != miplevel) {
        ImageSpec tmp;
        if (! m_input->seek_subimage (subimage, miplevel, tmp)) {
            imagecache().error ("%s", m_input->geterror().c_str());
            return false;
        }
    }

    // Strides for a single tile
    ImageSpec &spec (this->spec(subimage,miplevel));
    int tw = spec.tile_width;
    int th = spec.tile_height;
    stride_t xstride=AutoStride, ystride=AutoStride, zstride=AutoStride;
    spec.auto_stride (xstride, ystride, zstride, format,
                      spec.nchannels, tw, th);

    bool ok = true;
    if (imagecache().autotile()) {
        // Auto-tile is on, with a tile size that isn't the whole image.
        // We're only being asked for one tile, but since it's a
        // scanline image, we are forced to read (at the very least) a
        // whole row of tiles.  So we add all those tiles to the cache,
        // if not already present, on the assumption that it's highly
        // likely that they will also soon be requested.
        // FIXME -- I don't think this works properly for 3D images
        size_t pixelsize = size_t (spec.nchannels * format.size());
        // Because of the way we copy below, we need to allocate the
        // buffer to be an even multiple of the tile width, so round up.
        stride_t scanlinesize = tw * ((spec.width+tw-1)/tw);
        scanlinesize *= pixelsize;
        boost::scoped_array<char> buf (new char [scanlinesize * th]); // a whole tile-row size
        int yy = y - spec.y;   // counting from top scanline
        // [y0,y1] is the range of scanlines to read for a tile-row
        int y0 = yy - (yy % th);
        int y1 = std::min (y0 + th - 1, spec.height - 1);
        y0 += spec.y;
        y1 += spec.y;
        // Read the whole tile-row worth of scanlines
        ok = m_input->read_scanlines (y0, y1+1, z, format, (void *)&buf[0],
                                      pixelsize, scanlinesize);
        if (! ok)
            imagecache().error ("%s", m_input->geterror().c_str());
        size_t b = (y1-y0+1) * spec.scanline_bytes();
        thread_info->m_stats.bytes_read += b;
        m_bytesread += b;
        ++m_tilesread;
        // At this point, we aren't reading from the file any longer,
        // and to avoid deadlock, we MUST release the input lock prior
        // to any attempt to add_tile_to_cache, lest another thread add
        // the same tile to the cache before us but need the input mutex
        // to actually read the texels before marking it as pixels_ready.
        unlock_input_mutex ();

        // For all tiles in the tile-row, enter them into the cache if not
        // already there.  Special case for the tile we're actually being
        // asked for -- save it in 'data' rather than adding a tile.
        int xx = x - spec.x;   // counting from left row
        int x0 = xx - (xx % tw); // start of the tile we are retrieving
        for (int i = 0;  i < spec.width;  i += tw) {
            if (i == xx) {
                // This is the tile we've been asked for
                convert_image (spec.nchannels, tw, th, 1,
                               &buf[x0 * pixelsize], format, pixelsize,
                               scanlinesize, scanlinesize*th, data, format,
                               xstride, ystride, zstride);
            } else {
                // Not the tile we asked for, but it's in the same
                // tile-row, so let's put it in the cache anyway so
                // it'll be there when asked for.
                TileID id (*this, subimage, miplevel, i+spec.x, y0, z);
                if (! imagecache().tile_in_cache (id, thread_info)) {
                    ImageCacheTileRef tile;
                    tile = new ImageCacheTile (id, &buf[i*pixelsize],
                                            format, pixelsize,
                                            scanlinesize, scanlinesize*th);
                    ok &= tile->valid ();
                    imagecache().add_tile_to_cache (tile, thread_info);
                }
            }
        }
        // The lock_guard inside the calling function, read_tile, passed
        // us the input_mutex locked, and expects to get it back the
        // same way, so we need to re-lock.
        lock_input_mutex ();
    } else {
        // No auto-tile -- the tile is the whole image
        ok = m_input->read_image (format, data, xstride, ystride, zstride);
        if (! ok)
            imagecache().error ("%s", m_input->geterror().c_str());
        size_t b = spec.image_bytes();
        thread_info->m_stats.bytes_read += b;
        m_bytesread += b;
        ++m_tilesread;
        // If we read the whole image, presumably we're done, so release
        // the file handle.
        close ();
    }

    return ok;
}



void
ImageCacheFile::close ()
{
    // N.B. close() does not need to lock the m_input_mutex, because close()
    // itself is only called by routines that hold the lock.
    if (opened()) {
        m_input->close ();
        m_input.reset ();
        m_imagecache.decr_open_files ();
    }
}



void
ImageCacheFile::release ()
{
    recursive_lock_guard guard (m_input_mutex);
    if (m_used)
        m_used = false;
    else
        close ();
}



void
ImageCacheFile::invalidate ()
{
    recursive_lock_guard guard (m_input_mutex);
    close ();
    invalidate_spec ();
    m_broken = false;
    m_fingerprint.clear ();
    duplicate (NULL);

    m_filename = m_imagecache.resolve_filename (m_filename_original.string());

    // Eat any errors that occurred in the open/close
    while (! imagecache().geterror().empty())
        ;
}



ImageCacheFile *
ImageCacheImpl::find_file (ustring filename,
                           ImageCachePerThreadInfo *thread_info,
                           ImageInput::Creator creator,
                           bool header_only)
{
    // Debugging aid: attribute "substitute_image" forces all image
    // references to be to one named file.
    if (m_substitute_image)
        filename = m_substitute_image;

    // Shortcut - check the per-thread microcache before grabbing a more
    // expensive lock on the shared file cache.
    ImageCacheFile *tf = thread_info->find_file (filename);

    // Part 1 - make sure the ImageCacheFile entry exists and is in the
    // file cache.  For this part, we need to lock the file cache.
    bool newfile = false;
    if (! tf) {  // was not found in microcache
#if IMAGECACHE_TIME_STATS
        Timer timer;
#endif
        size_t bin = m_files.lock_bin (filename);
        FilenameMap::iterator found = m_files.find (filename, false);
        if (found) {
            tf = found->second.get();
        } else {
            // No such entry in the file cache.  Add it, but don't open yet.
            tf = new ImageCacheFile (*this, thread_info, filename, creator);
            m_files.insert (filename, tf, false);
            newfile = true;
        }
        m_files.unlock_bin (bin);

        if (newfile)
            check_max_files (thread_info);
        thread_info->filename (filename, tf);  // add to the microcache
#if IMAGECACHE_TIME_STATS
        thread_info->m_stats.find_file_time += timer();
#endif
    }

    // Part 2 - open tihe file if it's never been opened before.
    // No need to have the file cache locked for this, though we lock
    // the tf->m_input_mutex if we need to open it.
    if (! tf->validspec()) {
        Timer timer;
        recursive_lock_guard guard (tf->m_input_mutex);
        if (! tf->validspec()) {
            tf->open (thread_info);
            DASSERT (tf->m_broken || tf->validspec());
            double createtime = timer();
            ImageCacheStatistics &stats (thread_info->m_stats);
            stats.fileio_time += createtime;
            stats.fileopen_time += createtime;
            tf->iotime() += createtime;

            // What if we've opened another file, with a different name,
            // but the SAME pixels?  It can happen!  Bad user, bad!  But
            // let's save them from their own foolishness.
            if (tf->fingerprint() && m_deduplicate) {
                // std::cerr << filename << " hash=" << tf->fingerprint() << "\n";
                ImageCacheFile *dup = find_fingerprint (tf->fingerprint(), tf);
                if (dup != tf) {
                    // Already in fingerprints -- mark this one as a
                    // duplicate, but ONLY if we don't have other
                    // reasons not to consider them true duplicates (the
                    // fingerprint only considers source image pixel values.
                    // FIXME -- be sure to add extra tests
                    // here if more metadata have significance later!
                    bool match = (tf->subimages() == dup->subimages());
                    match &= (tf->m_swrap == dup->m_swrap &&
                              tf->m_twrap == dup->m_twrap &&
                              tf->m_rwrap == dup->m_rwrap &&
                              tf->m_envlayout == dup->m_envlayout &&
                              tf->m_y_up == dup->m_y_up &&
                              tf->m_sample_border == dup->m_sample_border);
                    for (int s = 0, e = tf->subimages(); match && s < e; ++s) {
                        match &= (tf->datatype(s) == dup->datatype(s));
                    }
                    if (match) {
                        tf->duplicate (dup);
                        tf->close ();
                        // std::cerr << "  duplicates " 
                        //   << fingerfound->second.get()->filename() << "\n";
                    }
                }
            }
#if IMAGECACHE_TIME_STATS
            stats.find_file_time += timer()-createtime;
#endif
        }
    }

    // if this is a duplicate texture, switch to the canonical copy
    if (tf->duplicate()) {
        if (! header_only)
            tf = tf->duplicate();
        // N.B. If looking up header info (i.e., get_image_info, rather
        // than getting pixels, use the original not the duplicate, since
        // metadata may differ even if pixels are identical).
    } else {
        // not a duplicate -- if opening the first time, count as unique
        if (newfile)
            ++thread_info->m_stats.unique_files;
    }

    if (! header_only)
        tf->use ();  // Mark it as recently used
    return tf;
}



ImageCacheFile *
ImageCacheImpl::find_fingerprint (ustring finger, ImageCacheFile *file)
{
    spin_lock lock (m_fingerprints_mutex);
    FingerprintMap::iterator found = m_fingerprints.find (finger);
    if (found == m_fingerprints.end()) {
        // Not already in the fingerprint list -- add it
        m_fingerprints[finger] = file;
    } else {
        // In the list -- return its mapping
        file = found->second.get();
    }
    return file;
}



void
ImageCacheImpl::clear_fingerprints ()
{
    spin_lock lock (m_fingerprints_mutex);
    m_fingerprints.clear ();
}



void
ImageCacheImpl::check_max_files (ImageCachePerThreadInfo *thread_info)
{
#if 0
    if (! (m_stat_open_files_created % 16) || m_stat_open_files_current >= m_max_open_files) {
        std::cerr << "open files " << m_stat_open_files_current << ", max = " << m_max_open_files << "\n";
    std::cout << "    ImageInputs : " << m_stat_open_files_created << " created, " << m_stat_open_files_current << " current, " << m_stat_open_files_peak << " peak\n";
    }
#endif

    // Early out if we aren't exceeding the open file handle limit
    if (m_stat_open_files_current < m_max_open_files)
        return;

    // Try to grab the file_sweep_mutex lock. If somebody else holds it,
    // just return -- leave the handle limit enforcement to whomever is
    // already in this function, no need for two threads to do it at
    // once.  If this means we may ephemerally be over the handle limit,
    // so be it.
    if (! m_file_sweep_mutex.try_lock())
        return;

    // Now, what we want to do is have a "clock hand" that sweeps across
    // the cache, releasing files that haven't been used for a long
    // time.  Because of multi-thread, rather than keep an iterator
    // around for this (which could be invalidated since the last time
    // we used it), we just remember the filename of the next file to
    // check, then look it up fresh.  That is m_file_sweep_name.

    // If we don't have a valid file_sweep_name, establish it by just
    // looking up the filename of the first entry in the file cache.
    if (! m_file_sweep_name) {
        FilenameMap::iterator sweep = m_files.begin();
        ASSERT (sweep != m_files.end() &&
                "no way m_files can be empty and have too many files open");
        m_file_sweep_name = sweep->first;
    }

    // Get a (locked) iterator for the next file to be examined.
    FilenameMap::iterator sweep = m_files.find (m_file_sweep_name);

    // Loop while we still have too many files open.  Also, be careful
    // of looping for too long, exit the loop if we just keep spinning
    // uncontrollably.
    int full_loops = 0;
    FilenameMap::iterator end = m_files.end();
    while (m_stat_open_files_current >= m_max_open_files
           && full_loops <= 100) {
        // If we have fallen off the end of the cache, loop back to the
        // beginning and increment our full_loops count.
        if (sweep == end) {
            sweep = m_files.begin();
            ++full_loops;
        }
        // If we're STILL at the end, it must be that somehow the entire
        // cache is empty.  So just declare ourselves done.
        if (sweep == end)
            break;
        DASSERT (sweep->second);
        sweep->second->release ();  // May reduce open files
        ++sweep;
    }

    // OK, by this point we have either closed enough files to be below
    // the limit again, or the cache is empty, or we've looped over the
    // cache too many times and are giving up.

    // Now we must save the filename for next time.  Just set it to an
    // empty string if we don't have a valid iterator at this point.
    m_file_sweep_name = (sweep == end ? ustring() : sweep->first);
    m_file_sweep_mutex.unlock ();

    // N.B. As we exit, the iterators will go out of scope and we will
    // retain no locks on the cache.
}



void
ImageCacheImpl::set_min_cache_size (long long newsize)
{
    long long oldsize = m_max_memory_bytes;
    while (newsize > oldsize) {
	if (atomic_compare_and_exchange ((long long *)&m_max_memory_bytes,
                                         oldsize, newsize))
            return;
        oldsize = m_max_memory_bytes;
    }
}



ImageCacheTile::ImageCacheTile (const TileID &id,
                                ImageCachePerThreadInfo *thread_info,
                                bool read_now)
    : m_id (id), m_valid(true) // , m_used(true)
{
    m_used = true;
    m_pixels_ready = false;
    m_pixels_size = 0;
    if (read_now) {
        read (thread_info);
    }
    id.file().imagecache().incr_tiles (0);  // mem counted separately in read
}



ImageCacheTile::ImageCacheTile (const TileID &id, const void *pels,
                    TypeDesc format,
                    stride_t xstride, stride_t ystride, stride_t zstride)
    : m_id (id) // , m_used(true)
{
    m_used = true;
    m_pixels_size = 0;
    ImageCacheFile &file (m_id.file ());
    const ImageSpec &spec (file.spec(id.subimage(), id.miplevel()));
    size_t size = memsize_needed ();
    ASSERT_MSG (size > 0 && memsize() == 0, "size was %llu, memsize = %llu",
                (unsigned long long)size, (unsigned long long)memsize());
    m_pixels.reset (new char [m_pixels_size = size]);
    size_t dst_pelsize = file.pixelsize(id.subimage());
    m_valid = convert_image (spec.nchannels, spec.tile_width, spec.tile_height,
                             spec.tile_depth, pels, format, xstride, ystride,
                             zstride, &m_pixels[0], file.datatype(id.subimage()),
                             dst_pelsize, dst_pelsize * spec.tile_width,
                             dst_pelsize * spec.tile_width * spec.tile_height);
    id.file().imagecache().incr_tiles (size);
    m_pixels_ready = true;  // Caller sent us the pixels, no read necessary
    // FIXME -- for shadow, fill in mindepth, maxdepth
}



ImageCacheTile::~ImageCacheTile ()
{
    m_id.file().imagecache().decr_tiles (memsize ());
}



void
ImageCacheTile::read (ImageCachePerThreadInfo *thread_info)
{
    size_t size = memsize_needed ();
    ASSERT (memsize() == 0 && size > 0);
    m_pixels.reset (new char [m_pixels_size = size]);
    ImageCacheFile &file (m_id.file());
    m_valid = file.read_tile (thread_info, m_id.subimage(), m_id.miplevel(),
                              m_id.x(), m_id.y(), m_id.z(),
                              file.datatype(m_id.subimage()), &m_pixels[0]);
    m_id.file().imagecache().incr_mem (size);
    if (! m_valid) {
        m_used = false;  // Don't let it hold mem if invalid
#if 0
        std::cerr << "(1) error reading tile " << m_id.x() << ' ' << m_id.y()
                  << ' ' << m_id.z()
                  << " subimg=" << m_id.subimage()
                  << " mip=" << m_id.miplevel()
                  << " from " << file.filename() << "\n";
#endif
    }
    m_pixels_ready = true;
    // FIXME -- for shadow, fill in mindepth, maxdepth
}



void
ImageCacheTile::wait_pixels_ready () const
{
    atomic_backoff backoff;
    while (! m_pixels_ready) {
        backoff();
    }
}



const void *
ImageCacheTile::data (int x, int y, int z) const
{
    const ImageSpec &spec = m_id.file().spec (m_id.subimage(), m_id.miplevel());
    size_t w = spec.tile_width;
    size_t h = spec.tile_height;
    size_t d = spec.tile_depth;
    DASSERT (d >= 1);
    x -= m_id.x();
    y -= m_id.y();
    z -= m_id.z();
    if (x < 0 || x >= (int)w || y < 0 || y >= (int)h || z < 0 || z >= (int)d)
        return NULL;
    size_t offset = ((z * h + y) * w + x) * m_id.file().pixelsize(m_id.subimage());
    return (const void *)&m_pixels[offset];
}



ImageCacheImpl::ImageCacheImpl ()
    : m_perthread_info (&cleanup_perthread_info)
{
    init ();
}



void
ImageCacheImpl::init ()
{
    m_max_open_files = 100;
    m_max_memory_bytes = 256 * 1024 * 1024;   // 256 MB default cache size
    m_autotile = 0;
    m_autoscanline = false;
    m_automip = false;
    m_forcefloat = false;
    m_accept_untiled = true;
    m_accept_unmipped = true;
    m_read_before_insert = false;
    m_deduplicate = true;
    m_unassociatedalpha = false;
    m_failure_retries = 0;
    m_latlong_y_up_default = true;
    m_Mw2c.makeIdentity();
    m_mem_used = 0;
    m_statslevel = 0;
    m_stat_tiles_created = 0;
    m_stat_tiles_current = 0;
    m_stat_tiles_peak = 0;
    m_stat_open_files_created = 0;
    m_stat_open_files_current = 0;
    m_stat_open_files_peak = 0;

    // Allow environment variable to override default options
    const char *options = getenv ("OPENIMAGEIO_IMAGECACHE_OPTIONS");
    if (options)
        attribute ("options", options);
}



ImageCacheImpl::~ImageCacheImpl ()
{
    printstats ();
    erase_perthread_info ();
}



void
ImageCacheImpl::mergestats (ImageCacheStatistics &stats) const
{
    stats.init ();
    lock_guard lock (m_perthread_info_mutex);
    for (size_t i = 0;  i < m_all_perthread_info.size();  ++i)
        stats.merge (m_all_perthread_info[i]->m_stats);
}



std::string
ImageCacheImpl::onefile_stat_line (const ImageCacheFileRef &file,
                                   int i, bool includestats) const
{
    // FIXME -- make meaningful stat printouts for multi-image textures
    std::ostringstream out;
    const ImageSpec &spec (file->spec(0,0));
    const char *formatcode = "u8";
    switch (spec.format.basetype) {
    case TypeDesc::UINT8  : formatcode = "u8 ";  break;
    case TypeDesc::INT8   : formatcode = "i8 ";  break;
    case TypeDesc::UINT16 : formatcode = "u16"; break;
    case TypeDesc::INT16  : formatcode = "i16"; break;
    case TypeDesc::UINT   : formatcode = "u32"; break;
    case TypeDesc::INT    : formatcode = "i32"; break;
    case TypeDesc::UINT64 : formatcode = "i64"; break;
    case TypeDesc::INT64  : formatcode = "u64"; break;
    case TypeDesc::HALF   : formatcode = "f16"; break;
    case TypeDesc::FLOAT  : formatcode = "f32"; break;
    case TypeDesc::DOUBLE : formatcode = "f64"; break;
    default: break;
    }
    if (i >= 0)
        out << Strutil::format ("%7d ", i);
    if (includestats)
        out << Strutil::format ("%4llu    %5llu   %6.1f %9s  ",
                                (unsigned long long) file->timesopened(),
                                (unsigned long long) file->tilesread(),
                                file->bytesread()/1024.0/1024.0,
                                Strutil::timeintervalformat(file->iotime()).c_str());
    if (file->subimages() > 1)
        out << Strutil::format ("%3d face x%d.%s", file->subimages(),
                                spec.nchannels, formatcode);
    else
        out << Strutil::format ("%4dx%4dx%d.%s", spec.width, spec.height,
                                spec.nchannels, formatcode);
    out << "  " << file->filename();
    if (file->duplicate()) {
        out << " DUPLICATES " << file->duplicate()->filename();
        return out.str();
    }
    for (int s = 0;  s < file->subimages();  ++s)
        if (file->subimageinfo(s).untiled) {
            out << " UNTILED";
            break;
        }
    if (automip()) {
        // FIXME -- we should directly measure whether we ever automipped
        // this file.  This is a little inexact.
        for (int s = 0;  s < file->subimages();  ++s)
            if (file->subimageinfo(s).unmipped) {
                out << " UNMIPPED";
                break;
            }
    }
    if (! file->mipused()) {
        for (int s = 0;  s < file->subimages();  ++s)
            if (! file->subimageinfo(s).unmipped) {
                out << " MIP-UNUSED";
                break;
            }
    }
    if (file->mipreadcount().size() > 1) {
        out << " MIP-COUNT [";
        int nmip = (int) file->mipreadcount().size();
        for (int c = 0; c < nmip; c++)
            out << (c ? "," : "") << file->mipreadcount()[c];
        out << "]";
    }

    return out.str ();
}



std::string
ImageCacheImpl::getstats (int level) const
{
    // Merge all the threads
    ImageCacheStatistics stats;
    mergestats (stats);

    std::ostringstream out;
    if (level > 0) {
        out << "OpenImageIO ImageCache statistics (";
        {
            spin_lock guard (shared_image_cache_mutex);
            if ((void *)this == (void *)shared_image_cache.get())
                out << "shared";
            else
                out << (void *)this;
        }
        out << ") ver " << OIIO_VERSION_STRING << "\n";
        if (stats.unique_files) {
            out << "  Images : " << stats.unique_files << " unique\n";
            out << "    ImageInputs : " << m_stat_open_files_created << " created, " << m_stat_open_files_current << " current, " << m_stat_open_files_peak << " peak\n";
            out << "    Total size of all images referenced : " << Strutil::memformat (stats.files_totalsize) << "\n";
            out << "    Read from disk : " << Strutil::memformat (stats.bytes_read) << "\n";
        } else {
            out << "  No images opened\n";
        }
        if (stats.find_file_time > 0.001)
            out << "    Find file time : " << Strutil::timeintervalformat (stats.find_file_time) << "\n";
        if (stats.fileio_time > 0.001) {
            out << "    File I/O time : " 
                << Strutil::timeintervalformat (stats.fileio_time);
            {
                lock_guard lock (m_perthread_info_mutex);
                size_t nthreads = m_all_perthread_info.size();
                if (nthreads > 1) {
                    double perthreadtime = stats.fileio_time / (float)nthreads;
                    out << " (" << Strutil::timeintervalformat (perthreadtime)
                        << " average per thread)";
                }
            }
            out << "\n";
            out << "    File open time only : " 
                << Strutil::timeintervalformat (stats.fileopen_time) << "\n";
        }
        if (stats.file_locking_time > 0.001)
            out << "    File mutex locking time : " << Strutil::timeintervalformat (stats.file_locking_time) << "\n";
        if (m_stat_tiles_created > 0) {
            out << "  Tiles: " << m_stat_tiles_created << " created, " << m_stat_tiles_current << " current, " << m_stat_tiles_peak << " peak\n";
            out << "    total tile requests : " << stats.find_tile_calls << "\n";
            out << "    micro-cache misses : " << stats.find_tile_microcache_misses << " (" << 100.0*(double)stats.find_tile_microcache_misses/(double)stats.find_tile_calls << "%)\n";
            out << "    main cache misses : " << stats.find_tile_cache_misses << " (" << 100.0*(double)stats.find_tile_cache_misses/(double)stats.find_tile_calls << "%)\n";
        }
        out << "    Peak cache memory : " << Strutil::memformat (m_mem_used) << "\n";
        if (stats.tile_locking_time > 0.001)
            out << "    Tile mutex locking time : " << Strutil::timeintervalformat (stats.tile_locking_time) << "\n";
        if (stats.find_tile_time > 0.001)
            out << "    Find tile time : " << Strutil::timeintervalformat (stats.find_tile_time) << "\n";
        if (stats.file_retry_success || stats.tile_retry_success)
            out << "    Failure reads followed by unexplained success: "
                << stats.file_retry_success << " files, "
                << stats.tile_retry_success << " tiles\n";
    }

    // Gather file list and statistics
    size_t total_opens = 0, total_tiles = 0;
    imagesize_t total_bytes = 0;
    size_t total_untiled = 0, total_unmipped = 0, total_duplicates = 0;
    double total_iotime = 0;
    std::vector<ImageCacheFileRef> files;
    {
        for (FilenameMap::iterator f = m_files.begin(); f != m_files.end(); ++f) {
            const ImageCacheFileRef &file (f->second);
            files.push_back (file);
            total_opens += file->timesopened();
            total_tiles += file->tilesread();
            total_bytes += file->bytesread();
            total_iotime += file->iotime();
            if (file->duplicate()) {
                ++total_duplicates;
                continue;
            }
            bool found_untiled = false, found_unmipped = false;
            for (int s = 0;  s < file->subimages();  ++s) {
                found_untiled |= file->subimageinfo(s).untiled;
                found_unmipped |= file->subimageinfo(s).unmipped;
            }
            if (found_untiled)
                ++total_untiled;
            if (found_unmipped)
                ++total_unmipped;
        }
    }

    if (level >= 2 && files.size()) {
        out << "  Image file statistics:\n";
        out << "        opens   tiles  MB read  I/O time  res              File\n";
        std::sort (files.begin(), files.end(), filename_compare);
        for (size_t i = 0;  i < files.size();  ++i) {
            const ImageCacheFileRef &file (files[i]);
            ASSERT (file);
            if (file->broken() || file->subimages() == 0) {
                out << "  BROKEN                                                  " 
                    << file->filename() << "\n";
                continue;
            }
            out << onefile_stat_line (file, i+1) << "\n";
        }
        out << Strutil::format ("\n  Tot:  %4llu    %5llu   %6.1f %9s\n",
                                (unsigned long long) total_opens,
                                (unsigned long long) total_tiles,
                                total_bytes/1024.0/1024.0,
                                Strutil::timeintervalformat(total_iotime).c_str());
    }

    // Try to point out hot spots
    if (level > 0) {
        if (total_duplicates)
            out << "  " << total_duplicates << " were exact duplicates of other images\n";
        if (total_untiled || (total_unmipped && automip())) {
            out << "  " << total_untiled << " not tiled, "
                << total_unmipped << " not MIP-mapped\n";
#if 0
            if (files.size() >= 50) {
                out << "  Untiled/unmipped files were:\n";
                for (size_t i = 0;  i < files.size();  ++i) {
                    const ImageCacheFileRef &file (files[i]);
                    if (file->untiled() || (file->unmipped() && automip()))
                        out << onefile_stat_line (file, -1) << "\n";
                }
            }
#endif
        }
        if (files.size() >= 50) {
            const int topN = 3;
            std::sort (files.begin(), files.end(), bytesread_compare);
            out << "  Top files by bytes read:\n";
            for (int i = 0;  i < std::min<int> (topN, files.size());  ++i) {
                if (files[i]->broken() || !files[i]->validspec())
                    continue;
                out << Strutil::format ("    %d   %6.1f MB (%4.1f%%)  ", i+1,
                                        files[i]->bytesread()/1024.0/1024.0,
                                        100.0 * (files[i]->bytesread() / (double)total_bytes));
                out << onefile_stat_line (files[i], -1, false) << "\n";
            }
            std::sort (files.begin(), files.end(), iotime_compare);
            out << "  Top files by I/O time:\n";
            for (int i = 0;  i < std::min<int> (topN, files.size());  ++i) {
                if (files[i]->broken() || !files[i]->validspec())
                    continue;
                out << Strutil::format ("    %d   %9s (%4.1f%%)   ", i+1,
                                        Strutil::timeintervalformat (files[i]->iotime()).c_str(),
                                        100.0 * files[i]->iotime() / total_iotime);
                out << onefile_stat_line (files[i], -1, false) << "\n";
            }
            std::sort (files.begin(), files.end(), iorate_compare);
            out << "  Files with slowest I/O rates:\n";
            int n = 0;
            BOOST_FOREACH (const ImageCacheFileRef &file, files) {
                if (file->broken() || !file->validspec())
                    continue;
                if (file->iotime() < 0.25)
                    continue;
                double mb = file->bytesread()/(1024.0*1024.0);
                double r = mb / file->iotime();
                out << Strutil::format ("    %d   %6.2f MB/s (%.2fMB/%.2fs)   ", n+1, r, mb, file->iotime());
                out << onefile_stat_line (file, -1, false) << "\n";
                if (++n >= topN)
                    break;
            }
            if (n == 0)
                out << "    (nothing took more than 0.25s)\n";
            double fast = files.back()->bytesread()/(1024.0*1024.0) / files.back()->iotime();
            out << Strutil::format ("    (fastest was %.1f MB/s)\n", fast);
        }
    }

    return out.str();
}



void
ImageCacheImpl::printstats () const
{
    if (m_statslevel == 0)
        return;
    std::cout << getstats (m_statslevel) << "\n\n";
}



void
ImageCacheImpl::reset_stats ()
{
    {
        lock_guard lock (m_perthread_info_mutex);
        for (size_t i = 0;  i < m_all_perthread_info.size();  ++i)
            m_all_perthread_info[i]->m_stats.init ();
    }

    {
        for (FilenameMap::iterator f = m_files.begin(); f != m_files.end(); ++f) {
            const ImageCacheFileRef &file (f->second);
            file->m_timesopened = 0;
            file->m_tilesread = 0;
            file->m_bytesread = 0;
            file->m_iotime = 0;
        }
    }
}



bool
ImageCacheImpl::attribute (string_view name, TypeDesc type,
                           const void *val)
{
    bool do_invalidate = false;
    bool force_invalidate = false;
    if (name == "options" && type == TypeDesc::STRING) {
        return optparser (*this, *(const char **)val);
    }
    if (name == "max_open_files" && type == TypeDesc::INT) {
        m_max_open_files = *(const int *)val;
    }
    else if (name == "max_memory_MB" && type == TypeDesc::FLOAT) {
        float size = *(const float *)val;
#ifdef NDEBUG
        size = std::max (size, 10.0f);  // Don't let users choose < 10 MB
#else
        size = std::max (size, 1.0f);   // But let developers debugging do it
#endif
        m_max_memory_bytes = size_t(size * 1024 * 1024);
    }
    else if (name == "max_memory_MB" && type == TypeDesc::INT) {
        float size = *(const int *)val;
#ifdef NDEBUG
        size = std::max (size, 10.0f);  // Don't let users choose < 10 MB
#else
        size = std::max (size, 1.0f);   // But let developers debugging do it
#endif
        m_max_memory_bytes = size_t(size) * 1024 * 1024;
    }
    else if (name == "searchpath" && type == TypeDesc::STRING) {
        std::string s = std::string (*(const char **)val);
        if (s != m_searchpath) {
            m_searchpath = s;
            Filesystem::searchpath_split (m_searchpath, m_searchdirs, true);
            do_invalidate = true;   // in case file can be found with new path
            force_invalidate = true;
        }
    }
    else if (name == "plugin_searchpath" && type == TypeDesc::STRING) {
        m_plugin_searchpath = std::string (*(const char **)val);
    }
    else if (name == "statistics:level" && type == TypeDesc::INT) {
        m_statslevel = *(const int *)val;
    }
    else if (name == "autotile" && type == TypeDesc::INT) {
        int a = pow2roundup (*(const int *)val);  // guarantee pow2
        // Clamp to minimum 8x8 tiles to protect against stupid user who
        // think this is a boolean rather than the tile size.  Unless
        // we're in DEBUG mode, then allow developers to play with fire.
#ifdef NDEBUG
        if (a > 0 && a < 8)
            a = 8;
#endif
        if (a != m_autotile) {
            m_autotile = a;
            do_invalidate = true;
        }
    }
    else if (name == "autoscanline" && type == TypeDesc::INT) {
        int a = *(const int *)val;
        if (a != m_autoscanline) {
            m_autoscanline = a;
            do_invalidate = true;
        }
    }
    else if (name == "automip" && type == TypeDesc::INT) {
        int a = *(const int *)val;
        if (a != m_automip) {
            m_automip = a;
            do_invalidate = true;
        }
    }
    else if (name == "forcefloat" && type == TypeDesc::INT) {
        int a = *(const int *)val;
        if (a != m_forcefloat) {
            m_forcefloat = a;
            do_invalidate = true;
        }
    }
    else if (name == "accept_untiled" && type == TypeDesc::INT) {
        int a = *(const int *)val;
        if (a != m_accept_untiled) {
            m_accept_untiled = a;
            do_invalidate = true;
        }
    }
    else if (name == "accept_unmipped" && type == TypeDesc::INT) {
        int a = *(const int *)val;
        if (a != m_accept_unmipped) {
            m_accept_unmipped = a;
            do_invalidate = true;
        }
    }
    else if (name == "read_before_insert" && type == TypeDesc::INT) {
        int r = *(const int *)val;
        if (r != m_read_before_insert) {
            m_read_before_insert = r;
            do_invalidate = true;
        }
    }
    else if (name == "deduplicate" && type == TypeDesc::INT) {
        int r = *(const int *)val;
        if (r != m_deduplicate) {
            m_deduplicate = r;
            do_invalidate = true;
        }
    }
    else if (name == "unassociatedalpha" && type == TypeDesc::INT) {
        int r = *(const int *)val;
        if (r != m_unassociatedalpha) {
            m_unassociatedalpha = r;
            do_invalidate = true;
        }
    }
    else if (name == "failure_retries" && type == TypeDesc::INT) {
        m_failure_retries = *(const int *)val;
    }
    else if (name == "latlong_up" && type == TypeDesc::STRING) {
        bool y_up = ! strcmp ("y", *(const char **)val);
        if (y_up != m_latlong_y_up_default) {
            m_latlong_y_up_default = y_up;
            do_invalidate = true;
        }
    } else if (name == "substitute_image" && type == TypeDesc::STRING) {
        m_substitute_image = ustring (*(const char **)val);
        do_invalidate = true;
    } else {
        // Otherwise, unknown name
        return false;
    }

    if (do_invalidate)
        invalidate_all (force_invalidate);
    return true;
}



bool
ImageCacheImpl::getattribute (string_view name, TypeDesc type,
                              void *val)
{
#define ATTR_DECODE(_name,_ctype,_src)                                  \
    if (name == _name && type == BaseTypeFromC<_ctype>::value) {        \
        *(_ctype *)(val) = (_ctype)(_src);                              \
        return true;                                                    \
    }

    ATTR_DECODE ("max_open_files", int, m_max_open_files);
    ATTR_DECODE ("max_memory_MB", float, m_max_memory_bytes/(1024.0*1024.0));
    ATTR_DECODE ("max_memory_MB", int, m_max_memory_bytes/(1024*1024));
    ATTR_DECODE ("statistics:level", int, m_statslevel);
    ATTR_DECODE ("autotile", int, m_autotile);
    ATTR_DECODE ("autoscanline", int, m_autoscanline);
    ATTR_DECODE ("automip", int, m_automip);
    ATTR_DECODE ("forcefloat", int, m_forcefloat);
    ATTR_DECODE ("accept_untiled", int, m_accept_untiled);
    ATTR_DECODE ("accept_unmipped", int, m_accept_unmipped);
    ATTR_DECODE ("read_before_insert", int, m_read_before_insert);
    ATTR_DECODE ("deduplicate", int, m_deduplicate);
    ATTR_DECODE ("unassociatedalpha", int, m_unassociatedalpha);
    ATTR_DECODE ("failure_retries", int, m_failure_retries);

    // The cases that don't fit in the simple ATTR_DECODE scheme
    if (name == "searchpath" && type == TypeDesc::STRING) {
        *(ustring *)val = m_searchpath;
        return true;
    }
    if (name == "plugin_searchpath" && type == TypeDesc::STRING) {
        *(ustring *)val = m_plugin_searchpath;
        return true;
    }
    if (name == "worldtocommon" && (type == TypeDesc::TypeMatrix ||
                                    type == TypeDesc(TypeDesc::FLOAT,16))) {
        *(Imath::M44f *)val = m_Mw2c;
        return true;
    }
    if (name == "commontoworld" && (type == TypeDesc::TypeMatrix ||
                                    type == TypeDesc(TypeDesc::FLOAT,16))) {
        *(Imath::M44f *)val = m_Mc2w;
        return true;
    }
    if (name == "latlong_up" && type == TypeDesc::STRING) {
        *(const char **)val = ustring (m_latlong_y_up_default ? "y" : "z").c_str();
        return true;
    }
    if (name == "substitute_image" && type == TypeDesc::STRING) {
        *(const char **)val = m_substitute_image.c_str();
        return true;
    }

    // Stats we can just grab
    ATTR_DECODE ("stat:cache_memory_used", long long, m_mem_used);
    ATTR_DECODE ("stat:tiles_created", int, m_stat_tiles_created);
    ATTR_DECODE ("stat:tiles_current", int, m_stat_tiles_current);
    ATTR_DECODE ("stat:tiles_peak", int, m_stat_tiles_peak);
    ATTR_DECODE ("stat:open_files_created", int, m_stat_open_files_created);
    ATTR_DECODE ("stat:open_files_current", int, m_stat_open_files_current);
    ATTR_DECODE ("stat:open_files_peak", int, m_stat_open_files_peak);

    if (boost::algorithm::starts_with(name, "stat:")) {
        // All the other stats are those that need to be summed from all
        // the threads.
        ImageCacheStatistics stats;
        mergestats (stats);
        ATTR_DECODE ("stat:find_tile_calls", long long, stats.find_tile_calls);
        ATTR_DECODE ("stat:find_tile_microcache_misses", long long, stats.find_tile_microcache_misses);
        ATTR_DECODE ("stat:find_tile_cache_misses", int, stats.find_tile_cache_misses);
        ATTR_DECODE ("stat:files_totalsize", long long, stats.files_totalsize);
        ATTR_DECODE ("stat:bytes_read", long long, stats.bytes_read);
        ATTR_DECODE ("stat:unique_files", int, stats.unique_files);
        ATTR_DECODE ("stat:fileio_time", float, stats.fileio_time);
        ATTR_DECODE ("stat:fileopen_time", float, stats.fileopen_time);
        ATTR_DECODE ("stat:file_locking_time", float, stats.file_locking_time);
        ATTR_DECODE ("stat:tile_locking_time", float, stats.tile_locking_time);
        ATTR_DECODE ("stat:find_file_time", float, stats.find_file_time);
        ATTR_DECODE ("stat:find_tile_time", float, stats.find_tile_time);
    }

    return false;
#undef ATTR_DECODE
}



bool
ImageCacheImpl::find_tile_main_cache (const TileID &id, ImageCacheTileRef &tile,
                           ImageCachePerThreadInfo *thread_info)
{
    DASSERT (! id.file().broken());
    ImageCacheStatistics &stats (thread_info->m_stats);

    ++stats.find_tile_microcache_misses;

    {
#if IMAGECACHE_TIME_STATS
        Timer timer1;
#endif
        TileCache::iterator found = m_tilecache.find (id);
#if IMAGECACHE_TIME_STATS
        stats.find_tile_time += timer1();
#endif
        if (found) {
            tile = (*found).second;
            found.unlock();  // release the lock
            // We found the tile in the cache, but we need to make sure we
            // wait until the pixels are ready to read.  We purposely have
            // released the lock (above) before calling wait_pixels_ready,
            // otherwise we could deadlock if another thread reading the
            // pixels needs to lock the cache because it's doing automip.
            tile->wait_pixels_ready ();
            tile->use ();
            DASSERT (id == tile->id());
            DASSERT (tile);
            return true;
        }
    }

    // The tile was not found in cache.

    ++stats.find_tile_cache_misses;

    // Yes, we're creating and reading a tile with no lock -- this is to
    // prevent all the other threads from blocking because of our
    // expensive disk read.  We believe this is safe, since underneath
    // the ImageCacheFile will lock itself for the read_tile and there are
    // no other non-threadsafe side effects.
    Timer timer;
    tile = new ImageCacheTile (id, thread_info, m_read_before_insert);
    // N.B. the ImageCacheTile ctr starts the tile out as 'used'
    DASSERT (tile);
    DASSERT (id == tile->id());
    double readtime = timer();
    stats.fileio_time += readtime;
    id.file().iotime() += readtime;

    add_tile_to_cache (tile, thread_info);
    DASSERT (id == tile->id());
    return tile->valid();
}



void
ImageCacheImpl::add_tile_to_cache (ImageCacheTileRef &tile,
                                   ImageCachePerThreadInfo *thread_info)
{
    bool ourtile = true;
    {
        // Protect us from using too much memory if another thread added the
        // same tile just before us
        TileCache::iterator found = m_tilecache.find (tile->id());
        if (found != m_tilecache.end ()) {
            // Already added!  Use the other one, discard ours.
            tile = (*found).second;
            found.unlock ();
            ourtile = false;  // Don't need to add it
        } else {
            // Still not in cache, add ours to the cache.
            // N.B. at this time, we do not hold any locks.
            check_max_mem (thread_info);
            m_tilecache.insert (tile->id(), tile);
        }
    }

    // At this point, we no longer have the write lock, and we are no
    // longer modifying the cache itself.  However, if we added a new
    // tile to the cache, we may still need to read the pixels; and if
    // we found the tile in cache, we may need to wait for somebody else
    // to read the pixels.
    if (ourtile) {
        if (! tile->pixels_ready ()) {
            Timer timer;
            tile->read (thread_info);
            double readtime = timer();
            thread_info->m_stats.fileio_time += readtime;
            tile->id().file().iotime() += readtime;
        }
    } else {
        tile->wait_pixels_ready ();
    }
}



void
ImageCacheImpl::check_max_mem (ImageCachePerThreadInfo *thread_info)
{
    DASSERT (m_mem_used < (long long)m_max_memory_bytes*10); // sanity
#if 0
    static atomic_int n;
    if (! (n++ % 64) || m_mem_used >= (long long)m_max_memory_bytes)
        std::cerr << "mem used: " << m_mem_used << ", max = " << m_max_memory_bytes << "\n";
#endif
    // Early out if the cache is empty
    if (m_tilecache.empty())
        return;
    // Early out if we aren't exceeding the tile memory limit
    if (m_mem_used < (long long)m_max_memory_bytes)
        return;

    // Try to grab the tile_sweep_mutex lock. If somebody else holds it,
    // just return -- leave the memory limit enforcement to whomever is
    // already in this function, no need for two threads to do it at
    // once.  If this means we may ephemerally be over the memory limit
    // (because another thread adds a tile before we have freed enough
    // here), so be it.
    if (! m_tile_sweep_mutex.try_lock())
        return;

    // Now, what we want to do is have a "clock hand" that sweeps across
    // the cache, releasing tiles that haven't been used for a long
    // time.  Because of multi-thread, rather than keep an iterator
    // around for this (which could be invalidated since the last time
    // we used it), we just remember the tileID of the next tile to
    // check, then look it up fresh.  That is m_tile_sweep_id.

    // If we don't have a valid tile_sweep_id, establish it by just
    // looking up the first entry in the tile cache.
    if (m_tile_sweep_id.empty()) {
        TileCache::iterator sweep = m_tilecache.begin();
        ASSERT (sweep != m_tilecache.end() &&
                "no way m_tilecache can be empty and use too much memory");
        m_tile_sweep_id = (*sweep).first;
    }

    // Get a (locked) iterator for the next tile to be examined.
    TileCache::iterator sweep = m_tilecache.find (m_tile_sweep_id);

    // Loop while we still use too much tile memory.  Also, be careful
    // of looping for too long, exit the loop if we just keep spinning
    // uncontrollably.
    int full_loops = 0;
    TileCache::iterator end = m_tilecache.end();
    while (m_mem_used >= (long long)m_max_memory_bytes
           && full_loops < 100) {
        // If we have fallen off the end of the cache, loop back to the
        // beginning and increment our full_loops count.
        if (sweep == end) {
            sweep = m_tilecache.begin();
            ++full_loops;
        }
        // If we're STILL at the end, it must be that somehow the entire
        // cache is empty.  So just declare ourselves done.
        if (sweep == end)
            break;
        DASSERT (sweep->second);

        if (! sweep->second->release ()) {
            // This is a tile we should delete.  To keep iterating
            // safely, we have a good trick:
            // 1. remember the TileID of the tile to delete
            TileID todelete = sweep->first;
            size_t size = sweep->second->memsize();
            ASSERT (m_mem_used >= (long long)size);
            // 2. Increment the iterator to the next item to be visited
            // in the cache and then unlock it (since it can't be locked
            // for the subsequent erase() call).
            ++sweep;
            sweep.unlock ();
            // 3. Erase the tile we wish to delete
            m_tilecache.erase (todelete);
                // std::cerr << "  Freed tile, recovering " << size << "\n";
            // 4. Re-lock the iterator, which now points to the next
            // item the from the cache to examine.
            sweep.lock ();
        } else {
            ++sweep;
        }
    }

    // OK, by this point we have either freed enough tiles to be below
    // the limit again, or the cache is empty, or we've looped over the
    // cache too many times and are giving up.

    // Now we must save the tileid for next time.  Just set it to an
    // empty ID if we don't have a valid iterator at this point.
    m_tile_sweep_id = (sweep == end ? TileID() : sweep->first);
    m_tile_sweep_mutex.unlock ();

    // N.B. As we exit, the iterators will go out of scope and we will
    // retain no locks on the cache.
}



std::string
ImageCacheImpl::resolve_filename (const std::string &filename) const
{
    std::string s = Filesystem::searchpath_find (filename, m_searchdirs, true);
    return s.empty() ? filename : s;
}



bool
ImageCacheImpl::get_image_info (ustring filename, int subimage, int miplevel,
                                ustring dataname,
                                TypeDesc datatype, void *data)
{
    ImageCachePerThreadInfo *thread_info = get_perthread_info ();
    ImageCacheFile *file = find_file (filename, thread_info, NULL, true);
    if (dataname == s_exists && datatype == TypeDesc::TypeInt) {
        // Just check for existence.  Need to do this before the invalid
        // file error below, since in this one case, it's not an error
        // for the file to be nonexistant or broken!
        *(int *)data = (file && !file->broken());
        (void) geterror();  // eat any error generated by find_file
        return true;
    }
    if (!file || file->broken()) {
        error ("Invalid image file \"%s\"", filename.c_str());
        return false;
    }
    if (dataname == s_subimages && datatype == TypeDesc::TypeInt) {
        *(int *)data = file->subimages();
        return true;
    }
    
    const ImageSpec &spec (file->spec(subimage,miplevel));
    if (dataname == s_resolution && datatype==TypeDesc(TypeDesc::INT,2)) {
        int *d = (int *)data;
        d[0] = spec.width;
        d[1] = spec.height;
        return true;
    }
    if (dataname == s_resolution && datatype==TypeDesc(TypeDesc::INT,3)) {
        int *d = (int *)data;
        d[0] = spec.width;
        d[1] = spec.height;
        d[2] = spec.depth;
        return true;
    }
    if (dataname == s_texturetype && datatype == TypeDesc::TypeString) {
        ustring s (texture_type_name (file->textureformat()));
        *(const char **)data = s.c_str();
        return true;
    }
    if (dataname == s_textureformat && datatype == TypeDesc::TypeString) {
        ustring s (texture_format_name (file->textureformat()));
        *(const char **)data = s.c_str();
        return true;
    }
    if (dataname == s_fileformat && datatype == TypeDesc::TypeString) {
        *(const char **)data = file->fileformat().c_str();
        return true;
    }
    if (dataname == s_channels && datatype == TypeDesc::TypeInt) {
        *(int *)data = spec.nchannels;
        return true;
    }
    if (dataname == s_channels && datatype == TypeDesc::TypeFloat) {
        *(float *)data = spec.nchannels;
        return true;
    }
    if (dataname == s_format && datatype == TypeDesc::TypeInt) {
        *(int *)data = (int) spec.format.basetype;
        return true;
    }
    if ((dataname == s_cachedformat || dataname == s_cachedpixeltype) &&
            datatype == TypeDesc::TypeInt) {
        *(int *)data = (int) file->datatype(subimage).basetype;
        return true;
    }
    if (dataname == s_miplevels && datatype == TypeDesc::TypeInt) {
        *(int *)data = file->miplevels(subimage);
        return true;
    }
    if (dataname == s_datawindow && datatype.basetype == TypeDesc::INT &&
        (datatype == TypeDesc(TypeDesc::INT,4) ||
         datatype == TypeDesc(TypeDesc::INT,6))) {
        int *d = (int *)data;
        if (datatype.arraylen == 4) {
            d[0] = spec.x;
            d[1] = spec.y;
            d[2] = spec.x + spec.width - 1;
            d[3] = spec.y + spec.height - 1;
        } else {
            d[0] = spec.x;
            d[1] = spec.y;
            d[2] = spec.z;
            d[3] = spec.x + spec.width - 1;
            d[4] = spec.y + spec.height - 1;
            d[5] = spec.z + spec.depth - 1;
        }
    }
    if (dataname == s_displaywindow && datatype.basetype == TypeDesc::INT &&
        (datatype == TypeDesc(TypeDesc::INT,4) ||
         datatype == TypeDesc(TypeDesc::INT,6))) {
        int *d = (int *)data;
        if (datatype.arraylen == 4) {
            d[0] = spec.full_x;
            d[1] = spec.full_y;
            d[2] = spec.full_x + spec.full_width - 1;
            d[3] = spec.full_y + spec.full_height - 1;
        } else {
            d[0] = spec.full_x;
            d[1] = spec.full_y;
            d[2] = spec.full_z;
            d[3] = spec.full_x + spec.full_width - 1;
            d[4] = spec.full_y + spec.full_height - 1;
            d[5] = spec.full_z + spec.full_depth - 1;
        }
    }

    // general case -- handle anything else that's able to be found by
    // spec.find_attribute().
    const ImageIOParameter *p = spec.find_attribute (dataname.string());
    if (p && p->type().arraylen == datatype.arraylen) {
        // First test for exact type match
        if (p->type() == datatype) {
            memcpy (data, p->data(), datatype.size());
            return true;
        }
        // If the real data is int but user asks for float, translate it
        if (p->type().basetype == TypeDesc::FLOAT &&
                datatype.basetype == TypeDesc::INT) {
            for (int i = 0;  i < p->type().arraylen;  ++i)
                ((float *)data)[i] = ((int *)p->data())[i];
            return true;
        }
    }

    return false;
}



bool
ImageCacheImpl::get_imagespec (ustring filename, ImageSpec &spec,
                               int subimage, int miplevel, bool native)
{
    const ImageSpec *specptr = imagespec (filename, subimage, miplevel, native);
    if (specptr) {
        spec = *specptr;
        return true;
    } else {
        return false;  // imagespec() already handled the errors
    }
}



const ImageSpec *
ImageCacheImpl::imagespec (ustring filename, int subimage, int miplevel,
                           bool native)
{
    ImageCachePerThreadInfo *thread_info = get_perthread_info ();
    ImageCacheFile *file = find_file (filename, thread_info, NULL, true);
    if (! file) {
        error ("Image file \"%s\" not found", filename.c_str());
        return NULL;
    }
    if (file->broken()) {
        error ("Invalid image file \"%s\"", filename.c_str());
        return NULL;
    }
    if (subimage < 0 || subimage >= file->subimages()) {
        error ("Unknown subimage %d (out of %d)", subimage, file->subimages());
        return NULL;
    }
    if (miplevel < 0 || miplevel >= file->miplevels(subimage)) {
        error ("Unknown mip level %d (out of %d)", miplevel,
               file->miplevels(subimage));
        return NULL;
    }
    const ImageSpec *spec = native ? &file->nativespec (subimage,miplevel)
                                   : &file->spec (subimage, miplevel);
    return spec;
}



int
ImageCacheImpl::subimage_from_name (ImageCacheFile *file, ustring subimagename)
{
    for (int s = 0, send = file->subimages();  s < send;  ++s) {
        if (file->subimageinfo(s).subimagename == subimagename)
            return s;
    }
    return -1;  // No matching subimage name
}



bool
ImageCacheImpl::get_pixels (ustring filename, int subimage, int miplevel,
                            int xbegin, int xend, int ybegin, int yend,
                            int zbegin, int zend,
                            TypeDesc format, void *result)
{
    return get_pixels (filename, subimage, miplevel,
                       xbegin, xend, ybegin, yend, zbegin, zend,
                       0, -1, format, result);
}



bool
ImageCacheImpl::get_pixels (ustring filename,
                            int subimage, int miplevel,
                            int xbegin, int xend, int ybegin, int yend,
                            int zbegin, int zend, int chbegin, int chend,
                            TypeDesc format, void *result, stride_t xstride,
                            stride_t ystride, stride_t zstride)
{
    ImageCachePerThreadInfo *thread_info = get_perthread_info ();
    ImageCacheFile *file = find_file (filename, thread_info);
    if (! file) {
        error ("Image file \"%s\" not found", filename.c_str());
        return false;
    }
    if (file->broken()) {
        error ("Invalid image file \"%s\"", filename.c_str());
        return false;
    }
    if (subimage < 0 || subimage >= file->subimages()) {
        error ("get_pixels asked for nonexistant subimage %d of \"%s\"",
               subimage, filename.c_str());
        return false;
    }
    if (miplevel < 0 || miplevel >= file->miplevels(subimage)) {
        error ("get_pixels asked for nonexistant MIP level %d of \"%s\"",
               miplevel, filename.c_str());
        return false;
    }

    return get_pixels (file, thread_info, subimage, miplevel,
                       xbegin, xend, ybegin, yend, zbegin, zend,
                       chbegin, chend, format, result,
                       xstride, ystride, zstride);
}



bool
ImageCacheImpl::get_pixels (ImageCacheFile *file, 
                            ImageCachePerThreadInfo *thread_info,
                            int subimage, int miplevel,
                            int xbegin, int xend, int ybegin, int yend,
                            int zbegin, int zend, int chbegin, int chend,
                            TypeDesc format, void *result,
                            stride_t xstride, stride_t ystride, stride_t zstride)
{
    const ImageSpec &spec (file->spec(subimage, miplevel));
    bool ok = true;

    // Compute channels and stride if not given (assume all channels,
    // contiguous data layout for strides).
    if (chbegin < 0 || chend < 0) {
        chbegin = 0;
        chend = spec.nchannels;
    }
    int nchans = chend - chbegin;
    ImageSpec::auto_stride (xstride, ystride, zstride, format, nchans,
                            xend-xbegin, yend-ybegin);

    // formatpixelsize, scanlinesize, and zplanesize assume contiguous
    // layout.  This may or may not be the same as the strides passed by
    // the caller.
    TypeDesc cachetype = file->datatype(subimage);
    const size_t cachesize = cachetype.size();
    const stride_t cache_stride = cachesize * spec.nchannels;
    size_t formatsize = format.size();
    stride_t formatpixelsize = nchans * formatsize;
    bool xcontig = (formatpixelsize == xstride && nchans == spec.nchannels);
    stride_t scanlinesize = (xend-xbegin) * formatpixelsize;
    stride_t zplanesize = (yend-ybegin) * scanlinesize;
    DASSERT (spec.depth >= 1 && spec.tile_depth >= 1);

    char *zptr = (char *)result;
    for (int z = zbegin;  z < zend;  ++z, zptr += zstride) {
        if (z < spec.z || z >= (spec.z+spec.depth)) {
            // nonexistant planes
            if (xstride == formatpixelsize && ystride == scanlinesize) {
                // Can zero out the plane in one shot
                memset (zptr, 0, zplanesize);
            } else {
                // Non-contiguous strides -- zero out individual pixels
                char *yptr = zptr;
                for (int y = ybegin;  y < yend;  ++y, yptr += ystride) {
                    char *xptr = yptr;
                    for (int x = xbegin;  x < xend;  ++x, xptr += xstride)
                        memset (xptr, 0, formatpixelsize);
                }
            }
            continue;
        }
        int old_tx = -100000, old_ty = -100000, old_tz = -100000;
        int tz = z - ((z - spec.z) % spec.tile_depth);
        char *yptr = zptr;
        int ty = ybegin - ((ybegin - spec.y) % spec.tile_height);
        int tyend = ty + spec.tile_height;
        for (int y = ybegin;  y < yend;  ++y, yptr += ystride) {
            if (y == tyend) {
                ty = tyend;
                tyend += spec.tile_height;
            }
            if (y < spec.y || y >= (spec.y+spec.height)) {
                // nonexistant scanlines
                if (xstride == formatpixelsize) {
                    // Can zero out the scanline in one shot
                    memset (yptr, 0, scanlinesize);
                } else {
                    // Non-contiguous strides -- zero out individual pixels
                    char *xptr = yptr;
                    for (int x = xbegin;  x < xend;  ++x, xptr += xstride)
                        memset (xptr, 0, formatpixelsize);
                }
                continue;
            }
            // int ty = y - ((y - spec.y) % spec.tile_height);
            char *xptr = yptr;
            const char *data = NULL;
            for (int x = xbegin;  x < xend;  ++x, xptr += xstride) {
                if (x < spec.x || x >= (spec.x+spec.width)) {
                    // nonexistant columns
                    memset (xptr, 0, formatpixelsize);
                    continue;
                }
                int tx = x - ((x - spec.x) % spec.tile_width);
                if (old_tx != tx || old_ty != ty || old_tz != tz) {
                    // Only do a find_tile and re-setup of the data
                    // pointer when we move across a tile boundary.
                    TileID tileid (*file, subimage, miplevel, tx, ty, tz);
                    ok &= find_tile (tileid, thread_info);
                    if (! ok)
                        return false;  // Just stop if file read failed
                    old_tx = tx;
                    old_ty = ty;
                    old_tz = tz;
                    data = NULL;
                }
                if (! data) {
                    ImageCacheTileRef &tile (thread_info->tile);
                    ASSERT (tile);
                    data = (const char *)tile->data (x, y, z);
                    ASSERT (data);
                    data += chbegin*cachesize;
                }
                if (xcontig) {
                    // Special case for a contiguous span within one tile
                    int spanend = std::min (tx + spec.tile_width, xend);
                    stride_t span = spanend - x;
                    convert_types (cachetype, data, format, xptr, nchans*span);
                    x += (span-1);
                    xptr += xstride * (span-1);
                    // no need to increment data, since next read will
                    // be from a different tile
                } else {
                    convert_types (cachetype, data, format, xptr, nchans);
                    data += cache_stride;
                }
            }
        }
    }

    return ok;
}



ImageCache::Tile *
ImageCacheImpl::get_tile (ustring filename, int subimage, int miplevel,
                          int x, int y, int z)
{
    ImageCachePerThreadInfo *thread_info = get_perthread_info ();
    ImageCacheFile *file = find_file (filename, thread_info);
    if (! file || file->broken())
        return NULL;
    const ImageSpec &spec (file->spec(subimage,miplevel));
    // Snap x,y,z to the corner of the tile
    int xtile = (x-spec.x) / spec.tile_width;
    int ytile = (y-spec.y) / spec.tile_height;
    int ztile = (z-spec.z) / spec.tile_depth;
    x = spec.x + xtile * spec.tile_width;
    y = spec.y + ytile * spec.tile_height;
    z = spec.z + ztile * spec.tile_depth;
    TileID id (*file, subimage, miplevel, x, y, z);
    if (find_tile(id, thread_info)) {
        ImageCacheTileRef tile(thread_info->tile);
        tile->_incref();   // Fake an extra reference count
        return (ImageCache::Tile *) tile.get();
    } else {
        return NULL;
    }
}



void
ImageCacheImpl::release_tile (ImageCache::Tile *tile) const
{
    if (! tile)
        return;
    ImageCacheTileRef tileref((ImageCacheTile *)tile);
    tileref->use ();
    tileref->_decref();  // Reduce ref count that we bumped in get_tile
    // when we exit scope, tileref will do the final dereference
}



const void *
ImageCacheImpl::tile_pixels (ImageCache::Tile *tile, TypeDesc &format) const
{
    if (! tile)
        return NULL;
    ImageCacheTile * t = (ImageCacheTile *)tile;
    format = t->file().datatype(t->id().subimage());
    return t->data ();
}



bool
ImageCacheImpl::add_file (ustring filename, ImageInput::Creator creator)
{
    if (! creator) {
        error ("ImageCache::add_file must be given an ImageInput::Creator");
        return false;
    }
    ImageCachePerThreadInfo *thread_info = get_perthread_info ();
    ImageCacheFile *file = find_file (filename, thread_info, creator);
    if (!file || file->broken())
        return false;
    return true;
}



bool
ImageCacheImpl::add_tile (ustring filename, int subimage, int miplevel,
                          int x, int y, int z,
                          TypeDesc format, const void *buffer,
                          stride_t xstride, stride_t ystride, stride_t zstride)
{
    ImageCachePerThreadInfo *thread_info = get_perthread_info ();
    ImageCacheFile *file = find_file (filename, thread_info);
    if (! file || file->broken()) {
        error ("Cannot add_tile for an image file that was not set up with add_file()");
        return false;
    }

    TileID tileid (*file, subimage, miplevel, x, y, z);
    ImageCacheTileRef tile = new ImageCacheTile (tileid, buffer, format,
                                                 xstride, ystride, zstride);
    if (! tile || ! tile->valid()) {
        error ("Could not construct the tile; unknown reasons.");
        return false;
    }
    add_tile_to_cache (tile, thread_info);
    return true;
}



void
ImageCacheImpl::invalidate (ustring filename)
{
    ImageCacheFile *file = NULL;
    {
        FilenameMap::iterator fileit = m_files.find (filename);
        if (fileit != m_files.end())
            file = fileit->second.get();
        else
            return;  // no such file
    }

    // Iterate over the entire tilecache, record the TileID's of all
    // tiles that are from the file we are invalidating.
    std::vector<TileID> tiles_to_delete;
    for (TileCache::iterator tci = m_tilecache.begin(), e = m_tilecache.end();
             tci != e;  ++tci) {
        if (&(*tci).second->file() == file)
            tiles_to_delete.push_back ((*tci).second->id());
    }
    // N.B. at this point, we hold no locks!

    // Safely erase all the tiles we found
    BOOST_FOREACH (const TileID &id, tiles_to_delete) {
        m_tilecache.erase (id);
    }

    // Invalidate the file itself (close it and clear its spec)
    file->invalidate ();

    // Remove the fingerprint corresponding to this file
    {
        spin_lock lock (m_fingerprints_mutex);
        FingerprintMap::iterator f = m_fingerprints.find (filename);
        if (f != m_fingerprints.end())
            m_fingerprints.erase (f);
    }

    purge_perthread_microcaches ();
}



void
ImageCacheImpl::invalidate_all (bool force)
{
    // Special case: invalidate EVERYTHING -- we can take some shortcuts
    // to do it all in one shot.
    if (force) {
        // Clear the whole tile cache
        std::vector<TileID> tiles_to_delete;
        for (TileCache::iterator t = m_tilecache.begin(), e = m_tilecache.end();
             t != e;  ++t) {
            tiles_to_delete.push_back (t->second->id());
        }
        BOOST_FOREACH (const TileID &id, tiles_to_delete) {
            m_tilecache.erase (id);
        }
        // Invalidate (close and clear spec) all individual files
        for (FilenameMap::iterator fileit = m_files.begin(), e = m_files.end();
                 fileit != e;  ++fileit) {
            fileit->second->invalidate ();
        }
        // Clear fingerprints list
        clear_fingerprints ();
        // Mark the per-thread microcaches as invalid
        purge_perthread_microcaches ();
        return;
    }

    // Not forced... we need to look for particular files that seem
    // to need invalidation.

    // Make a list of all files that need to be invalidated
    std::vector<ustring> all_files;
    for (FilenameMap::iterator fileit = m_files.begin(), e = m_files.end();
             fileit != e;  ++fileit) {
        ImageCacheFileRef &f (fileit->second);
        ustring name = f->filename();
        recursive_lock_guard guard (f->m_input_mutex);
        // If the file was broken when we opened it, or if it no longer
        // exists, definitely invalidate it.
        if (f->broken() || ! Filesystem::exists(name.string())) {
            all_files.push_back (name);
            continue;
        }
        // Invalidate the file if it has been modified since it was
        // last opened.
        std::time_t t = Filesystem::last_write_time (name.string());
        if (t != f->mod_time()) {
            all_files.push_back (name);
            continue;
        }
        // Invalidate if any unmipped subimage...
        // ... didn't automip, but automip is now on
        // ... did automip, but automip is now off
        for (int s = 0;  s < f->subimages();  ++s) {
            ImageCacheFile::SubimageInfo &sub (f->subimageinfo(s));
            if (sub.unmipped &&
                ((m_automip && f->miplevels(s) <= 1) ||
                 (!m_automip && f->miplevels(s) > 1))) {
                all_files.push_back (name);
                break;
            }
        }
    }

    // Now, invalidate all the files in our "needs invalidation" list
    BOOST_FOREACH (ustring f, all_files) {
        // fprintf (stderr, "Invalidating %s\n", f.c_str());
        invalidate (f);
    }

    // Mark the per-thread microcaches as invalid
    purge_perthread_microcaches ();
}



ImageCachePerThreadInfo *
ImageCacheImpl::get_perthread_info ()
{
    ImageCachePerThreadInfo *p = m_perthread_info.get();
    if (! p) {
        p = new ImageCachePerThreadInfo;
        m_perthread_info.reset (p);
        // printf ("New perthread %p\n", (void *)p);
        lock_guard lock (m_perthread_info_mutex);
        m_all_perthread_info.push_back (p);
        p->shared = true;  // both the IC and the thread point to it
    }
    if (p->purge) {  // has somebody requested a tile purge?
        // This is safe, because it's our thread.
        lock_guard lock (m_perthread_info_mutex);
        p->tile = NULL;
        p->lasttile = NULL;
        p->purge = 0;
        for (int i = 0;  i < ImageCachePerThreadInfo::nlastfile;  ++i) {
            p->last_filename[i] = ustring();
            p->last_file[i] = NULL;
        }
    }
    return p;
}



void
ImageCacheImpl::erase_perthread_info ()
{
    lock_guard lock (m_perthread_info_mutex);
    for (size_t i = 0;  i < m_all_perthread_info.size();  ++i) {
        ImageCachePerThreadInfo *p = m_all_perthread_info[i];
        if (p) {
            // Clear the microcache.
            p->tile = NULL;
            p->lasttile = NULL;
            if (p->shared) {
                // Pointed to by both thread-specific-ptr and our list.
                // Just remove from out list, then ownership is only
                // by the thread-specific-ptr.
                p->shared = false;
            } else {
                // Only pointed to by us -- delete it!
                delete p;
            }
            m_all_perthread_info[i] = NULL;
        }
    }
}



void
ImageCacheImpl::cleanup_perthread_info (ImageCachePerThreadInfo *p)
{
    lock_guard lock (m_perthread_info_mutex);
    if (p) {
        // Clear the microcache.
        p->tile = NULL;
        p->lasttile = NULL;
        if (! p->shared)  // If we own it, delete it
            delete p;
        else
            p->shared = false;  // thread disappearing, no longer shared
    }
}



void
ImageCacheImpl::purge_perthread_microcaches ()
{
    // Mark the per-thread microcaches as invalid
    lock_guard lock (m_perthread_info_mutex);
    for (size_t i = 0, e = m_all_perthread_info.size();  i < e;  ++i)
        if (m_all_perthread_info[i])
            m_all_perthread_info[i]->purge = 1;
}



std::string
ImageCacheImpl::geterror () const
{
    std::string e;
    std::string *errptr = m_errormessage.get ();
    if (errptr) {
        e = *errptr;
        errptr->clear ();
    }
    return e;
}



void
ImageCacheImpl::append_error (const std::string& message) const
{
    std::string *errptr = m_errormessage.get ();
    if (! errptr) {
        errptr = new std::string;
        m_errormessage.reset (errptr);
    }
    ASSERT (errptr != NULL);
    ASSERT (errptr->size() < 1024*1024*16 &&
            "Accumulated error messages > 16MB. Try checking return codes!");
    if (errptr->size())
        *errptr += '\n';
    *errptr += message;
}



}  // end namespace pvt



ImageCache *
ImageCache::create (bool shared)
{
    if (shared) {
        // They requested a shared cache.  If a shared cache already
        // exists, just return it, otherwise record the new cache.
        spin_lock guard (shared_image_cache_mutex);
        if (! shared_image_cache.get())
            shared_image_cache.reset (new ImageCacheImpl);
        else
            shared_image_cache->invalidate_all ();

#if 0
        std::cerr << " shared ImageCache is "
                  << (void *)shared_image_cache.get() << "\n";
#endif
        return shared_image_cache.get ();
    }

    // Doesn't need a shared cache
    ImageCacheImpl *ic = new ImageCacheImpl;
#if 0
    std::cerr << "creating new ImageCache " << (void *)ic << "\n";
#endif
    return ic;
}



void
ImageCache::destroy (ImageCache *x, bool teardown)
{
    if (! x)
        return;
    spin_lock guard (shared_image_cache_mutex);
    if (x == shared_image_cache.get()) {
        // This is the shared cache, so don't really delete it. Invalidate
        // it fully, closing the files and throwing out any tiles that 
        // nobody is currently holding references to.  But only delete the
        // IC fully if 'teardown' is true, and even then, it won't destroy
        // until nobody else is still holding a shared_ptr to it.
        ((ImageCacheImpl *)x)->invalidate_all (teardown);
        if (teardown)
            shared_image_cache.reset ();
    } else {
        // Not a shared cache, we are the only owner, so truly destroy it.
        delete (ImageCacheImpl *) x;
    }
}



void
ImageCache::destroy (ImageCache *x)
{
    destroy (x, false);
}

}
OIIO_NAMESPACE_EXIT

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
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tr1/memory.hpp>
using namespace std::tr1;

#include <OpenEXR/ImathMatrix.h>

#include "dassert.h"
#include "typedesc.h"
#include "varyingref.h"
#include "ustring.h"
#include "filesystem.h"
#include "hash.h"
#include "thread.h"
#include "fmath.h"
#include "strutil.h"
#include "sysutil.h"
#include "timer.h"
#include "imageio.h"
#include "imagebuf.h"
#include "imagecache.h"
#include "texture.h"
#include "imagecache_pvt.h"

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



#ifdef OIIO_HAVE_BOOST_UNORDERED_MAP

/// Perform "map[key] = value", and set sweep_iter = end() if it is invalidated.
///
/// For some reason, unordered_map::insert and operator[] may invalidate
/// iterators (see the C++ Library Extensions document for C++0x at
/// http://www.open-std.org/jtc1/sc22/wg21/docs/projects).  This function
/// sets sweep_iter = end() if we detect it's become invalidated by the
/// insertion.
template<typename HashMapT>
void safe_insert (HashMapT& map, const typename HashMapT::key_type& key,
                  const typename HashMapT::mapped_type& value,
                  typename HashMapT::iterator& sweep_iter)
{
    size_t nbuckets_pre_insert = map.bucket_count();
    map[key] = value;
    // If the bucket count in the map has increased, it's probable that
    // sweep_iter was invalidated.  Just set it to the end, since the order of
    // elements has probably become essentially randomized anyway.
    if (nbuckets_pre_insert != map.bucket_count())
        sweep_iter = map.end ();
}

#else

template<typename HashMapT>
void safe_insert (HashMapT& map, const typename HashMapT::key_type& key,
                  const typename HashMapT::mapped_type& value,
                  typename HashMapT::iterator& /*sweep_iter*/)
{
    // Traditional implementations of hash_map don't typically invalidate
    // iterators on insertion.
    //   - VC++'s stdext::hash_map, according to msdn, and 
    //   - The implementation coming with g++ according to some vague
    //     indications in the SGI docs & other places on the web.
    map[key] = value;
}

#endif


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
    zero_origin = (spec.x == 0 && spec.y == 0 && spec.z == 0);
    onetile = (spec.width <= spec.tile_width &&
               spec.height <= spec.tile_height &&
               spec.depth <= spec.tile_depth);
    polecolorcomputed = false;
}



ImageCacheFile::ImageCacheFile (ImageCacheImpl &imagecache,
                                ImageCachePerThreadInfo *thread_info,
                                ustring filename)
    : m_filename(filename), m_used(true), m_broken(false),
      m_texformat(TexFormatTexture),
      m_swrap(TextureOpt::WrapBlack), m_twrap(TextureOpt::WrapBlack),
      m_rwrap(TextureOpt::WrapBlack),
      m_envlayout(LayoutTexture), m_y_up(false), m_sample_border(false),
      m_tilesread(0), m_bytesread(0), m_timesopened(0), m_iotime(0),
      m_mipused(false), m_validspec(false), 
      m_imagecache(imagecache), m_duplicate(NULL)
{
    m_filename = imagecache.resolve_filename (m_filename.string());
    // N.B. the file is not opened, the ImageInput is NULL.  This is
    // reflected by the fact that m_validspec is false.
}



ImageCacheFile::~ImageCacheFile ()
{
    close ();
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

    m_input.reset (ImageInput::create (m_filename.c_str(),
                                       m_imagecache.searchpath().c_str()));
    if (! m_input) {
        imagecache().error ("%s", OIIO_NAMESPACE::geterror().c_str());
        m_broken = true;
        invalidate_spec ();
        return false;
    }

    ImageSpec nativespec, tempspec;
    m_broken = false;
    bool ok = true;
    for (int tries = 0; tries <= imagecache().failure_retries(); ++tries) {
        ok = m_input->open (m_filename.c_str(), nativespec);
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
    do {
        m_subimages.resize (nsubimages+1);
        SubimageInfo &si (subimageinfo(nsubimages));
        si.volume = (nativespec.depth > 1 || nativespec.full_depth > 1);
        int nmip = 0;
        do {
            tempspec = nativespec;
            if (tempspec.tile_width == 0 || tempspec.tile_height == 0) {
                si.untiled = true;
                if (imagecache().autotile()) {
                    // Automatically make it appear as if it's tiled
                    tempspec.tile_width = imagecache().autotile();
                    tempspec.tile_height = imagecache().autotile();
                    if (tempspec.depth > 1)
                        tempspec.tile_depth = imagecache().autotile();
                    else
                        tempspec.tile_depth = 1;
                } else {
                    // Don't auto-tile -- which really means, make it look like
                    // a single tile that's as big as the whole image.
                    // We round to a power of 2 because the texture system
                    // currently requires power of 2 tile sizes.
                    tempspec.tile_width = pow2roundup (tempspec.width);
                    tempspec.tile_height = pow2roundup (tempspec.height);
                    tempspec.tile_depth = pow2roundup(tempspec.depth);
                }
            }
            thread_info->m_stats.files_totalsize += tempspec.image_bytes();
            // All MIP levels need the same number of channels
            if (nmip > 1 && tempspec.nchannels != spec(nsubimages,0).nchannels) {
                // No idea what to do with a subimage that doesn't have the
                // same number of channels as the others, so just skip it.
                close ();
                m_broken = true;
                invalidate_spec ();
                return false;
            }
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
                    s.tile_width = std::min (imagecache().autotile(), w);
                    s.tile_height = std::min (imagecache().autotile(), h);
                    s.tile_depth = std::min (imagecache().autotile(), d);
                } else {
                    s.tile_width = w;
                    s.tile_height = h;
                    s.tile_depth = d;
                }
                // Texture system requires pow2 tile sizes
                s.tile_width = pow2roundup (s.tile_width);
                s.tile_height = pow2roundup (s.tile_height);
                s.tile_depth = pow2roundup (s.tile_depth);
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
            if (! strcmp (textureformat, texture_format_name((TexFormat)i))) {
                m_texformat = (TexFormat) i;
                break;
            }
        // For textures marked as such, doctor the full_width/full_height to
        // not be non-sensical.
        if (m_texformat == TexFormatTexture) {
            for (int s = 0;  s < nsubimages;  ++s) {
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
        const char *wrapmodes = (const char *)p->data();
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
    std::string desc = spec.get_string_attribute ("ImageDescription");
    const char *prefix = "SHA-1=";
    size_t found = desc.rfind (prefix);
    if (found != std::string::npos)
        m_fingerprint = ustring (desc, found+strlen(prefix), 40);

    m_datatype = TypeDesc::FLOAT;
    if (! m_imagecache.forcefloat()) {
        // If we aren't forcing everything to be float internally, then 
        // there are a few other types we allow.
        if (spec.format == TypeDesc::UINT8)
            m_datatype = spec.format;
    }

    m_channelsize = m_datatype.size();
    m_pixelsize = m_channelsize * spec.nchannels;
    m_eightbit = (m_datatype == TypeDesc::UINT8);
    m_mod_time = boost::filesystem::last_write_time (m_filename.string());

    DASSERT (! m_broken);
    m_validspec = true;
    return true;
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
        // But wait, it's possible that somebody else is holding the
        // filemutex that will be needed by check_max_files_with_lock,
        // and they are waiting on our m_input_mutex, which we locked
        // above.  To avoid deadlock, we need to release m_input_mutex
        // while we close files.
        m_input_mutex.unlock ();
        imagecache().check_max_files_with_lock (thread_info);
        // Now we're back, whew!  Grab the lock again.
        m_input_mutex.lock ();
    }

    bool ok = open (thread_info);
    if (! ok)
        return false;

    // Mark if we ever use a mip level that's not the first
    if (miplevel > 0)
        m_mipused = true;

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
            imagecache().error ("%s", m_input->error_message().c_str());
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
    ImageBuf lores ("tmp", lospec);

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
                                           0, 1, TypeDesc::FLOAT, bilerppels);
            bilerp (bilerppels+0, bilerppels+spec.nchannels,
                    bilerppels+2*spec.nchannels, bilerppels+3*spec.nchannels,
                    xfrac, yfrac, spec.nchannels, resultpel);
            lores.setpixel (i-x0, j-y0, resultpel);
        }
    }

    // Now convert and copy those values out to the caller's buffer
    lores.copy_pixels (0, tw, 0, th, format, data);

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
        if (! m_input->seek_subimage (subimage, miplevel, tmp))
            return false;
    }

    // We should not hold the tile mutex at this point
    DASSERT (imagecache().tilemutex_holder() != thread_info &&
             "read_untiled expects NOT to hold the tile lock");
    
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
        int pixelsize = spec.nchannels * format.size();
        // Because of the way we copy below, we need to allocate the
        // buffer to be an even multiple of the tile width, so round up.
        stride_t scanlinesize = tw * ((spec.width+tw-1)/tw);
        scanlinesize *= pixelsize;
        std::vector<char> buf (scanlinesize * th); // a whole tile-row size
        int yy = y - spec.y;   // counting from top scanline
        // [y0,y1] is the range of scanlines to read for a tile-row
        int y0 = yy - (yy % th);
        int y1 = std::min (y0 + th - 1, spec.height - 1);
        y0 += spec.y;
        y1 += spec.y;
        // Read the whole tile-row worth of scanlines
        for (int scanline = y0, i = 0; scanline <= y1 && ok; ++scanline, ++i) {
            ok = m_input->read_scanline (scanline, z, format, (void *)&buf[scanlinesize*i]);
            if (! ok)
                imagecache().error ("%s", m_input->error_message().c_str());
        }
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
                if (! imagecache().tile_in_cache (id, thread_info,
                                                  true /*lock*/)) {
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
            imagecache().error ("%s", m_input->error_message().c_str());
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
#if 0
    // Old code
    // FIXME -- why do we need to reopen here?  Why reload the spec?
    // LG thinks this was  broken because we still had m_subimages intact
    // but still set m_validspec=true.  Weird, and led to subtle bugs.
    open (imagecache().get_perthread_info());  // Force reload of spec
    close ();
    if (m_broken)
        m_subimages.clear ();
    m_validspec = false;  // force it to read next time
    m_broken = false;
#endif
    // Eat any errors that occurred in the open/close
    while (! imagecache().geterror().empty())
        ;
}



ImageCacheFile *
ImageCacheImpl::find_file (ustring filename,
                           ImageCachePerThreadInfo *thread_info)
{
    ImageCacheStatistics &stats (thread_info->m_stats);
    ImageCacheFile *tf = NULL;
    bool newfile = false;

    // Part 1 - make sure the ImageCacheFile entry exists and is in the
    // file cache.  For this part, we need to lock the file cache.
    {
#if IMAGECACHE_TIME_STATS
        Timer timer;
#endif
        DASSERT (m_filemutex_holder != thread_info);
        ic_read_lock readguard (m_filemutex);
        DASSERT (m_filemutex_holder == NULL);
        filemutex_holder (thread_info);
#if IMAGECACHE_TIME_STATS
        double donelocking = timer();
        stats.file_locking_time += donelocking;
#endif
        FilenameMap::iterator found = m_files.find (filename);

#if IMAGECACHE_TIME_STATS
        stats.find_file_time += timer() - donelocking;
#endif

        if (found != m_files.end()) {
            tf = found->second.get();
        } else {
            // No such entry in the file cache.  Add it, but don't open yet.
            tf = new ImageCacheFile (*this, thread_info, filename);
            check_max_files (thread_info);
            safe_insert (m_files, filename, tf, m_file_sweep);
            newfile = true;
        }

        filemutex_holder (NULL);
#if IMAGECACHE_TIME_STATS
        stats.find_file_time += timer()-donelocking;
#endif
    }
    DASSERT (m_filemutex_holder != thread_info); // we better not hold

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
            stats.fileio_time += createtime;
            stats.fileopen_time += createtime;
            tf->iotime() += createtime;

            // What if we've opened another file, with a different name,
            // but the SAME pixels?  It can happen!  Bad user, bad!  But
            // let's save them from their own foolishness.
            bool was_duplicate = false;
            if (tf->fingerprint ()) {
                // std::cerr << filename << " hash=" << tf->fingerprint() << "\n";
                ImageCacheFile *dup = find_fingerprint (tf->fingerprint(), tf);
                if (dup != tf) {
                    // Already in fingerprints -- mark this one as a
                    // duplicate, but ONLY if we don't have other
                    // reasons not to consider them true duplicates (the
                    // fingerprint only considers source image pixel values.
                    // FIXME -- be sure to add extra tests
                    // here if more metadata have significance later!
                    if (tf->m_swrap == dup->m_swrap && tf->m_twrap == dup->m_twrap &&
                        tf->m_rwrap == dup->m_rwrap &&
                        tf->m_datatype == dup->m_datatype && 
                        tf->m_envlayout == dup->m_envlayout &&
                        tf->m_y_up == dup->m_y_up &&
                        tf->m_sample_border == dup->m_sample_border) {
                        tf->duplicate (dup);
                        tf->close ();
                        was_duplicate = true;
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
    if (tf->duplicate())
        tf = tf->duplicate();
    else {
        // not a duplicate -- if opening the first time, count as unique
        if (newfile)
            ++stats.unique_files;
    }

    tf->use ();  // Mark it as recently used
    return tf;
}



ImageCacheFile *
ImageCacheImpl::find_fingerprint (ustring finger, ImageCacheFile *file)
{
    spin_lock lock (m_fingerprints_mutex);
    FilenameMap::iterator found = m_fingerprints.find (finger);
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
    DASSERT (m_filemutex_holder == thread_info &&
             "check_max_files should only be called by file lock holder");
#if 0
    if (! (m_stat_open_files_created % 16) || m_stat_open_files_current >= m_max_open_files) {
        std::cerr << "open files " << m_stat_open_files_current << ", max = " << m_max_open_files << "\n";
    std::cout << "    ImageInputs : " << m_stat_open_files_created << " created, " << m_stat_open_files_current << " current, " << m_stat_open_files_peak << " peak\n";
    }
#endif
    int full_loops = 0;
    while (m_stat_open_files_current >= m_max_open_files) {
        if (m_file_sweep == m_files.end()) { // If at the end of list,
            m_file_sweep = m_files.begin();  //     loop back to beginning
            ++full_loops;
        }
        if (m_file_sweep == m_files.end())   // If STILL at the end,
            break;                           //     it must be empty, done
        ASSERT (full_loops < 100);  // abort rather than infinite loop
        DASSERT (m_file_sweep->second);
        m_file_sweep->second->release ();  // May reduce open files
        ++m_file_sweep;
    }
}



void
ImageCacheImpl::check_max_files_with_lock (ImageCachePerThreadInfo *thread_info)
{
#if IMAGECACHE_TIME_STATS
    Timer timer;
#endif
    DASSERT (m_filemutex_holder != thread_info);
    ic_read_lock readguard (m_filemutex);
    DASSERT (m_filemutex_holder == NULL);
    filemutex_holder (thread_info);
#if IMAGECACHE_TIME_STATS
    double donelocking = timer();
    ImageCacheStatistics &stats (thread_info->m_stats);
    stats.file_locking_time += donelocking;
#endif

    check_max_files (thread_info);

    filemutex_holder (NULL);
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
    if (read_now) {
        read (thread_info);
    }
    id.file().imagecache().incr_tiles (0);  // mem counted separately in read
}



ImageCacheTile::ImageCacheTile (const TileID &id, void *pels, TypeDesc format,
                    stride_t xstride, stride_t ystride, stride_t zstride)
    : m_id (id) // , m_used(true)
{
    m_used = true;
    ImageCacheFile &file (m_id.file ());
    const ImageSpec &spec (file.spec(id.subimage(), id.miplevel()));
    size_t size = memsize_needed ();
    ASSERT (size > 0 && memsize() == 0);
    m_pixels.resize (size);
    size_t dst_pelsize = spec.nchannels * file.datatype().size();
    m_valid = convert_image (spec.nchannels, spec.tile_width, spec.tile_height,
                             spec.tile_depth, pels, format, xstride, ystride,
                             zstride, &m_pixels[0], file.datatype(),
                             dst_pelsize, dst_pelsize * spec.tile_width,
                             dst_pelsize * spec.tile_width * spec.tile_height);
    id.file().imagecache().incr_tiles (size);
    m_pixels_ready = true;
    // FIXME -- for shadow, fill in mindepth, maxdepth
}



ImageCacheTile::~ImageCacheTile ()
{
    m_id.file().imagecache().decr_tiles (memsize ());
}



void
ImageCacheTile::read (ImageCachePerThreadInfo *thread_info)
{
    DASSERT (m_id.file().imagecache().tilemutex_holder() != thread_info &&
             "ImageCacheTile::read expects to NOT hold the tile lock");
    size_t size = memsize_needed ();
    ASSERT (memsize() == 0 && size > 0);
    m_pixels.resize (size);
    ImageCacheFile &file (m_id.file());
    m_valid = file.read_tile (thread_info, m_id.subimage(), m_id.miplevel(),
                              m_id.x(), m_id.y(), m_id.z(),
                              file.datatype(), &m_pixels[0]);
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
    while (! m_pixels_ready) {
        // Be kind to the CPU.  As far as I know, this does nothing on
        // Windows.  Any Windows gurus know a better idea?
#ifdef __TBB_Yield
        __TBB_Yield ();
#endif
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
    size_t pixelsize = spec.nchannels * m_id.file().datatype().size();
    size_t offset = ((z * h + y) * w + x) * pixelsize;
    return (const void *)&m_pixels[offset];
}



ImageCacheImpl::ImageCacheImpl ()
    : m_perthread_info (&cleanup_perthread_info),
      m_file_sweep(m_files.end()),
      m_tile_sweep(m_tilecache.end())
{
    init ();
}



void
ImageCacheImpl::init ()
{
    m_max_open_files = 100;
    m_max_memory_bytes = 256 * 1024 * 1024;   // 256 MB default cache size
    m_autotile = 0;
    m_automip = false;
    m_forcefloat = false;
    m_accept_untiled = true;
    m_accept_unmipped = true;
    m_read_before_insert = false;
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
    m_tilemutex_holder = NULL;
    m_filemutex_holder = NULL;
}



ImageCacheImpl::~ImageCacheImpl ()
{
    printstats ();
    erase_perthread_info ();
    DASSERT (m_tilemutex_holder == NULL);
    DASSERT (m_filemutex_holder == NULL);
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
        out << "OpenImageIO ImageCache statistics (" << (void*)this 
            << ") ver " << OIIO_VERSION_STRING << "\n";
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
        ic_read_lock fileguard (m_filemutex);
        for (FilenameMap::const_iterator f = m_files.begin(); f != m_files.end(); ++f) {
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
                if (files[i]->broken())
                    continue;
                out << Strutil::format ("    %d   %6.1f MB (%4.1f%%)  ", i+1,
                                        files[i]->bytesread()/1024.0/1024.0,
                                        100.0 * (files[i]->bytesread() / (double)total_bytes));
                out << onefile_stat_line (files[i], -1, false) << "\n";
            }
            std::sort (files.begin(), files.end(), iotime_compare);
            out << "  Top files by I/O time:\n";
            for (int i = 0;  i < std::min<int> (topN, files.size());  ++i) {
                if (files[i]->broken())
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
                if (file->broken())
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
        ic_read_lock fileguard (m_filemutex);
        for (FilenameMap::const_iterator f = m_files.begin(); f != m_files.end(); ++f) {
            const ImageCacheFileRef &file (f->second);
            file->m_timesopened = 0;
            file->m_tilesread = 0;
            file->m_bytesread = 0;
            file->m_iotime = 0;
        }
    }
}



bool
ImageCacheImpl::attribute (const std::string &name, TypeDesc type,
                           const void *val)
{
    bool do_invalidate = false;
    bool force_invalidate = false;
    if (name == "max_open_files" && type == TypeDesc::INT) {
        m_max_open_files = *(const int *)val;
    }
    else if (name == "max_memory_MB" && type == TypeDesc::FLOAT) {
        float size = *(const float *)val;
#ifndef DEBUG
        size = std::max (size, 10.0f);  // Don't let users choose < 10 MB
#else
        size = std::max (size, 1.0f);   // But let developers debugging do it
#endif
        m_max_memory_bytes = size_t(size * 1024 * 1024);
    }
    else if (name == "max_memory_MB" && type == TypeDesc::INT) {
        float size = *(const int *)val;
#ifndef DEBUG
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
    else if (name == "statistics:level" && type == TypeDesc::INT) {
        m_statslevel = *(const int *)val;
    }
    else if (name == "autotile" && type == TypeDesc::INT) {
        int a = pow2roundup (*(const int *)val);  // guarantee pow2
        // Clamp to minimum 8x8 tiles to protect against stupid user who
        // think this is a boolean rather than the tile size.  Unless
        // we're in DEBUG mode, then allow developers to play with fire.
#ifndef DEBUG
        if (a > 0 && a < 8)
            a = 8;
#endif
        if (a != m_autotile) {
            m_autotile = a;
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
    else if (name == "failure_retries" && type == TypeDesc::INT) {
        m_failure_retries = *(const int *)val;
    }
    else if (name == "latlong_up" && type == TypeDesc::STRING) {
        bool y_up = ! strcmp ("y", *(const char **)val);
        if (y_up != m_latlong_y_up_default) {
            m_latlong_y_up_default = y_up;
            do_invalidate = true;
        }
    } else {
        // Otherwise, unknown name
        return false;
    }

    if (do_invalidate)
        invalidate_all (force_invalidate);
    return true;
}



bool
ImageCacheImpl::getattribute (const std::string &name, TypeDesc type,
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
    ATTR_DECODE ("automip", int, m_automip);
    ATTR_DECODE ("forcefloat", int, m_forcefloat);
    ATTR_DECODE ("accept_untiled", int, m_accept_untiled);
    ATTR_DECODE ("accept_unmipped", int, m_accept_unmipped);
    ATTR_DECODE ("read_before_insert", int, m_read_before_insert);
    ATTR_DECODE ("failure_retries", int, m_failure_retries);

    // The cases that don't fit in the simple ATTR_DECODE scheme
    if (name == "searchpath" && type == TypeDesc::STRING) {
        *(ustring *)val = m_searchpath;
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
        Timer timer;
#endif
        DASSERT (m_tilemutex_holder != thread_info); // shouldn't hold
        ic_read_lock readguard (m_tilemutex);
        tilemutex_holder (thread_info);
#if IMAGECACHE_TIME_STATS
        stats.tile_locking_time += timer();
#endif

        TileCache::iterator found = m_tilecache.find (id);
#if IMAGECACHE_TIME_STATS
        stats.find_tile_time += timer();
#endif
        if (found != m_tilecache.end()) {
            tile = found->second;
            // We need to release the tile lock BEFORE calling
            // wait_pixels_ready, or we could end up deadlocked if the
            // other thread reading the pixels needs to lock the cache
            // because it's doing automip.
            DASSERT (m_tilemutex_holder == thread_info); // better still be us
            tilemutex_holder (NULL);
            m_tilemutex.unlock ();
            tile->wait_pixels_ready ();
            tile->use ();
            DASSERT (id == tile->id());
            DASSERT (tile);
            DASSERT (m_tilemutex_holder != thread_info); // shouldn't hold
            // Relock -- this shouldn't be necessary, but by golly, if I
            // don't do this, I get inconsistent lock states.  Maybe a TBB
            // bug in the lock_guard destructor if the spin mutex was 
            // unlocked by hand, as above?
            // FIXME -- try removing this if/when we upgrade to a newer TBB.
            m_tilemutex.lock ();
            return true;
        }
        DASSERT (tilemutex_holder() == thread_info); // better still be us
        tilemutex_holder (NULL);
    }

    DASSERT (m_tilemutex_holder != thread_info); // shouldn't hold

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
    DASSERT (m_tilemutex_holder != thread_info); // shouldn't hold
    return tile->valid();
}



void
ImageCacheImpl::add_tile_to_cache (ImageCacheTileRef &tile,
                                   ImageCachePerThreadInfo *thread_info)
{
    bool ourtile = true;
    {
#if IMAGECACHE_TIME_STATS
        Timer timer;
#endif
        DASSERT (m_tilemutex_holder != thread_info); // shouldn't hold
        ic_write_lock writeguard (m_tilemutex);
        tilemutex_holder (thread_info);
#if IMAGECACHE_TIME_STATS
        thread_info->m_stats.tile_locking_time += timer();
#endif
        // Protect us from using too much memory if another thread added the
        // same tile just before us
        TileCache::iterator found = m_tilecache.find (tile->id());
        if (found != m_tilecache.end ()) {
            // Already added!  Use the other one, discard ours.
            tile = m_tilecache[tile->id()];
            ourtile = false;  // Don't need to add it
        } else {
            // Still not in cache, add ours to the cache
            check_max_mem (thread_info);
            safe_insert (m_tilecache, tile->id(), tile, m_tile_sweep);
        }
        DASSERT (tilemutex_holder() == thread_info); // better still be us
        tilemutex_holder (NULL);
    }
    DASSERT (m_tilemutex_holder != thread_info); // shouldn't hold

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
    DASSERT (m_tilemutex_holder != thread_info); // shouldn't hold
}



void
ImageCacheImpl::check_max_mem (ImageCachePerThreadInfo *thread_info)
{
    DASSERT (m_tilemutex_holder == thread_info &&
             "check_max_mem should only be called by tile lock holder");
    DASSERT (m_mem_used < (long long)m_max_memory_bytes*10); // sanity
#if 0
    static atomic_int n;
    if (! (n++ % 64) || m_mem_used >= (long long)m_max_memory_bytes)
        std::cerr << "mem used: " << m_mem_used << ", max = " << m_max_memory_bytes << "\n";
#endif
    if (m_tilecache.empty())
        return;
    if (m_mem_used < (long long)m_max_memory_bytes)
        return;
    int full_loops = 0;
    while (m_mem_used >= (long long)m_max_memory_bytes) {
        if (m_tile_sweep == m_tilecache.end()) {// If at the end of list,
            m_tile_sweep = m_tilecache.begin(); //     loop back to beginning
            ++full_loops;
        }
        if (m_tile_sweep == m_tilecache.end())  // If STILL at the end,
            break;                              //      it must be empty, done
        ASSERT (full_loops < 100);  // abort rather than infinite loop
        if (! m_tile_sweep->second->release ()) {
            TileCache::iterator todelete = m_tile_sweep;
            ++m_tile_sweep;
            size_t size = todelete->second->memsize();
            ASSERT (m_mem_used >= (long long)size);
#if 0
            std::cerr << "  Freeing tile, recovering " << size << "\n";
#endif
            m_tilecache.erase (todelete);
        } else {
            ++m_tile_sweep;
        }
    }
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
    ImageCacheFile *file = find_file (filename, thread_info);
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
        *(int *)data = (int) file->m_datatype.basetype;
        return true;
    }
    if (dataname == s_miplevels && datatype == TypeDesc::TypeInt) {
        *(int *)data = file->miplevels(subimage);
        return true;
    }
    // FIXME - "viewingmatrix"
    // FIXME - "projectionmatrix"

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
    ImageCacheFile *file = find_file (filename, thread_info);
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



bool
ImageCacheImpl::get_pixels (ustring filename, int subimage, int miplevel,
                            int xbegin, int xend, int ybegin, int yend,
                            int zbegin, int zend,
                            TypeDesc format, void *result)
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

    return get_pixels (file, thread_info, subimage, miplevel, xbegin, xend, 
                       ybegin, yend, zbegin, zend, format, result);
}



bool
ImageCacheImpl::get_pixels (ImageCacheFile *file, 
                            ImageCachePerThreadInfo *thread_info,
                            int subimage, int miplevel,
                            int xbegin, int xend, int ybegin, int yend,
                            int zbegin, int zend, 
                            TypeDesc format, void *result)
{
    const ImageSpec &spec (file->spec(subimage, miplevel));
    bool ok = true;

    // FIXME -- this could be WAY more efficient than starting from
    // scratch for each pixel within the rectangle.  Instead, we should
    // grab a whole tile at a time and memcpy it rapidly.  But no point
    // doing anything more complicated (not to mention bug-prone) until
    // somebody reports this routine as being a bottleneck.
    int nc = spec.nchannels;
    size_t formatpixelsize = nc * format.size();
    size_t scanlinesize = (xend-xbegin) * formatpixelsize;
    size_t zplanesize = (yend-ybegin) * scanlinesize;
    DASSERT (spec.depth >= 1 && spec.tile_depth >= 1);
    for (int z = zbegin;  z < zend;  ++z) {
        if (z < spec.z || z >= (spec.z+spec.depth)) {
            // nonexistant planes
            memset (result, 0, zplanesize);
            result = (void *) ((char *) result + zplanesize);
            continue;
        }
        int tz = z - ((z - spec.z) % spec.tile_depth);
        for (int y = ybegin;  y < yend;  ++y) {
            if (y < spec.y || y >= (spec.y+spec.height)) {
                // nonexistant scanlines
                memset (result, 0, scanlinesize);
                result = (void *) ((char *) result + scanlinesize);
                continue;
            }
            int ty = y - ((y - spec.y) % spec.tile_height);
            for (int x = xbegin;  x < xend;  ++x) {
                if (x < spec.x || x >= (spec.x+spec.width)) {
                    // nonexistant columns
                    memset (result, 0, formatpixelsize);
                    result = (void *) ((char *) result + formatpixelsize);
                    continue;
                }
                int tx = x - ((x - spec.x) % spec.tile_width);
                TileID tileid (*file, subimage, miplevel, tx, ty, tz);
                ok &= find_tile (tileid, thread_info);
                if (! ok)
                    return false;  // Just stop if file read failed
                ImageCacheTileRef &tile (thread_info->tile);
                const char *data;
                if (tile && (data = (const char *)tile->data (x, y, z))) {
                    convert_types (file->datatype(), data, format, result, nc);
                } else {
                    memset (result, 0, formatpixelsize);
                }
                result = (void *) ((char *) result + formatpixelsize);
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
    ImageCacheTileRef tile;
    if (find_tile_main_cache (id, tile, thread_info)) {
        tile->_incref();   // Fake an extra reference count
        tile->use ();
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
    format = t->file().datatype();
    return t->data ();
}



void
ImageCacheImpl::invalidate (ustring filename)
{
    ImageCacheFile *file = NULL;
    {
        ic_read_lock fileguard (m_filemutex);
        FilenameMap::iterator fileit = m_files.find (filename);
        if (fileit != m_files.end()) {
            file = fileit->second.get();
            filemutex_holder (NULL);
        } else {
            filemutex_holder (NULL);
            return;  // no such file
        }
    }

    {
        ic_write_lock tileguard (m_tilemutex);
#ifdef DEBUG
        tilemutex_holder (get_perthread_info ());
#endif
        for (TileCache::iterator tci = m_tilecache.begin();  tci != m_tilecache.end();  ) {
            TileCache::iterator todelete (tci);
            ++tci;
            if (&todelete->second->file() == file) {
                m_tilecache.erase (todelete);
                // If the tile we deleted is the current clock sweep
                // position, that would leave it pointing to an invalid
                // tile entry, ick!  In this case, just advance it.
                if (todelete == m_tile_sweep)
                    m_tile_sweep = tci;
            }
        }
        tilemutex_holder (NULL);
    }

    {
        ic_write_lock fileguard (m_filemutex);
        file->invalidate ();
    }

    // Mark the per-thread microcaches as invalid
    lock_guard lock (m_perthread_info_mutex);
    for (size_t i = 0;  i < m_all_perthread_info.size();  ++i)
        if (m_all_perthread_info[i])
            m_all_perthread_info[i]->purge = 1;
}



void
ImageCacheImpl::invalidate_all (bool force)
{
    // Make a list of all files that need to be invalidated
    std::vector<ustring> all_files;
    {
        ic_read_lock fileguard (m_filemutex);
        for (FilenameMap::iterator fileit = m_files.begin();
                 fileit != m_files.end();  ++fileit) {
            ImageCacheFileRef &f (fileit->second);
            ustring name = f->filename();
            recursive_lock_guard guard (f->m_input_mutex);
            if (f->broken() || ! Filesystem::exists(name.string())) {
                all_files.push_back (name);
                continue;
            }
            std::time_t t = boost::filesystem::last_write_time (name.string());
            // Invalidate the file if it has been modified since it was
            // last opened, or if 'force' is true.
            bool inval = force || (t != f->mod_time());
            for (int s = 0;  !inval && s < f->subimages();  ++s) {
                ImageCacheFile::SubimageInfo &sub (f->subimageinfo(s));
                // Invalidate if any unmipped subimage:
                // ... didn't automip, but automip is now on
                // ... did automip, but automip is now off
                if (sub.unmipped &&
                      ((m_automip && f->miplevels(s) <= 1) ||
                       (!m_automip && f->miplevels(s) > 1)))
                    inval = true;
            }
            if (inval)
                all_files.push_back (name);
        }
    }

    BOOST_FOREACH (ustring f, all_files) {
        // fprintf (stderr, "Invalidating %s\n", f.c_str());
        invalidate (f);
    }

    clear_fingerprints ();

    // Mark the per-thread microcaches as invalid
    lock_guard lock (m_perthread_info_mutex);
    for (size_t i = 0;  i < m_all_perthread_info.size();  ++i)
        if (m_all_perthread_info[i])
            m_all_perthread_info[i]->purge = 1;
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
ImageCacheImpl::error (const char *message, ...)
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
    va_list ap;
    va_start (ap, message);
    *errptr += Strutil::vformat (message, ap);
    va_end (ap);
}



};  // end namespace pvt



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
ImageCache::destroy (ImageCache *x)
{
    // If this is not a shared cache, delete it for real.  But if it is
    // the same as the shared cache, don't really delete it, since others
    // may be using it now, or may request a shared cache some time in
    // the future.  Don't worry that it will leak; because shared_image_cache
    // is itself a shared_ptr, when the process ends it will properly
    // destroy the shared cache.
    spin_lock guard (shared_image_cache_mutex);
    if (x == shared_image_cache.get()) {
        // Don't destroy the shared cache, but do invalidate and close the files.
        ((ImageCacheImpl *)x)->invalidate_all ();
    } else {
        // Not a shared cache, we are the only owner, so truly destroy it.
        delete (ImageCacheImpl *) x;
    }
}

}
OIIO_NAMESPACE_EXIT

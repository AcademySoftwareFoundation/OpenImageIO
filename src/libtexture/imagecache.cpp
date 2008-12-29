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
#include "strutil.h"
#include "sysutil.h"
#include "timer.h"
#include "imageio.h"
using namespace OpenImageIO;

#define DLL_EXPORT_PUBLIC /* Because we are implementing ImageCache */
#include "imagecache.h"
#undef DLL_EXPORT_PUBLIC

#include "texture.h"
#include "imagecache_pvt.h"
using namespace OpenImageIO::pvt;


namespace OpenImageIO {


namespace pvt {   // namespace OpenImageIO::pvt



ImageCacheFile::ImageCacheFile (ImageCacheImpl &imagecache, ustring filename)
    : m_filename(filename), m_used(true), m_broken(false),
      m_untiled(false), m_unmipped(false),
      m_texformat(TexFormatTexture), 
      m_swrap(TextureOptions::WrapBlack), m_twrap(TextureOptions::WrapBlack),
      m_cubelayout(CubeUnknown), m_y_up(false),
      m_imagecache(imagecache)
{
    m_spec.clear ();
    open ();
#if 0
    static int x=0;
    if ((++x % 16) == 0) {
    std::cerr << "Opened " << filename ;
    std::cerr << ", now mem is " 
              << Strutil::memformat (Sysutil::memory_used()) 
              << " virtual, resident = " 
              << Strutil::memformat (Sysutil::memory_used(true)) 
              << "\n";
    }
#endif
}



ImageCacheFile::~ImageCacheFile ()
{
    close ();
}



bool
ImageCacheFile::open ()
{
    if (m_input)         // Already opened
        return !m_broken;
    if (m_broken)        // Already failed an open -- it's broken
        return false;

    m_input.reset (ImageInput::create (m_filename.c_str(),
                                       m_imagecache.searchpath().c_str()));
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
    m_imagecache.incr_open_files ();
    use ();

    // If m_spec has already been filled out, we've opened this file
    // before, read the spec, and filled in all the fields.  So now that
    // we've re-opened it, we're done.
    if (m_spec.size())
        return true;

    // From here on, we know that we've opened this file for the very
    // first time.  So read all the MIP levels, fill out all the fields
    // of the ImageCacheFile.
    ++imagecache().m_stat_unique_files;
    m_spec.reserve (16);
    int nsubimages = 0;
    do {
        if (nsubimages > 1 && tempspec.nchannels != m_spec[0].nchannels) {
            // No idea what to do with a subimage that doesn't have the
            // same number of channels as the others, so just skip it.
            close ();
            m_broken = true;
            return false;
        }
        if (tempspec.tile_width == 0 || tempspec.tile_height == 0) {
            m_untiled = true;
            if (imagecache().autotile()) {
                // Automatically make it appear as if it's tiled
                tempspec.tile_width = imagecache().autotile();
                tempspec.tile_height = imagecache().autotile();
            } else {
                // Don't auto-tile -- which really means, make it look like
                // a single tile that's as big as the whole image
                tempspec.tile_width = pow2roundup (tempspec.width);
                tempspec.tile_height = pow2roundup (tempspec.height);
            }
        }
        ++nsubimages;
        m_spec.push_back (tempspec);
        imagecache().m_stat_files_totalsize += (long long)tempspec.image_bytes();
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
    m_imagecache.get_commontoworld (c2w);
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
#if 1
    if (spec.format == TypeDesc::UINT8)
        m_datatype = TypeDesc::UINT8;
#endif

    m_channelsize = m_datatype.size();
    m_pixelsize = m_channelsize * spec.nchannels;
    m_eightbit = (m_datatype == TypeDesc::UINT8);

    if (m_untiled || m_unmipped) {
        close ();
    }

    return !m_broken;
}



bool
ImageCacheFile::read_tile (int level, int x, int y, int z,
                           TypeDesc format, void *data)
{
    bool ok = open ();
    if (! ok)
        return false;

    ImageSpec tmp;
    if (m_input->current_subimage() != level)
        m_input->seek_subimage (level, tmp);

    // Special case for untiled, unmip-mapped
    if (m_untiled | m_unmipped)
        return read_untiled (level, x, y, z, format, data);

    // Ordinary tiled
    imagecache().m_stat_bytes_read += spec(level).tile_bytes();
    return m_input->read_tile (x, y, z, format, data);
}



bool
ImageCacheFile::read_untiled (int level, int x, int y, int z,
                              TypeDesc format, void *data)
{
    stride_t xstride=AutoStride, ystride=AutoStride, zstride=AutoStride;
    spec().auto_stride (xstride, ystride, zstride, format, spec().nchannels,
                        spec().tile_width, spec().tile_height);

    bool ok = true;
    if (imagecache().autotile()) {
        // Auto-tile is on, with a tile size that isn't the whole image.
        // Read ine a tile-sized region from individual scanlines.
        // FIXME -- I don't think this works properly for 3D images
        int tile = imagecache().autotile();
        int pixelsize = spec().nchannels * format.size();
        int tilelinesize = pixelsize * spec().tile_width;
        std::vector<char> buf (spec().width * pixelsize);
        int yy = y - spec().y;   // counting from top scanline
        int y0 = yy - (yy % spec().tile_height);
        int y1 = std::min (y0 + spec().tile_height - 1, spec().height - 1);
        y0 += spec().y;
        y1 += spec().y;
        int xx = x - spec().x;   // counting from left row
        int x0 = xx - (xx % spec().tile_width);
        int xoffset = x0 * pixelsize;
        for (int scanline = y0, i = 0; scanline <= y1 && ok; ++scanline, ++i) {
            ok = m_input->read_scanline (scanline, z, format, (void *)&buf[0]);
            memcpy ((char *)data + tilelinesize * i,
                    (char *)&buf[xoffset], tilelinesize);
        }
        // FIXME -- It's hugely wasteful to read 64 scanlines for one
        // tile.  We should opportunistically populate the cache with
        // the whole line of tiles corresponding to this swath of
        // scanlines, to amortize this cost.
    } else {
        // No auto-tile -- the tile is the whole image
        ok = m_input->read_image (format, data, xstride, ystride, zstride);
        imagecache().m_stat_bytes_read += spec().image_bytes();
    }

    close ();   // Done with it
    return ok;
}



void
ImageCacheFile::close ()
{
    if (opened()) {
        m_input->close ();
        m_input.reset ();
        m_imagecache.decr_open_files ();
    }
}



void
ImageCacheFile::release ()
{
    if (m_used)
        m_used = false;
    else
        close ();
}



ImageCacheFile *
ImageCacheImpl::find_file (ustring filename)
{
    Timer locktime;
    lock_guard_t guard (m_mutex);
    m_stat_locking_time += locktime();

    FilenameMap::iterator found = m_files.find (filename);
    ImageCacheFile *tf;
    if (found == m_files.end()) {
        // We don't already have this file in the table.  Try to
        // open it and create a record.
        Timer time;
        tf = new ImageCacheFile (*this, filename);
        m_stat_fileio_time += time();
        m_files[filename] = tf;
    } else {
        tf = found->second.get();
    }
    tf->use ();
    check_max_files ();
    return tf;
}



void
ImageCacheImpl::check_max_files ()
{
#ifdef DEBUG
    if (! (m_stat_open_files_created % 16) || m_stat_open_files_current >= m_max_open_files) {
        std::cerr << "open files " << m_stat_open_files_current << ", max = " << m_max_open_files << "\n";
    std::cout << "    ImageInputs : " << m_stat_open_files_created << " created, " << m_stat_open_files_current << " current, " << m_stat_open_files_peak << " peak\n";
    }
#endif
    while (m_stat_open_files_current >= m_max_open_files) {
        if (m_file_sweep == m_files.end())
            m_file_sweep = m_files.begin();
        ASSERT (m_file_sweep != m_files.end());
        m_file_sweep->second->release ();  // May reduce open files
        ++m_file_sweep;
    }
}



ImageCacheTile::ImageCacheTile (const TileID &id)
    : m_id (id), m_used(true)
{
    ImageCacheFile &file (m_id.file ());
    ImageCacheImpl &imagecache (file.imagecache());
    ++imagecache.m_stat_tiles_created;
    ++imagecache.m_stat_tiles_current;
    if (imagecache.m_stat_tiles_current > imagecache.m_stat_tiles_peak)
        imagecache.m_stat_tiles_peak = imagecache.m_stat_tiles_current;
    size_t size = memsize();
    m_pixels.resize (size);
    imagecache.m_mem_used += size;
    m_valid = file.read_tile (m_id.level(), m_id.x(), m_id.y(), m_id.z(),
                              file.datatype(), &m_pixels[0]);
    if (! m_valid)
        std::cerr << "(1) error reading tile " << m_id.x() << ' ' << m_id.y() 
                  << " from " << file.filename() << "\n";

    // FIXME -- for shadow, fill in mindepth, maxdepth
}



ImageCacheTile::~ImageCacheTile ()
{
    DASSERT (memsize() == m_pixels.size());
    m_id.file().imagecache().m_mem_used -= (int) memsize ();
    --(m_id.file().imagecache().m_stat_tiles_current);
}



const void *
ImageCacheTile::data (int x, int y, int z) const
{
    const ImageSpec &spec = m_id.file().spec (m_id.level());
    size_t w = spec.tile_width;
    size_t h = spec.tile_height;
    size_t d = std::max (1, spec.tile_depth);
    x -= m_id.x();
    y -= m_id.y();
    z -= m_id.z();
    if (x < 0 || x >= w || y < 0 || y >= h || z < 0 || z >= d)
        return NULL;
    size_t pixelsize = spec.nchannels * m_id.file().datatype().size();
    size_t offset = ((z * h + y) * w + x) * pixelsize;
    return (const void *)&m_pixels[offset];
}



ImageCacheImpl::ImageCacheImpl ()
    : m_file_sweep(m_files.end()), 
      m_tile_sweep(m_tilecache.end()), m_mem_used(0)
{
    init ();
}



void
ImageCacheImpl::init ()
{
    m_max_open_files = 100;
    m_max_memory_MB = 50;
    m_max_memory_bytes = (int) (m_max_memory_MB * 1024 * 1024);
    m_autotile = 0;
    m_Mw2c.makeIdentity();
    m_mem_used = 0;
    m_statslevel = 0;
    m_stat_find_tile_calls = 0;
    m_stat_find_tile_microcache_misses = 0;
    m_stat_find_tile_cache_misses = 0;
    m_stat_tiles_created = 0;
    m_stat_tiles_current = 0;
    m_stat_tiles_peak = 0;
    m_stat_files_totalsize = 0;
    m_stat_bytes_read = 0;
    m_stat_open_files_created = 0;
    m_stat_open_files_current = 0;
    m_stat_open_files_peak = 0;
    m_stat_unique_files = 0;
    m_stat_fileio_time = 0;
    m_stat_locking_time = 0;
}



ImageCacheImpl::~ImageCacheImpl ()
{
    printstats ();
}



std::string
ImageCacheImpl::getstats (int level) const
{
    std::ostringstream out;
    if (level > 0) {
        out << "OpenImageIO ImageCache statistics (" << (void*)this << ")\n";
        out << "  Images : " << m_stat_unique_files << " unique\n";
        out << "    ImageInputs : " << m_stat_open_files_created << " created, " << m_stat_open_files_current << " current, " << m_stat_open_files_peak << " peak\n";
        out << "    Total size of all images referenced : " << Strutil::memformat (m_stat_files_totalsize) << "\n";
        out << "    Read from disk : " << Strutil::memformat (m_stat_bytes_read) << "\n";
        out << "    Total file I/O time : " << Strutil::timeintervalformat (m_stat_fileio_time) << "\n";
        out << "    Locking time : " << Strutil::timeintervalformat (m_stat_locking_time) << "\n";
        out << "  Tiles: " << m_stat_tiles_created << " created, " << m_stat_tiles_current << " current, " << m_stat_tiles_peak << " peak\n";
        out << "    total tile requests : " << m_stat_find_tile_calls << "\n";
        out << "    micro-cache misses : " << m_stat_find_tile_microcache_misses << " (" << 100.0*(double)m_stat_find_tile_microcache_misses/(double)m_stat_find_tile_calls << "%)\n";
        out << "    main cache misses : " << m_stat_find_tile_cache_misses << " (" << 100.0*(double)m_stat_find_tile_cache_misses/(double)m_stat_find_tile_calls << "%)\n";
        out << "    Peak cache memory : " << Strutil::memformat (m_mem_used) << "\n";
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



bool
ImageCacheImpl::attribute (const std::string &name, TypeDesc type,
                           const void *val)
{
    if (name == "max_open_files" && type == TypeDesc::INT) {
        m_max_open_files = *(const int *)val;
        return true;
    }
    if (name == "max_memory_MB" && type == TypeDesc::FLOAT) {
        float size = *(const float *)val;
        m_max_memory_MB = size;
        m_max_memory_bytes = (int)(size * 1024 * 1024);
        return true;
    }
    if (name == "max_memory_MB" && type == TypeDesc::INT) {
        float size = *(const int *)val;
        m_max_memory_MB = size;
        m_max_memory_bytes = (int)(size * 1024 * 1024);
        return true;
    }
    if (name == "searchpath" && type == TypeDesc::STRING) {
        m_searchpath = ustring (*(const char **)val);
        return true;
    }
    if (name == "statistics:level" && type == TypeDesc::INT) {
        m_statslevel = *(const int *)val;
        return true;
    }
    if (name == "autotile" && type == TypeDesc::INT) {
        m_autotile = pow2roundup (*(const int *)val);  // guarantee pow2
        return true;
    }
    return false;
}



bool
ImageCacheImpl::getattribute (const std::string &name, TypeDesc type,
                              void *val)
{
    if (name == "max_open_files" && type == TypeDesc::INT) {
        *(int *)val = m_max_open_files;
        return true;
    }
    if (name == "max_memory_MB" && type == TypeDesc::FLOAT) {
        *(float *)val = m_max_memory_MB;
        return true;
    }
    if (name == "searchpath" && type == TypeDesc::STRING) {
        *(ustring *)val = m_searchpath;
        return true;
    }
    if (name == "statistics:level" && type == TypeDesc::INT) {
        *(int *)val = m_statslevel;
        return true;
    }
    if (name == "autotile" && type == TypeDesc::INT) {
        *(int *)val = m_autotile;
        return true;
    }
    if (name == "worldtocommon" && (type == TypeDesc::PT_MATRIX ||
                                    type == TypeDesc(TypeDesc::PT_FLOAT,16))) {
        *(Imath::M44f *)val = m_Mw2c;
        return true;
    }
    if (name == "commontoworld" && (type == TypeDesc::PT_MATRIX ||
                                    type == TypeDesc(TypeDesc::PT_FLOAT,16))) {
        *(Imath::M44f *)val = m_Mc2w;
        return true;
    }
    return false;
}



void
ImageCacheImpl::find_tile (const TileID &id, ImageCacheTileRef &tile)
{
    DASSERT (! id.file().broken());
    Timer locktime;
    lock_guard_t guard (m_mutex);
    m_stat_locking_time += locktime();
    ++m_stat_find_tile_microcache_misses;
    TileCache::iterator found = m_tilecache.find (id);
    if (found != m_tilecache.end()) {
        tile = found->second;
        tile->used ();
    } else {
        ++m_stat_find_tile_cache_misses;
        check_max_mem ();
        Timer time;
        tile.reset (new ImageCacheTile (id));
        m_stat_fileio_time += time();
        // FIXME -- should we create the ICT above while not locked?
        m_tilecache[id] = tile;
    }
    DASSERT (id == tile->id() && !memcmp(&id, &tile->id(), sizeof(TileID)));
    DASSERT (tile);
}



void
ImageCacheImpl::check_max_mem ()
{
#ifdef DEBUG
    static size_t n = 0;
    if (! (n++ % 16) || m_mem_used >= m_max_memory_bytes)
        std::cerr << "mem used: " << m_mem_used << ", max = " << m_max_memory_bytes << "\n";
#endif
    if (m_tilecache.empty())
        return;
    while (m_mem_used >= m_max_memory_bytes) {
        if (m_tile_sweep == m_tilecache.end())
            m_tile_sweep = m_tilecache.begin();
        ASSERT (m_tile_sweep != m_tilecache.end());
        if (! m_tile_sweep->second->used (false)) {
            TileCache::iterator todelete = m_tile_sweep;
            ++m_tile_sweep;
            size_t size = todelete->second->memsize();
            ASSERT (m_mem_used >= size);
#ifdef DEBUG
            std::cerr << "  Freeing tile, recovering " << size << "\n";
#endif
            m_tilecache.erase (todelete);
        } else {
            ++m_tile_sweep;
        }
    }
}



bool
ImageCacheImpl::get_image_info (ustring filename, ustring dataname,
                                TypeDesc datatype, void *data)
{
    ImageCacheFile *file = find_file (filename);
    if (! file) {
        error ("Image file \"%s\" not found", filename.c_str());
        return false;
    }
    if (file->broken()) {
        error ("Invalid image file \"%s\"", filename.c_str());
        return false;
    }
    const ImageSpec &spec (file->spec());
    if (dataname == "resolution" && datatype==TypeDesc(TypeDesc::INT,2)) {
        int *d = (int *)data;
        d[0] = spec.width;
        d[1] = spec.height;
        return true;
    }
    if (dataname == "texturetype" && datatype == TypeDesc::TypeString) {
        ustring s (texture_type_name (file->textureformat()));
        *(const char **)data = s.c_str();
        return true;
    }
    if (dataname == "textureformat" && datatype == TypeDesc::TypeString) {
        ustring s (texture_format_name (file->textureformat()));
        *(const char **)data = s.c_str();
        return true;
    }
    if (dataname == "channels" && datatype == TypeDesc::TypeInt) {
        *(int *)data = spec.nchannels;
        return true;
    }
    if (dataname == "channels" && datatype == TypeDesc::TypeFloat) {
        *(float *)data = spec.nchannels;
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
ImageCacheImpl::get_imagespec (ustring filename, ImageSpec &spec, int subimage)
{
    ImageCacheFile *file = find_file (filename);
    if (! file) {
        error ("Image file \"%s\" not found", filename.c_str());
        return false;
    }
    if (file->broken()) {
        error ("Invalid image file \"%s\"", filename.c_str());
        return false;
    }
    spec = file->spec (subimage);
    return true;
}



bool
ImageCacheImpl::get_pixels (ustring filename, int level,
                            int xmin, int xmax, int ymin, int ymax,
                            int zmin, int zmax, 
                            TypeDesc format, void *result)
{
    ImageCacheFile *file = find_file (filename);
    if (! file) {
        error ("Image file \"%s\" not found", filename.c_str());
        return false;
    }
    if (file->broken()) {
        error ("Invalid image file \"%s\"", filename.c_str());
        return false;
    }
    if (level < 0 || level >= file->levels()) {
        error ("get_pixels asked for nonexistant level %d of \"%s\"",
               level, filename.c_str());
        return false;
    }
    const ImageSpec &spec (file->spec());

    // FIXME -- this could be WAY more efficient than starting from
    // scratch for each pixel within the rectangle.  Instead, we should
    // grab a whole tile at a time and memcpy it rapidly.  But no point
    // doing anything more complicated (not to mention bug-prone) until
    // somebody reports this routine as being a bottleneck.
    int actualchannels = spec.nchannels;
    ImageCacheTileRef tile, lasttile;
    int nc = file->spec().nchannels;
    size_t formatpixelsize = nc * format.size();
    for (int z = zmin;  z <= zmax;  ++z) {
        int tz = z - (z % spec.tile_depth);
        for (int y = ymin;  y <= ymax;  ++y) {
            int ty = y - (y % spec.tile_height);
            for (int x = xmin;  x <= xmax;  ++x) {
                int tx = x - (x % spec.tile_width);
                TileID tileid (*file, level, tx, ty, tz);
                find_tile (tileid, tile, lasttile);
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
    return false;
}



ImageCache::Tile *
ImageCacheImpl::get_tile (ustring filename, int level, int x, int y, int z)
{
    ImageCacheFile *file = find_file (filename);
    if (! file || file->broken())
        return NULL;
    TileID id (*file, level, x, y, z);
    ImageCacheTileRef tile;
    find_tile (id, tile);
    tile->_incref();   // Fake an extra reference count
    return tile->valid() ? (ImageCache::Tile *) tile.get() : NULL;
}



void
ImageCacheImpl::release_tile (ImageCache::Tile *tile) const
{
    if (! tile)
        return;
    ImageCacheTileRef tileref((ImageCacheTile *)tile);
    tileref->used ();
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



std::string
ImageCacheImpl::geterror () const
{
    lock_guard lock (m_errmutex);
    std::string e = m_errormessage;
    m_errormessage.clear();
    return e;
}



void
ImageCacheImpl::error (const char *message, ...)
{
    lock_guard lock (m_errmutex);
    va_list ap;
    va_start (ap, message);
    m_errormessage = Strutil::vformat (message, ap);
    va_end (ap);
}



static shared_ptr<ImageCacheImpl> shared_image_cache;
static mutex shared_image_cache_mutex;

};  // end namespace OpenImageIO::pvt



ImageCache *
ImageCache::create (bool shared)
{
    if (shared) {
        // They requested a shared cache.  If a shared cache already
        // exists, just return it, otherwise record the new cache.
        lock_guard guard (shared_image_cache_mutex);
        if (! shared_image_cache.get())
            shared_image_cache.reset (new ImageCacheImpl);
#ifdef DEBUG
        std::cerr << " shared ImageCache is " 
                  << (void *)shared_image_cache.get() << "\n";
#endif
        return shared_image_cache.get ();
    }

    // Doesn't need a shared cache
    ImageCacheImpl *ic = new ImageCacheImpl;
#ifdef DEBUG
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
    lock_guard guard (shared_image_cache_mutex);
    if (x != shared_image_cache.get())
        delete (ImageCacheImpl *) x;
}


};  // end namespace OpenImageIO

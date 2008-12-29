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


/// \file
/// Non-public classes used internally by ImgeCacheImpl.


#ifndef IMAGECACHE_PVT_H
#define IMAGECACHE_PVT_H

#include "texture.h"
#include "refcnt.h"

namespace OpenImageIO {
namespace pvt {

class ImageCacheImpl;

const char * texture_format_name (TexFormat f);
const char * texture_type_name (TexFormat f);



/// Unique in-memory record for each image file on disk.  Note that
/// this class is not in and of itself thread-safe.  It's critical that
/// any calling routine use a mutex any time a ImageCacheFile's methods are
/// being called, including constructing a new ImageCacheFile.
///
class ImageCacheFile : public RefCnt {
public:
    ImageCacheFile (ImageCacheImpl &imagecache, ustring filename);
    ~ImageCacheFile ();

    bool broken () const { return m_broken; }
    int levels () const { return (int)m_spec.size(); }
    const ImageSpec & spec (int level=0) const { return m_spec[level]; }
    ustring filename (void) const { return m_filename; }
    TexFormat textureformat () const { return m_texformat; }
    TextureOptions::Wrap swrap () const { return m_swrap; }
    TextureOptions::Wrap twrap () const { return m_twrap; }
    bool opened () const { return m_input.get() != NULL; }
    TypeDesc datatype () const { return m_datatype; }
    ImageCacheImpl &imagecache () const { return m_imagecache; }

    /// We will need to read pixels from the file, so be sure it's
    /// currently opened.  Return true if ok, false if error.
    bool open ();

    /// Load new data tile
    ///
    bool read_tile (int level, int x, int y, int z,
                    TypeDesc format, void *data);

    /// Mark the file as recently used.
    ///
    void use (void) { m_used = true; }

    /// Try to release resources for this file -- if recently used, mark
    /// as not recently used; if already not recently used, close the
    /// file and return true.
    void release (void);

    size_t channelsize () const { return m_channelsize; }
    size_t pixelsize () const { return m_pixelsize; }
    bool eightbit (void) const { return m_eightbit; }

private:
    ustring m_filename;             ///< Filename
    bool m_used;                    ///< Recently used (in the LRU sense)
    bool m_broken;                  ///< has errors; can't be used properly
    bool m_untiled;                 ///< Not tiled
    bool m_unmipped;                ///< Not really MIP-mapped
    shared_ptr<ImageInput> m_input; ///< Open ImageInput, NULL if closed
    std::vector<ImageSpec> m_spec;  ///< Format for each MIP-map level
    TexFormat m_texformat;          ///< Which texture format
    TextureOptions::Wrap m_swrap;   ///< Default wrap modes
    TextureOptions::Wrap m_twrap;   ///< Default wrap modes
    Imath::M44f m_Mlocal;           ///< shadows: world-to-local (light) matrix
    Imath::M44f m_Mproj;            ///< shadows: world-to-pseudo-NDC
    Imath::M44f m_Mtex;             ///< shadows: world-to-pNDC with camera z
    Imath::M44f m_Mras;             ///< shadows: world-to-raster with camera z
    TypeDesc m_datatype;            ///< Type of pixels we store internally
    CubeLayout m_cubelayout;        ///< cubemap: which layout?
    bool m_y_up;                    ///< latlong: is y "up"? (else z is up)
    bool m_eightbit;                ///< Eight bit?  (or float)
    unsigned int m_channelsize;     ///< Channel size, in bytes
    unsigned int m_pixelsize;       ///< Channel size, in bytes

    ImageCacheImpl &m_imagecache;   ///< Back pointer for ImageCache

    /// Close and delete the ImageInput, if currently open
    ///
    void close (void);

    /// Load the requested tile, from a file that's not really tiled.
    /// Preconditions: the ImageInput is already opened, and we already did
    /// a seek_subimage to the right mip level.
    bool read_untiled (int level, int x, int y, int z,
                       TypeDesc format, void *data);
};



/// Reference-counted pointer to a ImageCacheFile
///
typedef intrusive_ptr<ImageCacheFile> ImageCacheFileRef;


/// Map file names to file references
///
typedef hash_map<ustring,ImageCacheFileRef,ustringHash> FilenameMap;



/// Compact identifier for a particular tile of a particular image
///
class TileID {
public:
    /// Default constructor -- do not define
    ///
    TileID ();

    /// Initialize a TileID based on full elaboration of image file,
    /// MIPmap level, and tile x,y,z indices.
    TileID (ImageCacheFile &file, int level, int x, int y, int z=0)
        : m_file(file), m_level(level), m_x(x), m_y(y), m_z(z)
    { }

    /// Destructor is trivial, because we don't hold any resources
    /// of our own.  This is by design.
    ~TileID () { }

    ImageCacheFile &file (void) const { return m_file; }
    int level (void) const { return m_level; }
    int x (void) const { return m_x; }
    int y (void) const { return m_y; }
    int z (void) const { return m_z; }

    /// Do the two ID's refer to the same tile?  
    ///
    friend bool equal (const TileID &a, const TileID &b) {
        // Try to speed up by comparing field by field in order of most
        // probable rejection if they really are unequal.
        return (a.m_x == b.m_x && a.m_y == b.m_y && a.m_z == b.m_z &&
                a.m_level == b.m_level && (&a.m_file == &b.m_file));
    }

    /// Do the two ID's refer to the same tile, given that the
    /// caller *guarantees* that the two tiles point to the same
    /// file and level (so it only has to compare xyz)?
    friend bool equal_same_level (const TileID &a, const TileID &b) {
        DASSERT ((&a.m_file == &b.m_file) && a.m_level == b.m_level);
        return (a.m_x == b.m_x && a.m_y == b.m_y && a.m_z == b.m_z);
    }

    /// Do the two ID's refer to the same tile?  
    ///
    bool operator== (const TileID &b) const { return equal (*this, b); }

    /// Digest the TileID into a size_t to use as a hash key.  We do
    /// this by multiplying each element by a different prime and
    /// summing, so that collisions are unlikely.
    size_t hash () const {
        return m_x * 53 + m_y * 97 + m_z * 193 + 
               m_level * 389 + (size_t)(&m_file) * 769;
    }

    /// Functor that hashes a TileID
    class Hasher
#ifdef WINNT
        : public hash_compare<TileID>
#endif
    {
      public:
        size_t operator() (const TileID &a) const { return a.hash(); }
        bool operator() (const TileID &a, const TileID &b) const {
            return a.hash() < b.hash();
        }
    };

private:
    int m_x, m_y, m_z;        ///< x,y,z tile index within the mip level
    int m_level;              ///< MIP-map level
    ImageCacheFile &m_file;   ///< Which ImageCacheFile we refer to
};




/// Record for a single image tile.
///
class ImageCacheTile : public RefCnt {
public:
    ImageCacheTile (const TileID &id);

    ~ImageCacheTile ();

    /// Return pointer to the floating-point pixel data
    ///
    const float *data (void) const { return (const float *)&m_pixels[0]; }

    /// Return pointer to the floating-point pixel data for a particular
    /// pixel.  Be extremely sure the pixel is within this tile!
    const void *data (int x, int y, int z=0) const;

    /// Return a pointer to the character data
    ///
    const unsigned char *bytedata (void) const {
        return (unsigned char *) &m_pixels[0];
    }

    /// Return the id for this tile.
    ///
    const TileID& id (void) const { return m_id; }

    const ImageCacheFile & file () const { return m_id.file(); }
    /// Return the allocated memory size for this tile's pixels.
    ///
    size_t memsize () const {
        const ImageSpec &spec (file().spec(m_id.level()));
        return spec.tile_pixels() * spec.nchannels * file().datatype().size();
    }

    /// Mark the tile as recently used (or not, if used==false).  Return
    /// its previous value.
    bool used (bool used=true) { bool r = m_used;  m_used = used;  return r; }

    /// Has this tile been recently used?
    ///
    bool used (void) const { return m_used; }

    bool valid (void) const { return m_valid; }

private:
    TileID m_id;                  ///< ID of this tile
    std::vector<char> m_pixels;   ///< The pixel data
    bool m_valid;                 ///< Valid pixels
    bool m_used;                  ///< Used recently
    float m_mindepth, m_maxdepth; ///< shadows only: min/max depth of the tile
};



/// Reference-counted pointer to a ImageCacheTile
/// 
typedef intrusive_ptr<ImageCacheTile> ImageCacheTileRef;



/// Hash table that maps TileID to ImageCacheTileRef -- this is the type of the
/// main tile cache.
typedef hash_map<TileID, ImageCacheTileRef, TileID::Hasher> TileCache;



/// Working implementation of the abstract ImageCache class.
///
class ImageCacheImpl : public ImageCache {
public:
    ImageCacheImpl ();
    virtual ~ImageCacheImpl ();

    virtual bool attribute (const std::string &name, TypeDesc type, const void *val);
    virtual bool attribute (const std::string &name, int val) {
        return attribute (name, TypeDesc::INT, &val);
    }
    virtual bool attribute (const std::string &name, float val) {
        return attribute (name, TypeDesc::FLOAT, &val);
    }
    virtual bool attribute (const std::string &name, double val) {
        float f = (float) val;
        return attribute (name, TypeDesc::FLOAT, &f);
    }
    virtual bool attribute (const std::string &name, const char *val) {
        return attribute (name, TypeDesc::STRING, &val);
    }
    virtual bool attribute (const std::string &name, const std::string &val) {
        const char *s = val.c_str();
        return attribute (name, TypeDesc::INT, &s);
    }

    virtual bool getattribute (const std::string &name, TypeDesc type, void *val);
    virtual bool getattribute (const std::string &name, int &val) {
        return getattribute (name, TypeDesc::INT, &val);
    }
    virtual bool getattribute (const std::string &name, float &val) {
        return getattribute (name, TypeDesc::FLOAT, &val);
    }
    virtual bool getattribute (const std::string &name, double &val) {
        float f;
        bool ok = getattribute (name, TypeDesc::FLOAT, &f);
        if (ok)
            val = f;
        return ok;
    }
    virtual bool getattribute (const std::string &name, char **val) {
        return getattribute (name, TypeDesc::STRING, val);
    }
    virtual bool getattribute (const std::string &name, std::string &val) {
        const char *s;
        bool ok = getattribute (name, TypeDesc::STRING, &s);
        if (ok)
            val = s;
        return ok;
    }


    /// Close everything, free resources, start from scratch.
    ///
    virtual void clear () { }

    // Retrieve options
    int max_open_files () const { return m_max_open_files; }
    float max_memory_MB () const { return m_max_memory_MB; }
    std::string searchpath () const { return m_searchpath.string(); }
    int autotile () const { return m_autotile; }
    void get_commontoworld (Imath::M44f &result) const {
        result = m_Mc2w;
    }

    /// Get information about the given image.
    ///
    virtual bool get_image_info (ustring filename, ustring dataname,
                                 TypeDesc datatype, void *data);

    /// Get the ImageSpec associated with the named image.  If the file
    /// is found and is an image format that can be read, store a copy
    /// of its specification in spec and return true.  Return false if
    /// the file was not found or could not be opened as an image file
    /// by any available ImageIO plugin.
    virtual bool get_imagespec (ustring filename, ImageSpec &spec,
                                int subimage=0);

    /// Retrieve a rectangle of raw unfiltered pixels.
    ///
    virtual bool get_pixels (ustring filename, 
                             int subimage, int xmin, int xmax,
                             int ymin, int ymax, int zmin, int zmax, 
                             TypeDesc format, void *result);

    /// Find the ImageCacheFile record for the named image, or NULL if
    /// no such file can be found.  This returns a plain old pointer,
    /// which is ok because the file hash table has ref-counted pointers
    /// and those won't be freed until the texture system is destroyed.
    ImageCacheFile *find_file (ustring filename);

    /// Find a tile identified by 'id' in the tile cache, paging it in if
    /// needed, and store a reference to the tile.  Return true if ok,
    /// false if no such tile exists in the file or could not be read.
    void find_tile (const TileID &id, ImageCacheTileRef &tile);

    /// Find the tile specified by id and place its reference in 'tile'.
    /// Use tile and lasttile as a 2-item cache of tiles to boost our
    /// hit rate over the big cache.  This is just a wrapper around
    /// find_tile(id) and avoids looking to the big cache (and locking)
    /// most of the time for fairly coherent tile access patterns.
    /// If tile is null, so is lasttile.  Inlined for speed.
    void find_tile (const TileID &id,
                    ImageCacheTileRef &tile, ImageCacheTileRef &lasttile) {
        ++m_stat_find_tile_calls;
        if (tile) {
            if (tile->id() == id)
                return;    // already have the tile we want
            // Tile didn't match, maybe lasttile will?  Swap tile
            // and last tile.  Then the new one will either match,
            // or we'll fall through and replace tile.
            tile.swap (lasttile);
            if (tile && tile->id() == id)
                return;
        }
        find_tile (id, tile);
    }

    /// Find the tile specified by id and place its reference in 'tile'.
    /// Use tile and lasttile as a 2-item cache of tiles to boost our
    /// hit rate over the big cache.  The caller *guarantees* that tile
    /// contains a reference to a tile in the same file and MIP-map
    /// level as 'id', and so does lasttile (if it contains a reference
    /// at all).  Thus, it's a slightly simplified and faster version of
    /// find_tile and should be used in loops where it's known that we
    /// are reading several tiles from the same level.
    void find_tile_same_level (const TileID &id,
                               ImageCacheTileRef &tile, ImageCacheTileRef &lasttile) {
        ++m_stat_find_tile_calls;
        DASSERT (tile);
        if (equal_same_level (tile->id(), id))
            return;
        tile.swap (lasttile);
        if (tile && equal_same_level (tile->id(), id))
            return;
        find_tile (id, tile);
    }

    virtual Tile *get_tile (ustring filename, int level, int x, int y, int z);
    virtual void release_tile (Tile *tile) const;
    virtual const void * tile_pixels (Tile *tile, TypeDesc &format) const;

    virtual std::string geterror () const;
    virtual std::string getstats (int level=1) const;

    void operator delete (void *todel) { ::delete ((char *)todel); }

private:
    void init ();

    /// Called when a new file is opened, so that the system can track
    /// the number of simultaneously-opened files.  This should only
    /// be invoked when the caller holds m_images_mutex.
    void incr_open_files (void) {
        ++m_stat_open_files_created;
        ++m_stat_open_files_current;
        if (m_stat_open_files_current > m_stat_open_files_peak)
            m_stat_open_files_peak = m_stat_open_files_current;
    }

    /// Called when a file is closed, so that the system can track
    /// the number of simultyaneously-opened files.  This should only
    /// be invoked when the caller holds m_images_mutex.
    void decr_open_files (void) { --m_stat_open_files_current; }

    /// Enforce the max number of open files.  This should only be invoked
    /// when the caller holds m_images_mutex.
    void check_max_files ();

    /// Enforce the max memory for tile data.  This should only be invoked
    /// when the caller holds m_images_mutex.
    void check_max_mem ();

    /// Internal error reporting routine, with printf-like arguments.
    ///
    void error (const char *message, ...);

    /// Internal statistics printing routine
    ///
    void printstats () const;

#if 0
    // Turns out this isn't really any faster in my tests.
    typedef fast_mutex mutex_t;
    typedef fast_mutex::lock_guard lock_guard_t;
#else
    typedef mutex mutex_t;
    typedef lock_guard lock_guard_t;
#endif

    int m_max_open_files;
    float m_max_memory_MB;
    size_t m_max_memory_bytes;
    ustring m_searchpath;
    int m_autotile;              ///< if nonzero, pretend tiles of this size
    Imath::M44f m_Mw2c;          ///< world-to-"common" matrix
    Imath::M44f m_Mc2w;          ///< common-to-world matrix
    FilenameMap m_files;         ///< Map file names to ImageCacheFile's
    FilenameMap::iterator m_file_sweep; ///< Sweeper for "clock" paging algorithm
    mutex_t m_mutex;             ///< Thread safety
    TileCache m_tilecache;       ///< Our in-memory tile cache
    TileCache::iterator m_tile_sweep; ///< Sweeper for "clock" paging algorithm
    size_t m_mem_used;           ///< Memory being used for tiles
    int m_statslevel;            ///< Statistics level
    mutable std::string m_errormessage;   ///< Saved error string.
    mutable mutex m_errmutex;             ///< error mutex
    int m_stat_find_tile_calls;
    int m_stat_find_tile_microcache_misses;
    int m_stat_find_tile_cache_misses;
    int m_stat_tiles_created;
    int m_stat_tiles_current;
    int m_stat_tiles_peak;
    long long m_stat_files_totalsize;
    long long m_stat_bytes_read;
    int m_stat_open_files_created;
    int m_stat_open_files_current;
    int m_stat_open_files_peak;
    int m_stat_unique_files;
    double m_stat_fileio_time;
    double m_stat_locking_time;

    friend class ImageCacheFile;
    friend class ImageCacheTile;
};



};  // end namespace OpenImageIO::pvt
};  // end namespace OpenImageIO


#endif // IMAGECACHE_PVT_H

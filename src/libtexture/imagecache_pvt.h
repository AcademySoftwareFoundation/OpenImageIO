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

#ifdef DEBUG
# define IMAGECACHE_TIME_STATS 1
#else
    // Change the following to 1 to get timing statistics even for
    // optimized runs.  Note that this has some performance penalty.
# define IMAGECACHE_TIME_STATS 0
#endif


class ImageCacheImpl;

const char * texture_format_name (TexFormat f);
const char * texture_type_name (TexFormat f);



/// Unique in-memory record for each image file on disk.  Note that
/// this class is not in and of itself thread-safe.  It's critical that
/// any calling routine use a mutex any time a ImageCacheFile's methods are
/// being called, including constructing a new ImageCacheFile.
///
/// The public routines of ImageCacheFile are thread-safe!  In
/// particular, callers do not need to lock around calls to read_tile.
///
class ImageCacheFile : public RefCnt {
public:
    ImageCacheFile (ImageCacheImpl &imagecache, ustring filename);
    ~ImageCacheFile ();

    bool broken () const { return m_broken; }
    int subimages () const { return (int)m_spec.size(); }
    const ImageSpec & spec (int subimage=0) const { return m_spec[subimage]; }
    ustring filename (void) const { return m_filename; }
    ustring fileformat (void) const { return m_fileformat; }
    TexFormat textureformat () const { return m_texformat; }
    TextureOptions::Wrap swrap () const { return m_swrap; }
    TextureOptions::Wrap twrap () const { return m_twrap; }
    TypeDesc datatype () const { return m_datatype; }
    ImageCacheImpl &imagecache () const { return m_imagecache; }

    /// Load new data tile
    ///
    bool read_tile (int subimage, int x, int y, int z,
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
    bool untiled (void) const { return m_untiled; }
    bool unmipped (void) const { return m_unmipped; }

    void invalidate ();

    size_t timesopened () const { return m_timesopened; }
    size_t tilesread () const { return m_tilesread; }
    imagesize_t bytesread () const { return m_bytesread; }
    double & iotime () { return m_iotime; }

    std::time_t mod_time () const { return m_mod_time; }
    ustring fingerprint () const { return m_fingerprint; }
    void duplicate (ImageCacheFile *dup) { m_duplicate = dup;}
    ImageCacheFile *duplicate () const { return m_duplicate; }

private:
    ustring m_filename;             ///< Filename
    bool m_used;                    ///< Recently used (in the LRU sense)
    bool m_broken;                  ///< has errors; can't be used properly
    bool m_untiled;                 ///< Not tiled
    bool m_unmipped;                ///< Not really MIP-mapped
    shared_ptr<ImageInput> m_input; ///< Open ImageInput, NULL if closed
    std::vector<ImageSpec> m_spec;  ///< Format for each subimage
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
    ustring m_fileformat;           ///< File format name
    size_t m_tilesread;             ///< Tiles read from this file
    imagesize_t m_bytesread;        ///< Bytes read from this file
    size_t m_timesopened;           ///< Separate times we opened this file
    double m_iotime;                ///< I/O time for this file
    ImageCacheImpl &m_imagecache;   ///< Back pointer for ImageCache
    mutable recursive_mutex m_input_mutex; ///< Mutex protecting the ImageInput
    std::time_t m_mod_time;         ///< Time file was last updated
    ustring m_fingerprint;          ///< Optional cryptographic fingerprint
    ImageCacheFile *m_duplicate;    ///< Is this a duplicate?

    /// We will need to read pixels from the file, so be sure it's
    /// currently opened.  Return true if ok, false if error.
    bool open ();

    bool opened () const { return m_input.get() != NULL; }

    /// Close and delete the ImageInput, if currently open
    ///
    void close (void);

    /// Load the requested tile, from a file that's not really tiled.
    /// Preconditions: the ImageInput is already opened, and we already did
    /// a seek_subimage to the right subimage.
    bool read_untiled (int subimage, int x, int y, int z,
                       TypeDesc format, void *data);

    /// Load the requested tile, from a file that's not really MIPmapped.
    /// Preconditions: the ImageInput is already opened, and we already did
    /// a seek_subimage to the right subimage.
    bool read_unmipped (int subimage, int x, int y, int z,
                        TypeDesc format, void *data);

    friend class ImageCacheImpl;
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
    /// subimage, and tile x,y,z indices.
    TileID (ImageCacheFile &file, int subimage, int x, int y, int z=0)
        : m_x(x), m_y(y), m_z(z), m_subimage(subimage), m_file(file)
    { }

    /// Destructor is trivial, because we don't hold any resources
    /// of our own.  This is by design.
    ~TileID () { }

    ImageCacheFile &file (void) const { return m_file; }
    int subimage (void) const { return m_subimage; }
    int x (void) const { return m_x; }
    int y (void) const { return m_y; }
    int z (void) const { return m_z; }

    void x (int v) { m_x = v; }
    void y (int v) { m_y = v; }
    void z (int v) { m_z = v; }

    /// Do the two ID's refer to the same tile?  
    ///
    friend bool equal (const TileID &a, const TileID &b) {
        // Try to speed up by comparing field by field in order of most
        // probable rejection if they really are unequal.
        return (a.m_x == b.m_x && a.m_y == b.m_y && a.m_z == b.m_z &&
                a.m_subimage == b.m_subimage && (&a.m_file == &b.m_file));
    }

    /// Do the two ID's refer to the same tile, given that the
    /// caller *guarantees* that the two tiles point to the same
    /// file and subimage (so it only has to compare xyz)?
    friend bool equal_same_subimage (const TileID &a, const TileID &b) {
        DASSERT ((&a.m_file == &b.m_file) && a.m_subimage == b.m_subimage);
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
               m_subimage * 389 + (size_t)(&m_file) * 769;
    }

    /// Functor that hashes a TileID
    class Hasher
#ifdef _WIN32
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
    int m_x, m_y, m_z;        ///< x,y,z tile index within the subimage
    int m_subimage;           ///< subimage (usually MIP-map level)
    ImageCacheFile &m_file;   ///< Which ImageCacheFile we refer to
};




/// Record for a single image tile.
///
class ImageCacheTile : public RefCnt {
public:
    /// Construct a new tile, read the pixels from disk.
    ///
    ImageCacheTile (const TileID &id);

    /// Construct a new tile out of the pixels supplied.
    ///
    ImageCacheTile (const TileID &id, void *pels, TypeDesc format,
                    stride_t xstride, stride_t ystride, stride_t zstride);

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
        const ImageSpec &spec (file().spec(m_id.subimage()));
        return spec.tile_pixels() * spec.nchannels * file().datatype().size();
    }

    /// Mark the tile as recently used.
    ///
    void use () { m_used = true; }

    /// Mark the tile as not recently used, return its previous value.
    ///
    bool release () { bool r = m_used;  m_used = false;  return r; }

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
        return attribute (name, TypeDesc::STRING, &s);
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
        const char *s = NULL;
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
    const std::string &searchpath () const { return m_searchpath; }
    int autotile () const { return m_autotile; }
    bool automip () const { return m_automip; }
    void get_commontoworld (Imath::M44f &result) const {
        result = m_Mc2w;
    }

    virtual std::string resolve_filename (const std::string &filename) const;

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

    // Retrieve a rectangle of raw unfiltered pixels.
    virtual bool get_pixels (ustring filename, 
                             int subimage, int xmin, int xmax,
                             int ymin, int ymax, int zmin, int zmax, 
                             TypeDesc format, void *result);

    /// Retrieve a rectangle of raw unfiltered pixels, from an open valid
    /// ImageCacheFile.
    bool get_pixels (ImageCacheFile *file, int subimage, int xmin, int xmax,
                     int ymin, int ymax, int zmin, int zmax, 
                     TypeDesc format, void *result);

    /// Find the ImageCacheFile record for the named image, or NULL if
    /// no such file can be found.  This returns a plain old pointer,
    /// which is ok because the file hash table has ref-counted pointers
    /// and those won't be freed until the texture system is destroyed.
    ImageCacheFile *find_file (ustring filename);

    /// Is the tile specified by the TileID already in the cache?
    /// Only safe to call when the caller holds tilemutex.
    bool tile_in_cache (const TileID &id) {
        TileCache::iterator found = m_tilecache.find (id);
        return (found != m_tilecache.end());
    }

    /// Add the tile to the cache.  This will grab a unique lock to the
    /// tilemutex, and will also enforce cache memory limits.
    void add_tile_to_cache (ImageCacheTileRef &tile);

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
    /// contains a reference to a tile in the same file and 
    /// subimage as 'id', and so does lasttile (if it contains a reference
    /// at all).  Thus, it's a slightly simplified and faster version of
    /// find_tile and should be used in loops where it's known that we
    /// are reading several tiles from the same subimage.
    void find_tile_same_subimage (const TileID &id,
                               ImageCacheTileRef &tile, ImageCacheTileRef &lasttile) {
        ++m_stat_find_tile_calls;
        DASSERT (tile);
        if (equal_same_subimage (tile->id(), id))
            return;
        tile.swap (lasttile);
        if (tile && equal_same_subimage (tile->id(), id))
            return;
        find_tile (id, tile);
    }

    virtual Tile *get_tile (ustring filename, int subimage, int x, int y, int z);
    virtual void release_tile (Tile *tile) const;
    virtual const void * tile_pixels (Tile *tile, TypeDesc &format) const;

    virtual std::string geterror () const;
    virtual std::string getstats (int level=1) const;
    virtual void invalidate (ustring filename);
    virtual void invalidate_all (bool force=false);

    void operator delete (void *todel) { ::delete ((char *)todel); }

    /// Called when a new file is opened, so that the system can track
    /// the number of simultaneously-opened files.
    void incr_open_files (void) {
        ++m_stat_open_files_created;
        ++m_stat_open_files_current;
        if (m_stat_open_files_current > m_stat_open_files_peak)
            m_stat_open_files_peak = m_stat_open_files_current;
        // FIXME -- can we make an atomic_max?
    }

    /// Called when a file is closed, so that the system can track
    /// the number of simultyaneously-opened files.
    void decr_open_files (void) {
        --m_stat_open_files_current;
    }

    /// Called when a new tile is created, to update all the stats.
    ///
    void incr_tiles (size_t size) {
        ++m_stat_tiles_created;
        ++m_stat_tiles_current;
        if (m_stat_tiles_current > m_stat_tiles_peak)
            m_stat_tiles_peak = m_stat_tiles_current;
        m_mem_used += size;
    }

    /// Called when a tile is destroyed, to update all the stats.
    ///
    void decr_tiles (size_t size) {
        --m_stat_tiles_current;
        m_mem_used -= size;
    }

    void incr_files_totalsize (size_t size) {
        m_stat_files_totalsize += size;
    }

    void incr_bytes_read (size_t size) {
        m_stat_bytes_read += size;
    }

    /// Internal error reporting routine, with printf-like arguments.
    ///
    void error (const char *message, ...);

private:
    void init ();

    /// Enforce the max number of open files.  This should only be invoked
    /// when the caller holds m_filemutex.
    void check_max_files ();

    /// Enforce the max memory for tile data.  This should only be invoked
    /// when the caller holds m_tilemutex.
    void check_max_mem ();

    /// Internal statistics printing routine
    ///
    void printstats () const;

    // Helper function for printstats()
    std::string onefile_stat_line (const ImageCacheFileRef &file,
                                   int i, bool includestats=true) const;

    int m_max_open_files;
    float m_max_memory_MB;
    size_t m_max_memory_bytes;
    std::string m_searchpath;    ///< Colon-separated directory list
    std::vector<std::string> m_searchdirs; ///< Searchpath split into dirs
    int m_autotile;              ///< if nonzero, pretend tiles of this size
    bool m_automip;              ///< auto-mipmap on demand?
    Imath::M44f m_Mw2c;          ///< world-to-"common" matrix
    Imath::M44f m_Mc2w;          ///< common-to-world matrix
    FilenameMap m_files;         ///< Map file names to ImageCacheFile's
    FilenameMap::iterator m_file_sweep; ///< Sweeper for "clock" paging algorithm
    FilenameMap m_fingerprints;  ///< Map fingerprints to files
    TileCache m_tilecache;       ///< Our in-memory tile cache
    TileCache::iterator m_tile_sweep; ///< Sweeper for "clock" paging algorithm
    size_t m_mem_used;           ///< Memory being used for tiles
    int m_statslevel;            ///< Statistics level
    mutable std::string m_errormessage;   ///< Saved error string.
    mutable shared_mutex m_filemutex; ///< Thread safety for file cache
    mutable shared_mutex m_tilemutex; ///< Thread safety for tile cache
    mutable mutex m_errmutex;         ///< error mutex

private:
    atomic_ll m_stat_find_tile_calls;
    atomic_ll m_stat_find_tile_microcache_misses;
    atomic_int m_stat_find_tile_cache_misses;
    atomic_int m_stat_tiles_created;
    atomic_int m_stat_tiles_current;
    atomic_int m_stat_tiles_peak;
    atomic_ll m_stat_files_totalsize;
    atomic_ll m_stat_bytes_read;
    atomic_int m_stat_open_files_created;
    atomic_int m_stat_open_files_current;
    atomic_int m_stat_open_files_peak;
    atomic_int m_stat_unique_files;
    double m_stat_fileio_time;
    double m_stat_file_locking_time;
    double m_stat_tile_locking_time;
    double m_stat_find_file_time;
    double m_stat_find_tile_time;

    // Simulate an atomic double with a long long!
    void incr_time_stat (double &stat, double incr) {
        DASSERT (sizeof (atomic_ll) == sizeof(double));
        double oldval, newval;
        long long *lloldval = (long long *)&oldval;
        long long *llnewval = (long long *)&newval;
        atomic_ll *llstat = (atomic_ll *)&stat;
        // Make long long and atomic_ll pointers to the doubles in question.
        do { 
            // Grab the double bits, shove into a long long
            *lloldval = *llstat;
            // increment
            newval = oldval + incr;
            // Now try to atomically swap it, and repeat until we've
            // done it with nobody else interfering.
        } while (llstat->compare_and_swap (*llnewval,*lloldval) != *lloldval); 
    }

};



};  // end namespace OpenImageIO::pvt
};  // end namespace OpenImageIO


#endif // IMAGECACHE_PVT_H

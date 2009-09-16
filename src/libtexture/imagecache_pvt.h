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


#ifndef OPENIMAGEIO_IMAGECACHE_PVT_H
#define OPENIMAGEIO_IMAGECACHE_PVT_H

#include "texture.h"
#include "refcnt.h"

#ifdef OPENIMAGEIO_NAMESPACE
namespace OPENIMAGEIO_NAMESPACE {
#endif

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
struct ImageCachePerThreadInfo;

const char * texture_format_name (TexFormat f);
const char * texture_type_name (TexFormat f);


/// Structure to hold IC and TS statistics.  We combine into a single
/// structure to minimize the number of costly thread_specific_ptr
/// retrievals.  If somebody is using the ImageCache without a
/// TextureSystem, a few extra stats come along for the ride, but this
/// has no performance penalty.
struct ImageCacheStatistics {
    // First, the ImageCache-specific fields:
    long long find_tile_calls;
    long long find_tile_microcache_misses;
    int find_tile_cache_misses;
    long long files_totalsize;
    long long bytes_read;
    // These stats are hard to deal with on a per-thread basis, so for
    // now, they are still atomics shared by the whole IC.
    // int tiles_created;
    // int tiles_current;
    // int tiles_peak;
    // int open_files_created;
    // int open_files_current;
    // int open_files_peak;
    int unique_files;
    double fileio_time;
    double fileopen_time;
    double file_locking_time;
    double tile_locking_time;
    double find_file_time;
    double find_tile_time;

    // TextureSystem-specific fields below:
    long long texture_queries;
    long long texture_batches;
    long long texture3d_queries;
    long long texture3d_batches;
    long long shadow_queries;
    long long shadow_batches;
    long long environment_queries;
    long long environment_batches;
    long long aniso_queries;
    long long aniso_probes;
    float max_aniso;
    long long closest_interps;
    long long bilinear_interps;
    long long cubic_interps;
    
    ImageCacheStatistics () { init (); }
    void init ();
    void merge (const ImageCacheStatistics &s);
};



/// Unique in-memory record for each image file on disk.  Note that
/// this class is not in and of itself thread-safe.  It's critical that
/// any calling routine use a mutex any time a ImageCacheFile's methods are
/// being called, including constructing a new ImageCacheFile.
///
/// The public routines of ImageCacheFile are thread-safe!  In
/// particular, callers do not need to lock around calls to read_tile.
/// However, a few of them require passing in a pointer to the
/// thread-specific IC data including microcache and statistics.
///
class ImageCacheFile : public RefCnt {
public:
    ImageCacheFile (ImageCacheImpl &imagecache,
                    ImageCachePerThreadInfo *thread_info, ustring filename);
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
    bool read_tile (ImageCachePerThreadInfo *thread_info,
                    int subimage, int x, int y, int z,
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
    bool mipused (void) const { return m_mipused; }

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
    bool m_mipused;                 ///< MIP level >0 accessed
    ImageCacheImpl &m_imagecache;   ///< Back pointer for ImageCache
    mutable recursive_mutex m_input_mutex; ///< Mutex protecting the ImageInput
    std::time_t m_mod_time;         ///< Time file was last updated
    ustring m_fingerprint;          ///< Optional cryptographic fingerprint
    ImageCacheFile *m_duplicate;    ///< Is this a duplicate?

    /// We will need to read pixels from the file, so be sure it's
    /// currently opened.  Return true if ok, false if error.
    bool open (ImageCachePerThreadInfo *thread_info);

    bool opened () const { return m_input.get() != NULL; }

    /// Close and delete the ImageInput, if currently open
    ///
    void close (void);

    /// Load the requested tile, from a file that's not really tiled.
    /// Preconditions: the ImageInput is already opened, and we already did
    /// a seek_subimage to the right subimage.
    bool read_untiled (ImageCachePerThreadInfo *thread_info,
                       int subimage, int x, int y, int z,
                       TypeDesc format, void *data);

    /// Load the requested tile, from a file that's not really MIPmapped.
    /// Preconditions: the ImageInput is already opened, and we already did
    /// a seek_subimage to the right subimage.
    bool read_unmipped (ImageCachePerThreadInfo *thread_info,
                        int subimage, int x, int y, int z,
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
    /// Requires a pointer to the thread-specific IC data including
    /// microcache and statistics.
    ImageCacheTile (const TileID &id, ImageCachePerThreadInfo *thread_info);

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



/// A very small amount of per-thread data that saves us from locking
/// the mutex quite as often.  We store things here used by both
/// ImageCache and TextureSystem, so they don't each need a costly
/// thread_specific_ptr retrieval.  There's no real penalty for this,
/// even if you are using only ImageCache but not TextureSystem.
struct ImageCachePerThreadInfo {
    // Store just a few filename/fileptr pairs
    static const int nlastfile = 4;
    ustring last_filename[nlastfile];
    ImageCacheFile *last_file[nlastfile];
    int next_last_file;
    // We have a two-tile "microcache", storing the last two tiles needed.
    ImageCacheTileRef tile, lasttile;
    atomic_int purge;   // If set, tile ptrs need purging!
    ImageCacheStatistics m_stats;
    bool shared;   // Pointed to both by the IC and the thread_specific_ptr

    ImageCachePerThreadInfo ()
        : next_last_file(0), shared(false)
    {
        for (int i = 0;  i < nlastfile;  ++i)
            last_file[i] = NULL;
        purge = 0;
    }

    // Add a new filename/fileptr pair to our microcache
    void filename (ustring n, ImageCacheFile *f) {
        last_filename[next_last_file] = n;
        last_file[next_last_file] = f;
        ++next_last_file;
        next_last_file %= nlastfile;
    }

    // See if a filename has a fileptr in the microcache
    ImageCacheFile *find_file (ustring n) const {
        for (int i = 0;  i < nlastfile;  ++i)
            if (last_filename[i] == n)
                return last_file[i];
        return NULL;
    }
};



/// Working implementation of the abstract ImageCache class.
///
/// Some of the methods require a pointer to the thread-specific IC data
/// including microcache and statistics.
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
    bool forcefloat () const { return m_forcefloat; }
    bool accept_untiled () const { return m_accept_untiled; }
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
                             int subimage, int xbegin, int xend,
                             int ybegin, int yend, int zbegin, int zend,
                             TypeDesc format, void *result);

    /// Retrieve a rectangle of raw unfiltered pixels, from an open valid
    /// ImageCacheFile.
    bool get_pixels (ImageCacheFile *file, ImageCachePerThreadInfo *thread_info,
                     int subimage, int xmin, int xmax,
                     int ymin, int ymax, int zmin, int zmax, 
                     TypeDesc format, void *result);

    /// Find the ImageCacheFile record for the named image, or NULL if
    /// no such file can be found.  This returns a plain old pointer,
    /// which is ok because the file hash table has ref-counted pointers
    /// and those won't be freed until the texture system is destroyed.
    ImageCacheFile *find_file (ustring filename,
                               ImageCachePerThreadInfo *thread_info);

    /// Is the tile specified by the TileID already in the cache?
    /// Only safe to call when the caller holds tilemutex.
    bool tile_in_cache (const TileID &id) {
        TileCache::iterator found = m_tilecache.find (id);
        return (found != m_tilecache.end());
    }

    /// Add the tile to the cache.  This will grab a unique lock to the
    /// tilemutex, and will also enforce cache memory limits.
    void add_tile_to_cache (ImageCacheTileRef &tile,
                            ImageCachePerThreadInfo *thread_info);

    /// Find a tile identified by 'id' in the tile cache, paging it in if
    /// needed, and store a reference to the tile.  Return true if ok,
    /// false if no such tile exists in the file or could not be read.
    bool find_tile_main_cache (const TileID &id, ImageCacheTileRef &tile,
                               ImageCachePerThreadInfo *thread_info);

    /// Find the tile specified by id.  If found, return true and place
    /// the tile ref in thread_info->tile; if not found, return false.
    /// This is more efficient than find_tile_main_cache() because it
    /// avoids looking to the big cache (and locking) most of the time
    /// for fairly coherent tile access patterns, by using the
    /// per-thread microcache to boost our hit rate over the big cache.
    /// Inlined for speed.
    bool find_tile (const TileID &id, ImageCachePerThreadInfo *thread_info) {
        ++thread_info->m_stats.find_tile_calls;
        ImageCacheTileRef &tile (thread_info->tile);
        if (tile) {
            if (tile->id() == id)
                return true;    // already have the tile we want
            // Tile didn't match, maybe lasttile will?  Swap tile
            // and last tile.  Then the new one will either match,
            // or we'll fall through and replace tile.
            tile.swap (thread_info->lasttile);
            if (tile && tile->id() == id)
                return true;
        }
        return find_tile_main_cache (id, tile, thread_info);
    }

    virtual Tile *get_tile (ustring filename, int subimage, int x, int y, int z);
    virtual void release_tile (Tile *tile) const;
    virtual const void * tile_pixels (Tile *tile, TypeDesc &format) const;

    virtual std::string geterror () const;
    virtual std::string getstats (int level=1) const;
    virtual void invalidate (ustring filename);
    virtual void invalidate_all (bool force=false);

    /// Merge all the per-thread statistics into one set of stats.
    ///
    void mergestats (ImageCacheStatistics &merged) const;

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

    /// Internal error reporting routine, with printf-like arguments.
    ///
    void error (const char *message, ...);

    /// Get a pointer to the caller's thread's per-thread info, or create
    /// one in the first place if there isn't one already.
    ImageCachePerThreadInfo *get_perthread_info ();

    /// Called when the IC is destroyed.  We have a list of all the 
    /// perthread pointers -- go through and delete the ones for which we
    /// hold the only remaining pointer.
    void erase_perthread_info ();

    /// This is called when the thread terminates.  If p->m_imagecache
    /// is non-NULL, there's still an imagecache alive that might want
    /// the per-thread info (say, for statistics, though it's safe to
    /// clear its tile microcache), so don't delete the perthread info
    /// (it will be owned thereafter by the IC).  If there is no IC still
    /// depending on it (signalled by m_imagecache == NULL), delete it.
    static void cleanup_perthread_info (ImageCachePerThreadInfo *p);

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

    thread_specific_ptr< ImageCachePerThreadInfo > m_perthread_info;
    std::vector<ImageCachePerThreadInfo *> m_all_perthread_info;
    static mutex m_perthread_info_mutex; ///< Thread safety for perthread
    int m_max_open_files;
    float m_max_memory_MB;
    size_t m_max_memory_bytes;
    std::string m_searchpath;    ///< Colon-separated directory list
    std::vector<std::string> m_searchdirs; ///< Searchpath split into dirs
    int m_autotile;              ///< if nonzero, pretend tiles of this size
    bool m_automip;              ///< auto-mipmap on demand?
    bool m_forcefloat;           ///< force all cache tiles to be float
    bool m_accept_untiled;       ///< Accept untiled images?
    Imath::M44f m_Mw2c;          ///< world-to-"common" matrix
    Imath::M44f m_Mc2w;          ///< common-to-world matrix
    FilenameMap m_files;         ///< Map file names to ImageCacheFile's
    FilenameMap::iterator m_file_sweep; ///< Sweeper for "clock" paging algorithm
    FilenameMap m_fingerprints;  ///< Map fingerprints to files
    TileCache m_tilecache;       ///< Our in-memory tile cache
    TileCache::iterator m_tile_sweep; ///< Sweeper for "clock" paging algorithm
    size_t m_mem_used;           ///< Memory being used for tiles
    int m_statslevel;            ///< Statistics level
    /// Saved error string, per-thread
    ///
    mutable thread_specific_ptr< std::string > m_errormessage;
#if 0
    // This approach uses regular shared mutexes to protect the caches.
    typedef shared_mutex ic_mutex;
    typedef shared_lock  ic_read_lock;
    typedef unique_lock  ic_write_lock;
#else
    // This alternate approach uses spin locks.
    typedef spin_mutex ic_mutex;
    typedef spin_lock  ic_read_lock;
    typedef spin_lock  ic_write_lock;
#endif
    mutable ic_mutex m_filemutex; ///< Thread safety for file cache
    mutable ic_mutex m_tilemutex; ///< Thread safety for tile cache

private:
    // Statistics that are really hard to track per-thread
    atomic_int m_stat_tiles_created;
    atomic_int m_stat_tiles_current;
    atomic_int m_stat_tiles_peak;
    atomic_int m_stat_open_files_created;
    atomic_int m_stat_open_files_current;
    atomic_int m_stat_open_files_peak;

    // Simulate an atomic double with a long long!
    void incr_time_stat (double &stat, double incr) {
        stat += incr;
        return;
#ifdef NOTHREADS
        stat += incr;
#else
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
#endif
    }

};



};  // end namespace OpenImageIO::pvt
};  // end namespace OpenImageIO

#ifdef OPENIMAGEIO_NAMESPACE
}; // end namespace OPENIMAGEIO_NAMESPACE
using namespace OPENIMAGEIO_NAMESPACE;
#endif

#endif // OPENIMAGEIO_IMAGECACHE_PVT_H

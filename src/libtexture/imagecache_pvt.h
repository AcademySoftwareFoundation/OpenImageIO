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


OIIO_NAMESPACE_ENTER
{

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
    int file_retry_success;
    int tile_retry_success;
    
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
    int subimages () const { return (int)m_subimages.size(); }
    int miplevels (int subimage) const {
        return (int)m_subimages[subimage].levels.size();
    }
    const ImageSpec & spec (int subimage, int miplevel) const {
        return levelinfo(subimage,miplevel).spec;
    }
    ImageSpec & spec (int subimage, int miplevel) {
        return levelinfo(subimage,miplevel).spec;
    }
    const ImageSpec & nativespec (int subimage, int miplevel) const {
        return levelinfo(subimage,miplevel).nativespec;
    }
    ustring filename (void) const { return m_filename; }
    ustring fileformat (void) const { return m_fileformat; }
    TexFormat textureformat () const { return m_texformat; }
    TextureOpt::Wrap swrap () const { return m_swrap; }
    TextureOpt::Wrap twrap () const { return m_twrap; }
    TextureOpt::Wrap rwrap () const { return m_rwrap; }
    TypeDesc datatype () const { return m_datatype; }
    ImageCacheImpl &imagecache () const { return m_imagecache; }
    ImageInput *imageinput () const { return m_input.get(); }

    /// Load new data tile
    ///
    bool read_tile (ImageCachePerThreadInfo *thread_info,
                    int subimage, int miplevel, int x, int y, int z,
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

    /// Info for each MIP level that isn't in the ImageSpec, or that we
    /// precompute.
    struct LevelInfo {
        ImageSpec spec;             ///< ImageSpec for the mip level
        ImageSpec nativespec;       ///< Native ImageSpec for the mip level
        bool full_pixel_range;      ///< pixel data window matches image window
        bool onetile;               ///< Whole level fits on one tile
        mutable bool polecolorcomputed;     ///< Pole color was computed
        mutable std::vector<float> polecolor;///< Pole colors
        LevelInfo (const ImageSpec &spec, const ImageSpec &nativespec);  ///< Initialize based on spec
    };

    /// Info for each subimage
    ///
    struct SubimageInfo {
        std::vector<LevelInfo> levels;  ///< Extra per-level info
        bool untiled;                   ///< Not tiled
        bool unmipped;                  ///< Not really MIP-mapped
        bool volume;                    ///< It's a volume image
        bool full_pixel_range;          ///< pixel data window matches image window
        // The scale/offset accounts for crops or overscans, converting
        // 0-1 texture space relative to the "display/full window" into 
        // 0-1 relative to the "pixel window".
        float sscale, soffset, tscale, toffset;

        SubimageInfo () : untiled(false), unmipped(false) { }
        ImageSpec &spec (int m) { return levels[m].spec; }
        const ImageSpec &spec (int m) const { return levels[m].spec; }
        const ImageSpec &nativespec (int m) const { return levels[m].nativespec; }
    };

    const SubimageInfo &subimageinfo (int subimage) const {
        return m_subimages[subimage];
    }

    SubimageInfo &subimageinfo (int subimage) { return m_subimages[subimage]; }

    const LevelInfo &levelinfo (int subimage, int miplevel) const {
        DASSERT ((int)m_subimages.size() > subimage);
        DASSERT ((int)m_subimages[subimage].levels.size() > miplevel);
        return m_subimages[subimage].levels[miplevel];
    }
    LevelInfo &levelinfo (int subimage, int miplevel) {
        DASSERT ((int)m_subimages.size() > subimage);
        DASSERT ((int)m_subimages[subimage].levels.size() > miplevel);
        return m_subimages[subimage].levels[miplevel];
    }

    /// Do we currently have a valid spec?
    bool validspec () const {
        DASSERT ((m_validspec == false || m_subimages.size() > 0) &&
                 "validspec is true, but subimages are empty");
        return m_validspec;
    }

    /// Forget the specs we know
    void invalidate_spec () {
        m_validspec = false;
        m_subimages.clear ();
    }

private:
    ustring m_filename;             ///< Filename
    bool m_used;                    ///< Recently used (in the LRU sense)
    bool m_broken;                  ///< has errors; can't be used properly
    shared_ptr<ImageInput> m_input; ///< Open ImageInput, NULL if closed
    std::vector<SubimageInfo> m_subimages;  ///< Image on each subimage
    TexFormat m_texformat;          ///< Which texture format
    TextureOpt::Wrap m_swrap;       ///< Default wrap modes
    TextureOpt::Wrap m_twrap;       ///< Default wrap modes
    TextureOpt::Wrap m_rwrap;       ///< Default wrap modes
    Imath::M44f m_Mlocal;           ///< shadows: world-to-local (light) matrix
    Imath::M44f m_Mproj;            ///< shadows: world-to-pseudo-NDC
    Imath::M44f m_Mtex;             ///< shadows: world-to-pNDC with camera z
    Imath::M44f m_Mras;             ///< shadows: world-to-raster with camera z
    TypeDesc m_datatype;            ///< Type of pixels we store internally
    EnvLayout m_envlayout;          ///< env map: which layout?
    bool m_y_up;                    ///< latlong: is y "up"? (else z is up)
    bool m_sample_border;           ///< are edge samples exactly on the border?
    bool m_eightbit;                ///< Eight bit?  (or float)
    unsigned int m_channelsize;     ///< Channel size, in bytes
    unsigned int m_pixelsize;       ///< Pixel size, in bytes
    ustring m_fileformat;           ///< File format name
    size_t m_tilesread;             ///< Tiles read from this file
    imagesize_t m_bytesread;        ///< Bytes read from this file
    size_t m_timesopened;           ///< Separate times we opened this file
    double m_iotime;                ///< I/O time for this file
    bool m_mipused;                 ///< MIP level >0 accessed
    volatile bool m_validspec;      ///< If false, reread spec upon open
    ImageCacheImpl &m_imagecache;   ///< Back pointer for ImageCache
    mutable recursive_mutex m_input_mutex; ///< Mutex protecting the ImageInput
    std::time_t m_mod_time;         ///< Time file was last updated
    ustring m_fingerprint;          ///< Optional cryptographic fingerprint
    ImageCacheFile *m_duplicate;    ///< Is this a duplicate?

    /// We will need to read pixels from the file, so be sure it's
    /// currently opened.  Return true if ok, false if error.
    bool open (ImageCachePerThreadInfo *thread_info);

    bool opened () const { return m_input.get() != NULL; }

    /// Force the file to open, thread-safe.
    bool forceopen (ImageCachePerThreadInfo *thread_info) {
        recursive_lock_guard guard (m_input_mutex);
        return open (thread_info);
    }

    /// Close and delete the ImageInput, if currently open
    ///
    void close (void);

    /// Load the requested tile, from a file that's not really tiled.
    /// Preconditions: the ImageInput is already opened, and we already did
    /// a seek_subimage to the right subimage and MIP level.
    bool read_untiled (ImageCachePerThreadInfo *thread_info,
                       int subimage, int miplevel, int x, int y, int z,
                       TypeDesc format, void *data);

    /// Load the requested tile, from a file that's not really MIPmapped.
    /// Preconditions: the ImageInput is already opened, and we already did
    /// a seek_subimage to the right subimage.
    bool read_unmipped (ImageCachePerThreadInfo *thread_info,
                        int subimage, int miplevel, int x, int y, int z,
                        TypeDesc format, void *data);

    void lock_input_mutex () {
#if (BOOST_VERSION >= 103500)
        m_input_mutex.lock ();
#else
        boost::detail::thread::lock_ops<recursive_mutex>::lock (m_input_mutex);
#endif
    }

    void unlock_input_mutex () {
#if (BOOST_VERSION >= 103500)
        m_input_mutex.unlock ();
#else
        boost::detail::thread::lock_ops<recursive_mutex>::unlock (m_input_mutex);
#endif
    }

    friend class ImageCacheImpl;
    friend class TextureSystemImpl;
};



/// Reference-counted pointer to a ImageCacheFile
///
typedef intrusive_ptr<ImageCacheFile> ImageCacheFileRef;


/// Map file names to file references
///
#ifdef OIIO_HAVE_BOOST_UNORDERED_MAP
typedef boost::unordered_map<ustring,ImageCacheFileRef,ustringHash> FilenameMap;
#else
typedef hash_map<ustring,ImageCacheFileRef,ustringHash> FilenameMap;
#endif




/// Compact identifier for a particular tile of a particular image
///
class TileID {
public:
    /// Default constructor -- do not define
    ///
    TileID ();

    /// Initialize a TileID based on full elaboration of image file,
    /// subimage, and tile x,y,z indices.
    TileID (ImageCacheFile &file, int subimage, int miplevel,
            int x, int y, int z=0)
        : m_x(x), m_y(y), m_z(z), m_subimage(subimage),
          m_miplevel(miplevel), m_file(file)
    { }

    /// Destructor is trivial, because we don't hold any resources
    /// of our own.  This is by design.
    ~TileID () { }

    ImageCacheFile &file (void) const { return m_file; }
    int subimage (void) const { return m_subimage; }
    int miplevel (void) const { return m_miplevel; }
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
                a.m_subimage == b.m_subimage && 
                a.m_miplevel == b.m_miplevel && (&a.m_file == &b.m_file));
    }

    /// Do the two ID's refer to the same tile, given that the
    /// caller *guarantees* that the two tiles point to the same
    /// file, subimage, and miplevel (so it only has to compare xyz)?
    friend bool equal_same_subimage (const TileID &a, const TileID &b) {
        DASSERT ((&a.m_file == &b.m_file) &&
                 a.m_subimage == b.m_subimage && a.m_miplevel == b.m_miplevel);
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
               m_subimage * 389 + m_miplevel * 1543 +
               m_file.filename().hash() * 769;
    }

    /// Functor that hashes a TileID
    class Hasher
    {
      public:
        size_t operator() (const TileID &a) const { return a.hash(); }
    };

private:
    int m_x, m_y, m_z;        ///< x,y,z tile index within the subimage
    int m_subimage;           ///< subimage
    int m_miplevel;           ///< MIP-map level
    ImageCacheFile &m_file;   ///< Which ImageCacheFile we refer to
};




/// Record for a single image tile.
///
class ImageCacheTile : public RefCnt {
public:
    /// Construct a new tile, read the pixels from disk if read_now is true.
    /// Requires a pointer to the thread-specific IC data including
    /// microcache and statistics.
    ImageCacheTile (const TileID &id, ImageCachePerThreadInfo *thread_info,
                    bool read_now=true);

    /// Construct a new tile out of the pixels supplied.
    ///
    ImageCacheTile (const TileID &id, void *pels, TypeDesc format,
                    stride_t xstride, stride_t ystride, stride_t zstride);

    ~ImageCacheTile ();

    /// Actually read the pixels.  The caller had better be the thread
    /// that constructed the tile.
    void read (ImageCachePerThreadInfo *thread_info);

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

    /// Return the actual allocated memory size for this tile's pixels.
    ///
    size_t memsize () const {
        return m_pixels.size();
    }

    /// Return the space that will be needed for this tile's pixels.
    ///
    size_t memsize_needed () const {
        const ImageSpec &spec (file().spec(m_id.subimage(),m_id.miplevel()));
        return spec.tile_pixels() * spec.nchannels * file().datatype().size();
    }

    /// Mark the tile as recently used.
    ///
    void use () { m_used = 1; }

    /// Mark the tile as not recently used, return its previous value.
    ///
    bool release () {
        if (! pixels_ready() || ! valid())
            return true;  // Don't really release invalid or unready tiles
        // If m_used is 1, set it to zero and return true.  If it was already
        // zero, it's fine and return false.
        return atomic_compare_and_exchange ((volatile int *)&m_used, 1, 0);
    }

    /// Has this tile been recently used?
    ///
    int used (void) const { return m_used; }

    bool valid (void) const { return m_valid; }

    /// Are the pixels ready for use?  If false, they're still being
    /// read from disk.
    bool pixels_ready () const { return m_pixels_ready; }

    /// Spin until the pixels have been read and are ready for use.
    ///
    void wait_pixels_ready () const;

private:
    TileID m_id;                  ///< ID of this tile
    std::vector<char> m_pixels;   ///< The pixel data
    bool m_valid;                 ///< Valid pixels
    atomic_int m_used;            ///< Used recently
    volatile bool m_pixels_ready; ///< The pixels have been read from disk
    float m_mindepth, m_maxdepth; ///< shadows only: min/max depth of the tile
};



/// Reference-counted pointer to a ImageCacheTile
/// 
typedef intrusive_ptr<ImageCacheTile> ImageCacheTileRef;



/// Hash table that maps TileID to ImageCacheTileRef -- this is the type of the
/// main tile cache.
#ifdef OIIO_HAVE_BOOST_UNORDERED_MAP
typedef boost::unordered_map<TileID, ImageCacheTileRef, TileID::Hasher> TileCache;
#else
typedef hash_map<TileID, ImageCacheTileRef, TileID::Hasher> TileCache;
#endif

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
    const std::string &searchpath () const { return m_searchpath; }
    const std::string &plugin_searchpath () const { return m_plugin_searchpath; }
    int autotile () const { return m_autotile; }
    bool autoscanline () const { return m_autoscanline; }
    bool automip () const { return m_automip; }
    bool forcefloat () const { return m_forcefloat; }
    bool accept_untiled () const { return m_accept_untiled; }
    bool accept_unmipped () const { return m_accept_unmipped; }
    int failure_retries () const { return m_failure_retries; }
    bool latlong_y_up_default () const { return m_latlong_y_up_default; }
    void get_commontoworld (Imath::M44f &result) const {
        result = m_Mc2w;
    }

    virtual std::string resolve_filename (const std::string &filename) const;

    /// Get information about the given image.
    ///
    virtual bool get_image_info (ustring filename, int subimage, int miplevel,
                         ustring dataname, TypeDesc datatype, void *data);

    /// Get the ImageSpec associated with the named image.  If the file
    /// is found and is an image format that can be read, store a copy
    /// of its specification in spec and return true.  Return false if
    /// the file was not found or could not be opened as an image file
    /// by any available ImageIO plugin.
    virtual bool get_imagespec (ustring filename, ImageSpec &spec,
                                int subimage=0, int miplevel=0,
                                bool native=false);

    virtual const ImageSpec *imagespec (ustring filename, int subimage=0,
                                        int miplevel=0, bool native=false);

    // Retrieve a rectangle of raw unfiltered pixels.
    virtual bool get_pixels (ustring filename, int subimage, int miplevel,
                             int xbegin, int xend,
                             int ybegin, int yend, int zbegin, int zend,
                             TypeDesc format, void *result);

    /// Retrieve a rectangle of raw unfiltered pixels, from an open valid
    /// ImageCacheFile.
    bool get_pixels (ImageCacheFile *file, ImageCachePerThreadInfo *thread_info,
                     int subimage, int miplevel, int xmin, int xmax,
                     int ymin, int ymax, int zmin, int zmax, 
                     TypeDesc format, void *result);

    /// Find the ImageCacheFile record for the named image, or NULL if
    /// no such file can be found.  This returns a plain old pointer,
    /// which is ok because the file hash table has ref-counted pointers
    /// and those won't be freed until the texture system is destroyed.
    ImageCacheFile *find_file (ustring filename,
                               ImageCachePerThreadInfo *thread_info);

    /// Is the tile specified by the TileID already in the cache?
    /// Assume the caller holds tilemutex, unless do_lock is true.
    bool tile_in_cache (const TileID &id,
                        ImageCachePerThreadInfo *thread_info,
                        bool do_lock = false) {
        TileCache::iterator found;
        if (do_lock) {
            DASSERT (m_tilemutex_holder != thread_info &&
                "tile_in_cache called with do_lock=true, but already locked!");
            ic_read_lock lock (m_tilemutex);
#ifdef DEBUG
            DASSERT (m_tilemutex_holder == NULL);
            m_tilemutex_holder = thread_info;
#endif
            found = m_tilecache.find (id);
#ifdef DEBUG
            m_tilemutex_holder = NULL;
#endif
        } else {
            // Caller already holds the lock
            DASSERT (m_tilemutex_holder == thread_info &&
                     "tile_in_cache caller should be the tile lock holder");
            found = m_tilecache.find (id);
        }
        return (found != m_tilecache.end());
    }

    /// Add the tile to the cache.  This will grab a unique lock to the
    /// tilemutex, and will also enforce cache memory limits.
    void add_tile_to_cache (ImageCacheTileRef &tile,
                            ImageCachePerThreadInfo *thread_info);

    /// Find the tile specified by id.  If found, return true and place
    /// the tile ref in thread_info->tile; if not found, return false.
    /// Try to avoid looking to the big cache (and locking) most of the
    /// time for fairly coherent tile access patterns, by using the
    /// per-thread microcache to boost our hit rate over the big cache.
    /// Inlined for speed.  The tile is marked as 'used'.
    bool find_tile (const TileID &id, ImageCachePerThreadInfo *thread_info) {
        DASSERT (m_tilemutex_holder != thread_info &&
                 "find_tile should not be holding the tile mutex when called");
        ++thread_info->m_stats.find_tile_calls;
        ImageCacheTileRef &tile (thread_info->tile);
        if (tile) {
            if (tile->id() == id) {
                tile->use ();
                return true;    // already have the tile we want
            }
            // Tile didn't match, maybe lasttile will?  Swap tile
            // and last tile.  Then the new one will either match,
            // or we'll fall through and replace tile.
            tile.swap (thread_info->lasttile);
            if (tile && tile->id() == id) {
                tile->use ();
                return true;
            }
        }
        return find_tile_main_cache (id, tile, thread_info);
        // N.B. find_tile_main_cache marks the tile as used
    }

    virtual Tile *get_tile (ustring filename, int subimage, int miplevel,
                            int x, int y, int z);
    virtual void release_tile (Tile *tile) const;
    virtual const void * tile_pixels (Tile *tile, TypeDesc &format) const;

    virtual std::string geterror () const;
    virtual std::string getstats (int level=1) const;
    virtual void reset_stats ();
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

    /// Called when a tile's pixel memory is allocated, but a new tile
    /// is not created.
    void incr_mem (size_t size) {
        m_mem_used += size;
    }

    /// Called when a tile is destroyed, to update all the stats.
    ///
    void decr_tiles (size_t size) {
        --m_stat_tiles_current;
        m_mem_used -= size;
        DASSERT (m_mem_used >= 0);
    }

    /// Internal error reporting routine, with printf-like arguments.
    ///
    /// void error (const char *message, ...);
    TINYFORMAT_WRAP_FORMAT (void, error, const,
        std::ostringstream msg;, msg, append_error(msg.str());)

    /// Append a string to the current error message
    void append_error (const std::string& message) const;

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

    /// Debugging aid -- which thread holds the tile mutex?
    ImageCachePerThreadInfo* &tilemutex_holder() { return m_tilemutex_holder; }

    /// Debugging aid -- which thread holds the file mutex?
    ImageCachePerThreadInfo* &filemutex_holder() { return m_filemutex_holder; }

    /// Ensure that the max_memory_bytes is at least newsize bytes.
    /// Override the previous value if necessary, with thread-safety.
    void set_min_cache_size (long long newsize);

    /// Wrapper around check_max_files that grabs the filemutex while it
    /// does so.
    void check_max_files_with_lock (ImageCachePerThreadInfo *thread_info);

private:
    void init ();

    /// Find a tile identified by 'id' in the tile cache, paging it in if
    /// needed, and store a reference to the tile.  Return true if ok,
    /// false if no such tile exists in the file or could not be read.
    bool find_tile_main_cache (const TileID &id, ImageCacheTileRef &tile,
                               ImageCachePerThreadInfo *thread_info);

    /// Enforce the max number of open files.  This should only be invoked
    /// when the caller holds m_filemutex.
    void check_max_files (ImageCachePerThreadInfo *thread_info);

    /// Enforce the max memory for tile data.  This should only be invoked
    /// when the caller holds m_tilemutex.
    void check_max_mem (ImageCachePerThreadInfo *thread_info);

    /// Debugging aid -- set which thread holds the tile mutex
    void tilemutex_holder (ImageCachePerThreadInfo *p) {
#ifdef DEBUG
        if (p)                                     // if we claim to own it,
            DASSERT (m_tilemutex_holder == NULL);  // nobody else better!
        m_tilemutex_holder = p;
#endif
    }
    /// Debugging aid -- set which thread holds the file mutex
    void filemutex_holder (ImageCachePerThreadInfo *p) {
#ifdef DEBUG
        if (p)                                     // if we claim to own it,
            DASSERT (m_filemutex_holder == NULL);  // nobody else better!
        m_filemutex_holder = p;
#endif
    }

    /// Internal statistics printing routine
    ///
    void printstats () const;

    // Helper function for printstats()
    std::string onefile_stat_line (const ImageCacheFileRef &file,
                                   int i, bool includestats=true) const;

    /// Search the fingerprint table for the given fingerprint.  If it
    /// doesn't already have an entry in the fingerprint map, then add
    /// one, mapping the it to file.  In either case, return the file it
    /// maps to (the caller can tell if it was newly added to the table
    /// by whether the return value is the same as the passed-in file).
    /// All the while, properly maintain thread safety on the
    /// fingerprint table.
    ImageCacheFile *find_fingerprint (ustring finger, ImageCacheFile *file);

    /// Clear the fingerprint list, thread-safe.
    void clear_fingerprints ();

    typedef spin_mutex ic_mutex;
    typedef spin_lock  ic_read_lock;
    typedef spin_lock  ic_write_lock;

    thread_specific_ptr< ImageCachePerThreadInfo > m_perthread_info;
    std::vector<ImageCachePerThreadInfo *> m_all_perthread_info;
    static mutex m_perthread_info_mutex; ///< Thread safety for perthread
    int m_max_open_files;
    atomic_ll m_max_memory_bytes;
    std::string m_searchpath;    ///< Colon-separated image directory list
    std::vector<std::string> m_searchdirs; ///< Searchpath split into dirs
    std::string m_plugin_searchpath; ///< Colon-separated plugin directory list
    int m_autotile;              ///< if nonzero, pretend tiles of this size
    bool m_autoscanline;         ///< autotile using full width tiles
    bool m_automip;              ///< auto-mipmap on demand?
    bool m_forcefloat;           ///< force all cache tiles to be float
    bool m_accept_untiled;       ///< Accept untiled images?
    bool m_accept_unmipped;      ///< Accept unmipped images?
    bool m_read_before_insert;   ///< Read tiles before adding to cache?
    bool m_deduplicate;          ///< Detect duplicate files?
    int m_failure_retries;       ///< Times to re-try disk failures
    bool m_latlong_y_up_default; ///< Is +y the default "up" for latlong?
    Imath::M44f m_Mw2c;          ///< world-to-"common" matrix
    Imath::M44f m_Mc2w;          ///< common-to-world matrix

    mutable ic_mutex m_filemutex; ///< Thread safety for file cache
    FilenameMap m_files;         ///< Map file names to ImageCacheFile's
    FilenameMap::iterator m_file_sweep; ///< Sweeper for "clock" paging algorithm
    ImageCachePerThreadInfo *m_filemutex_holder; // debugging

    spin_mutex m_fingerprints_mutex; ///< Protect m_fingerprints
    FilenameMap m_fingerprints;  ///< Map fingerprints to files

    mutable ic_mutex m_tilemutex; ///< Thread safety for tile cache
    TileCache m_tilecache;       ///< Our in-memory tile cache
    TileCache::iterator m_tile_sweep; ///< Sweeper for "clock" paging algorithm
    ImageCachePerThreadInfo *m_tilemutex_holder;   // debugging

    atomic_ll m_mem_used;        ///< Memory being used for tiles
    int m_statslevel;            ///< Statistics level

    /// Saved error string, per-thread
    ///
    mutable thread_specific_ptr< std::string > m_errormessage;

    // For debugging -- keep track of who holds the tile and file mutex

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
#  if USE_TBB
        } while (llstat->compare_and_swap (*llnewval,*lloldval) != *lloldval);
#  else
        } while (llstat->bool_compare_and_swap (*llnewval,*lloldval));
#  endif
#endif
    }

};



};  // end namespace pvt

}
OIIO_NAMESPACE_EXIT


#endif // OPENIMAGEIO_IMAGECACHE_PVT_H

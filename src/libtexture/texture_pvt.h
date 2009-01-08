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
/// Non-public classes used internally by TextureSystemImpl.


#ifndef TEXTURE_PVT_H
#define TEXTURE_PVT_H


class Filter1D;  // Forward declaration;


namespace OpenImageIO {

class ImageCache;

namespace pvt {

class TextureSystemImpl;

#ifndef IMAGECACHE_PVT_H
class ImageCacheImpl;
class ImageCacheFile;
class ImageCacheTile;
class ImageCacheTileRef;
#endif



/// Working implementation of the abstract TextureSystem class.
///
class TextureSystemImpl : public TextureSystem {
public:
    TextureSystemImpl (ImageCache *imagecache);
    virtual ~TextureSystemImpl ();

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
    void get_commontoworld (Imath::M44f &result) const {
        result = m_Mc2w;
    }

    /// Filtered 2D texture lookup for a single point, no runflags.
    ///
    virtual bool texture (ustring filename, TextureOptions &options,
                  float s, float t,
                  float dsdx, float dtdx, float dsdy, float dtdy,
                  float *result) {
        Runflag rf = RunFlagOn;
        return texture (filename, options, &rf, 0, 0, s, t,
                        dsdx, dtdx, dsdy, dtdy, result);
    }

    /// Retrieve a 2D texture lookup at many points at once.
    ///
    virtual bool texture (ustring filename, TextureOptions &options,
                          Runflag *runflags, int firstactive, int lastactive,
                          VaryingRef<float> s, VaryingRef<float> t,
                          VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                          VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                          float *result);

    /// Retrieve a 3D texture lookup at a single point.
    ///
    virtual bool texture (ustring filename, TextureOptions &options,
                          const Imath::V3f &P,
                          const Imath::V3f &dPdx, const Imath::V3f &dPdy,
                          float *result) {
        Runflag rf = RunFlagOn;
        return texture (filename, options, &rf, 0, 0, *(Imath::V3f *)&P,
                        *(Imath::V3f *)&dPdx, *(Imath::V3f *)&dPdy, result);
    }

    /// Retrieve 3D filtered texture lookup
    ///
    virtual bool texture (ustring filename, TextureOptions &options,
                          Runflag *runflags, int firstactive, int lastactive,
                          VaryingRef<Imath::V3f> P,
                          VaryingRef<Imath::V3f> dPdx,
                          VaryingRef<Imath::V3f> dPdy,
                          float *result) {
        return false;
    }

    /// Retrieve a shadow lookup for a single position P.
    ///
    virtual bool shadow (ustring filename, TextureOptions &options,
                         const Imath::V3f &P, const Imath::V3f &dPdx,
                         const Imath::V3f &dPdy, float *result) {
        Runflag rf = RunFlagOn;
        return shadow (filename, options, &rf, 0, 0, *(Imath::V3f *)&P,
                       *(Imath::V3f *)&dPdx, *(Imath::V3f *)&dPdy, result);
    }

    /// Retrieve a shadow lookup for position P at many points at once.
    ///
    virtual bool shadow (ustring filename, TextureOptions &options,
                         Runflag *runflags, int firstactive, int lastactive,
                         VaryingRef<Imath::V3f> P,
                         VaryingRef<Imath::V3f> dPdx,
                         VaryingRef<Imath::V3f> dPdy,
                         float *result) {
        return false;
    }

    /// Retrieve an environment map lookup for direction R.
    ///
    virtual bool environment (ustring filename, TextureOptions &options,
                              const Imath::V3f &R, const Imath::V3f &dRdx,
                              const Imath::V3f &dRdy, float *result) {
        Runflag rf = RunFlagOn;
        return environment (filename, options, &rf, 0, 0, *(Imath::V3f *)&R,
                            *(Imath::V3f *)&dRdx, *(Imath::V3f *)&dRdy, result);
    }

    /// Retrieve an environment map lookup for direction R, for many
    /// points at once.
    virtual bool environment (ustring filename, TextureOptions &options,
                              Runflag *runflags, int firstactive, int lastactive,
                              VaryingRef<Imath::V3f> R,
                              VaryingRef<Imath::V3f> dRdx,
                              VaryingRef<Imath::V3f> dRdy,
                              float *result) {
        return false;
    }

    /// Get information about the given texture.
    ///
    virtual bool get_texture_info (ustring filename, ustring dataname,
                                   TypeDesc datatype, void *data);

    /// Get the ImageSpec associated with the named texture
    /// (specifically, the first MIP-map level).  If the file is found
    /// and is an image format that can be read, store a copy of its
    /// specification in spec and return true.  Return false if the file
    /// was not found or could not be opened as an image file by any
    /// available ImageIO plugin.
    virtual bool get_imagespec (ustring filename, ImageSpec &spec);

    /// Retrieve a rectangle of raw unfiltered texels.
    ///
    virtual bool get_texels (ustring filename, TextureOptions &options,
                             int subimage, int xmin, int xmax,
                             int ymin, int ymax, int zmin, int zmax,
                             TypeDesc format, void *result);

    virtual std::string geterror () const;
    virtual std::string getstats (int level=1, bool icstats=true) const;

    void operator delete (void *todel) { ::delete ((char *)todel); }

private:
    typedef ImageCacheFile TextureFile;
    typedef ImageCacheTileRef TileRef;

    /// A very small amount of per-thread data that saves us from locking
    /// the mutex quite as often.
    struct PerThreadInfo {
        // Store just a few filename/fileptr pairs
        static const int nlastfile = 4;
        ustring last_filename[nlastfile];
        ImageCacheFile *last_file[nlastfile];
        int next_last_file;
        ImageCacheTileRef tilecache0, tilecache1;
        std::list<ImageCacheTileRef> minicache;

        PerThreadInfo () : next_last_file(0) {
            for (int i = 0;  i < nlastfile;  ++i)
                last_file[i] = NULL;
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

    /// Get a pointer to the caller's thread's per-thread info, or create
    /// one in the first place if there isn't one already.
    PerThreadInfo *get_perthread_info () {
        PerThreadInfo *p = m_perthread_info.get();
        if (! p) {
            p = new PerThreadInfo;
            m_perthread_info.reset (p);
        }
        return p;
    }

    void init ();

    /// Find the TextureFile record for the named texture, or NULL if no
    /// such file can be found.
    TextureFile *find_texturefile (ustring filename) {
        return m_imagecache->find_file (filename);
    }

    /// Find the tile specified by id and place its reference in 'tile'.
    /// Use tile and lasttile as a 2-item cache of tiles to boost our
    /// hit rate over the big cache.  This is just a wrapper around
    /// find_tile(id) and avoids looking to the big cache (and locking)
    /// most of the time for fairly coherent tile access patterns.
    /// If tile is null, so is lasttile.  Inlined for speed.
    void find_tile (const TileID &id,
                    TileRef &tile, TileRef &lasttile)
    {
        m_imagecache->find_tile (id, tile, lasttile);
    }

    /// Find the tile specified by id and place its reference in 'tile'.
    /// Use tile and lasttile as a 2-item cache of tiles to boost our
    /// hit rate over the big cache.  The caller *guarantees* that tile
    /// contains a reference to a tile in the same file and MIP-map
    /// subimage as 'id', and so does lasttile (if it contains a reference
    /// at all).  Thus, it's a slightly simplified and faster version of
    /// find_tile and should be used in loops where it's known that we
    /// are reading several tiles from the same subimage.
    void find_tile_same_subimage (const TileID &id, TileRef &tile,
                               TileRef &lasttile)
    {
        m_imagecache->find_tile_same_subimage (id, tile, lasttile);
    }

    // Define a prototype of a member function pointer for texture
    // lookups.
    typedef void (TextureSystemImpl::*texture_lookup_prototype)
            (TextureFile &texfile, TextureOptions &options, int index,
             VaryingRef<float> _s, VaryingRef<float> _t,
             VaryingRef<float> _dsdx, VaryingRef<float> _dtdx,
             VaryingRef<float> _dsdy, VaryingRef<float> _dtdy,
             TileRef &tilecache0, TileRef &tilecache1,
             float *result);

    /// Look up texture from just ONE point
    ///
    void texture_lookup (TextureFile &texfile,
                         TextureOptions &options, int index,
                         VaryingRef<float> _s, VaryingRef<float> _t,
                         VaryingRef<float> _dsdx, VaryingRef<float> _dtdx,
                         VaryingRef<float> _dsdy, VaryingRef<float> _dtdy,
                         TileRef &tilecache0, TileRef &tilecache1,
                         float *result);
    
    void texture_lookup_nomip (TextureFile &texfile,
                         TextureOptions &options, int index,
                         VaryingRef<float> _s, VaryingRef<float> _t,
                         VaryingRef<float> _dsdx, VaryingRef<float> _dtdx,
                         VaryingRef<float> _dsdy, VaryingRef<float> _dtdy,
                         TileRef &tilecache0, TileRef &tilecache1,
                         float *result);
    
    void texture_lookup_trilinear_mipmap (TextureFile &texfile,
                         TextureOptions &options, int index,
                         VaryingRef<float> _s, VaryingRef<float> _t,
                         VaryingRef<float> _dsdx, VaryingRef<float> _dtdx,
                         VaryingRef<float> _dsdy, VaryingRef<float> _dtdy,
                         TileRef &tilecache0, TileRef &tilecache1,
                         float *result);
    
    typedef void (TextureSystemImpl::*accum_prototype)
                              (float s, float t, int level,
                               TextureFile &texturefile,
                               TextureOptions &options, int index,
                               TileRef &tilecache0, TileRef &tilecache1,
                               float weight, float *accum);

    void accum_sample_closest (float s, float t, int level,
                               TextureFile &texturefile,
                               TextureOptions &options, int index,
                               TileRef &tilecache0, TileRef &tilecache1,
                               float weight, float *accum);

    void accum_sample_bilinear (float s, float t, int level,
                                TextureFile &texturefile,
                                TextureOptions &options, int index,
                                TileRef &tilecache0, TileRef &tilecache1,
                                float weight, float *accum);

    void accum_sample_bicubic (float s, float t, int level,
                               TextureFile &texturefile,
                               TextureOptions &options, int index,
                               TileRef &tilecache0, TileRef &tilecache1,
                               float weight, float *accum);

    /// Internal error reporting routine, with printf-like arguments.
    ///
    void error (const char *message, ...);

    void printstats () const;

    ImageCacheImpl *m_imagecache;
    boost::thread_specific_ptr< PerThreadInfo > m_perthread_info;
    Imath::M44f m_Mw2c;          ///< world-to-"common" matrix
    Imath::M44f m_Mc2w;          ///< common-to-world matrix
    mutable std::string m_errormessage;   ///< Saved error string.
    mutable mutex m_errmutex;             ///< error mutex
    Filter1D *hq_filter;         ///< Better filter for magnification
    int m_statslevel;
    int m_stat_texture_queries;
    int m_stat_texture_batches;
    int m_stat_texture3d_queries;
    int m_stat_texture3d_batches;
    int m_stat_shadow_queries;
    int m_stat_shadow_batches;
    int m_stat_environment_queries;
    int m_stat_environment_batches;
    int m_stat_aniso_queries;
    int m_stat_aniso_probes;
    float m_stat_max_aniso;
    int m_stat_closest_interps;
    int m_stat_bilinear_interps;
    int m_stat_cubic_interps;
    friend class ImageCacheFile;
    friend class ImageCacheTile;
};



};  // end namespace OpenImageIO::pvt
};  // end namespace OpenImageIO


#endif // TEXTURE_PVT_H

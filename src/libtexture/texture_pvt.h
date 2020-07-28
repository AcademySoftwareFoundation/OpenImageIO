// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


/// \file
/// Non-public classes used internally by TextureSystemImpl.


#ifndef OPENIMAGEIO_TEXTURE_PVT_H
#define OPENIMAGEIO_TEXTURE_PVT_H

#include <OpenImageIO/simd.h>
#include <OpenImageIO/texture.h>

OIIO_NAMESPACE_BEGIN

class ImageCache;
class Filter1D;

namespace pvt {

class TextureSystemImpl;

#ifndef OPENIMAGEIO_IMAGECACHE_PVT_H
class ImageCacheImpl;
class ImageCacheFile;
class ImageCacheTile;
class ImageCacheTileRef;
#endif



/// Working implementation of the abstract TextureSystem class.
///
class TextureSystemImpl : public TextureSystem {
public:
    typedef ImageCacheFile TextureFile;

    TextureSystemImpl(ImageCache* imagecache);
    virtual ~TextureSystemImpl();

    virtual bool attribute(string_view name, TypeDesc type, const void* val);
    virtual bool attribute(string_view name, int val)
    {
        return attribute(name, TypeDesc::INT, &val);
    }
    virtual bool attribute(string_view name, float val)
    {
        return attribute(name, TypeDesc::FLOAT, &val);
    }
    virtual bool attribute(string_view name, double val)
    {
        float f = (float)val;
        return attribute(name, TypeDesc::FLOAT, &f);
    }
    virtual bool attribute(string_view name, string_view val)
    {
        const char* s = val.c_str();
        return attribute(name, TypeDesc::STRING, &s);
    }

    virtual bool getattribute(string_view name, TypeDesc type, void* val) const;
    virtual bool getattribute(string_view name, int& val) const
    {
        return getattribute(name, TypeDesc::INT, &val);
    }
    virtual bool getattribute(string_view name, float& val) const
    {
        return getattribute(name, TypeDesc::FLOAT, &val);
    }
    virtual bool getattribute(string_view name, double& val) const
    {
        float f;
        bool ok = getattribute(name, TypeDesc::FLOAT, &f);
        if (ok)
            val = f;
        return ok;
    }
    virtual bool getattribute(string_view name, char** val) const
    {
        return getattribute(name, TypeDesc::STRING, val);
    }
    virtual bool getattribute(string_view name, std::string& val) const
    {
        const char* s;
        bool ok = getattribute(name, TypeDesc::STRING, &s);
        if (ok)
            val = s;
        return ok;
    }


    // Retrieve options
    void get_commontoworld(Imath::M44f& result) const { result = m_Mc2w; }

    virtual Perthread* get_perthread_info(Perthread* thread_info = NULL)
    {
        return (Perthread*)m_imagecache->get_perthread_info(
            (ImageCachePerThreadInfo*)thread_info);
    }
    virtual Perthread* create_thread_info()
    {
        OIIO_ASSERT(m_imagecache);
        return (Perthread*)m_imagecache->create_thread_info();
    }
    virtual void destroy_thread_info(Perthread* threadinfo)
    {
        OIIO_ASSERT(m_imagecache);
        m_imagecache->destroy_thread_info((ImageCachePerThreadInfo*)threadinfo);
    }

    virtual TextureHandle* get_texture_handle(ustring filename,
                                              Perthread* thread)
    {
        PerThreadInfo* thread_info = thread
                                         ? ((PerThreadInfo*)thread)
                                         : m_imagecache->get_perthread_info();
        return (TextureHandle*)find_texturefile(filename, thread_info);
    }

    virtual bool good(TextureHandle* texture_handle)
    {
        return texture_handle && !((TextureFile*)texture_handle)->broken();
    }

    virtual bool texture(ustring filename, TextureOpt& options, float s,
                         float t, float dsdx, float dtdx, float dsdy,
                         float dtdy, int nchannels, float* result,
                         float* dresultds = NULL, float* dresultdt = NULL);
    virtual bool texture(TextureHandle* texture_handle, Perthread* thread_info,
                         TextureOpt& options, float s, float t, float dsdx,
                         float dtdx, float dsdy, float dtdy, int nchannels,
                         float* result, float* dresultds = NULL,
                         float* dresultdt = NULL);
    virtual bool texture(ustring filename, TextureOptBatch& options,
                         Tex::RunMask mask, const float* s, const float* t,
                         const float* dsdx, const float* dtdx,
                         const float* dsdy, const float* dtdy, int nchannels,
                         float* result, float* dresultds = nullptr,
                         float* dresultdt = nullptr);
    virtual bool texture(TextureHandle* texture_handle, Perthread* thread_info,
                         TextureOptBatch& options, Tex::RunMask mask,
                         const float* s, const float* t, const float* dsdx,
                         const float* dtdx, const float* dsdy,
                         const float* dtdy, int nchannels, float* result,
                         float* dresultds = nullptr,
                         float* dresultdt = nullptr);
    virtual bool texture(ustring filename, TextureOptions& options,
                         Runflag* runflags, int beginactive, int endactive,
                         VaryingRef<float> s, VaryingRef<float> t,
                         VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                         VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                         int nchannels, float* result, float* dresultds = NULL,
                         float* dresultdt = NULL);
    virtual bool texture(TextureHandle* texture_handle, Perthread* thread_info,
                         TextureOptions& options, Runflag* runflags,
                         int beginactive, int endactive, VaryingRef<float> s,
                         VaryingRef<float> t, VaryingRef<float> dsdx,
                         VaryingRef<float> dtdx, VaryingRef<float> dsdy,
                         VaryingRef<float> dtdy, int nchannels, float* result,
                         float* dresultds = NULL, float* dresultdt = NULL);

    virtual bool texture3d(ustring filename, TextureOpt& options,
                           const Imath::V3f& P, const Imath::V3f& dPdx,
                           const Imath::V3f& dPdy, const Imath::V3f& dPdz,
                           int nchannels, float* result,
                           float* dresultds = NULL, float* dresultdt = NULL,
                           float* dresultdr = NULL);
    virtual bool texture3d(TextureHandle* texture_handle,
                           Perthread* thread_info, TextureOpt& options,
                           const Imath::V3f& P, const Imath::V3f& dPdx,
                           const Imath::V3f& dPdy, const Imath::V3f& dPdz,
                           int nchannels, float* result,
                           float* dresultds = NULL, float* dresultdt = NULL,
                           float* dresultdr = NULL);
    virtual bool texture3d(ustring filename, TextureOptBatch& options,
                           Tex::RunMask mask, const float* P, const float* dPdx,
                           const float* dPdy, const float* dPdz, int nchannels,
                           float* result, float* dresultds = nullptr,
                           float* dresultdt = nullptr,
                           float* dresultdr = nullptr);
    virtual bool texture3d(TextureHandle* texture_handle,
                           Perthread* thread_info, TextureOptBatch& options,
                           Tex::RunMask mask, const float* P, const float* dPdx,
                           const float* dPdy, const float* dPdz, int nchannels,
                           float* result, float* dresultds = nullptr,
                           float* dresultdt = nullptr,
                           float* dresultdr = nullptr);
    virtual bool texture3d(ustring filename, TextureOptions& options,
                           Runflag* runflags, int beginactive, int endactive,
                           VaryingRef<Imath::V3f> P,
                           VaryingRef<Imath::V3f> dPdx,
                           VaryingRef<Imath::V3f> dPdy,
                           VaryingRef<Imath::V3f> dPdz, int nchannels,
                           float* result, float* dresultds = NULL,
                           float* dresultdt = NULL, float* dresultdr = NULL);
    virtual bool texture3d(TextureHandle* texture_handle,
                           Perthread* thread_info, TextureOptions& options,
                           Runflag* runflags, int beginactive, int endactive,
                           VaryingRef<Imath::V3f> P,
                           VaryingRef<Imath::V3f> dPdx,
                           VaryingRef<Imath::V3f> dPdy,
                           VaryingRef<Imath::V3f> dPdz, int nchannels,
                           float* result, float* dresultds = NULL,
                           float* dresultdt = NULL, float* dresultdr = NULL);

    virtual bool shadow(ustring /*filename*/, TextureOpt& /*options*/,
                        const Imath::V3f& /*P*/, const Imath::V3f& /*dPdx*/,
                        const Imath::V3f& /*dPdy*/, float* /*result*/,
                        float* /*dresultds*/, float* /*dresultdt*/)
    {
        return false;
    }
    virtual bool shadow(TextureHandle* /*texture_handle*/,
                        Perthread* /*thread_info*/, TextureOpt& /*options*/,
                        const Imath::V3f& /*P*/, const Imath::V3f& /*dPdx*/,
                        const Imath::V3f& /*dPdy*/, float* /*result*/,
                        float* /*dresultds*/, float* /*dresultdt*/)
    {
        return false;
    }
    virtual bool shadow(ustring /*filename*/, TextureOptBatch& /*options*/,
                        Tex::RunMask /*mask*/, const float* /*P*/,
                        const float* /*dPdx*/, const float* /*dPdy*/,
                        float* /*result*/, float* /*dresultds*/,
                        float* /*dresultdt*/)
    {
        return false;
    }
    virtual bool shadow(TextureHandle* /*texture_handle*/,
                        Perthread* /*thread_info*/,
                        TextureOptBatch& /*options*/, Tex::RunMask /*mask*/,
                        const float* /*P*/, const float* /*dPdx*/,
                        const float* /*dPdy*/, float* /*result*/,
                        float* /*dresultds*/, float* /*dresultdt*/)
    {
        return false;
    }
    virtual bool shadow(ustring /*filename*/, TextureOptions& /*options*/,
                        Runflag* /*runflags*/, int /*beginactive*/,
                        int /*endactive*/, VaryingRef<Imath::V3f> /*P*/,
                        VaryingRef<Imath::V3f> /*dPdx*/,
                        VaryingRef<Imath::V3f> /*dPdy*/, float* /*result*/,
                        float* /*dresultds*/, float* /*dresultdt*/)
    {
        return false;
    }
    virtual bool shadow(TextureHandle* /*texture_handle*/,
                        Perthread* /*thread_info*/, TextureOptions& /*options*/,
                        Runflag* /*runflags*/, int /*beginactive*/,
                        int /*endactive*/, VaryingRef<Imath::V3f> /*P*/,
                        VaryingRef<Imath::V3f> /*dPdx*/,
                        VaryingRef<Imath::V3f> /*dPdy*/, float* /*result*/,
                        float* /*dresultds*/, float* /*dresultdt*/)
    {
        return false;
    }


    virtual bool environment(ustring filename, TextureOpt& options,
                             const Imath::V3f& R, const Imath::V3f& dRdx,
                             const Imath::V3f& dRdy, int nchannels,
                             float* result, float* dresultds = NULL,
                             float* dresultdt = NULL);
    virtual bool environment(TextureHandle* texture_handle,
                             Perthread* thread_info, TextureOpt& options,
                             const Imath::V3f& R, const Imath::V3f& dRdx,
                             const Imath::V3f& dRdy, int nchannels,
                             float* result, float* dresultds = NULL,
                             float* dresultdt = NULL);
    virtual bool environment(ustring filename, TextureOptBatch& options,
                             Tex::RunMask mask, const float* R,
                             const float* dRdx, const float* dRdy,
                             int nchannels, float* result,
                             float* dresultds = nullptr,
                             float* dresultdt = nullptr);
    virtual bool environment(TextureHandle* texture_handle,
                             Perthread* thread_info, TextureOptBatch& options,
                             Tex::RunMask mask, const float* R,
                             const float* dRdx, const float* dRdy,
                             int nchannels, float* result,
                             float* dresultds = nullptr,
                             float* dresultdt = nullptr);
    virtual bool environment(ustring filename, TextureOptions& options,
                             Runflag* runflags, int beginactive, int endactive,
                             VaryingRef<Imath::V3f> R,
                             VaryingRef<Imath::V3f> dRdx,
                             VaryingRef<Imath::V3f> dRdy, int nchannels,
                             float* result, float* dresultds = NULL,
                             float* dresultdt = NULL);
    virtual bool environment(TextureHandle* texture_handle,
                             Perthread* thread_info, TextureOptions& options,
                             Runflag* runflags, int beginactive, int endactive,
                             VaryingRef<Imath::V3f> R,
                             VaryingRef<Imath::V3f> dRdx,
                             VaryingRef<Imath::V3f> dRdy, int nchannels,
                             float* result, float* dresultds = NULL,
                             float* dresultdt = NULL);

    virtual std::string resolve_filename(const std::string& filename) const;

    virtual bool get_texture_info(ustring filename, int subimage,
                                  ustring dataname, TypeDesc datatype,
                                  void* data);
    virtual bool get_texture_info(TextureHandle* texture_handle,
                                  Perthread* thread_info, int subimage,
                                  ustring dataname, TypeDesc datatype,
                                  void* data);

    virtual bool get_imagespec(ustring filename, int subimage, ImageSpec& spec);
    virtual bool get_imagespec(TextureHandle* texture_handle,
                               Perthread* thread_info, int subimage,
                               ImageSpec& spec);

    virtual const ImageSpec* imagespec(ustring filename, int subimage = 0);
    virtual const ImageSpec* imagespec(TextureHandle* texture_handle,
                                       Perthread* thread_info = NULL,
                                       int subimage           = 0);

    virtual bool get_texels(ustring filename, TextureOpt& options, int miplevel,
                            int xbegin, int xend, int ybegin, int yend,
                            int zbegin, int zend, int chbegin, int chend,
                            TypeDesc format, void* result);
    virtual bool get_texels(TextureHandle* texture_handle,
                            Perthread* thread_info, TextureOpt& options,
                            int miplevel, int xbegin, int xend, int ybegin,
                            int yend, int zbegin, int zend, int chbegin,
                            int chend, TypeDesc format, void* result);

    virtual std::string geterror() const;
    virtual std::string getstats(int level = 1, bool icstats = true) const;
    virtual void reset_stats();

    virtual void invalidate(ustring filename, bool force);
    virtual void invalidate_all(bool force = false);
    virtual void close(ustring filename);
    virtual void close_all();

    void operator delete(void* todel) { ::delete ((char*)todel); }

    typedef bool (*wrap_impl)(int& coord, int origin, int width);

    /// Return an opaque, non-owning pointer to the underlying ImageCache
    /// (if there is one).
    virtual ImageCache* imagecache() const { return m_imagecache; }

private:
    typedef ImageCacheTileRef TileRef;
    typedef ImageCachePerThreadInfo PerThreadInfo;

    void init();

    /// Find the TextureFile record for the named texture, or NULL if no
    /// such file can be found.
    TextureFile* find_texturefile(ustring filename, PerThreadInfo* thread_info)
    {
        return m_imagecache->find_file(filename, thread_info);
    }
    TextureFile* verify_texturefile(TextureFile* texturefile,
                                    PerThreadInfo* thread_info)
    {
        texturefile = m_imagecache->verify_file(texturefile, thread_info);
        if (!texturefile || texturefile->broken()) {
            std::string err = m_imagecache->geterror();
            if (err.size())
                errorf("%s", err);
#if 0
            // If the file is "broken", at least one verbose error message
            // has already been issued about it, so don't belabor the point.
            // But for debugging purposes, these might help:
            else if (texturefile && texturefile->broken())
                error ("(unknown error - broken texture \"%s\")", texturefile->filename());
            else
                error ("(unknown error - NULL texturefile)");
#endif
        }
        return texturefile;
    }

    /// Find the tile specified by id.  Just a pass-through to the
    /// underlying ImageCache.
    bool find_tile(const TileID& id, PerThreadInfo* thread_info,
                   bool mark_same_tile_used)
    {
        return m_imagecache->find_tile(id, thread_info, mark_same_tile_used);
    }

    // Define a prototype of a member function pointer for texture
    // lookups.
    // If simd is nonzero, it's guaranteed that all float* inputs and
    // outputs are padded to length 'simd' and aligned to a simd*4-byte
    // boundary (for example, 4 for SSE). This means that the functions can
    // behave AS IF the number of channels being retrieved is simd, and any
    // extra values returned will be discarded by the caller.
    typedef bool (TextureSystemImpl::*texture_lookup_prototype)(
        TextureFile& texfile, PerThreadInfo* thread_info, TextureOpt& options,
        int nchannels_result, int actualchannels, float _s, float _t,
        float _dsdx, float _dtdx, float _dsdy, float _dtdy, float* result,
        float* dresultds, float* resultdt);

    /// Look up texture from just ONE point
    ///
    bool texture_lookup(TextureFile& texfile, PerThreadInfo* thread_info,
                        TextureOpt& options, int nchannels_result,
                        int actualchannels, float _s, float _t, float _dsdx,
                        float _dtdx, float _dsdy, float _dtdy, float* result,
                        float* dresultds, float* resultdt);

    bool texture_lookup_nomip(TextureFile& texfile, PerThreadInfo* thread_info,
                              TextureOpt& options, int nchannels_result,
                              int actualchannels, float _s, float _t,
                              float _dsdx, float _dtdx, float _dsdy,
                              float _dtdy, float* result, float* dresultds,
                              float* resultdt);

    bool texture_lookup_trilinear_mipmap(
        TextureFile& texfile, PerThreadInfo* thread_info, TextureOpt& options,
        int nchannels_result, int actualchannels, float _s, float _t,
        float _dsdx, float _dtdx, float _dsdy, float _dtdy, float* result,
        float* dresultds, float* resultdt);

    // For the samplers, it's guaranteed that all float* inputs and outputs
    // are padded to length 'simd' and aligned to a simd*4-byte boundary
    // (for example, 4 for SSE). This means that the functions can behave AS
    // IF the number of channels being retrieved is simd, and any extra
    // values returned will be discarded by the caller.
    typedef bool (TextureSystemImpl::*sampler_prototype)(
        int nsamples, const float* s, const float* t, int level,
        TextureFile& texturefile, PerThreadInfo* thread_info,
        TextureOpt& options, int nchannels_result, int actualchannels,
        const float* weight, simd::vfloat4* accum, simd::vfloat4* daccumds,
        simd::vfloat4* daccumdt);
    bool sample_closest(int nsamples, const float* s, const float* t, int level,
                        TextureFile& texturefile, PerThreadInfo* thread_info,
                        TextureOpt& options, int nchannels_result,
                        int actualchannels, const float* weight,
                        simd::vfloat4* accum, simd::vfloat4* daccumds,
                        simd::vfloat4* daccumdt);
    bool sample_bilinear(int nsamples, const float* s, const float* t,
                         int level, TextureFile& texturefile,
                         PerThreadInfo* thread_info, TextureOpt& options,
                         int nchannels_result, int actualchannels,
                         const float* weight, simd::vfloat4* accum,
                         simd::vfloat4* daccumds, simd::vfloat4* daccumdt);
    bool sample_bicubic(int nsamples, const float* s, const float* t, int level,
                        TextureFile& texturefile, PerThreadInfo* thread_info,
                        TextureOpt& options, int nchannels_result,
                        int actualchannels, const float* weight,
                        simd::vfloat4* accum, simd::vfloat4* daccumds,
                        simd::vfloat4* daccumdt);

    // Define a prototype of a member function pointer for texture3d
    // lookups.
    typedef bool (TextureSystemImpl::*texture3d_lookup_prototype)(
        TextureFile& texfile, PerThreadInfo* thread_info, TextureOpt& options,
        int nchannels_result, int actualchannels, const Imath::V3f& P,
        const Imath::V3f& dPdx, const Imath::V3f& dPdy, const Imath::V3f& dPdz,
        float* result, float* dresultds, float* dresultdt, float* dresultdr);
    bool texture3d_lookup_nomip(TextureFile& texfile,
                                PerThreadInfo* thread_info, TextureOpt& options,
                                int nchannels_result, int actualchannels,
                                const Imath::V3f& P, const Imath::V3f& dPdx,
                                const Imath::V3f& dPdy, const Imath::V3f& dPdz,
                                float* result, float* dresultds,
                                float* dresultdt, float* dresultdr);
    typedef bool (TextureSystemImpl::*accum3d_prototype)(
        const Imath::V3f& P, int level, TextureFile& texturefile,
        PerThreadInfo* thread_info, TextureOpt& options, int nchannels_result,
        int actualchannels, float weight, float* accum, float* daccumds,
        float* daccumdt, float* daccumdr);
    bool accum3d_sample_closest(const Imath::V3f& P, int level,
                                TextureFile& texturefile,
                                PerThreadInfo* thread_info, TextureOpt& options,
                                int nchannels_result, int actualchannels,
                                float weight, float* accum, float* daccumds,
                                float* daccumdt, float* daccumdr);
    bool accum3d_sample_bilinear(const Imath::V3f& P, int level,
                                 TextureFile& texturefile,
                                 PerThreadInfo* thread_info,
                                 TextureOpt& options, int nchannels_result,
                                 int actualchannels, float weight, float* accum,
                                 float* daccumds, float* daccumdt,
                                 float* daccumdr);

    /// Helper function to calculate the anisotropic aspect ratio from
    /// the major and minor ellipse axis lengths.  The "clamped" aspect
    /// ratio is returned (possibly adjusting major and minorlength to
    /// conform to the aniso limits) but the true aspect is stored in
    /// 'trueaspect'.
    static float anisotropic_aspect(float& majorlength, float& minorlength,
                                    TextureOpt& options, float& trueaspect);

    /// Convert texture coordinates (s,t), which range on 0-1 for the
    /// "full" image boundary, to texel coordinates (i+ifrac,j+jfrac)
    /// where (i,j) is the texel to the immediate upper left of the
    /// sample position, and ifrac and jfrac are the fractional (0-1)
    /// portion of the way to the next texel to the right or down,
    /// respectively.
    void st_to_texel(float s, float t, TextureFile& texturefile,
                     const ImageSpec& spec, int& i, int& j, float& ifrac,
                     float& jfrac);

    /// Called when the requested texture is missing, fills in the
    /// results.
    bool missing_texture(TextureOpt& options, int nchannels, float* result,
                         float* dresultds, float* dresultdt,
                         float* dresultdr = NULL);

    /// Handle gray-to-RGB promotion.
    void fill_gray_channels(const ImageSpec& spec, int nchannels, float* result,
                            float* dresultds, float* dresultdt,
                            float* dresultdr = NULL);

    static bool wrap_periodic_sharedborder(int& coord, int origin, int width);
    static const wrap_impl wrap_functions[];

    /// Helper function for lat-long environment maps: compute a "pole"
    /// pixel that's the average of all of row y.  This will only be
    /// called for levels where the whole mipmap level fits on one tile.
    const float* pole_color(TextureFile& texturefile,
                            PerThreadInfo* thread_info,
                            const ImageCacheFile::LevelInfo& levelinfo,
                            TileRef& tile, int subimage, int miplevel,
                            int pole);
    /// Helper function for lat-long environment maps: called near pole
    /// regions, this figures out the average pole color and fades to it
    /// right at the pole, and also adjusts weight so that the regular
    /// interpolated texture color will be added in correctly.
    /// This should only be called on the edge texels.
    void fade_to_pole(float t, float* accum, float& weight,
                      TextureFile& texturefile, PerThreadInfo* thread_info,
                      const ImageCacheFile::LevelInfo& levelinfo,
                      TextureOpt& options, int miplevel, int nchannels);

    /// Perform short unit tests.
    void unit_test_texture();

    /// Internal error reporting routine, with printf-like arguments.
    template<typename... Args>
    void errorf(const char* fmt, const Args&... args) const
    {
        append_error(Strutil::sprintf(fmt, args...));
    }
    /// Internal error reporting routine, with std::format-like arguments.
    template<typename... Args>
    void error(const char* fmt, const Args&... args) const
    {
        append_error(Strutil::fmt::format(fmt, args...));
    }

    /// Append a string to the current error message
    void append_error(const std::string& message) const;

    void printstats() const;

    // Debugging aid
    void visualize_ellipse(const std::string& name, float dsdx, float dtdx,
                           float dsdy, float dtdy, float sblur, float tblur);

    ImageCacheImpl* m_imagecache = nullptr;
    bool m_imagecache_owner      = false;  ///< True if we own the ImageCache
    Imath::M44f m_Mw2c;                    ///< world-to-"common" matrix
    Imath::M44f m_Mc2w;                    ///< common-to-world matrix
    bool m_gray_to_rgb;       ///< automatically copy gray to rgb channels?
    bool m_flip_t;            ///< Flip direction of t coord?
    int m_max_tile_channels;  ///< narrow tile ID channel range when
                              ///<   the file has more channels
    /// Saved error string, per-thread
    ///
    mutable thread_specific_ptr<std::string> m_errormessage;
    Filter1D* hq_filter;  ///< Better filter for magnification
    int m_statslevel;
    friend class TextureSystem;
};



inline float
TextureSystemImpl::anisotropic_aspect(float& majorlength, float& minorlength,
                                      TextureOpt& options, float& trueaspect)
{
    float aspect = Imath::clamp(majorlength / minorlength, 1.0f, 1.0e6f);
    trueaspect   = aspect;
    if (aspect > options.anisotropic) {
        aspect = options.anisotropic;
        // We have to clamp the ellipse to the maximum amount of anisotropy
        // that we allow.  How do we do it?
        // a. Widen the short axis so we never alias along the major
        //    axis, but we over-blur along the minor axis.  I've never
        //    been happy with this, it visibly overblurs.
        // b. Clamp the long axis so we don't blur, but might alias.
        // c. Split the difference, take the geometric mean, this makes
        //    it slightly too blurry along the minor axis, slightly
        //    aliasing along the major axis.  You can't please everybody.
        if (options.conservative_filter) {
#if 0
            // Solution (a) -- never alias by blurring more along minor axis
            minorlength = majorlength / options.anisotropic;
#else
            // Solution (c) -- this is our default, usually a nice balance.
            // We used to take the geometric mean...
            //            majorlength = sqrtf ((majorlength) *
            //                                 (minorlength * options.anisotropic));
            // ...but frankly I find the average to be a little more
            // visually pleasing.
            majorlength
                = 0.5f * ((majorlength) + (minorlength * options.anisotropic));
            minorlength = majorlength / options.anisotropic;
#endif
        } else {
            // Solution (b) -- alias slightly, never overblur
            majorlength = minorlength * options.anisotropic;
        }
    }
    return aspect;
}



inline void
TextureSystemImpl::st_to_texel(float s, float t, TextureFile& texturefile,
                               const ImageSpec& spec, int& i, int& j,
                               float& ifrac, float& jfrac)
{
    // As passed in, (s,t) map the texture to (0,1).  Remap to texel coords.
    // Note that we have two modes, depending on the m_sample_border.
    if (texturefile.m_sample_border == 0) {
        // texel samples are at 0.5/res, 1.5/res, ..., (res-0.5)/res,
        s = s * spec.width + spec.x - 0.5f;
        t = t * spec.height + spec.y - 0.5f;
    } else {
        // first and last rows/columns are *exactly* on the boundary,
        // so samples are at 0, 1/(res-1), ..., 1.
        s = s * (spec.width - 1) + spec.x;
        t = t * (spec.height - 1) + spec.y;
    }
    ifrac = floorfrac(s, &i);
    jfrac = floorfrac(t, &j);
    // Now (i,j) are the integer coordinates of the texel to the
    // immediate "upper left" of the lookup point, and (ifrac,jfrac) are
    // the amount that the lookup point is actually offset from the
    // texel center (with (1,1) being all the way to the next texel down
    // and to the right).
}



}  // end namespace pvt

OIIO_NAMESPACE_END

#endif  // OPENIMAGEIO_TEXTURE_PVT_H

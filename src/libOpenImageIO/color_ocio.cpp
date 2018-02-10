/*
  Copyright 2012 Larry Gritz and the other authors and contributors.
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

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <boost/container/flat_map.hpp>

#include <OpenEXR/half.h>

#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>

#ifdef USE_OCIO
#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;
#endif


OIIO_NAMESPACE_BEGIN


// Class used as the key to index color processors in the cache.
class ColorProcCacheKey {
public:
    ColorProcCacheKey (ustring in, ustring out,
                       ustring key=ustring(), ustring val=ustring(),
                       ustring looks=ustring(), ustring display=ustring(),
                       ustring view=ustring(), ustring file=ustring(),
                       bool inverse=false)
        : inputColorSpace(in), outputColorSpace(out),
          context_key(key), context_value(val), looks(looks), file(file),
          inverse(inverse)
    {
        hash = inputColorSpace.hash() + 14033ul*outputColorSpace.hash()
             + 823ul*context_key.hash() + 28411ul*context_value.hash()
             + 1741ul*(looks.hash() + display.hash() + view.hash() + file.hash())
             + (inverse?6421:0);
        // N.B. no separate multipliers for looks, display, view, file
        // because they're never used for the same lookup.
    }

    friend bool operator< (const ColorProcCacheKey& a, const ColorProcCacheKey& b) {
        if (a.hash < b.hash) return true;
        if (b.hash < a.hash) return false;
        // They hash the same, so now compare for real. Note that we just to
        // impose an order, any order -- does not need to be alphabetical --
        // so we just compare the pointers.
        if (a.inputColorSpace.c_str() < b.inputColorSpace.c_str()) return true;
        if (b.inputColorSpace.c_str() < a.inputColorSpace.c_str()) return false;
        if (a.outputColorSpace.c_str() < b.outputColorSpace.c_str()) return true;
        if (b.outputColorSpace.c_str() < a.outputColorSpace.c_str()) return false;
        if (a.context_key.c_str() < b.context_key.c_str()) return true;
        if (b.context_key.c_str() < a.context_key.c_str()) return false;
        if (a.looks.c_str() < b.looks.c_str()) return true;
        if (b.looks.c_str() < a.looks.c_str()) return false;
        if (a.display.c_str() < b.display.c_str()) return true;
        if (b.display.c_str() < a.display.c_str()) return false;
        if (a.view.c_str() < b.view.c_str()) return true;
        if (b.view.c_str() < a.view.c_str()) return false;
        if (a.file.c_str() < b.view.c_str()) return true;
        if (b.view.c_str() < a.view.c_str()) return false;
        return int(a.inverse) < int(b.inverse);
    }

    ustring inputColorSpace;
    ustring outputColorSpace;
    ustring context_key;
    ustring context_value;
    ustring looks;
    ustring display;
    ustring view;
    ustring file;
    bool    inverse;
    size_t  hash;
};



typedef boost::container::flat_map<ColorProcCacheKey, ColorProcessorHandle> ColorProcessorMap;




bool
ColorConfig::supportsOpenColorIO ()
{
#ifdef USE_OCIO
    return true;
#else
    return false;
#endif
}



// Hidden implementation of ColorConfig
class ColorConfig::Impl
{
public:
#ifdef USE_OCIO
    OCIO::ConstConfigRcPtr config_;
#endif
    std::vector<std::pair<std::string,int> > colorspaces;
    std::string linear_alias;  // Alias for a scene-linear color space
private:
    mutable spin_rw_mutex m_mutex;
    mutable std::string m_error;
    ColorProcessorMap colorprocmap;  // cache of ColorProcessors
    atomic_int colorprocs_requested;
    atomic_int colorprocs_created;
    std::string m_configname;

public:
    Impl() { }

    ~Impl() {
#if 0
        // Debugging the cache -- make sure we're creating a small number
        // compared to repeated requests.
        if (colorprocs_requested)
            Strutil::printf ("ColorConfig::Impl : color procs requested: %d, created: %d\n",
                             colorprocs_requested, colorprocs_created);
#endif
    }

    void inventory ();
    void add (const std::string &name, int index) {
        colorspaces.emplace_back(name, index);
    }

    // Search for a matching ColorProcessor, return it if found (otherwise
    // return an empty handle).
    ColorProcessorHandle findproc (const ColorProcCacheKey &key) {
        ++colorprocs_requested;
        spin_rw_read_lock lock (m_mutex);
        auto found = colorprocmap.find (key);
        return (found == colorprocmap.end()) ? ColorProcessorHandle()
                                             : found->second;
    }

    // Add the given color processor. Be careful -- if a matching one is
    // already in the table, just return the existing one. If they pass
    // in an empty handle, just return it.
    ColorProcessorHandle addproc (const ColorProcCacheKey &key,
                                  ColorProcessorHandle handle) {
        if (! handle)
            return handle;
        ++colorprocs_created;
        spin_rw_write_lock lock (m_mutex);
        auto found = colorprocmap.find (key);
        if (found == colorprocmap.end()) {
            // No equivalent item in the map. Add this one.
            colorprocmap[key] = handle;
        } else {
            // There's already an equivalent one. Oops. Discard this one and
            // return the one already in the map.
            handle = found->second;
        }
        return handle;
    }

    void error (const std::string& err) {
        spin_rw_write_lock lock (m_mutex);
        m_error = err;
    }
    std::string geterror (bool clear = true) {
        std::string err;
        if (clear) {
            spin_rw_write_lock lock (m_mutex);
            err = m_error;
            m_error.clear();
        } else {
            spin_rw_read_lock lock (m_mutex);
            err = m_error;
        }
        return err;
    }
    bool haserror () const {
        spin_rw_read_lock lock (m_mutex);
        return ! m_error.empty();
    }
    void clear_error () {
        spin_rw_write_lock lock (m_mutex);
        m_error.clear();
    }

    const std::string& configname () const { return m_configname; }
    void configname (string_view name) { m_configname = name; }
};



// ColorConfig utility to take inventory of the color spaces available.
// It sets up knowledge of "linear", "sRGB", "Rec709",
// even if the underlying OCIO configuration lacks them.
void
ColorConfig::Impl::inventory ()
{
#ifdef USE_OCIO
    if (config_) {
        bool nonraw = false;
        for (int i = 0, e = config_->getNumColorSpaces();  i < e;  ++i)
            nonraw |= ! Strutil::iequals(config_->getColorSpaceNameByIndex(i), "raw");
        if (nonraw) {
            for (int i = 0, e = config_->getNumColorSpaces();  i < e;  ++i)
                add (config_->getColorSpaceNameByIndex(i), i);
            OCIO::ConstColorSpaceRcPtr lin = config_->getColorSpace ("scene_linear");
            if (lin)
                linear_alias = lin->getName();
            return;   // If any non-"raw" spaces were defined, we're done
        }
    }
    // If we had some kind of bogus configuration that seemed to define
    // only a "raw" color space and nothing else, that's useless, so
    // figure out our own way to move forward.
    config_.reset();
#endif

    // If there was no configuration, or we didn't compile with OCIO
    // support at all, register a few basic names we know about.
    add ("linear", 0);
    add ("default", 0);
    add ("rgb", 0);
    add ("RGB", 0);
    add ("sRGB", 1);
    add ("Rec709", 2);
}



ColorConfig::ColorConfig (string_view filename)
{
    reset (filename);
}



ColorConfig::~ColorConfig()
{
}



bool
ColorConfig::reset (string_view filename)
{
    bool ok = true;

    m_impl.reset (new ColorConfig::Impl);
#ifdef USE_OCIO
    OCIO::SetLoggingLevel (OCIO::LOGGING_LEVEL_NONE);
    try {
        if (filename.empty()) {
            getImpl()->config_ = OCIO::GetCurrentConfig();
            string_view ocioenv = Sysutil::getenv ("OCIO");
            if (ocioenv.size())
                getImpl()->configname (ocioenv);
        } else {
            getImpl()->config_ = OCIO::Config::CreateFromFile (filename.c_str());
            getImpl()->configname (filename);
        }
    }
    catch(OCIO::Exception &e) {
        getImpl()->error (e.what());
        ok = false;
    }
    catch(...) {
        getImpl()->error ("An unknown error occurred in OpenColorIO creating the config");
        ok = false;
    }
#endif

    getImpl()->inventory ();

    // If we populated our own, remove any errors.
    if (getNumColorSpaces() && !getImpl()->haserror())
        getImpl()->clear_error();

    return ok;
}



bool
ColorConfig::error () const
{
    return (getImpl()->haserror());
}



std::string
ColorConfig::geterror ()
{
    return getImpl()->geterror (true /*clear*/);
}



int
ColorConfig::getNumColorSpaces () const
{
    return (int) getImpl()->colorspaces.size();
}



const char *
ColorConfig::getColorSpaceNameByIndex (int index) const
{
    return getImpl()->colorspaces[index].first.c_str();
}



int
ColorConfig::getNumLooks () const
{
#ifdef USE_OCIO
    if (getImpl()->config_)
        return getImpl()->config_->getNumLooks();
#endif
    return 0;
}



const char *
ColorConfig::getLookNameByIndex (int index) const
{
#ifdef USE_OCIO
    if (getImpl()->config_)
        return getImpl()->config_->getLookNameByIndex (index);
#endif
    return NULL;
}



const char *
ColorConfig::getColorSpaceNameByRole (string_view role) const
{
#ifdef USE_OCIO
    if (getImpl()->config_) {
        OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace (role.c_str());
        // Catch special case of obvious name synonyms
        if (!c && (Strutil::iequals(role,"RGB") || Strutil::iequals(role,"default")))
            role = string_view("linear");
        if (!c && Strutil::iequals(role,"linear"))
            c = getImpl()->config_->getColorSpace ("scene_linear");
        if (!c && Strutil::iequals(role,"scene_linear"))
            c = getImpl()->config_->getColorSpace ("linear");
        if (c)
            return c->getName();
    }
#endif

    // No OCIO at build time, or no OCIO configuration at run time
    if (Strutil::iequals (role, "linear") || Strutil::iequals (role, "scene_linear"))
        return "linear";

    return NULL;  // Dunno what role
}



TypeDesc
ColorConfig::getColorSpaceDataType (string_view name, int *bits) const
{
#ifdef USE_OCIO
    OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace (name.c_str());
    if (c) {
        OCIO::BitDepth b = c->getBitDepth();
        switch (b) {
        case OCIO::BIT_DEPTH_UNKNOWN : return TypeDesc::UNKNOWN;
        case OCIO::BIT_DEPTH_UINT8   : *bits =  8; return TypeDesc::UINT8;
        case OCIO::BIT_DEPTH_UINT10  : *bits = 10; return TypeDesc::UINT16;
        case OCIO::BIT_DEPTH_UINT12  : *bits = 12; return TypeDesc::UINT16;
        case OCIO::BIT_DEPTH_UINT14  : *bits = 14; return TypeDesc::UINT16;
        case OCIO::BIT_DEPTH_UINT16  : *bits = 16; return TypeDesc::UINT16;
        case OCIO::BIT_DEPTH_UINT32  : *bits = 32; return TypeDesc::UINT32;
        case OCIO::BIT_DEPTH_F16     : *bits = 16; return TypeDesc::HALF;
        case OCIO::BIT_DEPTH_F32     : *bits = 32; return TypeDesc::FLOAT;
        }
    }
#endif
    return TypeDesc::UNKNOWN;
}



int
ColorConfig::getNumDisplays() const
{
#ifdef USE_OCIO
    if (getImpl()->config_)
        return getImpl()->config_->getNumDisplays();
#endif
    return 0;
}



const char *
ColorConfig::getDisplayNameByIndex(int index) const
{
#ifdef USE_OCIO
    if (getImpl()->config_)
        return getImpl()->config_->getDisplay(index);
#endif
    return NULL;
}



int
ColorConfig::getNumViews(string_view display) const
{
#ifdef USE_OCIO
    if (getImpl()->config_)
        return getImpl()->config_->getNumViews(display.c_str());
#endif
    return 0;
}



const char *
ColorConfig::getViewNameByIndex(string_view display, int index) const
{
#ifdef USE_OCIO
    if (getImpl()->config_)
        return getImpl()->config_->getView(display.c_str(), index);
#endif
    return NULL;
}



const char *
ColorConfig::getDefaultDisplayName() const
{
#ifdef USE_OCIO
    if (getImpl()->config_)
        return getImpl()->config_->getDefaultDisplay();
#endif
    return NULL;
}



const char *
ColorConfig::getDefaultViewName(string_view display) const
{
#ifdef USE_OCIO
    if (getImpl()->config_)
        return getImpl()->config_->getDefaultView(display.c_str());
#endif
    return NULL;
}



std::string
ColorConfig::configname () const
{
#ifdef USE_OCIO
    if (getImpl()->config_)
        return getImpl()->configname();
#endif
    return "built-in";
}



#ifdef USE_OCIO
// Custom ColorProcessor that wraps an OpenColorIO Processor.
class ColorProcessor_OCIO : public ColorProcessor
{
public:
    ColorProcessor_OCIO (OCIO::ConstProcessorRcPtr p) : m_p(p) {};
    virtual ~ColorProcessor_OCIO (void) { };
    
    virtual bool isNoOp() const { return m_p->isNoOp(); }
    virtual bool hasChannelCrosstalk() const {
        return m_p->hasChannelCrosstalk();
    }
    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
        OCIO::PackedImageDesc pid (data, width, height, channels,
                                   chanstride, xstride, ystride);
        m_p->apply (pid);
    }

private:
    OCIO::ConstProcessorRcPtr m_p;
};
#endif



// ColorProcessor that hard-codes sRGB-to-linear
class ColorProcessor_sRGB_to_linear : public ColorProcessor {
public:
    ColorProcessor_sRGB_to_linear () : ColorProcessor() { };
    ~ColorProcessor_sRGB_to_linear () { };

    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
        if (channels > 3)
            channels = 3;
        if (channels == 3) {
            for (int y = 0;  y < height;  ++y) {
                char *d = (char *)data + y*ystride;
                for (int x = 0;  x < width;  ++x, d += xstride) {
                    simd::vfloat4 r;
                    r.load ((float *)d, 3);
                    r = sRGB_to_linear (r);
                    r.store ((float *)d, 3);
                }
            }
        } else {
            for (int y = 0;  y < height;  ++y) {
                char *d = (char *)data + y*ystride;
                for (int x = 0;  x < width;  ++x, d += xstride)
                    for (int c = 0;  c < channels;  ++c)
                        ((float *)d)[c] = sRGB_to_linear (((float *)d)[c]);
            }
        }
    }
};


// ColorProcessor that hard-codes linear-to-sRGB
class ColorProcessor_linear_to_sRGB : public ColorProcessor {
public:
    ColorProcessor_linear_to_sRGB () : ColorProcessor() { };
    ~ColorProcessor_linear_to_sRGB () { };
    
    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
        if (channels > 3)
            channels = 3;
        if (channels == 3) {
            for (int y = 0;  y < height;  ++y) {
                char *d = (char *)data + y*ystride;
                for (int x = 0;  x < width;  ++x, d += xstride) {
                    simd::vfloat4 r;
                    r.load ((float *)d, 3);
                    r = linear_to_sRGB (r);
                    r.store ((float *)d, 3);
                }
            }
        } else {
            for (int y = 0;  y < height;  ++y) {
                char *d = (char *)data + y*ystride;
                for (int x = 0;  x < width;  ++x, d += xstride)
                    for (int c = 0;  c < channels;  ++c)
                        ((float *)d)[c] = linear_to_sRGB (((float *)d)[c]);
            }
        }
    }
};



// ColorProcessor that hard-codes Rec709-to-linear
class ColorProcessor_Rec709_to_linear : public ColorProcessor {
public:
    ColorProcessor_Rec709_to_linear () : ColorProcessor() { };
    ~ColorProcessor_Rec709_to_linear () { };

    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
        if (channels > 3)
            channels = 3;
        for (int y = 0;  y < height;  ++y) {
            char *d = (char *)data + y*ystride;
            for (int x = 0;  x < width;  ++x, d += xstride)
                for (int c = 0;  c < channels;  ++c)
                    ((float *)d)[c] = Rec709_to_linear (((float *)d)[c]);
        }
    }
};


// ColorProcessor that hard-codes linear-to-Rec709
class ColorProcessor_linear_to_Rec709 : public ColorProcessor {
public:
    ColorProcessor_linear_to_Rec709 () : ColorProcessor() { };
    ~ColorProcessor_linear_to_Rec709 () { };
    
    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
        if (channels > 3)
            channels = 3;
        for (int y = 0;  y < height;  ++y) {
            char *d = (char *)data + y*ystride;
            for (int x = 0;  x < width;  ++x, d += xstride)
                for (int c = 0;  c < channels;  ++c)
                    ((float *)d)[c] = linear_to_Rec709 (((float *)d)[c]);
        }
    }
};



// ColorProcessor that performs gamma correction
class ColorProcessor_gamma : public ColorProcessor {
public:
    ColorProcessor_gamma (float gamma)
        : ColorProcessor(), m_gamma(gamma)
    { };
    ~ColorProcessor_gamma () { };

    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
        if (channels > 3)
            channels = 3;
        if (channels == 3) {
            simd::vfloat4 g = m_gamma;
            for (int y = 0;  y < height;  ++y) {
                char *d = (char *)data + y*ystride;
                for (int x = 0;  x < width;  ++x, d += xstride) {
                    simd::vfloat4 r;
                    r.load ((float *)d, 3);
                    r = fast_pow_pos (r, g);
                    r.store ((float *)d, 3);
                }
            }
        } else {
            for (int y = 0;  y < height;  ++y) {
                char *d = (char *)data + y*ystride;
                for (int x = 0;  x < width;  ++x, d += xstride)
                    for (int c = 0;  c < channels;  ++c)
                        ((float *)d)[c] = powf (((float *)d)[c], m_gamma);
            }
        }
    }
private:
    float m_gamma;
};


// ColorProcessor that does nothing (identity transform)
class ColorProcessor_Ident : public ColorProcessor {
public:
    ColorProcessor_Ident () : ColorProcessor() { };
    ~ColorProcessor_Ident () { };
    virtual void apply (float *data, int width, int height, int channels,
                        stride_t chanstride, stride_t xstride,
                        stride_t ystride) const
    {
    }
};



ColorProcessorHandle
ColorConfig::createColorProcessor (string_view inputColorSpace,
                                   string_view outputColorSpace,
                                   string_view context_key,
                                   string_view context_value) const
{
    return createColorProcessor (ustring(inputColorSpace),
                                 ustring(outputColorSpace),
                                 ustring(context_key),
                                 ustring(context_value));
}



ColorProcessorHandle
ColorConfig::createColorProcessor (ustring inputColorSpace,
                                   ustring outputColorSpace,
                                   ustring context_key,
                                   ustring context_value) const
{
    ustring inputrole, outputrole;
    std::string pending_error;

    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey (inputColorSpace, outputColorSpace,
                               context_key, context_value);
    ColorProcessorHandle handle = getImpl()->findproc (prockey);
    if (handle)
        return handle;

#ifdef USE_OCIO
    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    OCIO::ConstProcessorRcPtr p;
    if (getImpl()->config_) {
        // If the names are roles, convert them to color space names
        string_view name;
        name = getColorSpaceNameByRole (inputColorSpace);
        if (! name.empty()) {
            inputrole = inputColorSpace;
            inputColorSpace = name;
        }
        name = getColorSpaceNameByRole (outputColorSpace);
        if (! name.empty()) {
            outputrole = outputColorSpace;
            outputColorSpace = name;
        }

        OCIO::ConstConfigRcPtr config = getImpl()->config_;
        OCIO::ConstContextRcPtr context = config->getCurrentContext();
        std::vector<string_view> keys, values;
        Strutil::split (context_key, keys, ",");
        Strutil::split (context_value, values, ",");
        if (keys.size() && values.size() && keys.size() == values.size()) {
            OCIO::ContextRcPtr ctx = context->createEditableCopy();
            for (size_t i = 0; i < keys.size(); ++i)
                ctx->setStringVar (keys[i].c_str(), values[i].c_str());
            context = ctx;
        }

        try {
            // Get the processor corresponding to this transform.
            p = getImpl()->config_->getProcessor(context, inputColorSpace.c_str(),
                                                 outputColorSpace.c_str());
            getImpl()->clear_error();
        }
        catch(OCIO::Exception &e) {
            // Don't quit yet, remember the error and see if any of our
            // built-in knowledge of some generic spaces will save us.
            p.reset();
            pending_error = e.what();
        }
        catch(...) {
            p.reset();
            getImpl()->error ("An unknown error occurred in OpenColorIO, getProcessor");
        }

        if (p && ! p->isNoOp()) {
            // If we got a valid processor that does something useful,
            // return it now. If it boils down to a no-op, give a second
            // chance below to recognize it as a special case.
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        }
    }
#endif

    if (! handle) {
        // Either not compiled with OCIO support, or no OCIO configuration
        // was found at all.  There are a few color conversions we know
        // about even in such dire conditions.
        using namespace Strutil;
        if (iequals(inputColorSpace,outputColorSpace)) {
            handle = ColorProcessorHandle(new ColorProcessor_Ident);
        }
        else if ((iequals(inputColorSpace,"linear") || iequals(inputrole,"linear") ||
             iequals(inputColorSpace,"lnf") || iequals(inputColorSpace,"lnh"))
            && iequals(outputColorSpace,"sRGB")) {
            handle = ColorProcessorHandle(new ColorProcessor_linear_to_sRGB);
        }
        else if (iequals(inputColorSpace,"sRGB") &&
            (iequals(outputColorSpace,"linear") || iequals(outputrole,"linear") ||
             iequals(outputColorSpace,"lnf") || iequals(outputColorSpace,"lnh"))) {
            handle = ColorProcessorHandle(new ColorProcessor_sRGB_to_linear);
        }
        else if ((iequals(inputColorSpace,"linear") || iequals(inputrole,"linear") ||
             iequals(inputColorSpace,"lnf") || iequals(inputColorSpace,"lnh")) &&
            iequals(outputColorSpace,"Rec709")) {
            handle = ColorProcessorHandle(new ColorProcessor_linear_to_Rec709);
        }
        else if (iequals(inputColorSpace,"Rec709") &&
            (iequals(outputColorSpace,"linear") || iequals(outputrole,"linear") ||
             iequals(outputColorSpace,"lnf") || iequals(outputColorSpace,"lnh"))) {
            handle = ColorProcessorHandle(new ColorProcessor_Rec709_to_linear);
        }
        else if ((iequals(inputColorSpace,"linear") || iequals(inputrole,"linear") ||
             iequals(inputColorSpace,"lnf") || iequals(inputColorSpace,"lnh")) &&
            istarts_with(outputColorSpace,"GammaCorrected")) {
            string_view gamstr = outputColorSpace;
            Strutil::parse_prefix (gamstr, "GammaCorrected");
            float g = from_string<float>(gamstr);
            handle = ColorProcessorHandle(new ColorProcessor_gamma(1.0f/g));
        }
        else if (istarts_with(inputColorSpace,"GammaCorrected") &&
            (iequals(outputColorSpace,"linear") || iequals(outputrole,"linear") ||
             iequals(outputColorSpace,"lnf") || iequals(outputColorSpace,"lnh"))) {
            string_view gamstr = inputColorSpace;
            Strutil::parse_prefix (gamstr, "GammaCorrected");
            float g = from_string<float>(gamstr);
            handle = ColorProcessorHandle(new ColorProcessor_gamma(g));
        }
    }

#ifdef USE_OCIO
    if (!handle && p) {
        // If we found a procesor from OCIO, even if it was a NoOp, and we
        // still don't have a better idea, return it.
        handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
    }
#endif

    if (pending_error.size())
        getImpl()->error (pending_error);

    return getImpl()->addproc (prockey, handle);
}



ColorProcessorHandle
ColorConfig::createLookTransform (string_view looks,
                                  string_view inputColorSpace,
                                  string_view outputColorSpace,
                                  bool inverse,
                                  string_view context_key,
                                  string_view context_value) const
{
    return createLookTransform (ustring(looks), ustring(inputColorSpace),
                                ustring(outputColorSpace), inverse,
                                ustring(context_key), ustring(context_value));
}



ColorProcessorHandle
ColorConfig::createLookTransform (ustring looks,
                                  ustring inputColorSpace,
                                  ustring outputColorSpace,
                                  bool inverse,
                                  ustring context_key,
                                  ustring context_value) const
{
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey (inputColorSpace, outputColorSpace,
                               context_key, context_value, looks,
                               ustring() /*display*/, ustring() /*view*/,
                               ustring() /*file*/, inverse);
    ColorProcessorHandle handle = getImpl()->findproc (prockey);
    if (handle)
        return handle;

#ifdef USE_OCIO
    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_) {
        OCIO::ConstConfigRcPtr config = getImpl()->config_;
        OCIO::LookTransformRcPtr transform = OCIO::LookTransform::Create();
        transform->setLooks (looks.c_str());
        OCIO::TransformDirection dir;
        if (inverse) {
            // The TRANSFORM_DIR_INVERSE applies an inverse for the
            // end-to-end transform, which would otherwise do dst->inv
            // look -> src.  This is an unintuitive result for the
            // artist (who would expect in, out to remain unchanged), so
            // we account for that here by flipping src/dst
            transform->setSrc (outputColorSpace.c_str());
            transform->setDst (inputColorSpace.c_str());
            dir = OCIO::TRANSFORM_DIR_INVERSE;
        } else { // forward
            transform->setSrc (inputColorSpace.c_str());
            transform->setDst (outputColorSpace.c_str());
            dir = OCIO::TRANSFORM_DIR_FORWARD;
        }
        OCIO::ConstContextRcPtr context = config->getCurrentContext();
        std::vector<string_view> keys, values;
        Strutil::split (context_key, keys, ",");
        Strutil::split (context_value, values, ",");
        if (keys.size() && values.size() && keys.size() == values.size()) {
            OCIO::ContextRcPtr ctx = context->createEditableCopy();
            for (size_t i = 0; i < keys.size(); ++i)
                ctx->setStringVar (keys[i].c_str(), values[i].c_str());
            context = ctx;
        }

        OCIO::ConstProcessorRcPtr p;
        try {
            // Get the processor corresponding to this transform.
            p = getImpl()->config_->getProcessor (context, transform, dir);
            getImpl()->clear_error();
            handle = ColorProcessorHandle (new ColorProcessor_OCIO(p));
        }
        catch(OCIO::Exception &e) {
            getImpl()->error (e.what());
        }
        catch(...) {
            getImpl()->error ("An unknown error occurred in OpenColorIO, getProcessor");
        }
    }
#endif

    return getImpl()->addproc (prockey, handle);
}



ColorProcessorHandle
ColorConfig::createDisplayTransform (string_view display,
                                     string_view view,
                                     string_view inputColorSpace,
                                     string_view looks,
                                     string_view context_key,
                                     string_view context_value) const
{
    return  createDisplayTransform (ustring(display), ustring(view),
                                    ustring(inputColorSpace), ustring(looks),
                                    ustring(context_key), ustring(context_value));
}



ColorProcessorHandle
ColorConfig::createDisplayTransform (ustring display,
                                     ustring view,
                                     ustring inputColorSpace,
                                     ustring looks,
                                     ustring context_key,
                                     ustring context_value) const
{
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey (inputColorSpace, ustring() /*outputColorSpace*/,
                               context_key, context_value, looks,
                               display, view);
    ColorProcessorHandle handle = getImpl()->findproc (prockey);
    if (handle)
        return handle;

#ifdef USE_OCIO
    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_) {
        OCIO::ConstConfigRcPtr config = getImpl()->config_;
        OCIO::DisplayTransformRcPtr transform = OCIO::DisplayTransform::Create();
        transform->setInputColorSpaceName (inputColorSpace.c_str());
        transform->setDisplay(display.c_str());
        transform->setView(view.c_str());
        if (looks.size()) {
            transform->setLooksOverride(looks.c_str());
            transform->setLooksOverrideEnabled(true);
        } else {
            transform->setLooksOverrideEnabled(false);
        }
        OCIO::ConstContextRcPtr context = config->getCurrentContext();
        std::vector<string_view> keys, values;
        Strutil::split (context_key, keys, ",");
        Strutil::split (context_value, values, ",");
        if (keys.size() && values.size() && keys.size() == values.size()) {
            OCIO::ContextRcPtr ctx = context->createEditableCopy();
            for (size_t i = 0; i < keys.size(); ++i)
                ctx->setStringVar (keys[i].c_str(), values[i].c_str());
            context = ctx;
        }

        OCIO::ConstProcessorRcPtr p;
        try {
            // Get the processor corresponding to this transform.
            p = getImpl()->config_->getProcessor (context, transform,
                                                  OCIO::TRANSFORM_DIR_FORWARD);
            getImpl()->clear_error();
            handle = ColorProcessorHandle (new ColorProcessor_OCIO(p));
        }
        catch(OCIO::Exception &e) {
            getImpl()->error (e.what());
        }
        catch(...) {
            getImpl()->error ("An unknown error occurred in OpenColorIO, getProcessor");
        }
    }
#endif

    return getImpl()->addproc (prockey, handle);
}



ColorProcessorHandle
ColorConfig::createFileTransform (string_view name, bool inverse) const
{
    return createFileTransform (ustring(name), inverse);
}



ColorProcessorHandle
ColorConfig::createFileTransform (ustring name, bool inverse) const
{
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey (ustring() /*inputColorSpace*/,
                               ustring() /*outputColorSpace*/,
                               ustring() /*context_key*/,
                               ustring() /*context_value*/,
                               ustring() /*looks*/,
                               ustring() /*display*/, ustring() /*view*/,
                               name, inverse);
    ColorProcessorHandle handle = getImpl()->findproc (prockey);
    if (handle)
        return handle;

#ifdef USE_OCIO
    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_) {
        OCIO::ConstConfigRcPtr config = getImpl()->config_;
        OCIO::FileTransformRcPtr transform = OCIO::FileTransform::Create();
        transform->setSrc (name.c_str());
        transform->setInterpolation (OCIO::INTERP_BEST);
        OCIO::TransformDirection dir = inverse ? OCIO::TRANSFORM_DIR_INVERSE
                                               : OCIO::TRANSFORM_DIR_FORWARD;
        OCIO::ConstContextRcPtr context = config->getCurrentContext();
        OCIO::ConstProcessorRcPtr p;
        try {
            // Get the processor corresponding to this transform.
            p = getImpl()->config_->getProcessor (context, transform, dir);
            getImpl()->clear_error();
            handle = ColorProcessorHandle (new ColorProcessor_OCIO(p));
        }
        catch(OCIO::Exception &e) {
            getImpl()->error (e.what());
        }
        catch(...) {
            getImpl()->error ("An unknown error occurred in OpenColorIO, getProcessor");
        }
    }
#endif

    return getImpl()->addproc (prockey, handle);
}



string_view
ColorConfig::parseColorSpaceFromString (string_view str) const
{
#ifdef USE_OCIO
    if (getImpl() && getImpl()->config_) {
        return getImpl()->config_->parseColorSpaceFromString (str.c_str());
    }
#endif
    return "";
}



//////////////////////////////////////////////////////////////////////////
//
// Image Processing Implementations


static std::shared_ptr<ColorConfig> default_colorconfig;  // default color config
static spin_mutex colorconfig_mutex;



bool
ImageBufAlgo::colorconvert (ImageBuf &dst, const ImageBuf &src,
                            string_view from, string_view to,
                            bool unpremult, string_view context_key,
                            string_view context_value,
                            ColorConfig *colorconfig,
                            ROI roi, int nthreads)
{
    if (from.empty() || from == "current") {
        from = src.spec().get_string_attribute ("oiio:Colorspace", "Linear");
    }
    if (from.empty() || to.empty()) {
        dst.error ("Unknown color space name");
        return false;
    }
    ColorProcessorHandle processor;
    {
        spin_lock lock (colorconfig_mutex);
        if (! colorconfig)
            colorconfig = default_colorconfig.get();
        if (! colorconfig)
            default_colorconfig.reset (colorconfig = new ColorConfig);
        processor = colorconfig->createColorProcessor (from, to,
                                                context_key, context_value);
        if (! processor) {
            if (colorconfig->error())
                dst.error ("%s", colorconfig->geterror());
            else
                dst.error ("Could not construct the color transform %s -> %s",
                           from, to);
            return false;
        }
    }
    bool ok = colorconvert (dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok)
        dst.specmod().attribute ("oiio:ColorSpace", to);
    return ok;
}



template<class Rtype, class Atype>
static bool
colorconvert_impl (ImageBuf &R, const ImageBuf &A,
                   const ColorProcessor* processor, bool unpremult,
                   ROI roi, int nthreads)
{
    using namespace ImageBufAlgo;
    using namespace simd;
    // Only process up to, and including, the first 4 channels.  This
    // does let us process images with fewer than 4 channels, which is
    // the intent.
    int channelsToCopy = std::min (4, roi.nchannels());
    if (channelsToCopy < 4)
        unpremult = false;
    parallel_image (roi, parallel_image_options(nthreads),
                    [&,unpremult,channelsToCopy,processor](ROI roi){
        int width = roi.width();
        // Temporary space to hold one RGBA scanline
        vfloat4 *scanline = OIIO_ALLOCA (vfloat4, width);
        vfloat4 *alpha = OIIO_ALLOCA (vfloat4, width);
        const float fltmin = std::numeric_limits<float>::min();
        ImageBuf::ConstIterator<Atype> a (A, roi);
        ImageBuf::Iterator<Rtype> r (R, roi);
        for (int k = roi.zbegin; k < roi.zend; ++k) {
            for (int j = roi.ybegin; j < roi.yend; ++j) {
                // Load the scanline
                a.rerange (roi.xbegin, roi.xend, j, j+1, k, k+1);
                for (int i = 0; !a.done(); ++a, ++i) {
                    vfloat4 v (0.0f);
                    for (int c = 0; c < channelsToCopy; ++c)
                        v[c] = a[c];
                    scanline[i] = v;
                }

                // Optionally unpremult
                if (unpremult) {
                    for (int i = 0; i < width; ++i) {
                        // float alpha = scanline[i][3];
                        vfloat4 a = shuffle<3>(scanline[i]);
                        a = select (a >= fltmin, a, vfloat4::One());
                        alpha[i] = a;
                        scanline[i] *= rcp_fast(a);
                    }
                }

                // Apply the color transformation in place
                processor->apply ((float *)&scanline[0], width, 1, 4,
                                  sizeof(float), 4*sizeof(float),
                                  width*4*sizeof(float));

                // Optionally re-premult
                if (unpremult) {
                    for (int i = 0; i < width; ++i)
                        scanline[i] *= alpha[i];
                }

                // Store the scanline
                float *dstPtr = (float *)&scanline[0];
                r.rerange (roi.xbegin, roi.xend, j, j+1, k, k+1);
                for ( ; !r.done(); ++r, dstPtr += 4)
                    for (int c = 0; c < channelsToCopy; ++c)
                        r[c] = dstPtr[c];
            }
        }
    });
    return true;
}



// Specialized version where both buffers are in memory (not cache based),
// float data, and we are dealing with 4 channels.
static bool
colorconvert_impl_float_rgba (ImageBuf &R, const ImageBuf &A,
                   const ColorProcessor* processor, bool unpremult,
                   ROI roi, int nthreads)
{
    using namespace ImageBufAlgo;
    using namespace simd;
    ASSERT (R.localpixels() && A.localpixels() &&
            R.spec().format == TypeFloat && A.spec().format == TypeFloat &&
            R.nchannels() == 4 && A.nchannels() == 4);
    parallel_image (roi, parallel_image_options(nthreads), [&](ROI roi){
        // int Rchans = R.nchannels();
        // int Achans = A.nchannels();
        int width = roi.width();
        // Temporary space to hold one RGBA scanline
        vfloat4 *scanline = OIIO_ALLOCA (vfloat4, width);
        vfloat4 *alpha = OIIO_ALLOCA (vfloat4, width);
        const float fltmin = std::numeric_limits<float>::min();
        for (int k = roi.zbegin; k < roi.zend; ++k) {
            for (int j = roi.ybegin; j < roi.yend; ++j) {
                // Load the scanline
                memcpy (scanline, A.pixeladdr (roi.xbegin, j, k), width*4*sizeof(float));
                // Optionally unpremult
                if (unpremult) {
                    for (int i = 0; i < width; ++i) {
                        vfloat4 p (scanline[i]);
                        vfloat4 a = shuffle<3>(p);
                        a = select (a >= fltmin, a, vfloat4::One());
                        alpha[i] = a;
                        scanline[i] = p * rcp_fast(a);
                    }
                }

                // Apply the color transformation in place
                processor->apply ((float *)&scanline[0], width, 1, 4,
                                  sizeof(float), 4*sizeof(float),
                                  width*4*sizeof(float));

                // Optionally premult
                if (unpremult) {
                    for (int i = 0; i < width; ++i)
                        scanline[i] *= alpha[i];
                }
                memcpy (R.pixeladdr (roi.xbegin, j, k), scanline, width*4*sizeof(float));
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::colorconvert (ImageBuf &dst, const ImageBuf &src,
                            const ColorProcessor* processor, bool unpremult,
                            ROI roi, int nthreads)
{
    // If the processor is NULL, return false (error)
    if (!processor) {
        dst.error ("Passed NULL ColorProcessor to colorconvert() [probable application bug]");
        return false;
    }

    // If the processor is a no-op and the conversion is being done
    // in place, no work needs to be done. Early exit.
    if (processor->isNoOp() && (&dst == &src))
        return true;

    if (! IBAprep (roi, &dst, &src))
        return false;

    // If the processor is a no-op (and it's not an in-place conversion),
    // use paste() to simplify the operation.
    if (processor->isNoOp()) {
        roi.chend = std::max (roi.chbegin+4, roi.chend);
        return ImageBufAlgo::paste (dst, roi.xbegin, roi.ybegin, roi.zbegin,
                                    roi.chbegin, src, roi, nthreads);
    }

    if (unpremult && src.spec().alpha_channel >= 0 &&
        src.spec().get_int_attribute("oiio:UnassociatedAlpha") != 0) {
        // If we appear to be operating on an image that already has
        // unassociated alpha, don't do a redundant unpremult step.
        unpremult = false;
    }

    if (dst.localpixels() && src.localpixels() &&
          dst.spec().format == TypeFloat && src.spec().format == TypeFloat &&
          dst.nchannels() == 4 && src.nchannels() == 4) {
        return colorconvert_impl_float_rgba (dst, src, processor,
                                             unpremult, roi, nthreads);
    }

    bool ok = true;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "colorconvert", colorconvert_impl,
                                 dst.spec().format, src.spec().format,
                                 dst, src, processor, unpremult, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::ociolook (ImageBuf &dst, const ImageBuf &src,
                        string_view looks, string_view from, string_view to,
                        bool inverse, bool unpremult,
                        string_view key, string_view value,
                        ColorConfig *colorconfig,
                        ROI roi, int nthreads)
{
    if (from.empty() || from == "current") {
        from = src.spec().get_string_attribute ("oiio:Colorspace", "Linear");
    }
    if (to.empty() || to == "current") {
        to = src.spec().get_string_attribute ("oiio:Colorspace", "Linear");
    }
    if (from.empty() || to.empty()) {
        dst.error ("Unknown color space name");
        return false;
    }
    ColorProcessorHandle processor;
    {
        spin_lock lock (colorconfig_mutex);
        if (! colorconfig)
            colorconfig = default_colorconfig.get();
        if (! colorconfig)
            default_colorconfig.reset (colorconfig = new ColorConfig);
        processor = colorconfig->createLookTransform (looks, from, to, inverse,
                                                 key, value);
        if (! processor) {
            if (colorconfig->error())
                dst.error ("%s", colorconfig->geterror());
            else
                dst.error ("Could not construct the color transform");
            return false;
        }
    }
    bool ok = colorconvert (dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok)
        dst.specmod().attribute ("oiio:ColorSpace", to);
    return ok;
}



bool
ImageBufAlgo::ociodisplay (ImageBuf &dst, const ImageBuf &src,
                           string_view display, string_view view,
                           string_view from, string_view looks,
                           bool unpremult,
                           string_view key, string_view value,
                           ColorConfig *colorconfig,
                           ROI roi, int nthreads)
{
    if (from.empty() || from == "current") {
        from = src.spec().get_string_attribute ("oiio:Colorspace", "Linear");
    }
    if (from.empty()) {
        dst.error ("Unknown color space name");
        return false;
    }
    ColorProcessorHandle processor;
    {
        spin_lock lock (colorconfig_mutex);
        if (! colorconfig)
            colorconfig = default_colorconfig.get();
        if (! colorconfig)
            default_colorconfig.reset (colorconfig = new ColorConfig);
        processor = colorconfig->createDisplayTransform (display, view, from, looks, key, value);
        if (! processor) {
            if (colorconfig->error())
                dst.error ("%s", colorconfig->geterror());
            else
                dst.error ("Could not construct the color transform");
            return false;
        }
    }
    bool ok = colorconvert (dst, src, processor.get(), unpremult, roi, nthreads);
    return ok;
}



bool
ImageBufAlgo::ociofiletransform (ImageBuf &dst, const ImageBuf &src,
                        string_view name, bool inverse, bool unpremult,
                        ColorConfig *colorconfig, ROI roi, int nthreads)
{
    if (name.empty()) {
        dst.error ("Unknown filetransform name");
        return false;
    }
    ColorProcessorHandle processor;
    {
        spin_lock lock (colorconfig_mutex);
        if (! colorconfig)
            colorconfig = default_colorconfig.get();
        if (! colorconfig)
            default_colorconfig.reset (colorconfig = new ColorConfig);
        processor = colorconfig->createFileTransform (name, inverse);
        if (! processor) {
            if (colorconfig->error())
                dst.error ("%s", colorconfig->geterror());
            else
                dst.error ("Could not construct the color transform");
            return false;
        }
    }
    bool ok = colorconvert (dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok)
        dst.specmod().attribute ("oiio:ColorSpace", name);
    return ok;
}



bool
ImageBufAlgo::colorconvert (float * color, int nchannels,
                            const ColorProcessor* processor, bool unpremult)
{
    // If the processor is NULL, return false (error)
    if (!processor) {
        return false;
    }

    // If the processor is a no-op, no work needs to be done. Early exit.
    if (processor->isNoOp())
        return true;
    
    // Load the pixel
    float rgba[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int channelsToCopy = std::min (4, nchannels);
    memcpy(rgba, color, channelsToCopy*sizeof (float));
    
    const float fltmin = std::numeric_limits<float>::min();
    
    // Optionally unpremult
    if ((channelsToCopy>=4) && unpremult) {
        float alpha = rgba[3];
        if (alpha > fltmin) {
            rgba[0] /= alpha;
            rgba[1] /= alpha;
            rgba[2] /= alpha;
        }
    }
    
    // Apply the color transformation
    processor->apply (rgba, 1, 1, 4,
                      sizeof(float), 4*sizeof(float), 4*sizeof(float));
    
    // Optionally premult
    if ((channelsToCopy>=4) && unpremult) {
        float alpha = rgba[3];
        if (alpha > fltmin) {
            rgba[0] *= alpha;
            rgba[1] *= alpha;
            rgba[2] *= alpha;
        }
    }
    
    // Store the scanline
    memcpy(color, rgba, channelsToCopy*sizeof(float));
    
    return true;
}



OIIO_NAMESPACE_END

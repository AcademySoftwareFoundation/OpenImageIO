// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <boost/container/flat_map.hpp>

#include <OpenImageIO/Imath.h>

#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

#include "imageio_pvt.h"

#define MAKE_OCIO_VERSION_HEX(maj, min, patch) \
    (((maj) << 24) | ((min) << 16) | (patch))

#ifdef USE_OCIO
#    include <OpenColorIO/OpenColorIO.h>
#    if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 0, 0)
#        define OCIO_v2 1
#    endif
#    if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 2, 0) \
        && !defined(OIIO_DISABLE_BUILTIN_OCIO_CONFIGS)
#        define OCIO_HAS_BUILTIN_CONFIGS 1
#    endif
namespace OCIO = OCIO_NAMESPACE;
#else
#    define OCIO_VERSION_HEX 0
#endif


OIIO_NAMESPACE_BEGIN

namespace {
// Some test colors we use to interrogate transformations
static const int n_test_colors = 5;
static const Imath::C3f test_colors[n_test_colors]
    = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1 }, { 0.5, 0.5, 0.5 } };
}  // namespace


#if 0 || !defined(NDEBUG) /* allow color configuration debugging */
static bool colordebug = Strutil::stoi(Sysutil::getenv("OIIO_COLOR_DEBUG"));
#    define DBG(...)    \
        if (colordebug) \
        Strutil::print(__VA_ARGS__)
#else
#    define DBG(...)
#endif


static int disable_ocio = Strutil::stoi(Sysutil::getenv("OIIO_DISABLE_OCIO"));
static int disable_builtin_configs = Strutil::stoi(
    Sysutil::getenv("OIIO_DISABLE_BUILTIN_OCIO_CONFIGS"));
#ifdef USE_OCIO
static OCIO::ConstConfigRcPtr ocio_current_config;
#endif



const ColorConfig&
ColorConfig::default_colorconfig()
{
    static ColorConfig config;
    return config;
}



// Class used as the key to index color processors in the cache.
class ColorProcCacheKey {
public:
    ColorProcCacheKey(ustring in, ustring out, ustring key = ustring(),
                      ustring val = ustring(), ustring looks = ustring(),
                      ustring display = ustring(), ustring view = ustring(),
                      ustring file = ustring(), bool inverse = false)
        : inputColorSpace(in)
        , outputColorSpace(out)
        , context_key(key)
        , context_value(val)
        , looks(looks)
        , file(file)
        , inverse(inverse)
    {
        hash = inputColorSpace.hash() + 14033ul * outputColorSpace.hash()
               + 823ul * context_key.hash() + 28411ul * context_value.hash()
               + 1741ul
                     * (looks.hash() + display.hash() + view.hash()
                        + file.hash())
               + (inverse ? 6421 : 0);
        // N.B. no separate multipliers for looks, display, view, file
        // because they're never used for the same lookup.
    }

    friend bool operator<(const ColorProcCacheKey& a,
                          const ColorProcCacheKey& b)
    {
        return std::tie(a.hash, a.inputColorSpace, a.outputColorSpace,
                        a.context_key, a.context_value, a.looks, a.display,
                        a.view, a.file, a.inverse)
               < std::tie(b.hash, b.inputColorSpace, b.outputColorSpace,
                          b.context_key, b.context_value, b.looks, b.display,
                          b.view, b.file, b.inverse);
    }

    ustring inputColorSpace;
    ustring outputColorSpace;
    ustring context_key;
    ustring context_value;
    ustring looks;
    ustring display;
    ustring view;
    ustring file;
    bool inverse;
    size_t hash;
};



typedef boost::container::flat_map<ColorProcCacheKey, ColorProcessorHandle>
    ColorProcessorMap;



bool
ColorConfig::supportsOpenColorIO()
{
#ifdef USE_OCIO
    return (disable_ocio == 0);
#else
    return false;
#endif
}



int
ColorConfig::OpenColorIO_version_hex()
{
    return OCIO_VERSION_HEX;
}


struct CSInfo {
    std::string name;  // Name of this color space
    int index;         // More than one can have the same index -- aliases
    enum Flags {
        none               = 0,
        is_linear_response = 1,   // any cs with linear transfer function
        is_scene_linear    = 2,   // equivalent to scene_linear
        is_srgb            = 4,   // sRGB (primaries, and transfer function)
        is_lin_srgb        = 8,   // sRGB/Rec709 primaries, linear response
        is_ACEScg          = 16,  // ACEScg
        is_Rec709          = 32,  // Rec709 primaries and transfer function
        is_known           = is_srgb | is_lin_srgb | is_ACEScg | is_Rec709
    };
    int m_flags   = 0;
    bool examined = false;
    std::string canonical;  // Canonical name for this color space
#ifdef USE_OCIO
    OCIO::ConstColorSpaceRcPtr ocio_cs;
#endif

    CSInfo(string_view name_, int index_, int flags_ = none,
           string_view canonical_ = "")
        : name(name_)
        , index(index_)
        , m_flags(flags_)
        , canonical(canonical_)
    {
    }

    void setflag(int flagval) { m_flags |= flagval; }

    // Set flag to include any bits in flagval, and also if alias is not yet
    // set, set it to name.
    void setflag(int flagval, std::string& alias)
    {
        m_flags |= flagval;
        if (alias.empty())
            alias = name;
    }

    int flags() const { return m_flags; }
};



// Hidden implementation of ColorConfig
class ColorConfig::Impl {
#ifdef USE_OCIO
public:
    OCIO::ConstConfigRcPtr config_;
    OCIO::ConstConfigRcPtr builtinconfig_;
#endif

private:
    std::vector<CSInfo> colorspaces;
    std::string scene_linear_alias;  // Alias for a scene-linear color space
    std::string lin_srgb_alias;
    std::string srgb_alias;
    std::string ACEScg_alias;
    std::string Rec709_alias;
    mutable spin_rw_mutex m_mutex;
    mutable std::string m_error;
    ColorProcessorMap colorprocmap;  // cache of ColorProcessors
    atomic_int colorprocs_requested;
    atomic_int colorprocs_created;
    std::string m_configname;
    ColorConfig* m_self       = nullptr;
    bool m_config_is_built_in = false;

public:
    Impl(ColorConfig* self)
        : m_self(self)
    {
    }

    ~Impl()
    {
#if 0
        // Debugging the cache -- make sure we're creating a small number
        // compared to repeated requests.
        if (colorprocs_requested)
            DBG("ColorConfig::Impl : color procs requested: {}, created: {}\n",
                           colorprocs_requested, colorprocs_created);
#endif
    }

    bool init(string_view filename);

    void add(const std::string& name, int index, int flags = 0)
    {
        spin_rw_write_lock lock(m_mutex);
        colorspaces.emplace_back(name, index, flags);
        // classify(colorspaces.back());
    }

    // Find the CSInfo record for the named color space, or nullptr if it's
    // not a color space we know.
    const CSInfo* find(string_view name) const
    {
        for (auto&& cs : colorspaces)
            if (cs.name == name)
                return &cs;
        return nullptr;
    }
    CSInfo* find(string_view name)
    {
        for (auto&& cs : colorspaces)
            if (cs.name == name)
                return &cs;
        return nullptr;
    }

    // Search for a matching ColorProcessor, return it if found (otherwise
    // return an empty handle).
    ColorProcessorHandle findproc(const ColorProcCacheKey& key)
    {
        ++colorprocs_requested;
        spin_rw_read_lock lock(m_mutex);
        auto found = colorprocmap.find(key);
        return (found == colorprocmap.end()) ? ColorProcessorHandle()
                                             : found->second;
    }

    // Add the given color processor. Be careful -- if a matching one is
    // already in the table, just return the existing one. If they pass
    // in an empty handle, just return it.
    ColorProcessorHandle addproc(const ColorProcCacheKey& key,
                                 ColorProcessorHandle handle)
    {
        if (!handle)
            return handle;
        spin_rw_write_lock lock(m_mutex);
        auto found = colorprocmap.find(key);
        if (found == colorprocmap.end()) {
            // No equivalent item in the map. Add this one.
            colorprocmap[key] = handle;
            ++colorprocs_created;
        } else {
            // There's already an equivalent one. Oops. Discard this one and
            // return the one already in the map.
            handle = found->second;
        }
        return handle;
    }

    int getNumColorSpaces() const { return (int)colorspaces.size(); }

    const char* getColorSpaceNameByIndex(int index) const
    {
        return colorspaces[index].name.c_str();
    }

    string_view resolve(string_view name) const;

    // Note: Uses std::format syntax
    template<typename... Args>
    void error(const char* fmt, const Args&... args) const
    {
        spin_rw_write_lock lock(m_mutex);
        m_error = Strutil::fmt::format(fmt, args...);
    }
    std::string geterror(bool clear = true) const
    {
        std::string err;
        spin_rw_write_lock lock(m_mutex);
        if (clear) {
            std::swap(err, m_error);
        } else {
            err = m_error;
        }
        return err;
    }
    bool haserror() const
    {
        spin_rw_read_lock lock(m_mutex);
        return !m_error.empty();
    }
    void clear_error()
    {
        spin_rw_write_lock lock(m_mutex);
        m_error.clear();
    }

    const std::string& configname() const { return m_configname; }
    void configname(string_view name) { m_configname = name; }

#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 2, 0)
    OCIO::ConstCPUProcessorRcPtr
    get_to_builtin_cpu_proc(const char* my_from, const char* builtin_to) const;
#endif
    bool isColorSpaceLinear(string_view name) const;

private:
    // Return the CSInfo flags for the given color space name
    int flags(string_view name)
    {
        CSInfo* cs = find(name);
        if (!cs)
            return 0;
        examine(cs);
        spin_rw_read_lock lock(m_mutex);
        return cs->flags();
    }

    // Set cs.flag to include any bits in flagval.
    void setflag(CSInfo& cs, int flagval)
    {
        spin_rw_write_lock lock(m_mutex);
        cs.setflag(flagval);
    }

    // Set cs.flag to include any bits in flagval, and also if alias is not
    // yet set, set it to cs.name.
    void setflag(CSInfo& cs, int flagval, std::string& alias)
    {
        spin_rw_write_lock lock(m_mutex);
        cs.setflag(flagval, alias);
    }

    void inventory();

    // Set the flags for the given color space and canonical name, if we can
    // make a guess based on the name. This is very inexpensive. This should
    // only be called from within a lock of the mutex.
    void classify_by_name(CSInfo& cs);

    // Set the flags for the given color space and canonical name, trying some
    // tricks to deduce the color space from the primaries, white point, and
    // transfer function. This is more expensive, and might only work for OCIO
    // 2.2 and above. This should only be called from within a lock of the
    // mutex.
    void classify_by_conversions(CSInfo& cs);

    // Apply more heuristics to try to deduce more color space information.
    void reclassify_heuristics(CSInfo& cs);

    // If the CSInfo hasn't yet been "examined" (fully classified by all
    // heuristics), do so. This should NOT be called from within a lock of the
    // mutex.
    void examine(CSInfo* cs)
    {
        if (!cs->examined) {
            spin_rw_write_lock lock(m_mutex);
            if (!cs->examined) {
                classify_by_name(*cs);
                classify_by_conversions(*cs);
                reclassify_heuristics(*cs);
                cs->examined = true;
            }
        }
    }

    void debug_print_aliases()
    {
        DBG("Aliases: scene_linear={}   lin_srgb={}   srgb={}   ACEScg={}   Rec709={}\n",
            scene_linear_alias, lin_srgb_alias, srgb_alias, ACEScg_alias,
            Rec709_alias);
    }

    // For OCIO 2.3+, we can ask for the equivalent of some built-in
    // color spaces.
    void identify_builtin_equivalents();

    bool check_same_as_builtin_transform(const char* my_from,
                                         const char* builtin_to) const;
    bool test_conversion_yields(const char* from, const char* to,
                                cspan<Imath::C3f> test_colors,
                                cspan<Imath::C3f> result_colors) const;
    const char* IdentifyBuiltinColorSpace(const char* name) const;
};



// ColorConfig utility to take inventory of the color spaces available.
// It sets up knowledge of "linear", "sRGB", "Rec709", etc,
// even if the underlying OCIO configuration lacks them.
void
ColorConfig::Impl::inventory()
{
    DBG("inventorying config {}\n", configname());
#ifdef USE_OCIO
    if (config_ && !disable_ocio) {
        bool nonraw = false;
        for (int i = 0, e = config_->getNumColorSpaces(); i < e; ++i)
            nonraw |= !Strutil::iequals(config_->getColorSpaceNameByIndex(i),
                                        "raw");
        if (nonraw) {
            for (int i = 0, e = config_->getNumColorSpaces(); i < e; ++i)
                add(config_->getColorSpaceNameByIndex(i), i);
            for (auto&& cs : colorspaces)
                classify_by_name(cs);
            OCIO::ConstColorSpaceRcPtr lin = config_->getColorSpace(
                "scene_linear");
            if (lin)
                scene_linear_alias = lin->getName();
            return;  // If any non-"raw" spaces were defined, we're done
        }
    }
    // If we had some kind of bogus configuration that seemed to define
    // only a "raw" color space and nothing else, that's useless, so
    // figure out our own way to move forward.
    config_.reset();
#endif

    // If there was no configuration, or we didn't compile with OCIO
    // support at all, register a few basic names we know about.
    // For the "no OCIO / no config" case, we assume an unsophisticated
    // color pipeline where "linear" and the like are all assumed to use
    // Rec709/sRGB color primaries.
    int linflags = CSInfo::is_linear_response | CSInfo::is_scene_linear
                   | CSInfo::is_lin_srgb;
    add("linear", 0, linflags);
    add("scene_linear", 0, linflags);
    add("default", 0, linflags);
    add("rgb", 0, linflags);
    add("RGB", 0, linflags);
    add("lin_srgb", 0, linflags);
    add("sRGB", 1, CSInfo::is_srgb);
    add("Rec709", 2, CSInfo::is_Rec709);

    for (auto&& cs : colorspaces)
        classify_by_name(cs);
}



inline bool
close_colors(cspan<Imath::C3f> a, cspan<Imath::C3f> b)
{
    OIIO_DASSERT(a.size() == b.size());
    for (size_t i = 0, e = a.size(); i < e; ++i)
        if (std::abs(a[i].x - b[i].x) > 1.0e-3f
            || std::abs(a[i].y - b[i].y) > 1.0e-3f
            || std::abs(a[i].z - b[i].z) > 1.0e-3f)
            return false;
    return true;
}



#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 2, 0)

OCIO::ConstCPUProcessorRcPtr
ColorConfig::Impl::get_to_builtin_cpu_proc(const char* my_from,
                                           const char* builtin_to) const
{
    try {
        auto proc = OCIO::Config::GetProcessorToBuiltinColorSpace(config_,
                                                                  my_from,
                                                                  builtin_to);
        return proc ? proc->getDefaultCPUProcessor()
                    : OCIO::ConstCPUProcessorRcPtr();
    } catch (...) {
        return {};
    }
}

#endif



// Is this config's `my_from` color space equivalent to the built-in
// `builtin_to` color space? Find out by transforming the primaries, white,
// and half white and see if the results indicate that it was the identity
// transform (or close enough).
bool
ColorConfig::Impl::check_same_as_builtin_transform(const char* my_from,
                                                   const char* builtin_to) const
{
    if (disable_builtin_configs)
        return false;
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 2, 0)
    auto proc = get_to_builtin_cpu_proc(my_from, builtin_to);
    if (proc) {
        Imath::C3f colors[n_test_colors];
        std::copy(test_colors, test_colors + n_test_colors, colors);
        proc->apply(OCIO::PackedImageDesc(colors, n_test_colors, 1, 3));
        if (close_colors(colors, test_colors))
            return true;
    }
#endif
    return false;
}



// If we transform test_colors from "from" to "to" space, do we get
// result_colors? This is a building block for deducing some color spaces.
bool
ColorConfig::Impl::test_conversion_yields(const char* from, const char* to,
                                          cspan<Imath::C3f> test_colors,
                                          cspan<Imath::C3f> result_colors) const
{
    auto proc = m_self->createColorProcessor(from, to);
    if (!proc)
        return false;
    OIIO_DASSERT(test_colors.size() == result_colors.size());
    int n              = std::ssize(test_colors);
    Imath::C3f* colors = OIIO_ALLOCA(Imath::C3f, n);
    std::copy(test_colors.data(), test_colors.data() + n, colors);
    proc->apply((float*)colors, n, 1, 3, sizeof(float), 3 * sizeof(float),
                n * 3 * sizeof(float));
    return close_colors({ colors, n }, result_colors);
}



#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 2, 0)
static bool
transform_has_Lut3D(string_view name, OCIO::ConstTransformRcPtr transform)
{
    using namespace OCIO;
    auto ttype = transform ? transform->getTransformType() : -1;
    if (ttype == TRANSFORM_TYPE_LUT3D || ttype == TRANSFORM_TYPE_COLORSPACE
        || ttype == TRANSFORM_TYPE_FILE || ttype == TRANSFORM_TYPE_LOOK
        || ttype == TRANSFORM_TYPE_DISPLAY_VIEW) {
        return true;
    }
    if (ttype == TRANSFORM_TYPE_GROUP) {
        auto group = dynamic_cast<const GroupTransform*>(transform.get());
        for (int i = 0, n = group->getNumTransforms(); i < n; ++i) {
            if (transform_has_Lut3D("", group->getTransform(i)))
                return true;
        }
    }
    if (name.size() && ttype >= 0)
        DBG("{} has type {}\n", name, ttype);
    return false;
}

#endif



void
ColorConfig::Impl::classify_by_name(CSInfo& cs)
{
    // General heuristics based on the names -- for a few canonical names,
    // believe them! Woe be unto the poor soul who names a color space "sRGB"
    // or "ACEScg" and it's really something entirely different.
    if (Strutil::iequals(cs.name, "sRGB")
        || Strutil::iequals(cs.name, "srgb_tx")
        || Strutil::iequals(cs.name, "srgb_texture")
        || Strutil::iequals(cs.name, "srgb texture")
        || Strutil::iequals(cs.name, "sRGB - Texture")) {
        cs.setflag(CSInfo::is_srgb, srgb_alias);
    } else if (Strutil::iequals(cs.name, "Rec709")) {
        cs.setflag(CSInfo::is_Rec709, Rec709_alias);
    } else if (Strutil::iequals(cs.name, "lin_srgb")
               || Strutil::iequals(cs.name, "lin_rec709")
               || Strutil::iequals(cs.name, "Linear Rec.709 (sRGB)")) {
        cs.setflag(CSInfo::is_lin_srgb | CSInfo::is_linear_response,
                   lin_srgb_alias);
    } else if (Strutil::iequals(cs.name, "ACEScg")
               || Strutil::iequals(cs.name, "lin_ap1")) {
        cs.setflag(CSInfo::is_ACEScg | CSInfo::is_linear_response,
                   ACEScg_alias);
    }
#ifdef OIIO_SITE_spi
    // Ugly SPI-specific hacks, so sorry
    else if (Strutil::starts_with(cs.name, "cgln")) {
        cs.setflag(CSInfo::is_ACEScg | CSInfo::is_linear_response,
                   ACEScg_alias);
    } else if (cs.name == "srgbf" || cs.name == "srgbh" || cs.name == "srgb16"
               || cs.name == "srgb8") {
        cs.setflag(CSInfo::is_srgb, srgb_alias);
    } else if (cs.name == "srgblnf" || cs.name == "srgblnh"
               || cs.name == "srgbln16" || cs.name == "srgbln8") {
        cs.setflag(CSInfo::is_lin_srgb, lin_srgb_alias);
    }
#endif

    // Set up some canonical names
    if (cs.flags() & CSInfo::is_srgb)
        cs.canonical = "sRGB";
    else if (cs.flags() & CSInfo::is_Rec709)
        cs.canonical = "Rec709";
    else if (cs.flags() & CSInfo::is_lin_srgb)
        cs.canonical = "lin_srgb";
    else if (cs.flags() & CSInfo::is_ACEScg)
        cs.canonical = "ACEScg";
    if (cs.canonical.size()) {
        DBG("classify by name identified '{}' as canonical {}\n", cs.name,
            cs.canonical);
        cs.examined = true;
    }
}



void
ColorConfig::Impl::classify_by_conversions(CSInfo& cs)
{
    DBG("classifying by conversions {}\n", cs.name);
    if (cs.examined)
        return;  // Already classified

    if (isColorSpaceLinear(cs.name))
        cs.setflag(CSInfo::is_linear_response);

#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 2, 0)
    // If the name didn't already tell us what it is, and we have a new enough
    // OCIO that has built-in configs, test whether this color space is
    // equivalent to one of a few particular built-in color spaces. That lets
    // us identify some color spaces even if they are named something
    // nonstandard. Skip this part if the color space we're classifying is
    // itself part of the built-in config -- in that case, it will already be
    // tagged correctly by the name above.
    if (!(cs.flags() & CSInfo::is_known) && config_ && !disable_ocio
        && !m_config_is_built_in) {
        using namespace OCIO;
        cs.ocio_cs = config_->getColorSpace(cs.name.c_str());
        if (transform_has_Lut3D(cs.name, cs.ocio_cs->getTransform(
                                             COLORSPACE_DIR_TO_REFERENCE))
            || transform_has_Lut3D(cs.name,
                                   cs.ocio_cs->getTransform(
                                       COLORSPACE_DIR_FROM_REFERENCE))) {
            // Skip things with LUT3d because they are expensive due to LUT
            // inversion costs, and they're not gonna be our favourite
            // canonical spaces anyway.
            // DBG("{} has LUT3\n", cs.name);
        } else if (check_same_as_builtin_transform(cs.name.c_str(), "srgb_tx")) {
            cs.setflag(CSInfo::is_srgb, srgb_alias);
        } else if (check_same_as_builtin_transform(cs.name.c_str(),
                                                   "lin_srgb")) {
            cs.setflag(CSInfo::is_lin_srgb | CSInfo::is_linear_response,
                       lin_srgb_alias);
        } else if (check_same_as_builtin_transform(cs.name.c_str(), "ACEScg")) {
            cs.setflag(CSInfo::is_ACEScg | CSInfo::is_linear_response,
                       ACEScg_alias);
        }
    }
#endif

    // Set up some canonical names
    if (cs.flags() & CSInfo::is_srgb)
        cs.canonical = "sRGB";
    else if (cs.flags() & CSInfo::is_Rec709)
        cs.canonical = "Rec709";
    else if (cs.flags() & CSInfo::is_lin_srgb)
        cs.canonical = "lin_srgb";
    else if (cs.flags() & CSInfo::is_ACEScg)
        cs.canonical = "ACEScg";
}



void
ColorConfig::Impl::reclassify_heuristics(CSInfo& cs)
{
#if OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 2, 0)
    // Extra checks for OCIO < 2.2. For >= 2.2, there is no need, we
    // already figured this out using the built-in configs.
    if (!(cs.flags() & CSInfo::is_known)) {
        // If this isn't one of the known color spaces, let's try some
        // tricks!
        static float srgb05 = linear_to_sRGB(0.5f);
        static Imath::C3f lin_srgb_to_srgb_results[n_test_colors]
            = { { 1, 0, 0 },
                { 0, 1, 0 },
                { 0, 0, 1 },
                { 1, 1, 1 },
                { srgb05, srgb05, srgb05 } };
        // If there is a known srgb space, and transforming our test
        // colors from "this cs" to srgb gives us what we expect for a
        // lin_srgb->srgb, then guess what? -- this is lin_srgb!
        if (srgb_alias.size()
            && test_conversion_yields(cs.name.c_str(), srgb_alias.c_str(),
                                      test_colors, lin_srgb_to_srgb_results)) {
            setflag(cs, CSInfo::is_lin_srgb | CSInfo::is_linear_response,
                    lin_srgb_alias);
            cs.canonical = "lin_srgb";
        }
    }
#endif
}



void
ColorConfig::Impl::identify_builtin_equivalents()
{
    if (disable_builtin_configs)
        return;
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 3, 0)
    Timer timer;
    if (auto n = IdentifyBuiltinColorSpace("srgb_tx")) {
        if (CSInfo* cs = find(n)) {
            cs->setflag(CSInfo::is_srgb, srgb_alias);
            DBG("Identified {} = builtin '{}'\n", "srgb", cs->name);
        }
    } else {
        DBG("No config space identified as srgb\n");
    }
    DBG("identify_builtin_equivalents srgb took {:0.2f}s\n", timer.lap());
    if (auto n = IdentifyBuiltinColorSpace("lin_srgb")) {
        if (CSInfo* cs = find(n)) {
            cs->setflag(CSInfo::is_lin_srgb | CSInfo::is_linear_response,
                        lin_srgb_alias);
            DBG("Identified {} = builtin '{}'\n", "lin_srgb", cs->name);
        }
    } else {
        DBG("No config space identified as lin_srgb\n");
    }
    DBG("identify_builtin_equivalents lin_srgb took {:0.2f}s\n", timer.lap());
    if (auto n = IdentifyBuiltinColorSpace("ACEScg")) {
        if (CSInfo* cs = find(n)) {
            cs->setflag(CSInfo::is_ACEScg | CSInfo::is_linear_response,
                        ACEScg_alias);
            DBG("Identified {} = builtin '{}'\n", "ACEScg", cs->name);
        }
    } else {
        DBG("No config space identified as acescg\n");
    }
    DBG("identify_builtin_equivalents acescg took {:0.2f}s\n", timer.lap());
#endif
}



const char*
ColorConfig::Impl::IdentifyBuiltinColorSpace(const char* name) const
{
#ifdef USE_OCIO
    if (!config_ || disable_builtin_configs)
        return nullptr;
#endif
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 3, 0)
    try {
        return OCIO::Config::IdentifyBuiltinColorSpace(config_, builtinconfig_,
                                                       name);
    } catch (...) {
    }
#endif
    return nullptr;
}



ColorConfig::ColorConfig(string_view filename) { reset(filename); }



ColorConfig::~ColorConfig() {}



bool
ColorConfig::Impl::init(string_view filename)
{
    OIIO_MAYBE_UNUSED Timer timer;
    bool ok = true;

#ifdef USE_OCIO
    auto oldlog = OCIO::GetLoggingLevel();
    OCIO::SetLoggingLevel(OCIO::LOGGING_LEVEL_NONE);

#    if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 2, 0)
    try {
        builtinconfig_ = OCIO::Config::CreateFromFile("ocio://default");
    } catch (OCIO::Exception& e) {
        error("Error making OCIO built-in config: {}", e.what());
    }
#    endif

    // If no filename was specified, use env $OCIO
    if (filename.empty())
        filename = Sysutil::getenv("OCIO");
#    ifdef OCIO_HAS_BUILTIN_CONFIGS
    if (filename.empty() && !disable_builtin_configs)
        filename = "ocio://default";
#    endif
    if (filename.size() && !Filesystem::exists(filename)
#    ifdef OCIO_HAS_BUILTIN_CONFIGS
        && !Strutil::istarts_with(filename, "ocio://")
#    endif
    ) {
        error("Requested non-existent OCIO config \"{}\"", filename);
    } else {
        // Either filename passed, or taken from $OCIO, and it seems to exist
        try {
            config_ = OCIO::Config::CreateFromFile(
                std::string(filename).c_str());
            configname(filename);
            m_config_is_built_in = Strutil::istarts_with(filename, "ocio://");
        } catch (OCIO::Exception& e) {
            error("Error reading OCIO config \"{}\": {}", filename, e.what());
        } catch (...) {
            error("Error reading OCIO config \"{}\"", filename);
        }
    }
    OCIO::SetLoggingLevel(oldlog);

    ok = config_.get() != nullptr;

    DBG("OCIO config {} loaded in {:0.2f} seconds\n", filename, timer.lap());
#endif

    inventory();
    // NOTE: inventory already does classify_by_name

#if OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 2, 0)
    // Prior to 2.2, there are some other heuristics we use
    for (auto&& cs : colorspaces)
        reclassify_heuristics(cs);
#endif
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 3, 0)
    DBG("\nIDENTIFY BUILTIN EQUIVALENTS\n");
    identify_builtin_equivalents();  // OCIO 2.3+ only
    DBG("OCIO 2.3+ builtin equivalents in {:0.2f} seconds\n", timer.lap());
#endif

#if 1
    for (auto&& cs : colorspaces) {
        // examine(&cs);
        DBG("Color space '{}':\n", cs.name);
        if (cs.flags() & CSInfo::is_srgb)
            DBG("'{}' is srgb\n", cs.name);
        if (cs.flags() & CSInfo::is_lin_srgb)
            DBG("'{}' is lin_srgb\n", cs.name);
        if (cs.flags() & CSInfo::is_ACEScg)
            DBG("'{}' is ACEScg\n", cs.name);
        if (cs.flags() & CSInfo::is_Rec709)
            DBG("'{}' is Rec709\n", cs.name);
        if (cs.flags() & CSInfo::is_linear_response)
            DBG("'{}' has linear response\n", cs.name);
        if (cs.flags() & CSInfo::is_scene_linear)
            DBG("'{}' is scene_linear\n", cs.name);
        if (cs.flags())
            DBG("\n");
    }
#endif
    debug_print_aliases();
    DBG("OCIO config {} classified in {:0.2f} seconds\n", filename,
        timer.lap());

    return ok;
}



bool
ColorConfig::reset(string_view filename)
{
    pvt::LoggedTimer logtime("ColorConfig::reset");
    if (m_impl
        && (filename == getImpl()->configname()
            || (filename == ""
                && getImpl()->configname() == "ocio://default"))) {
        // Request to reset to the config we're already using. Just return,
        // don't do anything expensive.
        return true;
    }

    m_impl.reset(new ColorConfig::Impl(this));
    return m_impl->init(filename);
}



bool
ColorConfig::has_error() const
{
    return (getImpl()->haserror());
}



bool
ColorConfig::error() const
{
    return has_error();
}



std::string
ColorConfig::geterror(bool clear) const
{
    return getImpl()->geterror(clear);
}



int
ColorConfig::getNumColorSpaces() const
{
    return (int)getImpl()->getNumColorSpaces();
}



const char*
ColorConfig::getColorSpaceNameByIndex(int index) const
{
    return getImpl()->getColorSpaceNameByIndex(index);
}



int
ColorConfig::getColorSpaceIndex(string_view name) const
{
    // Check for exact matches
    for (int i = 0, e = getNumColorSpaces(); i < e; ++i)
        if (Strutil::iequals(getColorSpaceNameByIndex(i), name))
            return i;
    // Check for aliases and equivalents
    for (int i = 0, e = getNumColorSpaces(); i < e; ++i)
        if (equivalent(getColorSpaceNameByIndex(i), name))
            return i;
    return -1;
}



const char*
ColorConfig::getColorSpaceFamilyByName(string_view name) const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio) {
        OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace(
            std::string(name).c_str());
        if (c)
            return c->getFamily();
    }
#endif
    return NULL;
}



std::vector<std::string>
ColorConfig::getColorSpaceNames() const
{
    std::vector<std::string> result;
    int n = getNumColorSpaces();
    result.reserve(n);
    for (int i = 0; i < n; ++i)
        result.emplace_back(getColorSpaceNameByIndex(i));
    return result;
}

int
ColorConfig::getNumRoles() const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getNumRoles();
#endif
    return 0;
}

const char*
ColorConfig::getRoleByIndex(int index) const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getRoleName(index);
#endif
    return nullptr;
}


std::vector<std::string>
ColorConfig::getRoles() const
{
    std::vector<std::string> result;
    for (int i = 0, e = getNumRoles(); i != e; ++i)
        result.emplace_back(getRoleByIndex(i));
    return result;
}



int
ColorConfig::getNumLooks() const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getNumLooks();
#endif
    return 0;
}



const char*
ColorConfig::getLookNameByIndex(int index) const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getLookNameByIndex(index);
#endif
    return nullptr;
}



std::vector<std::string>
ColorConfig::getLookNames() const
{
    std::vector<std::string> result;
    for (int i = 0, e = getNumLooks(); i != e; ++i)
        result.emplace_back(getLookNameByIndex(i));
    return result;
}



bool
ColorConfig::isColorSpaceLinear(string_view name) const
{
    return getImpl()->isColorSpaceLinear(name);
}



bool
ColorConfig::Impl::isColorSpaceLinear(string_view name) const
{
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 2, 0)
    if (config_ && !disable_builtin_configs && !disable_ocio) {
        return config_->isColorSpaceLinear(c_str(name),
                                           OCIO::REFERENCE_SPACE_SCENE)
               || config_->isColorSpaceLinear(c_str(name),
                                              OCIO::REFERENCE_SPACE_DISPLAY);
    }
#endif
    return Strutil::iequals(name, "linear")
           || Strutil::istarts_with(name, "linear ")
           || Strutil::istarts_with(name, "linear_")
           || Strutil::istarts_with(name, "lin_")
           || Strutil::iends_with(name, "_linear")
           || Strutil::iends_with(name, "_lin");
}



std::vector<std::string>
ColorConfig::getAliases(string_view color_space) const
{
    std::vector<std::string> result;
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 0, 0)
    auto config = getImpl()->config_;
    if (config) {
        auto cs = config->getColorSpace(c_str(color_space));
        if (cs) {
            for (int i = 0, e = cs->getNumAliases(); i < e; ++i)
                result.emplace_back(cs->getAlias(i));
        }
    }
#endif
    return result;
}



const char*
ColorConfig::getColorSpaceNameByRole(string_view role) const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio) {
        using Strutil::print;
        OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace(
            std::string(role).c_str());
        // DBG("looking first for named color space {} -> {}\n", role,
        //     c ? c->getName() : "not found");
        // Catch special case of obvious name synonyms
        if (!c
            && (Strutil::iequals(role, "RGB")
                || Strutil::iequals(role, "default")))
            role = string_view("linear");
        if (!c && Strutil::iequals(role, "linear"))
            c = getImpl()->config_->getColorSpace("scene_linear");
        if (!c && Strutil::iequals(role, "scene_linear"))
            c = getImpl()->config_->getColorSpace("linear");
#    ifdef OCIO_HAS_BUILTIN_CONFIGS
        if (!c && Strutil::iequals(role, "srgb")) {
            c = getImpl()->config_->getColorSpace("sRGB - Texture");
            // DBG("Unilaterally substituting {} -> '{}'\n", role,
            //                c->getName());
        }
#    endif

        if (c) {
            // DBG("found color space {} for role {}\n", c->getName(),
            //                role);
            return c->getName();
        }
    }
#endif

    // No OCIO at build time, or no OCIO configuration at run time
    if (Strutil::iequals(role, "linear")
        || Strutil::iequals(role, "scene_linear"))
        return "linear";

    return NULL;  // Dunno what role
}



TypeDesc
ColorConfig::getColorSpaceDataType(string_view name, int* bits) const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio) {
        OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace(
            std::string(name).c_str());
        if (c) {
            OCIO::BitDepth b = c->getBitDepth();
            switch (b) {
            case OCIO::BIT_DEPTH_UNKNOWN: return TypeDesc::UNKNOWN;
            case OCIO::BIT_DEPTH_UINT8: *bits = 8; return TypeDesc::UINT8;
            case OCIO::BIT_DEPTH_UINT10: *bits = 10; return TypeDesc::UINT16;
            case OCIO::BIT_DEPTH_UINT12: *bits = 12; return TypeDesc::UINT16;
            case OCIO::BIT_DEPTH_UINT14: *bits = 14; return TypeDesc::UINT16;
            case OCIO::BIT_DEPTH_UINT16: *bits = 16; return TypeDesc::UINT16;
            case OCIO::BIT_DEPTH_UINT32: *bits = 32; return TypeDesc::UINT32;
            case OCIO::BIT_DEPTH_F16: *bits = 16; return TypeDesc::HALF;
            case OCIO::BIT_DEPTH_F32: *bits = 32; return TypeDesc::FLOAT;
            }
        }
    }
#endif
    return TypeDesc::UNKNOWN;
}



int
ColorConfig::getNumDisplays() const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getNumDisplays();
#endif
    return 0;
}



const char*
ColorConfig::getDisplayNameByIndex(int index) const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getDisplay(index);
#endif
    return nullptr;
}



std::vector<std::string>
ColorConfig::getDisplayNames() const
{
    std::vector<std::string> result;
    for (int i = 0, e = getNumDisplays(); i != e; ++i)
        result.emplace_back(getDisplayNameByIndex(i));
    return result;
}



int
ColorConfig::getNumViews(string_view display) const
{
#ifdef USE_OCIO
    if (display.empty())
        display = getDefaultDisplayName();
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getNumViews(std::string(display).c_str());
#endif
    return 0;
}



const char*
ColorConfig::getViewNameByIndex(string_view display, int index) const
{
#ifdef USE_OCIO
    if (display.empty())
        display = getDefaultDisplayName();
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getView(std::string(display).c_str(), index);
#endif
    return nullptr;
}



std::vector<std::string>
ColorConfig::getViewNames(string_view display) const
{
    std::vector<std::string> result;
    if (display.empty())
        display = getDefaultDisplayName();
    for (int i = 0, e = getNumViews(display); i != e; ++i)
        result.emplace_back(getViewNameByIndex(display, i));
    return result;
}



const char*
ColorConfig::getDefaultDisplayName() const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getDefaultDisplay();
#endif
    return nullptr;
}



const char*
ColorConfig::getDefaultViewName(string_view display) const
{
#ifdef USE_OCIO
    if (display.empty() || display == "default")
        display = getDefaultDisplayName();
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getDefaultView(c_str(display));
#endif
    return nullptr;
}



const char*
ColorConfig::getDisplayViewColorSpaceName(const std::string& display,
                                          const std::string& view) const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio)
#    if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 0, 0)
        return getImpl()->config_->getDisplayViewColorSpaceName(display.c_str(),
                                                                view.c_str());
#    else
        return getImpl()->config_->getDisplayColorSpaceName(display.c_str(),
                                                            view.c_str());
#    endif
#endif
    return nullptr;
}



const char*
ColorConfig::getDisplayViewLooks(const std::string& display,
                                 const std::string& view) const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio)
#    if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 0, 0)
        return getImpl()->config_->getDisplayViewLooks(display.c_str(),
                                                       view.c_str());
#    else
        return getImpl()->config_->getDisplayLooks(display.c_str(),
                                                   view.c_str());
#    endif
#endif
    return nullptr;
}



std::string
ColorConfig::configname() const
{
#ifdef USE_OCIO
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->configname();
#endif
    return "built-in";
}



string_view
ColorConfig::resolve(string_view name) const
{
    return getImpl()->resolve(name);
}



string_view
ColorConfig::Impl::resolve(string_view name) const
{
#ifdef USE_OCIO
    OCIO::ConstConfigRcPtr config = config_;
    if (config && !disable_ocio) {
        const char* namestr           = c_str(name);
        OCIO::ConstColorSpaceRcPtr cs = config->getColorSpace(namestr);
        if (cs)
            return cs->getName();
    }
    // OCIO did not know this name as a color space, role, or alias.
#endif
    // Maybe it's an informal alias of common names?
    spin_rw_write_lock lock(m_mutex);
    if (Strutil::iequals(name, "sRGB") && !srgb_alias.empty())
        return srgb_alias;
    if (Strutil::iequals(name, "lin_srgb") && lin_srgb_alias.size())
        return lin_srgb_alias;
    if (Strutil::iequals(name, "ACEScg") && !ACEScg_alias.empty())
        return ACEScg_alias;
    if ((Strutil::iequals(name, "linear")
         || Strutil::iequals(name, "scene_linear"))
        && !scene_linear_alias.empty()) {
        return scene_linear_alias;
    }
    if (Strutil::iequals(name, "Rec709") && Rec709_alias.size())
        return Rec709_alias;

    return name;
}



bool
ColorConfig::equivalent(string_view color_space1,
                        string_view color_space2) const
{
    // Empty color spaces never match
    if (color_space1.empty() || color_space2.empty())
        return false;
    // Easy case: matching names are the same!
    if (Strutil::iequals(color_space1, color_space2))
        return true;

    // If "resolved" names (after converting aliases and roles to color
    // spaces) match, they are equivalent.
    color_space1 = resolve(color_space1);
    color_space2 = resolve(color_space2);
    if (color_space1.empty() || color_space2.empty())
        return false;
    if (Strutil::iequals(color_space1, color_space2))
        return true;

    // If the color spaces' flags (when masking only the bits that refer to
    // specific known color spaces) match, consider them equivalent.
    const int mask = CSInfo::is_srgb | CSInfo::is_lin_srgb | CSInfo::is_ACEScg
                     | CSInfo::is_Rec709;
    const CSInfo* csi1 = getImpl()->find(color_space1);
    const CSInfo* csi2 = getImpl()->find(color_space2);
    if (csi1 && csi2) {
        int flags1 = csi1->flags() & mask;
        int flags2 = csi2->flags() & mask;
        if ((flags1 | flags2) && csi1->flags() == csi2->flags())
            return true;
        if ((csi1->canonical.size() && csi2->canonical.size())
            && Strutil::iequals(csi1->canonical, csi2->canonical))
            return true;
    }

    return false;
}



#ifdef USE_OCIO

#    if OCIO_VERSION_HEX >= 0x02000000
inline OCIO::BitDepth
ocio_bitdepth(TypeDesc type)
{
    if (type == TypeDesc::UINT8)
        return OCIO::BIT_DEPTH_UINT8;
    if (type == TypeDesc::UINT16)
        return OCIO::BIT_DEPTH_UINT16;
    if (type == TypeDesc::UINT32)
        return OCIO::BIT_DEPTH_UINT32;
    // N.B.: OCIOv2 also supports 10, 12, and 14 bit int, but we won't
    // ever have data in that format at this stage.
    if (type == TypeDesc::HALF)
        return OCIO::BIT_DEPTH_F16;
    if (type == TypeDesc::FLOAT)
        return OCIO::BIT_DEPTH_F32;
    return OCIO::BIT_DEPTH_UNKNOWN;
}
#    endif


// Custom ColorProcessor that wraps an OpenColorIO Processor.
class ColorProcessor_OCIO final : public ColorProcessor {
public:
    ColorProcessor_OCIO(OCIO::ConstProcessorRcPtr p)
        : m_p(p)
#    if OCIO_VERSION_HEX >= 0x02000000
        , m_cpuproc(p->getDefaultCPUProcessor())
#    endif
    {
    }
    ~ColorProcessor_OCIO() override {}

    bool isNoOp() const override { return m_p->isNoOp(); }
    bool hasChannelCrosstalk() const override
    {
        return m_p->hasChannelCrosstalk();
    }
    void apply(float* data, int width, int height, int channels,
               stride_t chanstride, stride_t xstride,
               stride_t ystride) const override
    {
#    if OCIO_VERSION_HEX >= 0x02000000
        OCIO::PackedImageDesc pid(data, width, height, channels,
                                  OCIO::BIT_DEPTH_F32,  // For now, only float
                                  chanstride, xstride, ystride);
        m_cpuproc->apply(pid);
#    else
        OCIO::PackedImageDesc pid(data, width, height, channels, chanstride,
                                  xstride, ystride);
        m_p->apply(pid);
#    endif
    }

private:
    OCIO::ConstProcessorRcPtr m_p;
#    if OCIO_VERSION_HEX >= 0x02000000
    OCIO::ConstCPUProcessorRcPtr m_cpuproc;
#    endif
};
#endif



#if OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 2, 0)
// For version 2.2 and later, missing OCIO config will always fall back on
// built-in configs, so we don't need any of these secondary fallback
// heuristics.

// ColorProcessor that hard-codes sRGB-to-linear
class ColorProcessor_sRGB_to_linear final : public ColorProcessor {
public:
    ColorProcessor_sRGB_to_linear()
        : ColorProcessor() {};
    ~ColorProcessor_sRGB_to_linear() override {}

    void apply(float* data, int width, int height, int channels,
               stride_t chanstride, stride_t xstride,
               stride_t ystride) const override
    {
        if (channels > 3)
            channels = 3;
        if (channels == 3 && chanstride == sizeof(float)) {
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    simd::vfloat4 r;
                    r.load((float*)d, 3);
                    r = sRGB_to_linear(r);
                    r.store((float*)d, 3);
                }
            }
        } else {
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    char* dc = d;
                    for (int c = 0; c < channels; ++c, dc += chanstride)
                        ((float*)dc)[c] = sRGB_to_linear(((float*)dc)[c]);
                }
            }
        }
    }
};


// ColorProcessor that hard-codes linear-to-sRGB
class ColorProcessor_linear_to_sRGB final : public ColorProcessor {
public:
    ColorProcessor_linear_to_sRGB()
        : ColorProcessor() {};
    ~ColorProcessor_linear_to_sRGB() override {}

    void apply(float* data, int width, int height, int channels,
               stride_t chanstride, stride_t xstride,
               stride_t ystride) const override
    {
        if (channels > 3)
            channels = 3;
        if (channels == 3 && chanstride == sizeof(float)) {
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    simd::vfloat4 r;
                    r.load((float*)d, 3);
                    r = linear_to_sRGB(r);
                    r.store((float*)d, 3);
                }
            }
        } else {
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    char* dc = d;
                    for (int c = 0; c < channels; ++c, dc += chanstride)
                        ((float*)dc)[c] = linear_to_sRGB(((float*)dc)[c]);
                }
            }
        }
    }
};



// ColorProcessor that hard-codes Rec709-to-linear
class ColorProcessor_Rec709_to_linear final : public ColorProcessor {
public:
    ColorProcessor_Rec709_to_linear()
        : ColorProcessor() {};
    ~ColorProcessor_Rec709_to_linear() override {}

    void apply(float* data, int width, int height, int channels,
               stride_t chanstride, stride_t xstride,
               stride_t ystride) const override
    {
        if (channels > 3)
            channels = 3;
        for (int y = 0; y < height; ++y) {
            char* d = (char*)data + y * ystride;
            for (int x = 0; x < width; ++x, d += xstride) {
                char* dc = d;
                for (int c = 0; c < channels; ++c, dc += chanstride)
                    ((float*)d)[c] = Rec709_to_linear(((float*)d)[c]);
            }
        }
    }
};


// ColorProcessor that hard-codes linear-to-Rec709
class ColorProcessor_linear_to_Rec709 final : public ColorProcessor {
public:
    ColorProcessor_linear_to_Rec709()
        : ColorProcessor() {};
    ~ColorProcessor_linear_to_Rec709() override {}

    void apply(float* data, int width, int height, int channels,
               stride_t chanstride, stride_t xstride,
               stride_t ystride) const override
    {
        if (channels > 3)
            channels = 3;
        for (int y = 0; y < height; ++y) {
            char* d = (char*)data + y * ystride;
            for (int x = 0; x < width; ++x, d += xstride) {
                char* dc = d;
                for (int c = 0; c < channels; ++c, dc += chanstride)
                    ((float*)d)[c] = linear_to_Rec709(((float*)d)[c]);
            }
        }
    }
};



// ColorProcessor that performs gamma correction
class ColorProcessor_gamma final : public ColorProcessor {
public:
    ColorProcessor_gamma(float gamma)
        : ColorProcessor()
        , m_gamma(gamma) {};
    ~ColorProcessor_gamma() override {}

    void apply(float* data, int width, int height, int channels,
               stride_t chanstride, stride_t xstride,
               stride_t ystride) const override
    {
        if (channels > 3)
            channels = 3;
        if (channels == 3 && chanstride == sizeof(float)) {
            simd::vfloat4 g = m_gamma;
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    simd::vfloat4 r;
                    r.load((float*)d, 3);
                    r = fast_pow_pos(r, g);
                    r.store((float*)d, 3);
                }
            }
        } else {
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    char* dc = d;
                    for (int c = 0; c < channels; ++c, dc += chanstride)
                        ((float*)d)[c] = powf(((float*)d)[c], m_gamma);
                }
            }
        }
    }

private:
    float m_gamma;
};


// ColorProcessor that does nothing (identity transform)
class ColorProcessor_Ident final : public ColorProcessor {
public:
    ColorProcessor_Ident()
        : ColorProcessor()
    {
    }
    ~ColorProcessor_Ident() override {}
    void apply(float* /*data*/, int /*width*/, int /*height*/, int /*channels*/,
               stride_t /*chanstride*/, stride_t /*xstride*/,
               stride_t /*ystride*/) const override
    {
    }
};
#endif



// ColorProcessor that implements a matrix multiply color transformation.
class ColorProcessor_Matrix final : public ColorProcessor {
public:
    ColorProcessor_Matrix(const Imath::M44f& Matrix, bool inverse)
        : ColorProcessor()
        , m_M(Matrix)
    {
        if (inverse)
            m_M = m_M.inverse();
    }
    ~ColorProcessor_Matrix() override {}

    void apply(float* data, int width, int height, int channels,
               stride_t chanstride, stride_t xstride,
               stride_t ystride) const override
    {
        using namespace simd;
        if (channels == 3 && chanstride == sizeof(float)) {
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    vfloat4 color;
                    color.load((float*)d, 3);
                    vfloat4 xcolor = color * m_M;
                    xcolor.store((float*)d, 3);
                }
            }
        } else if (channels >= 4 && chanstride == sizeof(float)) {
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    vfloat4 color;
                    color.load((float*)d);
                    vfloat4 xcolor = color * m_M;
                    xcolor.store((float*)d);
                }
            }
        } else {
            channels = std::min(channels, 4);
            for (int y = 0; y < height; ++y) {
                char* d = (char*)data + y * ystride;
                for (int x = 0; x < width; ++x, d += xstride) {
                    vfloat4 color;
                    char* dc = d;
                    for (int c = 0; c < channels; ++c, dc += chanstride)
                        color[c] = *(float*)dc;
                    vfloat4 xcolor = color * m_M;
                    for (int c = 0; c < channels; ++c, dc += chanstride)
                        *(float*)dc = xcolor[c];
                }
            }
        }
    }

private:
    simd::matrix44 m_M;
};



ColorProcessorHandle
ColorConfig::createColorProcessor(string_view inputColorSpace,
                                  string_view outputColorSpace,
                                  string_view context_key,
                                  string_view context_value) const
{
    return createColorProcessor(ustring(inputColorSpace),
                                ustring(outputColorSpace), ustring(context_key),
                                ustring(context_value));
}



ColorProcessorHandle
ColorConfig::createColorProcessor(ustring inputColorSpace,
                                  ustring outputColorSpace, ustring context_key,
                                  ustring context_value) const
{
    std::string pending_error;

    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(inputColorSpace, outputColorSpace, context_key,
                              context_value);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

#ifdef USE_OCIO
    // DBG("createColorProcessor {} -> {}\n", inputColorSpace,
    //                outputColorSpace);
    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    OCIO::ConstProcessorRcPtr p;
    if (getImpl()->config_ && !disable_ocio) {
        // Canonicalize the names
        inputColorSpace  = ustring(resolve(inputColorSpace));
        outputColorSpace = ustring(resolve(outputColorSpace));
        // DBG("after role substitution, {} -> {}\n", inputColorSpace,
        //                outputColorSpace);
        auto config  = getImpl()->config_;
        auto context = config->getCurrentContext();
        auto keys    = Strutil::splits(context_key, ",");
        auto values  = Strutil::splits(context_value, ",");
        if (keys.size() && values.size() && keys.size() == values.size()) {
            OCIO::ContextRcPtr ctx = context->createEditableCopy();
            for (size_t i = 0; i < keys.size(); ++i)
                ctx->setStringVar(keys[i].c_str(), values[i].c_str());
            context = ctx;
        }

        try {
            // Get the processor corresponding to this transform.
            p = getImpl()->config_->getProcessor(context,
                                                 inputColorSpace.c_str(),
                                                 outputColorSpace.c_str());
            getImpl()->clear_error();
            // DBG("Created OCIO processor '{}' -> '{}'\n",
            //                inputColorSpace, outputColorSpace);
        } catch (OCIO::Exception& e) {
            // Don't quit yet, remember the error and see if any of our
            // built-in knowledge of some generic spaces will save us.
            p.reset();
            pending_error = e.what();
            // DBG("FAILED to create OCIO processor '{}' -> '{}'\n",
            //                inputColorSpace, outputColorSpace);
        } catch (...) {
            p.reset();
            getImpl()->error(
                "An unknown error occurred in OpenColorIO, getProcessor");
        }

        if (p && !p->isNoOp()) {
            // If we got a valid processor that does something useful,
            // return it now. If it boils down to a no-op, give a second
            // chance below to recognize it as a special case.
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
            // DBG("OCIO processor '{}' -> '{}' is NOT NoOp, handle = {}\n",
            //                inputColorSpace, outputColorSpace, (bool)handle);
        }
    }
#endif

#if OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 2, 0)
    // For version 2.2 and later, missing OCIO config will always fall back on
    // built-in configs, so we don't need any of these secondary fallback
    // heuristics.
    if (!handle) {
        // Either not compiled with OCIO support, or no OCIO configuration
        // was found at all.  There are a few color conversions we know
        // about even in such dire conditions.
        using namespace Strutil;
        if (equivalent(inputColorSpace, outputColorSpace)) {
            handle = ColorProcessorHandle(new ColorProcessor_Ident);
        } else if ((equivalent(inputColorSpace, "lin_srgb")
                    || equivalent(inputColorSpace, "linear"))
                   && equivalent(outputColorSpace, "sRGB")) {
            handle = ColorProcessorHandle(new ColorProcessor_linear_to_sRGB);
        } else if (equivalent(inputColorSpace, "sRGB")
                   && (equivalent(outputColorSpace, "lin_srgb")
                       || equivalent(outputColorSpace, "linear"))) {
            handle = ColorProcessorHandle(new ColorProcessor_sRGB_to_linear);
        } else if ((equivalent(inputColorSpace, "lin_srgb")
                    || equivalent(inputColorSpace, "linear"))
                   && equivalent(outputColorSpace, "Rec709")) {
            handle = ColorProcessorHandle(new ColorProcessor_linear_to_Rec709);
        } else if (equivalent(inputColorSpace, "Rec709")
                   && equivalent(outputColorSpace, "lin_srgb")) {
            handle = ColorProcessorHandle(new ColorProcessor_Rec709_to_linear);
        } else if ((equivalent(inputColorSpace, "linear")
                    || equivalent(inputColorSpace, "lin_srgb"))
                   && istarts_with(outputColorSpace, "Gamma")) {
            string_view gamstr = outputColorSpace;
            Strutil::parse_word(gamstr);
            float g = from_string<float>(gamstr);
            handle  = ColorProcessorHandle(new ColorProcessor_gamma(1.0f / g));
        } else if (istarts_with(inputColorSpace, "Gamma")
                   && (equivalent(outputColorSpace, "linear")
                       || iequals(outputColorSpace, "lin_srgb"))) {
            string_view gamstr = inputColorSpace;
            Strutil::parse_word(gamstr);
            float g = from_string<float>(gamstr);
            handle  = ColorProcessorHandle(new ColorProcessor_gamma(g));
        } else {
            DBG("No heuristic non-OCIO color processor for '{}' -> '{}'\n",
                inputColorSpace, outputColorSpace);
            DBG("  is input equiv to srgb? {}\n",
                equivalent(inputColorSpace, "sRGB"));
            DBG("  is output equiv to linear? {}\n",
                equivalent(outputColorSpace, "linear"));
        }
        if (handle)
            pending_error.clear();
    }
#endif

#ifdef USE_OCIO
    if (!handle && p) {
        // If we found a processor from OCIO, even if it was a NoOp, and we
        // still don't have a better idea, return it.
        handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
    }
#endif

    if (pending_error.size())
        getImpl()->error("{}", pending_error);

    return getImpl()->addproc(prockey, handle);
}



ColorProcessorHandle
ColorConfig::createLookTransform(string_view looks, string_view inputColorSpace,
                                 string_view outputColorSpace, bool inverse,
                                 string_view context_key,
                                 string_view context_value) const
{
    return createLookTransform(ustring(looks), ustring(inputColorSpace),
                               ustring(outputColorSpace), inverse,
                               ustring(context_key), ustring(context_value));
}



ColorProcessorHandle
ColorConfig::createLookTransform(ustring looks, ustring inputColorSpace,
                                 ustring outputColorSpace, bool inverse,
                                 ustring context_key,
                                 ustring context_value) const
{
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(inputColorSpace, outputColorSpace, context_key,
                              context_value, looks, ustring() /*display*/,
                              ustring() /*view*/, ustring() /*file*/, inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

#ifdef USE_OCIO
    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_ && !disable_ocio) {
        OCIO::ConstConfigRcPtr config      = getImpl()->config_;
        OCIO::LookTransformRcPtr transform = OCIO::LookTransform::Create();
        transform->setLooks(looks.c_str());
        OCIO::TransformDirection dir;
        if (inverse) {
            // The TRANSFORM_DIR_INVERSE applies an inverse for the
            // end-to-end transform, which would otherwise do dst->inv
            // look -> src.  This is an unintuitive result for the
            // artist (who would expect in, out to remain unchanged), so
            // we account for that here by flipping src/dst
            transform->setSrc(c_str(resolve(outputColorSpace)));
            transform->setDst(c_str(resolve(inputColorSpace)));
            dir = OCIO::TRANSFORM_DIR_INVERSE;
        } else {  // forward
            transform->setSrc(c_str(resolve(inputColorSpace)));
            transform->setDst(c_str(resolve(outputColorSpace)));
            dir = OCIO::TRANSFORM_DIR_FORWARD;
        }
        auto context = config->getCurrentContext();
        auto keys    = Strutil::splits(context_key, ",");
        auto values  = Strutil::splits(context_value, ",");
        if (keys.size() && values.size() && keys.size() == values.size()) {
            OCIO::ContextRcPtr ctx = context->createEditableCopy();
            for (size_t i = 0; i < keys.size(); ++i)
                ctx->setStringVar(keys[i].c_str(), values[i].c_str());
            context = ctx;
        }

        OCIO::ConstProcessorRcPtr p;
        try {
            // Get the processor corresponding to this transform.
            p = getImpl()->config_->getProcessor(context, transform, dir);
            getImpl()->clear_error();
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (OCIO::Exception& e) {
            getImpl()->error(e.what());
        } catch (...) {
            getImpl()->error(
                "An unknown error occurred in OpenColorIO, getProcessor");
        }
    }
#endif

    return getImpl()->addproc(prockey, handle);
}



ColorProcessorHandle
ColorConfig::createDisplayTransform(string_view display, string_view view,
                                    string_view inputColorSpace,
                                    string_view looks, bool inverse,
                                    string_view context_key,
                                    string_view context_value) const
{
    return createDisplayTransform(ustring(display), ustring(view),
                                  ustring(inputColorSpace), ustring(looks),
                                  inverse, ustring(context_key),
                                  ustring(context_value));
}



ColorProcessorHandle
ColorConfig::createDisplayTransform(ustring display, ustring view,
                                    ustring inputColorSpace, ustring looks,
                                    bool inverse, ustring context_key,
                                    ustring context_value) const
{
    if (display.empty() || display == "default")
        display = getDefaultDisplayName();
    if (view.empty() || view == "default")
        view = getDefaultViewName(display);
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(inputColorSpace, ustring() /*outputColorSpace*/,
                              context_key, context_value, looks, display, view,
                              ustring() /*file*/, inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

#ifdef USE_OCIO
    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_ && !disable_ocio) {
        OCIO::ConstConfigRcPtr config = getImpl()->config_;
#    ifdef OCIO_v2
        auto transform = OCIO::DisplayViewTransform::Create();
        transform->setSrc(inputColorSpace.c_str());
        if (looks.size()) {
            getImpl()->error(
                "createDisplayTransform: looks overrides are not allowed in OpenColorIO v2");
        }
#    else
        auto transform = OCIO::DisplayTransform::Create();
        transform->setInputColorSpaceName(inputColorSpace.c_str());
        if (looks.size()) {
            transform->setLooksOverride(looks.c_str());
            transform->setLooksOverrideEnabled(true);
        } else {
            transform->setLooksOverrideEnabled(false);
        }
#    endif
        OCIO::TransformDirection dir = inverse ? OCIO::TRANSFORM_DIR_INVERSE
                                               : OCIO::TRANSFORM_DIR_FORWARD;
        transform->setDisplay(display.c_str());
        transform->setView(view.c_str());
        auto context = config->getCurrentContext();
        auto keys    = Strutil::splits(context_key, ",");
        auto values  = Strutil::splits(context_value, ",");
        if (keys.size() && values.size() && keys.size() == values.size()) {
            OCIO::ContextRcPtr ctx = context->createEditableCopy();
            for (size_t i = 0; i < keys.size(); ++i)
                ctx->setStringVar(keys[i].c_str(), values[i].c_str());
            context = ctx;
        }

        OCIO::ConstProcessorRcPtr p;
        try {
            // Get the processor corresponding to this transform.
            p = config->getProcessor(context, transform, dir);
            getImpl()->clear_error();
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (OCIO::Exception& e) {
            getImpl()->error(e.what());
        } catch (...) {
            getImpl()->error(
                "An unknown error occurred in OpenColorIO, getProcessor");
        }
    }
#endif

    return getImpl()->addproc(prockey, handle);
}



ColorProcessorHandle
ColorConfig::createDisplayTransform(string_view display, string_view view,
                                    string_view inputColorSpace,
                                    string_view looks, string_view context_key,
                                    string_view context_value) const
{
    return createDisplayTransform(ustring(display), ustring(view),
                                  ustring(inputColorSpace), ustring(looks),
                                  false, ustring(context_key),
                                  ustring(context_value));
}

ColorProcessorHandle
ColorConfig::createDisplayTransform(ustring display, ustring view,
                                    ustring inputColorSpace, ustring looks,
                                    ustring context_key,
                                    ustring context_value) const
{
    return createDisplayTransform(display, view, inputColorSpace, looks, false,
                                  context_key, context_value);
}



ColorProcessorHandle
ColorConfig::createFileTransform(string_view name, bool inverse) const
{
    return createFileTransform(ustring(name), inverse);
}



ColorProcessorHandle
ColorConfig::createFileTransform(ustring name, bool inverse) const
{
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(ustring() /*inputColorSpace*/,
                              ustring() /*outputColorSpace*/,
                              ustring() /*context_key*/,
                              ustring() /*context_value*/, ustring() /*looks*/,
                              ustring() /*display*/, ustring() /*view*/, name,
                              inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

#ifdef USE_OCIO
    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    OCIO::ConstConfigRcPtr config = getImpl()->config_;
    // If no config was found, config_ will be null. But that shouldn't
    // stop us for a filetransform, which doesn't need color spaces anyway.
    // Just use the default current config, it'll be freed when we exit.
    if (!config)
        config = ocio_current_config;
    if (config) {
        OCIO::FileTransformRcPtr transform = OCIO::FileTransform::Create();
        transform->setSrc(name.c_str());
        transform->setInterpolation(OCIO::INTERP_BEST);
        OCIO::TransformDirection dir    = inverse ? OCIO::TRANSFORM_DIR_INVERSE
                                                  : OCIO::TRANSFORM_DIR_FORWARD;
        OCIO::ConstContextRcPtr context = config->getCurrentContext();
        OCIO::ConstProcessorRcPtr p;
        try {
            // Get the processor corresponding to this transform.
            p = config->getProcessor(context, transform, dir);
            getImpl()->clear_error();
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (OCIO::Exception& e) {
            getImpl()->error(e.what());
        } catch (...) {
            getImpl()->error(
                "An unknown error occurred in OpenColorIO, getProcessor");
        }
    }
#endif

    return getImpl()->addproc(prockey, handle);
}



ColorProcessorHandle
ColorConfig::createMatrixTransform(M44fParam M, bool inverse) const
{
    return ColorProcessorHandle(
        new ColorProcessor_Matrix(*(const Imath::M44f*)M.data(), inverse));
}



string_view
ColorConfig::getColorSpaceFromFilepath(string_view str) const
{
#if defined(USE_OCIO) && (OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 1, 0))
    if (getImpl() && getImpl()->config_) {
        std::string s(str);
        string_view r = getImpl()->config_->getColorSpaceFromFilepath(
            s.c_str());
        if (!getImpl()->config_->filepathOnlyMatchesDefaultRule(s.c_str()))
            return r;
    }
#endif
    // Fall back on parseColorSpaceFromString
    return parseColorSpaceFromString(str);
}



string_view
ColorConfig::parseColorSpaceFromString(string_view str) const
{
#if defined(USE_OCIO) && (OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 1, 0))
    if (getImpl() && getImpl()->config_)
        return getImpl()->config_->parseColorSpaceFromString(str.c_str());
#endif

    // Reproduce the logic in OCIO v1 parseColorSpaceFromString

    if (str.empty())
        return "";

    // Get the colorspace names, sorted shortest-to-longest
    auto names = getColorSpaceNames();
    std::sort(names.begin(), names.end(),
              [](const std::string& a, const std::string& b) {
                  return a.length() < b.length();
              });

    // See if it matches a LUT name.
    // This is the position of the RIGHT end of the colorspace substring,
    // not the left
    size_t rightMostColorPos = std::string::npos;
    std::string rightMostColorspace;

    // Find the right-most occurrence within the string for each colorspace.
    for (auto&& csname : names) {
        // find right-most extension matched in filename
        size_t pos = Strutil::irfind(str, csname);
        if (pos == std::string::npos)
            continue;

        // If we have found a match, move the pointer over to the right end
        // of the substring.  This will allow us to find the longest name
        // that matches the rightmost colorspace
        pos += csname.size();

        if (rightMostColorPos == std::string::npos
            || pos >= rightMostColorPos) {
            rightMostColorPos   = pos;
            rightMostColorspace = csname;
        }
    }
    return string_view(ustring(rightMostColorspace));
}



//////////////////////////////////////////////////////////////////////////
//
// Image Processing Implementations


bool
ImageBufAlgo::colorconvert(ImageBuf& dst, const ImageBuf& src, string_view from,
                           string_view to, bool unpremult,
                           string_view context_key, string_view context_value,
                           const ColorConfig* colorconfig, ROI roi,
                           int nthreads)
{
    pvt::LoggedTimer logtime("IBA::colorconvert");
    if (from.empty() || from == "current") {
        from = src.spec().get_string_attribute("oiio:Colorspace", "linear");
    }
    if (from.empty() || to.empty()) {
        dst.errorfmt("Unknown color space name");
        return false;
    }
    ColorProcessorHandle processor;
    {
        if (!colorconfig)
            colorconfig = &ColorConfig::default_colorconfig();
        processor
            = colorconfig->createColorProcessor(colorconfig->resolve(from),
                                                colorconfig->resolve(to),
                                                context_key, context_value);
        if (!processor) {
            if (colorconfig->error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
#ifdef USE_OCIO
                dst.errorfmt(
                    "Could not construct the color transform {} -> {} (unknown error)",
                    from, to);
#else
                dst.errorfmt(
                    "Could not construct the color transform {} -> {} (no OpenColorIO support)",
                    from, to);
#endif
            return false;
        }
    }

    logtime.stop(-1);  // transition to other colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok) {
        // DBG("done, setting output colorspace to {}\n", to);
        dst.specmod().set_colorspace(to);
    }
    return ok;
}



ImageBuf
ImageBufAlgo::colorconvert(const ImageBuf& src, string_view from,
                           string_view to, bool unpremult,
                           string_view context_key, string_view context_value,
                           const ColorConfig* colorconfig, ROI roi,
                           int nthreads)
{
    ImageBuf result;
    bool ok = colorconvert(result, src, from, to, unpremult, context_key,
                           context_value, colorconfig, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::colorconvert() error");
    return result;
}



bool
ImageBufAlgo::colormatrixtransform(ImageBuf& dst, const ImageBuf& src,
                                   M44fParam M, bool unpremult, ROI roi,
                                   int nthreads)
{
    pvt::LoggedTimer logtime("IBA::colormatrixtransform");
    ColorProcessorHandle processor
        = ColorConfig::default_colorconfig().createMatrixTransform(M);
    logtime.stop();  // transition to other colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::colormatrixtransform(const ImageBuf& src, M44fParam M,
                                   bool unpremult, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = colormatrixtransform(result, src, M, unpremult, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::colormatrixtransform() error");
    return result;
}



template<class Rtype, class Atype>
static bool
colorconvert_impl(ImageBuf& R, const ImageBuf& A,
                  const ColorProcessor* processor, bool unpremult, ROI roi,
                  int nthreads)
{
    using namespace ImageBufAlgo;
    using namespace simd;
    // Only process up to, and including, the first 4 channels.  This
    // does let us process images with fewer than 4 channels, which is
    // the intent.
    int channelsToCopy = std::min(4, roi.nchannels());
    if (channelsToCopy < 4)
        unpremult = false;
    // clang-format off
    parallel_image(
        roi, parallel_options(nthreads),
        [&, unpremult, channelsToCopy, processor](ROI roi) {
            int width = roi.width();
            // Temporary space to hold one RGBA scanline
            vfloat4* scanline;
            OIIO_ALLOCATE_STACK_OR_HEAP(scanline, vfloat4, width);
            float* alpha;
            OIIO_ALLOCATE_STACK_OR_HEAP(alpha, float, width);
            const float fltmin = std::numeric_limits<float>::min();
            ImageBuf::ConstIterator<Atype> a(A, roi);
            ImageBuf::Iterator<Rtype> r(R, roi);
            for (int k = roi.zbegin; k < roi.zend; ++k) {
                for (int j = roi.ybegin; j < roi.yend; ++j) {
                    // Load the scanline
                    a.rerange(roi.xbegin, roi.xend, j, j + 1, k, k + 1);
                    for (int i = 0; !a.done(); ++a, ++i) {
                        vfloat4 v(0.0f);
                        for (int c = 0; c < channelsToCopy; ++c)
                            v[c] = a[c];
                        if (channelsToCopy == 1)
                            v[2] = v[1] = v[0];
                        scanline[i] = v;
                    }

                    // Optionally unpremult. Be careful of alpha==0 pixels,
                    // preserve their color rather than div-by-zero.
                    if (unpremult) {
                        for (int i = 0; i < width; ++i) {
                            float a  = extract<3>(scanline[i]);
                            alpha[i] = a;
                            a        = a >= fltmin ? a : 1.0f;
                            scanline[i] /= vfloat4(a,a,a,1.0f);
                        }
                    }

                    // Apply the color transformation in place
                    processor->apply((float*)&scanline[0], width, 1, 4,
                                     sizeof(float), 4 * sizeof(float),
                                     width * 4 * sizeof(float));

                    // Optionally re-premult. Be careful of alpha==0 pixels,
                    // preserve their value rather than crushing to black.
                    if (unpremult) {
                        for (int i = 0; i < width; ++i) {
                            float a  = alpha[i];
                            a        = a >= fltmin ? a : 1.0f;
                            scanline[i] *= vfloat4(a,a,a,1.0f);
                        }
                    }

                    // Store the scanline
                    float* dstPtr = (float*)&scanline[0];
                    r.rerange(roi.xbegin, roi.xend, j, j + 1, k, k + 1);
                    for (; !r.done(); ++r, dstPtr += 4)
                        for (int c = 0; c < channelsToCopy; ++c)
                            r[c] = dstPtr[c];
                    if (channelsToCopy < roi.chend && (&R != &A)) {
                        // If there are "leftover" channels, just copy them
                        // unaltered from the source.
                        a.rerange(roi.xbegin, roi.xend, j, j + 1, k, k + 1);
                        r.rerange(roi.xbegin, roi.xend, j, j + 1, k, k + 1);
                        for (; !r.done(); ++r, ++a)
                            for (int c = channelsToCopy; c < roi.chend; ++c)
                                r[c] = 0.5 + 10 * a[c];
                    }
                }
            }
        });
    // clang-format on
    return true;
}



// Specialized version where both buffers are in memory (not cache based),
// float data, and we are dealing with 4 channels.
static bool
colorconvert_impl_float_rgba(ImageBuf& R, const ImageBuf& A,
                             const ColorProcessor* processor, bool unpremult,
                             ROI roi, int nthreads)
{
    using namespace ImageBufAlgo;
    using namespace simd;
    OIIO_ASSERT(R.localpixels() && A.localpixels()
                && R.spec().format == TypeFloat && A.spec().format == TypeFloat
                && R.nchannels() == 4 && A.nchannels() == 4);
    parallel_image(roi, parallel_options(nthreads), [&](ROI roi) {
        int width = roi.width();
        // Temporary space to hold one RGBA scanline
        vfloat4* scanline;
        OIIO_ALLOCATE_STACK_OR_HEAP(scanline, vfloat4, width);
        float* alpha;
        OIIO_ALLOCATE_STACK_OR_HEAP(alpha, float, width);
        const float fltmin = std::numeric_limits<float>::min();
        for (int k = roi.zbegin; k < roi.zend; ++k) {
            for (int j = roi.ybegin; j < roi.yend; ++j) {
                // Load the scanline
                memcpy((void*)scanline, A.pixeladdr(roi.xbegin, j, k),
                       width * 4 * sizeof(float));
                // Optionally unpremult
                if (unpremult) {
                    for (int i = 0; i < width; ++i) {
                        vfloat4 p(scanline[i]);
                        float a  = extract<3>(p);
                        alpha[i] = a;
                        a        = a >= fltmin ? a : 1.0f;
                        if (a == 1.0f)
                            scanline[i] = p;
                        else
                            scanline[i] = p / vfloat4(a, a, a, 1.0f);
                    }
                }

                // Apply the color transformation in place
                processor->apply((float*)&scanline[0], width, 1, 4,
                                 sizeof(float), 4 * sizeof(float),
                                 width * 4 * sizeof(float));

                // Optionally premult
                if (unpremult) {
                    for (int i = 0; i < width; ++i) {
                        vfloat4 p(scanline[i]);
                        float a = alpha[i];
                        a       = a >= fltmin ? a : 1.0f;
                        p *= vfloat4(a, a, a, 1.0f);
                        scanline[i] = p;
                    }
                }
                memcpy(R.pixeladdr(roi.xbegin, j, k), scanline,
                       width * 4 * sizeof(float));  //NOSONAR
            }
        }
    });
    return true;
}



bool
ImageBufAlgo::colorconvert(ImageBuf& dst, const ImageBuf& src,
                           const ColorProcessor* processor, bool unpremult,
                           ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::colorconvert");
    // If the processor is NULL, return false (error)
    if (!processor) {
        dst.errorfmt(
            "Passed NULL ColorProcessor to colorconvert() [probable application bug]");
        return false;
    }

    // If the processor is a no-op and the conversion is being done
    // in place, no work needs to be done. Early exit.
    if (processor->isNoOp() && (&dst == &src))
        return true;

    if (!IBAprep(roi, &dst, &src))
        return false;

    // If the processor is a no-op (and it's not an in-place conversion),
    // use copy() to simplify the operation.
    if (processor->isNoOp()) {
        logtime.stop();  // transition to copy
        return ImageBufAlgo::copy(dst, src, TypeUnknown, roi, nthreads);
    }

    if (unpremult && src.spec().alpha_channel >= 0
        && src.spec().get_int_attribute("oiio:UnassociatedAlpha") != 0) {
        // If we appear to be operating on an image that already has
        // unassociated alpha, don't do a redundant unpremult step.
        unpremult = false;
    }

    if (dst.localpixels() && src.localpixels() && dst.spec().format == TypeFloat
        && src.spec().format == TypeFloat && dst.nchannels() == 4
        && src.nchannels() == 4) {
        return colorconvert_impl_float_rgba(dst, src, processor, unpremult, roi,
                                            nthreads);
    }

    bool ok = true;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "colorconvert", colorconvert_impl,
                                dst.spec().format, src.spec().format, dst, src,
                                processor, unpremult, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::colorconvert(const ImageBuf& src, const ColorProcessor* processor,
                           bool unpremult, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = colorconvert(result, src, processor, unpremult, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::colorconvert() error");
    return result;
}



bool
ImageBufAlgo::ociolook(ImageBuf& dst, const ImageBuf& src, string_view looks,
                       string_view from, string_view to, bool unpremult,
                       bool inverse, string_view key, string_view value,
                       const ColorConfig* colorconfig, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::ociolook");
    if (from.empty() || from == "current") {
        auto linearspace = colorconfig->resolve("linear");
        from = src.spec().get_string_attribute("oiio:Colorspace", linearspace);
    }
    if (to.empty() || to == "current") {
        auto linearspace = colorconfig->resolve("linear");
        to = src.spec().get_string_attribute("oiio:Colorspace", linearspace);
    }
    if (from.empty() || to.empty()) {
        dst.errorfmt("Unknown color space name");
        return false;
    }
    ColorProcessorHandle processor;
    {
        if (!colorconfig)
            colorconfig = &ColorConfig::default_colorconfig();
        processor = colorconfig->createLookTransform(looks,
                                                     colorconfig->resolve(from),
                                                     colorconfig->resolve(to),
                                                     inverse, key, value);
        if (!processor) {
            if (colorconfig->error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
#ifdef USE_OCIO
                dst.errorfmt(
                    "Could not construct the color transform (unknown error)");
#else
                dst.errorfmt(
                    "Could not construct the color transform (no OpenColorIO support)");
#endif
            return false;
        }
    }

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok)
        dst.specmod().set_colorspace(to);
    return ok;
}



ImageBuf
ImageBufAlgo::ociolook(const ImageBuf& src, string_view looks, string_view from,
                       string_view to, bool unpremult, bool inverse,
                       string_view key, string_view value,
                       const ColorConfig* colorconfig, ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = ociolook(result, src, looks, from, to, unpremult, inverse, key,
                       value, colorconfig, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::ociolook() error");
    return result;
}



bool
ImageBufAlgo::ociodisplay(ImageBuf& dst, const ImageBuf& src,
                          string_view display, string_view view,
                          string_view from, string_view looks, bool unpremult,
                          bool inverse, string_view key, string_view value,
                          const ColorConfig* colorconfig, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::ociodisplay");
    ColorProcessorHandle processor;
    {
        if (!colorconfig)
            colorconfig = &ColorConfig::default_colorconfig();
        if (from.empty() || from == "current") {
            auto linearspace = colorconfig->resolve("linear");
            from = src.spec().get_string_attribute("oiio:Colorspace",
                                                   linearspace);
        }
        if (from.empty()) {
            dst.errorfmt("Unknown color space name");
            return false;
        }
        processor
            = colorconfig->createDisplayTransform(display, view,
                                                  colorconfig->resolve(from),
                                                  looks, inverse, key, value);
        if (!processor) {
            if (colorconfig->error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
#ifdef USE_OCIO
                dst.errorfmt(
                    "Could not construct the color transform (unknown error)");
#else
                dst.errorfmt(
                    "Could not construct the color transform (no OpenColorIO support)");
#endif
            return false;
        }
    }

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::ociodisplay(const ImageBuf& src, string_view display,
                          string_view view, string_view from, string_view looks,
                          bool unpremult, bool inverse, string_view key,
                          string_view value, const ColorConfig* colorconfig,
                          ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = ociodisplay(result, src, display, view, from, looks, unpremult,
                          inverse, key, value, colorconfig, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::ociodisplay() error");
    return result;
}



// DEPRECATED(2.5)
bool
ImageBufAlgo::ociodisplay(ImageBuf& dst, const ImageBuf& src,
                          string_view display, string_view view,
                          string_view from, string_view looks, bool unpremult,
                          string_view key, string_view value,
                          const ColorConfig* colorconfig, ROI roi, int nthreads)
{
    return ociodisplay(dst, src, display, view, from, looks, unpremult, false,
                       key, value, colorconfig, roi, nthreads);
}



// DEPRECATED(2.5)
ImageBuf
ImageBufAlgo::ociodisplay(const ImageBuf& src, string_view display,
                          string_view view, string_view from, string_view looks,
                          bool unpremult, string_view key, string_view value,
                          const ColorConfig* colorconfig, ROI roi, int nthreads)
{
    return ociodisplay(src, display, view, from, looks, unpremult, false, key,
                       value, colorconfig, roi, nthreads);
}



bool
ImageBufAlgo::ociofiletransform(ImageBuf& dst, const ImageBuf& src,
                                string_view name, bool unpremult, bool inverse,
                                const ColorConfig* colorconfig, ROI roi,
                                int nthreads)
{
    pvt::LoggedTimer logtime("IBA::ociofiletransform");
    if (name.empty()) {
        dst.errorfmt("Unknown filetransform name");
        return false;
    }
    ColorProcessorHandle processor;
    {
        if (!colorconfig)
            colorconfig = &ColorConfig::default_colorconfig();
        processor = colorconfig->createFileTransform(name, inverse);
        if (!processor) {
            if (colorconfig->error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
#ifdef USE_OCIO
                dst.errorfmt(
                    "Could not construct the color transform (unknown error)");
#else
                dst.errorfmt(
                    "Could not construct the color transform (no OpenColorIO support)");
#endif
            return false;
        }
    }

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok)
        dst.specmod().set_colorspace(name);
    return ok;
}



ImageBuf
ImageBufAlgo::ociofiletransform(const ImageBuf& src, string_view name,
                                bool unpremult, bool inverse,
                                const ColorConfig* colorconfig, ROI roi,
                                int nthreads)
{
    ImageBuf result;
    bool ok = ociofiletransform(result, src, name, unpremult, inverse,
                                colorconfig, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::ociofiletransform() error");
    return result;
}



bool
ImageBufAlgo::colorconvert(span<float> color, const ColorProcessor* processor,
                           bool unpremult)
{
    // If the processor is NULL, return false (error)
    if (!processor) {
        return false;
    }

    // If the processor is a no-op, no work needs to be done. Early exit.
    if (processor->isNoOp())
        return true;

    // Load the pixel
    float rgba[4]      = { 0.0f, 0.0f, 0.0f, 0.0f };
    int channelsToCopy = std::min(4, (int)color.size());
    memcpy(rgba, color.data(), channelsToCopy * sizeof(float));

    const float fltmin = std::numeric_limits<float>::min();

    // Optionally unpremult
    if ((channelsToCopy >= 4) && unpremult) {
        float alpha = rgba[3];
        if (alpha > fltmin) {
            rgba[0] /= alpha;
            rgba[1] /= alpha;
            rgba[2] /= alpha;
        }
    }

    // Apply the color transformation
    processor->apply(rgba, 1, 1, 4, sizeof(float), 4 * sizeof(float),
                     4 * sizeof(float));

    // Optionally premult
    if ((channelsToCopy >= 4) && unpremult) {
        float alpha = rgba[3];
        if (alpha > fltmin) {
            rgba[0] *= alpha;
            rgba[1] *= alpha;
            rgba[2] *= alpha;
        }
    }

    // Store the scanline
    memcpy(color.data(), rgba, channelsToCopy * sizeof(float));

    return true;
}



OIIO_NAMESPACE_END

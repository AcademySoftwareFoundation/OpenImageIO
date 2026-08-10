// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <tsl/robin_map.h>

#include <OpenImageIO/Imath.h>

#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/unordered_map_concurrent.h>

#include "imageio_pvt.h"

#include "color_ocio_pvt.h"


OIIO_NAMESPACE_3_1_BEGIN

namespace {
// Some test colors we use to interrogate transformations
static const int n_test_colors = 5;
static const Imath::C3f test_colors[n_test_colors]
    = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1 }, { 0.5, 0.5, 0.5 } };
}  // namespace

// Make an editable copy of `config`, working around an OCIO bug (fixed in
// 2.3.1) where createEditableCopy() drops the default view transform name.
OCIO::ConfigRcPtr
copy_config(const OCIO::ConstConfigRcPtr& config)
{
    auto copy = config->createEditableCopy();
#if OCIO_VERSION_HEX < 0x02030100
    // OCIO before 2.3.1 loses the default view transform name when copying a
    // config; restore it.
    std::string default_vt = config->getDefaultViewTransformName();
    if (!default_vt.empty()
        && default_vt != copy->getDefaultViewTransformName())
        copy->setDefaultViewTransformName(default_vt.c_str());
#endif
    return copy;
}


// Test probe backing pvt::copy_config_preserves_default_view_transform()
// (declared in the current namespace, forwarded here). Builds a config with
// TWO view transforms whose explicit default is the NON-first one, runs the
// real copy_config(), and reports whether the copy kept that explicit default.
// The two-VT shape matters: OCIO reports the first view transform as the
// implicit default when none is set, so a single-VT probe would pass even if
// createEditableCopy() silently dropped the explicit default. Returns true on
// preservation -- native on OCIO >= 2.3.1, workaround-restored below that --
// and vacuously true when OCIO support or the view-transform API is
// unavailable.
bool
copy_config_default_vt_probe()
{
    if (!ColorConfig::supportsOpenColorIO())
        return true;
    try {
        OCIO::ConfigRcPtr cfg = OCIO::Config::CreateRaw()->createEditableCopy();
        for (const char* name : { "probe_vt_a", "probe_vt_b" }) {
            auto vt = OCIO::ViewTransform::Create(OCIO::REFERENCE_SPACE_SCENE);
            vt->setName(name);
            vt->setTransform(OCIO::MatrixTransform::Create(),
                             OCIO::VIEWTRANSFORM_DIR_FROM_REFERENCE);
            cfg->addViewTransform(vt);
        }
        // Explicit default is the SECOND view transform, not OCIO's implicit
        // first-VT default -- so a dropped default is observable.
        cfg->setDefaultViewTransformName("probe_vt_b");

        OCIO::ConstConfigRcPtr src = cfg;
        OCIO::ConfigRcPtr copy     = copy_config(src);
        const char* copied_default = copy->getDefaultViewTransformName();
        return copied_default && std::string(copied_default) == "probe_vt_b";
    } catch (const OCIO::Exception&) {
        return true;  // view-transform API unavailable -> vacuous pass
    }
}


#if 1 || !defined(NDEBUG) /* allow color configuration debugging */
bool colordebug = Strutil::stoi(Sysutil::getenv("OIIO_DEBUG_COLOR"))
                  || Strutil::stoi(Sysutil::getenv("OIIO_DEBUG_ALL"));
#endif


int disable_ocio = Strutil::stoi(Sysutil::getenv("OIIO_DISABLE_OCIO"));
int disable_builtin_configs = Strutil::stoi(
    Sysutil::getenv("OIIO_DISABLE_BUILTIN_OCIO_CONFIGS"));
static OCIO::ConstConfigRcPtr ocio_current_config;



const ColorConfig&
ColorConfig::default_colorconfig()
{
    static ColorConfig config;
    return config;
}



bool
ColorConfig::supportsOpenColorIO()
{
    return (disable_ocio == 0);
}



int
ColorConfig::OpenColorIO_version_hex()
{
    return OCIO_VERSION_HEX;
}



// ColorConfig utility to take inventory of the color spaces available.
// It sets up knowledge of "linear", "srgb_rec709_scene", "Rec709", etc,
// even if the underlying OCIO configuration lacks them.
void
ColorConfig::Impl::inventory()
{
    DBG("inventorying config {}\n", configname());
    if (config_ && !disable_ocio) {
        try {
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
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in inventory: {}", e.what());
        }
    }

    // If we had some kind of bogus configuration that seemed to define
    // only a "raw" color space and nothing else, that's useless, so
    // figure out our own way to move forward.
    config_.reset();

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
    add("lin_rec709_scene", 0, linflags);
    add("lin_srgb", 0, linflags);
    add("lin_rec709", 0, linflags);
    add("srgb_rec709_scene", 1, CSInfo::is_srgb);
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
    auto proc = get_to_builtin_cpu_proc(my_from, builtin_to);
    if (proc) {
        Imath::C3f colors[n_test_colors];
        std::copy(test_colors, test_colors + n_test_colors, colors);
        proc->apply(OCIO::PackedImageDesc(colors, n_test_colors, 1, 3));
        if (close_colors(colors, test_colors))
            return true;
    }
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
    auto n             = test_colors.size();
    Imath::C3f* colors = OIIO_ALLOCA(Imath::C3f, n);
    std::copy(test_colors.data(), test_colors.data() + n, colors);
    proc->apply((float*)colors, int(n), 1, 3, sizeof(float), 3 * sizeof(float),
                int(n) * 3 * sizeof(float));
    return close_colors({ colors, n }, result_colors);
}



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



void
ColorConfig::Impl::classify_by_name(CSInfo& cs)
{
    // General heuristics based on the names -- for a few canonical names,
    // believe them! Woe be unto the poor soul who names a color space "sRGB"
    // or "ACEScg" and it's really something entirely different.
    if (Strutil::iequals(cs.name, "srgb_rec709_scene")
        || Strutil::iequals(cs.name, "srgb_tx")
        || Strutil::iequals(cs.name, "srgb_texture")
        || Strutil::iequals(cs.name, "srgb texture")
        || Strutil::iequals(cs.name, "srgb_rec709_scene")
        || Strutil::iequals(cs.name, "sRGB - Texture")
        || Strutil::iequals(cs.name, "sRGB")) {
        cs.setflag(CSInfo::is_srgb, srgb_alias);
    } else if (Strutil::iequals(cs.name, "lin_rec709_scene")
               || Strutil::iequals(cs.name, "lin_rec709")
               || Strutil::iequals(cs.name, "Linear Rec.709 (sRGB)")
               || Strutil::iequals(cs.name, "lin_srgb")
               || Strutil::iequals(cs.name, "linear")) {
        cs.setflag(CSInfo::is_lin_srgb | CSInfo::is_linear_response,
                   lin_srgb_alias);
    } else if (Strutil::iequals(cs.name, "ACEScg")
               || Strutil::iequals(cs.name, "lin_ap1_scene")
               || Strutil::iequals(cs.name, "lin_ap1")) {
        cs.setflag(CSInfo::is_ACEScg | CSInfo::is_linear_response,
                   ACEScg_alias);
    } else if (Strutil::iequals(cs.name, "Rec709")) {
        cs.setflag(CSInfo::is_Rec709, Rec709_alias);
    } else if (config_
               && Strutil::iequals(cs.name, config_->getCanonicalName("data"))) {
        cs.setflag(CSInfo::is_data);
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
    } else if (Strutil::starts_with(cs.name, "nc")) {
        cs.setflag(CSInfo::is_data);
        DBG("Classifying {} as data based on SPI name\n", cs.name);
    }
#endif

    // Set up some canonical names
    if (cs.flags() & CSInfo::is_srgb)
        cs.canonical = "srgb_rec709_scene";
    else if (cs.flags() & CSInfo::is_lin_srgb)
        cs.canonical = "lin_rec709_scene";
    else if (cs.flags() & CSInfo::is_ACEScg)
        cs.canonical = "lin_ap1_scene";
    else if (cs.flags() & CSInfo::is_Rec709)
        cs.canonical = "Rec709";
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
    if (cs.ocio_cs && cs.ocio_cs->isData()) {
        cs.setflag(CSInfo::is_data);
        DBG("Classifying {} as data isData() [1]\n", cs.name);
    }

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
        try {
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
            } else if (check_same_as_builtin_transform(cs.name.c_str(),
                                                       "srgb_tx")) {
                cs.setflag(CSInfo::is_srgb, srgb_alias);
            } else if (check_same_as_builtin_transform(cs.name.c_str(),
                                                       "lin_srgb")) {
                cs.setflag(CSInfo::is_lin_srgb | CSInfo::is_linear_response,
                           lin_srgb_alias);
            } else if (check_same_as_builtin_transform(cs.name.c_str(),
                                                       "ACEScg")) {
                cs.setflag(CSInfo::is_ACEScg | CSInfo::is_linear_response,
                           ACEScg_alias);
            }
            if (cs.ocio_cs->isData()) {
                cs.setflag(CSInfo::is_data);
                DBG("Classifying {} as data isData() [2]\n", cs.name);
            }
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in classify_by_conversions: {}", e.what());
        }
    }

    // Set up some canonical names
    if (cs.flags() & CSInfo::is_srgb)
        cs.canonical = "srgb_rec709_scene";
    else if (cs.flags() & CSInfo::is_lin_srgb)
        cs.canonical = "lin_rec709_scene";
    else if (cs.flags() & CSInfo::is_ACEScg)
        cs.canonical = "lin_ap1_scene";
    else if (cs.flags() & CSInfo::is_Rec709)
        cs.canonical = "Rec709";
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
    Timer timer;
    if (auto n = IdentifyBuiltinColorSpace("srgb_tx")) {
        if (CSInfo* cs = find(n)) {
            cs->setflag(CSInfo::is_srgb, srgb_alias);
            DBG("Identified {} = builtin '{}'\n", "srgb_rec709_scene",
                cs->name);
        }
    } else {
        DBG("No config space identified as srgb\n");
    }
    DBG("identify_builtin_equivalents srgb took {:0.2f}s\n", timer.lap());
    if (auto n = IdentifyBuiltinColorSpace("lin_srgb")) {
        if (CSInfo* cs = find(n)) {
            cs->setflag(CSInfo::is_lin_srgb | CSInfo::is_linear_response,
                        lin_srgb_alias);
            DBG("Identified {} = builtin '{}'\n", "lin_rec709_scene", cs->name);
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
}



const char*
ColorConfig::Impl::IdentifyBuiltinColorSpace(const char* name) const
{
    if (!config_ || disable_builtin_configs)
        return nullptr;
    try {
        return OCIO::Config::IdentifyBuiltinColorSpace(config_, builtinconfig_,
                                                       name);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in IdentifyBuiltinColorSpace: {}", e.what());
    }
    return nullptr;
}



ColorConfig::ColorConfig(string_view filename) { (void)reset(filename); }



ColorConfig::ColorConfig(UninitTag)
    : m_impl(new Impl(this))
{
}



ColorConfig::ColorConfig(ColorConfig&& other) noexcept
    : m_impl(std::move(other.m_impl))
{
    if (m_impl)
        m_impl->set_self(this);
}



ColorConfig&
ColorConfig::operator=(ColorConfig&& other) noexcept
{
    if (this != &other) {
        m_impl = std::move(other.m_impl);
        if (m_impl)
            m_impl->set_self(this);
    }
    return *this;
}



ColorConfig
ColorConfig::from_text(string_view config_text, string_view working_dir)
{
    ColorConfig cc { UninitTag() };
    (void)cc.m_impl->init_from_text(config_text, working_dir);
    return cc;
}



ColorConfig::~ColorConfig() {}



// OIIO doctoring of OCIO configs for different default file rules. Currently,
// we only do this for built-in configs.
static void
fix_config_file_rules(OCIO::ConfigRcPtr& config)
{
    OIIO_CONTRACT_ASSERT(config);
    DBG("Fixing up rules:\n");
#if 1
    // Just start with a clean slate
    auto rules = OCIO::FileRules::Create();
#else
    // Alternate universe: Start with the existing rules
    auto rules = config->getFileRules()->createEditableCopy();
#endif
    for (size_t i = 0, e = rules->getNumEntries(); i != e; ++i) {
        DBG("  rule {}/{}: pat='{}' ext='{}' -> \"{}\"\n", i, rules->getName(i),
            rules->getRegex(i), rules->getExtension(i),
            rules->getColorSpace(i));
#if 0
        // If we wanted to doctor just the exr rule, here's how:
        if (Strutil::iequals(rules->getExtension(i), "exr")) {
            // Change the rule for exr extension, if it exists, to "unknown".
            // Make no assumptions. OCIO's built-in configs think it should be
            // ACES2065-1, which is almost never right.
            rules->setColorSpace(i, "unknown");
            DBG("    changed cs to \"{}\"\n", rules->getColorSpace(i));
        } else
#endif
        if (!strcmp(rules->getName(i), "Default")) {
            // Default rule or one that matches everything -- for OIIO, we
            // just want to change this to unknown. We made decisions about
            // default per-file-format color space decisions in the individual
            // readers. We don't even consider file extension to be reliable
            // evidence of the file type.
            rules->setColorSpace(i, "unknown");
            DBG("    changed cs to \"{}\"\n", rules->getColorSpace(i));
        }
    }

    // But make the path search rule (look for the right-most color space name
    // embedded in the path) have precedence over file naming rules.
    rules->insertPathSearchRule(0);
    config->setFileRules(rules);
}



void
ColorConfig::Impl::init_builtin()
{
    try {
        auto cfg = OCIO::Config::CreateFromFile("ocio://default");
        OIIO_CONTRACT_ASSERT(cfg);
        // Fix up a mutable copy, then freeze it into the const member.
        OCIO::ConfigRcPtr builtin = copy_config(cfg);
        fix_config_file_rules(builtin);
        builtinconfig_ = builtin;
    } catch (OCIO::Exception& e) {
        error("Error making OCIO built-in config: {}", e.what());
    }
}



bool
ColorConfig::Impl::init(string_view filename)
{
    OIIO_MAYBE_UNUSED Timer timer;

    auto oldlog = OCIO::GetLoggingLevel();
    OCIO::SetLoggingLevel(OCIO::LOGGING_LEVEL_NONE);

    init_builtin();

    // If no filename was specified, use env $OCIO
    if (filename.empty())
        filename = Sysutil::getenv("OCIO");
    if (filename.empty() && !disable_builtin_configs)
        filename = "ocio://default";
    if (filename.size() && !OIIO::Filesystem::exists(filename)
        && !Strutil::istarts_with(filename, "ocio://")) {
        error("Requested non-existent OCIO config \"{}\"", filename);
    } else {
        // Either filename passed, or taken from $OCIO, and it seems to exist
        try {
            configname(filename);
            auto cfg = OCIO::Config::CreateFromFile(
                std::string(filename).c_str());
            if (cfg) {
                // Fix up a mutable copy, then freeze it into the const
                // member.
                OCIO::ConfigRcPtr copy = copy_config(cfg);
                if (copy && Strutil::istarts_with(filename, "ocio://"))
                    fix_config_file_rules(copy);
                config_ = copy;
            }
        } catch (OCIO::Exception& e) {
            error("Error reading OCIO config \"{}\": {}", filename, e.what());
        } catch (...) {
            error("Error reading OCIO config \"{}\"", filename);
        }
    }
    OCIO::SetLoggingLevel(oldlog);

    DBG("OCIO config {} loaded in {:0.2f} seconds\n", filename, timer.lap());

    return finish_init();
}



bool
ColorConfig::Impl::finish_init()
{
    OIIO_MAYBE_UNUSED Timer timer;
    bool ok = config_.get() != nullptr;

    if (!original_config_)
        original_config_ = config_;

    inventory();
    // NOTE: inventory already does classify_by_name

    DBG("\nIDENTIFY BUILTIN EQUIVALENTS\n");
    identify_builtin_equivalents();  // OCIO 2.3+ only
    DBG("OCIO 2.3+ builtin equivalents in {:0.2f} seconds\n", timer.lap());

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
    DBG("OCIO config {} classified in {:0.2f} seconds\n", configname(),
        timer.lap());

    return ok;
}



bool
ColorConfig::Impl::init_from_config(OCIO::ConstConfigRcPtr config,
                                    string_view name,
                                    OCIO::ConstConfigRcPtr original)
{
    auto oldlog = OCIO::GetLoggingLevel();
    OCIO::SetLoggingLevel(OCIO::LOGGING_LEVEL_NONE);
    init_builtin();
    OCIO::SetLoggingLevel(oldlog);

    configname(name);
    config_          = std::move(config);
    original_config_ = original ? std::move(original) : config_;
    return finish_init();
}



bool
ColorConfig::Impl::init_from_text(string_view config_text,
                                  string_view working_dir)
{
    auto oldlog = OCIO::GetLoggingLevel();
    OCIO::SetLoggingLevel(OCIO::LOGGING_LEVEL_NONE);

    OCIO::ConstConfigRcPtr frozen;
    std::string name = "text:(invalid)";
    try {
        std::istringstream iss { std::string(config_text) };
        auto cfg = OCIO::Config::CreateFromStream(iss);
        if (cfg) {
            // Fix up a mutable copy, then freeze it into the const member.
            OCIO::ConfigRcPtr copy = copy_config(cfg);
            if (!working_dir.empty())
                copy->setWorkingDir(std::string(working_dir).c_str());
            const char* cfgname = copy->getName();
            name                = (cfgname && *cfgname)
                                      ? Strutil::fmt::format("text:{}", cfgname)
                                      : std::string("text:(anonymous)");
            frozen              = copy;
        }
    } catch (OCIO::Exception& e) {
        error("Error reading OCIO config from text: {}", e.what());
    } catch (...) {
        error("Error reading OCIO config from text");
    }
    OCIO::SetLoggingLevel(oldlog);

    return init_from_config(std::move(frozen), name);
}



bool
ColorConfig::reset(string_view filename)
{
    OIIO::pvt::LoggedTimer logtime("ColorConfig::reset");
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
    if (getImpl()->config_ && !disable_ocio) {
        try {
            OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace(
                std::string(name).c_str());
            if (c)
                return c->getFamily();
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getColorSpaceFamilyByName: {}", e.what());
        }
    }
    return nullptr;
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
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->config_->getNumRoles();
    return 0;
}

const char*
ColorConfig::getRoleByIndex(int index) const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getRoleName(index);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getRoleByIndex: {}", e.what());
    }
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
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getNumLooks();
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getNumLooks: {}", e.what());
    }
    return 0;
}



const char*
ColorConfig::getLookNameByIndex(int index) const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getLookNameByIndex(index);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getLookNameByIndex: {}", e.what());
    }
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
    if (config_ && !disable_builtin_configs && !disable_ocio) {
        try {
            return config_->isColorSpaceLinear(c_str(name),
                                               OCIO::REFERENCE_SPACE_SCENE)
                   || config_->isColorSpaceLinear(c_str(name),
                                                  OCIO::REFERENCE_SPACE_DISPLAY);
        } catch (...) {
            return false;
        }
    }
    return Strutil::iequals(name, "linear")
           || Strutil::istarts_with(name, "linear ")
           || Strutil::istarts_with(name, "linear_")
           || Strutil::istarts_with(name, "lin_")
           || Strutil::iends_with(name, "_linear")
           || Strutil::iends_with(name, "_lin");
}



bool
ColorConfig::isData(string_view name) const
{
    return getImpl()->isData(name);
}



bool
ColorConfig::Impl::isData(string_view name) const
{
    if (const CSInfo* cs = find(name)) {
        return cs->flags() & CSInfo::is_data;
    }
    return false;
}



std::vector<std::string>
ColorConfig::getAliases(string_view color_space) const
{
    std::vector<std::string> result;
    auto config = getImpl()->config_;
    if (config) {
        auto cs = config->getColorSpace(c_str(color_space));
        if (cs) {
            for (int i = 0, e = cs->getNumAliases(); i < e; ++i)
                result.emplace_back(cs->getAlias(i));
        }
    }
    return result;
}



const char*
ColorConfig::getColorSpaceNameByRole(string_view role) const
{
    if (getImpl()->config_ && !disable_ocio) {
        try {
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
            if (!c && Strutil::iequals(role, "srgb")) {
                c = getImpl()->config_->getColorSpace("sRGB - Texture");
                // DBG("Unilaterally substituting {} -> '{}'\n", role,
                //                c->getName());
            }

            if (c) {
                // DBG("found color space {} for role {}\n", c->getName(),
                //                role);
                return c->getName();
            }
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getColorSpaceNameByRole: {}", e.what());
        }
    }

    // No OCIO at build time, or no OCIO configuration at run time
    if (Strutil::iequals(role, "linear")
        || Strutil::iequals(role, "scene_linear"))
        return "linear";

    return nullptr;  // Dunno what role
}



TypeDesc
ColorConfig::getColorSpaceDataType(string_view name, int* bits) const
{
    if (getImpl()->config_ && !disable_ocio) {
        try {
            OCIO::ConstColorSpaceRcPtr c = getImpl()->config_->getColorSpace(
                std::string(name).c_str());
            if (c) {
                OCIO::BitDepth b = c->getBitDepth();
                switch (b) {
                case OCIO::BIT_DEPTH_UNKNOWN: return TypeDesc::UNKNOWN;
                case OCIO::BIT_DEPTH_UINT8: *bits = 8; return TypeDesc::UINT8;
                case OCIO::BIT_DEPTH_UINT10:
                    *bits = 10;
                    return TypeDesc::UINT16;
                case OCIO::BIT_DEPTH_UINT12:
                    *bits = 12;
                    return TypeDesc::UINT16;
                case OCIO::BIT_DEPTH_UINT14:
                    *bits = 14;
                    return TypeDesc::UINT16;
                case OCIO::BIT_DEPTH_UINT16:
                    *bits = 16;
                    return TypeDesc::UINT16;
                case OCIO::BIT_DEPTH_UINT32:
                    *bits = 32;
                    return TypeDesc::UINT32;
                case OCIO::BIT_DEPTH_F16: *bits = 16; return TypeDesc::HALF;
                case OCIO::BIT_DEPTH_F32: *bits = 32; return TypeDesc::FLOAT;
                }
            }
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getColorSpaceDataType: {}", e.what());
        }
    }
    return TypeUnknown;
}



int
ColorConfig::getNumDisplays() const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getNumDisplays();
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getNumDisplays: {}", e.what());
    }
    return 0;
}



const char*
ColorConfig::getDisplayNameByIndex(int index) const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getDisplay(index);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getDisplayNameByIndex: {}", e.what());
    }
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
    if (display.empty())
        display = getDefaultDisplayName();
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getNumViews(
                std::string(display).c_str());
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getNumViews: {}", e.what());
    }
    return 0;
}



const char*
ColorConfig::getViewNameByIndex(string_view display, int index) const
{
    if (display.empty())
        display = getDefaultDisplayName();
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getView(std::string(display).c_str(),
                                               index);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getViewNameByIndex: {}", e.what());
    }
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
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getDefaultDisplay();
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getDefaultDisplayName: {}", e.what());
    }
    return nullptr;
}



const char*
ColorConfig::getDefaultViewName(string_view display) const
{
    if (display.empty() || display == "default")
        display = getDefaultDisplayName();
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getDefaultView(c_str(display));
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getDefaultViewName: {}", e.what());
    }
    return nullptr;
}


const char*
ColorConfig::getDefaultViewName(string_view display,
                                string_view inputColorSpace) const
{
    try {
        if (display.empty() || display == "default")
            display = getDefaultDisplayName();
        if (inputColorSpace.empty() || inputColorSpace == "default")
            inputColorSpace = getImpl()->config_->getColorSpaceFromFilepath(
                c_str(inputColorSpace));
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getDefaultView(c_str(display),
                                                      c_str(inputColorSpace));
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getDefaultViewName: {}", e.what());
    }
    return nullptr;
}


const char*
ColorConfig::getDisplayViewColorSpaceName(const std::string& display,
                                          const std::string& view) const
{
    if (getImpl()->config_ && !disable_ocio) {
        try {
            string_view name = getImpl()->config_->getDisplayViewColorSpaceName(
                c_str(display), c_str(view));
            // Handle certain Shared View cases. Return interned storage, not
            // c_str(display): the argument may be a temporary (string_view
            // callers convert implicitly), and a pointer into it dangles as
            // soon as the caller's full-expression ends.
            if (strcmp(c_str(name), "<USE_DISPLAY_NAME>") == 0)
                return ustring(display).c_str();
            return c_str(name);
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getDisplayViewColorSpaceName: {}", e.what());
        }
    }
    return nullptr;
}



const char*
ColorConfig::getDisplayViewLooks(const std::string& display,
                                 const std::string& view) const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getDisplayViewLooks(display.c_str(),
                                                           view.c_str());
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getDisplayViewLooks: {}", e.what());
    }
    return nullptr;
}



int
ColorConfig::getNumNamedTransforms() const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getNumNamedTransforms();
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getNumNamedTransforms: {}", e.what());
    }
    return 0;
}



const char*
ColorConfig::getNamedTransformNameByIndex(int index) const
{
    try {
        if (getImpl()->config_ && !disable_ocio)
            return getImpl()->config_->getNamedTransformNameByIndex(index);
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in getNamedTransformNameByIndex: {}", e.what());
    }
    return nullptr;
}



std::vector<std::string>
ColorConfig::getNamedTransformNames() const
{
    std::vector<std::string> result;
    for (int i = 0, e = getNumNamedTransforms(); i != e; ++i)
        result.emplace_back(getNamedTransformNameByIndex(i));
    return result;
}



std::vector<std::string>
ColorConfig::getNamedTransformAliases(string_view named_transform) const
{
    std::vector<std::string> result;
    auto config = getImpl()->config_;
    if (config) {
        auto nt = config->getNamedTransform(c_str(named_transform));
        if (nt) {
            for (int i = 0, e = nt->getNumAliases(); i < e; ++i)
                result.emplace_back(nt->getAlias(i));
        }
    }
    return result;
}



std::string
ColorConfig::configname() const
{
    if (getImpl()->config_ && !disable_ocio)
        return getImpl()->configname();
    return "built-in";
}



std::string
ColorConfig::serialize(const SerializeOptions& options) const
{
    OCIO::ConstConfigRcPtr config;
    if (getImpl()->config_ && !disable_ocio)
        config = options.interopified ? getImpl()->interopifiedConfig()
                                      : getImpl()->config_;
    if (!config) {
        getImpl()->error(
            "ColorConfig::serialize: no {}config is available to serialize",
            options.interopified ? "interoperability-repaired " : "");
        return {};
    }
    try {
        std::ostringstream os;
        config->serialize(os);
        return os.str();
    } catch (OCIO::Exception& e) {
        getImpl()->error("ColorConfig::serialize: {}", e.what());
    } catch (...) {
        getImpl()->error(
            "ColorConfig::serialize: unknown error in OpenColorIO serialize");
    }
    return {};
}



bool
ColorConfig::archive(string_view filename, const ArchiveOptions& options) const
{
    OCIO::ConstConfigRcPtr config;
    if (getImpl()->config_ && !disable_ocio)
        config = options.interopified ? getImpl()->interopifiedConfig()
                                      : getImpl()->config_;
    if (!config) {
        getImpl()->error(
            "ColorConfig::archive: no {}config is available to archive",
            options.interopified ? "interoperability-repaired " : "");
        return false;
    }
    try {
        if (options.working_dir.size()) {
            // Archive as if the working directory were the override, without
            // touching the frozen member config.
            OCIO::ConfigRcPtr copy = copy_config(config);
            copy->setWorkingDir(options.working_dir.c_str());
            config = copy;
        }
        if (!config->isArchivable()) {
            getImpl()->error(
                "ColorConfig::archive: config \"{}\" is not archivable "
                "(it needs a working directory, and every search path and "
                "FileTransform source must stay within it){}",
                configname(),
                options.working_dir.empty()
                    ? " -- consider passing ArchiveOptions::working_dir"
                    : "");
            return false;
        }
        OIIO::ofstream out;
        Filesystem::open(out, filename,
                         std::ios_base::out | std::ios_base::binary
                             | std::ios_base::trunc);
        if (!out) {
            getImpl()->error("ColorConfig::archive: could not open \"{}\"",
                             filename);
            return false;
        }
        config->archive(out);
        out.close();
        if (!out) {
            getImpl()->error("ColorConfig::archive: error writing \"{}\"",
                             filename);
            return false;
        }
        return true;
    } catch (OCIO::Exception& e) {
        getImpl()->error("ColorConfig::archive: {}", e.what());
    } catch (...) {
        getImpl()->error(
            "ColorConfig::archive: unknown error in OpenColorIO archive");
    }
    return false;
}



ColorConfig
ColorConfig::evolve(const EvolveOptions& options) const
{
    ColorConfig cc { UninitTag() };

    OCIO::ConstConfigRcPtr base;
    if (getImpl()->config_ && !disable_ocio)
        base = options.reset ? getImpl()->original_config_ : getImpl()->config_;
    // The evolved instance's configname marks its provenance (once -- an
    // evolve chain doesn't stack suffixes).
    std::string name = getImpl()->configname();
    if (!Strutil::ends_with(name, "#evolved"))
        name += "#evolved";

    OCIO::ConstConfigRcPtr modified;
    if (!base) {
        cc.m_impl->error(
            "ColorConfig::evolve: no config is available to evolve");
    } else {
        try {
            OCIO::ConfigRcPtr copy = copy_config(base);
            if (options.working_dir.size())
                copy->setWorkingDir(options.working_dir.c_str());
            for (const auto& kv : options.context)
                copy->addEnvironmentVar(kv.first.c_str(), kv.second.c_str());
            modified = copy;
        } catch (OCIO::Exception& e) {
            cc.m_impl->error("ColorConfig::evolve: {}", e.what());
        } catch (...) {
            cc.m_impl->error(
                "ColorConfig::evolve: unknown error in OpenColorIO");
        }
    }
    // Adopt the modified copy (or, on failure, initialize the usual
    // failed-config fallback state with the error preserved). The evolved
    // instance inherits this config's ORIGINAL as its reset root.
    (void)cc.m_impl->init_from_config(std::move(modified), name,
                                      getImpl()->original_config_);
    return cc;
}



ColorConfigDebugInfo
ColorConfig::get_debug_info(const DebugInfoOptions& /*options*/) const
{
    const Impl* impl = getImpl();
    ColorConfigDebugInfo info;
    info.oiio_version          = OIIO_VERSION_STRING;
    info.ocio_version          = OCIO::GetVersion();
    info.config_name           = configname();
    info.registry_data_version = interop_registry_data_version();
    if (impl->config_ && !disable_ocio) {
        info.structural_cache_id = get_config_cache_id(impl->config_);
        try {
            if (const char* id = impl->config_->getCacheID())
                info.cache_id = id;
        } catch (...) {
        }
    }
    // Interchange discovery: report the existing state, never trigger the
    // lazy bootstrap.
    if (!impl->interopComputed())
        info.interchange_state = ColorInterchangeState::Pending;
    else if (impl->interopIsInteroperable()) {
        info.interchange_state = ColorInterchangeState::Interoperable;
        info.interchange_name  = impl->interopInterchangeName();
    } else
        info.interchange_state = ColorInterchangeState::NotFound;
    info.cache_entries["color processors"] = impl->processorCacheSize();
    info.cache_entries["color processors requested"] = std::size_t(
        std::max(0, impl->processorsRequested()));
    info.cache_entries["color processors created"] = std::size_t(
        std::max(0, impl->processorsCreated()));
    info.cache_entries["fingerprints"]
        = OIIO::pvt::color_space_fingerprint_cache_size();
    info.cache_entries["characterizations"]
        = OIIO::pvt::characterization_cache_size();
    return info;
}



std::string
ColorConfigDebugInfo::to_string() const
{
    using Strutil::fmt::format;
    std::string out;
    out += format("OpenImageIO {} / OpenColorIO {}\n", oiio_version,
                  ocio_version);
    out += format("config: \"{}\"\n", config_name);
    out += format("  structural cache id: {}\n",
                  structural_cache_id.size() ? structural_cache_id : "(none)");
    out += format("  cache id (context folded in): {}\n",
                  cache_id.size() ? cache_id : "(none)");
    switch (interchange_state) {
    case ColorInterchangeState::Pending:
        out += "interchange discovery: pending (not yet queried)\n";
        break;
    case ColorInterchangeState::Interoperable:
        out += format(
            "interchange discovery: interoperable (scene interchange \"{}\")\n",
            interchange_name);
        break;
    case ColorInterchangeState::NotFound:
        out += "interchange discovery: no scene interchange identified\n";
        break;
    }
    out += format("interop registry data: {}\n", registry_data_version);
    out += "caches:\n";
    for (const auto& kv : cache_entries)
        out += format("  {}: {} entries\n", kv.first, kv.second);
    return out;
}



void
ColorConfig::clear_caches(const ClearCachesOptions& /*options*/) const
{
    // This instance's processor cache and per-query hints.
    getImpl()->clearInstanceCaches();
    // The process-global memo entries scoped to this config's structural
    // identity. Shared process data not scoped to it (the built-in interop
    // registry and its fingerprint index) is untouched.
    if (getImpl()->config_ && !disable_ocio) {
        const std::string cfgId = get_config_cache_id(getImpl()->config_);
        if (cfgId.size()) {
            fingerprint_cache_erase_config(cfgId);
            characterization_cache_erase_config(cfgId);
        }
    }
}



string_view
ColorConfig::resolve(string_view name) const
{
    return getImpl()->resolve(name);
}



string_view
ColorConfig::resolve(string_view name, string_view failover) const
{
    return getImpl()->resolve(name, failover);
}



namespace {

// Helpers for the interop-ID resolution tiers layered onto resolve(). They
// only read the OCIO config and the pure grammar/sanitization functions from
// color_pvt.h; none of them touch the interoperability bootstrap or the
// fingerprint engine. Every string_view they return is backed by an OCIO-owned
// color space name (stable for the life of the config), never a temporary.

// The UNIQUE owner of a sanitized base token among the config's color space
// names and aliases (across all reference types and visibilities): the owning
// space's name when exactly one space claims `token`, empty when none or more
// than one do. The sanitizer is many-to-one ("Foo Bar" and "foo_bar" both map
// to "foo_bar"), so this is the shared ambiguity guard for BOTH sides of the
// config-local id form: generation refuses to serialize an id that resolution
// could not uniquely reverse, and resolution refuses to guess among colliding
// spaces (the never-guess rule). Linear scan per query; cache an inverted map
// if either side ever shows up hot.
string_view
unique_space_for_sanitized_token(const OCIO::ConstConfigRcPtr& config,
                                 const std::string& token)
{
    int n = 0;
    try {
        n = config->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                      OCIO::COLORSPACE_ALL);
    } catch (...) {
        return {};
    }
    string_view found;
    for (int i = 0; i < n; ++i) {
        const char* nm
            = config->getColorSpaceNameByIndex(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                               OCIO::COLORSPACE_ALL, i);
        if (!nm || !*nm)
            continue;
        OCIO::ConstColorSpaceRcPtr cs;
        try {
            cs = config->getColorSpace(nm);
        } catch (...) {
            continue;
        }
        if (!cs)
            continue;
        bool claims = OIIO::pvt::sanitize_id_token(cs->getName()) == token;
        for (int a = 0, ae = cs->getNumAliases(); !claims && a < ae; ++a)
            claims = OIIO::pvt::sanitize_id_token(cs->getAlias(a)) == token;
        if (!claims)
            continue;
        if (!found.empty())
            return {};  // a second owner: the token is ambiguous
        found = cs->getName();
    }
    return found;
}

// Tier 1a'' -- the config-local form "<config>:local:<base>". Resolves only
// when the id parses as outer:local:base and `outer` sanitizes to this config's
// own name; then it matches `base` against every color space's sanitized name
// or alias (across all reference types and visibilities), refusing an
// ambiguous token via the unique-owner guard above. Deliberately never
// consults a color space's interop_id attribute -- that is tier 1c's job.
string_view
resolve_local_namespace(const OCIO::ConstConfigRcPtr& config, string_view name)
{
    OIIO::pvt::InteropIdParts parts = OIIO::pvt::parse_interop_id(
        std::string(name));
    if (parts.form != OIIO::pvt::InteropIdForm::OUTER_INNER_BASE
        || parts.inner != "local")
        return {};
    const char* cfgname = config->getName();
    if (!cfgname || !*cfgname)
        return {};
    if (OIIO::pvt::sanitize_id_token(cfgname) != parts.outer)
        return {};
    return unique_space_for_sanitized_token(config, parts.base);
}

// Tier 1c -- match `name` against a color space's explicit interop_id attribute
// (OCIO 2.5+). A match requires the attribute to equal the query exactly, or to
// match with exactly ONE side's leftmost namespace stripped (never both -- so
// "oiio:x" does not false-match a query for "ocio:x" just because both strip to
// "x"). Utility tokens (data/unknown/bypass) are excluded from this lookup
// entirely: declaring interop_id: bypass must not make a space reachable by
// querying "bypass". Attributes in the reserved `local` namespace are
// likewise excluded (see below).
string_view
resolve_explicit_interop_id(const OCIO::ConstConfigRcPtr& config,
                            string_view name)
{
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
    std::string id(name);
    std::string id_stripped = OIIO::pvt::strip_leftmost_namespace(id);
    // Linear scan over the catalog; add a cached inverted map if this tier gets hot.
    // only fires when the direct/stripped/local tiers all miss (a rare path),
    // so a per-query scan is cheaper than maintaining a cache. Build the map if
    // this ever shows up hot.
    int n = 0;
    try {
        n = config->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                      OCIO::COLORSPACE_ALL);
    } catch (...) {
        return {};
    }
    for (int i = 0; i < n; ++i) {
        const char* nm
            = config->getColorSpaceNameByIndex(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                               OCIO::COLORSPACE_ALL, i);
        if (!nm || !*nm)
            continue;
        OCIO::ConstColorSpaceRcPtr cs;
        try {
            cs = config->getColorSpace(nm);
        } catch (...) {
            continue;
        }
        if (!cs)
            continue;
        const char* iid = cs->getInteropID();
        if (!iid || !*iid)
            continue;
        std::string attr(iid);
        if (OIIO::pvt::is_utility_interop_id(attr))
            continue;
        // The `local` namespace is reserved end-to-end for the config-local
        // "<config>:local:<base>" form. A declared attribute whose leftmost
        // segment is `local` must never satisfy this tier: honoring a
        // grammar-legal "local:x" here would let one config poach another
        // config's private local IDs through the stripped-attribute match
        // below ("othercfg:local:x" strips to "local:x").
        if (attr == "local" || Strutil::starts_with(attr, "local:"))
            continue;
        if (attr == id || attr == id_stripped
            || OIIO::pvt::strip_leftmost_namespace(attr) == id)
            return cs->getName();
    }
#else
    (void)config;
    (void)name;
#endif
    return {};
}

// The color space name that `token` (a name, alias, or role) resolves to in
// `config`, or empty if it resolves to nothing.
std::string
resolve_token_colorspace_name(const OCIO::ConstConfigRcPtr& config,
                              const char* token)
{
    try {
        OCIO::ConstColorSpaceRcPtr c = config->getColorSpace(token);
        return c ? std::string(c->getName()) : std::string();
    } catch (...) {
        return std::string();
    }
}

// Whether a data color space `cs` (name `nm`) identifies as `token` -- either
// via its explicit interop_id attribute (OCIO 2.5+), or because `token`'s
// name/alias/role resolution (precomputed as `token_target`) lands on it.
bool
data_space_identifies_as(const OCIO::ConstColorSpaceRcPtr& cs, const char* nm,
                         string_view token, const std::string& token_target)
{
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
    const char* iid = cs->getInteropID();
    if (iid && *iid && token == string_view(iid))
        return true;
#endif
    return !token_target.empty() && token_target == nm;
}

// Utility-token preference for the literal queries "data" and "bypass": rank
// every data color space and return the lowest-ranked one. rank 0 = identifies
// as the requested token, rank 1 = plain data space (no identity), rank 2 =
// identifies as the other token (data<->bypass), rank 3 = identifies as
// "unknown"; lowest rank wins and a rank-0 hit short-circuits. ("unknown" is
// intentionally not routed here -- it only ever resolves as a literal
// name/alias, handled by tier 1a.)
string_view
resolve_data_utility(const OCIO::ConstConfigRcPtr& config,
                     string_view requested)
{
    const char* req          = requested == "data" ? "data" : "bypass";
    const char* other        = requested == "data" ? "bypass" : "data";
    std::string req_target   = resolve_token_colorspace_name(config, req);
    std::string other_target = resolve_token_colorspace_name(config, other);
    std::string unk_target   = resolve_token_colorspace_name(config, "unknown");

    const char* best = nullptr;
    int best_rank    = 4;  // worse than any real rank (0..3)
    int n            = 0;
    try {
        n = config->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                      OCIO::COLORSPACE_ALL);
    } catch (...) {
        return {};
    }
    for (int i = 0; i < n; ++i) {
        const char* nm
            = config->getColorSpaceNameByIndex(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                               OCIO::COLORSPACE_ALL, i);
        if (!nm || !*nm)
            continue;
        OCIO::ConstColorSpaceRcPtr cs;
        try {
            cs = config->getColorSpace(nm);
        } catch (...) {
            continue;
        }
        if (!cs || !cs->isData())
            continue;
        // A one-space OCIO::Config::CreateRaw() config is nothing but a
        // synthetic data space named "raw". Skip that space so an empty/raw
        // config does not spuriously resolve "data"/"bypass" to it -- but key
        // the skip on the config's single-colorspace shape (n == 1), not the
        // name alone: a real config may legitimately hold a data space named
        // "Raw" alongside others, and that one is a valid target. A role
        // explicitly naming the requested token still wins.
        if (n == 1 && Strutil::iequals(nm, "raw") && req_target != nm)
            continue;
        int rank;
        if (data_space_identifies_as(cs, nm, req, req_target))
            rank = 0;
        else if (data_space_identifies_as(cs, nm, other, other_target))
            rank = 2;
        else if (data_space_identifies_as(cs, nm, "unknown", unk_target))
            rank = 3;
        else
            rank = 1;  // plain data space, no identity
        if (rank < best_rank) {
            best_rank = rank;
            best      = cs->getName();
            if (rank == 0)
                break;
        }
    }
    return best ? string_view(best) : string_view();
}

}  // namespace



string_view
ColorConfig::Impl::resolve_name_tier1a(string_view name) const
{
    OCIO::ConstConfigRcPtr config = config_;
    if (config && !disable_ocio) {
        try {
            OCIO::ConstColorSpaceRcPtr cs = config->getColorSpace(c_str(name));
            if (cs)
                return cs->getName();
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in resolve: {}", e.what());
        }
    }
    // OCIO did not know this name as a color space, role, or alias.

    // Maybe it's an informal alias of common names?
    spin_rw_write_lock lock(m_mutex);
    if ((Strutil::iequals(name, "sRGB")
         || Strutil::iequals(name, "srgb_rec709_scene"))
        && !srgb_alias.empty())
        return srgb_alias;
    if ((Strutil::iequals(name, "lin_srgb")
         || Strutil::iequals(name, "lin_rec709")
         || Strutil::iequals(name, "lin_rec709_scene")
         || Strutil::iequals(name, "linear"))
        && lin_srgb_alias.size())
        return lin_srgb_alias;
    if ((Strutil::iequals(name, "ACEScg")
         || Strutil::iequals(name, "lin_ap1_scene"))
        && !ACEScg_alias.empty())
        return ACEScg_alias;
    if (Strutil::iequals(name, "scene_linear") && !scene_linear_alias.empty()) {
        return scene_linear_alias;
    }
    if (Strutil::iequals(name, "Rec709") && Rec709_alias.size())
        return Rec709_alias;

    return {};
}



string_view
ColorConfig::Impl::resolve_syntactic(string_view name) const
{
    // Tier 1a: direct OCIO color space / role / alias, then informal aliases.
    if (string_view r = resolve_name_tier1a(name); !r.empty())
        return r;

    // Tier 1a': stripped-namespace retry. Only meaningful when the name carries
    // a namespace to strip. strip_leftmost_namespace() consumes exactly one
    // leading "<ns>:"; note "my-studio::srgb" strips to ":srgb" (the blank inner
    // colon survives), which deliberately will NOT match "srgb".
    if (name.find(':') != string_view::npos) {
        std::string stripped = OIIO::pvt::strip_leftmost_namespace(
            std::string(name));
        if (string_view r = resolve_name_tier1a(stripped); !r.empty())
            return r;
    }

    // The remaining tiers all consult the OCIO config directly.
    if (config_ && !disable_ocio) {
        // Tier 1a'': config-local form "<config>:local:<space>".
        if (string_view r = resolve_local_namespace(config_, name); !r.empty())
            return r;

        // Tier 1c: a color space's explicit interop_id attribute (OCIO 2.5+).
        if (string_view r = resolve_explicit_interop_id(config_, name);
            !r.empty())
            return r;

        // Utility tokens: "data"/"bypass" resolve to a ranked data color space.
        // "unknown" is intentionally not ranked -- it only ever resolves via the
        // literal name/alias match already attempted in tier 1a.
        if (name == "data" || name == "bypass")
            if (string_view r = resolve_data_utility(config_, name); !r.empty())
                return r;
    }

    return {};
}



string_view
ColorConfig::Impl::resolve(string_view name, string_view failover) const
{
    // Every syntactic (fingerprint-free) tier first.
    if (string_view r = resolve_syntactic(name); !r.empty())
        return r;

    if (config_ && !disable_ocio) {
        // Tier 2: registry equivalence (fingerprint match). Canonicalize the id
        // through the built-in interop identities registry and return this
        // config's OWN equivalent simple color space -- the user's own space
        // wins over building any cross-config processor (that is a later,
        // separate feature; this tier returns only names). Utility tokens have
        // no registry fingerprint and miss automatically. This is the first tier
        // that fingerprints, so it lazily builds the process-global registry
        // index and this config's probe/bootstrap state on first use; every
        // earlier tier -- and ColorConfig construction -- stays fingerprint-free.
        // The const_cast reaches the memoizing (logically-const) fingerprint
        // caches, mirroring getImpl()'s non-const handle onto a const config.
        if (string_view r = const_cast<ColorConfig::Impl*>(this)
                                ->resolve_registry_equivalence(name);
            !r.empty())
            return r;
    }

    // Total miss: the caller decides. The 1-arg public resolve() passes the
    // input name, preserving OIIO's historical passthrough so callers that
    // assumed identity resolution keep working.
    return failover;
}



bool
ColorConfig::Impl::equivalent_syntactic(string_view color_space1,
                                        string_view color_space2) const
{
    // Mirrors ColorConfig::equivalent() exactly, except resolution is
    // resolve_syntactic() -- so a match can come from names, roles, aliases
    // (formal or informal), a declared interop_id, or the cheap
    // classification flags, but NEVER from the fingerprint tier.
    if (color_space1.empty() || color_space2.empty())
        return false;
    if (Strutil::iequals(color_space1, color_space2))
        return true;

    string_view r1 = resolve_syntactic(color_space1);
    if (!r1.empty())
        color_space1 = r1;
    string_view r2 = resolve_syntactic(color_space2);
    if (!r2.empty())
        color_space2 = r2;
    if (Strutil::iequals(color_space1, color_space2))
        return true;

    const int mask = CSInfo::is_srgb | CSInfo::is_lin_srgb | CSInfo::is_ACEScg
                     | CSInfo::is_Rec709;
    const CSInfo* csi1 = find(color_space1);
    const CSInfo* csi2 = find(color_space2);
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



bool
equivalent_colorspace(string_view a, string_view b)
{
    return ColorConfig::default_colorconfig().equivalent(a, b);
}



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
    std::string lenient_fallback_msg;

    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(inputColorSpace, outputColorSpace, context_key,
                              context_value);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle) {
        // A cached lenient cross-config fallback must behave exactly like
        // its first computation: re-signal its continue-message so this
        // call's outcome (pixels will NOT be converted) is observable per
        // call, not lost to whoever consumed the shared error string first.
        std::string fallback = getImpl()->lenient_fallback_message(
            handle.get());
        if (fallback.size())
            getImpl()->error("{}", fallback);
        return handle;
    }

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
        auto config = getImpl()->config_;
        try {
            auto context = config->getCurrentContext();
            auto keys    = Strutil::splits(context_key, ",");
            auto values  = Strutil::splits(context_value, ",");
            if (keys.size() && values.size() && keys.size() == values.size()) {
                OCIO::ContextRcPtr ctx = context->createEditableCopy();
                for (size_t i = 0; i < keys.size(); ++i)
                    ctx->setStringVar(keys[i].c_str(), values[i].c_str());
                context = ctx;
            }

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
            try {
                handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
            } catch (OCIO::Exception& e) {
                getImpl()->error("Exception from OCIO: {}", e.what());
            }
            // DBG("OCIO processor '{}' -> '{}' is NOT NoOp, handle = {}\n",
            //                inputColorSpace, outputColorSpace, (bool)handle);
        } else if (!p) {
            // Local resolution failed (OCIO threw for a name this config does
            // not define). If a requested name is a registry-known interop
            // identity the config lacks, reconcile the conversion across configs
            // through the interop bridge. This is deliberately on by default
            // (observable via debug log), with OCIO strict_parsing as the
            // opt-out. This may return a bridged processor, a lenient
            // pass-through fallback (non-strict parsing), or nothing --
            // leaving today's error -- when the feature does not apply or
            // strict parsing is on.
            std::string reconciled;
            ColorProcessorHandle bridged = getImpl()->reconcile_cross_config(
                inputColorSpace, outputColorSpace, reconciled);
            if (bridged) {
                handle = bridged;
                if (reconciled.empty())
                    getImpl()->clear_error();  // clean bridge success
                pending_error = reconciled;    // empty on success; a
                                               // continue-message on fallback
                lenient_fallback_msg = reconciled;  // ditto (registered below)
            } else if (reconciled.size()) {
                pending_error = reconciled;  // strict hard error: why+how-to-fix
            }
            // else: reconciliation declined -- keep today's OCIO error.
        }
    }

    if (!handle && p) {
        // If we found a processor from OCIO, even if it was a NoOp, and we
        // still don't have a better idea, return it.
        try {
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (OCIO::Exception& e) {
            getImpl()->error("Exception from OCIO: {}", e.what());
        }
    }

    if (pending_error.size())
        getImpl()->error("{}", pending_error);

    return getImpl()->addproc(prockey, handle,
                              lenient_fallback_msg.size()
                                  ? &lenient_fallback_msg
                                  : nullptr);
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
                              ustring() /*view*/, ustring() /*file*/,
                              ustring() /*namedtransform*/, inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_ && !disable_ocio) {
        OCIO::ConstConfigRcPtr config = getImpl()->config_;
        try {
            OCIO::LookTransformRcPtr transform = OCIO::LookTransform::Create();
            transform->setLooks(looks.c_str());
            OCIO::TransformDirection dir;
            if (inverse) {
                // The TRANSFORM_DIR_INVERSE applies an inverse for the
                // end-to-end transform, which would otherwise do dst->inv
                // look -> src.  This is an unintuitive result for the artist
                // (who would expect in, out to remain unchanged), so we
                // account for that here by flipping src/dst
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

            // Get the processor corresponding to this transform.
            OCIO::ConstProcessorRcPtr p;
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
        view = getDefaultViewName(display, inputColorSpace);
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(inputColorSpace, ustring() /*outputColorSpace*/,
                              context_key, context_value, looks, display, view,
                              ustring() /*file*/, ustring() /*namedtransform*/,
                              inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle) {
        // A cached lenient cross-config fallback must behave exactly like
        // its first computation: re-signal its continue-message (see
        // createColorProcessor for the identical pattern).
        std::string fallback = getImpl()->lenient_fallback_message(
            handle.get());
        if (fallback.size())
            getImpl()->error("{}", fallback);
        return handle;
    }
    std::string lenient_fallback_msg;

    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_ && !disable_ocio) {
        OCIO::ConstConfigRcPtr config = getImpl()->config_;
        std::string pending_error;
        try {
            auto transform = OCIO::DisplayViewTransform::Create();
            auto legacy_viewing_pipeline = OCIO::LegacyViewingPipeline::Create();
            OCIO::TransformDirection dir = inverse
                                               ? OCIO::TRANSFORM_DIR_INVERSE
                                               : OCIO::TRANSFORM_DIR_FORWARD;
            transform->setSrc(inputColorSpace.c_str());
            transform->setDisplay(display.c_str());
            transform->setView(view.c_str());
            transform->setDirection(dir);
            legacy_viewing_pipeline->setDisplayViewTransform(transform);
            if (looks.size()) {
                legacy_viewing_pipeline->setLooksOverride(looks.c_str());
                legacy_viewing_pipeline->setLooksOverrideEnabled(true);
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

            // Get the processor corresponding to this transform.
            OCIO::ConstProcessorRcPtr p;
            p = legacy_viewing_pipeline->getProcessor(config, context);
            getImpl()->clear_error();
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (OCIO::Exception& e) {
            // Local resolution failed (OCIO threw for an input name this config
            // does not define). If the input is a registry-known interop
            // identity the config lacks, reconcile the display transform across
            // configs through the interop bridge -- on by default, observable
            // via debug log, with OCIO strict_parsing as the opt-out -- mirroring
            // the color-space route. Crucially, on failure this takes the
            // strict-aware fallback -- it does NOT silently continue with
            // setSrc(ROLE_SCENE_LINEAR) as an earlier iteration of this display
            // path did.
            pending_error = e.what();
            std::string reconciled;
            ColorProcessorHandle bridged
                = getImpl()->reconcile_cross_config_display(inputColorSpace,
                                                            display, view,
                                                            inverse,
                                                            reconciled);
            if (bridged) {
                handle = bridged;
                if (reconciled.empty())
                    getImpl()->clear_error();  // clean bridge success
                pending_error = reconciled;    // empty on success; a
                                               // continue-message on fallback
                lenient_fallback_msg = reconciled;  // ditto (registered below)
            } else if (reconciled.size()) {
                pending_error = reconciled;  // strict hard error: why+how-to-fix
            }
            // else: reconciliation declined -- keep today's OCIO error.
        } catch (...) {
            getImpl()->error(
                "An unknown error occurred in OpenColorIO, getProcessor");
        }
        if (pending_error.size())
            getImpl()->error("{}", pending_error);
    }

    return getImpl()->addproc(prockey, handle,
                              lenient_fallback_msg.size()
                                  ? &lenient_fallback_msg
                                  : nullptr);
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
                              ustring() /*display*/, ustring() /*view*/,
                              ustring() /*file*/, name, inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    OCIO::ConstConfigRcPtr config = getImpl()->config_;
    // If no config was found, config_ will be null. But that shouldn't
    // stop us for a filetransform, which doesn't need color spaces anyway.
    // Just use the default current config, it'll be freed when we exit.
    if (!config)
        config = ocio_current_config;
    if (config) {
        try {
            OCIO::FileTransformRcPtr transform = OCIO::FileTransform::Create();
            transform->setSrc(name.c_str());
            transform->setInterpolation(OCIO::INTERP_BEST);
            OCIO::TransformDirection dir(inverse ? OCIO::TRANSFORM_DIR_INVERSE
                                                 : OCIO::TRANSFORM_DIR_FORWARD);
            OCIO::ConstContextRcPtr context = config->getCurrentContext();
            // Get the processor corresponding to this transform.
            OCIO::ConstProcessorRcPtr p;
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

    return getImpl()->addproc(prockey, handle);
}



ColorProcessorHandle
ColorConfig::createNamedTransform(string_view name, bool inverse,
                                  string_view context_key,
                                  string_view context_value) const
{
    return createNamedTransform(ustring(name), inverse, ustring(context_key),
                                ustring(context_value));
}



ColorProcessorHandle
ColorConfig::createNamedTransform(ustring name, bool inverse,
                                  ustring context_key,
                                  ustring context_value) const
{
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(ustring() /*inputColorSpace*/,
                              ustring() /*outputColorSpace*/, context_key,
                              context_value, ustring() /*looks*/,
                              ustring() /*display*/, ustring() /*view*/,
                              ustring() /*file*/, name, inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_ && !disable_ocio) {
        OCIO::ConstConfigRcPtr config = getImpl()->config_;
        try {
            auto transform = config->getNamedTransform(name.c_str());
            OCIO::TransformDirection dir(inverse ? OCIO::TRANSFORM_DIR_INVERSE
                                                 : OCIO::TRANSFORM_DIR_FORWARD);
            auto context = config->getCurrentContext();
            auto keys    = Strutil::splits(context_key, ",");
            auto values  = Strutil::splits(context_value, ",");
            if (keys.size() && values.size() && keys.size() == values.size()) {
                OCIO::ContextRcPtr ctx = context->createEditableCopy();
                for (size_t i = 0; i < keys.size(); ++i)
                    ctx->setStringVar(keys[i].c_str(), values[i].c_str());
                context = ctx;
            }

            // Get the processor corresponding to this transform.
            OCIO::ConstProcessorRcPtr p;
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
    if (getImpl() && getImpl()->config_) {
        try {
            std::string s(str);
            string_view r = getImpl()->config_->getColorSpaceFromFilepath(
                s.c_str());
            return r;
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getColorSpaceFromFilepath: {}", e.what());
        }
    }
    // Fall back on parseColorSpaceFromString
    return parseColorSpaceFromString(str);
}

string_view
ColorConfig::getColorSpaceFromFilepath(string_view str, string_view default_cs,
                                       bool cs_name_match) const
{
    if (getImpl() && getImpl()->config_) {
        try {
            std::string s(str);
            string_view r = getImpl()->config_->getColorSpaceFromFilepath(
                s.c_str());
            if (!getImpl()->config_->filepathOnlyMatchesDefaultRule(s.c_str()))
                return r;
        } catch (OCIO::Exception& e) {
            DBG("OCIO exception in getColorSpaceFromFilepath: {}", e.what());
        }
    }
    if (cs_name_match) {
        string_view parsed = parseColorSpaceFromString(str);
        if (parsed.size())
            return parsed;
    }
    return default_cs;
}

bool
ColorConfig::filepathOnlyMatchesDefaultRule(string_view str) const
{
    try {
        return getImpl()->config_->filepathOnlyMatchesDefaultRule(c_str(str));
    } catch (OCIO::Exception& e) {
        DBG("OCIO exception in filepathOnlyMatchesDefaultRule: {}", e.what());
    }
    return false;
}

string_view
ColorConfig::parseColorSpaceFromString(string_view str) const
{
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
// Color-space classification: the "simple" transform allowlist and the lazy
// per-space analysis pass that sets the CSInfo classification bits.

// Shared classification of atomic (non-structural) transform types. Returns
// true when the transform is simple enough for interop matching. GROUP,
// FILE, and COLORSPACE are structural -- they need recursive or deferred
// handling that differs by caller, so callers handle those themselves. This
// is the single policy switch shared by the recursive authored-transform
// scan (containsBlockableTransform) and, in the future, op-by-op inspection
// of a realized GroupTransform.
bool
isSimpleAtomicTransform(const OCIO::ConstTransformRcPtr& transform)
{
    using namespace OCIO;
    if (!transform)
        return true;
    switch (transform->getTransformType()) {
    case TRANSFORM_TYPE_LUT3D:
    case TRANSFORM_TYPE_CDL:
    case TRANSFORM_TYPE_LOOK:
    case TRANSFORM_TYPE_DISPLAY_VIEW: return false;

    case TRANSFORM_TYPE_BUILTIN: {
        auto builtin = DynamicPtrCast<const BuiltinTransform>(transform);
        if (builtin) {
            string_view style(builtin->getStyle() ? builtin->getStyle() : "");
            // ACES rendering pipelines (output + LMT) carry rendering, tone
            // mapping, and look logic that defeats the primaries+TF
            // fingerprint. "DISPLAY - CIE-XYZ-D65_to_*" builtins, by
            // contrast, are pure display encodings (matrix + transfer
            // function) and fingerprint cleanly -- keep them simple.
            if (Strutil::starts_with(style, "ACES-OUTPUT")
                || Strutil::starts_with(style, "ACES-LMT"))
                return false;
        }
        return true;
    }

    case TRANSFORM_TYPE_FIXED_FUNCTION: {
#if OCIO_VERSION_HEX <= MAKE_OCIO_VERSION_HEX(2, 4, 0)
        return false;
#else
        auto ff = DynamicPtrCast<const FixedFunctionTransform>(transform);
        if (ff) {
            const auto style = ff->getStyle();
            if (style == FIXED_FUNCTION_LIN_TO_DOUBLE_LOG
                || style == FIXED_FUNCTION_LIN_TO_GAMMA_LOG)
                return true;
        }
        return false;
#endif
    }

    case TRANSFORM_TYPE_MATRIX:
    case TRANSFORM_TYPE_RANGE:
    case TRANSFORM_TYPE_EXPONENT:
    case TRANSFORM_TYPE_EXPONENT_WITH_LINEAR:
    case TRANSFORM_TYPE_LOG:
    case TRANSFORM_TYPE_LOG_AFFINE:
    case TRANSFORM_TYPE_LOG_CAMERA:
    case TRANSFORM_TYPE_ALLOCATION:
    case TRANSFORM_TYPE_LUT1D:
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 3, 0)
    case TRANSFORM_TYPE_GRADING_RGB_CURVE:
#endif
        return true;

    default:
        // Unknown/unclassified transform types are not simple.
        return false;
    }
}


namespace {
using namespace OCIO;


// Does any string authored into this transform reference a context var, or
// could a FileTransform in it resolve through a context-dependent search
// path? Direct (non-recursive) scan of the authored transform only, except
// recursing through GROUP.
// For now a '$'-substring scan stands in for per-search-path-entry analysis;
// upgrade to per-entry var analysis if configs with mixed var/non-var search
// paths need finer verdicts.
bool
transformUsesContextVars(const ConstTransformRcPtr& transform,
                         bool search_path_has_vars)
{
    if (!transform)
        return false;
    auto has_var = [](const char* s) { return s && Strutil::contains(s, "$"); };
    switch (transform->getTransformType()) {
    case TRANSFORM_TYPE_FILE: {
        auto ft = DynamicPtrCast<const FileTransform>(transform);
        if (!ft)
            return false;
        return has_var(ft->getSrc()) || has_var(ft->getCCCId())
               || search_path_has_vars;
    }
    case TRANSFORM_TYPE_GROUP: {
        auto gt = DynamicPtrCast<const GroupTransform>(transform);
        for (int i = 0, e = gt ? gt->getNumTransforms() : 0; i < e; ++i)
            if (transformUsesContextVars(gt->getTransform(i),
                                         search_path_has_vars))
                return true;
        return false;
    }
    case TRANSFORM_TYPE_COLORSPACE: {
        auto cst = DynamicPtrCast<const ColorSpaceTransform>(transform);
        if (!cst)
            return false;
        return has_var(cst->getSrc()) || has_var(cst->getDst());
    }
    default: return false;
    }
}

bool
fileTransformIsBlockable(const ConstFileTransformRcPtr& fileTransform)
{
    if (!fileTransform) {
        return true;
    }
    const char* src = fileTransform->getSrc();
    if (!src || !*src) {
        return true;
    }
    if (Strutil::iends_with(src, ".spi1d")
        || Strutil::iends_with(src, ".spimtx")) {
        return false;
    }
    return true;
}

bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstContextRcPtr& context,
                           const ConstTransformRcPtr& transform,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit);
bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstContextRcPtr& context, const char* name,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit);
bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstTransformRcPtr& transform,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit);

// Walk every color space, keeping the ones with only simple transforms.
// keep/omit memoize spaces already scanned so referenced sub-transforms
// aren't rescanned.
std::unordered_set<std::string>
scan_simple_color_space_names(const ConstConfigRcPtr& config)
{
    std::unordered_set<std::string> keep;
    if (!config) {
        return keep;
    }

    std::unordered_set<std::string> omit;
    ConstContextRcPtr ctx = config->getCurrentContext();

    const int n = config->getNumColorSpaces(SEARCH_REFERENCE_SPACE_ALL,
                                            COLORSPACE_ALL);
    for (int i = 0; i < n; ++i) {
        const char* name
            = config->getColorSpaceNameByIndex(SEARCH_REFERENCE_SPACE_ALL,
                                               COLORSPACE_ALL, i);
        if (!name || !*name) {
            continue;
        }
        if (keep.count(name) || omit.count(name)) {
            continue;
        }

        if (containsBlockableTransform(config, ctx, name, keep, omit)) {
            omit.insert(name);
        } else {
            keep.insert(name);
        }
    }

    return keep;
}

bool
colorSpaceHasBlockableTransform(const ConstConfigRcPtr& config,
                                const ConstColorSpaceRcPtr& cs,
                                std::unordered_set<std::string>& keep,
                                std::unordered_set<std::string>& omit)
{
    if (!cs) {
        return true;
    }
    const char* csName = cs->getName();
    if (cs->isData()) {
        if (csName && *csName)
            omit.insert(csName);
        return true;
    }
    if (csName && keep.count(csName)) {
        return false;
    }

    ConstTransformRcPtr toRef = cs->getTransform(COLORSPACE_DIR_TO_REFERENCE);
    if (toRef && containsBlockableTransform(config, toRef, keep, omit)) {
        if (csName && *csName)
            omit.insert(csName);
        return true;
    }

    ConstTransformRcPtr fromRef = cs->getTransform(
        COLORSPACE_DIR_FROM_REFERENCE);
    if (fromRef && containsBlockableTransform(config, fromRef, keep, omit)) {
        if (csName && *csName)
            omit.insert(csName);
        return true;
    }

    if (csName && *csName)
        keep.insert(csName);
    return false;
}

bool
namedTransformHasBlockableTransform(const ConstConfigRcPtr& config,
                                    const ConstNamedTransformRcPtr& nt,
                                    std::unordered_set<std::string>& keep,
                                    std::unordered_set<std::string>& omit)
{
    if (!nt) {
        return true;
    }
    ConstTransformRcPtr fwd = nt->getTransform(TRANSFORM_DIR_FORWARD);
    if (fwd && containsBlockableTransform(config, fwd, keep, omit)) {
        return true;
    }
    ConstTransformRcPtr rev = nt->getTransform(TRANSFORM_DIR_INVERSE);
    if (rev && containsBlockableTransform(config, rev, keep, omit)) {
        return true;
    }
    return false;
}

bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstContextRcPtr& context, const char* name,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit)
{
    if (!name || !*name) {
        return true;
    }
    ConstContextRcPtr ctx = context ? context : config->getCurrentContext();
    auto name_cs          = ctx->resolveStringVar(c_str(name));


    ConstColorSpaceRcPtr cs = config->getColorSpace(c_str(name_cs));
    if (cs) {
        if (omit.count(c_str(cs->getName()))) {
            return true;
        }
        if (keep.count(c_str(cs->getName()))) {
            return false;
        }
        return colorSpaceHasBlockableTransform(config, cs, keep, omit);
    }

    ConstNamedTransformRcPtr nt = config->getNamedTransform(c_str(name_cs));
    if (!nt) {
        return true;
    }
    return namedTransformHasBlockableTransform(config, nt, keep, omit);
}

bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstTransformRcPtr& transform,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit)
{
    return containsBlockableTransform(config, config->getCurrentContext(),
                                      transform, keep, omit);
}

bool
containsBlockableTransform(const ConstConfigRcPtr& config,
                           const ConstContextRcPtr& context,
                           const ConstTransformRcPtr& transform,
                           std::unordered_set<std::string>& keep,
                           std::unordered_set<std::string>& omit)
{
    if (!transform) {
        return false;
    }

    ConstContextRcPtr ctx = context ? context : config->getCurrentContext();

    switch (transform->getTransformType()) {
    case TRANSFORM_TYPE_FILE: {
        ConstFileTransformRcPtr ft = DynamicPtrCast<const FileTransform>(
            transform);
        return fileTransformIsBlockable(ft);
    }
    case TRANSFORM_TYPE_GROUP: {
        ConstGroupTransformRcPtr gt = DynamicPtrCast<const GroupTransform>(
            transform);
        if (!gt)
            return false;
        for (int i = 0, e = gt->getNumTransforms(); i < e; ++i) {
            if (containsBlockableTransform(config, ctx, gt->getTransform(i),
                                           keep, omit)) {
                return true;
            }
        }
        return false;
    }
    case TRANSFORM_TYPE_COLORSPACE: {
        ConstColorSpaceTransformRcPtr cst
            = DynamicPtrCast<const ColorSpaceTransform>(transform);
        if (!cst) {
            return true;
        }

        const char* src = cst->getSrc();
        const char* dst = cst->getDst();

        auto src_cs_name            = ctx->resolveStringVar(c_str(src));
        auto dst_cs_name            = ctx->resolveStringVar(c_str(dst));
        ConstColorSpaceRcPtr src_cs = config->getColorSpace(c_str(src_cs_name));
        ConstColorSpaceRcPtr dst_cs = config->getColorSpace(c_str(dst_cs_name));

        if (!src_cs && dst_cs) {
            bool blocked = containsBlockableTransform(config, ctx,
                                                      c_str(dst_cs->getName()),
                                                      keep, omit);
            return blocked;
        }
        if (!dst_cs && src_cs) {
            bool blocked = containsBlockableTransform(config, ctx,
                                                      c_str(src_cs->getName()),
                                                      keep, omit);
            return blocked;
        }

        if (src_cs && dst_cs) {
            if (omit.count(src_cs->getName())
                || omit.count(dst_cs->getName())) {
                return true;
            }
            if (keep.count(c_str(src_cs->getName()))
                && keep.count(c_str(dst_cs->getName())))
                return false;
            bool blocked = containsBlockableTransform(config, ctx,
                                                      c_str(src_cs->getName()),
                                                      keep, omit);
            if (blocked)
                return true;
            blocked = containsBlockableTransform(config, ctx,
                                                 c_str(dst_cs->getName()), keep,
                                                 omit);
            return blocked;
        }
        return true;
    }
    default:
        // Atomic (non-structural) types: defer to the shared allowlist.
        return !isSimpleAtomicTransform(transform);
    }
}


}  // namespace

// The unsorted set of "simple" color space names for a config. "Simple"
// means likely stable for interop matching: not data, not "is-unique", and
// not blocked by an unsupported/complex transform construct (the policy
// lives in containsBlockableTransform()).
std::vector<std::string>
get_simple_color_spaces(const OCIO::ConstConfigRcPtr& config)
{
    std::vector<std::string> simpleSpaces;
    auto keep = scan_simple_color_space_names(config);
    simpleSpaces.reserve(keep.size());
    for (const auto& name : keep) {
        simpleSpaces.emplace_back(name);
    }
    return simpleSpaces;
}



const std::vector<std::string>&
ColorConfig::Impl::getSimpleColorSpaces() const
{
    {
        spin_rw_read_lock lock(m_mutex);
        if (m_simple_color_spaces_cached)
            return m_simple_color_spaces_cache;
    }

    auto simple_spaces = get_simple_color_spaces(config_);
    std::sort(simple_spaces.begin(), simple_spaces.end());

    {
        spin_rw_write_lock lock(m_mutex);
        if (!m_simple_color_spaces_cached) {
            m_simple_color_spaces_cache  = std::move(simple_spaces);
            m_simple_color_spaces_cached = true;
        }
        return m_simple_color_spaces_cache;
    }
}



void
ColorConfig::Impl::analyze(CSInfo* cs)
{
    // Unlocked fast-path check: acquire pairs with the release publish at
    // the bottom, making the flags/active written before it visible.
    if (cs->analyzed.load(std::memory_order_acquire))
        return;

    // Gather everything that takes its own locks *before* locking m_mutex
    // (the lock-ordering hazard examine() also avoids).
    const std::vector<std::string>& simple = getSimpleColorSpaces();

    int flagval = 0;
    bool active = true;
    if (config_ && !disable_ocio) {
        OCIO::ConstColorSpaceRcPtr ocs = config_->getColorSpace(
            cs->name.c_str());
        if (ocs) {
            if (ocs->isData())
                flagval |= CSInfo::is_data;
            if (ocs->hasCategory("is-unique"))
                flagval |= CSInfo::is_unique;

            const char* sp   = config_->getSearchPath();
            bool sp_has_vars = sp && Strutil::contains(sp, "$");
            if (!transformUsesContextVars(
                    ocs->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE),
                    sp_has_vars)
                && !transformUsesContextVars(
                    ocs->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE),
                    sp_has_vars))
                flagval |= CSInfo::is_context_invariant;

            // Membership in the active colorspace enumeration.
            // For now O(n) scan per analyzed space; build a name set once if
            // analysis of whole large configs becomes hot.
            active = false;
            const int n
                = config_->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                             OCIO::COLORSPACE_ACTIVE);
            for (int i = 0; i < n; ++i) {
                const char* aname = config_->getColorSpaceNameByIndex(
                    OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ACTIVE,
                    i);
                if (aname && cs->name == aname) {
                    active = true;
                    break;
                }
            }
        }
    }
    if (std::binary_search(simple.begin(), simple.end(), cs->name))
        flagval |= CSInfo::is_simple;
    else if (!(flagval & CSInfo::is_data))
        flagval |= CSInfo::has_complex_transform;
    if ((flagval & (CSInfo::is_data | CSInfo::is_unique))
        || isLearnedComplex(currentContextID(), cs->name))
        flagval |= CSInfo::should_skip_matching;

    spin_rw_write_lock lock(m_mutex);
    if (!cs->analyzed.load(std::memory_order_relaxed)) {
        cs->setflag(flagval);
        cs->active = active;
        cs->analyzed.store(true, std::memory_order_release);
    }
}



int
ColorConfig::Impl::analysisFlags(string_view name, bool* active)
{
    CSInfo* cs = find(name);
    if (!cs) {
        if (active)
            *active = false;
        return 0;
    }
    analyze(cs);
    spin_rw_read_lock lock(m_mutex);
    if (active)
        *active = cs->active;
    return cs->flags();
}



int
ColorConfig::Impl::compute_analysis_flags(const std::string& name,
                                          bool& active) const
{
    // Mirrors analyze() but for any name (including spaces outside the active
    // CSInfo inventory). Gather lock-taking work before any caller lock.
    const std::vector<std::string>& simple = getSimpleColorSpaces();

    int flagval = 0;
    active      = true;
    if (config_ && !disable_ocio) {
        OCIO::ConstColorSpaceRcPtr ocs = config_->getColorSpace(name.c_str());
        if (ocs) {
            if (ocs->isData())
                flagval |= CSInfo::is_data;
            if (ocs->hasCategory("is-unique"))
                flagval |= CSInfo::is_unique;

            const char* sp   = config_->getSearchPath();
            bool sp_has_vars = sp && Strutil::contains(sp, "$");
            if (!transformUsesContextVars(
                    ocs->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE),
                    sp_has_vars)
                && !transformUsesContextVars(
                    ocs->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE),
                    sp_has_vars))
                flagval |= CSInfo::is_context_invariant;

            active = false;
            const int n
                = config_->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                             OCIO::COLORSPACE_ACTIVE);
            for (int i = 0; i < n; ++i) {
                const char* aname = config_->getColorSpaceNameByIndex(
                    OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ACTIVE,
                    i);
                if (aname && name == aname) {
                    active = true;
                    break;
                }
            }
        }
    }
    if (std::binary_search(simple.begin(), simple.end(), name))
        flagval |= CSInfo::is_simple;
    else if (!(flagval & CSInfo::is_data))
        flagval |= CSInfo::has_complex_transform;
    if ((flagval & (CSInfo::is_data | CSInfo::is_unique))
        || isLearnedComplex(currentContextID(), name))
        flagval |= CSInfo::should_skip_matching;
    return flagval;
}


//////////////////////////////////////////////////////////////////////////
//
// Color Interop ID

namespace {
enum class CICPPrimaries : int {
    Rec709  = 1,
    Rec2020 = 9,
    XYZD65  = 10,
    P3D65   = 12,
};

enum class CICPTransfer : int {
    BT709   = 1,
    Gamma22 = 4,
    Linear  = 8,
    sRGB    = 13,
    PQ      = 16,
    Gamma26 = 17,
    HLG     = 18,
};

enum class CICPMatrix : int {
    RGB         = 0,
    BT709       = 1,
    Unspecified = 2,
    Rec2020_NCL = 9,
    Rec2020_CL  = 10,
};

enum class CICPRange : int {
    Narrow = 0,
    Full   = 1,
};

struct ColorInteropID {
    constexpr ColorInteropID(string_view interop_id)
        : interop_id(interop_id)
        , cicp({ 0, 0, 0, 0 })
        , has_cicp(false)
    {
    }

    constexpr ColorInteropID(string_view interop_id, CICPPrimaries primaries,
                             CICPTransfer transfer, CICPMatrix matrix)
        : interop_id(interop_id)
        , cicp({ int(primaries), int(transfer), int(matrix),
                 int(CICPRange::Full) })
        , has_cicp(true)
    {
    }

    string_view interop_id;
    std::array<int, 4> cicp;
    bool has_cicp;
};

// Mapping between color interop ID and CICP, based on Color Interop Forum
// recommendations.
//
// The `interop_id` string of every entry below (other than the "unknown"
// utility token, which the registry deliberately does not declare -- see
// pvt::is_utility_interop_id) spells a canonical id declared by the
// embedded interop identities registry (interop-identities-config.ocio).
// The entries are plain literals because they are data rows; the registry
// remains the source of truth for id spelling, and color_test.cpp's
// test_legacy_table_registry_sync() guards against any entry (a typo, a
// registry rename) quietly drifting out of the registry set.
constexpr ColorInteropID color_interop_ids[] = {
    // Scene referred interop IDs first so they are the default in automatic
    // conversion from CICP to interop ID. Some are not display color spaces
    // at all, but can be represented by CICP anyway.
    { "lin_ap1_scene" },
    { "lin_ap0_scene" },
    { "lin_rec709_scene", CICPPrimaries::Rec709, CICPTransfer::Linear,
      CICPMatrix::BT709 },
    { "lin_p3d65_scene", CICPPrimaries::P3D65, CICPTransfer::Linear,
      CICPMatrix::BT709 },
    { "lin_rec2020_scene", CICPPrimaries::Rec2020, CICPTransfer::Linear,
      CICPMatrix::Rec2020_CL },
    { "lin_adobergb_scene" },
    { "lin_ciexyzd65_scene", CICPPrimaries::XYZD65, CICPTransfer::Linear,
      CICPMatrix::Unspecified },
    // Rec.709 primaries + transfer 13 (IEC 61966-2-1, the sRGB OETF) is
    // display-referred sRGB, not scene-referred: CICP describes the
    // encoding of the actual (already display-referred) pixel values, per
    // ITU-T H.273. Listed here, ahead of srgb_rec709_scene, so it wins the
    // first-match lookup in get_color_interop_id(const int cicp[4]).
    { "srgb_rec709_display", CICPPrimaries::Rec709, CICPTransfer::sRGB,
      CICPMatrix::BT709 },
    { "srgb_rec709_scene", CICPPrimaries::Rec709, CICPTransfer::sRGB,
      CICPMatrix::BT709 },
    { "g22_rec709_scene", CICPPrimaries::Rec709, CICPTransfer::Gamma22,
      CICPMatrix::BT709 },
    { "g18_rec709_scene" },
    { "srgb_ap1_scene" },
    { "g22_ap1_scene" },
    { "srgb_p3d65_scene", CICPPrimaries::P3D65, CICPTransfer::sRGB,
      CICPMatrix::BT709 },
    { "g22_adobergb_scene" },
    { "data" },
    { "unknown" },  // utility token; deliberately not a registry entry.

    // Display referred interop IDs. (srgb_rec709_display is listed above,
    // ahead of srgb_rec709_scene, so it resolves first on read.)
    { "g24_rec709_display", CICPPrimaries::Rec709, CICPTransfer::BT709,
      CICPMatrix::BT709 },
    { "srgb_p3d65_display", CICPPrimaries::P3D65, CICPTransfer::sRGB,
      CICPMatrix::BT709 },
    { "srgbe_p3d65_display", CICPPrimaries::P3D65, CICPTransfer::sRGB,
      CICPMatrix::BT709 },
    { "pq_p3d65_display", CICPPrimaries::P3D65, CICPTransfer::PQ,
      CICPMatrix::Rec2020_NCL },
    { "pq_rec2020_display", CICPPrimaries::Rec2020, CICPTransfer::PQ,
      CICPMatrix::Rec2020_NCL },
    { "hlg_rec2020_display", CICPPrimaries::Rec2020, CICPTransfer::HLG,
      CICPMatrix::Rec2020_NCL },
    // No CICP mapping to keep previous behavior unchanged, as Gamma 2.2
    // display is more likely meant to be written as sRGB. On read the
    // scene referred interop ID will be used.
    { "g22_rec709_display",
      /* CICPPrimaries::Rec709, CICPTransfer::Gamma22, CICPMatrix::BT709 */ },
    // No CICP code for Adobe RGB primaries.
    { "g22_adobergb_display" },
    { "g26_p3d65_display", CICPPrimaries::P3D65, CICPTransfer::Gamma26,
      CICPMatrix::BT709 },
    { "g26_xyzd65_display", CICPPrimaries::XYZD65, CICPTransfer::Gamma26,
      CICPMatrix::Unspecified },
    // The P3-primaries DCDM form (g26_p3d65 colorimetry + the DCI white
    // headroom). CICP carries no headroom concept, so this shares the P3D65 /
    // gamma-2.6 tuple with g26_p3d65_display; the reverse cicp->id lookup is
    // first-match and keeps returning g26_p3d65_display (the no-headroom form),
    // the conservative decode. ponytail: distinct id, same tuple by design.
    { "dcdm_p3d65_display", CICPPrimaries::P3D65, CICPTransfer::Gamma26,
      CICPMatrix::BT709 },
    { "pq_xyzd65_display", CICPPrimaries::XYZD65, CICPTransfer::PQ,
      CICPMatrix::Unspecified },
};

// Read-only interop identities: valid to RESOLVE on read (a file may carry the
// tag and it must interpret), but never emitted as a write/derive target.
// dcdm_p3d65_display -- the P3-primaries DCDM form -- is a read-only
// interpretation identity; the canonical WRITE form of that colorimetry is
// g26_xyzd65_display (the XYZ DCDM). Derivation must never hand back a read-only
// id, and the CICP reverse lookup must skip it. [decision: Zach 2026-07-23]
static bool
is_readonly_interop_id(string_view id)
{
    return id == "dcdm_p3d65_display";
}
}  // namespace

string_view
ColorConfig::get_color_interop_id(string_view colorspace) const
{
    // Cheap lookup ONLY (see the doc comment in color.h): a declared
    // interop_id attribute, the data-space utility token, or a static-table
    // name/alias/classification match -- all through the syntactic
    // (fingerprint-free) resolution subset. The expensive derivation cascade
    // (fingerprint matching against the built-in registry, config-local id
    // manufacture) lives in pvt::derive_color_interop_id() and runs behind
    // the characterization engine's derive tier (where write planning
    // consumes it), never inside this getter.
    if (colorspace.empty())
        return "";

    if (getImpl()->config_ && !disable_ocio) {
        std::string resolved(getImpl()->resolve_syntactic(colorspace));
        if (resolved.empty())
            resolved = colorspace;
        OCIO::ConstColorSpaceRcPtr c;
        try {
            c = getImpl()->config_->getColorSpace(resolved.c_str());
        } catch (...) {
            c = nullptr;
        }
        if (c) {
            // An author-declared interop_id on the space is unconditionally
            // authoritative (OCIO 2.5+). A non-empty value is what "declared"
            // means; an unset attribute (empty string) falls through rather
            // than short-circuiting to empty.
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
            if (const char* iid = c->getInteropID();
                iid && *iid && !is_readonly_interop_id(iid))
                return iid;
#endif
            // Utility sub-case: a data space with no explicit token is
            // "data", never "unknown" or empty. (isData() is available on
            // all OCIO 2.x.)
            if (c->isData())
                return "data";
        }
    }

    // The static CICP / interop-id table matched by syntactic (name / role /
    // alias / cheap-classification) equivalence -- also the table get_cicp()
    // consults to map an id back to a CICP tuple. Its literals live in
    // static storage, so the returned view is stable.
    for (const ColorInteropID& interop : color_interop_ids) {
        if (is_readonly_interop_id(interop.interop_id))
            continue;  // read-only id: never a write/derive target
        if (getImpl()->equivalent_syntactic(colorspace, interop.interop_id))
            return interop.interop_id;
    }

    // Not identified: return empty, never a guessed default.
    return "";
}


// The full write-side derivation cascade behind pvt::derive_color_interop_id
// (the pvt shim at the end of this file forwards here). This is the
// EXPENSIVE path -- fingerprint probing can build the registry index and
// OCIO processors -- so it is a distinct entry point consumed by the
// characterization engine's derive tier (through which the write planner
// and the public derive verbs receive it), never hidden behind the cheap
// public getter.
string_view
derive_color_interop_id_impl(const ColorConfig& config, string_view colorspace)
{
    if (colorspace.empty())
        return "";

    // Marker-vs-marker precedence (ADR-0020): an incoming unknown-marker is
    // already a terminal statement about identity, not a color space to derive
    // FROM. Return it unchanged, canonically spelled, before the cascade runs.
    //
    // Without this guard the cascade silently rewrites one marker into
    // another: resolve() legally strips the leftmost namespace (a CIF
    // fall-back), so "error:unknown" and "oiio:unknown" both become bare
    // "unknown" -- and in a config that happens to contain a space NAMED
    // "unknown", step 1's config-declared branch below then answers
    // "ocio:unknown". That converts a strict-resolution FAILURE, or OIIO's own
    // synthetic isData/NoOp treatment marker, into a claim that the CONFIG
    // declared unknownness. The evidence of the error path is destroyed at the
    // point of derivation, and derive_color_interop_id is public-facing, so
    // any caller distinguishing "the config told us" from "we failed to
    // resolve" gets the wrong answer.
    //
    // The wire is unaffected either way -- the writer maps every marker in
    // this family to bare "unknown" on disk -- so this is purely about keeping
    // the internal signal diagnosable.
    switch (::OIIO::pvt::classify_interop_marker(colorspace)) {
    case ::OIIO::pvt::InteropMarker::OcioUnknown: return "ocio:unknown";
    case ::OIIO::pvt::InteropMarker::OiioUnknown: return "oiio:unknown";
    case ::OIIO::pvt::InteropMarker::ErrorUnknown: return "error:unknown";
    default: break;
    }

    // Four-step Color Interop Forum write-side derivation. The first step to
    // produce an id wins; the fingerprint engine only wakes on the first
    // query that reaches it.

    auto* impl = pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl)
        return "";

    std::string resolved;
    bool resolves_to_real_space = false;
    if (impl->config_ && !disable_ocio) {
        resolved = std::string(config.resolve(colorspace));
        OCIO::ConstColorSpaceRcPtr c;
        try {
            c = impl->config_->getColorSpace(resolved.c_str());
        } catch (...) {
            c = nullptr;
        }
        if (c) {
            resolves_to_real_space = true;
            // Step 1: an author-declared interop_id on the space is
            // unconditionally authoritative -- it wins over fingerprinting and
            // over any strict/unknown handling (OCIO 2.5+). A non-empty value is
            // what "declared" means; an unset attribute (empty string) falls
            // through to the tiers below rather than short-circuiting to empty.
            // A declared value of literally "unknown" is the config-side
            // declaration of unknownness and derives the "ocio:unknown"
            // marker (the config declared it, OIIO reports where).
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
            if (const char* iid = c->getInteropID();
                iid && *iid && !is_readonly_interop_id(iid))
                return Strutil::iequals(iid, "unknown") ? "ocio:unknown" : iid;
#endif
            // Step 1, config-declared unknown: a space NAMED (not merely
            // aliased) "unknown" with no contradicting declared interop_id
            // is the config's own statement that this data's color space is
            // unknown -- derive the "ocio:unknown" marker, before the
            // isData sub-case and any fingerprint tier. A user's explicit
            // colorInteropID attribute of "unknown" never reaches this
            // derivation (the planner writes explicit values verbatim), so
            // bare "unknown" on disk always means the author's own bytes.
            if (Strutil::iequals(c->getName(), "unknown"))
                return "ocio:unknown";
            // Step 1, utility sub-case: a data space with no explicit token
            // resolves to "data" HERE -- before any fingerprint tier -- never
            // "unknown" or empty. (isData() is available on all OCIO 2.x.)
            if (c->isData())
                return "data";
        }
    }

    // Step 2: definitional equivalence to a built-in registry identity, by
    // fingerprint. Returns THAT registry identity's id (a process-global-stable
    // string), not the query's own name. Only meaningful once the query resolves
    // to a real space to fingerprint.
    if (resolves_to_real_space) {
        if (string_view r = impl->deriveRegistryInteropId(resolved);
            !r.empty() && !is_readonly_interop_id(r))
            return r;
    }

    // Step 2.5 (legacy syntactic fallback -- DECISION a): the static CICP /
    // interop-id table matched by name/alias/flag equivalence. Kept AFTER the
    // real fingerprint match (so a genuine fingerprint match is always
    // preferred); retiring it would change get_cicp(), which consults this same
    // table to map an id back to a CICP tuple. It is never the final-resort
    // match -- steps 3 and 4 always follow -- so it can never act as a guessed
    // default. Its literals live in static storage, so the returned view is
    // stable. The "unknown" utility entry is deliberately SKIPPED here: the
    // derivation never emits bare "unknown" (a query that is only the
    // literal token, with no backing config space, is a cannot-determine and
    // omits; a space genuinely named "unknown" already derived the
    // "ocio:unknown" marker in step 1).
    for (const ColorInteropID& interop : color_interop_ids) {
        if (interop.interop_id == "unknown"
            || is_readonly_interop_id(interop.interop_id))
            continue;
        if (config.equivalent(colorspace, interop.interop_id))
            return interop.interop_id;
    }

    // Step 3 (DECISION b): a named config plus a query that resolves to a real
    // space yields a config-local id "<config>:local:<space>", both segments
    // sanitized independently per the CIF grammar. This ALWAYS attempts -- it is
    // not gated behind an opt-in knob -- because its two natural preconditions,
    // a non-empty config name AND a resolvable query, already keep it from
    // firing on a genuine miss. The generated std::string is interned via
    // ustring so the returned view outlives this call (the local literals and
    // OCIO-owned strings the earlier steps return are already stable).
    if (resolves_to_real_space) {
        const char* cfgname = impl->config_->getName();
        if (cfgname && *cfgname) {
            // Never serialize an ambiguous id: the sanitizer is many-to-one,
            // so if a SECOND space's sanitized name/alias collides on this
            // token, resolution could not uniquely reverse the id. Fall
            // through to step 4's never-guess empty rather than emit an id
            // that silently names two spaces.
            const std::string base = OIIO::pvt::sanitize_id_token(resolved);
            if (!unique_space_for_sanitized_token(impl->config_, base).empty())
                return ustring(OIIO::pvt::sanitize_id_token(cfgname)
                               + ":local:" + base);
        }
    }

    // Step 4: nothing identified the space -- return empty, never a guessed
    // default (a wrong id costs trust in the whole system).
    return "";
}

string_view
ColorConfig::get_color_interop_id(const int cicp[4]) const
{
    for (const ColorInteropID& interop : color_interop_ids) {
        // Skip read-only ids: the reverse tuple lookup feeds write derivation,
        // and a read-only id (e.g. dcdm_p3d65_display, which shares the
        // P3D65/Gamma26 tuple with g26_p3d65_display) must never be a target.
        if (is_readonly_interop_id(interop.interop_id))
            continue;
        if (interop.has_cicp && interop.cicp[0] == cicp[0]
            && interop.cicp[1] == cicp[1]) {
            return interop.interop_id;
        }
    }
    return "";
}

cspan<int>
ColorConfig::get_cicp(string_view colorspace) const
{
    string_view interop_id = get_color_interop_id(colorspace);
    if (!interop_id.empty()) {
        for (const ColorInteropID& interop : color_interop_ids) {
            if (interop.has_cicp && interop_id == interop.interop_id) {
                return interop.cicp;
            }
        }
    }
    return cspan<int>();
}


std::vector<std::string>
ColorConfig::find_color_spaces(cspan<std::string> chromaticities,
                               cspan<std::string> transfer_function,
                               cspan<std::string> encoding,
                               cspan<std::string> image_state,
                               const ColorSpaceSearchOptions& search) const
{
    // Thin public adapter: fill the internal option set (the active-space
    // toggle has no public counterpart -- the public API always searches
    // active spaces) and forward to the pvt search core. The internal core
    // throws std::invalid_argument on a malformed/unresolvable hint; the
    // public surface converts that to the class's has_error()/geterror()
    // convention and never throws.
    OIIO::pvt::FindColorSpacesOptions options;
    options.chromaticities.assign(chromaticities.begin(), chromaticities.end());
    options.transfer_functions.assign(transfer_function.begin(),
                                      transfer_function.end());
    options.encodings.assign(encoding.begin(), encoding.end());
    options.image_states.assign(image_state.begin(), image_state.end());
    options.include_inactive          = search.include_inactive;
    options.include_context_sensitive = search.include_context_sensitive;
    options.exhaustive                = search.include_complex;
    options.strict                    = search.authored_encoding_only;
    options.context                   = search.context;
    try {
        return OIIO::pvt::find_color_spaces(*this, options);
    } catch (const std::exception& e) {
        getImpl()->error("find_color_spaces: {}", e.what());
        return {};
    }
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
    OIIO::pvt::LoggedTimer logtime("IBA::colorconvert");
    if (!colorconfig)
        colorconfig = &ColorConfig::default_colorconfig();
    // Automatic metadata hygiene around the operation: prepare() resolves
    // the source (explicit -> tagged -> inferred from the spec's color
    // hints -> lenient default, with the unresolvable-source failure
    // split); finish() below maintains the output spec.
    OIIO::pvt::ColorOperationHygiene hygiene;
    if (!hygiene.prepare(src, dst, *colorconfig, from))
        return false;
    from = hygiene.source();
    if (to.empty() || to == "unknown") {
        dst.errorfmt("Unknown color space name (from=\"{}\", to=\"{}\")", from,
                     to);
        return false;
    }

    ColorProcessorHandle processor
        = colorconfig->createColorProcessor(colorconfig->resolve(from),
                                            colorconfig->resolve(to),
                                            context_key, context_value);
    if (!processor) {
        if (colorconfig->has_error())
            dst.errorfmt("{}", colorconfig->geterror());
        else
            dst.errorfmt(
                "Could not construct the color transform {} -> {} (unknown error)",
                from, to);
        return false;
    }

    // A lenient cross-config fallback is a pass-through no-op standing in
    // for a conversion that could not be reconciled (non-strict parsing: the
    // requested spaces couldn't be bridged, but the pipeline proceeds
    // anyway). The outcome travels WITH the processor -- never the shared
    // error string, which cache hits and unrelated calls may not reflect --
    // so ask the config whether THIS processor is such a fallback. No pixels
    // are actually converted in that case, so the output must keep
    // documenting its true (source) space rather than claiming the requested
    // destination -- an honest no-op instead of metadata that asserts a
    // conversion that never happened.
    bool lenient_passthrough = pvt::ColorConfigClassificationPeek::impl(
                                   *colorconfig)
                                   ->lenient_fallback_message(processor.get())
                                   .size()
                               > 0;

    logtime.stop(-1);  // transition to other colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    // Coming from a non-color space, or a lenient pass-through no-op, the
    // pixels never changed space: the operation is space-preserving and
    // the output keeps documenting its true (source) space with its
    // still-true hints. Otherwise the identity is Known: verdict stamped,
    // stale provenance facts scrubbed (uniformly -- explicit and inferred
    // sources alike), cheap current-state descriptors maintained.
    auto identity = OIIO::pvt::ColorOperationIdentity::Known;
    if (colorconfig->isData(from) || lenient_passthrough) {
        to       = from;
        identity = OIIO::pvt::ColorOperationIdentity::Preserved;
    }
    hygiene.finish(identity, to, ok);
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
    OIIO::pvt::LoggedTimer logtime("IBA::colormatrixtransform");
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
        roi, paropt(nthreads),
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
    parallel_image(roi, paropt(nthreads), [&](ROI roi) {
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
    OIIO::pvt::LoggedTimer logtime("IBA::colorconvert");
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
    OIIO::pvt::LoggedTimer logtime("IBA::ociolook");
    if (!colorconfig)
        colorconfig = &ColorConfig::default_colorconfig();
    // Hygiene resolves the operation's source; an unspecified `to` means
    // the look leaves the image in that same (resolved) space.
    OIIO::pvt::ColorOperationHygiene hygiene;
    if (!hygiene.prepare(src, dst, *colorconfig, from))
        return false;
    from = hygiene.source();
    if (to.empty() || to == "current")
        to = hygiene.source();
    if (to.empty()) {
        dst.errorfmt("Unknown color space name");
        return false;
    }
    ColorProcessorHandle processor;
    {
        processor = colorconfig->createLookTransform(looks,
                                                     colorconfig->resolve(from),
                                                     colorconfig->resolve(to),
                                                     inverse, key, value);
        if (!processor) {
            if (colorconfig->has_error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
                dst.errorfmt(
                    "Could not construct the color transform (unknown error)");
            return false;
        }
    }

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    // The look declares its output space: identity-known, full hygiene.
    hygiene.finish(OIIO::pvt::ColorOperationIdentity::Known, to, ok);
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
    OIIO::pvt::LoggedTimer logtime("IBA::ociodisplay");
    if (!colorconfig)
        colorconfig = &ColorConfig::default_colorconfig();
    OIIO::pvt::ColorOperationHygiene hygiene;
    if (!hygiene.prepare(src, dst, *colorconfig, from))
        return false;
    from = hygiene.source();
    ColorProcessorHandle processor;
    {
        processor
            = colorconfig->createDisplayTransform(display, view,
                                                  colorconfig->resolve(from),
                                                  looks, inverse, key, value);
        if (!processor) {
            if (colorconfig->has_error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
                dst.errorfmt(
                    "Could not construct the color transform (unknown error)");
            return false;
        }
    }

    // Same lenient cross-config pass-through signal as
    // ImageBufAlgo::colorconvert(): the fallback outcome travels WITH the
    // processor (reconcile_cross_config_display() fell back to a no-op under
    // non-strict parsing), never through the shared error string. No pixels
    // moved, so the output must keep documenting the space the pixels are
    // actually in -- never the space the failed conversion was reaching for.
    // Which space that is depends on direction (handled per-branch below).
    bool lenient_passthrough = pvt::ColorConfigClassificationPeek::impl(
                                   *colorconfig)
                                   ->lenient_fallback_message(processor.get())
                                   .size()
                               > 0;

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok) {
        if (display.empty() || display == "default")
            display = colorconfig->getDefaultDisplayName();
        if (view.empty() || view == "default")
            view = colorconfig->getDefaultViewName(display,
                                                   colorconfig->resolve(from));
        // The pixels land in the (display, view) space forward, or the
        // scene `from` space inverse -- unless the conversion fell back to
        // a lenient pass-through no-op, in which case they never left the
        // space the input arrived in: tag that source space, not the space
        // the failed conversion was reaching for (the honest no-op rule),
        // and treat the operation as space-preserving (its still-true
        // hints pass through). (getDisplayViewColorSpaceName's shared-view
        // path returns interned storage, so holding the result is safe.)
        const bool disp_view_target = inverse == lenient_passthrough;
        std::string target;
        if (disp_view_target) {
            const char* c = colorconfig->getDisplayViewColorSpaceName(display,
                                                                      view);
            target        = c ? c : "";
        } else {
            target = colorconfig->resolve(from);
        }
        hygiene.finish(lenient_passthrough
                           ? OIIO::pvt::ColorOperationIdentity::Preserved
                           : OIIO::pvt::ColorOperationIdentity::Known,
                       target, true);
    }
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



bool
ImageBufAlgo::ociofiletransform(ImageBuf& dst, const ImageBuf& src,
                                string_view name, bool unpremult, bool inverse,
                                const ColorConfig* colorconfig, ROI roi,
                                int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::ociofiletransform");
    if (name.empty()) {
        dst.errorfmt("Unknown filetransform name");
        return false;
    }
    if (!colorconfig)
        colorconfig = &ColorConfig::default_colorconfig();
    OIIO::pvt::ColorOperationHygiene hygiene;
    hygiene.prepare(src, dst, *colorconfig);
    ColorProcessorHandle processor;
    {
        processor = colorconfig->createFileTransform(name, inverse);
        if (!processor) {
            if (colorconfig->has_error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
                dst.errorfmt(
                    "Could not construct the color transform (unknown error)");
            return false;
        }
    }

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    // An arbitrary file/LUT transform's resulting space cannot generally
    // be known, and users must not expect it: identity-unknowable, so
    // hygiene erases the verdict, the stale provenance facts, and the
    // current-state descriptors (absence = could-not-determine, never a
    // guess). The one exception is when the file name itself names the
    // result via the config's file rules -- then the identity is declared
    // and full Known hygiene applies, preserving the longstanding
    // color-space-from-filepath behavior.
    if (!colorconfig->filepathOnlyMatchesDefaultRule(name))
        hygiene.finish(OIIO::pvt::ColorOperationIdentity::Known,
                       colorconfig->getColorSpaceFromFilepath(name), ok);
    else
        hygiene.finish(OIIO::pvt::ColorOperationIdentity::Unknowable, {}, ok);
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
ImageBufAlgo::ocionamedtransform(ImageBuf& dst, const ImageBuf& src,
                                 string_view name, bool unpremult, bool inverse,
                                 string_view key, string_view value,
                                 const ColorConfig* colorconfig, ROI roi,
                                 int nthreads)
{
    OIIO::pvt::LoggedTimer logtime("IBA::ocionamedtransform");
    ColorProcessorHandle processor;
    {
        if (!colorconfig)
            colorconfig = &ColorConfig::default_colorconfig();
        processor = colorconfig->createNamedTransform(name, inverse, key,
                                                      value);
        if (!processor) {
            if (colorconfig->has_error())
                dst.errorfmt("{}", colorconfig->geterror());
            else
                dst.errorfmt(
                    "Could not construct the color transform (unknown error)");
            return false;
        }
    }

    logtime.stop();  // transition to colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    return ok;
}



ImageBuf
ImageBufAlgo::ocionamedtransform(const ImageBuf& src, string_view name,
                                 bool unpremult, bool inverse, string_view key,
                                 string_view value,
                                 const ColorConfig* colorconfig, ROI roi,
                                 int nthreads)
{
    ImageBuf result;
    bool ok = ocionamedtransform(result, src, name, unpremult, inverse, key,
                                 value, colorconfig, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::ocionamedtransform() error");
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



void
ColorConfig::set_colorspace(ImageSpec& spec, string_view colorspace) const
{
    // Re-asserting the color space the spec already carries is a no-op: it
    // adds no information, and refreshing state on a pure re-assertion
    // would perturb read paths that re-assert redundantly (e.g. Exif
    // decoding re-tags sRGB on virtually every camera file after the
    // reader already did). Descriptor refreshes ride actual color
    // operations (ColorOperationHygiene) or an actual change of claim.
    string_view oldspace = spec.get_string_attribute("oiio:ColorSpace");
    if (oldspace.size() && colorspace.size() && oldspace == colorspace)
        return;

    if (colorspace.empty()) {
        // Absence semantics: assume NOTHING about the color space. The
        // verdict, every file-provenance fact, and every current-state
        // descriptor are erased (could-not-determine is expressed by
        // absence, never by a guess).
        spec.erase_attribute("oiio:ColorSpace");
        OIIO::pvt::scrub_color_metadata(spec);
        OIIO::pvt::erase_color_state_descriptors(spec);
    } else {
        spec.attribute("oiio:ColorSpace", colorspace);
        if (oldspace.size()) {
            // Asserting a DIFFERENT space over an existing claim: the
            // identity-known two-bucket hygiene. File-provenance facts
            // (colorInteropID, CICP, chromaticities, gamma, ICC, the ACES
            // container flag) described the old claim and are now stale:
            // scrub them all. Current-state descriptors are the other
            // bucket: MAINTAINED, update-or-erase from the cheap
            // characterization -- but only for a spec that already
            // carries some; introducing characterization a buffer never
            // had is the pixel-operation hygiene's job
            // (ColorOperationHygiene), not the tag's, and tag-only paths
            // keep their observable output unchanged.
            OIIO::pvt::scrub_color_metadata(spec);
            if (spec.find_attribute("oiio:ColorSpace:state")
                || spec.find_attribute("oiio:ColorSpace:encoding")
                || spec.find_attribute("oiio:ColorSpace:range")
                || spec.find_attribute("oiio:ColorSpace:equality_id"))
                OIIO::pvt::maintain_color_state_descriptors(spec, *this,
                                                            colorspace);
        }
        // First tagging (no previous claim) deliberately does NOT scrub
        // the provenance facts: at read time the verdict is routinely
        // DERIVED from those very facts (the metadata reconciler, the
        // format readers), which are evidence for the claim, not
        // contradictions of it.
    }

    // Format-specific color hints outside the provenance bucket that could
    // contradict the new claim, plus oiio:Gamma for the first-tagging path
    // (the scrub covers it on the paths above). Longstanding behavior.
    if (!equivalent(colorspace, "srgb_rec709_scene"))
        spec.erase_attribute("Exif:ColorSpace");
    spec.erase_attribute("tiff:ColorSpace");
    spec.erase_attribute("tiff:PhotometricInterpretation");
    spec.erase_attribute("oiio:Gamma");
}



void
ColorConfig::set_colorspace_rec709_gamma(ImageSpec& spec, float gamma) const
{
    // Round gamma to the nearest hundredth to prevent stupid precision choices
    // and make it easier for apps to make decisions based on known gamma values.
    float g_rounded = std::round(gamma * 100.0f) / 100.0f;
    if (fabsf(g_rounded - 1.0f) <= 0.01f) {
        set_colorspace(spec, "lin_rec709_scene");
    } else if (fabsf(g_rounded - 1.8f) <= 0.01f) {
        set_colorspace(spec, "g18_rec709_scene");
        spec.attribute("oiio:Gamma", 1.8f);
    } else if (fabsf(g_rounded - 2.2f) <= 0.01f) {
        set_colorspace(spec, "g22_rec709_scene");
        spec.attribute("oiio:Gamma", 2.2f);
    } else if (fabsf(g_rounded - 2.4f) <= 0.01f) {
        set_colorspace(spec, "g24_rec709_scene");
        spec.attribute("oiio:Gamma", 2.4f);
    } else {
        set_colorspace(spec,
                       Strutil::fmt::format("g{}_rec709_scene",
                                            std::lround(g_rounded * 10.0f)));
        // Preserve the original gamma value for use in color conversions.
        spec.attribute("oiio:Gamma", gamma);
    }
}


void
set_colorspace(ImageSpec& spec, string_view colorspace)
{
    ColorConfig::default_colorconfig().set_colorspace(spec, colorspace);
}

void
set_colorspace_rec709_gamma(ImageSpec& spec, float gamma)
{
    ColorConfig::default_colorconfig().set_colorspace_rec709_gamma(spec, gamma);
}

OIIO_NAMESPACE_END



// The pvt shims below are declared (OIIO_API) in the library's "current"
// namespace by color_pvt.h, so they must be defined there too, not inside
// the ABI-versioned v3_1 namespace the helpers above live in.
OIIO_NAMESPACE_BEGIN

namespace pvt {


bool
copy_config_preserves_default_view_transform()
{
    return v3_1::copy_config_default_vt_probe();
}


std::vector<std::string>
legacy_interop_id_table_names()
{
    std::vector<std::string> names;
    for (const auto& interop : v3_1::color_interop_ids)
        names.emplace_back(std::string(interop.interop_id));
    return names;
}


string_view
derive_color_interop_id(const ColorConfig& config, string_view colorspace)
{
    return v3_1::derive_color_interop_id_impl(config, colorspace);
}


ColorProcessorHandle
interop_registry_processor(string_view from_id, string_view to_id)
{
    // Build a processor between two spaces of OIIO's embedded interop
    // identities registry (NOT the user config): the write-canonical mapping
    // targets registry identities (e.g. the DCDM g26_p3d65 -> g26_xyzd65
    // P3->XYZ + DCI-headroom conversion) whose real transforms live only here,
    // even when a user config declares the same NAMES as bare, transform-less
    // color spaces (which would yield a no-op in that config).
    OCIO::ConstConfigRcPtr cfg = v3_1::build_interop_identities_config();
    if (!cfg || from_id.empty() || to_id.empty())
        return {};
    try {
        OCIO::ConstProcessorRcPtr p
            = cfg->getProcessor(std::string(from_id).c_str(),
                                std::string(to_id).c_str());
        if (!p || p->isNoOp())
            return {};
        return ColorProcessorHandle(new v3_1::ColorProcessor_OCIO(p));
    } catch (OCIO::Exception&) {
        return {};
    }
}


int
color_space_analysis_flags(const ColorConfig& config, string_view name,
                           bool* active)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl) {
        if (active)
            *active = false;
        return 0;
    }
    return impl->analysisFlags(name, active);
}

bool
color_space_analyzed(const ColorConfig& config, string_view name)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->analysisComputed(name) : false;
}


// Config-declared metadata policy (spec 09, RFC POC). An OCIO config author
// can attach `oiio:colorpolicy:*` custom keys to a FileRule that exists purely
// to carry policy (rule name `oiio:<profile>`, regex `$^` so it never matches
// a file). OCIO round-trips those custom keys byte-stably and other apps
// ignore them, so this is a zero-new-API channel for the config to DECLARE
// policy. This reads back the keys OIIO's policy snapshot then honors.
std::map<std::string, std::string>
config_declared_policy_keys(const ColorConfig& config, string_view rule_name)
{
    std::map<std::string, std::string> keys;
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl || !impl->config_)
        return keys;
    OCIO::ConstFileRulesRcPtr rules;
    try {
        rules = impl->config_->getFileRules();
    } catch (...) {
        return keys;  // a malformed config never breaks policy resolution
    }
    if (!rules)
        return keys;
    for (size_t i = 0, e = rules->getNumEntries(); i < e; ++i) {
        const char* rname = rules->getName(i);
        if (!rname || rule_name != rname)
            continue;
        for (size_t k = 0, nk = rules->getNumCustomKeys(i); k < nk; ++k) {
            const char* kn = rules->getCustomKeyName(i, k);
            const char* kv = rules->getCustomKeyValue(i, k);
            if (kn && kv)
                keys[kn] = kv;
        }
    }
    return keys;
}


// Config-declared metadata policy carried by the file rule that actually
// MATCHES `filepath` (spec 09 layer 5, "matched-rule per-file opinions"). OCIO
// evaluates its own file-rule patterns against the path and reports which rule
// won; this reads that rule's `oiio:colorpolicy:*` custom keys. Profile rules
// (regex `$^`) never match a real path, so they are never picked up here --
// only genuine pattern/extension/Default rules are. Empty when OCIO support is
// off, `filepath` is empty, or the matched rule carries no custom keys.
std::map<std::string, std::string>
config_matched_rule_policy_keys(const ColorConfig& config, string_view filepath)
{
    std::map<std::string, std::string> keys;
    if (filepath.empty())
        return keys;
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl || !impl->config_)
        return keys;
    try {
        OCIO::ConstFileRulesRcPtr rules = impl->config_->getFileRules();
        if (!rules)
            return keys;
        // Ask OCIO which rule this path matches (index is an out-param; the
        // returned color space is unused -- we only want the rule).
        size_t ruleIdx = 0;
        impl->config_->getColorSpaceFromFilepath(std::string(filepath).c_str(),
                                                 ruleIdx);
        for (size_t k = 0, nk = rules->getNumCustomKeys(ruleIdx); k < nk; ++k) {
            const char* kn = rules->getCustomKeyName(ruleIdx, k);
            const char* kv = rules->getCustomKeyValue(ruleIdx, k);
            if (kn && kv)
                keys[kn] = kv;
        }
    } catch (...) {
        return keys;  // a malformed config never breaks policy resolution
    }
    return keys;
}


// Automatic metadata hygiene around the color-aware IBA operations.
// (Contract and per-identity-class semantics: see color_pvt.h.)


void
ColorOperationHygiene::prepare(const ImageBuf& src, ImageBuf& dst,
                               const ColorConfig& config)
{
    m_config = &config;
    m_dst    = &dst;
    (void)src;
    // One locked snapshot of the read-policy state per operation.
    m_policy = ColorReadPolicy::snapshot();
}



bool
ColorOperationHygiene::prepare(const ImageBuf& src, ImageBuf& dst,
                               const ColorConfig& config, string_view from)
{
    prepare(src, dst, config);
    if (!from.empty() && from != "current") {
        // Explicit source: verbatim (the operation resolves it).
        m_source = from;
        if (m_source != "unknown")
            return true;
    } else {
        m_source = src.spec().get_string_attribute("oiio:ColorSpace");
        if (m_source.empty()) {
            // A fully untagged source: infer one from the color hints the
            // spec carries (colorInteropID, CICP, ICC, chromaticities/
            // gamma) before standing on any default.
            ColorCallContext ctx;
            ctx.filename       = std::string(src.name());
            ctx.format         = std::string(src.file_format_name());
            std::string hinted = infer_color_space_from_spec(&config,
                                                             src.spec(), ctx,
                                                             m_policy);
            if (!hinted.empty()) {
                Strutil::debug("color operation inferred source color space "
                               "\"{}\" from the input's color metadata\n",
                               hinted);
                m_source = hinted;
            }
        }
        if (!m_source.empty() && m_source != "unknown")
            return true;
    }

    // Unresolvable source (nothing determined it, or it is the literal
    // "unknown"). Failure split, by consequence:
    // - Lenient scope, hintless: today's scene_linear default stands (a
    //   tracking gap is not an error -- convenience, not contract).
    // - A literal "unknown" source, or any unresolved source under a
    //   strict scope, is an ERROR for pixel math -- never a config-default
    //   guess into a processor -- except that a config-declared
    //   "error:unknown" catch space is honored under effective-strict
    //   (strict scope AND the config's own strictparsing).
    const bool lenient = m_policy.scope == ColorResolutionScope::Lenient;
    if (lenient && m_source.empty()) {
        m_source = "scene_linear";
        return true;
    }
    if (!lenient) {
        bool config_strict = false;
        try {
            auto impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
            config_strict = impl->config_
                            && impl->config_->isStrictParsingEnabled();
        } catch (...) {
        }
        if (config_strict && config.getColorSpaceIndex("error:unknown") >= 0) {
            m_source = "error:unknown";
            return true;
        }
    }
    dst.errorfmt("Could not determine the source color space (from=\"{}\")",
                 m_source);
    m_source.clear();
    return false;
}



void
ColorOperationHygiene::finish(ColorOperationIdentity identity,
                              string_view target_color_space,
                              bool pixels_succeeded)
{
    if (!m_dst || !pixels_succeeded)
        return;  // failed pixel math: leave the spec exactly as it was
    ImageSpec& spec = m_dst->specmod();

    switch (identity) {
    case ColorOperationIdentity::Preserved:
        // Space-preserving (or an honest no-op): facts and descriptors
        // pass through, and the verdict is stamped only when the
        // operation names one the spec doesn't already carry (avoiding
        // set_colorspace's collateral invalidation of still-true hints).
        if (!target_color_space.empty()
            && target_color_space
                   != spec.get_string_attribute("oiio:ColorSpace"))
            spec.set_colorspace(target_color_space);
        break;

    case ColorOperationIdentity::Unknowable:
        // The resulting space cannot be known and users must not expect
        // it: absence everywhere (could-not-determine, never-guess) --
        // not "oiio:unknown", which marks treatment, not ignorance.
        // set_colorspace("") carries the full absence semantics: verdict,
        // provenance facts, and descriptors are all erased.
        spec.set_colorspace("");
        break;

    case ColorOperationIdentity::Known:
        // Through the operation's own config (not the process default the
        // ImageSpec convenience method uses).
        m_config->set_colorspace(spec, target_color_space);
        // Two-bucket rule, applied uniformly (explicit and inferred
        // sources alike): file-provenance facts are stale, scrub them;
        // current-state descriptors are retained and UPDATED below.
        // (set_colorspace itself only applies this hygiene when changing
        // an existing claim, and only maintains descriptors a spec
        // already carries; the pixel operation asserts the change
        // unconditionally and INTRODUCES the descriptors.)
        scrub_color_metadata(spec);
        // Cheap descriptor maintenance only: never a processor, a probe,
        // or a fingerprint, and this path never asks for chromaticities
        // or transfer information. Update-or-erase, never guess.
        // Range is current-state, operation-aware: a Known conversion sets
        // the target's intrinsic range when one is explicitly known and
        // otherwise does not invent one (the erase removes a stale value);
        // a Preserved operation retains the buffer's range untouched.
        maintain_color_state_descriptors(spec, *m_config, target_color_space);
        break;
    }
}



void
maintain_color_state_descriptors(ImageSpec& spec, const ColorConfig& config,
                                 string_view color_space)
{
    // Cheap get only: get_color_space_info() does direct or previously
    // cached work -- it never builds a processor, probes a transform, or
    // computes a fingerprint. Direct/cached values update the
    // sub-attribute; an unavailable value erases it -- update-or-erase,
    // never guess. (An uncomputed equality id in particular is removed,
    // never derived here: retaining the previous id would be observably
    // wrong, forcing a fingerprint would violate the cheap-only rule.)
    ColorSpaceInfo info = config.get_color_space_info(color_space);
    auto set_or_erase   = [&](const char* name, string_view value) {
        if (value.size())
            spec.attribute(name, value);
        else
            spec.erase_attribute(name);
    };
    set_or_erase("oiio:ColorSpace:state", info.image_state());
    set_or_erase("oiio:ColorSpace:encoding", info.encoding());
    set_or_erase("oiio:ColorSpace:range", info.range());
    set_or_erase("oiio:ColorSpace:equality_id", info.equality_id());
}



void
erase_color_state_descriptors(ImageSpec& spec)
{
    spec.erase_attribute("oiio:ColorSpace:state");
    spec.erase_attribute("oiio:ColorSpace:encoding");
    spec.erase_attribute("oiio:ColorSpace:range");
    spec.erase_attribute("oiio:ColorSpace:equality_id");
}

}  // namespace pvt

OIIO_NAMESPACE_END

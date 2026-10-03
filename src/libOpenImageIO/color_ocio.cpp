// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
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

#include "imageio_pvt.h"
#include "interop_config.h"

// Same layout as OCIO_VERSION_HEX in OpenColorABI.h: 0x01050200 == 1.5.2.
#define MAKE_OCIO_VERSION_HEX(maj, min, patch) \
    (((maj) << 24) | ((min) << 16) | ((patch) << 8))
static_assert(MAKE_OCIO_VERSION_HEX(1, 5, 2) == 0x01050200,
              "MAKE_OCIO_VERSION_HEX must match the OCIO_VERSION_HEX layout");

#include <OpenColorIO/OpenColorIO.h>

namespace OCIO = OCIO_NAMESPACE;


OIIO_NAMESPACE_3_1_BEGIN

#if 1 || !defined(NDEBUG) /* allow color configuration debugging */
static bool colordebug = Strutil::stoi(Sysutil::getenv("OIIO_DEBUG_COLOR"))
                         || Strutil::stoi(Sysutil::getenv("OIIO_DEBUG_ALL"));
#    define DBG(...)    \
        if (colordebug) \
        Strutil::print(__VA_ARGS__)
#else
#    define DBG(...)
#endif


static int disable_ocio = Strutil::stoi(Sysutil::getenv("OIIO_DISABLE_OCIO"));
static int disable_builtin_configs = Strutil::stoi(
    Sysutil::getenv("OIIO_DISABLE_BUILTIN_OCIO_CONFIGS"));
static OCIO::ConstConfigRcPtr ocio_current_config;

// OCIO 2.3 and 2.4 compose back-to-back gamma/exponent ops in the wrong
// direction, so skip that optimization before OCIO 2.5.0 (which has the fix,
// AcademySoftwareFoundation/OpenColorIO#2154).
#if OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 5, 0)
static constexpr auto ocio_optimization = OCIO::OptimizationFlags(
    OCIO::OPTIMIZATION_DEFAULT & ~OCIO::OPTIMIZATION_COMP_GAMMA);
#else
static constexpr auto ocio_optimization = OCIO::OPTIMIZATION_DEFAULT;
#endif
static bool
valid_interop_id(string_view id)
{
    int colons       = 0;
    size_t token_len = 0;
    for (unsigned char c : id) {
        if (c == ':') {
            if (++colons > 2 || (token_len == 0 && colons == 1))
                return false;
            token_len = 0;
            continue;
        }
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                             || c == '.' || c == '-' || c == '_' || c == '~'
                             || c == '/' || c == '*' || c == '#' || c == '%'
                             || c == '^' || c == '+' || c == '(' || c == ')'
                             || c == '[' || c == ']' || c == '|';
        if (!allowed)
            return false;
        ++token_len;
    }
    return token_len != 0;
}

static std::string
sanitize_id_token(string_view name)
{
    std::vector<uint32_t> codepoints;
    Strutil::utf8_to_unicode(name, codepoints);
    std::string result;
    result.reserve(codepoints.size());
    for (uint32_t c : codepoints) {
        if (c > 127)
            result += '^';
        else if (c >= 'A' && c <= 'Z')
            result += char(c - 'A' + 'a');
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                 || (c && strchr(".-_~/*#%^+()[]|", int(c))))
            result += char(c);
        else {
            switch (c) {
            case ' ':
            case '\t':
            case '\n':
            case '\r': result += '_'; break;
            case '{':
            case '<': result += '('; break;
            case '}':
            case '>': result += ')'; break;
            case ',': result += '.'; break;
            case ';':
            case ':': result += '|'; break;
            case '\'':
            case '"': result += '#'; break;
            case '\\': result += '/'; break;
            default: result += '*'; break;
            }
        }
    }
    return result;
}


// OCIO before 2.3.2 drops the default view transform name on copy, and native
// context copies may drop the environment mode; keep both on every copy.
static OCIO::ConfigRcPtr
copy_config(const OCIO::ConstConfigRcPtr& config)
{
#if OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 3, 2)
    const std::string default_view_transform
        = config->getDefaultViewTransformName();
#endif
    auto copy = config->createEditableCopy();
    if (copy->getEnvironmentMode() != config->getEnvironmentMode())
        copy->setEnvironmentMode(config->getEnvironmentMode());
#if OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 3, 2)
    if (!default_view_transform.empty()
        && default_view_transform != copy->getDefaultViewTransformName())
        copy->setDefaultViewTransformName(default_view_transform.c_str());
#endif
    return copy;
}



// The config's current context plus one variable per comma-separated key/value
// pair. Empty or mismatched lists leave the config's context unchanged.
static OCIO::ConstContextRcPtr
effective_context(const OCIO::ConstConfigRcPtr& config, string_view context_key,
                  string_view context_value)
{
    OCIO::ConstContextRcPtr context = config->getCurrentContext();
    auto keys                       = Strutil::splits(context_key, ",");
    auto values                     = Strutil::splits(context_value, ",");
    if (keys.size() && values.size() && keys.size() == values.size()) {
        OCIO::ContextRcPtr ctx = context->createEditableCopy();
        for (size_t i = 0; i < keys.size(); ++i)
            ctx->setStringVar(keys[i].c_str(), values[i].c_str());
        context = ctx;
    }
    return context;
}



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
                      ustring file           = ustring(),
                      ustring namedtransform = ustring(), bool inverse = false)
        : inputColorSpace(in)
        , outputColorSpace(out)
        , context_key(key)
        , context_value(val)
        , looks(looks)
        , file(file)
        , namedtransform(namedtransform)
        , inverse(inverse)
    {
        hash = inputColorSpace.hash() + 14033ul * outputColorSpace.hash()
               + 823ul * context_key.hash() + 28411ul * context_value.hash()
               + 1741ul
                     * (looks.hash() + display.hash() + view.hash()
                        + file.hash() + namedtransform.hash())
               + (inverse ? 6421 : 0);
        // N.B. no separate multipliers for looks, display, view, file,
        // namedtransform, because they're never used for the same lookup.
    }

    friend bool operator<(const ColorProcCacheKey& a,
                          const ColorProcCacheKey& b)
    {
        return std::tie(a.hash, a.inputColorSpace, a.outputColorSpace,
                        a.context_key, a.context_value, a.looks, a.display,
                        a.view, a.file, a.namedtransform, a.inverse)
               < std::tie(b.hash, b.inputColorSpace, b.outputColorSpace,
                          b.context_key, b.context_value, b.looks, b.display,
                          b.view, b.file, b.namedtransform, b.inverse);
    }

    friend bool operator==(const ColorProcCacheKey& a,
                           const ColorProcCacheKey& b)
    {
        return std::tie(a.hash, a.inputColorSpace, a.outputColorSpace,
                        a.context_key, a.context_value, a.looks, a.display,
                        a.view, a.file, a.namedtransform, a.inverse)
               == std::tie(b.hash, b.inputColorSpace, b.outputColorSpace,
                           b.context_key, b.context_value, b.looks, b.display,
                           b.view, b.file, b.namedtransform, b.inverse);
    }
    ustring inputColorSpace;
    ustring outputColorSpace;
    ustring context_key;
    ustring context_value;
    ustring looks;
    ustring display;
    ustring view;
    ustring file;
    ustring namedtransform;
    bool inverse;
    size_t hash;
};


struct ColorProcCacheKeyHasher {
    size_t operator()(const ColorProcCacheKey& c) const { return c.hash; }
};


typedef tsl::robin_map<ColorProcCacheKey, ColorProcessorHandle,
                       ColorProcCacheKeyHasher>
    ColorProcessorMap;



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


// Runtime evidence a retained ColorSpaceInfo record can carry. These are
// defined here, outside the anonymous namespace, because ColorSpaceInfo::Impl
// has external linkage and holds them. They are measured much further down,
// where the probes and the tolerances live.
//
// The neutral-axis profile of a conversion: what a set of probes came back as,
// and the slopes between them normalized at one anchor.
struct TransferSignature {
    std::vector<double> values;
    std::vector<double> slopes;
    bool valid() const { return !slopes.empty(); }
};

struct ProbeResponse {
    std::vector<std::array<float, 4>> samples;
};

struct AnalyticResponse {
    // Runtime responses, never serialized or checked in.
    std::vector<std::array<float, 4>> samples;
    std::array<double, 9> linear { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    int transfer = 0;  // 0 linear, 1 power, 2 power with linear toe, 3 log
    double gamma = 1.0, offset = 0.0;
    OCIO::NegativeStyle negative       = OCIO::NEGATIVE_PASS_THRU;
    OCIO::TransformDirection direction = OCIO::TRANSFORM_DIR_FORWARD;
    // The separated log curve, retained rather than reduced to "some log".
    // The end-to-end samples cannot stand in for it: they carry the gamut
    // matrix as well, and one encoding may reach one gamut through different
    // chromatic adaptations. The kind participates in comparison because an
    // affine log and a camera log sharing every affine parameter still differ
    // exactly in their behavior below the linear break.
    int log_kind = 0;  // 0 none, 1 log, 2 log affine, 3 log camera
    double base = 0.0, log_slope = 1.0, log_offset = 0.0;
    double lin_slope = 1.0, lin_offset = 0.0, lin_break = 0.0;
    double linear_slope   = 0.0;
    bool has_linear_slope = false;
    // The separated curve as the native operation it was authored as, so two
    // spellings of one curve can be compared by what they do rather than by
    // the parameters they state. Null for a definition with no curve.
    OCIO::ConstTransformRcPtr transfer_op;
};

struct AnalyticPair : public std::array<AnalyticResponse, 2> {};

struct ColorSpaceInfo::Impl {
    std::string identity;
    bool identity_evaluated = false;
    bool analytic_evaluated = false;
    // A native acquisition raised while this was being derived, so nothing
    // about it is a completed verdict and no consumer may publish it.
    bool incomplete = false;
    std::array<float, 8> chromaticities {};
    bool has_chromaticities      = false;
    bool chromaticities_computed = false;
    bool chromaticities_derived  = false;
    float gamma                  = 0.0f;
    bool transfer_computed       = false;
    bool transfer_derived        = false;
    // Present only on a record retained under the transfer-measurement key
    // variant, and never on an ordinary gamma/gamut/identity record. The mode
    // in the cache key is what keeps the two apart, so neither is ever served
    // for the other's question.
    std::shared_ptr<const TransferSignature> transfer;
    // Successful runtime evidence about this target definition. Ordinary and
    // measured records may reuse it, while retaining independent verdicts.
    std::shared_ptr<const ProbeResponse> target_probe_response;
    std::shared_ptr<const AnalyticPair> target_analytic;
    // What a measurement of the curve was classified as, and the published
    // family it named. Written only by the enrichment an explicit derivation
    // performs, and only where no exponent was established, so these are read
    // exactly when `gamma` has nothing to say.
    ColorTransferFunctionKind transfer_kind
        = ColorTransferFunctionKind::Undetermined;
    std::string transfer_name;
    std::string equality_id, color_interop_id, encoding, image_state;
    bool equality_computed = false, equality_derived = false;
    bool interop_computed = false, interop_derived = false;
    bool encoding_computed = false, encoding_derived = false;
    bool image_state_computed = false, image_state_derived = false;
};

ColorSpaceInfo::ColorSpaceInfo() noexcept                      = default;
ColorSpaceInfo::ColorSpaceInfo(const ColorSpaceInfo&) noexcept = default;
ColorSpaceInfo::ColorSpaceInfo(ColorSpaceInfo&&) noexcept      = default;
ColorSpaceInfo& ColorSpaceInfo::operator=(const ColorSpaceInfo&) noexcept
    = default;
ColorSpaceInfo& ColorSpaceInfo::operator=(ColorSpaceInfo&&) noexcept = default;
ColorSpaceInfo::~ColorSpaceInfo()                                    = default;
bool
ColorSpaceInfo::valid() const noexcept
{
    return bool(m_impl);
}
cspan<float>
ColorSpaceInfo::chromaticities() const noexcept
{
    return m_impl && m_impl->has_chromaticities
               ? cspan<float>(m_impl->chromaticities)
               : cspan<float>();
}
float
ColorSpaceInfo::transfer_function_gamma() const noexcept
{
    return m_impl ? m_impl->gamma : 0.0f;
}
// Both of these read fields and nothing else. An established exponent answers
// first, which is what keeps the exponent and the kind from ever disagreeing
// and what makes a definition read as a pure power report the same kind
// whether or not its curve was also measured.
ColorTransferFunctionKind
ColorSpaceInfoAccess::transfer_function_kind(const ColorSpaceInfo& info) noexcept
{
    const auto& m_impl = info.m_impl;
    if (!m_impl)
        return ColorTransferFunctionKind::Undetermined;
    if (m_impl->gamma == 1.0f)
        return ColorTransferFunctionKind::Linear;
    if (m_impl->gamma > 0.0f)
        return ColorTransferFunctionKind::Power;
    return m_impl->transfer_kind;
}
string_view
ColorSpaceInfoAccess::transfer_function_name(const ColorSpaceInfo& info) noexcept
{
    const auto& m_impl = info.m_impl;
    return m_impl
                   && transfer_function_kind(info)
                          == ColorTransferFunctionKind::Named
               ? string_view(m_impl->transfer_name)
               : string_view();
}
string_view
ColorSpaceInfoAccess::equality_id(const ColorSpaceInfo& info) noexcept
{
    const auto& m_impl = info.m_impl;
    return m_impl ? string_view(m_impl->equality_id) : string_view();
}
string_view
ColorSpaceInfoAccess::color_interop_id(const ColorSpaceInfo& info) noexcept
{
    const auto& m_impl = info.m_impl;
    return m_impl ? string_view(m_impl->color_interop_id) : string_view();
}
string_view
ColorSpaceInfoAccess::image_state(const ColorSpaceInfo& info) noexcept
{
    const auto& m_impl = info.m_impl;
    return m_impl ? string_view(m_impl->image_state) : string_view();
}
bool
ColorSpaceInfoAccess::computed(const ColorSpaceInfo& info,
                               ColorSpaceInfoField field) noexcept
{
    const auto& m_impl = info.m_impl;
    if (!m_impl)
        return false;
    switch (field) {
    case ColorSpaceInfoField::Chromaticities:
        return m_impl->chromaticities_computed;
    case ColorSpaceInfoField::TransferFunction:
        return m_impl->transfer_computed;
    case ColorSpaceInfoField::EqualityID: return m_impl->equality_computed;
    case ColorSpaceInfoField::ColorInteropID: return m_impl->interop_computed;
    case ColorSpaceInfoField::Encoding: return m_impl->encoding_computed;
    case ColorSpaceInfoField::ImageState: return m_impl->image_state_computed;
    }
    return false;
}
bool
ColorSpaceInfoAccess::available(const ColorSpaceInfo& info,
                                ColorSpaceInfoField field) noexcept
{
    const auto& m_impl = info.m_impl;
    if (!m_impl)
        return false;
    switch (field) {
    case ColorSpaceInfoField::Chromaticities: return m_impl->has_chromaticities;
    case ColorSpaceInfoField::TransferFunction:
        return transfer_function_kind(info)
               != ColorTransferFunctionKind::Undetermined;
    case ColorSpaceInfoField::EqualityID: return !m_impl->equality_id.empty();
    case ColorSpaceInfoField::ColorInteropID:
        return !m_impl->color_interop_id.empty();
    case ColorSpaceInfoField::Encoding: return !m_impl->encoding.empty();
    case ColorSpaceInfoField::ImageState: return !m_impl->image_state.empty();
    }
    return false;
}
bool
ColorSpaceInfoAccess::derived(const ColorSpaceInfo& info,
                              ColorSpaceInfoField field) noexcept
{
    const auto& m_impl = info.m_impl;
    if (!m_impl)
        return false;
    switch (field) {
    case ColorSpaceInfoField::Chromaticities:
        return m_impl->chromaticities_derived;
    case ColorSpaceInfoField::TransferFunction: return m_impl->transfer_derived;
    case ColorSpaceInfoField::EqualityID: return m_impl->equality_derived;
    case ColorSpaceInfoField::ColorInteropID: return m_impl->interop_derived;
    case ColorSpaceInfoField::Encoding: return m_impl->encoding_derived;
    case ColorSpaceInfoField::ImageState: return m_impl->image_state_derived;
    }
    return false;
}


struct CSInfo {
    std::string name;  // Name of this color space
    int index;         // More than one can have the same index -- aliases
    enum Flags {
        none               = 0,
        is_linear_response = 1,  // any cs with linear transfer function
        is_scene_linear    = 2,  // equivalent to scene_linear
        is_srgb_display = 4,  // sRGB (primaries, and transfer function) display
        is_srgb_scene   = 8,  // sRGB (primaries, and transfer function) scene
        is_lin_srgb     = 16,   // sRGB/Rec709 primaries, linear response
        is_ACEScg       = 32,   // ACEScg
        is_Rec709       = 64,   // Rec709 primaries and transfer function
        is_data         = 128,  // Non-color-managed data
        is_known = is_srgb_display | is_srgb_scene | is_lin_srgb | is_ACEScg
                   | is_Rec709

    };
    int m_flags = 0;
    bool active = true;
    std::string canonical;  // Canonical name for this color space
    std::string interop_id;
    float native_gamma = 0.0f;
    std::vector<std::string> aliases;
    // Native facts gathered in the one inventory pass that already visits
    // every definition. They are what the configuration states, never an
    // inference from a name: the authored encoding attribute verbatim, the
    // reference state OpenColorIO reports, and the configuration's own claim
    // that no shared identity describes this space.
    std::string encoding;
    bool display_referred = false;
    bool is_unique        = false;

    CSInfo(string_view name_, int index_, int flags_ = none,
           string_view canonical_ = "")
        : name(name_)
        , index(index_)
        , m_flags(flags_)
        , canonical(canonical_)
    {
    }

    void setflag(int flagval) { m_flags |= flagval; }

    int flags() const { return m_flags; }
};



// The configured space names OIIO has long taken as evidence of the first
// four `builtin_identities`. Consulted only when measurement gives no answer.
static int
named_identity(string_view name)
{
    static const char* names[][6] = {
        { "srgb_display", "sRGB - Display" },
        { "srgb_tx", "srgb_texture", "srgb texture", "sRGB - Texture", "sRGB" },
        { "lin_rec709", "Linear Rec.709 (sRGB)", "lin_srgb", "linear" },
        { "ACEScg", "lin_ap1" },
    };
    for (int identity = 0; identity < 4; ++identity)
        for (const char* n : names[identity])
            if (n && Strutil::iequals(name, n))
                return identity;
    return -1;
}



#ifdef OIIO_SITE_spi
// Explicit opt-in site conventions, not generic configured-name inference.
static void
classify_spi(CSInfo& cs)
{
    if (cs.flags() & CSInfo::is_data)
        return;
    if (Strutil::starts_with(cs.name, "cgln")) {
        cs.setflag(CSInfo::is_ACEScg | CSInfo::is_linear_response);
        cs.canonical = "lin_ap1_scene";
    } else if (cs.name == "srgbf" || cs.name == "srgbh" || cs.name == "srgb16"
               || cs.name == "srgb8") {
        cs.setflag(CSInfo::is_srgb_scene);
        cs.canonical = "srgb_rec709_scene";
    } else if (cs.name == "srgblnf" || cs.name == "srgblnh"
               || cs.name == "srgbln16" || cs.name == "srgbln8") {
        cs.setflag(CSInfo::is_lin_srgb);
        cs.canonical = "lin_rec709_scene";
    } else if (Strutil::starts_with(cs.name, "nc")) {
        cs.setflag(CSInfo::is_data);
    }
}
#endif



// Native facts own their strings independently of the working conversion config.
struct NativeCatalog {
    std::vector<CSInfo> spaces;
    std::map<std::string, size_t, std::less<>> names;
    // OCIO 2.5 authored IDs are also inbound selectors.
    std::map<std::string, size_t, std::less<>> interop_ids;
};

static spin_rw_mutex native_catalog_mutex;
static std::map<std::string, std::shared_ptr<const NativeCatalog>>
    native_catalogs;
static spin_rw_mutex identity_bridge_mutex;
static std::map<std::pair<std::string, int>, std::shared_ptr<const std::string>>
    identity_bridges;
static std::set<std::pair<std::string, int>> unplaced_bridges;

// Changing the authored artifact or preparation semantics invalidates derived
// results, but does not invalidate native catalogs.
static constexpr const char* reference_revision
    = "oiio-interop-identities-v1.1.0:aliases-v1";

static OCIO::ConstConfigRcPtr
internal_reference()
{
    static const OCIO::ConstConfigRcPtr reference = [] {
        std::string text(reinterpret_cast<const char*>(interop_config_bytes));
#if OCIO_VERSION_HEX < MAKE_OCIO_VERSION_HEX(2, 5, 0)
        // Only this fixed authored artifact uses these exact lines. Older OCIO
        // cannot retain this metadata; all transforms still go through OCIO.
        std::istringstream lines(text);
        std::string line;
        text.clear();
        while (std::getline(lines, line))
            if (!Strutil::starts_with(line, "    interop_id:"))
                text += line + '\n';
#endif
        std::istringstream input(text);
        auto cfg = copy_config(OCIO::Config::CreateFromStream(input));
        const char* names[][2] = { { "lin_ap0_scene", "ACES2065-1" },
                                   { "lin_ap1_scene", "ACEScg" },
                                   { "lin_rec709_scene",
                                     "Linear Rec.709 (sRGB)" },
                                   { "lin_p3d65_scene", "Linear P3-D65" },
                                   { "lin_rec2020_scene", "Linear Rec.2020" } };
        for (const auto& name : names) {
            auto cs = cfg->getColorSpace(name[0])->createEditableCopy();
            cs->addAlias(name[1]);
            cfg->addColorSpace(cs);
        }
        DBG("Internal reference prepared once\n");
        return OCIO::ConstConfigRcPtr(cfg);
    }();
    return reference;
}



// A color space of the built-in config, the `reference` every recognition pass
// below is handed. OCIO finds a name by scanning every space and alias, and the
// passes ask about the same built-in names and identities for every space they
// measure, so each answer is kept.
static OCIO::ConstColorSpaceRcPtr
reference_space(const char* name)
{
    static spin_rw_mutex mutex;
    static std::map<std::string, OCIO::ConstColorSpaceRcPtr> spaces;
    std::string key(name);
    {
        spin_rw_read_lock lock(mutex);
        auto found = spaces.find(key);
        if (found != spaces.end())
            return found->second;
    }
    auto cs = internal_reference()->getColorSpace(name);
    spin_rw_write_lock lock(mutex);
    return spaces.emplace(std::move(key), cs).first->second;
}



static const char* builtin_identities[] = { "srgb_rec709_display",
                                            "srgb_rec709_scene",
                                            "lin_rec709_scene", "lin_ap1_scene",
                                            "ocio:itu709_rec709_scene" };

namespace {

bool is_interop_id_spelling(string_view name);
const char* legacy_interop_selector(string_view identity);
std::string requested_identity(string_view name);
const char* narrow_identification(OCIO::ConfigRcPtr& analysis,
                                  OCIO::ConstConfigRcPtr config,
                                  const char* identity);
}  // namespace



// Hidden implementation of ColorConfig
class ColorConfig::Impl {
public:
    OCIO::ConfigRcPtr config_;

private:
    std::vector<CSInfo> colorspaces;  // Synthetic no-config inventory only
    std::shared_ptr<const NativeCatalog> m_catalog;
    mutable std::array<std::shared_ptr<const std::string>, 5> m_bridges;
    mutable spin_rw_mutex m_mutex;
    // Empty disables process-shared interop lookup memoization. Otherwise this
    // combines OCIO's structural/resource and effective-context cache IDs.
    std::string m_interop_cache_id;
    std::string m_active_cache_key;
    // Catalog identity only: endpoint admission must precede publication.
    std::string m_properties_cache_id;
    mutable std::atomic<bool> m_interop_cache_safe { true };
    mutable std::atomic<int> m_failed_recognition_flags { 0 };
    mutable std::string m_error;
    // Siblings of this Impl bound to caller-supplied contexts, keyed by OCIO's
    // cache ID of a context holding only the overrides this configuration (or
    // the queried name) reads. At most max_views are retained.
    mutable std::map<std::string, std::shared_ptr<const Impl>> m_views;
    // Whether this configuration's text reads each context variable.
    mutable std::map<std::string, bool> m_reads_variable;
    ColorProcessorMap colorprocmap;  // cache of ColorProcessors
    atomic_int colorprocs_requested;
    atomic_int colorprocs_created;
    std::string m_configname;

public:
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
        if (m_catalog) {
            auto it = m_catalog->names.find(Strutil::lower(name));
            return it == m_catalog->names.end()
                       ? nullptr
                       : &m_catalog->spaces[it->second];
        }
        for (const auto& cs : colorspaces)
            if (cs.name == name)
                return &cs;
        return nullptr;
    }

    // find() without roles: a name or alias only.
    const CSInfo* find_name_or_alias(string_view name) const
    {
        const CSInfo* cs = find(name);
        if (!cs || Strutil::iequals(cs->name, name))
            return cs;
        for (const auto& alias : cs->aliases)
            if (Strutil::iequals(alias, name))
                return cs;
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

    int getNumColorSpaces() const
    {
        return int(m_catalog ? m_catalog->spaces.size() : colorspaces.size());
    }

    const char* getColorSpaceNameByIndex(int index) const
    {
        return (m_catalog ? m_catalog->spaces : colorspaces)[index].name.c_str();
    }

    string_view resolve(string_view name, bool* success = nullptr,
                        int* required_recognition = nullptr,
                        bool allow_identification = true) const;
    string_view resolve_exact(string_view name, bool* success,
                              int* required_recognition,
                              bool allow_identification) const;
    string_view resolve_local(string_view name) const;
    string_view resolve_authored_interop_id(string_view name) const;
    bool has_named_transform(string_view name) const;

    std::string configured_space(string_view name) const;

    // This configuration seen under one effective context: `this` when the
    // overrides select the context it already has or name only variables
    // neither the configuration nor `name` reads, a sibling otherwise, or null
    // when that sibling could not be built.
    std::shared_ptr<const Impl> context_view(string_view context_key,
                                             string_view context_value,
                                             string_view name = {}) const;

    bool equivalent_resolved(string_view original1, string_view resolved1,
                             std::optional<const CSInfo*>& csi1,
                             string_view color_space2, bool* success = nullptr,
                             int* required_recognition = nullptr) const;

    string_view get_color_interop_id(string_view colorspace) const;
    string_view get_color_equality_id(string_view colorspace,
                                      bool* success = nullptr) const;
    // `measured_only` answers what the definition does rather than what it is
    // called or declared to be: every naming, declared and bridge hypothesis
    // is withheld and only the measuring passes may establish an identity.
    //
    // `enrich` (set only by the public queries) also classifies the transfer
    // curve; identity lookup and the writers never need that measurement.
    ColorSpaceInfo color_space_info(string_view colorspace, bool derive,
                                    bool measured_only = false,
                                    bool enrich        = false) const;
    // A built-in identity this config does not define, described by the
    // built-in interop-identities config, as conversions with it are.
    ColorSpaceInfo builtin_identity_info(string_view colorspace) const;
    // The same query on a view, with the one thing a view adds to a name: a
    // selector that spells a context variable expands through this view's own
    // context, as get_color_equality_id's admission already does.
    ColorSpaceInfo context_color_space_info(string_view colorspace, bool derive,
                                            bool enrich = false) const;
    // The bounded walk that admits a measurement of a local definition, and
    // the retained curve measurement itself, which the enrichment below reads
    // so that a definition is measured once.
    bool structurally_admitted(OCIO::ConstColorSpaceRcPtr target) const;
    TransferSignature retained_transfer(const std::string& name,
                                        OCIO::ConstColorSpaceRcPtr target) const;
    // A snapshot with the transfer kind classified, where an exponent left it
    // unanswered. Returns `base` unchanged when nothing admits or reaches a
    // measurement, which is not retained as a verdict.
    ColorSpaceInfo enriched(const CSInfo& cs, const ColorSpaceInfo& base,
                            const std::pair<std::string, std::string>& key,
                            bool publish) const;

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

    bool isColorSpaceLinear(string_view name) const;
    bool isData(string_view name) const;

private:
    void inventory();
    void classify_by_name(CSInfo& cs);
    string_view bridge(int identity, bool* success) const;
    // The bridge any wrapper of this config already retained, never computed:
    // the cheap query must not depend on which wrapper asked first.
    std::shared_ptr<const std::string> retained_bridge(int identity) const
    {
        if (!m_interop_cache_safe || m_interop_cache_id.empty())
            return {};
        spin_rw_read_lock lock(identity_bridge_mutex);
        auto it = identity_bridges.find({ m_interop_cache_id, identity });
        return it == identity_bridges.end() ? nullptr : it->second;
    }
    string_view named_space(int identity) const;
    std::shared_ptr<const Impl>
    make_view(const std::vector<std::string>& keys,
              const std::vector<std::string>& values) const;
};



// ColorConfig utility to take inventory of the color spaces available.
// Without a usable config, enumerate only explicit supported identities.
void
ColorConfig::Impl::inventory()
{
    DBG("inventorying config {}\n", configname());
    if (config_ && !disable_ocio) {
        try {
            const int n
                = config_->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                             OCIO::COLORSPACE_ALL);
            bool nonraw = false;
            for (int i = 0; i < n; ++i)
                nonraw |= !Strutil::iequals(config_->getColorSpaceNameByIndex(
                                                OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                                OCIO::COLORSPACE_ALL, i),
                                            "raw");
            if (nonraw) {
                // Null context requests the native structural ID, avoiding LUT
                // file inspection. Active selection may come from a native
                // API/environment override absent from serialization/context.
                std::string key = config_->getCacheID(nullptr);
                key += '\x1f';
                key += config_->getCurrentContext()->getCacheID();
                std::vector<std::string> active_names;
                const int active = config_->getNumColorSpaces(
                    OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ACTIVE);
                active_names.reserve(active);
                for (int i = 0; i < active; ++i) {
                    active_names.emplace_back(config_->getColorSpaceNameByIndex(
                        OCIO::SEARCH_REFERENCE_SPACE_ALL,
                        OCIO::COLORSPACE_ACTIVE, i));
                    const auto& name = active_names.back();
                    m_active_cache_key += std::to_string(name.size()) + ":"
                                          + name;
                }
                key += '\x1f';
                key += m_active_cache_key;
                m_properties_cache_id = key;
                {
                    spin_rw_read_lock lock(native_catalog_mutex);
                    auto it = native_catalogs.find(key);
                    if (it != native_catalogs.end()) {
                        m_catalog = it->second;
                        DBG("Native catalog reused: {} spaces\n", n);
                        return;
                    }
                }
                auto catalog = std::make_shared<NativeCatalog>();
                catalog->spaces.reserve(n);
                for (int i = 0; i < n; ++i) {
                    auto cs = config_->getColorSpace(
                        config_->getColorSpaceNameByIndex(
                            OCIO::SEARCH_REFERENCE_SPACE_ALL,
                            OCIO::COLORSPACE_ALL, i));
                    catalog->spaces.emplace_back(cs->getName(), i,
                                                 cs->isData() ? CSInfo::is_data
                                                              : CSInfo::none);
                    auto& row  = catalog->spaces.back();
                    row.active = false;
                    string_view encoding(cs->getEncoding());
                    if (!cs->isData()
                        && (encoding == "scene-linear"
                            || encoding == "display-linear"))
                        row.native_gamma = 1.0f;
                    row.encoding         = encoding;
                    row.display_referred = cs->getReferenceSpaceType()
                                           == OCIO::REFERENCE_SPACE_DISPLAY;
                    row.is_unique        = cs->hasCategory("is-unique");
                    catalog->names.emplace(Strutil::lower(row.name), i);
                    for (decltype(cs->getNumAliases()) j = 0;
                         j < cs->getNumAliases(); ++j) {
                        row.aliases.emplace_back(cs->getAlias(j));
                        catalog->names.emplace(Strutil::lower(
                                                   row.aliases.back()),
                                               i);
                    }
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 5, 0)
                    row.interop_id = cs->getInteropID();
                    if (!row.interop_id.empty()
                        && !Strutil::iequals(row.interop_id, "data")
                        && !Strutil::iequals(row.interop_id, "bypass")
                        && !Strutil::iequals(row.interop_id, "unknown"))
                        catalog->interop_ids.emplace(row.interop_id, i);
#endif
                }
                for (int i = 0; i < config_->getNumRoles(); ++i) {
                    const char* role = config_->getRoleName(i);
                    if (auto cs = config_->getColorSpace(role)) {
                        auto it = catalog->names.find(
                            Strutil::lower(cs->getName()));
                        if (it != catalog->names.end())
                            catalog->names.emplace(Strutil::lower(role),
                                                   it->second);
                    }
                }
#ifdef OIIO_SITE_spi
                for (auto& cs : catalog->spaces) {
                    classify_spi(cs);
                    if (cs.canonical.empty())
                        continue;
                    // Native selectors, including legacy aliases and roles,
                    // take precedence over implicit site selectors.
                    const char* legacy = legacy_interop_selector(cs.canonical);
                    if (!legacy || !catalog->names.count(legacy))
                        catalog->names.emplace(cs.canonical, cs.index);
                }
#endif
                for (const auto& name : active_names) {
                    auto it = catalog->names.find(Strutil::lower(name));
                    if (it != catalog->names.end())
                        catalog->spaces[it->second].active = true;
                }
                spin_rw_write_lock lock(native_catalog_mutex);
                m_catalog = native_catalogs.emplace(key, catalog).first->second;
                DBG("Native catalog published: {} spaces, {} retained catalogs\n",
                    n, native_catalogs.size());
                return;
            }
        } catch (std::exception& e) {
            DBG("OCIO exception in inventory: {}", e.what());
            m_interop_cache_safe = false;
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
    add("srgb_rec709_display", 1, CSInfo::is_srgb_display);
    add("srgb_rec709_scene", 1, CSInfo::is_srgb_scene);
    add("sRGB", 1, CSInfo::is_srgb_scene);
    add("Rec709", 2, CSInfo::is_Rec709);

    for (auto&& cs : colorspaces)
        classify_by_name(cs);
}



void
ColorConfig::Impl::classify_by_name(CSInfo& cs)
{
    // Synthetic no-config conventions only. Configured spelling is never
    // evidence of primaries or transfer function.
    if (cs.name == "srgb_rec709_display")
        cs.setflag(CSInfo::is_srgb_display);
    else if (cs.name == "srgb_rec709_scene")
        cs.setflag(CSInfo::is_srgb_scene);
    else if (cs.name == "linear" || cs.name == "lin_rec709_scene")
        cs.setflag(CSInfo::is_lin_srgb | CSInfo::is_linear_response);
    else if (cs.name == "Rec709")
        cs.setflag(CSInfo::is_Rec709);

#ifdef OIIO_SITE_spi
    classify_spi(cs);
#endif

    // Set up some canonical names
    if (cs.flags() & CSInfo::is_srgb_display)
        cs.canonical = "srgb_rec709_display";
    else if (cs.flags() & CSInfo::is_srgb_scene)
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
    }
}



string_view
ColorConfig::Impl::bridge(int identity, bool* success) const
{
    static const int flags[] = { CSInfo::is_srgb_display, CSInfo::is_srgb_scene,
                                 CSInfo::is_lin_srgb, CSInfo::is_ACEScg,
                                 CSInfo::is_Rec709 };
    if (!config_ || disable_ocio || disable_builtin_configs)
        return {};
    {
        spin_rw_read_lock lock(m_mutex);
        if (m_bridges[identity])
            return *m_bridges[identity];
    }
    const bool shared = m_interop_cache_safe && !m_interop_cache_id.empty();
    const auto key    = std::make_pair(m_interop_cache_id, identity);
    std::shared_ptr<const std::string> result;
    bool unplaced = false;
    if (shared) {
        spin_rw_read_lock lock(identity_bridge_mutex);
        auto it = identity_bridges.find(key);
        if (it != identity_bridges.end()) {
            result = it->second;
            DBG("Identity bridge shared hit: {}\n",
                builtin_identities[identity]);
        }
        unplaced = unplaced_bridges.count(key);
    }
    if (!result && !unplaced) {
        try {
            // IdentifyBuiltinColorSpace mutates BOTH processor-cache flags.
            // Detached analysis copies never affect conversion handles.
            auto reference = copy_config(internal_reference());
            auto analysis  = copy_config(config_);
            narrow_identification(analysis, config_,
                                  builtin_identities[identity]);
            DBG("Identity bridge cold work: {}\n",
                builtin_identities[identity]);
            const char* name = nullptr;
            try {
                name = OCIO::Config::IdentifyBuiltinColorSpace(
                    analysis, reference, builtin_identities[identity]);
            } catch (const OCIO::Exception& e) {
                // OCIO (2.3-2.5) raises the same exception type for a
                // completed miss and for a failure, and offers no structural
                // check, so its message is matched here and below. Changed
                // text fails toward re-measuring, never toward a verdict.
                const std::string no_match
                    = std::string(
                          "Heuristics were not able to find an equivalent to the requested color space: ")
                      + builtin_identities[identity] + ".";
                if (no_match != e.what())
                    throw;
                DBG("Identity bridge completed miss: {}\n",
                    builtin_identities[identity]);
            }
            const CSInfo* cs = name ? find(name) : nullptr;
            result           = std::make_shared<const std::string>(
                cs && !(cs->flags() & CSInfo::is_data) ? cs->name : "");
        } catch (const std::exception& e) {
            DBG("Identity bridge incomplete: {}\n", e.what());
            // Without interchange roles OCIO fails the same way on every call,
            // so that failure is not measured again. It stays incomplete.
            const string_view what(e.what());
            if (shared
                && (Strutil::ends_with(what, "Please set the interchange roles.")
                    || what
                           == "The supplied config does not have a color space for the reference.")) {
                spin_rw_write_lock lock(identity_bridge_mutex);
                unplaced_bridges.insert(key);
            }
        }
        if (result && shared) {
            spin_rw_write_lock lock(identity_bridge_mutex);
            result = identity_bridges.emplace(key, result).first->second;
            DBG("Identity bridge retained entries: {} (five per eligible config/context)\n",
                identity_bridges.size());
        }
    }
    if (!result) {
        m_failed_recognition_flags.fetch_or(flags[identity]);
        if (success)
            *success = false;
        return {};
    }
    spin_rw_write_lock lock(m_mutex);
    if (!m_bridges[identity])
        m_bridges[identity] = result;
    return *m_bridges[identity];
}



string_view
ColorConfig::Impl::named_space(int identity) const
{
    if (m_catalog)
        for (const auto& cs : m_catalog->spaces)
            if (!(cs.flags() & CSInfo::is_data)
                && named_identity(cs.name) == identity)
                return cs.name;
    return {};
}



// A sibling of this Impl bound to one effective context. The native catalog,
// the active-set key and the configuration's identity are facts no context
// changes, so they are shared rather than gathered again; only the working
// configuration is copied, once per distinct context, so no call reparses a
// configuration or clones one per query. The copy carries the caller's
// variables as its own environment defaults, which is the only way
// OpenColorIO lets a configuration hold a context -- and it resolves them to
// exactly the values createColorProcessor sets on its context, so what is
// measured here is what will convert. Everything this sibling publishes is
// keyed by its own cache ID, which carries the effective context, so a result
// measured under one context can never answer for another.
std::shared_ptr<const ColorConfig::Impl>
ColorConfig::Impl::make_view(const std::vector<std::string>& keys,
                             const std::vector<std::string>& values) const
{
    auto copy = copy_config(config_);
    for (size_t i = 0; i < keys.size(); ++i)
        copy->addEnvironmentVar(keys[i].c_str(), values[i].c_str());
    auto view                  = std::make_shared<Impl>();
    view->config_              = copy;
    view->colorspaces          = colorspaces;
    view->m_catalog            = m_catalog;
    view->m_active_cache_key   = m_active_cache_key;
    view->m_configname         = m_configname;
    view->m_interop_cache_safe = m_interop_cache_safe.load();
    // Same construction and the same failure rule as init(): a key that cannot
    // be computed only withholds process-shared publication, and every result
    // is still evaluated for this call.
    // Only the interop key is needed: the properties key is the fallback for
    // a configuration whose interop key could not be computed, and a view
    // without one publishes nothing.
    try {
        auto context        = copy->getCurrentContext();
        std::string interop = copy->getCacheID(context);
        interop += '\x1f';
        interop += context->getCacheID();
        interop += '\x1f';
        interop += reference_revision;
        interop += '\x1f';
        interop += m_active_cache_key;
        view->m_interop_cache_id = std::move(interop);
    } catch (const std::exception& e) {
        DBG("Full native cache key unavailable for a context view of {}: {}\n",
            m_configname, e.what());
    }
    return view;
}



std::shared_ptr<const ColorConfig::Impl>
ColorConfig::Impl::context_view(string_view context_key,
                                string_view context_value,
                                string_view name) const
{
    // Not owned: the caller's ColorConfig outlives the call.
    const std::shared_ptr<const Impl> self(std::shared_ptr<const Impl>(), this);
    if (!config_ || disable_ocio)
        return self;
    auto keys   = Strutil::splits(context_key, ",");
    auto values = Strutil::splits(context_value, ",");
    if (keys.empty() || keys.size() != values.size())
        return self;  // effective_context's rule: nothing is overridden.
    try {
        // OCIO's own substitution decides whether a text reads a variable.
        auto reads = [](const std::string& text, const std::string& key) {
            auto probe = OCIO::Context::Create();
            probe->setStringVar(key.c_str(), "");
            OCIO::ContextRcPtr used = OCIO::Context::Create();
            probe->resolveStringVar(text.c_str(), used);
            return used->getNumStringVars() > 0;
        };
        std::vector<std::string> read_keys, read_values;
        for (size_t i = 0; i < keys.size(); ++i) {
            std::optional<bool> known;
            {
                spin_rw_read_lock lock(m_mutex);
                auto found = m_reads_variable.find(keys[i]);
                if (found != m_reads_variable.end())
                    known = found->second;
            }
            if (!known) {
                std::ostringstream text;
                config_->serialize(text);
                known = reads(text.str(), keys[i]);
                spin_rw_write_lock lock(m_mutex);
                m_reads_variable.emplace(keys[i], *known);
            }
            if (*known || reads(std::string(name), keys[i])) {
                read_keys.push_back(keys[i]);
                read_values.push_back(values[i]);
            }
        }
        if (read_keys.empty())
            return self;  // Nothing overridden is ever read.
        auto context = effective_context(config_, Strutil::join(read_keys, ","),
                                         Strutil::join(read_values, ","));
        // The context's own key covers every variable it holds: string
        // variables a ColorSpaceTransform or a "$VAR" name reads never name a
        // file, so the config's file-based key would collapse them.
        const std::string id = context->getCacheID();
        if (id == config_->getCurrentContext()->getCacheID())
            return self;  // The overrides changed nothing.
        {
            spin_rw_read_lock lock(m_mutex);
            auto found = m_views.find(id);
            if (found != m_views.end())
                return found->second;
        }
        auto view = make_view(read_keys,
                              read_values);  // Built outside the lock.
        spin_rw_write_lock lock(m_mutex);
        // At capacity, drop the view with the lowest context cache ID, not
        // the least recently used one. Results measured under a dropped view
        // stay in the shared caches.
        const size_t max_views = 16;
        if (m_views.size() >= max_views && !m_views.count(id))
            m_views.erase(m_views.begin());
        auto published = m_views.emplace(id, std::move(view));
        DBG("Context views retained for {}: {}\n", m_configname,
            m_views.size());
        return published.first->second;
    } catch (const std::exception& e) {
        // Acquiring a configuration is not a verdict about it: nothing is
        // retained, and the next call tries again.
        DBG("Context view unavailable for {}: {}\n", m_configname, e.what());
    }
    return nullptr;
}



ColorConfig::ColorConfig(string_view filename)
{
    (void)reset(filename);
}



ColorConfig::~ColorConfig()
{
}



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



bool
ColorConfig::Impl::init(string_view filename)
{
    OIIO_MAYBE_UNUSED Timer timer;
    bool ok = true;

    // If no filename was specified, use env $OCIO
    if (filename.empty())
        filename = Sysutil::getenv("OCIO");
    if (filename.empty() && !disable_builtin_configs)
        filename = "ocio://default";
    if (filename.size() && !OIIO::Filesystem::exists(filename)
        && !Strutil::istarts_with(filename, "ocio://")) {
        error("Requested non-existent OCIO config \"{}\"", filename);
        m_interop_cache_safe = false;
    } else {
        // Either filename passed, or taken from $OCIO, and it seems to exist
        try {
            configname(filename);
            auto cfg = OCIO::Config::CreateFromFile(
                std::string(filename).c_str());
            if (cfg)
                config_ = copy_config(cfg);
            if (config_ && Strutil::istarts_with(filename, "ocio://"))
                fix_config_file_rules(config_);
        } catch (std::exception& e) {
            error("Error reading OCIO config \"{}\": {}", filename, e.what());
            m_interop_cache_safe = false;
        } catch (...) {
            error("Error reading OCIO config \"{}\"", filename);
            m_interop_cache_safe = false;
        }
    }

    ok = config_.get() != nullptr;

    DBG("OCIO config {} loaded in {:0.2f} seconds\n", filename, timer.lap());

    inventory();
    // OCIO's config cache ID includes structure and resolved file revisions,
    // but not the context ID used to select it internally, so retain both.
    // Every file a transform references is resolved and hashed into this key by
    // OCIO itself, with a distinct marker for one it cannot resolve, so a
    // result published under it is exactly as fresh as the processor
    // OpenColorIO would hand back for the same config and context. Naming an
    // external resource is provenance, not a reason to refuse publication:
    // OCIO's own cache lifecycle is the freshness boundary, and an acquisition
    // that actually fails is refused where it happens and stays retryable.
    if (m_interop_cache_safe && config_ && !disable_ocio) {
        try {
            const auto context = config_->getCurrentContext();
            m_interop_cache_id = config_->getCacheID(context);
            m_interop_cache_id += '\x1f';
            m_interop_cache_id += context->getCacheID();
            m_interop_cache_id += '\x1f';
            m_interop_cache_id += reference_revision;
            m_interop_cache_id += '\x1f';
            m_interop_cache_id += m_active_cache_key;
        } catch (...) {
            // The structural key from inventory() still governs publication of
            // resource-free results; not having the full one is not a reason
            // to distrust the configuration.
            m_interop_cache_id.clear();
            DBG("Full native cache key unavailable for {}\n", filename);
        }
    } else if (m_interop_cache_safe) {
        m_interop_cache_id = "oiio:builtin";
    }

#if 1
    for (auto&& cs : colorspaces) {
        DBG("Color space '{}':\n", cs.name);
        if (cs.flags() & CSInfo::is_srgb_display)
            DBG("'{}' is srgb_display\n", cs.name);
        if (cs.flags() & CSInfo::is_srgb_scene)
            DBG("'{}' is srgb_scene\n", cs.name);
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
    DBG("OCIO config {} classified in {:0.2f} seconds\n", filename,
        timer.lap());

    return ok;
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

    m_impl.reset(new ColorConfig::Impl);
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
    if (getImpl()->config_ && !disable_ocio) {
        const CSInfo* cs = getImpl()->find(getImpl()->resolve(name));
        return cs ? cs->index : -1;
    }
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
        } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
ColorConfig::isColorSpaceActive(string_view name) const
{
    const CSInfo* cs = getImpl()->find(getImpl()->resolve(name));
    return cs ? cs->active : true;
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
    if (const CSInfo* cs = find(name))
        return cs->flags() & CSInfo::is_data;
    if (has_named_transform(name))
        return false;
    const auto colon     = name.find(':');
    const bool qualified = colon != string_view::npos
                           && valid_interop_id(Strutil::lower(name));
    string_view stripped;
    if (auto authored = resolve_authored_interop_id(name); !authored.empty())
        return find(authored)->flags() & CSInfo::is_data;
    if (qualified) {
        stripped = name.substr(colon + 1);
        if (const CSInfo* cs = find_name_or_alias(stripped))
            return cs->flags() & CSInfo::is_data;
        if (has_named_transform(stripped))
            return false;
        if (auto local = resolve_local(name); !local.empty())
            return find(local)->flags() & CSInfo::is_data;
        if (auto authored = resolve_authored_interop_id(stripped);
            !authored.empty())
            return find(authored)->flags() & CSInfo::is_data;
    }
    // Standard data is the last fallback after every native owner misses.
    return m_catalog && !disable_ocio && !disable_builtin_configs
           && Strutil::iequals(qualified ? stripped : name, "data");
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
        } catch (std::exception& e) {
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
        } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
            // Handle certain Shared View cases
            if (strcmp(c_str(name), "<USE_DISPLAY_NAME>") == 0)
                name = display;
            return c_str(name);
        } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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



string_view
ColorConfig::resolve(string_view name) const
{
    return getImpl()->resolve(name);
}



string_view
ColorConfig::Impl::resolve(string_view name, bool* success,
                           int* required_recognition,
                           bool allow_identification) const
{
    if (!m_catalog) {
        // No config: OIIO's informal aliases name the synthetic inventory.
        if (Strutil::iequals(name, "sRGB")
            || Strutil::iequals(name, "srgb_rec709_scene"))
            return "srgb_rec709_scene";
        if (Strutil::iequals(name, "srgb_rec709_display"))
            return "srgb_rec709_display";
        if (Strutil::iequals(name, "lin_srgb")
            || Strutil::iequals(name, "lin_rec709")
            || Strutil::iequals(name, "lin_rec709_scene")
            || Strutil::iequals(name, "linear"))
            return "linear";
        if (Strutil::iequals(name, "Rec709"))
            return "Rec709";
        return name;
    }
    if (const CSInfo* cs = find(name))
        return cs->name;
    if (has_named_transform(name))
        return name;

    // CIF Rec 03: an authored ID matching the whole name wins before any
    // namespace is stripped from the query.
    if (auto authored = resolve_authored_interop_id(name); !authored.empty())
        return authored;
    const auto colon = name.find(':');
    if (colon == string_view::npos || !valid_interop_id(Strutil::lower(name))) {
        auto exact = resolve_exact(name, success, required_recognition,
                                   allow_identification);
        return exact.empty() ? name : exact;
    }

    // A qualified spelling falls back in native-ownership order: strip one
    // namespace, then try this config's Annex C local identity. The remainder
    // matches only a name, alias, named transform or authored interop ID --
    // never a role, a recognized identity or a generic name. Only the whole
    // name may then select another endpoint by recognition.
    const string_view stripped = name.substr(colon + 1);
    if (const CSInfo* cs = find_name_or_alias(stripped))
        return cs->name;
    if (has_named_transform(stripped))
        return stripped;
    if (auto local = resolve_local(name); !local.empty())
        return local;
    if (auto authored = resolve_authored_interop_id(stripped);
        !authored.empty())
        return authored;
    if (auto exact = resolve_exact(name, success, required_recognition,
                                   allow_identification);
        !exact.empty())
        return exact;
    return name;
}


string_view
ColorConfig::Impl::resolve_exact(string_view name, bool* success,
                                 int* required_recognition,
                                 bool allow_identification) const
{
    if (m_catalog) {
        if (const CSInfo* cs = find(name))
            return cs->name;
        int identity = -1;
        int flag     = 0;
        if (Strutil::iequals(name, "srgb_rec709_display")) {
            identity = 0;
            flag     = CSInfo::is_srgb_display;
        } else if (Strutil::iequals(name, "srgb_rec709_scene")) {
            identity = 1;
            flag     = CSInfo::is_srgb_scene;
        } else if (Strutil::iequals(name, "lin_rec709_scene")) {
            identity = 2;
            flag     = CSInfo::is_lin_srgb;
        } else if (Strutil::iequals(name, "lin_ap1_scene")) {
            identity = 3;
            flag     = CSInfo::is_ACEScg;
        } else if (Strutil::iequals(name, "ocio:itu709_rec709_scene")) {
            identity = 4;
            flag     = CSInfo::is_Rec709;
        } else if (Strutil::iequals(name, "sRGB")) {
            // OIIO's generic selectors keep their established meanings.
            identity = 1;
            flag     = CSInfo::is_srgb_scene;
        } else if (Strutil::iequals(name, "lin_srgb")
                   || Strutil::iequals(name, "lin_rec709")
                   || Strutil::iequals(name, "linear")) {
            identity = 2;
            flag     = CSInfo::is_lin_srgb;
        } else if (Strutil::iequals(name, "ACEScg")) {
            identity = 3;
            flag     = CSInfo::is_ACEScg;
        }
        if (identity >= 0) {
            // Native legacy selectors include inactive spaces that OCIO's
            // transform-identification scan may not consider.
            if (const char* legacy = legacy_interop_selector(
                    builtin_identities[identity]))
                if (const CSInfo* cs = find(legacy);
                    cs && !(cs->flags() & CSInfo::is_data))
                    return cs->name;
#ifdef OIIO_SITE_spi
            if (const CSInfo* cs = find(builtin_identities[identity]);
                cs && !(cs->flags() & CSInfo::is_data))
                return cs->name;
#else
            // So does a space named (not aliased) by the identity itself,
            // which OIIO's generic names have always selected.
            if (const CSInfo* cs = find(builtin_identities[identity]);
                cs && !(cs->flags() & CSInfo::is_data)
                && Strutil::iequals(cs->name, builtin_identities[identity]))
                return cs->name;
#endif
            // Without identification the name alone stands in for it.
            if (required_recognition)
                *required_recognition |= flag;
            if (!allow_identification)
                return named_space(identity);
            auto local = bridge(identity, success);
            if (!local.empty())
                return local;
        }
        // Native selectors above always win. Only then may a portable identity
        // select the local encoding that measurement says carries it. Both
        // identifications are evidence gathered in the config's default
        // context, so a caller working under an override opts out of them.
        if (!allow_identification)
            return {};
        const std::string requested = requested_identity(name);
        if (!requested.empty()) {
            // Derivation matches only within a reference state, so a space in
            // the other state can carry the identity only by declaring it.
            const bool display = Strutil::ends_with(requested, "_display");
            const bool scene   = Strutil::ends_with(requested, "_scene");
            for (const auto& candidate : m_catalog->spaces) {
                if (candidate.flags() & CSInfo::is_data)
                    continue;
                if ((display || scene) && candidate.display_referred != display
                    && !Strutil::iequals(candidate.interop_id, requested))
                    continue;
                auto info = color_space_info(candidate.name, true);
                if (info.m_impl
                    && Strutil::iequals(info.m_impl->identity, requested))
                    return candidate.name;
            }
        }
        return identity >= 0 ? named_space(identity) : string_view();
    }

    // Synthetic catalogs contain explicit identities only.
    if (const CSInfo* cs = find(name))
        return cs->name;

    return {};
}


bool
ColorConfig::Impl::has_named_transform(string_view name) const
{
    if (!config_ || disable_ocio)
        return false;
    try {
        return bool(config_->getNamedTransform(c_str(name)));
    } catch (...) {
        return false;
    }
}


string_view
ColorConfig::Impl::resolve_local(string_view name) const
{
    if (!config_ || disable_ocio || !m_catalog || !config_->getName()[0])
        return {};
    const auto first  = name.find(':');
    const auto second = first == string_view::npos ? string_view::npos
                                                   : name.find(':', first + 1);
    if (second == string_view::npos
        || name.substr(first + 1, second - first - 1) != "local"
        || name.substr(0, first) != sanitize_id_token(config_->getName()))
        return {};
    const string_view base = name.substr(second + 1);
    for (const auto& cs : m_catalog->spaces) {
        if (sanitize_id_token(cs.name) == base)
            return cs.name;
        for (const auto& alias : cs.aliases)
            if (sanitize_id_token(alias) == base)
                return cs.name;
    }
    return {};
}



string_view
ColorConfig::Impl::resolve_authored_interop_id(string_view name) const
{
    if (!m_catalog)
        return {};
    // Exact only: a bare ID never selects a space declaring a namespaced one.
    auto found = m_catalog->interop_ids.find(name);
    return found != m_catalog->interop_ids.end()
               ? string_view(m_catalog->spaces[found->second].name)
               : string_view();
}



// The configured color space a name, alias, role or portable identity selects,
// or empty when this configuration cannot express it. resolve() is the shared
// path, so an identity also selects a renamed local space that measurement --
// under this Impl's own context -- says carries it.
std::string
ColorConfig::Impl::configured_space(string_view name) const
{
    if (name.empty())
        return {};
    string_view resolved = resolve(name);
    if (find(resolved) || isData(resolved))
        return std::string(resolved);
    // Without a catalog the inventory is the explicit synthetic identities,
    // which the public index lookup matches without regard to case.
    if (!m_catalog)
        for (const auto& cs : colorspaces)
            if (Strutil::iequals(cs.name, resolved))
                return cs.name;
    return {};
}



// Private seam, befriended by ColorConfig, that lets createDisplayTransform
// and ImageBufAlgo::ociodisplay make the same display/view selection.
struct ColorConfigAccess {
    // One display, view and source name for a display transform, chosen under
    // one context view, so that the processor and the tag its consumer writes
    // describe the same conversion.
    struct DisplayView {
        ustring display, view, source;
    };
    static DisplayView select_display_view(const ColorConfig& config,
                                           ustring display, ustring view,
                                           ustring source, string_view key,
                                           string_view value);
    // pvt::color_space_info.
    static ColorSpaceInfo color_space_info(const ColorConfig& config,
                                           string_view colorspace, bool derive,
                                           string_view key, string_view value);
};



ColorConfigAccess::DisplayView
ColorConfigAccess::select_display_view(const ColorConfig& config,
                                       ustring display, ustring view,
                                       ustring source, string_view key,
                                       string_view value)
{
    const ColorConfig::Impl* impl   = config.getImpl();
    const auto seen                 = impl->context_view(key, value, source);
    const ColorConfig::Impl& lookup = seen ? *seen : *impl;
    DisplayView selected;
    // The same source lookup createColorProcessor makes, under the same view,
    // so a name selected under these overrides names the same local color
    // space here. A view this build could not acquire is not a verdict, and
    // an override then opts out of identification rather than borrowing the
    // default context's evidence for it.
    selected.source = ustring(
        lookup.resolve(source, nullptr, nullptr, seen != nullptr));
    selected.display = display;
    if (selected.display.empty() || selected.display == "default")
        selected.display = ustring(config.getDefaultDisplayName());
    selected.view = view;
    if (selected.view.empty() || selected.view == "default") {
        // A source the config does not define cannot select a view by its
        // own viewing rules; it enters through the interchange role and is
        // offered the display's scene-linear default view, as its explicit
        // counterpart is.
        const bool local = !lookup.configured_space(selected.source).empty();
        selected.view    = ustring(
            config.getDefaultViewName(selected.display,
                                      local ? string_view(selected.source)
                                            : string_view("scene_linear")));
    }
    return selected;
}



bool
ColorConfig::Impl::equivalent_resolved(string_view original1,
                                       string_view resolved1,
                                       std::optional<const CSInfo*>& csi1,
                                       string_view color_space2, bool* success,
                                       int* required_recognition) const
{
    if (color_space2.empty())
        return false;
    // Easy case: matching names are the same!
    if (Strutil::iequals(original1, color_space2))
        return true;

    // If "resolved" names (after converting aliases and roles to color
    // spaces) match, they are equivalent.
    color_space2 = resolve(color_space2, success, required_recognition);
    if (resolved1.empty() || color_space2.empty())
        return false;
    if (Strutil::iequals(resolved1, color_space2))
        return true;

    // If the color spaces' flags (when masking only the bits that refer to
    // specific known color spaces) match, consider them equivalent.
    if (required_recognition)
        *required_recognition |= CSInfo::is_known;
    const int mask = CSInfo::is_srgb_display | CSInfo::is_srgb_scene
                     | CSInfo::is_lin_srgb | CSInfo::is_ACEScg
                     | CSInfo::is_Rec709;
    if (!csi1.has_value())
        csi1 = find(resolved1);
    const CSInfo* csi2 = find(color_space2);
    if (*csi1 && csi2) {
        int flags1 = (*csi1)->flags() & mask;
        int flags2 = csi2->flags() & mask;
        if ((flags1 | flags2) && (*csi1)->flags() == csi2->flags())
            return true;
        if (((*csi1)->canonical.size() && csi2->canonical.size())
            && Strutil::iequals((*csi1)->canonical, csi2->canonical))
            return true;
    }

    auto info1           = color_space_info(resolved1, true);
    auto info2           = color_space_info(color_space2, true);
    const CSInfo* local1 = csi1.value_or(nullptr);
    bool candidate       = info1.m_impl && info2.m_impl
                           && !info1.m_impl->identity.empty()
                           && info1.m_impl->identity == info2.m_impl->identity;
    // Two definitions measured to be the same encoding are a candidate pair
    // even when what they declare disagrees, which is the ordinary case for a
    // renamed or redeclared local copy of one encoding. Asked only after the
    // comparison above has already failed, and only between two spaces this
    // configuration defines, so the common cases measure nothing and no
    // substituted reference definition can enter the comparison. This
    // nominates a pair; the no-op guard below is what decides, and it refuses
    // a pair of different reference types before measuring anything.
    if (!candidate && local1 && csi2
        && local1->display_referred == csi2->display_referred) {
        string_view measured = get_color_equality_id(local1->name, success);
        candidate = !measured.empty()
                    && measured == get_color_equality_id(csi2->name, success);
    }
    if (candidate) {
        // Recognition can accept a close reference match, and declarations
        // describe intended encoding. Neither proves a conversion is a no-op.
        // Ask OCIO directly, without recursively entering identity resolution.
        try {
            if (!config_ || !local1 || !csi2) {
                if (!config_ && success)
                    *success = false;
                return false;
            }
            auto native1 = config_->getColorSpace(local1->name.c_str());
            auto native2 = config_->getColorSpace(csi2->name.c_str());
            if (!native1 || !native2
                || native1->getReferenceSpaceType()
                       != native2->getReferenceSpaceType())
                return false;
            auto processor = config_->getProcessor(config_->getCurrentContext(),
                                                   local1->name.c_str(),
                                                   csi2->name.c_str());
            if (!processor) {
                if (success)
                    *success = false;
                return false;
            }
            // Unoptimized processors keep inverse pairs, such as a round trip
            // through the interchange space, that optimization removes.
            return processor->getOptimizedProcessor(ocio_optimization)->isNoOp();
        } catch (const std::exception&) {
            if (success)
                *success = false;
            return false;
        }
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

    string_view resolved1 = getImpl()->resolve(color_space1);
    std::optional<const CSInfo*> csi1;
    return getImpl()->equivalent_resolved(color_space1, resolved1, csi1,
                                          color_space2);
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



// Custom ColorProcessor that wraps an OpenColorIO Processor.
class ColorProcessor_OCIO final : public ColorProcessor {
public:
    ColorProcessor_OCIO(OCIO::ConstProcessorRcPtr p,
                        bool preserve_alpha_precision = false)
        : m_p(p)
        , m_cpuproc(p->getOptimizedCPUProcessor(
              preserve_alpha_precision
                  ? OCIO::OptimizationFlags(
                        ocio_optimization & ~OCIO::OPTIMIZATION_FAST_LOG_EXP_POW)
                  : ocio_optimization))
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
        try {
            OCIO::PackedImageDesc pid(data, width, height, channels,
                                      OCIO::BIT_DEPTH_F32,  // For now, only float
                                      chanstride, xstride, ystride);
            m_cpuproc->apply(pid);
        } catch (std::exception& e) {
            OIIO::errorfmt("OCIO error in apply: {}\n", e.what());
            // FIXME -- some day, we should make ColorProcessor::apply return
            // a status, and we should indicate here that it failed.
        }
    }

private:
    OCIO::ConstProcessorRcPtr m_p;
    OCIO::ConstCPUProcessorRcPtr m_cpuproc;
};



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



// Where a transform endpoint lives when the active config does not define it.
// Native names, aliases, roles and named transforms retain their meaning.
// Only absent explicit encoding names select the private reference.
//
// `endpoint` and `ctx` are left alone for a name the config owns.
static void
external_endpoint(const OCIO::ConstConfigRcPtr& config, ustring name,
                  OCIO::ConstConfigRcPtr& endpoint,
                  OCIO::ConstContextRcPtr& ctx)
{
    if (config->getColorSpace(name.c_str())
        || config->getNamedTransform(name.c_str()))
        return;
    auto reference = internal_reference();
    auto cs        = reference->getColorSpace(name.c_str());
    if (cs && Strutil::iequals(name, cs->getName())) {
        endpoint = reference;
        ctx      = reference->getCurrentContext();
        return;
    }
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

    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(inputColorSpace, outputColorSpace, context_key,
                              context_value);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

    // DBG("createColorProcessor {} -> {}\n", inputColorSpace,
    //                outputColorSpace);
    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    OCIO::ConstProcessorRcPtr p;
    if (getImpl()->config_ && !disable_ocio) {
        auto config = getImpl()->config_;
        try {
            auto context = effective_context(config, context_key,
                                             context_value);

            // Identification is evidence about transforms, and an override can
            // change the transform that established a counterpart. So resolve
            // under the caller's own context, so a selected name converts
            // through the same local color space here. Authored selectors
            // still own their meaning. A view this build could not acquire is not a verdict,
            // and an override then opts out of identification rather than
            // borrowing the default context's evidence for it.
            const auto view = getImpl()->context_view(
                context_key, context_value,
                Strutil::fmt::format("{} {}", inputColorSpace,
                                     outputColorSpace));
            const Impl& lookup = view ? *view : *getImpl();
            inputColorSpace = ustring(lookup.resolve(inputColorSpace, nullptr,
                                                     nullptr, view != nullptr));
            outputColorSpace = ustring(lookup.resolve(outputColorSpace, nullptr,
                                                      nullptr,
                                                      view != nullptr));

            OCIO::ConstConfigRcPtr srcconfig = config, dstconfig = config;
            auto srccontext = context, dstcontext = context;
            if (!disable_builtin_configs) {
                external_endpoint(config, inputColorSpace, srcconfig,
                                  srccontext);
                external_endpoint(config, outputColorSpace, dstconfig,
                                  dstcontext);
            }
            if (srcconfig == dstconfig) {
                p = srcconfig->getProcessor(srccontext, inputColorSpace.c_str(),
                                            outputColorSpace.c_str());
            } else {
                auto src = srcconfig->getColorSpace(inputColorSpace.c_str());
                auto dst = dstconfig->getColorSpace(outputColorSpace.c_str());
                if (src && dst && (src->isData() || dst->isData())) {
                    // Data bypasses color management even without interchange
                    // roles. Use OCIO's native identity, not a second engine.
                    p = srcconfig->getProcessor(srccontext, src, src);
                } else {
                    p = OCIO::Config::GetProcessorFromConfigs(
                        srccontext, srcconfig, inputColorSpace.c_str(),
                        dstcontext, dstconfig, outputColorSpace.c_str());
                }
            }
            getImpl()->clear_error();
            // DBG("Created OCIO processor '{}' -> '{}'\n",
            //                inputColorSpace, outputColorSpace);
        } catch (std::exception& e) {
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
            } catch (std::exception& e) {
                getImpl()->error("Exception from OCIO: {}", e.what());
            }
            // DBG("OCIO processor '{}' -> '{}' is NOT NoOp, handle = {}\n",
            //                inputColorSpace, outputColorSpace, (bool)handle);
        }
    }

    if (!handle && p) {
        // If we found a processor from OCIO, even if it was a NoOp, and we
        // still don't have a better idea, return it.
        try {
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (std::exception& e) {
            getImpl()->error("Exception from OCIO: {}", e.what());
        }
    }

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
            auto context    = effective_context(config, context_key,
                                                context_value);
            const auto view = getImpl()->context_view(
                context_key, context_value,
                Strutil::fmt::format("{} {}", inputColorSpace,
                                     outputColorSpace));
            const Impl& lookup  = view ? *view : *getImpl();
            const bool identify = view != nullptr;
            const ustring resolved_input(
                lookup.resolve(inputColorSpace, nullptr, nullptr, identify));
            const ustring resolved_output(
                lookup.resolve(outputColorSpace, nullptr, nullptr, identify));
            OCIO::LookTransformRcPtr transform = OCIO::LookTransform::Create();
            transform->setLooks(looks.c_str());
            OCIO::TransformDirection dir;
            if (inverse) {
                // The TRANSFORM_DIR_INVERSE applies an inverse for the
                // end-to-end transform, which would otherwise do dst->inv
                // look -> src.  This is an unintuitive result for the artist
                // (who would expect in, out to remain unchanged), so we
                // account for that here by flipping src/dst
                transform->setSrc(c_str(resolved_output));
                transform->setDst(c_str(resolved_input));
                dir = OCIO::TRANSFORM_DIR_INVERSE;
            } else {  // forward
                transform->setSrc(c_str(resolved_input));
                transform->setDst(c_str(resolved_output));
                dir = OCIO::TRANSFORM_DIR_FORWARD;
            }

            // Get the processor corresponding to this transform.
            OCIO::ConstProcessorRcPtr p;
            p = getImpl()->config_->getProcessor(context, transform, dir);
            getImpl()->clear_error();
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (std::exception& e) {
            getImpl()->error("Exception from OCIO: {}", e.what());
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
    // Display, view and source are chosen together, under one context view,
    // by the selection ImageBufAlgo::ociodisplay tags its output from, so the
    // description of the pixels and the pixels come from the same choice.
    const auto selected = ColorConfigAccess::select_display_view(
        *this, display, view, inputColorSpace, context_key, context_value);
    display = selected.display;
    view    = selected.view;
    // First, look up the requested processor in the cache. If it already
    // exists, just return it.
    ColorProcCacheKey prockey(inputColorSpace, ustring() /*outputColorSpace*/,
                              context_key, context_value, looks, display, view,
                              ustring() /*file*/, ustring() /*namedtransform*/,
                              inverse);
    ColorProcessorHandle handle = getImpl()->findproc(prockey);
    if (handle)
        return handle;

    // Ask OCIO to make a Processor that can handle the requested
    // transformation.
    if (getImpl()->config_ && !disable_ocio) {
        OCIO::ConstConfigRcPtr config = getImpl()->config_;
        try {
            auto context = effective_context(config, context_key,
                                             context_value);

            // An encoding this config lacks is supplied by the reference or
            // by a session endpoint rather than reinterpreted. `src` was
            // already resolved under this same context view by the selection
            // above, which is also what chose the default view.
            ustring src                      = selected.source;
            OCIO::ConstConfigRcPtr srcconfig = config;
            auto srccontext                  = context;
            if (!disable_builtin_configs)
                external_endpoint(config, src, srcconfig, srccontext);

            OCIO::TransformDirection dir = inverse
                                               ? OCIO::TRANSFORM_DIR_INVERSE
                                               : OCIO::TRANSFORM_DIR_FORWARD;
            auto transform               = OCIO::DisplayViewTransform::Create();
            auto legacy_viewing_pipeline = OCIO::LegacyViewingPipeline::Create();
            transform->setDisplay(display.c_str());
            transform->setView(view.c_str());
            if (looks.size()) {
                legacy_viewing_pipeline->setLooksOverride(looks.c_str());
                legacy_viewing_pipeline->setLooksOverrideEnabled(true);
            }

            // Get the processor corresponding to this transform.
            OCIO::ConstProcessorRcPtr p;
            bool preserve_alpha_precision = false;
            if (srcconfig == config) {
                transform->setSrc(src.c_str());
                transform->setDirection(dir);
                legacy_viewing_pipeline->setDisplayViewTransform(transform);
                p = legacy_viewing_pipeline->getProcessor(config, context);
            } else if (auto cs = srcconfig->getColorSpace(src.c_str());
                       cs && cs->isData()) {
                // Data bypasses the display, as OCIO has it bypass any
                // two-config conversion.
                p = config->getProcessor(context,
                                         OCIO::GroupTransform::Create(), dir);
            } else {
                preserve_alpha_precision = true;
                // OpenColorIO's own two-config display recipe: the source
                // reaches this config's interchange space for its reference
                // space type, and the config's viewing pipeline, looks
                // included, runs from there. The forward chain is built and
                // inverted as a whole, as the native inverse would be.
                const char* interchange
                    = cs
                              && cs->getReferenceSpaceType()
                                     == OCIO::REFERENCE_SPACE_DISPLAY
                          ? OCIO::ROLE_INTERCHANGE_DISPLAY
                          : OCIO::ROLE_INTERCHANGE_SCENE;
                auto chain = OCIO::Config::GetProcessorFromConfigs(
                                 srccontext, srcconfig, src.c_str(), context,
                                 config, interchange)
                                 ->createGroupTransform();
                transform->setSrc(interchange);
                legacy_viewing_pipeline->setDisplayViewTransform(transform);
                chain->appendTransform(
                    legacy_viewing_pipeline->getProcessor(config, context)
                        ->createGroupTransform());
                p = config->getProcessor(context, chain, dir);
            }
            getImpl()->clear_error();
            // Fast power evaluation perturbs the cross-config display chain's
            // unit alpha exponent. Keep all other optimizations on this path.
            handle = ColorProcessorHandle(
                new ColorProcessor_OCIO(p, preserve_alpha_precision));
        } catch (OCIO::Exception& e) {
            getImpl()->error("Exception from OCIO: {}", e.what());
        } catch (...) {
            getImpl()->error(
                "An unknown error occurred in OpenColorIO, getProcessor");
        }
    }

    return getImpl()->addproc(prockey, handle);
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
        } catch (std::exception& e) {
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
            auto context = effective_context(config, context_key,
                                             context_value);

            // Get the processor corresponding to this transform.
            OCIO::ConstProcessorRcPtr p;
            p = config->getProcessor(context, transform, dir);
            getImpl()->clear_error();
            handle = ColorProcessorHandle(new ColorProcessor_OCIO(p));
        } catch (std::exception& e) {
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
        } catch (std::exception& e) {
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
        } catch (std::exception& e) {
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
    } catch (std::exception& e) {
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
// Color Interop ID

namespace {
enum class CICPPrimaries : int {
    Rec709    = 1,
    Rec601PAL = 5,
    Rec601    = 6,
    SMPTE240M = 7,
    Rec2020   = 9,
    XYZD65    = 10,
    P3D65     = 12,
};

enum class CICPTransfer : int {
    BT709     = 1,
    Gamma22   = 4,
    BT601     = 6,
    Linear    = 8,
    sRGB      = 13,
    BT2020_10 = 14,
    BT2020_12 = 15,
    PQ        = 16,
    Gamma26   = 17,
    HLG       = 18,
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
    constexpr ColorInteropID(const char* interop_id, const char* legacy_alias)
        : interop_id(interop_id)
        , legacy_alias(legacy_alias)
        , cicp({ 0, 0, 0, 0 })
        , has_cicp(false)
    {
    }

    constexpr ColorInteropID(const char* interop_id, const char* legacy_alias,
                             CICPPrimaries primaries, CICPTransfer transfer,
                             CICPMatrix matrix)
        : interop_id(interop_id)
        , legacy_alias(legacy_alias)
        , cicp({ int(primaries), int(transfer), int(matrix),
                 int(CICPRange::Full) })
        , has_cicp(true)
    {
    }

    const char* interop_id;
    const char* legacy_alias;
    std::array<int, 4> cicp;
    bool has_cicp;
};

// Mapping between color interop ID and CICP, based on Color Interop Forum
// recommendations. The legacy aliases are for older ACES configs.
constexpr ColorInteropID color_interop_ids[] = {
    // Scene referred interop IDs first so they are the default in automatic
    // conversion from CICP to interop ID. Some are not display color spaces
    // at all, but can be represented by CICP anyway.
    { "lin_ap1_scene", "lin_ap1" },
    { "lin_ap0_scene", "lin_ap0" },
    { "lin_rec709_scene", "lin_rec709", CICPPrimaries::Rec709,
      CICPTransfer::Linear, CICPMatrix::BT709 },
    { "lin_p3d65_scene", "lin_p3d65", CICPPrimaries::P3D65,
      CICPTransfer::Linear, CICPMatrix::BT709 },
    { "lin_rec2020_scene", "lin_rec2020", CICPPrimaries::Rec2020,
      CICPTransfer::Linear, CICPMatrix::Rec2020_CL },
    { "lin_adobergb_scene", "lin_adobergb" },
    { "lin_ciexyzd65_scene", "cie_xyz_d65", CICPPrimaries::XYZD65,
      CICPTransfer::Linear, CICPMatrix::Unspecified },
    { "srgb_rec709_scene", "srgb_texture", CICPPrimaries::Rec709,
      CICPTransfer::sRGB, CICPMatrix::BT709 },
    { "g24_rec709_scene", "g24_rec709" },
    { "ocio:itu709_rec709_scene", "itu709_rec709_scene" },
    { "g22_rec709_scene", "g22_rec709", CICPPrimaries::Rec709,
      CICPTransfer::Gamma22, CICPMatrix::BT709 },
    { "g18_rec709_scene", "g18_rec709" },
    { "srgb_ap1_scene", "srgb_ap1" },
    { "g22_ap1_scene", "g22_ap1" },
    { "srgb_p3d65_scene", "srgb_p3d65", CICPPrimaries::P3D65,
      CICPTransfer::sRGB, CICPMatrix::BT709 },
    { "g22_adobergb_scene", nullptr },
    { "data", nullptr },
    { "unknown", nullptr },

    // Display referred interop IDs.
    // The linear displays share their scene rows' codes, which come first,
    // so CICP input still reads as the scene identity.
    { "lin_rec709_display", nullptr, CICPPrimaries::Rec709,
      CICPTransfer::Linear, CICPMatrix::BT709 },
    { "lin_p3d65_display", nullptr, CICPPrimaries::P3D65, CICPTransfer::Linear,
      CICPMatrix::BT709 },
    { "lin_rec2020_display", nullptr, CICPPrimaries::Rec2020,
      CICPTransfer::Linear, CICPMatrix::Rec2020_CL },
    { "srgb_rec709_display", "srgb_display", CICPPrimaries::Rec709,
      CICPTransfer::sRGB, CICPMatrix::BT709 },
    // Not all software interprets this CICP the same, see the
    // "QuickTime Gamma Shift" issue. We follow the CIF recommendation and
    // interpret it as BT.1886.
    { "g24_rec709_display", "rec1886_rec709_display", CICPPrimaries::Rec709,
      CICPTransfer::BT709, CICPMatrix::BT709 },
    { "srgb_p3d65_display", "displayp3_display", CICPPrimaries::P3D65,
      CICPTransfer::sRGB, CICPMatrix::BT709 },
    { "srgbe_p3d65_display", "displayp3_hdr_display", CICPPrimaries::P3D65,
      CICPTransfer::sRGB, CICPMatrix::BT709 },
    { "pq_p3d65_display", "st2084_p3d65_display", CICPPrimaries::P3D65,
      CICPTransfer::PQ, CICPMatrix::Rec2020_NCL },
    { "pq_rec2020_display", "rec2100_pq_display", CICPPrimaries::Rec2020,
      CICPTransfer::PQ, CICPMatrix::Rec2020_NCL },
    { "hlg_rec2020_display", "rec2100_hlg_display", CICPPrimaries::Rec2020,
      CICPTransfer::HLG, CICPMatrix::Rec2020_NCL },
    // No CICP mapping to keep previous behavior unchanged, as Gamma 2.2
    // display is more likely meant to be written as sRGB. On read the
    // scene referred interop ID will be used.
    { "g22_rec709_display", nullptr
      /* CICPPrimaries::Rec709, CICPTransfer::Gamma22, CICPMatrix::BT709 */ },
    // No CICP code for Adobe RGB primaries.
    { "g22_adobergb_display", nullptr },
    // CICP transfer 17 includes DCI-white headroom scaling, which this
    // ordinary gamma-2.6 identity does not. It remains a legacy selector but
    // has no output tuple; the read-only DCDM endpoint handles 12/17 input.
    { "g26_p3d65_display", "p3d65_display" },
    { "g26_xyzd65_display", nullptr, CICPPrimaries::XYZD65,
      CICPTransfer::Gamma26, CICPMatrix::Unspecified },
    { "pq_xyzd65_display", nullptr, CICPPrimaries::XYZD65, CICPTransfer::PQ,
      CICPMatrix::Unspecified },
    // The two standard-definition video gamuts, 525-line (SMPTE 170M, the
    // "SMPTE-C" primaries) and 625-line (BT.470BG, the "PAL" primaries). They
    // are distinct gamuts, not spellings of Rec.709, and the reference
    // definitions they name already exist in the built-in interop-identities
    // config. No legacy alias: these identities are new here and have no older
    // ACES-config spelling.
    { "oiio:g24_rec601_display", nullptr, CICPPrimaries::Rec601,
      CICPTransfer::BT709, CICPMatrix::RGB },
    { "oiio:lin_rec601_display", nullptr, CICPPrimaries::Rec601,
      CICPTransfer::Linear, CICPMatrix::RGB },
    { "oiio:g24_rec601pal_display", nullptr, CICPPrimaries::Rec601PAL,
      CICPTransfer::BT709, CICPMatrix::RGB },
    { "oiio:lin_rec601pal_display", nullptr, CICPPrimaries::Rec601PAL,
      CICPTransfer::Linear, CICPMatrix::RGB },

    // OpenColorIO interop IDs, last so that the official ones above take
    // priority when converting a CICP to an interop ID.
    { "ocio:lin_ciexyzd65_display", "cie_xyz_d65", CICPPrimaries::XYZD65,
      CICPTransfer::Linear, CICPMatrix::Unspecified },
};

// Reasons a table lookup only succeeded because the tuple was spelled with an
// equivalent code. A trace step keeps a view of its reason rather than a copy,
// so each of these is a literal with process lifetime.
constexpr string_view cicp_transfer_equivalence_reason
    = "CICP tuple identifies a usable encoding; transfer codes 6, 14 and 15 "
      "are the same transfer function as code 1";
constexpr string_view cicp_primaries_equivalence_reason
    = "CICP tuple identifies a usable encoding; primaries code 7 has exactly "
      "the SMPTE 170M primaries of code 6";
constexpr string_view cicp_both_equivalence_reason
    = "CICP tuple identifies a usable encoding; primaries code 7 has exactly "
      "the SMPTE 170M primaries of code 6, and transfer codes 6, 14 and 15 "
      "are the same transfer function as code 1";

// The primaries and transfer codes a table lookup is performed with. Several
// registered codes name something the table already holds under another code:
// transfer 6 (BT.601), 14 (BT.2020 10 bit) and 15 (BT.2020 12 bit) are the
// same opto-electronic transfer function as transfer 1, and primaries 7
// (SMPTE 240M) are exactly the primaries of 6 (SMPTE 170M). Recognizing them
// is one-directional: this is consulted on the way in, never on the way out,
// so nothing is ever written with 7, 14 or 15. The caller's own tuple is left
// exactly as authored.
//
// `equivalence` reports which equivalence, if either, the answer depended on,
// so a trace can say so without resolving anything a second time.
struct CICPPair {
    int primaries;
    int transfer;
};

CICPPair
normalized_cicp_pair(int primaries, int transfer,
                     string_view* equivalence = nullptr)
{
    const bool same_transfer  = transfer == int(CICPTransfer::BT601)
                                || transfer == int(CICPTransfer::BT2020_10)
                                || transfer == int(CICPTransfer::BT2020_12);
    const bool same_primaries = primaries == int(CICPPrimaries::SMPTE240M);
    if (equivalence) {
        if (same_transfer && same_primaries)
            *equivalence = cicp_both_equivalence_reason;
        else if (same_primaries)
            *equivalence = cicp_primaries_equivalence_reason;
        else if (same_transfer)
            *equivalence = cicp_transfer_equivalence_reason;
    }
    return { same_primaries ? int(CICPPrimaries::Rec601) : primaries,
             same_transfer ? int(CICPTransfer::BT709) : transfer };
}



// ST 428-1 includes DCI-white headroom scaling, so the ordinary gamma-2.6
// P3-D65 identity is not an exact input interpretation. This read-only
// identity already exists in the built-in interop-identities config and is
// deliberately not added to the bidirectional table below.
string_view
exact_cicp_input_identity(const CICPPair& pair)
{
    return pair.primaries == int(CICPPrimaries::P3D65)
                   && pair.transfer == int(CICPTransfer::Gamma26)
               ? "dcdm_p3d65_display"
               : string_view();
}



const char*
legacy_interop_selector(string_view identity)
{
    for (const auto& interop : color_interop_ids)
        if (identity == interop.interop_id)
            return interop.legacy_alias;
    return nullptr;
}



bool
is_interop_id_spelling(string_view name)
{
    static const std::unordered_set<std::string> spellings = [] {
        std::unordered_set<std::string> result;
        for (const ColorInteropID& interop : color_interop_ids) {
            result.emplace(interop.interop_id);
            if (interop.legacy_alias)
                result.emplace(interop.legacy_alias);
        }
        return result;
    }();
    return spellings.count(Strutil::lower(name));
}



// A utility token names a treatment rather than color, and the read-only
// cinema identity interprets input only. Neither may ever be the answer to a
// measured comparison: an untransformed reference-space color space would
// otherwise report the transform-free "data" definition as its own encoding.
bool
identity_excluded_from_matching(string_view id)
{
    return id == "data" || id == "unknown" || id == "bypass"
           || id == "dcdm_p3d65_display";
}



// The portable identity a request may select, or empty when the request is an
// ordinary configured name. Established table spellings answer without loading
// anything; other spellings are canonicalized through the built-in
// interop-identities config, which the cheap shape test keeps out of ordinary
// name lookups.
std::string
requested_identity(string_view name)
{
    if (is_interop_id_spelling(name))
        return Strutil::lower(name);
    if (!Strutil::ends_with(name, "_scene")
        && !Strutil::ends_with(name, "_display"))
        return {};
    for (char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'
              || c == ':' || c == '.' || c == '-'))
            return {};
    try {
        auto cs = internal_reference()->getColorSpace(
            std::string(name).c_str());
        if (cs && !cs->isData()
            && !identity_excluded_from_matching(cs->getName()))
            return cs->getName();
    } catch (const std::exception&) {
    }
    return {};
}

// A color space of the built-in interop-identities config named exactly (in
// any case), which is how conversions accept one the active config does not
// define, or empty. Nothing when the environment disables that config.
std::string
builtin_identity(string_view name)
{
    // An absent name (a missing attribute's default string_view) has no
    // storage to compare, and names no identity anyway.
    if (name.empty() || disable_ocio || disable_builtin_configs)
        return {};
    std::string id = requested_identity(Strutil::lower(name));
    return Strutil::iequals(id, name) ? id : std::string();
}

string_view
known_image_state(string_view identity, string_view encoding)
{
    if (Strutil::iequals(encoding, "scene-linear"))
        return "scene";
    if (Strutil::iequals(encoding, "display-linear"))
        return "display";
    // Any valid interop ID names its image state by its suffix.
    if (!valid_interop_id(identity))
        return {};
    if (Strutil::ends_with(identity, "_scene"))
        return "scene";
    if (Strutil::ends_with(identity, "_display"))
        return "display";
    return {};
}

using InteropMemoKey = std::pair<std::string, std::string>;

struct InteropMemoValue {
    std::string result;
    int required_recognition_flags = 0;
};

spin_rw_mutex interop_id_memo_mutex;
// Retained for reuse by sequential ColorConfig wrappers. The map owns nothing a
// caller sees: every identity this function returns is interned first, so a
// returned string_view outlives map growth and its originating ColorConfig.
// Retention is unbounded; cap admission if growth is measured.
std::map<InteropMemoKey, InteropMemoValue> interop_id_memo;
}  // namespace

string_view
ColorConfig::get_color_interop_id(string_view colorspace) const
{
    if (colorspace.empty())
        return "";
    return getImpl()->get_color_interop_id(colorspace);
}



string_view
ColorConfig::Impl::get_color_interop_id(string_view colorspace) const
{
    // A table identity or legacy spelling the config does not define answers
    // as its canonical ID, whatever local endpoint it may resolve to. So does
    // any other built-in identity, which resolve() and conversions already
    // accept by that name.
    if (!find(colorspace) && !has_named_transform(colorspace)) {
        for (const ColorInteropID& interop : color_interop_ids)
            if (Strutil::iequals(colorspace, interop.interop_id)
                || (interop.legacy_alias
                    && Strutil::iequals(colorspace, interop.legacy_alias)))
                return interop.interop_id;
        if (std::string id = builtin_identity(colorspace); !id.empty())
            return ustring(id);  // Interned; the vocabulary is bounded.
    }

    bool lookup_ok           = true;
    int required_recognition = 0;
    string_view resolved     = resolve(colorspace, &lookup_ok,
                                       &required_recognition);
    if (const CSInfo* cs = find(resolved)) {
        // Interned, not a view of the catalog: this answer is documented to
        // outlive the wrapper that gave it. A configuration's declared
        // identities are a bounded vocabulary.
        if (!cs->interop_id.empty())
            return ustring(cs->interop_id);
        if (m_catalog && (cs->flags() & CSInfo::is_data))
            return "data";
    }

    std::optional<const CSInfo*> csi = find(resolved);
    // A request that spells a table identity and names no color space in this
    // configuration was resolved by the identity machinery itself -- a bridge,
    // a legacy selector, a measured identity -- so there is nothing
    // further to derive from it and the naming evidence below answers.
    //
    // A configuration that uses the same spelling as one of its own names or
    // aliases is a different case entirely: that is an ordinary configured
    // name, and what the definition behind it does is measurable. It has to
    // be measured, because the spelling may belong to a different space than
    // the one carrying it -- a P3-D65 display space aliased `srgb_display` is
    // the encoding its operations describe, not the one its alias is called.
    const bool raw_table_name = is_interop_id_spelling(colorspace)
                                && !find(colorspace);

    // Cache safety governs publication, never evaluation. A config that names
    // external resources derives the same identity that resolve() and the
    // writer adapter already derive from it, and publishes it under a key that
    // covers those resources; only a derivation an acquisition interrupted
    // stays unpublished, so it is attempted again on the next call.
    const bool derivable = csi.value_or(nullptr) && !raw_table_name;
    const bool cacheable = lookup_ok && m_interop_cache_safe
                           && !m_interop_cache_id.empty() && derivable;
    InteropMemoKey key;
    if (cacheable) {
        key = { m_interop_cache_id, std::string(resolved) };
        spin_rw_read_lock lock(interop_id_memo_mutex);
        auto found = interop_id_memo.find(key);
        if (found != interop_id_memo.end()
            && !((found->second.required_recognition_flags
                  | required_recognition)
                 & m_failed_recognition_flags.load())) {
            DBG("Interop ID memo hit for {}:{}\n", m_interop_cache_id,
                resolved);
            // Interned under the lock so the caller never borrows storage from
            // the retained entry. The identity vocabulary is the reference
            // config's names, so interning it is bounded.
            return ustring(found->second.result);
        }
    }

    if (cacheable) {
        DBG("Interop ID memo cold work for {}:{}\n", m_interop_cache_id,
            resolved);
    } else {
        DBG("Interop ID memo bypass for '{}'\n", colorspace);
    }
    string_view result;
    ColorSpaceInfo derived;
    // With a configuration to measure, derivation answers first, so a name or
    // alias its measurement contradicts is not reported. Without one, the
    // built-in name equivalences answer first.
    const bool derive_first = derivable && m_catalog;
    auto derive             = [&]() {
        if (result.empty() && derivable) {
            derived = color_space_info(resolved, true);
            // An interrupted derivation answers this call only; publishing it
            // would turn a retryable failure into a process-lifetime miss.
            if (derived.m_impl && derived.m_impl->incomplete)
                lookup_ok = false;
            if (derived.m_impl && !derived.m_impl->identity.empty()) {
                // An unpublishable result is owned only by `derived`, so the
                // returned view needs process lifetime. The identity
                // vocabulary is the reference config's names, so interning it
                // is bounded.
                result = ustring(derived.m_impl->identity);
            }
        }
    };
    if (derive_first)
        derive();
    if (result.empty() && m_catalog && csi.value_or(nullptr)) {
        // Configurations derivation cannot reach -- no interchange role, no
        // OpenColorIO, a definition this build cannot measure -- still answer
        // from the configuration's own naming, in table order. A request that
        // spells a table identity directly was never derivable, so it reads
        // the same naming evidence here rather than going unanswered.
        if (!derivable) {
            for (const auto& interop : color_interop_ids) {
                if (find(interop.interop_id) == *csi
                    || (interop.legacy_alias
                        && find(interop.legacy_alias) == *csi)) {
                    result = interop.interop_id;
                    break;
                }
            }
        }
#ifdef OIIO_SITE_spi
        if (result.empty())
            result = ustring((*csi)->canonical);  // Interned; see above.
#endif
        static const char* identities[] = { "srgb_rec709_scene",
                                            "srgb_rec709_display",
                                            "lin_rec709_scene", "lin_ap1_scene",
                                            "ocio:itu709_rec709_scene" };
        static const int indices[]      = { 1, 0, 2, 3, 4 };
        for (int i = 0; result.empty() && i < 5; ++i) {
            required_recognition |= CSInfo::is_known;
            if (bridge(indices[i], &lookup_ok) == resolved) {
                result = identities[i];
                break;
            }
        }
        // Nothing measured answered: fall back to the configured name.
        if (int identity = named_identity(resolved);
            result.empty() && identity >= 0)
            result = builtin_identities[identity];
    } else if (result.empty()) {
        for (const ColorInteropID& interop : color_interop_ids) {
            if (equivalent_resolved(colorspace, resolved, csi,
                                    interop.interop_id, &lookup_ok,
                                    &required_recognition)
                || (interop.legacy_alias && find(interop.legacy_alias)
                    && equivalent_resolved(colorspace, resolved, csi,
                                           interop.legacy_alias, &lookup_ok,
                                           &required_recognition))) {
                result = interop.interop_id;
                break;
            }
        }
    }
    if (!derive_first)
        derive();
    if (!cacheable || !lookup_ok
        || (required_recognition & m_failed_recognition_flags.load())) {
        if (cacheable && lookup_ok && required_recognition)
            DBG("Interop ID memo bypass for incomplete recognition of '{}'\n",
                resolved);
        return result;
    }

    // Compute outside the lock and publish first-writer-wins. What is returned
    // is interned rather than a view of the published entry, so the answer a
    // caller holds does not depend on the entry staying in the map.
    spin_rw_write_lock lock(interop_id_memo_mutex);
    InteropMemoValue value;
    if (!result.empty())
        value.result.assign(result.data(), result.size());
    value.required_recognition_flags = required_recognition;
    auto published = interop_id_memo.emplace(std::move(key), std::move(value));
    if (published.first->second.required_recognition_flags
        & m_failed_recognition_flags.load()) {
        DBG("Interop ID memo rejected existing entry for incomplete recognition of '{}'\n",
            resolved);
        return result;
    }
    if (colordebug) {
        size_t bytes = 0;
        for (const auto& item : interop_id_memo)
            bytes += item.first.first.size() + item.first.second.size()
                     + item.second.result.size();
        DBG("Interop ID memo {} entry; retains {} entries, {} string bytes\n",
            published.second ? "published" : "reused", interop_id_memo.size(),
            bytes);
    }
    return ustring(published.first->second.result);
}



namespace {
// The naming rule for a space native OpenColorIO has already proven is data.
// A data space has no color to measure, so its own naming is the whole answer:
// the name decides first, and an alias only decides when the space carries no
// data identity at all. It is read after `isData()` and never before, so a
// utility spelling on an ordinary color space asserts nothing.
string_view
data_equality_token(const CSInfo& cs)
{
    if (Strutil::iequals(cs.name, "bypass"))
        return "bypass";
    bool alias_bypass = false, alias_data = false;
    for (const auto& alias : cs.aliases) {
        alias_bypass |= Strutil::iequals(alias, "bypass");
        alias_data |= Strutil::iequals(alias, "data");
    }
    if (alias_bypass && !alias_data && !Strutil::iequals(cs.name, "data"))
        return "bypass";
    return "data";
}
}  // namespace



string_view
ColorConfig::Impl::get_color_equality_id(string_view colorspace,
                                         bool* success) const
{
    // The native catalog is what makes `isData` a native fact and the
    // configuration something to measure. Without it the inventory is the
    // synthetic fallback naming, which is exactly what this query withholds.
    const bool native = m_catalog && config_ && !disable_ocio;
    // The names, aliases and roles this configuration defines, and nothing
    // else. A portable identity it does not define names no local definition,
    // so measuring a substituted reference definition on its behalf would only
    // report the identity back to the caller who spelled it.
    const CSInfo* cs = find(colorspace);
    if (!cs && native && colorspace.find_first_of("$%") != string_view::npos) {
        try {
            const std::string expanded
                = config_->getCurrentContext()->resolveStringVar(
                    std::string(colorspace).c_str());
            cs = find(expanded);
        } catch (const std::exception& e) {
            if (success)
                *success = false;
            DBG("Color equality context expansion unavailable for '{}': {}\n",
                colorspace, e.what());
            return "";
        }
    }
    if (!cs || !native)
        return "";
    if (cs->flags() & CSInfo::is_data)
        return data_equality_token(*cs);
    // A configuration that declares a space unique has said no shared identity
    // describes it, which the sampled pass already honors.
    try {
        auto space = config_->getColorSpace(cs->name.c_str());
        if (space && space->hasCategory("is-unique"))
            return "";
    } catch (const std::exception&) {
        if (success)
            *success = false;
        return "";
    }
    auto measured = color_space_info(cs->name, true, true);
    // An interrupted acquisition answers nothing rather than answering partly,
    // and nothing was retained, so the next call measures again.
    if (measured.m_impl && measured.m_impl->incomplete) {
        if (success)
            *success = false;
        return "";
    }
    if (!measured.m_impl || measured.m_impl->identity.empty())
        return "";
    // The result may be owned by the caller's own temporary rather than by the
    // shared map, so the returned view needs process lifetime. The vocabulary
    // is the reference configuration's own names, so interning it is bounded.
    return ustring(measured.m_impl->identity);
}

namespace {
spin_rw_mutex properties_mutex;
// Same safety boundary and ownership as the identity cache: entries are
// retained until process exit, and each value owns a shared Impl so a snapshot
// a caller holds remains independent of its originating ColorConfig.
// Completed values are retained; cap admission if growth is measured.
std::map<std::pair<std::string, std::string>, ColorSpaceInfo> properties_memo;

// These authored definitions have unadapted XYZ-D65 matrix coordinates.
// Do not generalize this to adapted D50/D60 matrices by inspecting their white.
// The built-in interop-identities config authors every one of these as a bare
// matrix, or as a matrix followed by one exponent, which is exactly what
// reference_properties below can read; a definition authored any other way is
// deliberately absent.
string_view
property_display_definition(string_view identity)
{
    static const char* display[] = {
        "lin_rec709_display",
        "lin_rec2020_display",
        "lin_p3d65_display",
        "g24_rec709_display",
        "g22_rec709_display",
        "g22_adobergb_display",
        "g26_p3d65_display",
        "oiio:g22_p3d65_display",
        "srgb_rec709_display",
        "srgb_p3d65_display",
        "srgbe_p3d65_display",
        // Namespaced definitions from the same built-in config, authored
        // in the same readable shape and, like the ones above, at their own
        // native D65 white. That config also carries P3-D60, P3-DCI, P3-D50,
        // AdobeRGB-D50 and ProPhoto displays; those are deliberately absent,
        // because their authored matrices are already chromatically adapted
        // and reading primaries back out of one reports the adapted
        // coordinates rather than the published ones. Their primaries come
        // from the forward construction below instead.
        "oiio:lin_rec601_display",
        "oiio:lin_rec601pal_display",
        "oiio:g24_rec601_display",
        "oiio:g24_rec601pal_display",
        "oiio:g24_rec2020_display",
    };
    for (const char* name : display)
        if (identity == name)
            return name;
    // Established identities with the same numerical primaries/transfer.
    static const char* scene[][2]
        = { { "lin_rec709_scene", "lin_rec709_display" },
            { "lin_rec2020_scene", "lin_rec2020_display" },
            { "lin_p3d65_scene", "lin_p3d65_display" },
            { "g22_rec709_scene", "g22_rec709_display" },
            { "g24_rec709_scene", "g24_rec709_display" },
            { "g22_adobergb_scene", "g22_adobergb_display" },
            { "srgb_rec709_scene", "srgb_rec709_display" },
            { "srgb_p3d65_scene", "srgb_p3d65_display" },
            { "ocio:itu709_rec709_scene", "srgb_rec709_display" } };
    for (const auto& pair : scene)
        if (identity == pair[0])
            return pair[1];
    return {};
}

// These structured HDR identities do not expose RGBW/gamma properties, but an
// exact authored reference definition can still establish their CICP identity.
bool
property_identity_definition(string_view identity)
{
    if (!property_display_definition(identity).empty())
        return true;
    return identity == "pq_p3d65_display" || identity == "pq_rec2020_display"
           || identity == "hlg_rec2020_display";
}

// OCIO's identification heuristic uses a loose tolerance. It is useful for
// reference/interchange validation, but cannot prove an exact exponent. This
// bounded check admits only the same authored definition on an identity
// interchange basis. Equivalent alternate authoring stays unsupported.
bool
same_reference_definition(OCIO::ConstConfigRcPtr config,
                          OCIO::ConstColorSpaceRcPtr native,
                          OCIO::ConstColorSpaceRcPtr reference)
{
    if (!native
        || reference->getReferenceSpaceType() != OCIO::REFERENCE_SPACE_DISPLAY
        || native->getReferenceSpaceType()
               != reference->getReferenceSpaceType())
        return false;
    auto interchange = config->getColorSpace("cie_xyz_d65_interchange");
    if (!interchange || interchange->isData()
        || interchange->getReferenceSpaceType()
               != reference->getReferenceSpaceType()
        || interchange->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE)
        || interchange->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE))
        return false;
    for (auto direction : { OCIO::COLORSPACE_DIR_TO_REFERENCE,
                            OCIO::COLORSPACE_DIR_FROM_REFERENCE }) {
        auto a = native->getTransform(direction);
        auto b = reference->getTransform(direction);
        if (bool(a) != bool(b))
            return false;
        if (a) {
            std::ostringstream authored, expected;
            authored << *a;
            expected << *b;
            if (authored.str() != expected.str())
                return false;
        }
    }
    return true;
}

// Admission is deliberately smaller than OCIO's grammar. Resolve dependencies
// before constructing any processor. Native file operations may be evaluated;
// whether the result may be shared is decided by the key it would be published
// under, since a structural key alone cannot certify an external resource.
class AnalyticClosure {
public:
    AnalyticClosure(OCIO::ConstConfigRcPtr config,
                    OCIO::ReferenceSpaceType reference)
        : m_config(config)
        , m_context(config->getCurrentContext())
        , m_reference(reference)
    {
    }

    bool space(OCIO::ConstColorSpaceRcPtr cs)
    {
        // A link no context resolves, and a chain that returns to a space it
        // is already inside, are the two refusals here that are not statements
        // about the shape: they are what native acquisition would raise on.
        // Separated from the rest so a refusal to read a definition is not
        // confused with an inability to reach one. See recognize_analytic.
        if (!cs) {
            m_unresolved = true;
            return false;
        }
        if (cs->isData() || cs->getReferenceSpaceType() != m_reference)
            return false;
        const std::string name = cs->getName();
        if (m_done.count(name))
            return true;
        if (!m_pending.insert(name).second) {
            m_unresolved = true;
            return false;
        }
        bool ok = transform(cs->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE))
                  && transform(
                      cs->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE));
        m_pending.erase(name);
        if (ok)
            m_done.insert(name);
        return ok;
    }

    bool resource_free() const { return !m_external; }
    bool resolved() const { return !m_unresolved; }

private:
    bool transform(OCIO::ConstTransformRcPtr t)
    {
        if (!t)
            return true;
        if (OCIO::DynamicPtrCast<const OCIO::FileTransform>(t)) {
            m_external = true;
            return true;  // Native acquisition and expanded ops decide support.
        }
        if (auto group = OCIO::DynamicPtrCast<const OCIO::GroupTransform>(t)) {
            for (int i = 0; i < group->getNumTransforms(); ++i)
                if (!transform(group->getTransform(i)))
                    return false;
            return true;
        }
        if (auto link = OCIO::DynamicPtrCast<const OCIO::ColorSpaceTransform>(
                t)) {
            // Native context resolves selectors; native config resolves aliases
            // and roles. No name from a different capture is consulted.
            std::string src = m_context->resolveStringVar(link->getSrc());
            std::string dst = m_context->resolveStringVar(link->getDst());
            return space(m_config->getColorSpace(src.c_str()))
                   && space(m_config->getColorSpace(dst.c_str()));
        }
        return bool(OCIO::DynamicPtrCast<const OCIO::MatrixTransform>(t))
               || bool(OCIO::DynamicPtrCast<const OCIO::ExponentTransform>(t))
               || bool(
                   OCIO::DynamicPtrCast<const OCIO::ExponentWithLinearTransform>(
                       t))
               || bool(OCIO::DynamicPtrCast<const OCIO::LogTransform>(t))
               || bool(OCIO::DynamicPtrCast<const OCIO::LogAffineTransform>(t))
               || bool(OCIO::DynamicPtrCast<const OCIO::LogCameraTransform>(t))
               || bool(OCIO::DynamicPtrCast<const OCIO::BuiltinTransform>(t));
    }

    OCIO::ConstConfigRcPtr m_config;
    OCIO::ConstContextRcPtr m_context;
    OCIO::ReferenceSpaceType m_reference;
    bool m_external   = false;
    bool m_unresolved = false;
    std::unordered_set<std::string> m_pending, m_done;
};

// ---------------------------------------------------------------------------
// Measured comparison protocol
// ---------------------------------------------------------------------------
//
// Six RGBA probes per image state. Their construction is what makes them
// usable, and it is not arbitrary: the three chromatic probes are ordinary
// in-gamut broadcast colors -- (0.9, 0.03, 0.01), (0.06, 0.9, 0.02) and
// (0.01, 0.02, 0.9) in Rec.709 -- carried into the interchange space, joined
// by black, a dark neutral and the interchange white. Because they stay
// inside the gamuts an ordinary configuration encodes, a transfer function's
// treatment of negative input never enters the comparison, and two spellings
// of one encoding that differ only in how they mirror, clamp or pass through
// below zero still compare equal. That is a deliberate property: the strict
// comparison above answers first and does separate those spellings.
//
// The display probes are the same colors in absolute CIE XYZ at D65, with the
// D65 illuminant itself in the white slot.

static const std::array<float, 4> interop_probes_scene[]
    = { { 0.408933127871f, 0.106169822808f, 0.027842572707f, 0.0f },
        { 0.374615373650f, 0.739417755017f, 0.118862613721f, 0.0f },
        { 0.171696591718f, 0.104272268468f, 0.786227391453f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.5f },
        { 0.037018876439f, 0.030827687576f, 0.021641700645f, 0.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f } };

static const std::array<float, 4> interop_probes_display[]
    = { { 0.383684057405f, 0.213552088801f, 0.030478901760f, 0.0f },
        { 0.350178969169f, 0.657853997550f, 0.127445793983f, 0.0f },
        { 0.173708304342f, 0.081402847459f, 0.858056140808f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.5f },
        { 0.034956685913f, 0.033530856964f, 0.023553027375f, 0.0f },
        { 0.950455927052f, 1.0f, 1.089057750760f, 1.0f } };

// The response tolerance. It is absolute because the probes are bounded, and
// it is wide enough that a definition authored as a curve and the same
// definition tabulated as a one-dimensional look-up still compare equal, which
// is what lets a camera vendor's own encoding match its published description.
static const float interop_response_tolerance = 5e-3f;

bool
measurable_atomic_transform(OCIO::ConstTransformRcPtr t)
{
    if (OCIO::DynamicPtrCast<const OCIO::MatrixTransform>(t)
        || OCIO::DynamicPtrCast<const OCIO::RangeTransform>(t)
        || OCIO::DynamicPtrCast<const OCIO::ExponentTransform>(t)
        || OCIO::DynamicPtrCast<const OCIO::ExponentWithLinearTransform>(t)
        || OCIO::DynamicPtrCast<const OCIO::LogTransform>(t)
        || OCIO::DynamicPtrCast<const OCIO::LogAffineTransform>(t)
        || OCIO::DynamicPtrCast<const OCIO::LogCameraTransform>(t)
        || OCIO::DynamicPtrCast<const OCIO::AllocationTransform>(t)
        || OCIO::DynamicPtrCast<const OCIO::Lut1DTransform>(t)
        // Per-channel curves, evaluated natively and bounded exactly like the
        // tabulated ones beside them. The built-in config authors Apple Log
        // this way itself, so excluding the family would refuse to recognize a
        // configuration that spells an encoding the way that config does.
        || OCIO::DynamicPtrCast<const OCIO::GradingRGBCurveTransform>(t))
        return true;
    if (auto builtin = OCIO::DynamicPtrCast<const OCIO::BuiltinTransform>(t)) {
        const std::string style = builtin->getStyle();
        return !Strutil::starts_with(style, "ACES-OUTPUT")
               && !Strutil::starts_with(style, "ACES-LMT");
    }
    if (auto fixed = OCIO::DynamicPtrCast<const OCIO::FixedFunctionTransform>(
            t)) {
        const auto style = fixed->getStyle();
        return style == OCIO::FIXED_FUNCTION_REC2100_SURROUND
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 4, 0)
               || style == OCIO::FIXED_FUNCTION_LIN_TO_DOUBLE_LOG
               || style == OCIO::FIXED_FUNCTION_LIN_TO_GAMMA_LOG
               || style == OCIO::FIXED_FUNCTION_LIN_TO_PQ
#endif
            ;
    }
    return false;
}

bool
measurable_expanded_transform(OCIO::ConstTransformRcPtr t)
{
    if (auto group = OCIO::DynamicPtrCast<const OCIO::GroupTransform>(t)) {
        for (int i = 0; i < group->getNumTransforms(); ++i)
            if (!measurable_expanded_transform(group->getTransform(i)))
                return false;
        return true;
    }
    return measurable_atomic_transform(t);
}

// The protocol probes negative handling, transfer knees, unequal channels,
// values above diffuse white, and alpha independently. Structural admission
// bounds its meaning; these samples are not proof for arbitrary transforms.
static const std::array<float, 4> analytic_probes[]
    = { { 0, 0, 0, 0 },
        { 1, 0, 0, 1 },
        { 0, 1, 0, .5f },
        { 0, 0, 1, 0 },
        { 1, 1, 1, 1 },
        { .18f, .18f, .18f, .5f },
        { .001f, .02f, .5f, 0 },
        { .25f, .5f, .75f, 1 },
        { -.1f, -.5f, -1.f, .5f },
        { -.02f, .3f, 1.5f, 0 },
        { 2, 4, .75f, 1 },
        { .0031307f, .0031308f, .0031309f, .5f },
        { .040449f, .04045f, .040451f, 1 },
        { .1f, .1f, .1f, -.25f },
        { .5f, .5f, .5f, 1.5f } };

// Compose one native matrix operation into an accumulating linear part.
// Anything that is not a plain invertible mixing of the three color channels
// is refused: a non-finite or singular matrix, a translation, or any coupling
// between color and alpha. `identity` reports whether the operation moved
// anything, which is what separates a gamut mapping from a no-op.
bool
compose_linear_matrix(OCIO::ConstMatrixTransformRcPtr matrix,
                      std::array<double, 9>& linear, bool& identity)
{
    double m[16], offset[4];
    matrix->getMatrix(m);
    matrix->getOffset(offset);
    identity = true;
    for (int j = 0; j < 16; ++j) {
        if (!std::isfinite(m[j]))
            return false;
        identity &= m[j] == (j % 5 == 0 ? 1.0 : 0.0);
    }
    for (int j = 0; j < 4; ++j)
        if (offset[j] != 0.0 || m[4 * j + 3] != (j == 3 ? 1.0 : 0.0)
            || m[12 + j] != (j == 3 ? 1.0 : 0.0))
            return false;
    const double det = m[0] * (m[5] * m[10] - m[6] * m[9])
                       - m[1] * (m[4] * m[10] - m[6] * m[8])
                       + m[2] * (m[4] * m[9] - m[5] * m[8]);
    if (!std::isfinite(det) || det == 0.0)
        return false;
    Imath::M33d effective(m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9],
                          m[10]);
    if (matrix->getDirection() == OCIO::TRANSFORM_DIR_INVERSE)
        effective = effective.inverse();
    const auto previous = linear;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) {
            double v = 0.0;
            for (int k = 0; k < 3; ++k)
                v += effective[row][k] * previous[3 * k + col];
            if (!std::isfinite(v))
                return false;
            linear[3 * row + col] = v;
        }
    return true;
}

// The parameter contract a parameterized per-channel curve must satisfy before
// the definition containing it may be recognized as an encoding: the three
// color channels bend identically, every parameter is finite and in range, and
// alpha passes through untouched. Being channel-independent is not enough on
// its own -- an exponent of (2.35, 2.2, 2.35) or a log with unequal side slopes
// is separable and still says nothing about a single encoding. Both the curve
// reader below and the linear-part fallback admit through here, so neither can
// accept a shape the other refuses. The read parameters are the reader's; the
// fallback discards them and keeps only the admission answer.
bool
read_separable_curve(OCIO::ConstTransformRcPtr t, AnalyticResponse& response)
{
    double gamma[4], offset[4] = {};
    if (auto power = OCIO::DynamicPtrCast<const OCIO::ExponentTransform>(t)) {
        response.transfer = 1;
        power->getValue(gamma);
        response.negative = power->getNegativeStyle();
    } else if (auto toe
               = OCIO::DynamicPtrCast<const OCIO::ExponentWithLinearTransform>(
                   t)) {
        response.transfer = 2;
        toe->getGamma(gamma);
        toe->getOffset(offset);
        response.negative = toe->getNegativeStyle();
    } else {
        // Native separable logs can carry useful gamut evidence without
        // being a scalar gamma or an established encoding identity.
        auto symmetric = [](const double (&v)[3]) {
            return std::isfinite(v[0]) && v[0] == v[1] && v[0] == v[2];
        };
        auto affine_parameters = [&](const auto& log) {
            double slope[3], offset[3];
            log->getLogSideSlopeValue(slope);
            log->getLogSideOffsetValue(offset);
            if (!symmetric(slope) || slope[0] <= 0.0 || !symmetric(offset))
                return false;
            response.log_slope  = slope[0];
            response.log_offset = offset[0];
            log->getLinSideSlopeValue(slope);
            log->getLinSideOffsetValue(offset);
            if (!symmetric(slope) || slope[0] <= 0.0 || !symmetric(offset))
                return false;
            response.lin_slope  = slope[0];
            response.lin_offset = offset[0];
            return true;
        };
        double base = 0.0;
        if (auto log = OCIO::DynamicPtrCast<const OCIO::LogTransform>(t)) {
            base              = log->getBase();
            response.log_kind = 1;
        } else if (auto log
                   = OCIO::DynamicPtrCast<const OCIO::LogAffineTransform>(t)) {
            base = log->getBase();
            if (!affine_parameters(log))
                return false;
            response.log_kind = 2;
        } else if (auto log
                   = OCIO::DynamicPtrCast<const OCIO::LogCameraTransform>(t)) {
            base = log->getBase();
            if (!affine_parameters(log))
                return false;
            double breakpoint[3], slope[3], offset[3];
            log->getLinSideBreakValue(breakpoint);
            log->getLinSideSlopeValue(slope);
            log->getLinSideOffsetValue(offset);
            const double at_break = slope[0] * breakpoint[0] + offset[0];
            if (!symmetric(breakpoint) || !std::isfinite(at_break)
                || at_break <= 0.0)
                return false;
            if (log->getLinearSlopeValue(slope)) {
                if (!symmetric(slope) || slope[0] <= 0.0)
                    return false;
                response.linear_slope     = slope[0];
                response.has_linear_slope = true;
            }
            response.lin_break = breakpoint[0];
            response.log_kind  = 3;
        } else {
            return false;
        }
        if (!std::isfinite(base) || base <= 0.0 || base == 1.0)
            return false;
        response.base        = base;
        response.transfer    = 3;
        response.direction   = t->getDirection();
        response.transfer_op = t;
        return true;
    }
    if (!std::isfinite(gamma[0]) || gamma[0] <= 0.0 || gamma[0] != gamma[1]
        || gamma[0] != gamma[2] || gamma[3] != 1.0 || !std::isfinite(offset[0])
        || offset[0] != offset[1] || offset[0] != offset[2] || offset[3] != 0.0)
        return false;
    response.gamma       = gamma[0];
    response.offset      = offset[0];
    response.direction   = t->getDirection();
    response.transfer_op = t;
    // Normalize power direction, but retain native toe direction.
    if (response.transfer == 1) {
        if (response.direction == OCIO::TRANSFORM_DIR_INVERSE)
            response.gamma = 1.0 / response.gamma;
        response.direction = OCIO::TRANSFORM_DIR_FORWARD;
    }
    return true;
}

bool
analytic_response(OCIO::ConstProcessorRcPtr processor, bool reverse,
                  AnalyticResponse& response)
{
    auto group       = processor->createGroupTransform();
    bool matrix_seen = false;
    for (int i = 0; i < group->getNumTransforms(); ++i) {
        auto t = group->getTransform(i);
        if (auto matrix = OCIO::DynamicPtrCast<const OCIO::MatrixTransform>(t)) {
            bool identity = false;
            if (!compose_linear_matrix(matrix, response.linear, identity))
                return false;
            if (!identity) {
                if (!reverse && response.transfer)
                    return false;
                matrix_seen = true;
            }
            continue;
        }
        if (response.transfer || (reverse && matrix_seen))
            return false;
        if (!read_separable_curve(t, response))
            return false;
    }
    auto cpu = processor->getOptimizedCPUProcessor(OCIO::OPTIMIZATION_NONE);
    response.samples.assign(std::begin(analytic_probes),
                            std::end(analytic_probes));
    for (size_t i = 0; i < response.samples.size(); ++i) {
        auto& rgba = response.samples[i];
        cpu->applyRGBA(rgba.data());
        for (float v : rgba)
            if (!std::isfinite(v))
                return false;
        // Native moncurve evaluation can round even structurally identity
        // alpha. Use the response tolerance; authored alpha stays exact above.
        // A native clamping power also clamps negative alpha with exponent
        // one. Keep that sampled behavior for exact identity comparison while
        // permitting independent RGB properties; authored alpha is still one.
        const float alpha = response.transfer == 1
                                    && response.negative == OCIO::NEGATIVE_CLAMP
                                ? std::max(0.0f, analytic_probes[i][3])
                                : analytic_probes[i][3];
        if (std::abs(rgba[3] - alpha)
            > 2e-6f + 2e-6f * std::max(std::abs(rgba[3]), std::abs(alpha)))
            return false;
    }
    return true;
}

// The operations a per-channel curve is realized as. Membership is decided by
// each operation's own contract -- every output channel depends on that channel
// alone -- and never by what container a resource arrived in: a one-dimensional
// table is per-channel whether it was written as a `.spi1d`, a `.cube` or
// anything else. A table carrying a hue adjustment is not per-channel, and
// neither is anything absent from this list.
bool
separable_transform(OCIO::ConstTransformRcPtr t)
{
    if (auto lut = OCIO::DynamicPtrCast<const OCIO::Lut1DTransform>(t)) {
        // A table passes alpha through and touches no other channel, but
        // HUE_NONE only says the table is not a hue adjustment; it says
        // nothing about whether the three channels bend the same way. Read
        // the entries, which is the only statement of that a table makes.
        if (lut->getHueAdjust() != OCIO::HUE_NONE)
            return false;
        for (unsigned long i = 0, n = lut->getLength(); i < n; ++i) {
            float r, g, b;
            lut->getValue(i, r, g, b);
            if (!std::isfinite(r) || r != g || r != b)
                return false;
        }
        return true;
    }
    if (auto range = OCIO::DynamicPtrCast<const OCIO::RangeTransform>(t))
        return (!range->hasMinInValue()
                || std::isfinite(range->getMinInValue()))
               && (!range->hasMaxInValue()
                   || std::isfinite(range->getMaxInValue()))
               && (!range->hasMinOutValue()
                   || std::isfinite(range->getMinOutValue()))
               && (!range->hasMaxOutValue()
                   || std::isfinite(range->getMaxOutValue()));
    // Every other separable family is parameterized, and a parameterized curve
    // is admitted only on the same terms the curve reader admits it: equal
    // channels, finite and in-range parameters, untouched alpha. The read
    // parameters are discarded -- this establishes the linear part alone.
    AnalyticResponse discarded;
    return read_separable_curve(t, discarded);
}

// The linear part of a definition whose curve the reader above cannot state.
//
// That reader stops at the first operation it cannot restate as a gamma or a
// log, so a camera encoding shipped as a one-dimensional table beside an
// ordinary primaries matrix reports no primaries at all -- though its
// primaries were never in doubt and are recoverable without knowing anything
// about the curve. They are recoverable exactly when the realized chain
// separates: every operation is either a mixing matrix or a per-channel curve,
// and the two do not interleave. `reverse` says which order that means, in the
// same sense the reader uses -- decoding to the interchange the curve comes
// first, encoding away from it the matrix does.
//
// The non-interleaving condition is the whole guard, and it is not a
// formality. A chain that maps into one gamut, bends per channel there and
// maps into another has no single linear part; reading its leading matrices as
// primaries would report the intermediate working gamut, which is how an image
// formation chain's inset space gets published as the space's own. Such a
// chain is refused here rather than approximated.
//
// Only the linear part is taken. The curve is not sampled, not parameterized
// and not named, so what this establishes is the gamut and nothing else.
bool
native_gamut_matrix(OCIO::ConstProcessorRcPtr processor, bool reverse,
                    std::array<double, 9>& linear)
{
    auto group       = processor->createGroupTransform();
    bool matrix_seen = false, curve_seen = false;
    for (int i = 0; i < group->getNumTransforms(); ++i) {
        auto t = group->getTransform(i);
        if (auto matrix = OCIO::DynamicPtrCast<const OCIO::MatrixTransform>(t)) {
            bool identity = false;
            if (!compose_linear_matrix(matrix, linear, identity))
                return false;
            if (!identity) {
                if (!reverse && curve_seen)
                    return false;
                matrix_seen = true;
            }
            continue;
        }
        if (!separable_transform(t) || (reverse && matrix_seen))
            return false;
        curve_seen = true;
    }
    return true;
}

// The expanded-operation gate bounds what a comparison against an arbitrary
// configured transform is allowed to mean. The built-in interop-identities
// config is not an arbitrary transform, so its own definitions are measured
// without it; a definition OIIO cannot measure is skipped rather than trusted.
bool
probe_response(OCIO::ConstProcessorRcPtr processor, bool expanded_check,
               bool display, ProbeResponse& response)
{
    if (expanded_check
        && !measurable_expanded_transform(processor->createGroupTransform()))
        return false;
    auto cpu = processor->getOptimizedCPUProcessor(OCIO::OPTIMIZATION_NONE);
    if (display)
        response.samples.assign(std::begin(interop_probes_display),
                                std::end(interop_probes_display));
    else
        response.samples.assign(std::begin(interop_probes_scene),
                                std::end(interop_probes_scene));
    for (auto& rgba : response.samples) {
        cpu->applyRGBA(rgba.data());
        for (float v : rgba)
            if (!std::isfinite(v))
                return false;
    }
    return true;
}

bool
same_probe_response(const ProbeResponse& a, const ProbeResponse& b)
{
    if (a.samples.empty() || a.samples.size() != b.samples.size())
        return false;
    for (size_t i = 0; i < a.samples.size(); ++i)
        for (int j = 0; j < 4; ++j) {
            float x = a.samples[i][j], y = b.samples[i][j];
            if (!std::isfinite(x) || !std::isfinite(y)
                || std::abs(x - y) > interop_response_tolerance)
                return false;
        }
    return true;
}

// Apply bounded structural admission before treating finite samples as
// equality evidence. File-backed supported transforms are evaluated by OCIO,
// and whether their result may be published is a question about the key rather
// than about the provenance; the caller decides it.
class TransformClosure {
public:
    TransformClosure(OCIO::ConstConfigRcPtr config,
                     OCIO::ReferenceSpaceType reference)
        : m_config(config)
        , m_context(config->getCurrentContext())
        , m_reference(reference)
    {
    }

    bool space(OCIO::ConstColorSpaceRcPtr cs)
    {
        if (!cs || cs->isData() || cs->getReferenceSpaceType() != m_reference)
            return false;
        const std::string name = cs->getName();
        if (m_done.count(name))
            return true;
        if (!m_pending.insert(name).second)
            return false;
        bool ok = transform(cs->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE))
                  && transform(
                      cs->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE));
        m_pending.erase(name);
        if (ok)
            m_done.insert(name);
        return ok;
    }

    bool resource_free() const { return !m_external; }

private:
    bool transform(OCIO::ConstTransformRcPtr t)
    {
        if (!t)
            return true;
        if (OCIO::DynamicPtrCast<const OCIO::FileTransform>(t)) {
            // The expanded-operation gate decides what the resource contains,
            // whatever its container, and still rejects a 3D LUT.
            m_external = true;
            return true;  // Native processor acquisition evaluates the resource.
        }
        if (auto group = OCIO::DynamicPtrCast<const OCIO::GroupTransform>(t)) {
            for (int i = 0; i < group->getNumTransforms(); ++i)
                if (!transform(group->getTransform(i)))
                    return false;
            return true;
        }
        if (auto link = OCIO::DynamicPtrCast<const OCIO::ColorSpaceTransform>(
                t)) {
            std::string src = m_context->resolveStringVar(link->getSrc());
            std::string dst = m_context->resolveStringVar(link->getDst());
            return space(m_config->getColorSpace(src.c_str()))
                   && space(m_config->getColorSpace(dst.c_str()));
        }
        return measurable_atomic_transform(t);
    }

    OCIO::ConstConfigRcPtr m_config;
    OCIO::ConstContextRcPtr m_context;
    OCIO::ReferenceSpaceType m_reference;
    bool m_external = false;
    std::unordered_set<std::string> m_pending, m_done;
};

bool
same_analytic_response(const AnalyticResponse& a, const AnalyticResponse& b)
{
    if (a.transfer == 3 || b.transfer == 3 || a.transfer != b.transfer
        || a.direction != b.direction
        || (a.transfer && a.negative != b.negative)
        || std::abs(a.gamma - b.gamma) > 1e-12
        || std::abs(a.offset - b.offset) > 1e-12
        || a.samples.size() != b.samples.size())
        return false;
    for (size_t i = 0; i < a.samples.size(); ++i)
        for (int j = 0; j < 4; ++j) {
            float x = a.samples[i][j], y = b.samples[i][j];
            if (!std::isfinite(x) || !std::isfinite(y)
                || std::abs(x - y)
                       > 2e-6f + 2e-6f * std::max(std::abs(x), std::abs(y)))
                return false;
        }
    return true;
}

spin_rw_mutex analytic_reference_mutex;
std::map<std::string, std::shared_ptr<const AnalyticPair>> analytic_references;
std::map<std::string, std::shared_ptr<const ProbeResponse>> probe_references;

std::shared_ptr<const AnalyticPair>
analytic_pair(OCIO::ConstConfigRcPtr config, const char* interchange,
              const char* endpoint)
{
    auto result  = std::make_shared<AnalyticPair>();
    auto context = config->getCurrentContext();
    for (int reverse = 0; reverse < 2; ++reverse) {
        auto processor = config->getProcessor(context,
                                              reverse ? endpoint : interchange,
                                              reverse ? interchange : endpoint);
        if (!analytic_response(processor, reverse != 0, (*result)[reverse]))
            return {};
    }
    return result;
}

// One direction only, interchange to endpoint, which is the direction the
// probes are stated in. The reverse conversion carries the same information
// about an invertible definition and, for one authored as a table, carries
// its inversion error as well; measuring it would reject such spaces without
// separating any encoding the forward direction leaves ambiguous.
std::shared_ptr<const ProbeResponse>
probe_response_of(OCIO::ConstConfigRcPtr config, const char* interchange,
                  const char* endpoint, bool display, bool expanded_check)
{
    auto result    = std::make_shared<ProbeResponse>();
    auto context   = config->getCurrentContext();
    auto processor = config->getProcessor(context, interchange, endpoint);
    if (!probe_response(processor, expanded_check, display, *result))
        return {};
    return result;
}

// Reference measurements for the analytic passes, kept in the same retained
// map as the identity ones under their own variant key so a retained empty
// response can never be read as a comparable one. A definition this build
// cannot separate into a curve and a matrix is a stable property of the fixed
// built-in config, so the empty result is retained instead of remeasured; it
// can never match, so it can never make a unique answer ambiguous.
std::shared_ptr<const AnalyticPair>
retained_analytic_pair(OCIO::ConstConfigRcPtr reference, const char* role,
                       const char* id)
{
    const std::string key = std::string(reference_revision)
                            + ":analytic-v1:" + id;
    {
        spin_rw_read_lock lock(analytic_reference_mutex);
        auto found = analytic_references.find(key);
        if (found != analytic_references.end())
            return found->second;
    }
    auto measured = analytic_pair(reference, role, id);
    if (!measured)
        measured = std::make_shared<const AnalyticPair>();
    spin_rw_write_lock lock(analytic_reference_mutex);
    auto retained = analytic_references.emplace(key, measured).first->second;
    DBG("Color analytic reference prepared: {}\n", id);
    return retained;
}


bool
same_linear_response(const AnalyticResponse& a, const AnalyticResponse& b)
{
    for (size_t i = 0; i < a.linear.size(); ++i)
        if (!std::isfinite(a.linear[i]) || !std::isfinite(b.linear[i])
            || std::abs(a.linear[i] - b.linear[i])
                   > 2e-6
                         + 2e-6
                               * std::max(std::abs(a.linear[i]),
                                          std::abs(b.linear[i])))
            return false;
    return true;
}

// Exact comparison of the separated log curve. Parameter agreement is
// agreement everywhere, including below the linear break and outside [0,1],
// which neither a sampled comparison nor a slope profile establishes. A curve
// alone names no encoding; the caller must also establish the gamut.
bool
same_log_curve(const AnalyticResponse& a, const AnalyticResponse& b)
{
    auto agree = [](double x, double y) {
        return std::isfinite(x) && std::isfinite(y)
               && std::abs(x - y)
                      <= 1e-12 + 1e-12 * std::max(std::abs(x), std::abs(y));
    };
    return a.transfer == 3 && b.transfer == 3 && a.log_kind == b.log_kind
           && a.direction == b.direction
           && a.has_linear_slope == b.has_linear_slope && agree(a.base, b.base)
           && agree(a.log_slope, b.log_slope)
           && agree(a.log_offset, b.log_offset)
           && agree(a.lin_slope, b.lin_slope)
           && agree(a.lin_offset, b.lin_offset)
           && agree(a.lin_break, b.lin_break)
           && agree(a.linear_slope, b.linear_slope);
}



// ---------------------------------------------------------------------------
// Adaptation-aware gamut derivation
// ---------------------------------------------------------------------------
//
// A configuration and the built-in config can reach one gamut through
// different chromatic adaptations. Comparing the composed matrices, as
// same_linear_response does, then reports a difference that is not a
// difference of encoding. Widening that comparison is the wrong repair: it
// would also admit genuinely different gamuts.
//
// Instead each hypothesis is constructed forward -- adapt the published
// primaries to the interchange white under one installed method, then compare
// the constructed matrix with the measured one. Inverting the measured matrix
// and reading primaries back cannot settle this, because every (white, method)
// pair reproduces the interchange white by construction and so is exactly
// self-consistent; the hypotheses differ only in the primaries they imply.
// Constructed forward they are decisive, and the residual stays strict.
//
// Everything below is published primaries and the two standard cone response
// matrices. None of it is derived from, or describes, any configuration.

// R, G, B, W as xy pairs.
using GamutPrimaries = std::array<double, 8>;

const GamutPrimaries gamut_rec709 { 0.64, 0.33, 0.30,   0.60,
                                    0.15, 0.06, 0.3127, 0.3290 };
const GamutPrimaries gamut_rec2020 { 0.708, 0.292, 0.170,  0.797,
                                     0.131, 0.046, 0.3127, 0.3290 };
const GamutPrimaries gamut_rec601 { 0.63,  0.34, 0.31,   0.595,
                                    0.155, 0.07, 0.3127, 0.3290 };
const GamutPrimaries gamut_rec601pal { 0.64, 0.33, 0.29,   0.60,
                                       0.15, 0.06, 0.3127, 0.3290 };
const GamutPrimaries gamut_p3d65 { 0.68, 0.32, 0.265,  0.69,
                                   0.15, 0.06, 0.3127, 0.3290 };
const GamutPrimaries gamut_p3d60 { 0.68, 0.32, 0.265,   0.69,
                                   0.15, 0.06, 0.32168, 0.33767 };
const GamutPrimaries gamut_p3dci { 0.68, 0.32, 0.265, 0.69,
                                   0.15, 0.06, 0.314, 0.351 };
const GamutPrimaries gamut_ap0 { 0.7347, 0.2653, 0.0,     1.0,
                                 0.0001, -0.077, 0.32168, 0.33767 };
const GamutPrimaries gamut_ap1 { 0.713, 0.293, 0.165,   0.830,
                                 0.128, 0.044, 0.32168, 0.33767 };
const GamutPrimaries gamut_adobergb { 0.64, 0.33, 0.21,   0.71,
                                      0.15, 0.06, 0.3127, 0.3290 };
const GamutPrimaries gamut_bmdwg5 { 0.7177215, 0.3171181,  0.228041, 0.861569,
                                    0.1005841, -0.0820452, 0.312717, 0.3290312 };
const GamutPrimaries gamut_sgamut3venice { 0.74046426, 0.27936437, 0.08924115,
                                           0.89380953, 0.11048824, -0.05257933,
                                           0.3127,     0.3290 };
const GamutPrimaries gamut_sgamut3cinevenice { 0.77590187, 0.27450239,
                                               0.1886829,  0.82868494,
                                               0.10133738, -0.08918752,
                                               0.3127,     0.3290 };

const GamutPrimaries* const gamut_library[] = { &gamut_rec709,
                                                &gamut_rec2020,
                                                &gamut_rec601,
                                                &gamut_rec601pal,
                                                &gamut_p3d65,
                                                &gamut_p3d60,
                                                &gamut_p3dci,
                                                &gamut_ap0,
                                                &gamut_ap1,
                                                &gamut_adobergb,
                                                &gamut_bmdwg5,
                                                &gamut_sgamut3venice,
                                                &gamut_sgamut3cinevenice };


// Tristimulus and RGB triples. A plain array rather than Imath::V3d, whose
// operator[] in Imath 3.1 is (&x)[i]: for i > 0 that reads past the member it
// points into, which is undefined behavior, and Intel icpx 2023 compiles the
// loops below to NaN when they index a V3d.
using GamutVector = std::array<double, 3>;


GamutVector
gamut_xy_to_xyz(double x, double y)
{
    const double d = y != 0.0 ? y : std::numeric_limits<double>::epsilon();
    return { x / d, 1.0, (1.0 - x - y) / d };
}


// Explicit column-vector products: xyz = M * rgb, matching the convention
// reference_properties already reads its authored matrices in.
Imath::M33d
gamut_multiply(const Imath::M33d& a, const Imath::M33d& b)
{
    Imath::M33d out;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) {
            double v = 0.0;
            for (int k = 0; k < 3; ++k)
                v += a[row][k] * b[k][col];
            out[row][col] = v;
        }
    return out;
}


GamutVector
gamut_multiply(const Imath::M33d& a, const GamutVector& b)
{
    GamutVector out {};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            out[row] += a[row][col] * b[col];
    return out;
}


// Normalized primary matrix: RGB to CIE XYZ at the gamut's own white.
Imath::M33d
gamut_npm(const GamutPrimaries& primaries)
{
    const GamutVector red   = gamut_xy_to_xyz(primaries[0], primaries[1]);
    const GamutVector green = gamut_xy_to_xyz(primaries[2], primaries[3]);
    const GamutVector blue  = gamut_xy_to_xyz(primaries[4], primaries[5]);
    const GamutVector white = gamut_xy_to_xyz(primaries[6], primaries[7]);
    Imath::M33d columns(red[0], green[0], blue[0], red[1], green[1], blue[1],
                        red[2], green[2], blue[2]);
    const GamutVector scale = gamut_multiply(columns.inverse(), white);
    Imath::M33d npm;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            npm[row][col] = columns[row][col] * scale[col];
    return npm;
}


// Von Kries cone responses. 0 is no adaptation, which is the correct
// hypothesis for two gamuts that share a white.
Imath::M33d
gamut_cone_response(int method)
{
    if (method == 1)  // Bradford
        return Imath::M33d(0.8951, 0.2664, -0.1614, -0.7502, 1.7135, 0.0367,
                           0.0389, -0.0685, 1.0296);
    if (method == 2)  // CAT02
        return Imath::M33d(0.7328, 0.4296, -0.1624, -0.7036, 1.6975, 0.0061,
                           0.0030, 0.0136, 0.9834);
    return Imath::M33d();
}


// Von Kries adaptation between two whites under one cone response. Method 0
// is no adaptation, which is also what two equal whites require.
Imath::M33d
gamut_adaptation(const GamutVector& src_white, const GamutVector& dst_white,
                 int method)
{
    bool same_white = true;
    for (int i = 0; i < 3; ++i)
        same_white &= std::abs(src_white[i] - dst_white[i]) <= 1e-12;
    if (same_white || !method)
        return Imath::M33d();
    const Imath::M33d cone = gamut_cone_response(method);
    const GamutVector from = gamut_multiply(cone, src_white);
    const GamutVector to   = gamut_multiply(cone, dst_white);
    Imath::M33d scale;
    for (int i = 0; i < 3; ++i)
        scale[i][i] = from[i] != 0.0 ? to[i] / from[i] : 0.0;
    return gamut_multiply(cone.inverse(), gamut_multiply(scale, cone));
}


Imath::M33d
gamut_conversion(const GamutPrimaries& src, const GamutPrimaries& dst,
                 int method)
{
    const Imath::M33d src_npm = gamut_npm(src), dst_npm = gamut_npm(dst);
    const GamutVector unit { 1.0, 1.0, 1.0 };
    const Imath::M33d adaptation
        = gamut_adaptation(gamut_multiply(src_npm, unit),
                           gamut_multiply(dst_npm, unit), method);
    return gamut_multiply(dst_npm.inverse(),
                          gamut_multiply(adaptation, src_npm));
}


// Published white tristimuli at Y = 1, from the chromaticities the standards
// state rather than recomputed from the CIE daylight locus. These are the
// hypotheses the reconstruction below un-adapts under; they are also the
// whites OpenColorIO's own builtin matrices are generated from, so a
// configuration built by OCIO reconstructs to rounding.
struct NamedWhite {
    const char* name;
    GamutVector xyz;
};

const NamedWhite named_whites[] = {
    { "D65", { 0.95045592705167, 1.0, 1.08905775075988 } },
    { "D60", { 0.95264607456985, 1.0, 1.00882518435159 } },
    { "DCI", { 0.89458689458689, 1.0, 0.95441595441595 } },
    { "D50", { 0.96429567642956771, 1.0, 0.82510460251046047 } },
};


// CIE XYZ at a stated white, expressed the way the conversion above expresses
// an RGB gamut: axis-aligned primaries under the equal-energy illuminant, so
// its normalized primary matrix is the identity and the conversion reduces to
// the adaptation alone. Only the white participates.
Imath::M33d
gamut_to_xyz(const GamutPrimaries& src, const GamutVector& white, int method)
{
    const Imath::M33d npm = gamut_npm(src);
    const GamutVector unit { 1.0, 1.0, 1.0 };
    return gamut_multiply(gamut_adaptation(gamut_multiply(npm, unit), white,
                                           method),
                          npm);
}


// The interchange gamut the candidate table is built against is fixed by the
// role, not by the space under test, which is what makes the table shared.
// aces_interchange is ACES2065-1 by OCIO's own contract, and
// cie_xyz_d65_interchange is CIE XYZ at D65. CIE XYZ has no published RGB
// primaries of its own, so the display table is the same published gamuts
// carried to XYZ instead of to AP0; only the destination differs.
//
// The caller reaches here having already reduced the whole conversion to one
// 3x3 matrix, so a display-referred probe cannot half-reduce a rendering
// transform into a plausible wrong answer: a definition that is not a matrix
// never produces a matrix to test.
bool
match_gamut_library(const std::array<double, 9>& rgb_to_interchange,
                    OCIO::ReferenceSpaceType state, std::array<float, 8>& xy)
{
    struct Candidate {
        Imath::M33d matrix;
        const GamutPrimaries* primaries;
    };
    const auto build = [](bool display) {
        std::vector<Candidate> built;
        built.reserve(std::size(gamut_library) * 3);
        for (const GamutPrimaries* gamut : gamut_library)
            for (int method = 0; method < 3; ++method)
                built.push_back(
                    { display
                          ? gamut_to_xyz(*gamut, named_whites[0].xyz, method)
                          : gamut_conversion(*gamut, gamut_ap0, method),
                      gamut });
        return built;
    };
    static const std::vector<Candidate> scene_candidates   = build(false);
    static const std::vector<Candidate> display_candidates = build(true);
    const auto& candidates = state == OCIO::REFERENCE_SPACE_DISPLAY
                                 ? display_candidates
                                 : scene_candidates;
    // A chromaticity is a ratio, xyz over their sum, so a positive global
    // scalar on the whole matrix leaves all four of them exactly where they
    // were. Comparing absolute coefficients would make the same gamut
    // recognizable or not according to where the author happened to write that
    // scalar -- on the interchange role, in a group, in an external matrix
    // file, or in the endpoint chain -- which is a fact about the spelling and
    // not about the colorimetry. Dividing both sides by their own Frobenius
    // norm removes exactly that one degree of freedom and nothing else: an
    // unequal diagonal moves the white, a permutation moves the primaries and
    // a chromatic adaptation moves both, and all three survive the
    // normalization and still fail the residual below.
    //
    // The residual is then dimensionless, and no two library candidates differ
    // by a global scale, so the normalization cannot make one ambiguous. What
    // the scale does establish is luminance, and that is not a gamut question:
    // the sampled comparisons that decide identity are absolute, see the gain
    // and refuse the identity, which is tested separately.
    const auto norm = [](const double* m) {
        double sum = 0.0;
        for (int i = 0; i < 9; ++i)
            sum += m[i] * m[i];
        return std::sqrt(sum);
    };
    const double measured_norm = norm(rgb_to_interchange.data());
    if (!std::isfinite(measured_norm) || measured_norm <= 0.0)
        return false;
    // A true hypothesis reproduces the matrix to rounding while the nearest
    // other published gamut sits orders of magnitude away, so this stays a
    // residual on constructed coefficients rather than a widened comparison.
    const GamutPrimaries* best = nullptr;
    double best_residual       = 1e-5;
    for (const auto& candidate : candidates) {
        double candidate_norm = 0.0;
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                candidate_norm += candidate.matrix[row][col]
                                  * candidate.matrix[row][col];
        candidate_norm = std::sqrt(candidate_norm);
        if (!std::isfinite(candidate_norm) || candidate_norm <= 0.0)
            continue;
        double residual = 0.0;
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                residual = std::max(residual,
                                    std::abs(candidate.matrix[row][col]
                                                 / candidate_norm
                                             - rgb_to_interchange[3 * row + col]
                                                   / measured_norm));
        if (std::isfinite(residual) && residual < best_residual) {
            best_residual = residual;
            best          = candidate.primaries;
        }
    }
    if (!best)
        return false;
    for (int i = 0; i < 8; ++i)
        xy[i] = float((*best)[i]);
    return true;
}


// Round to six decimals, then drop the finer representation when a coarser
// one restates it exactly. A coordinate somebody typed survives; one that
// arrived through a matrix product keeps the digits the product produced.
double
round_chromaticity_coord(double value)
{
    const double rounded = std::round(value * 1e6) / 1e6;
    for (int digits : { 5, 4, 3, 2 }) {
        const double factor    = std::pow(10.0, digits);
        const double candidate = std::round(rounded * factor) / factor;
        if (std::abs(rounded - candidate) <= 2e-7)
            return candidate;
    }
    return rounded;
}


// How plainly written a reconstructed gamut's primaries are, in [0, 1] (nine
// decimals 0, whole numbers 1; the hypothesized white is not counted). Scored
// on doubles, since a float copy carries all nine decimals.
double
chromaticity_precision_score(const std::array<double, 8>& xy)
{
    const auto decimals = [](double value) {
        long long scaled = std::llround(std::abs(value) * 1e9);
        if (scaled == 0)
            return 0;
        int places = 9;
        while (places > 0 && scaled % 10 == 0) {
            scaled /= 10;
            --places;
        }
        return places;
    };
    int total = 0;
    for (int i = 0; i < 6; ++i)
        total += decimals(xy[i]);
    return 1.0 - double(total) / double(9 * 6);
}


// Reconstruct primaries a matrix implies under one (white, method) hypothesis.
// The measured matrix reaches the interchange, so un-adapting it back to the
// hypothesized white and reading the basis columns as chromaticities restates
// the gamut the configuration's author started from.
void
reconstruct_chromaticities(const std::array<double, 9>& rgb_to_interchange,
                           const Imath::M33d& interchange_to_xyz,
                           const GamutVector& interchange_white,
                           const GamutVector& white, int method,
                           std::array<double, 8>& xy)
{
    Imath::M33d measured;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            measured[row][col] = rgb_to_interchange[3 * row + col];
    // Carry the measurement into CIE XYZ, then un-adapt the interchange's own
    // white to the hypothesized one.
    const Imath::M33d rgb_to_xyz
        = gamut_multiply(gamut_adaptation(interchange_white, white, method),
                         gamut_multiply(interchange_to_xyz, measured));
    for (int i = 0; i < 4; ++i) {
        const GamutVector basis { double(i == 0 || i == 3),
                                  double(i == 1 || i == 3),
                                  double(i == 2 || i == 3) };
        const GamutVector xyz = gamut_multiply(rgb_to_xyz, basis);
        const double sum      = xyz[0] + xyz[1] + xyz[2];
        const double scale    = std::abs(sum) > 1e-12 ? sum : 1.0;
        xy[2 * i]             = round_chromaticity_coord(xyz[0] / scale);
        xy[2 * i + 1]         = round_chromaticity_coord(xyz[1] / scale);
    }
    if (xy[6] == xy[7])
        xy[6] = xy[7] = 1.0 / 3.0;
}


// Name the gamut a composed matrix carries. The published library answers
// exactly whenever the gamut is one it holds, whatever chromatic adaptation
// the configuration reached it through. Otherwise the primaries are
// reconstructed.
bool
derive_gamut_chromaticities(const std::array<double, 9>& rgb_to_interchange,
                            OCIO::ReferenceSpaceType state,
                            std::array<float, 8>& xy)
{
    // A trivial scene matrix lands directly on aces_interchange, whose OCIO
    // contract is ACES2065-1, so its coordinate basis establishes AP0 even
    // when the endpoint carries an otherwise unnamed transfer. Refusing exact
    // identity while accepting a positive scalar identity below would make the
    // same gamut less knowable merely because no exposure scale was present.
    // The display interchange is CIE XYZ D65 and has no RGB primaries, so its
    // trivial matrix still reports no gamut.
    bool trivial = true;
    for (int i = 0; i < 9; ++i)
        trivial &= rgb_to_interchange[i] == (i % 4 == 0 ? 1.0 : 0.0);
    if (trivial) {
        if (state != OCIO::REFERENCE_SPACE_SCENE)
            return false;
        for (int i = 0; i < 8; ++i)
            xy[i] = float(gamut_ap0[i]);
        return true;
    }
    for (double v : rgb_to_interchange)
        if (!std::isfinite(v))
            return false;
    if (match_gamut_library(rgb_to_interchange, state, xy))
        return true;

    // Reconstruction is scene-referred only, and deliberately so. A scene
    // matrix reaches AP0, whose white is known and whose gamut is known, so
    // un-adapting it isolates the primaries. A display matrix already lands
    // in CIE XYZ, where an unusual gamut, an unusual white and a plain gain
    // all reconstruct into plausible primaries and nothing separates them.
    // The published library above still names a display gamut it recognizes,
    // which is where the display answers come from; a display gamut it does
    // not recognize reports nothing.
    if (state == OCIO::REFERENCE_SPACE_DISPLAY)
        return false;
    const Imath::M33d interchange_to_xyz = gamut_npm(gamut_ap0);
    const GamutVector interchange_white
        = gamut_multiply(interchange_to_xyz, GamutVector { 1.0, 1.0, 1.0 });
    // Every (white, method) hypothesis reproduces the interchange white by
    // construction, so exact arithmetic cannot choose between them; authored
    // primaries come back in round decimals under the right hypothesis while
    // an arbitrary matrix yields six-decimal noise under all of them, so the
    // plainest reading above a floor wins and nothing is reported otherwise.
    std::array<double, 8> best {};
    double best_score = 0.6;
    for (const NamedWhite& white : named_whites)
        for (int method = 1; method <= 2; ++method) {
            std::array<double, 8> candidate {};
            reconstruct_chromaticities(rgb_to_interchange, interchange_to_xyz,
                                       interchange_white, white.xyz, method,
                                       candidate);
            bool finite = true;
            for (double v : candidate)
                finite &= std::isfinite(v);
            const double score = finite
                                     ? chromaticity_precision_score(candidate)
                                     : 0.0;
            if (finite && score > best_score) {
                best_score = score;
                best       = candidate;
            }
        }
    if (best_score <= 0.6)
        return false;
    for (int i = 0; i < 8; ++i)
        xy[i] = float(best[i]);
    return true;
}


// Identify a log encoding from its curve and its gamut separately, because
// the end-to-end comparison cannot: the same encoding reached through a
// different chromatic adaptation has a different composed response. Both
// halves are required and both are exact. A curve alone identifies nothing,
// and a gamut alone identifies nothing.
//
// Candidates are whatever the built-in config declares as a log encoding in
// the target's own image state, read at runtime, so a corrected or added
// reference definition participates without a code change.
std::string
recognize_log_reference(OCIO::ConstConfigRcPtr reference, const char* role,
                        OCIO::ReferenceSpaceType state,
                        const AnalyticPair& measured,
                        const std::array<float, 8>& xy)
{
    std::string matched;
    bool ambiguous = false;
    const int count
        = reference->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                       OCIO::COLORSPACE_ALL);
    for (int i = 0; i < count; ++i) {
        const char* id = reference->getColorSpaceNameByIndex(
            OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ALL, i);
        auto candidate = reference_space(id);
        if (!candidate || candidate->getReferenceSpaceType() != state
            || candidate->isData() || candidate->hasCategory("is-unique")
            || identity_excluded_from_matching(id)
            || string_view(candidate->getEncoding()) != "log")
            continue;
        auto expected = retained_analytic_pair(reference, role, id);
        if (!same_log_curve(measured[0], (*expected)[0])
            || !same_log_curve(measured[1], (*expected)[1]))
            continue;
        std::array<float, 8> candidate_xy {};
        if (!derive_gamut_chromaticities((*expected)[1].linear, state,
                                         candidate_xy)
            || candidate_xy != xy)
            continue;
        if (!matched.empty())
            ambiguous = true;
        matched = id;
    }
    return ambiguous ? std::string() : matched;
}


// The gamut component of an interop ID: one namespace and the state suffix
// are removed, and what follows the last underscore is the primaries.
// `oiio:g22_p3d65_display` is `p3d65`.
std::string
gamut_component(string_view interop_id)
{
    std::string base = Strutil::lower(interop_id);
    const auto colon = base.find(':');
    if (colon != std::string::npos)
        base = base.substr(colon + 1);
    for (string_view suffix : { "_scene", "_display" })
        if (base.size() > suffix.size() && Strutil::ends_with(base, suffix)) {
            base.resize(base.size() - suffix.size());
            break;
        }
    const auto separator = base.rfind('_');
    return separator == std::string::npos ? std::string()
                                          : base.substr(separator + 1);
}


// The gamut an established identity carries is the built-in config's own
// declaration about itself, not a measurement of the configuration under test.
// The authored-definition reader below can only describe the unadapted display
// definitions it lists; this recovers the rest, so an identity reached by
// declaration and one reached by measurement report the same properties.
bool
measured_identity_chromaticities(OCIO::ConstConfigRcPtr reference,
                                 string_view identity, std::array<float, 8>& xy)
{
    auto cs = reference_space(std::string(identity).c_str());
    if (!cs || cs->isData())
        return false;
    const auto state = cs->getReferenceSpaceType();
    const char* role = state == OCIO::REFERENCE_SPACE_SCENE
                           ? "aces_interchange"
                           : "cie_xyz_d65_interchange";
    if (!reference->hasRole(role))
        return false;
    auto measured = retained_analytic_pair(reference, role, cs->getName());
    return !(*measured)[1].samples.empty()
           && derive_gamut_chromaticities((*measured)[1].linear, state, xy);
}


// The same question, answered from the built-in config's own vocabulary, for
// a definition the reader above cannot separate into a curve and a matrix.
// OpenColorIO tabulates several of its own encodings -- the ACEScc builtin
// expands to a range, a one-dimensional table and a matrix -- and the reader
// stops at the first operation it cannot parameterize, so the matrix beside
// it is never reached and a gamut the built-in config plainly names goes
// unreported.
//
// The gamut component of an interop ID is what the built-in config says that
// ID's primaries are, and every definition spelling one component carries the
// same ones. So the component is read across the built-in config, in the
// identity's own image state, and adopted only where the definitions that
// *can* be measured agree on exactly one set. Two readings, or none, report
// nothing. What is adopted is therefore a measurement of the built-in config
// that the spelling points at, never a value the spelling on its own implies.
bool
identity_component_chromaticities(OCIO::ConstConfigRcPtr reference,
                                  string_view identity,
                                  std::array<float, 8>& xy)
{
    auto cs = reference_space(std::string(identity).c_str());
    const std::string component = gamut_component(identity);
    if (!cs || cs->isData() || component.empty())
        return false;
    const auto state = cs->getReferenceSpaceType();
    std::array<float, 8> agreed {};
    bool agreed_once = false;
    const int count
        = reference->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                       OCIO::COLORSPACE_ALL);
    for (int i = 0; i < count; ++i) {
        const char* id = reference->getColorSpaceNameByIndex(
            OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ALL, i);
        auto candidate = id ? reference_space(id)
                            : OCIO::ConstColorSpaceRcPtr();
        if (!candidate || candidate->getReferenceSpaceType() != state
            || gamut_component(id) != component)
            continue;
        std::array<float, 8> candidate_xy {};
        if (!measured_identity_chromaticities(reference, id, candidate_xy))
            continue;
        if (agreed_once && candidate_xy != agreed)
            return false;  // The component names more than one gamut here.
        agreed      = candidate_xy;
        agreed_once = true;
    }
    if (agreed_once)
        xy = agreed;
    return agreed_once;
}


bool
identity_chromaticities(OCIO::ConstConfigRcPtr reference, string_view identity,
                        std::array<float, 8>& xy)
{
    return measured_identity_chromaticities(reference, identity, xy)
           || identity_component_chromaticities(reference, identity, xy);
}


// Whether the two independently authored directions compose to the identity.
// A reverse that is not the forward's inverse describes no single matrix, so
// neither direction may be read as a gamut or as a reciprocal gamma.
bool
reciprocal_matrices(const std::array<double, 9>& forward,
                    const std::array<double, 9>& reverse)
{
    AnalyticResponse product, identity;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) {
            double v = 0.0;
            for (int k = 0; k < 3; ++k)
                v += forward[3 * row + k] * reverse[3 * k + col];
            product.linear[3 * row + col] = v;
        }
    return same_linear_response(product, identity);
}


bool
reciprocal_matrices(const AnalyticPair& pair)
{
    return reciprocal_matrices(pair[0].linear, pair[1].linear);
}


// The decoding exponent of a pair separated into a pure power (or no curve)
// and reciprocal matrices, 1 for linear. Zero for any other shape.
float
pure_power_gamma(const AnalyticPair& pair)
{
    const auto& encode = pair[0];
    const auto& decode = pair[1];
    const float power  = float(decode.gamma);
    return encode.transfer <= 1 && decode.transfer <= 1
                   && encode.transfer == decode.transfer
                   && encode.negative == decode.negative
                   && std::abs(encode.gamma * decode.gamma - 1.0) <= 1e-12
                   && std::isfinite(power) && power > 0.0f
                   && reciprocal_matrices(pair)
               ? power
               : 0.0f;
}


// The exponent an established identity's own built-in definition decodes
// with, measured like its gamut above, so a declared identity reports the
// exponent a measured one would. Zero when that definition is no pure power.
float
identity_gamma(OCIO::ConstConfigRcPtr reference, string_view identity)
{
    auto cs = reference_space(std::string(identity).c_str());
    if (!cs || cs->isData())
        return 0.0f;
    const auto state = cs->getReferenceSpaceType();
    const char* role = state == OCIO::REFERENCE_SPACE_SCENE
                           ? "aces_interchange"
                           : "cie_xyz_d65_interchange";
    // The shapes recognize_analytic admits, and no others.
    AnalyticClosure closure(reference, state);
    if (!reference->hasRole(role) || !closure.space(cs)
        || !closure.space(reference->getColorSpace(role)))
        return 0.0f;
    auto measured = retained_analytic_pair(reference, role, cs->getName());
    return (*measured)[1].samples.empty() ? 0.0f : pure_power_gamma(*measured);
}


// The gamut half of what an analytic pair would have carried, for a definition
// whose curve this build cannot state. Both directions are read from the
// caller's own configuration and context, exactly as the analytic pair is, and
// the two linear parts must be each other's inverse before either is reported
// -- a definition whose to- and from-reference transforms disagree describes no
// single matrix, and nothing may be read as a gamut from it.
bool
native_gamut_pair(OCIO::ConstConfigRcPtr config, const char* interchange,
                  const char* endpoint, std::array<double, 9>& decode)
{
    auto context = config->getCurrentContext();
    std::array<double, 9> encode { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    decode = encode;
    return native_gamut_matrix(config->getProcessor(context, interchange,
                                                    endpoint),
                               false, encode)
           && native_gamut_matrix(config->getProcessor(context, endpoint,
                                                       interchange),
                                  true, decode)
           && reciprocal_matrices(encode, decode);
}


bool reference_properties(OCIO::ConstConfigRcPtr reference,
                          string_view identity, std::array<float, 8>& xy,
                          bool& has_xy, float& gamma);

// Every reference definition an exact comparison may name. The list is not a
// statement about which encodings exist: it is the subset whose primaries the
// authored-definition reader can also describe, so a match here carries both
// an identity and a gamut. Definitions outside it still participate through
// the measured comparison below.
static const char* analytic_candidates[] = { "lin_rec709_scene",
                                             "lin_rec2020_scene",
                                             "lin_p3d65_scene",
                                             "lin_adobergb_scene",
                                             "g18_rec709_scene",
                                             "g22_rec709_scene",
                                             "g24_rec709_scene",
                                             "g22_adobergb_scene",
                                             "srgb_rec709_scene",
                                             "srgb_p3d65_scene",
                                             "lin_rec709_display",
                                             "lin_rec2020_display",
                                             "lin_p3d65_display",
                                             "oiio:lin_p3d60_display",
                                             "oiio:lin_p3dci_display",
                                             "oiio:lin_rec601_display",
                                             "oiio:lin_rec601pal_display",
                                             "oiio:lin_prophoto_display",
                                             "g22_rec709_display",
                                             "g24_rec709_display",
                                             "g22_adobergb_display",
                                             "g26_p3d65_display",
                                             "oiio:g22_p3d65_display",
                                             "oiio:g22_p3d50_display",
                                             "oiio:g22_adobergbd50_display",
                                             "oiio:g24_rec601_display",
                                             "oiio:g24_rec601pal_display",
                                             "oiio:g24_rec2020_display",
                                             "oiio:g26_p3dci_display",
                                             "oiio:g26_p3d60_display",
                                             "srgb_rec709_display",
                                             "srgb_p3d65_display",
                                             "srgbe_p3d65_display" };

bool
analytic_candidate(string_view identity)
{
    for (const char* candidate : analytic_candidates)
        if (identity == candidate)
            return true;
    return false;
}

// Whether two definitions that each authored a curve authored the same curve.
// Defined with the probe positions it evaluates on, below.
bool same_authored_curve(const AnalyticResponse& a, const AnalyticResponse& b);

// General comparison against the built-in config. Candidates are whatever
// that configuration defines in the target's own image state, not a separate
// list of supported encodings, so an added or corrected reference definition
// participates without further code. The target must still satisfy the
// structural admission above; these bounded samples are evidence about an
// admitted shape, never a claim about an arbitrary function.
//
// `shape` is the target's separated curve and matrix when this build could
// separate them, and `declared` the identity its naming points at, so the
// sweep can report that it tested that identity and did not reproduce it.
std::string
recognize_reference(OCIO::ConstConfigRcPtr config,
                    OCIO::ConstConfigRcPtr reference, const char* name,
                    bool resource_key_ok, const AnalyticPair* shape,
                    std::shared_ptr<const ProbeResponse>& target_probe_response,
                    string_view declared, bool& attempted, bool& admitted,
                    bool& evaluated, bool& contradicted)
{
    auto target = config->getColorSpace(name);
    if (!target || target->isData() || target->hasCategory("is-unique"))
        return {};
    const auto state = target->getReferenceSpaceType();
    const char* role = state == OCIO::REFERENCE_SPACE_SCENE
                           ? "aces_interchange"
                           : "cie_xyz_d65_interchange";
    if (!config->hasRole(role) || !reference->hasRole(role))
        return {};
    TransformClosure closure(config, state);
    if (!closure.space(target) || !closure.space(config->getColorSpace(role)))
        return {};
    const bool display = state == OCIO::REFERENCE_SPACE_DISPLAY;
    attempted          = true;
    // A resource in the walked chain only bars publication when the key this
    // result would be published under cannot describe that resource. When the
    // full native key succeeded, it already hashes every referenced file.
    admitted      = closure.resource_free() || resource_key_ok;
    auto measured = target_probe_response;
    if (!measured)
        measured = probe_response_of(config, role, name, display, true);
    if (!measured) {
        // An expanded operation or alpha behavior outside the comparison is a
        // completed negative verdict. Acquisition failures raise instead, so
        // they stay retryable.
        evaluated = true;
        return {};
    }
    if (!target_probe_response) {
        target_probe_response = measured;
        DBG("Color probe measured: {}\n", name);
    }
    // Both directions are part of what a color space is, and OpenColorIO uses
    // each one as authored: it derives a direction by inversion only when that
    // direction is absent, and it never reconciles two that are present. The
    // comparison this pass runs reads interchange -> endpoint alone, for the
    // reason stated at probe_response_of, so a definition that authors its
    // reverse independently -- and authors it as something else -- reproduces
    // a reference encoding forward while converting back to a different one.
    // Nothing above sees that: the forward samples are the reference's, and
    // the candidate's own declaration then fills in primaries measurement did
    // not establish.
    //
    // So take the endpoint samples this pass just measured back through the
    // space's own reverse and require the probes to return, at the tolerance
    // the forward is accepted at. This is evidence about the space, not about
    // one candidate: every reference encoding round-trips, so a reverse that
    // contradicts the forward rules all of them out at once, and the identity
    // is refused as a whole while the separated directional facts the passes
    // above established are untouched. A definition with one authored
    // direction returns the probes by construction, and so does a pair whose
    // authored inverse is approximate, rounded or tabulated: the bound is the
    // response tolerance, not the exact-comparison one.
    //
    // Measured at most once, and only after some candidate has reproduced, so
    // a space this pass names nothing for pays nothing for the check. The
    // reverse processor is the one the analytic pass has already built for
    // this configuration and context wherever that pass ran. A native
    // acquisition failure raises here as everywhere else and reaches the
    // caller's retry path rather than becoming a verdict.
    int reversible = -1;  // -1 not yet measured, 0 contradicted, 1 consistent
    auto reverses  = [&]() {
        if (reversible < 0) {
            auto cpu
                = config->getProcessor(config->getCurrentContext(), name, role)
                      ->getOptimizedCPUProcessor(OCIO::OPTIMIZATION_NONE);
            const std::array<float, 4>* probes = display
                                                     ? interop_probes_display
                                                     : interop_probes_scene;
            reversible                         = 1;
            for (size_t i = 0; i < measured->samples.size(); ++i) {
                std::array<float, 4> rgba = measured->samples[i];
                cpu->applyRGBA(rgba.data());
                for (int j = 0; j < 4; ++j)
                    if (!(std::abs(rgba[j] - probes[i][j])
                          <= interop_response_tolerance))
                        reversible = 0;
            }
            if (!reversible)
                DBG("Color probe reverse contradicts: {}\n", name);
        }
        return reversible > 0;
    };
    // Two passes in the built-in config's own declaration order: an
    // unnamespaced identity answers before a project-namespaced one, and
    // within a pass the first definition that reproduces the measurement is
    // the answer. Later ones cannot unseat it and do not make it ambiguous.
    //
    // Some pairs genuinely cannot be separated by measurement at all --
    // srgb_p3d65_display and srgbe_p3d65_display have identical authored
    // operations and differ only in the dynamic range they declare -- so
    // declining on a second match would decline on those permanently and
    // return nothing where the standard, first-declared reading is available.
    // The declaration order is fixed by the built-in config, so the answer is
    // deterministic; that config's own comment records the intended reading.
    //
    // What a configuration declares its encoding to be does not select the
    // candidates. It is a label its author wrote, in the same class of
    // evidence as a name, and production configurations routinely disagree
    // with the reference about it: the extended-range display spaces are
    // spelled `hdr-video` over the same operations the reference spells
    // `sdr-video`, and a cinema space may spell it `cinema-sdr` where the
    // reference spells it `sdr-cinema`. The image state, which OpenColorIO
    // enforces, is the only gate.
    //
    // The identity the naming points at is a selector, so the sweep runs to
    // the end and that identity's own verdict is known. Where the measurement
    // reproduces it, it is the answer even though an equally valid reading was
    // declared earlier: the two are the same colorimetry, and between two
    // readings a measurement cannot separate, the one the configuration's
    // author asked for is the better answer.
    //
    // Only after that may the declared encoding speak, and only between
    // readings the measurement itself cannot separate. It never removes a
    // candidate -- a unique mathematical match is the answer whatever label
    // either side carries -- but where two equally valid readings remain and
    // exactly one of them declares the label the space declares, that one is
    // the answer. Where the label is absent, unrecognized, or shared by more
    // than one of the tied readings, the first-declared reading answers, which
    // is the deterministic fallback the built-in declaration order fixes.
    struct Slot {
        std::string first, labelled;
        bool ambiguous = false;
        void add(const char* id, bool labelled_match)
        {
            if (first.empty())
                first = id;
            if (!labelled_match)
                return;
            if (labelled.empty())
                labelled = id;
            else
                ambiguous = true;
        }
        const std::string& pick() const
        {
            return labelled.empty() || ambiguous ? first : labelled;
        }
        // No later reading can change pick(): the first reading is labelled
        // itself, or two labelled readings already made it the answer.
        bool decided() const
        {
            return !first.empty() && (ambiguous || labelled == first);
        }
    };
    Slot bare, namespaced;
    std::string confirmed;
    const string_view encoding(target->getEncoding());
    const int count
        = reference->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                       OCIO::COLORSPACE_ALL);
    // The bare pass is measured before the namespaced one; each pass keeps its
    // own declaration order, which is all either reading depends on.
    for (int i = 0; i < 2 * count; ++i) {
        const char* id = reference->getColorSpaceNameByIndex(
            OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ALL, i % count);
        const bool prefixed = string_view(id).find(':') != string_view::npos;
        if (prefixed != (i >= count))
            continue;
        const bool is_declared = !declared.empty() && declared == id;
        auto candidate         = reference_space(id);
        if (!candidate || candidate->getReferenceSpaceType() != state
            || candidate->isData() || candidate->hasCategory("is-unique")
            || identity_excluded_from_matching(id))
            continue;
        const bool labelled = !encoding.empty()
                              && encoding
                                     == string_view(candidate->getEncoding());
        // Past a confirmed declaration, a decided pass, or any bare reading for
        // a namespaced candidate, nothing can change the answer, and past a
        // pass's first reading only a labelled one can, so the rest are not
        // measured.
        const Slot& slot = prefixed ? namespaced : bare;
        if (!is_declared
            && (!confirmed.empty() || (prefixed && !bare.first.empty())
                || slot.decided() || (!labelled && !slot.first.empty())))
            continue;
        const std::string key = std::string(reference_revision)
                                + ":probe-v2:" + id;
        std::shared_ptr<const ProbeResponse> expected;
        {
            spin_rw_read_lock lock(analytic_reference_mutex);
            auto found = probe_references.find(key);
            if (found != probe_references.end())
                expected = found->second;
        }
        if (!expected) {
            expected = probe_response_of(reference, role, id, display, false);
            // Acquisition failures raise and stay retryable. A null result is
            // the fixed built-in definition leaving the sampled domain, which
            // is stable, so it is retained as an empty response.
            if (!expected) {
                DBG("Color probe reference excluded: {}\n", id);
                expected = std::make_shared<const ProbeResponse>();
            }
            spin_rw_write_lock lock(analytic_reference_mutex);
            expected = probe_references.emplace(key, expected).first->second;
            DBG("Color probe reference prepared: {}\n", id);
        }
        if (expected->samples.empty())
            continue;  // Nothing was compared, so nothing was ruled out.
        bool reproduced = same_probe_response(*measured, *expected);
        if (reproduced && shape) {
            // The sampled comparison is deliberately wide enough to accept a
            // curve supplied as a table, which also makes it unable to
            // separate a 0.055 knee from a 0.056 one. Where both definitions
            // authored their curve, compare the curves themselves: what they
            // do, evaluated apart from the gamut matrix, rather than the
            // parameters they state.
            auto authored = retained_analytic_pair(reference, role, id);
            reproduced    = (*authored)[0].samples.empty()
                            || same_authored_curve((*shape)[0], (*authored)[0]);
        }
        if (reproduced)
            reproduced = reverses();
        if (!reproduced) {
            contradicted |= is_declared;
            continue;
        }
        if (is_declared)
            confirmed = id;
        // First declaration wins within each pass; the declared encoding only
        // separates readings that are otherwise tied there.
        (prefixed ? namespaced : bare).add(id, labelled);
    }
    evaluated = true;
    if (!confirmed.empty())
        return confirmed;
    return bare.first.empty() ? namespaced.pick() : bare.pick();
}

// Caller checks native declarations first. Acquisition failures propagate and
// stay retryable.
//
// `publishable` says whether this pass's outcome may be shared under the
// caller's key:
//
//   - A measurement that cannot be stood behind -- an unmeasurable candidate,
//     or a resource the key cannot describe -- is not publishable.
//   - Unread operations or a missing interchange role are a completed verdict
//     that only a key change can alter, so they are publishable.
//   - An unresolved link or a cycle is what native acquisition would raise
//     on, so it stays retryable (not publishable).
std::string
recognize_analytic(OCIO::ConstConfigRcPtr config,
                   OCIO::ConstConfigRcPtr reference, const char* name,
                   bool resource_key_ok, bool& publishable, bool& evaluated,
                   std::array<float, 8>& xy, bool& has_xy, float& gamma,
                   std::shared_ptr<const AnalyticPair>& shape)
{
    auto target = config->getColorSpace(name);
    if (!target || target->isData())
        return {};
    auto state       = target->getReferenceSpaceType();
    const char* role = state == OCIO::REFERENCE_SPACE_SCENE
                           ? "aces_interchange"
                           : "cie_xyz_d65_interchange";
    if (!config->hasRole(role) || !reference->hasRole(role)) {
        publishable = true;
        return {};
    }
    auto interchange = config->getColorSpace(role);
    AnalyticClosure closure(config, state);
    if (!closure.space(target) || !closure.space(interchange)) {
        publishable = closure.resolved();
        return {};
    }
    // Same admission rule as the sampled pass: provenance alone does not bar
    // publication once the full native key can describe the resource.
    publishable   = closure.resource_free() || resource_key_ok;
    auto measured = shape;
    if (!measured)
        measured = analytic_pair(config, role, name);
    if (!measured) {
        // The curve is not one this pass can state, but the gamut beside it may
        // still be independently readable; see native_gamut_pair. Only the
        // primaries follow from it. No identity is claimed, no exponent is
        // recorded, the separated shape stays unset and the pass is still not
        // `evaluated`, so a definition this build cannot describe goes on being
        // undescribed apart from the one fact that was recovered. `publishable`
        // is already decided above and this does not revisit it: the same
        // resource admission governs a gamut read from the same operations.
        std::array<double, 9> linear {};
        if (native_gamut_pair(config, role, name, linear))
            has_xy = derive_gamut_chromaticities(linear, state, xy);
        return {};
    }
    if (!shape)
        DBG("Color analytic measured: {}\n", name);
    // The separated curve and matrix are a fact about the definition whatever
    // this pass concludes; the sampled comparison that runs next reads them.
    shape = measured;
    // Reuse existing reference measurements for identity and independent gamut
    // evidence. A new pure-power gamma does not require a new identity entry.
    std::string matched;
    bool ambiguous_identity = false, ambiguous_gamut = false;
    bool matched_gamut = false;
    std::array<float, 8> measured_xy {};
    for (const char* id : analytic_candidates) {
        auto candidate = reference_space(id);
        if (!candidate || candidate->getReferenceSpaceType() != state)
            continue;
        std::string key = std::string(reference_revision)
                          + ":analytic-v2:" + id;
        std::shared_ptr<const AnalyticPair> expected;
        {
            spin_rw_read_lock lock(analytic_reference_mutex);
            auto found = analytic_references.find(key);
            if (found != analytic_references.end())
                expected = found->second;
        }
        if (!expected) {
            expected = analytic_pair(reference, role, id);
            if (!expected) {
                // Distinct from a resource verdict: the measurement itself is
                // incomplete, so no conclusion drawn from it may be shared.
                DBG("Color analytic reference unavailable: {}\n", id);
                publishable = false;
                return {};  // An unavailable candidate cannot prove uniqueness.
            }
            spin_rw_write_lock lock(analytic_reference_mutex);
            expected = analytic_references.emplace(key, expected).first->second;
            DBG("Color analytic reference prepared: {}\n", id);
        }
        // The declared encoding does not participate; see recognize_reference.
        if (same_analytic_response((*measured)[0], (*expected)[0])
            && same_analytic_response((*measured)[1], (*expected)[1])) {
            if (!matched.empty())
                ambiguous_identity = true;
            matched = id;
        }
        if (same_linear_response((*measured)[0], (*expected)[0])
            && same_linear_response((*measured)[1], (*expected)[1])) {
            std::array<float, 8> candidate_xy {};
            bool candidate_has_xy = false;
            float candidate_gamma = 0.0f;
            reference_properties(reference, id, candidate_xy, candidate_has_xy,
                                 candidate_gamma);
            if (candidate_has_xy) {
                if (matched_gamut && measured_xy != candidate_xy)
                    ambiguous_gamut = true;
                measured_xy   = candidate_xy;
                matched_gamut = true;
            }
        }
    }
    if (matched_gamut && !ambiguous_gamut) {
        xy     = measured_xy;
        has_xy = true;
    } else if (!matched_gamut) {
        // No reference definition shares this composed matrix. The gamut may
        // still be a published one reached under a different chromatic
        // adaptation, which the forward hypothesis sweep names without
        // relaxing any comparison, and a gamut outside the library is
        // reconstructed from the matrix instead of going unreported. Only one
        // direction is read here, so both must agree first.
        //
        // The matrix is read as measured, in whatever frame the configuration's
        // own interchange role establishes. Both readings below are invariant
        // to a positive global scale on it -- reconstruction because every
        // chromaticity is a ratio, the library comparison because it normalizes
        // -- so an interchange that is the reference up to luminance still
        // establishes the primaries exactly, wherever the author wrote that
        // scale. Any other stated basis moves the ratios themselves and no
        // primaries follow from it, which is what both readings then report.
        has_xy = reciprocal_matrices(*measured)
                 && derive_gamut_chromaticities((*measured)[1].linear, state,
                                                xy);
    }
    // A log encoding cannot be established end to end for the same reason, so
    // it is established from its curve and its gamut, both exactly. Guarded on
    // the gamut being known: without it there is no identity to claim.
    if (matched.empty() && has_xy && (*measured)[0].transfer == 3
        && (*measured)[1].transfer == 3)
        matched = recognize_log_reference(reference, role, state, *measured,
                                          xy);
    // Scalar gamma describes the separated nonnegative decoding curve, not
    // the full conversion's gamut or linear gain. Preserve that partial fact
    // even when the matrix does not match a reference gamut or whitepoint.
    // Negative-style distinctions still constrain exact identity above.
    if (float power = pure_power_gamma(*measured); power > 0.0f)
        gamma = power;
    evaluated = true;
    // Like the sampled pass, a unique space is never named by comparison with
    // a reference definition; its measured curve and primaries still stand.
    return ambiguous_identity || target->hasCategory("is-unique")
               ? std::string()
               : matched;
}

bool
reference_properties(OCIO::ConstConfigRcPtr reference, string_view identity,
                     std::array<float, 8>& xy, bool& has_xy, float& gamma)
{
    string_view definition = property_display_definition(identity);
    if (definition.empty())
        return false;
    auto cs        = reference_space(std::string(definition).c_str());
    auto transform = cs->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE);
    auto matrix = OCIO::DynamicPtrCast<const OCIO::MatrixTransform>(transform);
    float exponent = 1.0f;
    if (auto group = OCIO::DynamicPtrCast<const OCIO::GroupTransform>(
            transform)) {
        if (group->getDirection() != OCIO::TRANSFORM_DIR_FORWARD
            || group->getNumTransforms() != 2)
            return false;
        matrix = OCIO::DynamicPtrCast<const OCIO::MatrixTransform>(
            group->getTransform(0));
        auto transfer = group->getTransform(1);
        if (auto power = OCIO::DynamicPtrCast<const OCIO::ExponentTransform>(
                transfer)) {
            double values[4];
            power->getValue(values);
            if (values[0] != values[1] || values[0] != values[2]
                || !std::isfinite(values[0]) || values[0] <= 0.0)
                return false;
            exponent = float(power->getDirection()
                                     == OCIO::TRANSFORM_DIR_INVERSE
                                 ? values[0]
                                 : 1.0 / values[0]);
        } else if (OCIO::DynamicPtrCast<const OCIO::ExponentWithLinearTransform>(
                       transfer)) {
            exponent = 0.0f;
        } else {
            return false;
        }
    }
    if (!matrix || matrix->getDirection() != OCIO::TRANSFORM_DIR_FORWARD)
        return false;
    double values[16], offset[4];
    matrix->getMatrix(values);
    matrix->getOffset(offset);
    for (int i = 0; i < 4; ++i)
        if (offset[i] != 0.0 || values[4 * i + 3] != (i == 3 ? 1.0 : 0.0)
            || values[12 + i] != (i == 3 ? 1.0 : 0.0))
            return false;
    Imath::M33d rgb_to_xyz(values[0], values[1], values[2], values[4],
                           values[5], values[6], values[8], values[9],
                           values[10]);
    rgb_to_xyz = rgb_to_xyz.inverse();
    for (int i = 0; i < 4; ++i) {
        double xyz[3] {};
        for (int row = 0; row < 3; ++row)
            xyz[row] = i < 3 ? rgb_to_xyz[row][i]
                             : rgb_to_xyz[row][0] + rgb_to_xyz[row][1]
                                   + rgb_to_xyz[row][2];
        double sum = xyz[0] + xyz[1] + xyz[2];
        if (!std::isfinite(sum) || sum <= 0.0)
            return false;
        xy[2 * i]     = float(xyz[0] / sum);
        xy[2 * i + 1] = float(xyz[1] / sum);
    }
    has_xy = true;
    gamma  = identity == "ocio:itu709_rec709_scene" ? 0.0f : exponent;
    return true;
}


// ---------------------------------------------------------------------------
// Transfer recognition by slope profile
// ---------------------------------------------------------------------------
//
// The comparisons above establish an encoding by reproducing its operations
// or its end-to-end response. Neither can speak for a curve that is composed
// rather than authored: a camera encoding reached through a different
// chromatic adaptation, or one tabulated instead of parameterized, has the
// same curve and a different response, and its operations no longer resemble
// the definition it implements. Those are ordinary cases -- most camera log
// encodings, and the extended-range display spaces broadcast configurations
// carry -- so the curve is also measured on its own, separately from the
// gamut, and an identity requires both to agree.
//
// Each probe position below is chosen to separate curves where they actually
// differ: the toe positions separate camera logs from gamma, 0.005 sits on the
// sRGB linear-segment break, 0.01 separates the D-Log and LogC3 breaks, and
// 1.1 separates a display curve that clips at reference white from one
// carrying headroom.
static const double transfer_probes[] = { -0.005, 0.0,  0.002, 0.005, 0.01,
                                          0.05,   0.18, 0.50,  1.0,   1.1 };
static const int transfer_probe_count = int(std::size(transfer_probes));
// Index of the reference-white probe, and of the 0.18 to 0.50 slope the
// profile is normalized against.
static const int transfer_white_probe  = 8;
static const int transfer_anchor_slope = 6;
// Per-encoding slope tolerance. Wider for log and high dynamic range because
// those curves vary more across their profile.
static const double transfer_tolerance_sdr = 0.02;
static const double transfer_tolerance_log = 0.05;
static const double transfer_tolerance_hdr = 0.1;
// Fraction of compared slopes that must agree. The remainder accommodates the
// masked first and last positions, which describe clipping rather than shape.
static const double transfer_min_score = 0.8;
// Normalized slopes discard constant gain deliberately, so the gain at
// reference white is compared separately. It is what separates a cinema
// distribution curve from an ordinary 2.6 gamma behind the projection
// headroom scale.
static const double transfer_white_gain_tolerance = 0.01;

// How far apart two authored curves may evaluate and still be one curve. It
// sits between the two comparisons on either side of it: an order tighter
// than the response tolerance, which is wide enough to accept a curve supplied
// as a table and therefore cannot see a knee offset of 0.056 in place of
// 0.055, and orders looser than the exact operation comparison, which
// separates two spellings of one curve. A published camera curve rounded to
// the digits its vendor states, with the linear-segment slope stated where
// OpenColorIO would derive it, moves the response by tens of microunits; a
// different published encoding moves it by hundreds.
static const double authored_curve_tolerance = 1e-4;


// Compare the separated curves by what they do, evaluated natively and apart
// from the gamut matrix beside them. Comparing the parameters instead rejects
// two spellings of one curve, which is the ordinary case between a vendor's
// published rounding and a configuration generated at full precision.
//
// Negative probe positions are excluded deliberately: the samples never reach
// them, and clamp-versus-mirror is the ordinary difference between one
// configuration's spelling of an encoding and another's.
bool
same_authored_curve(const AnalyticResponse& a, const AnalyticResponse& b)
{
    // No curve on either side is agreement; a curve against none is not.
    if (!a.transfer_op || !b.transfer_op)
        return !a.transfer_op && !b.transfer_op;
    static const OCIO::ConstConfigRcPtr raw = OCIO::Config::CreateRaw();
    OCIO::ConstCPUProcessorRcPtr cpu[2];
    try {
        const OCIO::ConstTransformRcPtr op[2] = { a.transfer_op,
                                                  b.transfer_op };
        for (int side = 0; side < 2; ++side)
            cpu[side] = raw->getProcessor(op[side])->getOptimizedCPUProcessor(
                OCIO::OPTIMIZATION_NONE);
    } catch (const std::exception&) {
        return false;
    }
    int compared = 0;
    for (int i = 0; i < transfer_probe_count; ++i) {
        if (transfer_probes[i] < 0.0)
            continue;
        float value[2][3];
        for (int side = 0; side < 2; ++side) {
            for (int c = 0; c < 3; ++c)
                value[side][c] = float(transfer_probes[i]);
            cpu[side]->applyRGB(value[side]);
        }
        for (int c = 0; c < 3; ++c) {
            // A curve with no linear segment diverges at zero. Two that
            // diverge at the same position are not separated there; one that
            // diverges where the other does not is a different curve.
            const bool finite[2] = { bool(std::isfinite(value[0][c])),
                                     bool(std::isfinite(value[1][c])) };
            if (finite[0] != finite[1])
                return false;
            if (!finite[0])
                continue;
            if (std::abs(double(value[0][c]) - double(value[1][c]))
                > authored_curve_tolerance)
                return false;
            ++compared;
        }
    }
    return compared > 0;
}


// Reference measurements of the fixed built-in config, in the same retained
// store and under the same lock as the other reference passes, keyed by their
// own variants so one pass's record is never read as another's. Only a valid
// signature is ever retained here; see reference_transfer_signature.
std::map<std::string, std::shared_ptr<const TransferSignature>>
    transfer_references;


double
transfer_slope_tolerance(string_view encoding)
{
    if (encoding == "log")
        return transfer_tolerance_log;
    if (encoding == "hdr-video" || encoding == "hdr-cinema")
        return transfer_tolerance_hdr;
    return transfer_tolerance_sdr;
}


bool
transfer_clips_superwhite(const std::vector<double>& values)
{
    return values.size() >= 2
           && std::abs(values.back() - values[values.size() - 2]) < 1e-6;
}


// Probe the neutral axis of an already-built conversion into the space under
// test. Channel averaging keeps the profile independent of the gamut matrix
// the conversion also carries, which is the point: the curve is being
// measured apart from the primaries.
//
// Taking a processor rather than a pair of names is what lets one measurement
// protocol answer for a color space, a NamedTransform and a transform supplied
// as text: all three are the same encoding-direction conversion once
// OpenColorIO has built them, so they are comparable without a second
// comparator.
TransferSignature
transfer_signature(OCIO::ConstProcessorRcPtr processor)
{
    TransferSignature signature;
    try {
        if (!processor)
            return {};
        auto cpu = processor->getOptimizedCPUProcessor(OCIO::OPTIMIZATION_NONE);
        signature.values.reserve(transfer_probe_count);
        for (int i = 0; i < transfer_probe_count; ++i) {
            float rgb[3] = { float(transfer_probes[i]),
                             float(transfer_probes[i]),
                             float(transfer_probes[i]) };
            cpu->applyRGB(rgb);
            const double mean = (double(rgb[0]) + rgb[1] + rgb[2]) / 3.0;
            if (!std::isfinite(mean))
                return {};
            signature.values.push_back(mean);
        }
    } catch (const std::exception&) {
        return {};
    }
    std::vector<double> slopes;
    slopes.reserve(transfer_probe_count - 1);
    for (int i = 0; i + 1 < transfer_probe_count; ++i) {
        const double step = transfer_probes[i + 1] - transfer_probes[i];
        slopes.push_back((signature.values[i + 1] - signature.values[i])
                         / (std::abs(step) > 1e-15 ? step : 1e-15));
    }
    const double anchor = slopes[transfer_anchor_slope];
    if (!std::isfinite(anchor) || std::abs(anchor) < 1e-12)
        return {};
    for (double& slope : slopes) {
        slope /= anchor;
        if (!std::isfinite(slope))
            return {};
    }
    signature.slopes = std::move(slopes);
    return signature;
}


// The same measurement for a named endpoint, from the linear source the caller
// nominates. Acquisition failures answer with an invalid signature, which
// compares equal to nothing -- and which says nothing about *why*: a
// definition this build cannot describe and a resource it could not reach this
// once are the same answer here, and nothing downstream can tell them apart.
// That is why no caller retains an invalid signature as a settled result.
TransferSignature
transfer_signature(OCIO::ConstConfigRcPtr config, const char* linear_source,
                   const char* name)
{
    try {
        return transfer_signature(
            config->getProcessor(config->getCurrentContext(), linear_source,
                                 name));
    } catch (const std::exception&) {
        return {};
    }
}


// One reference definition measured in the encoding direction, from the
// interchange role its own image state names, and retained: the built-in
// config is fixed, so the recognition pass below measures each one once.
//
// An invalid signature is returned unretained. It reports what this build could
// not measure now, which is not a property of the built-in config, so the next
// call tries again -- and a definition that stays unreadable is skipped rather
// than counted, because failing to reach one candidate is not evidence about
// the candidates that remain.
std::shared_ptr<const TransferSignature>
reference_transfer_signature(OCIO::ConstConfigRcPtr reference, const char* role,
                             const std::string& id)
{
    const std::string key = std::string(reference_revision)
                            + ":transfer-v1:" + role + ":" + id;
    {
        spin_rw_read_lock lock(analytic_reference_mutex);
        auto found = transfer_references.find(key);
        if (found != transfer_references.end())
            return found->second;
    }
    auto measured = std::make_shared<const TransferSignature>(
        transfer_signature(reference, role, id.c_str()));
    if (!measured->valid())
        return measured;
    spin_rw_write_lock lock(analytic_reference_mutex);
    return transfer_references.emplace(key, measured).first->second;
}


bool
same_transfer_signature(const TransferSignature& a, const TransferSignature& b,
                        string_view encoding)
{
    if (!a.valid() || !b.valid() || a.slopes.size() != b.slopes.size())
        return false;
    const double tolerance = transfer_slope_tolerance(encoding);
    const bool clips       = transfer_clips_superwhite(a.values)
                             || transfer_clips_superwhite(b.values);
    const int count        = int(a.slopes.size());
    int compared = 0, within = 0;
    for (int i = 0; i < count; ++i) {
        // The first position describes only how the curve treats negative
        // input, and the last only whether it clips; neither is shape.
        if (i == 0 || (i == count - 1 && clips))
            continue;
        ++compared;
        if (std::abs(a.slopes[i] - b.slopes[i]) <= tolerance)
            ++within;
    }
    if (!compared || double(within) / double(compared) < transfer_min_score)
        return false;
    const double left  = a.values[transfer_white_probe];
    const double right = b.values[transfer_white_probe];
    const double scale = std::max({ std::abs(left), std::abs(right), 1e-12 });
    return std::abs(left - right) / scale <= transfer_white_gain_tolerance;
}


// Measure the space's curve and primaries separately and look for the
// reference definition that agrees on both. This is what recognizes an
// encoding whose composed response differs from the reference's -- a camera
// gamut reached through another chromatic adaptation, a curve supplied as a
// table -- without weakening either half: the primaries must be the same
// published set, and the curve must agree across its profile.
//
// Scene-referred only. A display encoding is reached through a rendering
// transform whose curve is not the encoding's own, so probing one measures the
// rendering rather than the space, and there is nothing for the comparison to
// mean.
std::string
recognize_transfer_and_gamut(OCIO::ConstConfigRcPtr config,
                             OCIO::ConstConfigRcPtr reference, const char* name,
                             const char* role, OCIO::ReferenceSpaceType state,
                             const std::array<float, 8>& xy)
{
    if (state != OCIO::REFERENCE_SPACE_SCENE)
        return {};
    auto target = config->getColorSpace(name);
    if (!target)
        return {};
    string_view encoding(target->getEncoding());
    if (encoding == "scene-linear" || encoding == "display-linear")
        return {};  // A linear space is its gamut; there is no curve to match.
    const auto measured = transfer_signature(config, role, name);
    if (!measured.valid())
        return {};
    std::string bare, namespaced;
    const int count
        = reference->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                       OCIO::COLORSPACE_ALL);
    for (int i = 0; i < count && bare.empty(); ++i) {
        const char* id = reference->getColorSpaceNameByIndex(
            OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ALL, i);
        auto candidate = reference_space(id);
        if (!candidate || candidate->getReferenceSpaceType() != state
            || candidate->isData() || candidate->hasCategory("is-unique")
            || identity_excluded_from_matching(id))
            continue;
        string_view candidate_encoding(candidate->getEncoding());
        if (candidate_encoding == "scene-linear"
            || candidate_encoding == "display-linear")
            continue;
        const bool prefixed = string_view(id).find(':') != string_view::npos;
        if (prefixed && !namespaced.empty())
            continue;
        // Primaries first: it is the cheaper half and it is already memoized,
        // so a candidate on a different gamut costs no probe at all.
        std::array<float, 8> candidate_xy {};
        if (!identity_chromaticities(reference, id, candidate_xy)
            || candidate_xy != xy)
            continue;
        auto expected = reference_transfer_signature(reference,
                                                     "aces_interchange", id);
        // The space under test states how much its own curve varies across
        // its profile, which is what the tolerance is for; the candidate's
        // label stands in only where the space carries none. The candidates
        // themselves are not filtered by it -- the primaries and the curve
        // decide -- for the reason recognize_reference gives.
        if (!same_transfer_signature(measured, *expected,
                                     encoding.empty() ? candidate_encoding
                                                      : encoding))
            continue;
        (prefixed ? namespaced : bare) = id;
    }
    return bare.empty() ? namespaced : bare;
}

// Every comparison above reads the operations OpenColorIO built, so a
// non-default optimization level changes what can be read from a definition.
// The same string that gates the passes also belongs in the key any result
// they produce is retained under.
bool
analytic_optimization(std::string& flags)
{
    flags = Sysutil::getenv("OCIO_OPTIMIZATION_FLAGS");
    if (flags.empty())
        return true;
    try {
        return std::stoul(flags, nullptr, 0) == 0;
    } catch (...) {
        return false;
    }
}

// The identity every record about a definition is retained under: the caller's
// configuration key, the built-in config revision, and the optimization
// level. `mode` names the question that was asked of the definition, so records
// answering different questions share the one map and never each other's keys.
std::string
properties_cache_id(const std::string& config_id,
                    const std::string& optimization, const char* mode)
{
    std::string id = config_id;
    id += "\x1f";
    id += reference_revision;
    id += "\x1f"
          "probe-v2:";
    id += optimization;
    if (mode && *mode) {
        id += "\x1f";
        id += mode;
    }
    return id;
}

// OCIO::Config::IdentifyBuiltinColorSpace answers with the first active color
// space, in configuration order, whose conversion to the identity returns five
// fixed probe colors to within its tolerance (1e-3 before OCIO 2.5, 5e-3
// since), and it builds every processor it tests uncached, on every call. With
// both interchange roles present no heuristic reads the active list, so this
// deactivates, on the detached copy that call will search, the leading spaces
// whose conversion -- the same two processors, taken from the caches of the
// configurations they belong to -- misses by more than either tolerance. OCIO
// still gives the answer; it just starts at the first space that could be it.
//
// Returns that first space when its conversion is also within 1e-3 of every
// probe, less the same margin, so that every OCIO version accepts it unless
// its rules pass over the space's own transforms; otherwise null.
const char*
narrow_identification(OCIO::ConfigRcPtr& analysis,
                      OCIO::ConstConfigRcPtr config, const char* identity)
{
    std::string flags;
    if (!analytic_optimization(flags))
        return nullptr;  // Optimization could fuse the halves OCIO tests as one.
    auto reference = internal_reference();
    auto target    = reference->getColorSpace(identity);
    if (!target || target->isData())
        return nullptr;
    const auto state = target->getReferenceSpaceType();
    const char* role = state == OCIO::REFERENCE_SPACE_SCENE
                           ? "aces_interchange"
                           : "cie_xyz_d65_interchange";
    if (!config->hasRole(role) || !reference->hasRole(role))
        return nullptr;
    // The replacement list must select exactly the spaces meant: one spelling
    // the comma-separated list cannot carry, or that selects another space,
    // leaves the list alone.
    std::string inactive;
    auto add = [&](const char* name) {
        auto cs = config->getColorSpace(name);
        if (!cs || string_view(cs->getName()) != name || !*name
            || strchr(name, ',') || Strutil::strip(name) != name)
            return false;
        inactive += inactive.empty() ? "" : ", ";
        inactive += name;
        return true;
    };
    const int inactive_count
        = config->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL,
                                    OCIO::COLORSPACE_INACTIVE);
    for (int i = 0; i < inactive_count; ++i)
        if (!add(config->getColorSpaceNameByIndex(
                OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_INACTIVE, i)))
            return nullptr;
    // OCIO's own probe colors, RGBA.
    static const float probes[20] = { .7f, .4f, .02f, 0,   .02f, .6f, .2f,
                                      0,   .3f, .02f, .5f, 0,    0,   0,
                                      0,   0,   1,    1,   1,    0 };

    bool narrowed     = false;
    const char* first = nullptr;
    try {
        auto encode  = reference
                           ->getProcessor(reference->getCurrentContext(), role,
                                          identity)
                           ->getOptimizedCPUProcessor(OCIO::OPTIMIZATION_NONE);
        auto context = config->getCurrentContext();
        for (int i = 0, n = config->getNumColorSpaces(); i < n; ++i) {
            const char* name = config->getColorSpaceNameByIndex(i);
            auto cs          = config->getColorSpace(name);
            if (!cs || cs->isData() || cs->getReferenceSpaceType() != state)
                continue;  // OCIO passes over these itself.
            float rgba[20];
            std::copy(std::begin(probes), std::end(probes), rgba);
            OCIO::PackedImageDesc pixels(rgba, 5, 1, 4);
            config->getProcessor(context, name, role)
                ->getOptimizedCPUProcessor(OCIO::OPTIMIZATION_NONE)
                ->apply(pixels);
            encode->apply(pixels);
            bool miss = false, sure = true;
            for (int j = 0; j < 20; ++j) {
                const float error = std::abs(rgba[j] - probes[j]);
                miss |= !(error <= 5e-3f + 1e-5f);
                sure &= error <= 1e-3f - 1e-5f;
            }
            if (!miss || !add(name)) {
                first = miss || !sure ? nullptr : name;
                break;  // OCIO's answer starts here.
            }
            narrowed = true;
        }
    } catch (const std::exception&) {
        // A space that raises is left for OCIO, which may skip it.
    }
    if (narrowed)
        analysis->setInactiveColorSpaces(inactive.c_str());
    return first;
}


// The primaries and transfer function a built-in identity's own definition
// states. Both a derivation and a declaration of the identity report these.
void
identity_properties(string_view identity, std::array<float, 8>& xy,
                    bool& has_xy, float& gamma)
{
    auto reference = internal_reference();
    if (!reference_properties(reference, identity, xy, has_xy, gamma)) {
        has_xy = identity_chromaticities(reference, identity, xy);
        gamma  = identity_gamma(reference, identity);
    }
}
}  // namespace


ColorSpaceInfo
ColorConfig::Impl::color_space_info(string_view colorspace, bool derive,
                                    bool measured_only, bool enrich) const
{
    // Enrichment measures, so it belongs to the derived query alone. The cheap
    // query stays what it says it is and reports whatever a derivation already
    // retained under this same key, which is why the two agree afterwards.
    enrich           = enrich && derive && !measured_only;
    const CSInfo* cs = find(colorspace);
    bool lookup_ok   = true;
    if (!cs) {
        for (int i = 0; i < 5; ++i)
            if (colorspace == builtin_identities[i]) {
                if (auto bridged = retained_bridge(i))
                    cs = find(*bridged);
                break;
            }
    }
    if (!cs) {
        // The cheap query takes the steps of resolve() that need no
        // measurement, such as a stripped namespace or a legacy name.
        int recognized       = 0;
        string_view resolved = resolve(colorspace, &lookup_ok, &recognized,
                                       derive);
        if (derive || !recognized)
            cs = find(resolved);
    }
    if (!cs)
        return builtin_identity_info(colorspace);
    std::string known_identity;
    bool bridge_identity = false;
    // A measured query is answered by the definition's own operations. What an
    // author declared, what a site convention canonicalizes and which local
    // space the identity bridge picked for a portable identity are all
    // statements about intent, so none of them seeds a hypothesis here.
    if (!measured_only) {
        known_identity = m_catalog ? cs->interop_id : cs->canonical;
        if (!valid_interop_id(known_identity))
            known_identity.clear();  // e.g. the synthetic "Rec709"
        if (known_identity.empty()) {
            for (int i = 0; i < 5; ++i)
                if (auto bridged = retained_bridge(i);
                    bridged && *bridged == cs->name) {
                    known_identity  = builtin_identities[i];
                    bridge_identity = true;
                    break;
                }
        }
    }
    std::string optimization;
    const bool analytic_enabled = analytic_optimization(optimization);
    // The two modes answer different questions about the same definition, so
    // their verdicts share the one map and never each other's keys. A successful
    // runtime measurement may seed the other mode under this same scoped key.
    const std::string properties_id = m_interop_cache_id.empty()
                                          ? m_properties_cache_id
                                          : m_interop_cache_id;
    const std::string cache_id
        = properties_cache_id(properties_id, optimization,
                              measured_only ? "measured-v1" : "");
    const auto key    = std::make_pair(cache_id, cs->name);
    const bool shared = lookup_ok && m_interop_cache_safe
                        && (!m_interop_cache_id.empty()
                            || !m_properties_cache_id.empty());
    bool publish      = !m_interop_cache_id.empty();
    if (shared) {
        ColorSpaceInfo hit;
        {
            spin_rw_read_lock lock(properties_mutex);
            auto found = properties_memo.find(key);
            if (found != properties_memo.end()
                && !(derive && !known_identity.empty()
                     && !found->second.m_impl->identity_evaluated
                     && !(bridge_identity
                          && found->second.m_impl->analytic_evaluated
                          && analytic_candidate(known_identity)))) {
                DBG("Color properties shared hit: {}\n", cs->name);
                hit = found->second;
            }
        }
        // A retained record answers the derivation. It may still owe the one
        // thing only enrichment establishes, and acquiring that is not a
        // reason to derive the record again: what was published about the
        // identity, the gamut and the exponent stands.
        if (hit.m_impl)
            return enrich ? enriched(*cs, hit, key, publish && lookup_ok) : hit;
    }
    ColorSpaceInfo result;
    auto value               = std::make_shared<ColorSpaceInfo::Impl>();
    result.m_impl            = value;
    value->color_interop_id  = m_catalog ? cs->interop_id : known_identity;
    value->interop_computed  = true;
    value->encoding          = cs->encoding;
    value->encoding_computed = true;
    if (shared && derive) {
        const std::string evidence_cache_id
            = properties_cache_id(properties_id, optimization,
                                  measured_only ? "" : "measured-v1");
        const auto evidence_key = std::make_pair(evidence_cache_id, cs->name);
        spin_rw_read_lock lock(properties_mutex);
        auto found = properties_memo.find(evidence_key);
        if (found != properties_memo.end()
            && !found->second.m_impl->incomplete) {
            value->target_probe_response
                = found->second.m_impl->target_probe_response;
            value->target_analytic = found->second.m_impl->target_analytic;
        }
    }
    value->image_state = cs->flags() & CSInfo::is_data
                             ? std::string()
                             : std::string(
                                   known_image_state(value->color_interop_id,
                                                     value->encoding));
    value->image_state_computed = true;
    if (cs->flags() & CSInfo::is_data) {
        // Native data classification completes both questions directly: a
        // data space has neither colorimetric primaries nor a color transfer.
        value->chromaticities_computed = true;
        value->transfer_computed       = true;
        value->equality_id             = data_equality_token(*cs);
        value->equality_computed       = true;
        value->color_interop_id        = value->equality_id;
        return result;
    }
    // A declared linear encoding is honored, not measured: it seeds gamma 1.0
    // here, and only a measured pure-power exponent replaces it below. A
    // measured query declares nothing of its own, so it starts from no
    // transfer hypothesis at all; seeding one there would also let the
    // declaration pass the reference-transfer check at the end of this
    // function, which exists to test what was measured.
    if (!measured_only) {
        value->gamma = cs->native_gamma;
        if (!m_catalog && (cs->flags() & CSInfo::is_linear_response))
            value->gamma = 1.0f;
        value->transfer_computed = value->gamma > 0.0f;
    }
    // A gamma seeded from the declared encoding alone yields to a measured
    // one, but an identity must still agree with the declaration.
    const float encoding_gamma = value->gamma;
    if (!derive) {
        // A Color Interop ID the config declares for this space -- its
        // interop_id, or else its name or an alias spelling one exactly --
        // gives that ID's properties, which is what deriving the ID itself
        // reports. Nothing about this space's own transforms is analyzed. An
        // ID contradicting a declared linear encoding is ignored, as
        // derivation ignores it.
        std::vector<std::string> spellings { m_catalog ? cs->interop_id
                                                       : cs->canonical };
        if (spellings[0].empty()) {
            spellings[0] = cs->name;
            spellings.insert(spellings.end(), cs->aliases.begin(),
                             cs->aliases.end());
        }
        for (const auto& spelling : spellings) {
            const std::string id = builtin_identity(spelling);
            std::array<float, 8> xy {};
            bool has_xy = false;
            float gamma = 0.0f;
            try {
                auto defined = id.empty() ? nullptr
                                          : reference_space(id.c_str());
                // Not an ID, or only a legacy or unnamespaced alias of one.
                if (!defined || id != defined->getName())
                    continue;
                identity_properties(id, xy, has_xy, gamma);
            } catch (const std::exception& e) {
                DBG("Declared identity properties unavailable: {}\n", e.what());
                break;
            }
            if (encoding_gamma == 1.0f && gamma != 1.0f)
                break;
            value->chromaticities          = xy;
            value->has_chromaticities      = has_xy;
            value->chromaticities_computed = has_xy;
            if (gamma > 0.0f) {
                value->gamma             = gamma;
                value->transfer_computed = true;
            }
            break;
        }
        // Derivation may still establish an ID and the state it names.
        value->interop_computed     = !value->color_interop_id.empty();
        value->image_state_computed = !value->image_state.empty();
        return result;
    }
    try {
        DBG("Color properties {}cold work: {}\n",
            measured_only ? "measured " : "", cs->name);
        std::string identity = known_identity;
        // A name or a legacy alias is evidence about what the author meant,
        // not about what the definition does. It selects a hypothesis, which
        // measurement may replace with its own answer and which the transfer
        // check at the end of this function may reject outright. What it may
        // never do is answer over a measurement that disagrees with it.
        bool legacy_identity = false;
        // Set by the pass that separates a definition into a curve and a
        // matrix, and read by the sampled pass after it.
        std::shared_ptr<const AnalyticPair> shape = value->target_analytic;
        bool contradicted                         = false;
        auto reference                            = internal_reference();
        // Every hypothesis this block selects comes from a name, an alias, a
        // legacy selector or the identity bridge, so a measured query skips it
        // whole and reaches the measuring passes below with nothing assumed.
        if (!measured_only && identity.empty() && config_) {
            auto candidate = reference->getColorSpace(cs->name.c_str());
            for (const auto& alias : cs->aliases) {
                if (candidate)
                    break;
                candidate = reference->getColorSpace(alias.c_str());
            }
            // Legacy native aliases can select a candidate, never prove it.
            if (!candidate) {
                for (const auto& entry : color_interop_ids) {
                    if ((find(entry.interop_id) == cs
                         || (entry.legacy_alias
                             && find(entry.legacy_alias) == cs))) {
                        candidate = reference_space(entry.interop_id);
                        if (candidate)
                            break;
                    }
                }
            }
            if (candidate) {
                for (int i = 0; i < 5; ++i) {
                    if (string_view(candidate->getName())
                            == builtin_identities[i]
                        && bridge(i, &lookup_ok) == cs->name) {
                        identity        = builtin_identities[i];
                        bridge_identity = true;
                        break;
                    }
                }
            }
            if (identity.empty() && candidate
                && property_identity_definition(candidate->getName())
                && same_reference_definition(
                    config_, config_->getColorSpace(cs->name.c_str()),
                    candidate)) {
                // Let OCIO verify interchange/reference connections as well
                // as the candidate transform; serialization alone is unsafe.
                auto analysis = copy_config(config_);
                // This space's transforms are the built-in definition's, which
                // no OCIO version's search passes over, so the narrowed search
                // certainly accepting it first is OCIO's own answer.
                const char* matched
                    = narrow_identification(analysis, config_,
                                            candidate->getName());
                try {
                    if (!matched || find(matched) != cs)
                        matched = OCIO::Config::IdentifyBuiltinColorSpace(
                            analysis, copy_config(reference),
                            candidate->getName());
                } catch (const OCIO::Exception& e) {
                    const std::string miss
                        = std::string(
                              "Heuristics were not able to find an equivalent to the requested color space: ")
                          + candidate->getName() + ".";
                    if (miss != e.what())
                        throw;
                }
                if (matched && find(matched) == cs)
                    identity = candidate->getName();
            }
            // Nothing above established anything. Record which identity the
            // configuration's own naming points at, so a definition this build
            // cannot measure still reports what its author declared -- and so
            // a declaration measurement contradicts can be withdrawn rather
            // than published. Table order sets the preference.
            if (identity.empty()) {
                for (const auto& entry : color_interop_ids) {
                    if (find(entry.interop_id) == cs
                        || (entry.legacy_alias
                            && find(entry.legacy_alias) == cs)) {
                        identity        = entry.interop_id;
                        legacy_identity = true;
                        break;
                    }
                }
            }
        }
        if ((identity.empty() || legacy_identity
             || (bridge_identity && analytic_candidate(identity)))
            && config_ && analytic_enabled) {
            bool publishable = false;
            bool evaluated   = false;
            float gamma      = 0.0f;
            auto measured_identity
                = recognize_analytic(config_, reference, cs->name.c_str(),
                                     !m_interop_cache_id.empty(), publishable,
                                     evaluated, value->chromaticities,
                                     value->has_chromaticities, gamma, shape);
            value->target_analytic = shape;
            // `evaluated` belongs to this analytic pass, unlike the aggregate
            // identity lifecycle flag below. A fallback gamut may be recovered
            // even when the whole analytic pair could not be evaluated.
            if (evaluated) {
                value->chromaticities_computed = true;
                value->chromaticities_derived  = true;
            } else if (value->has_chromaticities) {
                value->chromaticities_computed = true;
                value->chromaticities_derived  = true;
            }
            // A completed strict miss overrides a loose native bridge hint;
            // unsupported shapes keep the bridge's answer. A naming hint is not withdrawn here, only replaced: the same
            // measurement that fails to name an encoding usually cannot rule
            // one out either, and the transfer check below decides that.
            if (!measured_identity.empty()) {
                identity        = std::move(measured_identity);
                legacy_identity = false;
            } else if (evaluated && !legacy_identity) {
                identity.clear();
            }
            value->analytic_evaluated = evaluated;
            publish                   = publishable;
            // The measured exponent is recorded whatever the identity turned
            // out to be. It is a fact about the definition, it is what the
            // check at the end of this function tests a candidate identity
            // against, and for an identity the authored-definition reader
            // cannot describe it is the only transfer fact available.
            if (encoding_gamma > 0.0f && gamma > 0.0f
                && gamma != value->gamma) {
                value->gamma            = gamma;
                value->transfer_derived = true;
            } else if (value->gamma == 0.0f)
                value->gamma = gamma;
            else if ((identity.empty() || legacy_identity) && gamma != 0.0f
                     && gamma != value->gamma)
                value->has_chromaticities = false;
            if (gamma > 0.0f && !value->transfer_computed) {
                value->transfer_computed = true;
                value->transfer_derived  = true;
            }
        }
        if ((identity.empty() || legacy_identity) && config_
            && analytic_enabled) {
            bool attempted         = false;
            bool admitted          = false;
            bool evaluated         = false;
            auto measured_identity = recognize_reference(
                config_, reference, cs->name.c_str(),
                !m_interop_cache_id.empty(), shape.get(),
                value->target_probe_response,
                legacy_identity ? string_view(identity) : string_view(),
                attempted, admitted, evaluated, contradicted);
            if (!measured_identity.empty()) {
                identity        = std::move(measured_identity);
                legacy_identity = false;
            } else if (evaluated && !legacy_identity) {
                identity.clear();
            }
            value->analytic_evaluated |= evaluated;
            if (attempted)
                publish = evaluated && admitted;
        }
        // Last: the curve and the primaries measured separately. It
        // reaches encodings the comparisons above cannot, because it does not
        // require the configuration to have composed them the same way.
        if ((identity.empty() || legacy_identity) && config_ && analytic_enabled
            && value->has_chromaticities) {
            auto target      = config_->getColorSpace(cs->name.c_str());
            const auto state = target ? target->getReferenceSpaceType()
                                      : OCIO::REFERENCE_SPACE_SCENE;
            const char* role = state == OCIO::REFERENCE_SPACE_SCENE
                                   ? "aces_interchange"
                                   : "cie_xyz_d65_interchange";
            if (target && config_->hasRole(role)) {
                auto measured_identity = recognize_transfer_and_gamut(
                    config_, reference, cs->name.c_str(), role, state,
                    value->chromaticities);
                if (!measured_identity.empty()) {
                    identity        = std::move(measured_identity);
                    legacy_identity = false;
                }
            }
        }
        // A naming hint that survived measurement must still agree with what
        // was measured. Disagreement withdraws it: an alias naming one
        // encoding on a definition that demonstrably implements another is a
        // stale name, and reporting it would publish a contradiction. The
        // gamma half of the same test runs below, against the same evidence.
        //
        // Three disagreements withdraw it. A measured gamut that is not the
        // identity's; a sampled comparison that tested this very identity --
        // same image state, a reference definition this build could measure --
        // and did not reproduce it; and a reference state the identity names
        // that is not the state this space is in.
        //
        // The second is what answers a configuration whose display interchange
        // role is not the CIE XYZ it declares: nothing about such a space can
        // be named, and the reference's own primaries must not be published on
        // its behalf because a name survived unexamined.
        //
        // The third is not a measurement, and it does not need one. The state
        // a candidate identity names is binding, and a candidate in the other
        // state is never compared against this space by any pass above -- so a
        // scene-referred definition carrying a legacy alias that names a
        // display encoding would otherwise keep that identity precisely
        // because nothing could contradict it. That the mathematics matches a
        // display encoding says what the curve and the primaries are; it does
        // not say that a display rendering has been applied, which is what the
        // identity's own suffix claims. The facts measurement did establish
        // survive the withdrawal, here as in the other two cases.
        if (legacy_identity && analytic_enabled) {
            bool withdraw = contradicted;
            if (!withdraw && config_) {
                auto target = config_->getColorSpace(cs->name.c_str());
                auto named  = reference_space(identity.c_str());
                withdraw    = target && named
                              && named->getReferenceSpaceType()
                                     != target->getReferenceSpaceType();
            }
            if (!withdraw && value->has_chromaticities) {
                std::array<float, 8> declared {};
                bool declared_has_xy = false;
                float declared_gamma = 0.0f;
                if (!reference_properties(reference, identity, declared,
                                          declared_has_xy, declared_gamma))
                    declared_has_xy = identity_chromaticities(reference,
                                                              identity,
                                                              declared);
                withdraw = declared_has_xy && declared != value->chromaticities;
            }
            if (withdraw) {
                DBG("Color naming hint withdrawn: {}\n", cs->name);
                identity.clear();
                legacy_identity = false;
            }
        }
        value->identity           = identity;
        value->identity_evaluated = !identity.empty();
        // An identity established by declaration reports the same gamut as one
        // established by measurement. The authored-definition reader below
        // describes only the unadapted definitions it lists, so the reference's
        // own definitions are measured for the rest, ACEScg among them, and
        // reach the same forward construction and reconstruction the space
        // under test did. Both sides are therefore described the same way.
        if (!identity.empty() && !value->has_chromaticities && analytic_enabled
            && property_display_definition(identity).empty()) {
            value->has_chromaticities
                = identity_chromaticities(reference, identity,
                                          value->chromaticities);
            if (value->has_chromaticities) {
                value->chromaticities_computed = true;
                value->chromaticities_derived  = true;
            }
        }
        if (identity == "lin_ap1_scene") {
            // Linear response is established by the identity itself; the
            // authored ACEScg definition is not one of the unadapted display
            // matrices the reader below can describe.
            const bool replaces_direct = value->transfer_computed
                                         && value->gamma != 1.0f;
            value->gamma               = 1.0f;
            value->transfer_derived |= !value->transfer_computed
                                       || replaces_direct;
            value->transfer_computed = true;
        } else if (!identity.empty()) {
            float gamma = 0.0f;
            // Read into a copy: a conflict withdraws the identity, and what
            // was measured about the definition has to survive that. Reporting
            // the encoding is one question and reporting its primaries and its
            // exponent is another, and the second is still answered.
            std::array<float, 8> declared = value->chromaticities;
            bool declared_has_xy          = value->has_chromaticities;
            const bool supported = reference_properties(reference, identity,
                                                        declared,
                                                        declared_has_xy, gamma);
            if (!supported && analytic_enabled)
                gamma = identity_gamma(reference, identity);
            // Native declared linear encoding must not be overwritten by a
            // conflicting reference transfer declaration.
            const float native = encoding_gamma > 0.0f ? encoding_gamma
                                                       : value->gamma;
            if (supported && native != 0.0f && gamma != native) {
                value->identity.clear();
            } else {
                if (value->gamma == 0.0f)
                    value->gamma = gamma;
                if (gamma > 0.0f && !value->transfer_computed) {
                    value->transfer_computed = true;
                    value->transfer_derived  = true;
                }
                // The candidate's own declaration fills what measurement could
                // not establish; it does not replace what measurement did.
                // An identity is accepted within a stated response tolerance,
                // which is not a claim that the definition reproduces the
                // reference's coefficients, so the measured primaries stay the
                // ones reported wherever they exist.
                if (!value->has_chromaticities) {
                    value->chromaticities     = declared;
                    value->has_chromaticities = declared_has_xy;
                    if (declared_has_xy) {
                        value->chromaticities_computed = true;
                        value->chromaticities_derived  = true;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        DBG("Color properties incomplete: {}\n", e.what());
        // A later identity or transfer acquisition may fail after the gamut
        // producer wrote a complete partial fact. Preserve its own lifecycle;
        // the unrelated failure still prevents publication of the record.
        if (value->has_chromaticities) {
            value->chromaticities_computed = true;
            value->chromaticities_derived  = true;
        }
        value->incomplete = true;
        return result;  // Transient failures are never completed misses.
    }
    if (value->color_interop_id.empty() && !value->identity.empty()) {
        value->color_interop_id = value->identity;
        value->interop_derived  = true;
    }
    // Identity may be established after the initial authored-facts snapshot.
    // Complete the dependent semantic fact here so every caller sees the
    // state named by that identity in the same result, without requiring a
    // later equality enrichment or a warm cache entry.
    if (value->image_state.empty() && !value->color_interop_id.empty()) {
        value->image_state = std::string(
            known_image_state(value->color_interop_id, value->encoding));
        value->image_state_derived = !value->image_state.empty()
                                     && value->interop_derived;
    }
    if (shared && publish && lookup_ok) {
        spin_rw_write_lock lock(properties_mutex);
        auto published = properties_memo.emplace(key, result).first;
        // Later existing-bridge evidence may upgrade an earlier unsupported
        // result. Replacing the owning handle leaves earlier snapshots intact.
        if ((!published->second.m_impl->analytic_evaluated
             && value->analytic_evaluated)
            || (!published->second.m_impl->identity_evaluated
                && value->identity_evaluated
                && !(published->second.m_impl->analytic_evaluated
                     && bridge_identity && analytic_candidate(value->identity))))
            published->second = result;
        result = published->second;
        DBG("Color properties retained entries: {}\n", properties_memo.size());
    } else {
        // Which gate refused, so a declined publication is a read rather than
        // an inference: `key` when no cache key describes this configuration,
        // `admission` when a pass would not stand behind the conclusion, and
        // `lookup` when an identity lookup itself did not complete. A native
        // acquisition failure never reaches here; it returns above.
        DBG("Color properties declined publication: {} ({})\n", cs->name,
            !lookup_ok ? "lookup" : (!shared ? "key" : "admission"));
    }
    return enrich ? enriched(*cs, result, key, shared && publish && lookup_ok)
                  : result;
}


ColorSpaceInfo
ColorConfig::Impl::builtin_identity_info(string_view colorspace) const
{
    const std::string identity = builtin_identity(colorspace);
    auto cs = identity.empty() || has_named_transform(colorspace)
                  ? nullptr
                  : reference_space(identity.c_str());
    if (!cs)
        return {};
    ColorSpaceInfo result;
    auto value              = std::make_shared<ColorSpaceInfo::Impl>();
    result.m_impl           = value;
    value->identity         = identity;
    value->color_interop_id = identity;
    value->encoding         = cs->getEncoding();
    value->image_state      = std::string(
        known_image_state(identity, value->encoding));
    try {
        identity_properties(identity, value->chromaticities,
                            value->has_chromaticities, value->gamma);
    } catch (const std::exception& e) {
        DBG("Built-in identity properties incomplete: {}\n", e.what());
        value->incomplete = true;
    }
    value->identity_evaluated      = true;
    value->interop_computed        = true;
    value->interop_derived         = true;
    value->encoding_computed       = true;
    value->image_state_computed    = !value->image_state.empty();
    value->chromaticities_computed = value->has_chromaticities;
    value->chromaticities_derived  = value->has_chromaticities;
    value->transfer_computed       = value->gamma > 0.0f;
    value->transfer_derived        = value->gamma > 0.0f;
    return result;
}


ColorSpaceInfo
ColorConfig::get_color_space_info(string_view colorspace) const
{
    return getImpl()->color_space_info(colorspace, false);
}

ColorSpaceInfo
ColorConfig::derive_color_space_info(string_view colorspace) const
{
    return getImpl()->color_space_info(colorspace, true, false, true);
}

ColorSpaceInfo
ColorConfig::Impl::context_color_space_info(string_view colorspace, bool derive,
                                            bool enrich) const
{
    // Only a spelling this view does not define is expanded through the view's
    // context, where a variable can name a local space.
    if (config_ && !disable_ocio && !find(colorspace)
        && colorspace.find_first_of("$%") != string_view::npos) {
        try {
            const std::string expanded
                = config_->getCurrentContext()->resolveStringVar(
                    std::string(colorspace).c_str());
            return color_space_info(expanded, derive, false, enrich);
        } catch (const std::exception& e) {
            DBG("Color property context expansion unavailable for '{}': {}\n",
                colorspace, e.what());
            return {};
        }
    }
    return color_space_info(colorspace, derive, false, enrich);
}

ColorSpaceInfo
ColorConfigAccess::color_space_info(const ColorConfig& config,
                                    string_view colorspace, bool derive,
                                    string_view context_key,
                                    string_view context_value)
{
    if (context_key.empty() && context_value.empty())
        return derive ? config.derive_color_space_info(colorspace)
                      : config.get_color_space_info(colorspace);
    // A view this build could not acquire is not a verdict, and answering
    // from the configuration's own context instead would let default-context
    // evidence stand in for the caller's.
    const auto view = config.getImpl()->context_view(context_key, context_value,
                                                     colorspace);
    return view ? view->context_color_space_info(colorspace, derive, derive)
                : ColorSpaceInfo();
}

namespace {


// ---------------------------------------------------------------------------
// Vocabulary shared with the built-in config
// ---------------------------------------------------------------------------

// The family key of a reference curve name: `crv_g24_tx` and `crv_g24` are
// both `g24`. The pass-through and mirrored variants describe the same
// positive-axis curve, so a family is what a cross-state comparison can mean.
std::string
curve_family(string_view catalog_name)
{
    std::string family = Strutil::lower(catalog_name);
    if (Strutil::starts_with(family, "crv_"))
        family = family.substr(4);
    for (string_view suffix : { "_scene", "_display", "_tx" })
        if (family.size() > suffix.size()
            && Strutil::ends_with(family, suffix)) {
            family.resize(family.size() - suffix.size());
            break;
        }
    return family;
}


// ---------------------------------------------------------------------------
// Curve measurement
// ---------------------------------------------------------------------------

// A NamedTransform is probed in the encoding direction, which is the direction
// every other measurement here is stated in: `transform` on a `crv_` entry
// decodes to linear, so its inverse is the encode, and a color space is probed
// from the interchange role toward the space. One direction, three sources.
TransferSignature
named_transform_signature(OCIO::ConstConfigRcPtr config, const char* name)
{
    try {
        auto nt = config->getNamedTransform(name);
        if (!nt)
            return {};
        return transfer_signature(
            config->getProcessor(config->getCurrentContext(), nt,
                                 OCIO::TRANSFORM_DIR_INVERSE));
    } catch (const std::exception&) {
        return {};
    }
}

// One reference curve, measured once for the process and retained beside the
// other reference passes under its own key variant.
//
// Only a valid signature is retained. An empty one is not a property of the
// fixed built-in config -- named_transform_signature answers a definition it
// cannot describe and a resource it could not reach this once identically -- so
// it is returned unretained and the next call measures again. A curve that
// stays unreadable is skipped by the callers below, which is not a claim that
// the families they did reach are the only ones that could have matched.
std::shared_ptr<const TransferSignature>
reference_curve_signature(const std::string& name)
{
    const std::string key = std::string(reference_revision)
                            + ":curve-v1:" + name;
    {
        spin_rw_read_lock lock(analytic_reference_mutex);
        auto found = transfer_references.find(key);
        if (found != transfer_references.end())
            return found->second;
    }
    auto measured = std::make_shared<const TransferSignature>(
        named_transform_signature(internal_reference(), name.c_str()));
    if (!measured->valid())
        return measured;
    spin_rw_write_lock lock(analytic_reference_mutex);
    return transfer_references.emplace(key, measured).first->second;
}

struct ReferenceCurve {
    std::string name;
    std::string family;
    std::string encoding;
};

// The published curve vocabulary, read off the built-in config's own
// NamedTransforms rather than restated as a table here. Nothing is measured
// by building this list; the signatures are measured on demand.
//
// Nothing is caught here on purpose. A vocabulary this build could not read is
// a failed acquisition, not an empty or partial vocabulary, and a list that was
// never finished would make every family comparison below answer from it
// forever. A function-local static whose initializer throws stays
// uninitialized, so the exception reaches the caller that asked -- which
// retains nothing -- and the next call reads the built-in config again.
const std::vector<ReferenceCurve>&
reference_curves()
{
    static const std::vector<ReferenceCurve> curves = [] {
        std::vector<ReferenceCurve> result;
        auto reference  = internal_reference();
        const int count = reference->getNumNamedTransforms();
        for (int i = 0; i < count; ++i) {
            const char* name = reference->getNamedTransformNameByIndex(i);
            if (!name || !Strutil::starts_with(Strutil::lower(name), "crv_"))
                continue;
            auto nt = reference->getNamedTransform(name);
            result.push_back(
                { name, curve_family(name),
                  nt && nt->getEncoding() ? nt->getEncoding() : "" });
        }
        DBG("Reference curve vocabulary: {} curves\n", result.size());
        return result;
    }();
    return curves;
}

// The published family this measurement agrees with, or empty. Empty is not a
// verdict of unlike: it means no published curve describes what was measured,
// which is the ordinary case for a bespoke exponent, and such a curve is still
// found by the signature comparison beside this one.
std::string
measured_family(const TransferSignature& signature, string_view encoding)
{
    if (!signature.valid())
        return {};
    for (const auto& curve : reference_curves()) {
        auto expected = reference_curve_signature(curve.name);
        if (!expected || !expected->valid())
            continue;
        if (same_transfer_signature(signature, *expected,
                                    encoding.empty()
                                        ? string_view(curve.encoding)
                                        : encoding))
            return curve.family;
    }
    return {};
}


// ---------------------------------------------------------------------------
// What to call an operation in a refusal.
const char*
native_transform_kind(OCIO::ConstTransformRcPtr t)
{
    switch (t->getTransformType()) {
    case OCIO::TRANSFORM_TYPE_MATRIX: return "a matrix";
    case OCIO::TRANSFORM_TYPE_RANGE: return "a range";
    case OCIO::TRANSFORM_TYPE_EXPONENT: return "an exponent";
    case OCIO::TRANSFORM_TYPE_EXPONENT_WITH_LINEAR:
        return "an exponent with a linear segment";
    case OCIO::TRANSFORM_TYPE_LOG:
    case OCIO::TRANSFORM_TYPE_LOG_AFFINE:
    case OCIO::TRANSFORM_TYPE_LOG_CAMERA: return "a log";
    case OCIO::TRANSFORM_TYPE_LUT1D: return "a one-dimensional look-up table";
    case OCIO::TRANSFORM_TYPE_LUT3D: return "a three-dimensional look-up table";
    case OCIO::TRANSFORM_TYPE_CDL: return "a color decision list";
    case OCIO::TRANSFORM_TYPE_FIXED_FUNCTION: return "a fixed function";
    case OCIO::TRANSFORM_TYPE_ALLOCATION: return "an allocation";
    case OCIO::TRANSFORM_TYPE_EXPOSURE_CONTRAST:
        return "an exposure and contrast adjustment";
    case OCIO::TRANSFORM_TYPE_GRADING_PRIMARY: return "a grading primary";
    case OCIO::TRANSFORM_TYPE_GRADING_RGB_CURVE: return "a grading curve";
    case OCIO::TRANSFORM_TYPE_GRADING_TONE: return "a grading tone";
    default: return "an operation";
    }
}


// How far the excluded matrix may carry the interchange neutral off the
// space's own white before the separation stops being proven. It is far
// tighter than the recognition tolerance beside it, and deliberately so: this
// is not a recognition. A primaries matrix authored at any reasonable
// precision lands on the space's white to within a rounding of its own
// coefficients, while the gentlest headroom scale anyone writes -- the 0.9166
// a cinema white carries -- misses it by more than eight hundred times this.
static const double transfer_export_neutral_tolerance = 1e-4;

// An invalid native topology is a stable semantic refusal. Keeping that
// distinct from exceptions raised while OCIO expands or acquires a processor
// lets property derivation retain only the former as Unrecognized.
class NativeTransferRefusal final : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};


// The operations that are the transfer function, taken out of the conversion
// OpenColorIO built from the interchange role toward the space.
//
// A transfer function is a function of the space's own linear RGB, so the
// channel-mixing matrix carrying the interchange primaries into that RGB is
// not part of it and is left out. Everything else the conversion does is part
// of it, and that is deliberately more than a curve reader admits: a uniform
// diagonal gain or offset is how a headroom scale is spelled, a range is how a
// clamp is spelled, and OpenColorIO spells an unclamped range as a matrix.
// Dropping any of the three would yield a curve the configuration does not
// implement.
//
// Separation is proven rather than assumed. Every retained operation must bend
// the three color channels alike and leave alpha where it was -- the contract
// `separable_transform` already states, which this reads and does not widen.
// A mixing matrix may stand only before the curve begins, and it may be
// excluded only once it is shown to carry the interchange reference neutral
// onto the space's own white, an unmixed (1, 1, 1). A leading matrix that does
// anything else is carrying a gain, an offset or an inset working gamut that
// excluding it would silently discard, and nothing in the matrix says which,
// so it is refused. Nothing here factors, redistributes or invents an
// operation.
OCIO::GroupTransformRcPtr
native_transfer_transform(OCIO::ConstProcessorRcPtr processor, bool display)
{
    auto built                   = processor->createGroupTransform();
    auto retained                = OCIO::GroupTransform::Create();
    std::array<double, 9> prefix = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    bool prefix_seen             = false;
    for (int i = 0; i < built->getNumTransforms(); ++i) {
        OCIO::TransformRcPtr op      = built->getTransform(i);
        OCIO::ConstTransformRcPtr it = op;
        auto matrix = OCIO::DynamicPtrCast<const OCIO::MatrixTransform>(it);
        if (!matrix) {
            if (!separable_transform(it))
                throw NativeTransferRefusal(Strutil::fmt::format(
                    "the conversion applies {}, which is not one per-channel "
                    "curve the three color channels share",
                    native_transform_kind(it)));
            retained->appendTransform(op);
            continue;
        }
        double m[16], offset[4];
        matrix->getMatrix(m);
        matrix->getOffset(offset);
        // Alpha is never part of a transfer function. A matrix that couples it
        // to color or scales it is refused wherever it stands: excluding it
        // would discard the effect, and retaining it would yield something
        // that is not a curve.
        bool separable = offset[3] == 0.0 && m[15] == 1.0;
        for (int j = 0; j < 3; ++j)
            separable &= m[4 * j + 3] == 0.0 && m[12 + j] == 0.0;
        for (int j = 0; j < 16; ++j)
            separable &= std::isfinite(m[j]);
        for (int j = 0; j < 4; ++j)
            separable &= std::isfinite(offset[j]);
        if (!separable)
            throw NativeTransferRefusal(
                "the conversion carries a matrix that changes alpha or is not "
                "finite, which no transfer function does");
        bool diagonal = true, unmoved = true;
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c) {
                const double v = m[4 * r + c];
                diagonal &= r == c || v == 0.0;
                unmoved &= v == (r == c ? 1.0 : 0.0);
            }
        const bool shifts = offset[0] != 0.0 || offset[1] != 0.0
                            || offset[2] != 0.0;
        if (unmoved && !shifts)
            continue;  // Moves nothing, so it belongs to neither part.
        if (diagonal) {
            // A per-channel matrix is a gain and an offset, which is curve.
            // The symmetric contract governs it as it governs every other
            // per-channel operation retained here.
            if (m[0] != m[5] || m[0] != m[10] || offset[0] != offset[1]
                || offset[0] != offset[2])
                throw NativeTransferRefusal(
                    "the conversion scales or shifts the three color channels "
                    "differently, so no one curve describes it");
            retained->appendTransform(op);
            continue;
        }
        if (retained->getNumTransforms())
            throw NativeTransferRefusal(
                "the conversion mixes the color channels after its curve has "
                "begun, so no curve of the space's own linear RGB separates "
                "from it");
        if (shifts)
            throw NativeTransferRefusal(
                "the conversion's primaries matrix carries an offset, which "
                "excluding that matrix would discard");
        bool composed_identity = false;
        if (!compose_linear_matrix(matrix, prefix, composed_identity))
            throw NativeTransferRefusal(
                "the conversion's primaries matrix is singular or couples "
                "alpha, so no transfer function separates from it");
        prefix_seen = true;
    }
    // Nothing was excluded, so there is nothing to prove: the space's own
    // linear RGB is the interchange's, and everything the conversion does is
    // the curve.
    if (!prefix_seen)
        return retained;
    const auto& white = display ? interop_probes_display[5]
                                : interop_probes_scene[5];
    for (int row = 0; row < 3; ++row) {
        double v = 0.0;
        for (int k = 0; k < 3; ++k)
            v += prefix[3 * row + k] * double(white[k]);
        if (!std::isfinite(v)
            || std::abs(v - 1.0) > transfer_export_neutral_tolerance)
            throw NativeTransferRefusal(
                "the matrix this conversion begins with does not carry the "
                "interchange neutral onto the space's own white, so it is "
                "carrying a gain or an inset gamut as well as the primaries "
                "and no transfer function separates from it");
    }
    return retained;
}



// A curve measured from the interchange role toward the space, which is the
// encoding direction every comparison here is stated in.
TransferSignature
measured_transfer(OCIO::ConstConfigRcPtr config, OCIO::ConstColorSpaceRcPtr cs)
{
    if (!config || !cs || cs->isData())
        return {};
    const char* role = cs->getReferenceSpaceType()
                               == OCIO::REFERENCE_SPACE_SCENE
                           ? "aces_interchange"
                           : "cie_xyz_d65_interchange";
    if (!config->hasRole(role))
        return {};
    return transfer_signature(config, role, cs->getName());
}

}  // namespace



// Whether a sampled curve measurement of this definition means anything: the
// sampled pass's structural walk, plus, for file-backed definitions, the
// expanded-operation gate (a 3D table's neutral axis is not a curve).
bool
ColorConfig::Impl::structurally_admitted(OCIO::ConstColorSpaceRcPtr target) const
{
    if (!m_catalog || !config_ || disable_ocio || !target)
        return false;
    const auto refstate = target->getReferenceSpaceType();
    const char* role    = refstate == OCIO::REFERENCE_SPACE_SCENE
                              ? "aces_interchange"
                              : "cie_xyz_d65_interchange";
    if (!config_->hasRole(role))
        return false;
    TransformClosure closure(config_, refstate);
    if (!closure.space(target) || !closure.space(config_->getColorSpace(role)))
        return false;
    if (closure.resource_free())
        return true;
    try {
        return measurable_expanded_transform(
            config_
                ->getProcessor(config_->getCurrentContext(), role,
                               target->getName())
                ->createGroupTransform());
    } catch (const std::exception&) {
        // A resource this build could not reach now is not a verdict about the
        // definition. Nothing is retained under the key that says so, so the
        // next derivation asks again.
        return false;
    }
}



// One target's curve, measured once and retained process-wide under the full
// configuration/context/resource key. Without that key, or when the
// measurement fails, nothing is retained.
TransferSignature
ColorConfig::Impl::retained_transfer(const std::string& name,
                                     OCIO::ConstColorSpaceRcPtr target) const
{
    std::string optimization;
    (void)analytic_optimization(optimization);
    if (!m_interop_cache_safe || m_interop_cache_id.empty())
        return measured_transfer(config_, target);
    const auto key = std::make_pair(properties_cache_id(m_interop_cache_id,
                                                        optimization,
                                                        "transfer-v1"),
                                    name);
    {
        spin_rw_read_lock lock(properties_mutex);
        auto found = properties_memo.find(key);
        if (found != properties_memo.end()) {
            DBG("Color transfer shared hit: {}\n", name);
            return found->second.m_impl->transfer
                       ? *found->second.m_impl->transfer
                       : TransferSignature();
        }
    }
    auto signature = measured_transfer(config_, target);
    if (!signature.valid())
        return signature;
    ColorSpaceInfo record;
    auto value      = std::make_shared<ColorSpaceInfo::Impl>();
    value->transfer = std::make_shared<const TransferSignature>(signature);
    record.m_impl   = value;
    spin_rw_write_lock lock(properties_mutex);
    properties_memo.emplace(key, record);
    return signature;
}



// Add the public derived record's declaration-independent equality evidence,
// then answer the one thing an exponent cannot: which published curve family
// the definition's own measurement agrees with, or that it agrees with none.
//
// Transfer classification uses the bounded structural admission and the
// retained measurement under the admitted key, so repeated derivations measure
// a definition once.
//
// Nothing here is a verdict. A definition the admission declines and a resource
// this build could not reach both leave the snapshot exactly as it was, so the
// next explicit derivation asks again rather than reading back a permanent
// negative -- and the snapshot the caller already holds is never touched: a
// strengthened record is a new one that replaces the published handle.
ColorSpaceInfo
ColorConfig::Impl::enriched(const CSInfo& cs, const ColorSpaceInfo& base,
                            const std::pair<std::string, std::string>& key,
                            bool publish) const
{
    if (!base.m_impl)
        return base;
    ColorSpaceInfo current = base;
    if (!current.m_impl->equality_computed) {
        try {
            bool equality_ok = true;
            auto value       = std::make_shared<ColorSpaceInfo::Impl>(
                *current.m_impl);
            value->equality_id = std::string(
                get_color_equality_id(cs.name, &equality_ok));
            if (!equality_ok)
                return base;
            value->equality_computed = true;
            value->equality_derived  = !value->equality_id.empty();
            if (value->encoding.empty() && !value->equality_id.empty()) {
                auto twin = internal_reference()->getColorSpace(
                    value->equality_id.c_str());
                if (twin && twin->getEncoding() && twin->getEncoding()[0]) {
                    value->encoding         = twin->getEncoding();
                    value->encoding_derived = true;
                }
            }
            if (value->image_state.empty()) {
                value->image_state = std::string(
                    known_image_state(value->equality_id, value->encoding));
                value->image_state_derived = !value->image_state.empty();
            }
            current.m_impl = value;
            if (publish) {
                spin_rw_write_lock lock(properties_mutex);
                auto found = properties_memo.find(key);
                if (found != properties_memo.end()
                    && found->second.m_impl == base.m_impl)
                    found->second = current;
            }
        } catch (const std::exception& e) {
            DBG("Color property enrichment incomplete: {}\n", e.what());
            return base;
        }
    }
    if (!m_catalog || !config_ || disable_ocio)
        return current;
    // An exponent has already answered, or a previous derivation classified
    // this record. Either way there is nothing left to measure.
    if (ColorSpaceInfoAccess::transfer_function_kind(current)
            != ColorTransferFunctionKind::Undetermined
        || current.m_impl->incomplete || (cs.flags() & CSInfo::is_data))
        return current;
    OCIO::ConstColorSpaceRcPtr target;
    try {
        target = config_->getColorSpace(cs.name.c_str());
        if (!target || target->isData() || !structurally_admitted(target))
            return current;
        const auto signature = retained_transfer(cs.name, target);
        if (!signature.valid())
            return current;
        auto value = std::make_shared<ColorSpaceInfo::Impl>(*current.m_impl);
        // The space states how much its own curve varies across its
        // profile, which is what the comparison tolerance is for; where it
        // states nothing, each published curve's own encoding stands in.
        const std::string encoding = Strutil::lower(cs.encoding);
        value->transfer_name       = measured_family(signature, encoding);
        value->transfer_kind       = ColorTransferFunctionKind::Named;
        value->transfer_computed   = true;
        value->transfer_derived    = true;
        if (value->transfer_name.empty()) {
            const bool display = target->getReferenceSpaceType()
                                 == OCIO::REFERENCE_SPACE_DISPLAY;
            const char* role   = display ? "cie_xyz_d65_interchange"
                                         : "aces_interchange";
            // Acquisition remains in the outer catch: a missing resource or
            // other transient failure publishes no verdict and is retried.
            auto processor = config_->getProcessor(config_->getCurrentContext(),
                                                   role, target->getName());
            try {
                (void)native_transfer_transform(processor, display);
                value->transfer_kind = ColorTransferFunctionKind::Transform;
            } catch (const NativeTransferRefusal&) {
                // Only the extractor's semantic refusal establishes that the
                // measured definition has no exact separable representation.
                value->transfer_kind = ColorTransferFunctionKind::Unrecognized;
            }
        }
        ColorSpaceInfo result;
        result.m_impl = value;
        if (publish) {
            spin_rw_write_lock lock(properties_mutex);
            auto found = properties_memo.find(key);
            // Only the record this was derived from is replaced. Anything else
            // under the key was published by another thread, which measured the
            // same definition and has its own right to stand.
            if (found != properties_memo.end()
                && found->second.m_impl == current.m_impl)
                found->second = result;
        }
        DBG("Color transfer family classified: {} ({})\n", cs.name,
            value->transfer_name.empty()
                ? (value->transfer_kind == ColorTransferFunctionKind::Transform
                       ? "transform"
                       : "unrecognized")
                : value->transfer_name);
        return result;
    } catch (const std::exception& e) {
        DBG("Color transfer classification unavailable: {}\n", e.what());
        return current;  // Retry interrupted acquisition on the next derivation.
    }
}



string_view
ColorConfig::get_color_interop_id(const int cicp[4]) const
{
    return v3_1::get_color_interop_id(cspan<int>(cicp, 4));
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
    if (from.empty() || from == "current") {
        from = src.spec().get_string_attribute("oiio:Colorspace",
                                               "scene_linear");
    }
    if (from.empty() || from == "unknown" || to.empty() || to == "unknown") {
        dst.errorfmt("Unknown color space name (from=\"{}\", to=\"{}\")", from,
                     to);
        return false;
    }

    if (!colorconfig)
        colorconfig = &ColorConfig::default_colorconfig();

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

    logtime.stop(-1);  // transition to other colorconvert
    bool ok = colorconvert(dst, src, processor.get(), unpremult, roi, nthreads);
    if (ok) {
        // Coming from a non-color space preserves the original space
        // DBG("done, setting output colorspace to {}\n", to);
        if (colorconfig->isData(from))
            to = from;
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
    if (from.empty() || from == "current") {
        auto linearspace = colorconfig->resolve("scene_linear");
        from = src.spec().get_string_attribute("oiio:Colorspace", linearspace);
    }
    if (to.empty() || to == "current") {
        auto linearspace = colorconfig->resolve("scene_linear");
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
    OIIO::pvt::LoggedTimer logtime("IBA::ociodisplay");
    ColorProcessorHandle processor;
    {
        if (!colorconfig)
            colorconfig = &ColorConfig::default_colorconfig();
        if (from.empty() || from == "current") {
            auto linearspace = colorconfig->resolve("scene_linear");
            from = src.spec().get_string_attribute("oiio:ColorSpace",
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
    if (ok) {
        if (inverse)
            dst.specmod().set_colorspace(colorconfig->resolve(from));
        else {
            // Tag with the display and view the processor used.
            const auto selected = ColorConfigAccess::select_display_view(
                *colorconfig, ustring(display), ustring(view),
                ustring(colorconfig->resolve(from)), key, value);
            dst.specmod().set_colorspace(
                colorconfig->getDisplayViewColorSpaceName(
                    selected.display.string(), selected.view.string()));
        }
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
    ColorProcessorHandle processor;
    {
        if (!colorconfig)
            colorconfig = &ColorConfig::default_colorconfig();
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
    if (ok)
        // If we can parse a color space from the file name, and we're not inverting
        // the transform, then we'll use the color space name from the file.
        // Otherwise, we'll leave `oiio:ColorSpace` alone.
        // TODO: Use OCIO to extract InputDescription and OutputDescription CLF
        // metadata attributes, if present.
        if (!colorconfig->filepathOnlyMatchesDefaultRule(name))
            dst.specmod().set_colorspace(
                colorconfig->getColorSpaceFromFilepath(name));
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



namespace {

// Set or clear "oiio:ColorSpace" and clear metadata that might contradict
// it. Only deciding whether to keep "Exif:ColorSpace" can need a color
// config: `config` if given, else the default one, loaded only then.
void
set_colorspace_attribute(ImageSpec& spec, string_view colorspace,
                         const ColorConfig* config)
{
    // If we're not changing color space, don't mess with anything
    string_view oldspace = spec.get_string_attribute("oiio:ColorSpace");
    if (oldspace.size() && colorspace.size() && oldspace == colorspace)
        return;

    // Set or clear the main "oiio:ColorSpace" attribute
    if (colorspace.empty()) {
        spec.erase_attribute("oiio:ColorSpace");
    } else {
        spec.attribute("oiio:ColorSpace", colorspace);
    }

    // Clear a bunch of other metadata that might contradict the colorspace,
    // including some format-specific things that we don't want to propagate
    // from input to output if we know that color space transformations have
    // occurred.
    if (!Strutil::iequals(colorspace, "srgb_rec709_scene")
        && spec.find_attribute("Exif:ColorSpace")
        && !(config ? *config : ColorConfig::default_colorconfig())
                .equivalent(colorspace, "srgb_rec709_scene"))
        spec.erase_attribute("Exif:ColorSpace");
    spec.erase_attribute("tiff:ColorSpace");
    spec.erase_attribute("tiff:PhotometricInterpretation");
    spec.erase_attribute("oiio:Gamma");
}



void
set_colorspace_rec709_gamma_attribute(ImageSpec& spec, float gamma,
                                      const ColorConfig* config)
{
    // Round gamma to the nearest hundredth to prevent stupid precision choices
    // and make it easier for apps to make decisions based on known gamma values.
    float g_rounded = std::round(gamma * 100.0f) / 100.0f;
    if (fabsf(g_rounded - 1.0f) <= 0.01f) {
        set_colorspace_attribute(spec, "lin_rec709_scene", config);
    } else if (fabsf(g_rounded - 1.8f) <= 0.01f) {
        set_colorspace_attribute(spec, "g18_rec709_scene", config);
        spec.attribute("oiio:Gamma", 1.8f);
    } else if (fabsf(g_rounded - 2.2f) <= 0.01f) {
        set_colorspace_attribute(spec, "g22_rec709_scene", config);
        spec.attribute("oiio:Gamma", 2.2f);
    } else if (fabsf(g_rounded - 2.4f) <= 0.01f) {
        set_colorspace_attribute(spec, "g24_rec709_scene", config);
        spec.attribute("oiio:Gamma", 2.4f);
    } else {
        set_colorspace_attribute(spec,
                                 Strutil::fmt::format("g{}_rec709_scene",
                                                      std::lround(g_rounded
                                                                  * 10.0f)),
                                 config);
        // Preserve the original gamma value for use in color conversions.
        spec.attribute("oiio:Gamma", gamma);
    }
}

}  // namespace



void
ColorConfig::set_colorspace(ImageSpec& spec, string_view colorspace) const
{
    set_colorspace_attribute(spec, colorspace, this);
}



void
ColorConfig::set_colorspace_rec709_gamma(ImageSpec& spec, float gamma) const
{
    set_colorspace_rec709_gamma_attribute(spec, gamma, this);
}


void
set_colorspace(ImageSpec& spec, string_view colorspace)
{
    set_colorspace_attribute(spec, colorspace, nullptr);
}

void
set_colorspace_rec709_gamma(ImageSpec& spec, float gamma)
{
    set_colorspace_rec709_gamma_attribute(spec, gamma, nullptr);
}

bool
is_colorspace_srgb(const ImageSpec& spec, bool default_to_srgb)
{
    string_view colorspace = spec.get_string_attribute("oiio:ColorSpace");
    if (default_to_srgb && colorspace.empty()) {
        return true;
    }

    const ColorConfig& colorconfig(ColorConfig::default_colorconfig());
    string_view interop_id = colorconfig.get_color_interop_id(colorspace);

    return (interop_id == "srgb_rec709_scene"
            || interop_id == "srgb_rec709_display");
}

std::vector<uint8_t>
get_colorspace_icc_profile(const ImageSpec& spec, bool /*from_colorspace*/)
{
    std::vector<uint8_t> icc_profile;
    const ParamValue* p = spec.find_attribute("ICCProfile");
    if (p) {
        cspan<uint8_t> icc_profile_span = p->as_cspan<uint8_t>();
        icc_profile.assign(icc_profile_span.begin(), icc_profile_span.end());
    }
    return icc_profile;
}

cspan<int>
get_colorspace_cicp(const ImageSpec& spec, bool from_colorspace)
{
    const ParamValue* p = spec.find_attribute("CICP",
                                              TypeDesc(TypeDesc::INT, 4));
    if (p)
        return p->as_cspan<int>();
    if (!from_colorspace)
        return cspan<int>();
    const ColorConfig& colorconfig(ColorConfig::default_colorconfig());
    return colorconfig.get_cicp(spec.get_string_attribute("oiio:ColorSpace"));
}

string_view
get_color_interop_id(cspan<int> cicp)
{
    if (cicp.size() < 2)
        return "";
    // Only primaries and transfer are consulted; matrix and range are the
    // caller's.
    const auto pair = normalized_cicp_pair(cicp[0], cicp[1]);
    if (string_view exact = exact_cicp_input_identity(pair); !exact.empty())
        return exact;
    for (const ColorInteropID& interop : color_interop_ids) {
        if (interop.has_cicp && interop.cicp[0] == pair.primaries
            && interop.cicp[1] == pair.transfer) {
            return interop.interop_id;
        }
    }
    return "";
}

OIIO_NAMESPACE_3_1_END

OIIO_NAMESPACE_BEGIN

ColorSpaceInfo
pvt::color_space_info(const ColorConfig& config, string_view colorspace,
                      bool derive, string_view context_key,
                      string_view context_value)
{
    return v3_1::ColorConfigAccess::color_space_info(config, colorspace, derive,
                                                     context_key,
                                                     context_value);
}

ColorSpaceInfo
pvt::get_colorspace_info(const ImageSpec& spec, bool derive,
                         string_view context_key, string_view context_value)
{
    return pvt::color_space_info(ColorConfig::default_colorconfig(),
                                 spec.get_string_attribute("oiio:ColorSpace"),
                                 derive, context_key, context_value);
}

// Parse a color space name of the form "g<NN>_rec709_(scene|display)".
static float
rec709_colorspace_gamma(string_view colorspace)
{
    if (!Strutil::parse_prefix(colorspace, "g"))
        return 0.0f;
    int g10 = 0;
    if (!Strutil::parse_int(colorspace, g10) || g10 <= 0)
        return 0.0f;
    if (colorspace != "_rec709_scene" && colorspace != "_rec709_display")
        return 0.0f;
    return float(g10) / 10.0f;
}

float
pvt::get_colorspace_rec709_gamma(const ImageSpec& spec)
{
    const ColorConfig& colorconfig(ColorConfig::default_colorconfig());
    string_view colorspace = spec.get_string_attribute("oiio:ColorSpace");
    string_view interop_id = colorconfig.get_color_interop_id(colorspace);

    // Gamma interop IDs, as well as arbitrary names that do not have an
    // official interop ID as generated by set_colorspace_rec709_gamma().
    const float gamma = rec709_colorspace_gamma(
        interop_id.empty() ? colorspace : interop_id);
    // An ID or name that spells its gamma needs no further measurement.
    if (gamma != 0.0f)
        return gamma;

    // Backwards compatibility, scene_linear is not necessarily Rec.709. Any
    // other encoding with a linear transfer function is also gamma 1.0,
    // including one this configuration describes too thinly to measure but
    // whose color interop ID names a linear identity.
    if (colorconfig.equivalent(colorspace, "linear")
        || colorconfig.equivalent(colorspace, "scene_linear")
        || interop_id == "lin_rec709_scene"
        || ColorSpaceInfoAccess::transfer_function_kind(
               pvt::get_colorspace_info(spec, true))
               == ColorTransferFunctionKind::Linear
        || (!interop_id.empty()
            && colorconfig.get_color_space_info(interop_id)
                       .transfer_function_gamma()
                   == 1.0f))
        return 1.0f;
    // Backwards compatibility, this is DEPRECATED(3.1)
    else if (Strutil::istarts_with(colorspace, "Gamma")) {
        Strutil::parse_word(colorspace);
        float g = Strutil::from_string<float>(colorspace);
        if (g >= 0.01f && g <= 10.0f /* sanity check */)
            return g;
    }

    // Obsolete "oiio:Gamma" attribute for backwards compatibility
    return spec.get_float_attribute("oiio:Gamma", 0.0f);
}

OIIO_NAMESPACE_END

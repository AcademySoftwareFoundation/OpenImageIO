// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Interchange discovery, the interoperability assertion/bootstrap
// ("interopify") machinery, and the cross-config processor chokepoints and
// reconciliation routes. Split out of color_ocio.cpp; see color_ocio_pvt.h
// for the shared internal declarations.

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/strutil.h>

#include "color_ocio_pvt.h"





// The built-in interop identities config and the interoperability
// assertion/bootstrap machinery below touch ColorConfig::Impl, which lives in
// the ABI-versioned v3_1 namespace -- so they must too. The OIIO_API pvt
// shims that expose them are declared (by color_pvt.h) in the library's
// "current" namespace and are defined further down in a separate
// OIIO_NAMESPACE_BEGIN block; those reach back here with explicit v3_1::
// qualification.
OIIO_NAMESPACE_3_1_BEGIN

//////////////////////////////////////////////////////////////////////////
//
// Interoperability assertion + in-memory bootstrap.
//
// A config is "color-interoperable" when it resolves a scene-referred
// interchange space (ACES2065-1 / the aces_interchange role) that cross-config
// color features anchor on. For configs that don't, we synthesize a repaired,
// PROCESSOR_CACHE_OFF *copy* ("interopified") that does -- the original config
// is never mutated. All of this runs lazily on the first interop query. The
// free helpers below live in the same anonymous namespace as
// build_interop_identities_config() (opened above); the Impl methods that use
// them are defined after it is closed.

namespace {

// Scene-referred interchange discovery aliases, tried in order. The role name
// is first so an explicit aces_interchange role always wins; the rest are the
// well-known ACES2065-1 / AP0 spellings different configs use.
static const std::array<const char*, 16> kSceneInterchangeAliases = {
    OCIO::ROLE_INTERCHANGE_SCENE,
    "aces2065-1",
    "ACES: Linear - AP0",
    "aces 2065-1",
    "aces",
    "aces20651",
    "aces - aces2065-1",
    "ap0ln",
    "ap0",
    "lin_ap0_scene",
    "lin_ap0",
    "ap0_linear",
    "ap0_lin",
    "linear ap0",
    "linear - aces ap0",
    "linear - ap0",
};

// Display-referred interchange discovery aliases (CIE-XYZ-D65), role first.
static const std::array<const char*, 4> kDisplayInterchangeAliases = {
    OCIO::ROLE_INTERCHANGE_DISPLAY,
    "CIE-XYZ-D65",
    "CIE-XYZ D65",
    "lin_ciexyzd65_display",
};

// Resolve `name` (a color space name, alias, or role) to a real color space
// name in `config`, or empty if it doesn't resolve.
std::string
try_canonical_name(const OCIO::ConstConfigRcPtr& config, const char* name)
{
    if (!name || !*name)
        return {};
    try {
        if (auto cs = config->getColorSpace(name))
            return cs->getName();
        const char* canonical = config->getCanonicalName(name);
        if (canonical && *canonical)
            if (auto cs = config->getColorSpace(canonical))
                return cs->getName();
    } catch (...) {
    }
    return {};
}

// Discover the scene interchange color space name: the alias list (role name
// first), then OCIO's builtin identification against OIIO's interop identities
// config. Returns empty if the config resolves no scene interchange.
//
// Version-dependent quality, not a defect: IdentifyBuiltinColorSpace's
// interchange-heuristics were reworked in OCIO 2.3.1 (#1913), so results on
// non-trivial configs can differ between 2.3.0 and 2.3.1+. Both are correct
// per their own version's heuristic; this is not version-gated here.
std::string
discover_scene_interchange(const OCIO::ConstConfigRcPtr& config)
{
    if (!config)
        return {};
    for (const char* alias : kSceneInterchangeAliases)
        if (std::string name = try_canonical_name(config, alias); !name.empty())
            return name;
    if (auto ids = build_interop_identities_config()) {
        try {
            const char* id = OCIO::Config::IdentifyBuiltinColorSpace(
                config, ids, OCIO::ROLE_INTERCHANGE_SCENE);
            if (id && *id)
                return id;
        } catch (...) {
        }
    }
    return {};
}

// Mirror of discover_scene_interchange for the display-referred side.
std::string
discover_display_interchange(const OCIO::ConstConfigRcPtr& config)
{
    if (!config)
        return {};
    for (const char* alias : kDisplayInterchangeAliases)
        if (std::string name = try_canonical_name(config, alias); !name.empty())
            return name;
    if (auto ids = build_interop_identities_config()) {
        try {
            const char* id = OCIO::Config::IdentifyBuiltinColorSpace(
                config, ids, OCIO::ROLE_INTERCHANGE_DISPLAY);
            if (id && *id)
                return id;
        } catch (...) {
        }
    }
    return {};
}

// The scene-referred identity (reference) space: a non-data scene space with
// neither a to- nor a from-reference transform. Returns its name, or empty.
std::string
find_scene_reference_identity(const OCIO::ConstConfigRcPtr& config)
{
    const int n = config->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_SCENE,
                                            OCIO::COLORSPACE_ALL);
    for (int i = 0; i < n; ++i) {
        const char* name = config->getColorSpaceNameByIndex(
            OCIO::SEARCH_REFERENCE_SPACE_SCENE, OCIO::COLORSPACE_ALL, i);
        if (!name || !*name)
            continue;
        auto cs = config->getColorSpace(name);
        if (!cs || cs->isData())
            continue;
        const bool toRef = bool(
            cs->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE));
        const bool fromRef = bool(
            cs->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE));
        if (!toRef && !fromRef)
            return name;
    }
    return {};
}

// Bootstrap display-referred interchange on an editable copy: ensure a
// scene_reference role, then a ROLE_INTERCHANGE_DISPLAY space (synthesizing
// lin_ciexyzd65_display if the config carries none), and -- only if the config
// has no default view transform yet -- a scene_to_display_bridge chaining the
// scene reference to CIE-XYZ-D65. Touches only `editable`.
void
bootstrap_display_interchange(const OCIO::ConfigRcPtr& editable,
                              const std::string& interchangeScene)
{
    if (interchangeScene.empty())
        return;

    // Ensure a scene_reference role.
    std::string sceneRefName = find_scene_reference_identity(editable);
    if (!sceneRefName.empty()) {
        try {
            editable->setRole("scene_reference", sceneRefName.c_str());
        } catch (...) {
        }
    }

    // If the config already resolves a display interchange, bind the role and
    // stop.
    if (std::string existing = discover_display_interchange(editable);
        !existing.empty()) {
        try {
            editable->setRole(OCIO::ROLE_INTERCHANGE_DISPLAY, existing.c_str());
        } catch (...) {
        }
        return;
    }

    // Otherwise synthesize lin_ciexyzd65_display as the display reference.
    try {
        auto displayRef = OCIO::ColorSpace::Create(
            OCIO::REFERENCE_SPACE_DISPLAY);
        displayRef->setName("lin_ciexyzd65_display");
        displayRef->setEncoding("display-linear");
        editable->addColorSpace(displayRef);
        editable->setRole(OCIO::ROLE_INTERCHANGE_DISPLAY,
                          "lin_ciexyzd65_display");
        editable->setRole("display_reference", "lin_ciexyzd65_display");
    } catch (...) {
        return;
    }

    // Build a scene_to_display_bridge view transform, but only if the config
    // does not already declare a default one.
    try {
        const char* existingVt = editable->getDefaultViewTransformName();
        if (existingVt && *existingVt)
            return;

        auto ap0ToXyz = OCIO::BuiltinTransform::Create();
        ap0ToXyz->setStyle("UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD");

        OCIO::ConstTransformRcPtr bridge;
        auto acesCS = editable->getColorSpace(interchangeScene.c_str());
        const bool acesIsSceneRef
            = acesCS && !acesCS->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE);
        if (acesIsSceneRef || sceneRefName == interchangeScene) {
            // The scene reference IS the (positively identified) AP0
            // interchange space: use the builtin alone.
            bridge = ap0ToXyz;
        } else if (sceneRefName.empty()) {
            // The reference has no nameable space to chain through and is
            // not itself the identified interchange -- don't guess that it
            // is AP0. Skip view-transform synthesis; the display interchange
            // role is still set, so cross-config processors work for many
            // spaces anyway.
            return;
        } else {
            // Chain scene_reference -> aces_interchange, then AP0 -> XYZ-D65.
            auto group = OCIO::GroupTransform::Create();
            auto proc  = editable->getProcessor(sceneRefName.c_str(),
                                                interchangeScene.c_str());
            group->appendTransform(
                proc->getOptimizedProcessor(OCIO::OPTIMIZATION_DEFAULT)
                    ->createGroupTransform());
            group->appendTransform(ap0ToXyz);
            bridge = group;
        }

        auto vt = OCIO::ViewTransform::Create(OCIO::REFERENCE_SPACE_SCENE);
        vt->setName("scene_to_display_bridge");
        vt->setTransform(bridge, OCIO::VIEWTRANSFORM_DIR_FROM_REFERENCE);
        editable->addViewTransform(vt);
        editable->setDefaultViewTransformName("scene_to_display_bridge");
    } catch (...) {
        // View transform synthesis failed -- the display interchange role is
        // still set, so cross-config processors work for many spaces anyway.
    }
}

// Warn at most once per STRUCTURAL config id across the process. Returns true
// only for the caller that first records `id` (which should emit the warning).
// spin_rw_mutex-guarded set, mirroring the learned-complex blacklist shape.
bool
note_interop_warning(const std::string& id)
{
    static spin_rw_mutex s_mutex;
    static std::unordered_set<std::string> s_warned;
    {
        spin_rw_read_lock lock(s_mutex);
        if (s_warned.count(id))
            return false;
    }
    spin_rw_write_lock lock(s_mutex);
    return s_warned.insert(id).second;
}

}  // namespace

// The STRUCTURAL cache id: identifies the config's structure independent of
// any context (distinct from getCacheID(), which folds in the current
// context). Used as the memo/warn key so a context-invariant result is shared
// across every context a config is queried with.
std::string
get_config_cache_id(const OCIO::ConstConfigRcPtr& config)
{
    if (!config)
        return {};
    try {
        const char* id = config->getCacheID(OCIO::ConstContextRcPtr{});
        return id ? std::string(id) : std::string();
    } catch (...) {
        return {};
    }
}

// Return the "interopified" copy of `config`: a PROCESSOR_CACHE_OFF editable
// copy repaired to resolve a scene (and, where possible, display) interchange.
// Memoized process-wide by structural cache id (first-writer-wins, size
// bounded) so all ColorConfig instances of the same config structure share
// one copy. `config` itself is never mutated. Returns {} if OCIO can't
// build the copy.
OCIO::ConstConfigRcPtr
interopify_config(const OCIO::ConstConfigRcPtr& config)
{
    if (!config)
        return {};
    const std::string key = get_config_cache_id(config);

    static spin_rw_mutex s_mutex;
    static std::unordered_map<std::string, OCIO::ConstConfigRcPtr> s_memo;
    if (!key.empty()) {
        spin_rw_read_lock lock(s_mutex);
        auto it = s_memo.find(key);
        if (it != s_memo.end())
            return it->second;
    }

    // Build the repaired copy OUTSIDE the lock.
    OCIO::ConstConfigRcPtr result;
    try {
        OCIO::ConfigRcPtr editable = copy_config(config);
        std::string interchange = discover_scene_interchange(editable);
        if (!interchange.empty()) {
            // Config carries the interchange space; bind the role to it if the
            // role itself is absent.
            if (!editable->getColorSpace(OCIO::ROLE_INTERCHANGE_SCENE))
                editable->setRole(OCIO::ROLE_INTERCHANGE_SCENE,
                                  interchange.c_str());
        }
        // Fail-don't-guess: when no scene interchange can be POSITIVELY
        // identified (the aces_interchange role, a known alias/name match,
        // or OCIO builtin identification -- all covered by
        // discover_scene_interchange above), NO repair is attempted. A
        // transformless scene reference could be linear Rec.709, a camera
        // gamut, or any config-defined reference; fabricating an AP0
        // equivalence for it would produce numerically wrong cross-config
        // transforms and fingerprints. The copy then resolves no scene
        // interchange, the bridge gate stays closed, and cross-config /
        // fingerprint queries fail cleanly with the existing
        // "not color-interoperable" narration.

        // Ensure lin_ap0_scene resolves (as an alias of the interchange space)
        // so the probe path's fallback lookups find it by that name.
        if (!interchange.empty()
            && !editable->getColorSpace("lin_ap0_scene")) {
            if (auto cs = editable->getColorSpace(interchange.c_str())) {
                auto editableCS = cs->createEditableCopy();
                editableCS->addAlias("lin_ap0_scene");
                editable->addColorSpace(editableCS);
            }
        }

        bootstrap_display_interchange(editable, interchange);

        // Probe processors are one-shot (results are memoized upstream), so
        // OCIO's per-config processor cache yields no hits here while adding
        // mutex contention, a full-cache scan on every miss, and keeping every
        // probe processor alive; disabling it is the documented fast path.
        editable->setProcessorCacheFlags(OCIO::PROCESSOR_CACHE_OFF);
        result = editable;
    } catch (...) {
        return {};
    }

    if (key.empty() || !result)
        return result;
    spin_rw_write_lock lock(s_mutex);
    // Bound the memo: every entry pins a full editable OCIO config copy for
    // the life of the process. Real workloads touch a handful of configs;
    // if the cap is ever reached, dropping the memo costs only a rebuild
    // for later instances (each ColorConfig::Impl keeps its own reference
    // to the copy it obtained, so nothing in use is invalidated).
    // ponytail: clear-on-limit; add LRU only if config-churny processes
    // show rebuild cost.
    if (s_memo.size() >= 16 && !s_memo.count(key))
        s_memo.clear();
    return s_memo.emplace(key, result).first->second;  // existing on race
}



// The cross-config chokepoint: the single wrapper over OCIO's two-config
// GetProcessorFromConfigs. Every cross-config color route funnels through
// here (color-space now; a display-view sibling with the explicit-interchange
// overload lands in a later slice), so the failure policy lives in exactly one
// place. Both configs must expose the interchange role GetProcessorFromConfigs
// needs (aces_interchange for scene-referred names, cie_xyz_d65_interchange
// for display-referred) -- that is the caller's obligation, satisfied by the
// interoperability bootstrap/repair above. With both contexts null the 4-arg
// overload is used (OCIO takes each config's current context); otherwise the
// context-aware overload runs, defaulting a missing side to that config's
// current context. On any OCIO failure the processor is null and `errmsg` is
// set from the exception -- never thrown across the boundary, never silent.
//
// Fast path: when both configs already carry the aces_interchange role (true
// by construction for the interopified analysis copy + the built-in identities
// config -- interopifiedResolvesSceneInterchange() is the caller's gate), pass
// "aces_interchange" explicitly as both interchange names. This skips OCIO's
// own interchange-role lookup/validation on every call and is materially
// faster; the role check below (Config::hasRole, not a name heuristic) is what
// makes it safe to take. Any config missing the role on either side falls
// through unchanged to the discovery form.
OCIO::ConstProcessorRcPtr
processor_from_configs(const OCIO::ConstConfigRcPtr& src_config,
                       string_view src_name,
                       const OCIO::ConstConfigRcPtr& dst_config,
                       string_view dst_name, std::string& errmsg,
                       const OCIO::ConstContextRcPtr& src_context,
                       const OCIO::ConstContextRcPtr& dst_context,
                       const char* interchange_role)
{
    errmsg.clear();
    if (!src_config || !dst_config) {
        errmsg = "Cross-config processor requires two valid configs";
        return {};
    }
    const std::string src(src_name);
    const std::string dst(dst_name);
    // `interchange_role` is the anchor both configs bridge through -- normally
    // aces_interchange (scene), but cie_xyz_d65_interchange (display) when a
    // display-referred endpoint prefers the display anchor (spec 10 B2). Both
    // are colorimetric; the fast path only fires when BOTH configs carry the
    // chosen role, otherwise OCIO's own discovery form runs.
    const bool explicit_interchange = src_config->hasRole(interchange_role)
                                      && dst_config->hasRole(interchange_role);
    try {
        if (!src_context && !dst_context) {
            if (explicit_interchange)
                return OCIO::Config::GetProcessorFromConfigs(
                    src_config, src.c_str(), interchange_role, dst_config,
                    dst.c_str(), interchange_role);
            return OCIO::Config::GetProcessorFromConfigs(src_config, src.c_str(),
                                                         dst_config, dst.c_str());
        }
        OCIO::ConstContextRcPtr sctx = src_context ? src_context
                                                   : src_config->getCurrentContext();
        OCIO::ConstContextRcPtr dctx = dst_context ? dst_context
                                                   : dst_config->getCurrentContext();
        if (explicit_interchange)
            return OCIO::Config::GetProcessorFromConfigs(
                sctx, src_config, src.c_str(), interchange_role, dctx, dst_config,
                dst.c_str(), interchange_role);
        return OCIO::Config::GetProcessorFromConfigs(sctx, src_config, src.c_str(),
                                                     dctx, dst_config, dst.c_str());
    } catch (OCIO::Exception& e) {
        errmsg = e.what();
    } catch (...) {
        errmsg = "Unknown error in OpenColorIO GetProcessorFromConfigs";
    }
    return {};
}

// Display-view sibling of the cross-config chokepoint: the single wrapper over
// OCIO's two-config display-view GetProcessorFromConfigs overload. Every
// cross-config display route funnels through here, so the failure policy lives
// in one place (as with processor_from_configs). This is the cross-config
// display bridge composition -- a scene-referred source in one config routed to
// a display/view in another -- which relies on both configs exposing the
// aces_interchange role OCIO auto-detects (the caller's obligation, satisfied
// by the interoperability bootstrap/repair). With both contexts null the 6-arg
// overload runs; otherwise the context-aware overload, defaulting a missing
// side to that config's current context. On any OCIO failure the processor is
// null and `errmsg` is set from the exception -- never thrown across the
// boundary, never silent.
//
// Version-dependent quality, not a defect: display-view data-space no-op
// semantics changed in OCIO 2.3.1 (#1896) -- a display/view that is itself a
// data space is a no-op transform on 2.3.1+, whereas 2.3.0 could produce a
// non-identity result in that case. No workaround here; document only.
//
// Fast path: same construction as processor_from_configs above, and the same
// role -- aces_interchange, not cie_xyz_d65_interchange. Every caller of this
// helper routes a scene-referred source (the identities config's ACES2065-1
// identity space) into a display/view, and OCIO picks the interchange role
// from the SOURCE color space's reference type
// (Config::GetProcessorFromConfigs, display/view overload), so
// aces_interchange is what this route actually resolves through today; there
// is no display/XYZ-referred source in this chokepoint to guarantee
// cie_xyz_d65_interchange for. If a display-referred source ever routes
// through here, this fast path must gate on cie_xyz_d65_interchange instead
// (or fall back) for that case.
OCIO::ConstProcessorRcPtr
display_processor_from_configs(const OCIO::ConstConfigRcPtr& src_config,
                              string_view src_name,
                              const OCIO::ConstConfigRcPtr& dst_config,
                              string_view display, string_view view,
                              OCIO::TransformDirection direction,
                              std::string& errmsg,
                              const OCIO::ConstContextRcPtr& src_context,
                              const OCIO::ConstContextRcPtr& dst_context)
{
    errmsg.clear();
    if (!src_config || !dst_config) {
        errmsg = "Cross-config display processor requires two valid configs";
        return {};
    }
    const std::string src(src_name);
    const std::string disp(display);
    const std::string vw(view);
    const bool explicit_interchange
        = src_config->hasRole(OCIO::ROLE_INTERCHANGE_SCENE)
          && dst_config->hasRole(OCIO::ROLE_INTERCHANGE_SCENE);
    try {
        if (!src_context && !dst_context) {
            if (explicit_interchange)
                return OCIO::Config::GetProcessorFromConfigs(
                    src_config, src.c_str(), OCIO::ROLE_INTERCHANGE_SCENE,
                    dst_config, disp.c_str(), vw.c_str(),
                    OCIO::ROLE_INTERCHANGE_SCENE, direction);
            return OCIO::Config::GetProcessorFromConfigs(src_config, src.c_str(),
                                                         dst_config, disp.c_str(),
                                                         vw.c_str(), direction);
        }
        OCIO::ConstContextRcPtr sctx = src_context ? src_context
                                                   : src_config->getCurrentContext();
        OCIO::ConstContextRcPtr dctx = dst_context ? dst_context
                                                   : dst_config->getCurrentContext();
        if (explicit_interchange)
            return OCIO::Config::GetProcessorFromConfigs(
                sctx, src_config, src.c_str(), OCIO::ROLE_INTERCHANGE_SCENE,
                dctx, dst_config, disp.c_str(), vw.c_str(),
                OCIO::ROLE_INTERCHANGE_SCENE, direction);
        return OCIO::Config::GetProcessorFromConfigs(sctx, src_config, src.c_str(),
                                                     dctx, dst_config, disp.c_str(),
                                                     vw.c_str(), direction);
    } catch (OCIO::Exception& e) {
        errmsg = e.what();
    } catch (...) {
        errmsg = "Unknown error in OpenColorIO GetProcessorFromConfigs (display)";
    }
    return {};
}


void
ColorConfig::Impl::interop_bootstrap(InteropState& state) const
{
    state.is_interoperable = false;
    state.interchange_colorspace.clear();
    if (!config_ || disable_builtin_configs)
        return;

    std::string name = discover_scene_interchange(config_);
    // Builtin config naming convention: ocio://default resolves the role name
    // itself even when the alias list above discovers nothing.
    if (name.empty() && Strutil::iequals(configname(), "ocio://default"))
        name = OCIO::ROLE_INTERCHANGE_SCENE;
    if (!name.empty()) {
        state.interchange_colorspace = name;
        state.is_interoperable       = true;
    }
}



void
ColorConfig::Impl::ensure_interop() const
{
    {
        spin_rw_read_lock lock(m_mutex);
        if (m_interop_ready)
            return;
    }

    // Do all OCIO work OUTSIDE the lock (OCIO takes its own locks; a racing
    // builder's work is simply discarded -- duplicate work is fine, blocking
    // is not), then publish under the write lock. Same discipline as examine()
    // and ensureProbeConfig(); ColorConfig construction never reaches here.
    InteropState state;
    interop_bootstrap(state);
    if (config_ && !disable_ocio)
        state.interopified = interopify_config(config_);

    // Non-interoperable configs warn. This is a WARNING, not an error: it is
    // deliberately never written to the ColorConfig error string here. That
    // string is a single overwrite-on-set slot shared per config (see
    // error()/geterror() above), so setting it at bootstrap -- before any
    // cross-config route is even attempted -- would make has_error() true for
    // callers who never touch cross-config features, polluting otherwise-
    // healthy same-config use and risking a spurious trip of the uncaught-
    // error exit dump. Instead: an OIIO::debug line (attr/env-gated, never an
    // unconditional stderr print -- R4), printed at most once per structural
    // config id across the process, plus the composed message recorded
    // in-memory on THIS Impl -- every Impl that finds itself non-
    // interoperable composes and records its own `warned`/`warning_message`,
    // regardless of which Impl (if any) won the process-global debug-line
    // dedup claim below; the dedup only throttles the printed line, it must
    // not decide whether this Impl's own observable reflects reality (a
    // second ColorConfig wrapping the same structural config independently
    // discovers it is non-interoperable and must report that). reconcile_
    // cross_config{,_display} compose their own why+how-to-fix error text
    // only if/when a cross-config route is actually attempted and fails
    // (unchanged by this slice). Skip when builtin configs are disabled --
    // we didn't actually assess interoperability in that case.
    if (!state.is_interoperable && config_ && !disable_builtin_configs) {
        state.warning_message = Strutil::fmt::format(
            "OpenImageIO ColorConfig \"{}\" is not color-interoperable: "
            "no scene interchange role (aces_interchange) could be found "
            "or repaired. Cross-config color conversions and display "
            "transforms are unavailable for this config -- OCIO strict "
            "parsing will error on them, non-strict parsing will pass "
            "them through unchanged. Add the aces_interchange role (and "
            "a matching color space) to this config to enable "
            "cross-config features.",
            configname());
        state.warned            = true;
        const std::string key = get_config_cache_id(config_);
        if (!key.empty() && note_interop_warning(key))
            Strutil::debug("{}\n", state.warning_message);
    }

    spin_rw_write_lock lock(m_mutex);
    if (!m_interop_ready) {
        m_interop       = std::move(state);
        m_interop_ready = true;
    }
}



OCIO::ConstConfigRcPtr
ColorConfig::Impl::interopifiedConfig() const
{
    ensure_interop();
    spin_rw_read_lock lock(m_mutex);
    return m_interop.interopified;
}



// Reconcile a color conversion whose local resolution failed. Fires only when
// a requested name is a registry-known interop identity this config lacks; a
// locally-absent, registry-unknown name is a genuine unknown and is left to
// the caller's today's-error path -- only the foreign endpoint may come from
// the identities config, a name unknown to both configs is declined (this
// asymmetry is deliberate: it never masks a typo). The foreign endpoint is
// drawn from the built-in interop identities config; the local endpoint from
// this config's in-memory, repaired ("interopified") copy, which carries the
// interchange role GetProcessorFromConfigs bridges through.
//
// Returned-handle / errmsg contract (errmsg is always assigned):
//   * bridged success   -> non-null handle, errmsg empty.
//   * lenient fallback   -> non-null pass-through no-op handle, errmsg set to a
//     continue-message (non-strict parsing: reconciliation could not build the
//     transform, but the pipeline proceeds -- the oiiotool --colorconvert:
//     strict=0 idiom, one layer down).
//   * strict hard error  -> null handle, errmsg set to a why + how-to-fix
//     message (OCIO strict parsing restores today's hard-error behavior).
//   * declined (feature N/A / builtin configs disabled) -> null handle, errmsg
//     empty (caller keeps today's OCIO error).
//
// The gate consults the interoperability state (the repaired copy actually
// resolves a scene interchange), never bare name-presence. Every reconciliation
// it triggers emits a single, complete OIIO::debug narration; nothing is
// silent, and no OCIO exception crosses this boundary.
ColorProcessorHandle
ColorConfig::Impl::reconcile_cross_config(string_view src, string_view dst,
                                          std::string& errmsg) const
{
    errmsg.clear();
    if (!config_ || disable_ocio || disable_builtin_configs)
        return {};

    OCIO::ConstConfigRcPtr ids = build_interop_identities_config();
    if (!ids)
        return {};

    // Classify each endpoint: does this config resolve it locally, and if not,
    // is it a registry-known interop identity? A locally-absent, registry-known
    // name is the "foreign" endpoint the bridge serves.
    const std::string s(src), d(dst);
    const std::string s_local = try_canonical_name(config_, s.c_str());
    const std::string d_local = try_canonical_name(config_, d.c_str());
    const std::string s_reg = s_local.empty() ? try_canonical_name(ids, s.c_str())
                                              : std::string();
    const std::string d_reg = d_local.empty() ? try_canonical_name(ids, d.c_str())
                                              : std::string();
    const bool src_foreign = s_local.empty() && !s_reg.empty();
    const bool dst_foreign = d_local.empty() && !d_reg.empty();

    // Feature applies only when at least one endpoint is a registry-known
    // identity this config lacks AND the other endpoint resolves locally (or is
    // itself foreign). Any locally-absent, registry-unknown endpoint is a
    // genuine unknown: decline, leaving today's error untouched.
    if (!src_foreign && !dst_foreign)
        return {};
    if (!src_foreign && s_local.empty())
        return {};
    if (!dst_foreign && d_local.empty())
        return {};

    // OCIO strict parsing opts out of the lenient bridge entirely: preserve
    // today's hard-error behavior exactly (the strict-facility user story).
    bool strict = true;
    try {
        strict = config_->isStrictParsingEnabled();
    } catch (...) {
    }

    const std::string foreign_name = src_foreign ? s : d;

    // Is the foreign endpoint a DISPLAY-referred registry identity (e.g.
    // srgb_rec709_display)? Such a CIID can never have a local equivalent in a
    // config that defines no display space, yet the registry lowers it
    // colorimetrically to the scene reference through its own default view
    // transform (scene_to_display_bridge). So it is not a "typo the user should
    // fix by adding the space" the way a scene CIID is -- bridging it is the
    // correct behavior, and it is exempt from the strict hard-error opt-out
    // below. Scene CIIDs keep that opt-out unchanged.
    auto foreign_is_display = [&ids](const std::string& reg_name) {
        if (reg_name.empty())
            return false;
        try {
            auto cs = ids->getColorSpace(reg_name.c_str());
            return cs
                   && cs->getReferenceSpaceType() == OCIO::REFERENCE_SPACE_DISPLAY;
        } catch (...) {
            return false;
        }
    };
    const bool display_foreign
        = (src_foreign && foreign_is_display(s_reg))
          || (dst_foreign && foreign_is_display(d_reg));

    if (strict && !display_foreign) {
        // Strict mode never touches the interopify/repair machinery for a
        // scene CIID -- the membership check above (against the already-built,
        // memoized identities config) is all that is needed to compose the
        // hard error, so skip interopifiedConfig()'s repair/bootstrap entirely.
        // (A display-referred foreign CIID takes the bridge below instead: spec
        // 10 B2 fallback routes it through the scene interchange.)
        errmsg = Strutil::fmt::format(
            "Could not reconcile color conversion \"{}\" -> \"{}\": OCIO "
            "strict parsing is enabled. \"{}\" is a registry-known interop "
            "identity this config does not define; add the aces_interchange "
            "role (and a matching color space) to \"{}\", or use a color "
            "space name this config defines.",
            src, dst, foreign_name, configname());
        Strutil::debug("OpenImageIO ColorConfig(\"{}\"): {}\n", configname(),
                       errmsg);
        return {};
    }

    // Gate on the interoperability state, not name-presence: only bridge when
    // in-memory detection/repair produced a copy that resolves the scene
    // interchange role GetProcessorFromConfigs needs.
    OCIO::ConstConfigRcPtr bridge = interopifiedConfig();
    const bool gate_open = bridge && interopifiedResolvesSceneInterchange();

    OCIO::ConstProcessorRcPtr proc;
    std::string ocio_err;
    if (gate_open) {
        // The repaired copy is a superset of config_, so the local endpoint
        // still resolves there; the foreign endpoint comes from the identities
        // config.
        OCIO::ConstConfigRcPtr src_cfg = src_foreign ? ids : bridge;
        OCIO::ConstConfigRcPtr dst_cfg = dst_foreign ? ids : bridge;
        std::string src_name = src_foreign ? s_reg
                                           : try_canonical_name(bridge, s.c_str());
        std::string dst_name = dst_foreign ? d_reg
                                           : try_canonical_name(bridge, d.c_str());
        if (src_name.empty())
            src_name = s;
        if (dst_name.empty())
            dst_name = d;

        // Interchange selection (spec 10 B2): a display-referred foreign CIID
        // PREFERS the display anchor (cie_xyz_d65_interchange) when both configs
        // resolve it -- the registry always does, the interopified copy does
        // when bootstrap_display_interchange synthesized it. The display anchor
        // is the only one that reaches a display-referred TARGET in the user's
        // config by a single colorimetric matrix (the display->display case);
        // the scene anchor is what reaches a scene-referred target (and is the
        // Step-1 route). Neither anchor is universal for a given config (a
        // display-having config may lack a scene<->display view transform, a
        // scene-only config lacks a display target), so try the preferred anchor
        // then fall back to the other -- both are colorimetric, so whichever
        // OCIO can build is correct.
        bool prefer_display = false;
        if (display_foreign) {
            try {
                prefer_display = ids->hasRole(OCIO::ROLE_INTERCHANGE_DISPLAY)
                                 && bridge->hasRole(OCIO::ROLE_INTERCHANGE_DISPLAY);
            } catch (...) {
            }
        }
        std::array<const char*, 2> roles
            = prefer_display ? std::array<const char*, 2> { OCIO::ROLE_INTERCHANGE_DISPLAY,
                                                            OCIO::ROLE_INTERCHANGE_SCENE }
                             : std::array<const char*, 2> { OCIO::ROLE_INTERCHANGE_SCENE,
                                                            OCIO::ROLE_INTERCHANGE_SCENE };
        const int n_roles = prefer_display ? 2 : 1;
        for (int r = 0; r < n_roles && !proc; ++r) {
            // R2(b)/R4(b): narrate every cross-config route as one complete msg.
            Strutil::debug(
                "OpenImageIO ColorConfig(\"{}\"): reconciling color conversion "
                "across configs -- source \"{}\" ({}) -> destination \"{}\" ({}) "
                "via {}\n",
                configname(), src_name,
                src_foreign ? "interop identities config" : "this config",
                dst_name,
                dst_foreign ? "interop identities config" : "this config",
                roles[r]);
            proc = processor_from_configs(src_cfg, src_name, dst_cfg, dst_name,
                                          ocio_err, nullptr, nullptr, roles[r]);
        }
        if (proc) {
            try {
                return ColorProcessorHandle(new ColorProcessor_OCIO(proc));
            } catch (OCIO::Exception& e) {
                ocio_err = e.what();
            } catch (...) {
                ocio_err = "Unknown error constructing cross-config processor";
            }
        }
    }

    // Reconciliation did not complete: the gate is closed, or the bridge could
    // not build the transform. (Strict parsing already returned above.)
    // Compose one complete why + how-to-fix message (the ColorConfig error
    // string is overwrite-on-set).
    std::string why;
    if (!gate_open)
        why = Strutil::fmt::format(
            "config \"{}\" is not color-interoperable (no scene interchange "
            "role could be found or repaired)",
            configname());
    else
        why = ocio_err.empty() ? "the interop bridge could not build the "
                                 "transform"
                               : ocio_err;
    errmsg = Strutil::fmt::format(
        "Could not reconcile color conversion \"{}\" -> \"{}\": {}. \"{}\" is a "
        "registry-known interop identity this config does not define; add the "
        "aces_interchange role (and a matching color space) to \"{}\", or use a "
        "color space name this config defines.",
        src, dst, why, foreign_name, configname());

    // Lenient parsing: warn (debug) and continue with a pass-through no-op so
    // the pipeline proceeds. The message is still recorded on the ColorConfig
    // for callers that surface it.
    Strutil::debug("OpenImageIO ColorConfig(\"{}\"): {} Continuing with a "
                   "pass-through (non-strict parsing).\n",
                   configname(), errmsg);
    return ColorProcessorHandle(new ColorProcessor_Matrix(Imath::M44f(),
                                                          false));
}



// Display-view sibling of reconcile_cross_config, mirroring its strict/lenient/
// narration contract exactly (one policy, two routes). The display and view are
// inherently local to this config; only the INPUT (source) color space can be a
// locally-absent, registry-known interop identity -- the "foreign" endpoint the
// bridge serves. The foreign source comes from the built-in interop identities
// config; the local display/view from this config's in-memory repaired copy,
// which carries the interchange role OCIO's display-view GetProcessorFromConfigs
// bridges through.
//
// Returned-handle / errmsg contract matches reconcile_cross_config:
//   * bridged success  -> non-null handle, errmsg empty.
//   * lenient fallback  -> non-null pass-through no-op handle, errmsg set to the
//     continue-message (non-strict parsing). Critically, the prototype's silent
//     setSrc(ROLE_SCENE_LINEAR) "continue anyway" is NOT taken here: an
//     unbridgeable source stays untouched (pass-through), never reinterpreted as
//     scene_linear.
//   * strict hard error -> null handle, errmsg set to why + how-to-fix.
//   * declined (input resolves locally, or is a genuine unknown, or feature
//     N/A) -> null handle, errmsg empty (caller keeps today's OCIO error).
//
// TODO: the cross-config display route does not carry a looks override --
// looks are config-local and the cross-config display bridge has none. A
// foreign source that also needs a looks override is not handled yet; add
// looks support to this route if/when a caller needs looks applied across
// configs.
ColorProcessorHandle
ColorConfig::Impl::reconcile_cross_config_display(string_view input,
                                                  string_view display,
                                                  string_view view, bool inverse,
                                                  std::string& errmsg) const
{
    errmsg.clear();
    if (!config_ || disable_ocio || disable_builtin_configs)
        return {};

    OCIO::ConstConfigRcPtr ids = build_interop_identities_config();
    if (!ids)
        return {};

    // Only the input can be foreign. If it resolves locally, today's local
    // display path already handles it -- decline. If it is locally absent but
    // registry-unknown, it is a genuine unknown: decline, leaving today's error.
    const std::string in(input);
    const std::string in_local = try_canonical_name(config_, in.c_str());
    if (!in_local.empty())
        return {};
    const std::string in_reg = try_canonical_name(ids, in.c_str());
    if (in_reg.empty())
        return {};

    // OCIO strict parsing opts out of the lenient bridge entirely (today's
    // hard-error behavior), exactly as the color-space route does.
    bool strict = true;
    try {
        strict = config_->isStrictParsingEnabled();
    } catch (...) {
    }

    if (strict) {
        // Strict mode never touches the interopify/repair machinery -- the
        // membership check above (against the already-built, memoized
        // identities config) is all that is needed to compose the hard
        // error, so skip interopifiedConfig()'s repair/bootstrap entirely.
        errmsg = Strutil::fmt::format(
            "Could not reconcile display transform \"{}\" -> display \"{}\" "
            "view \"{}\": OCIO strict parsing is enabled. \"{}\" is a "
            "registry-known interop identity this config does not define; "
            "add the aces_interchange role (and a matching color space) to "
            "\"{}\", or use a color space name this config defines.",
            input, display, view, input, configname());
        Strutil::debug("OpenImageIO ColorConfig(\"{}\"): {}\n", configname(),
                       errmsg);
        return {};
    }

    // Gate on the interoperability state, not name-presence: only bridge when
    // in-memory detection/repair produced a copy resolving the scene interchange
    // role the display-view chokepoint needs.
    OCIO::ConstConfigRcPtr bridge = interopifiedConfig();
    const bool gate_open = bridge && interopifiedResolvesSceneInterchange();

    OCIO::ConstProcessorRcPtr proc;
    std::string ocio_err;
    if (gate_open) {
        const OCIO::TransformDirection dir
            = inverse ? OCIO::TRANSFORM_DIR_INVERSE : OCIO::TRANSFORM_DIR_FORWARD;

        // R3/R4(b): narrate the cross-config display route as one complete
        // message (say "display transform" so the route is identifiable).
        Strutil::debug(
            "OpenImageIO ColorConfig(\"{}\"): reconciling display transform "
            "across configs -- source \"{}\" (interop identities config) -> "
            "display \"{}\" view \"{}\" (this config)\n",
            configname(), in_reg, display, view);

        proc = display_processor_from_configs(ids, in_reg, bridge, display, view,
                                              dir, ocio_err);
        if (proc) {
            try {
                return ColorProcessorHandle(new ColorProcessor_OCIO(proc));
            } catch (OCIO::Exception& e) {
                ocio_err = e.what();
            } catch (...) {
                ocio_err = "Unknown error constructing cross-config display "
                           "processor";
            }
        }
    }

    // Reconciliation did not complete: the gate is closed, or the bridge could
    // not build the transform. (Strict parsing already returned above.)
    // Compose one complete why + how-to-fix message (the ColorConfig error
    // string is overwrite-on-set).
    std::string why;
    if (!gate_open)
        why = Strutil::fmt::format(
            "config \"{}\" is not color-interoperable (no scene interchange "
            "role could be found or repaired)",
            configname());
    else
        why = ocio_err.empty() ? "the interop bridge could not build the "
                                 "transform"
                               : ocio_err;
    errmsg = Strutil::fmt::format(
        "Could not reconcile display transform \"{}\" -> display \"{}\" view "
        "\"{}\": {}. \"{}\" is a registry-known interop identity this config "
        "does not define; add the aces_interchange role (and a matching color "
        "space) to \"{}\", or use a color space name this config defines.",
        input, display, view, why, input, configname());

    // Lenient parsing: warn (debug) and continue with a pass-through no-op.
    // On bridge failure, do NOT fall back to treating the input as
    // scene_linear -- that silently reinterprets pixels; take the
    // strict-aware error path instead. Here (non-strict), that means the
    // pixels pass through unchanged, and the message is recorded on the
    // ColorConfig for callers that surface it.
    Strutil::debug("OpenImageIO ColorConfig(\"{}\"): {} Continuing with a "
                   "pass-through (non-strict parsing).\n",
                   configname(), errmsg);
    return ColorProcessorHandle(new ColorProcessor_Matrix(Imath::M44f(),
                                                          false));
}



bool
ColorConfig::Impl::interopIsInteroperable() const
{
    ensure_interop();
    spin_rw_read_lock lock(m_mutex);
    return m_interop.is_interoperable;
}



std::string
ColorConfig::Impl::interopInterchangeName() const
{
    ensure_interop();
    spin_rw_read_lock lock(m_mutex);
    return m_interop.interchange_colorspace;
}



bool
ColorConfig::Impl::interopComputed() const
{
    spin_rw_read_lock lock(m_mutex);
    return m_interop_ready;
}



bool
ColorConfig::Impl::interopWarned() const
{
    ensure_interop();
    spin_rw_read_lock lock(m_mutex);
    return m_interop.warned;
}



bool
ColorConfig::Impl::interopifiedResolvesSceneInterchange() const
{
    ensure_interop();
    OCIO::ConstConfigRcPtr cfg;
    {
        spin_rw_read_lock lock(m_mutex);
        cfg = m_interop.interopified;
    }
    if (!cfg)
        return false;
    try {
        if (cfg->getColorSpace(OCIO::ROLE_INTERCHANGE_SCENE))
            return true;
        const char* canon = cfg->getCanonicalName(OCIO::ROLE_INTERCHANGE_SCENE);
        return canon && *canon;
    } catch (...) {
        return false;
    }
}



bool
ColorConfig::Impl::interopifiedCacheOff() const
{
    ensure_interop();
    OCIO::ConstConfigRcPtr cfg;
    {
        spin_rw_read_lock lock(m_mutex);
        cfg = m_interop.interopified;
    }
    return cfg && cfg->getProcessorCacheFlags() == OCIO::PROCESSOR_CACHE_OFF;
}

OIIO_NAMESPACE_END




// The pvt shims below are declared (OIIO_API) in the library's "current"
// namespace by color_pvt.h, so they must be defined there too, not inside
// the ABI-versioned v3_1 namespace the helpers above live in.
OIIO_NAMESPACE_BEGIN

namespace pvt {


bool
color_config_is_interoperable(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopIsInteroperable() : false;
}

std::string
color_config_interchange_name(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopInterchangeName() : std::string();
}

bool
color_config_interop_computed(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopComputed() : false;
}

bool
color_config_interop_warned(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopWarned() : false;
}

bool
color_config_interopified_resolves_scene_interchange(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopifiedResolvesSceneInterchange() : false;
}

bool
color_config_interopified_cache_off(const ColorConfig& config)
{
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    return impl ? impl->interopifiedCacheOff() : false;
}


std::vector<float>
cross_config_probe(const ColorConfig& src_config, string_view src_name,
                   const ColorConfig& dst_config, string_view dst_name,
                   cspan<float> probe, string_view context_key,
                   string_view context_value)
{
    std::vector<float> out;
    auto* src_impl = v3_1::pvt::ColorConfigClassificationPeek::impl(src_config);
    auto* dst_impl = v3_1::pvt::ColorConfigClassificationPeek::impl(dst_config);
    if (!src_impl || !dst_impl)
        return out;
    OCIO::ConstConfigRcPtr sc = src_impl->config_;
    OCIO::ConstConfigRcPtr dc = dst_impl->config_;
    if (!sc || !dc) {
        dst_impl->error("Cross-config probe requires two OCIO-backed configs");
        return out;
    }
    if (probe.size() != 3) {
        dst_impl->error("Cross-config probe expects a 3-channel pixel");
        return out;
    }

    // A non-empty key/value pair drives the context-aware overload: set the var
    // on both configs' current contexts, exercising the chokepoint's 6-arg path.
    OCIO::ConstContextRcPtr sctx, dctx;
    if (context_key.size() && context_value.size()) {
        const std::string k(context_key), v(context_value);
        auto se = sc->getCurrentContext()->createEditableCopy();
        se->setStringVar(k.c_str(), v.c_str());
        sctx = se;
        auto de = dc->getCurrentContext()->createEditableCopy();
        de->setStringVar(k.c_str(), v.c_str());
        dctx = de;
    }

    std::string err;
    auto proc = v3_1::processor_from_configs(sc, src_name, dc, dst_name, err,
                                             sctx, dctx);
    if (!proc) {
        dst_impl->error("{}", err);
        return out;
    }
    // Probe-pixel comparison discipline (abs 1e-6/channel): apply the default
    // CPU processor to the caller's pixel and hand back the transformed floats.
    out.assign(probe.begin(), probe.end());
    try {
        proc->getDefaultCPUProcessor()->applyRGB(out.data());
    } catch (OCIO::Exception& e) {
        dst_impl->error("{}", e.what());
        out.clear();
    }
    return out;
}


std::vector<float>
identities_route_probe(const ColorConfig& config, string_view local_name,
                       string_view registry_name, cspan<float> probe)
{
    std::vector<float> out;
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl || probe.size() != 3)
        return out;
    // Route the config's repaired copy's local endpoint to the identities
    // config's registry endpoint through the same chokepoint the public
    // createColorProcessor bridge path uses -- the reference it must reproduce.
    OCIO::ConstConfigRcPtr bridge = impl->interopifiedConfig();
    OCIO::ConstConfigRcPtr ids    = v3_1::build_interop_identities_config();
    if (!bridge || !ids)
        return out;
    std::string err;
    auto proc = v3_1::processor_from_configs(bridge, local_name, ids,
                                             registry_name, err);
    if (!proc)
        return out;
    out.assign(probe.begin(), probe.end());
    try {
        proc->getDefaultCPUProcessor()->applyRGB(out.data());
    } catch (OCIO::Exception&) {
        out.clear();
    }
    return out;
}


std::vector<float>
identities_display_route_probe(const ColorConfig& config,
                               string_view registry_name, string_view display,
                               string_view view, cspan<float> probe)
{
    std::vector<float> out;
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl || probe.size() != 3)
        return out;
    // Route the identities config's registry source into this config's repaired
    // copy display/view through the same display-view chokepoint the public
    // createDisplayTransform bridge path uses -- the reference it must reproduce.
    OCIO::ConstConfigRcPtr bridge = impl->interopifiedConfig();
    OCIO::ConstConfigRcPtr ids    = v3_1::build_interop_identities_config();
    if (!bridge || !ids)
        return out;
    std::string err;
    auto proc = v3_1::display_processor_from_configs(ids, registry_name, bridge,
                                                     display, view,
                                                     OCIO::TRANSFORM_DIR_FORWARD,
                                                     err);
    if (!proc)
        return out;
    out.assign(probe.begin(), probe.end());
    try {
        proc->getDefaultCPUProcessor()->applyRGB(out.data());
    } catch (OCIO::Exception&) {
        out.clear();
    }
    return out;
}


std::vector<float>
interopified_display_interchange_probe(const ColorConfig& config,
                                       string_view scene_name, cspan<float> probe)
{
    std::vector<float> out;
    auto* impl = v3_1::pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl || probe.size() != 3)
        return out;
    OCIO::ConstConfigRcPtr bridge = impl->interopifiedConfig();
    if (!bridge)
        return out;
    OCIO::ConstProcessorRcPtr proc;
    try {
        // The synthesized display interchange -> a scene space, entirely within
        // the interopified copy. Colorimetric by construction; the caller feeds
        // XYZ-D65 white and asserts it lands on the scene space's white.
        proc = bridge->getProcessor(OCIO::ROLE_INTERCHANGE_DISPLAY,
                                    std::string(scene_name).c_str());
    } catch (OCIO::Exception&) {
        return out;
    }
    if (!proc)
        return out;
    out.assign(probe.begin(), probe.end());
    try {
        proc->getDefaultCPUProcessor()->applyRGB(out.data());
    } catch (OCIO::Exception&) {
        out.clear();
    }
    return out;
}

}  // namespace pvt

OIIO_NAMESPACE_END
